package uk.org.beebem.android

import android.content.res.AssetManager
import java.nio.ByteBuffer

object BeebEmNative {
    init { System.loadLibrary("beebem") }

    external fun nativeInit(assetManager: AssetManager, dataDir: String): Boolean
    external fun nativeShutdown()
    external fun nativeReset(hardReset: Boolean)

    // Returns true if a complete BBC frame was ready and frameBuffer was filled.
    // Returns false if the BBC hasn't completed a new VSYNC since the last call;
    // caller should re-render the existing GL texture without uploading.
    external fun nativeRunFrame(frameBuffer: ByteBuffer, widthOut: IntArray, heightOut: IntArray, activeWidthOut: IntArray): Boolean
    external fun nativeGetCycleCount(): Long

    external fun nativeKeyDown(beebRow: Int, beebCol: Int)
    external fun nativeKeyUp(beebRow: Int, beebCol: Int)
    external fun nativeBreakKey(shift: Boolean)

    external fun nativeMountDisc(drive: Int, path: String, writeProtect: Boolean): Boolean
    external fun nativeEjectDisc(drive: Int)
    external fun nativeFlushDisc(drive: Int): Boolean

    external fun nativeSaveState(path: String): Boolean
    external fun nativeLoadState(path: String): Boolean

    external fun nativeSetSoundEnabled(enabled: Boolean)
    external fun nativeSetSpeedLimit(percent: Int)
}
