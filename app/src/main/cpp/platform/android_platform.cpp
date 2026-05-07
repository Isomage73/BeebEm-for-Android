// Android platform implementation.
// Implements platform functions declared in platform/Sdl.h:
//   - Video buffer, palette, scanline rendering
//   - Sound ring buffer
//   - Timing, sleep
//   - EG_MessageBox and EG_TickBox stubs are in android_beebpages.cpp

#include <android/log.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include "Windows.h"
#include "Sdl.h"
#include "MonitorType.h"
#include "BeebWin.h"
#include "Video.h"

#define LOG_TAG "BeebEm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --------------------------------------------------------------------------
// SDL-compatible globals (declared in platform/Sdl.h)
// --------------------------------------------------------------------------

bool cfg_HaveX11               = false;
bool cfg_EmulateCrtGraphics    = true;
bool cfg_EmulateCrtTeletext    = true;
bool cfg_WantLowLatencySound   = true;
int  cfg_Windowed_Resolution   = RESOLUTION_640X512;
int  cfg_Fullscreen_Resolution = RESOLUTION_640X512;
int  cfg_VerticalOffset        = 0;
int  cfg_WaitType              = 0;

// --------------------------------------------------------------------------
// Video buffer (8-bit indexed; Phase 4 will upload this to a GL texture)
// --------------------------------------------------------------------------

static uint8_t  s_videoBuffer[BEEBEM_VIDEO_CORE_SCREEN_WIDTH *
                               BEEBEM_VIDEO_CORE_SCREEN_HEIGHT];
static uint32_t s_paletteBGRA[256];

// Complete-frame snapshot. CaptureFrame() copies s_videoBuffer here (with the
// cursor already drawn) at each BBC VSYNC, so the GL always uploads a coherent
// frame rather than a mid-scan mix of the current and previous BBC frames.
// Both caller and consumer run on the GL thread so no locking is needed.
static uint8_t s_capturedFrame[BEEBEM_VIDEO_CORE_SCREEN_WIDTH * BEEBEM_BITMAP_HEIGHT];
static int     s_capturedH    = 0;
static int     s_capturedW    = 0;
static bool    s_frameReady   = false;

// Public alias so BeebWin.cpp's __APPLE__ bitmap branch can use it.
unsigned char *g_videoBuffer = s_videoBuffer;

unsigned char *GetSDLScreenLinePtr(int line) {
    return s_videoBuffer + line * BEEBEM_VIDEO_CORE_SCREEN_WIDTH;
}

void SetBeebEmEmulatorCoresPalette(unsigned char * /*cols*/, MonitorType /*monitor*/) {
    // Pixel values in m_screen are 0-7, with bit0=R, bit1=G, bit2=B.
    // Teletext writes these directly via doHorizLine (0=black, 7=white).
    // Bitmap modes go through VideoULA_Palette which stores (ULA_write ^ 7),
    // already mapping physical BBC colors to the same 0-7 encoding (cols is identity).
    // Build a flat lookup: index i → RGBA where bits 0,1,2 of i are R,G,B.
    for (int i = 0; i < 256; ++i) {
        int r = (i & 1) ? 255 : 0;
        int g = (i & 2) ? 255 : 0;
        int b = (i & 4) ? 255 : 0;
        s_paletteBGRA[i] = (0xffu << 24) | ((uint32_t)b << 16) |
                           ((uint32_t)g << 8) | (uint32_t)r;
    }
}

void RenderLine(int /*line*/, bool /*isTeletext*/, int /*xoffset*/) {
    // Phase 3: no-op. Phase 4: copy line from s_videoBuffer into GL texture.
}

void ClearVideoWindow() {
    memset(s_videoBuffer, 0, sizeof(s_videoBuffer));
}

// --------------------------------------------------------------------------
// Window
// --------------------------------------------------------------------------

void SetWindowTitle(const char *pszTitle) {
    LOGI("Title: %s", pszTitle);
}

// --------------------------------------------------------------------------
// Timing
// --------------------------------------------------------------------------

void SaferSleep(unsigned int milliseconds) {
    usleep((useconds_t)milliseconds * 1000u);
}

