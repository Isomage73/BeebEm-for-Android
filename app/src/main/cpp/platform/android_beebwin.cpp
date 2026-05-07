// Android stub implementations for BeebWin platform-specific methods.
// Phase 3: no rendering, no DirectX, no window management.

#include <android/log.h>

#include "Windows.h"
#include "BeebWin.h"
#include "Sdl.h"
#include "android_internal.h"

#define LOG_TAG "BeebEm"

// Called once per complete BBC frame (NLines > 0), after VideoAddCursor() has
// drawn the cursor. Snapshot the pixel buffer so the GL thread gets a coherent
// frame.  NLines == 0 is a no-op call from BeebWin::UpdateScreen(); ignore it.
void BeebWin::UpdateLines(HDC /*hDC*/, int /*StartY*/, int NLines) {
    if (NLines > 0) CaptureFrame();
}

// Window title
void BeebWin::UpdateWindowTitle()
{
    char title[MAX_PATH + 64];
    snprintf(title, sizeof(title), "%s", m_szTitle);
    SetWindowTitle(title);
}

// DirectX stubs — no DirectX on Android.
void BeebWin::InitDX()         {}
void BeebWin::ExitDX()         {}
void BeebWin::UpdateSmoothing() {}
void BeebWin::OnDeviceLost()   {}

// Motion blur
void BeebWin::SetMotionBlur(int MotionBlur)
{
    m_MotionBlur = MotionBlur;
}

void BeebWin::UpdateMotionBlurMenu() {}
