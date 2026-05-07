package uk.org.beebem.android

import android.content.Context
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class BeebGLSurfaceView(context: Context) : GLSurfaceView(context) {

    private val renderer = BeebRenderer()

    init {
        setEGLContextClientVersion(3)
        preserveEGLContextOnPause = true
        setRenderer(renderer)
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    // Full lifecycle pause — call only from Activity.onPause/onResume.
    // Internally calls GLSurfaceView.onPause() which blocks the caller until
    // the GL thread acknowledges the pause; safe from Activity callbacks but
    // MUST NOT be called from a Compose side-effect (it would block the UI thread).
    fun pauseEmulation()  { onPause() }
    fun resumeEmulation() { onResume() }

    // Soft pause for in-app overlays (menus, pickers, etc.).
    // Posts to the GL thread via queueEvent — never blocks the caller.
    fun softPause()  { queueEvent { renderer.emulationPaused = true  } }
    fun softResume() { queueEvent { renderer.emulationPaused = false; renderer.quadDirty = true } }
}

private class BeebRenderer : GLSurfaceView.Renderer {

    var quadDirty:      Boolean  = true
    // Set via GLSurfaceView.queueEvent — always read/written on the GL thread.
    var emulationPaused: Boolean = false

    private var texId   = 0
    private var progId  = 0
    private var uTexLoc = -1
    private var vaoId   = 0
    private var posVboId = 0
    private var uvVboId  = 0

    private var texWidth  = 0
    // texHeight is fixed to MAX_VIDEO_HEIGHT once the texture is allocated so the
    // GL texture is never recreated when the BBC CRTC changes screen mode
    // mid-frame (e.g. Elite's split-screen mode 5 + mode 7 causes two distinct
    // PreviousLastPixmapLine values per BBC frame, which previously forced a
    // 2-frame reinit cycle on every change, collapsing the effective render rate
    // to near zero).
    private var texHeight = 0
    // activeH / activeW are the scanlines / columns the BBC actually drew this frame.
    // They drive the UV edges so we never show black padding outside the active content.
    private var activeH   = 0
    private var activeW   = 0
    private var texReady  = false

    private var surfWidth  = 0
    private var surfHeight = 0

    private var frameBuf: ByteBuffer? = null
    private val widthOut       = IntArray(1)
    private val heightOut      = IntArray(1)
    private val activeWidthOut = IntArray(1)

    private val posBuf: FloatBuffer = floatBuf(floatArrayOf(
        -1f,  1f,
        -1f, -1f,
         1f,  1f,
         1f, -1f,
    ))
    // uvBuf is updated in recomputeQuad() when activeH changes.
    private val uvBuf: FloatBuffer = floatBuf(floatArrayOf(
        0f, 0f,
        0f, 1f,
        1f, 0f,
        1f, 1f,
    ))

    private val vertSrc = """
        #version 300 es
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vUV = aUV;
        }
    """.trimIndent()

    private val fragSrc = """
        #version 300 es
        precision mediump float;
        in vec2 vUV;
        uniform sampler2D uTex;
        out vec4 fragColor;
        void main() {
            fragColor = texture(uTex, vUV);
        }
    """.trimIndent()

    // Recompute UV coordinates for the active content area and upload to the UV VBO.
    // Position never changes so the pos VBO is written once in onSurfaceCreated.
    private fun recomputeQuad() {
        if (texWidth == 0 || texHeight == 0 || activeW == 0 || activeH == 0) return

        // UV: only the active content region — rows/columns beyond are black padding.
        val uvRight  = activeW.toFloat() / texWidth.toFloat()
        val uvBottom = activeH.toFloat() / texHeight.toFloat()
        uvBuf.position(0)
        uvBuf.put(floatArrayOf(
            0f,      0f,
            0f,      uvBottom,
            uvRight, 0f,
            uvRight, uvBottom,
        ))
        uvBuf.rewind()

        // Push updated UVs to the GPU VBO.
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, uvVboId)
        GLES30.glBufferSubData(GLES30.GL_ARRAY_BUFFER, 0, uvBuf.capacity() * 4, uvBuf)
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, 0)

        quadDirty = false
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        GLES30.glClearColor(0f, 0f, 0f, 1f)

        progId  = buildProgram(vertSrc, fragSrc)
        uTexLoc = GLES30.glGetUniformLocation(progId, "uTex")

        val ids = IntArray(1)
        GLES30.glGenTextures(1, ids, 0); texId = ids[0]
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_NEAREST)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_NEAREST)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE)

        // VAO captures all vertex attribute state; after setup each frame only
        // needs glBindVertexArray + glDrawArrays, eliminating the per-frame
        // glVertexAttribPointer / glEnableVertexAttribArray calls.
        val vaos = IntArray(1)
        GLES30.glGenVertexArrays(1, vaos, 0); vaoId = vaos[0]
        GLES30.glBindVertexArray(vaoId)

        val vbos = IntArray(2)
        GLES30.glGenBuffers(2, vbos, 0)
        posVboId = vbos[0]; uvVboId = vbos[1]

        // Position VBO — static, never changes.
        posBuf.rewind()
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, posVboId)
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, posBuf.capacity() * 4, posBuf, GLES30.GL_STATIC_DRAW)
        GLES30.glVertexAttribPointer(0, 2, GLES30.GL_FLOAT, false, 0, 0)
        GLES30.glEnableVertexAttribArray(0)

        // UV VBO — updated when the active content area changes (CRTC mode switch).
        uvBuf.rewind()
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, uvVboId)
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, uvBuf.capacity() * 4, uvBuf, GLES30.GL_STREAM_DRAW)
        GLES30.glVertexAttribPointer(1, 2, GLES30.GL_FLOAT, false, 0, 0)
        GLES30.glEnableVertexAttribArray(1)

        GLES30.glBindVertexArray(0)
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, 0)

        // If we already know the video dimensions (surface recreated after a
        // keyboard animation resize), skip the probe and re-upload the last
        // frame immediately so rendering continues without a stall.
        val existingBuf = frameBuf
        if (texWidth > 0 && texHeight > 0 && existingBuf != null) {
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId)
            GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_RGBA8,
                texWidth, texHeight, 0, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, null)
            existingBuf.rewind()
            GLES30.glTexSubImage2D(GLES30.GL_TEXTURE_2D, 0, 0, 0,
                texWidth, texHeight, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, existingBuf)
            texReady  = true
            Log.i("BeebGL", "Surface recreated; reusing ${texWidth}x${texHeight} (active ${activeH})")
        } else {
            texReady  = false
        }
        quadDirty = true
        checkGLError("onSurfaceCreated")
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        GLES30.glViewport(0, 0, width, height)
        surfWidth  = width
        surfHeight = height
        quadDirty  = true
        Log.i("BeebGL", "Surface ${width}x${height}")
    }

    override fun onDrawFrame(gl: GL10?) {
        if (!texReady) {
            // Don't probe while soft-paused: nativeRunFrame advances TotalCycles
            // without advancing wall-clock time, which would cause cyclesToRun=0
            // on the next real frame and stall the emulator.
            if (emulationPaused) {
                GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
                return
            }
            // Probe: run one emulation step just to learn the video dimensions.
            // We don't render this frame — return after setting up the texture so
            // the NEXT frame is the first one drawn. Without this explicit return
            // we'd run emulation twice in one frame (probe + normal path below),
            // throwing the wall-clock pacing off by a full frame and causing a
            // brief emulation stall while timing catches up.
            BeebEmNative.nativeRunFrame(PROBE_BUF, widthOut, heightOut, activeWidthOut)
            val w = widthOut[0]; val h = heightOut[0]
            if (w > 0 && h > 0) {
                texWidth  = w
                // Allocate the texture at the maximum possible height (BEEBEM_BITMAP_HEIGHT = 512).
                // This must match the buffer written by GetVideoBufferRGBA on the C++ side.
                texHeight = MAX_VIDEO_HEIGHT
                // Start with activeH = full texture height so the UV covers the entire
                // texture on the first frame, avoiding a jarring jump from the VideoInit
                // default (257) to the real Mode 7 height (501) after the first VSYNC.
                // Rows beyond the active area are zeroed by GetVideoBufferRGBA so the
                // small black strip at the bottom disappears once activeH stabilises.
                activeH   = texHeight
                activeW   = activeWidthOut[0].takeIf { it > 0 } ?: w
                frameBuf  = ByteBuffer.allocateDirect(w * texHeight * 4).order(ByteOrder.nativeOrder())
                GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId)
                GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_RGBA8,
                    w, texHeight, 0, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, null)
                texReady  = true
                quadDirty = true
                Log.i("BeebGL", "Texture ${w}x${texHeight} (probe ${h}, activeW ${activeW})")
            }
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
            return
        }

        val buf = frameBuf ?: run {
            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
            return
        }

        if (!emulationPaused) {
            buf.rewind()
            val newFrame = BeebEmNative.nativeRunFrame(buf, widthOut, heightOut, activeWidthOut)

            if (newFrame) {
                // A complete BBC frame is ready (VideoAddCursor already ran).
                // Update UV geometry and upload texture together so they stay in sync.
                val newH = heightOut[0]
                if (newH > 0 && newH != activeH) {
                    activeH   = newH
                    quadDirty = true
                }
                val newW = activeWidthOut[0]
                if (newW > 0 && newW != activeW) {
                    activeW   = newW
                    quadDirty = true
                }
                GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId)
                buf.rewind()
                GLES30.glTexSubImage2D(GLES30.GL_TEXTURE_2D, 0, 0, 0,
                    texWidth, texHeight, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, buf)
            }
            // No new BBC frame yet — keep rendering the existing texture so the
            // display doesn't go black on GL frames that fall between BBC VSYNCs.
        }
        // When emulationPaused the texture is not updated; the last rendered frame
        // stays in the texture and is redrawn below so the screen stays visible.

        // recomputeQuad uploads the UV VBO if the active area changed.
        if (quadDirty) recomputeQuad()

        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
        GLES30.glUseProgram(progId)
        GLES30.glActiveTexture(GLES30.GL_TEXTURE0)
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId)
        GLES30.glUniform1i(uTexLoc, 0)

        // VAO captures all vertex attribute state — no per-frame pointer setup needed.
        GLES30.glBindVertexArray(vaoId)
        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0, 4)
        GLES30.glBindVertexArray(0)
    }

    private fun buildProgram(vertSrc: String, fragSrc: String): Int {
        val vs = compileShader(GLES30.GL_VERTEX_SHADER, vertSrc)
        val fs = compileShader(GLES30.GL_FRAGMENT_SHADER, fragSrc)
        val prog = GLES30.glCreateProgram()
        GLES30.glAttachShader(prog, vs)
        GLES30.glAttachShader(prog, fs)
        GLES30.glLinkProgram(prog)
        val status = IntArray(1)
        GLES30.glGetProgramiv(prog, GLES30.GL_LINK_STATUS, status, 0)
        if (status[0] == 0) Log.e("BeebGL", "Link: ${GLES30.glGetProgramInfoLog(prog)}")
        GLES30.glDeleteShader(vs)
        GLES30.glDeleteShader(fs)
        return prog
    }

    private fun compileShader(type: Int, src: String): Int {
        val sh = GLES30.glCreateShader(type)
        GLES30.glShaderSource(sh, src)
        GLES30.glCompileShader(sh)
        val status = IntArray(1)
        GLES30.glGetShaderiv(sh, GLES30.GL_COMPILE_STATUS, status, 0)
        if (status[0] == 0) Log.e("BeebGL", "Shader: ${GLES30.glGetShaderSource(sh)}\n${GLES30.glGetShaderInfoLog(sh)}")
        return sh
    }

    private fun checkGLError(tag: String) {
        val err = GLES30.glGetError()
        if (err != GLES30.GL_NO_ERROR) Log.e("BeebGL", "$tag GL error: $err")
    }

    companion object {
        // Must match BEEBEM_BITMAP_HEIGHT in BeebWin.h (currently 512).
        // The GL texture is always this tall regardless of the active scanline count.
        private const val MAX_VIDEO_HEIGHT = 512

        private val PROBE_BUF: ByteBuffer =
            ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder())

        private fun floatBuf(data: FloatArray): FloatBuffer =
            ByteBuffer.allocateDirect(data.size * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .also { it.put(data); it.rewind() }
    }
}