// --------------------------------------------------------------------------
// Sound ring buffer — lock-free SPSC for AAudio callback thread safety.
// Producer: emulation (GL render thread) via AddBytesToSDLSoundBuffer.
// Consumer: AAudio callback thread via GetBytesFromSDLSoundBuffer.
// --------------------------------------------------------------------------

#include <atomic>

static constexpr uint32_t kBufSize = 65536; // power of 2
static unsigned char s_soundBuf[kBufSize];

// Absolute indices; unsigned subtraction gives correct space/available counts
// even after wraparound.
static std::atomic<uint32_t> s_soundWrite{0}; // written only by producer
static std::atomic<uint32_t> s_soundRead{0};  // written only by consumer

void AddBytesToSDLSoundBuffer(void *p, int len) {
    if (!p || len <= 0) return;
    const auto *src = static_cast<const unsigned char*>(p);
    uint32_t w = s_soundWrite.load(std::memory_order_relaxed);
    uint32_t r = s_soundRead.load(std::memory_order_acquire);
    uint32_t space = kBufSize - (w - r);
    if ((uint32_t)len > space) len = (int)space;
    // Split memcpy at the ring-buffer wrap boundary.
    uint32_t wmod  = w & (kBufSize - 1);
    uint32_t first = kBufSize - wmod;
    if ((uint32_t)len <= first) {
        memcpy(s_soundBuf + wmod, src, (size_t)len);
    } else {
        memcpy(s_soundBuf + wmod, src,         first);
        memcpy(s_soundBuf,        src + first, (size_t)len - first);
    }
    s_soundWrite.store(w + (uint32_t)len, std::memory_order_release);
}

void CatchupSound() {}

unsigned long HowManyBytesLeftInSDLSoundBuffer() {
    uint32_t w = s_soundWrite.load(std::memory_order_relaxed);
    uint32_t r = s_soundRead.load(std::memory_order_acquire);
    return (unsigned long)(kBufSize - (w - r));
}

int GetBytesFromSDLSoundBuffer(int len, unsigned char *dst) {
    uint32_t r = s_soundRead.load(std::memory_order_relaxed);
    uint32_t w = s_soundWrite.load(std::memory_order_acquire);
    uint32_t avail = w - r;
    if ((uint32_t)len > avail) len = (int)avail;
    // Split memcpy at the ring-buffer wrap boundary.
    uint32_t rmod  = r & (kBufSize - 1);
    uint32_t first = kBufSize - rmod;
    if ((uint32_t)len <= first) {
        memcpy(dst, s_soundBuf + rmod, (size_t)len);
    } else {
        memcpy(dst,         s_soundBuf + rmod, first);
        memcpy(dst + first, s_soundBuf,        (size_t)len - first);
    }
    s_soundRead.store(r + (uint32_t)len, std::memory_order_release);
    return len;
}

// --------------------------------------------------------------------------
// Video buffer palette expansion (for GL upload)
// --------------------------------------------------------------------------

// GetVideoRenderedHeight() (from Video.cpp) returns PreviousLastPixmapLine+1,
// i.e. the number of rows the CRTC actually wrote in the last BBC frame.
// The GL texture is always BEEBEM_BITMAP_HEIGHT rows tall to avoid constant
// texture re-allocation when games like Elite reprogram the CRTC mid-frame
// (causing PreviousLastPixmapLine to oscillate between mode-5 and mode-7
// heights at ~100 Hz, which previously triggered a 2-frame re-init on every
// change and reduced the effective render rate to near zero).
//
// Only the active scanlines are expanded; inactive rows are zeroed so the
// texture is clean below the active content.

// Pixel values are 0–7 (3 bits): bit0=R, bit1=G, bit2=B, alpha=0xFF.
// We compute RGBA directly from bit-tests rather than going through the
// 256-entry palette table, which also avoids the scatter-gather pattern that
// prevents auto-vectorisation.
//
// arm64: NEON processes 16 pixels per iteration via vst4q_u8 interleave.
// x86_64: the scalar loop below auto-vectorises with -O3 -msse4.2.
#ifdef __ARM_NEON
static void expand_pixels(const uint8_t * __restrict__ src,
                          uint8_t       * __restrict__ dst, int n) {
    const uint8x16_t mask1 = vdupq_n_u8(1);
    const uint8x16_t mask2 = vdupq_n_u8(2);
    const uint8x16_t mask4 = vdupq_n_u8(4);
    const uint8x16_t ones  = vdupq_n_u8(0xFF);
    int i = 0;
    for (; i + 16 <= n; i += 16) {
        uint8x16_t px = vld1q_u8(src + i);
        // vceqq_u8 yields 0xFF when the masked bit is set, 0x00 otherwise.
        uint8x16x4_t rgba = {
            vceqq_u8(vandq_u8(px, mask1), mask1), // R: bit 0
            vceqq_u8(vandq_u8(px, mask2), mask2), // G: bit 1
            vceqq_u8(vandq_u8(px, mask4), mask4), // B: bit 2
            ones                                   // A: always 0xFF
        };
        // vst4q_u8 interleaves 4 byte lanes: R0 G0 B0 A0 R1 G1 B1 A1 …
        // matching the GL_RGBA GL_UNSIGNED_BYTE byte order expected by glTexSubImage2D.
        vst4q_u8(dst + i * 4, rgba);
    }
    for (; i < n; ++i) {
        const uint8_t px = src[i];
        uint8_t *o = dst + i * 4;
        o[0] = (px & 1u) ? 0xFF : 0;
        o[1] = (px & 2u) ? 0xFF : 0;
        o[2] = (px & 4u) ? 0xFF : 0;
        o[3] = 0xFF;
    }
}
#else
static void expand_pixels(const uint8_t * __restrict__ src,
                          uint8_t       * __restrict__ dst, int n) {
    for (int i = 0; i < n; ++i) {
        const uint8_t px = src[i];
        uint8_t *o = dst + i * 4;
        o[0] = (px & 1u) ? 0xFF : 0;
        o[1] = (px & 2u) ? 0xFF : 0;
        o[2] = (px & 4u) ? 0xFF : 0;
        o[3] = 0xFF;
    }
}
#endif

// Called by BeebWin::UpdateLines() (android_beebwin.cpp) each time the BBC
// finishes a complete frame (VideoAddCursor already run, cursor is in buffer).
// Snapshots the raw 8-bit pixel buffer so the GL upload path sees a coherent
// frame rather than a mid-scan mix of the current and previous BBC frames.
void CaptureFrame() {
    s_capturedH   = GetVideoRenderedHeight();
    s_capturedW   = (int)ActualScreenWidth;
    memcpy(s_capturedFrame, s_videoBuffer,
           BEEBEM_VIDEO_CORE_SCREEN_WIDTH * BEEBEM_BITMAP_HEIGHT);
    s_frameReady  = true;
}

// Expand the most recently captured complete frame into dst (RGBA8).
// Returns true if a new frame was available; false if no new frame since the
// last call (caller should re-render the existing GL texture unchanged).
bool GetVideoBufferRGBA(uint8_t *dst) {
    if (!s_frameReady) return false;
    s_frameReady = false;
    const int active_n = BEEBEM_VIDEO_CORE_SCREEN_WIDTH * s_capturedH;
    const int total_n  = BEEBEM_VIDEO_CORE_SCREEN_WIDTH * BEEBEM_BITMAP_HEIGHT;
    expand_pixels(s_capturedFrame, dst, active_n);
    if (active_n < total_n)
        memset(dst + active_n * 4, 0, (size_t)(total_n - active_n) * 4);
    return true;
}

int GetVideoWidth()       { return BEEBEM_VIDEO_CORE_SCREEN_WIDTH; }
// When a complete captured frame is waiting, return its height so the caller
// gets consistent dimensions even if the BBC is already mid-next-frame.
int GetVideoHeight()      { return s_frameReady ? s_capturedH : GetVideoRenderedHeight(); }
int GetVideoActiveWidth() { return s_frameReady ? s_capturedW : (int)ActualScreenWidth; }
