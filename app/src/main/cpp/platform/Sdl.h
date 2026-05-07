// Android shadow of core/Sdl.h — no SDL dependency.
// Provides the same interface so all core files compile without SDL.
//
// This file is found first because platform/ precedes core/ in include paths.

#ifndef SDL_HEADER
#define SDL_HEADER

#include <stdint.h>

#include "MonitorType.h"
#include "Types.h"

// Screen dimensions (same values as core/Sdl.h).
#define BEEBEM_VIDEO_CORE_SCREEN_WIDTH  800
#define BEEBEM_VIDEO_CORE_SCREEN_HEIGHT 600
#define SDL_WINDOW_WIDTH  640
#define SDL_WINDOW_HEIGHT 512

// LOCK/UNLOCK no-ops — no SDL surface.
#define LOCK(s)   ((void)(s))
#define UNLOCK(s) ((void)(s))

// screen_ptr is SDL_Surface* on Linux; void* stub here.
extern void *screen_ptr;

// Global configuration options
extern bool cfg_HaveX11;
extern bool cfg_EmulateCrtGraphics;
extern bool cfg_EmulateCrtTeletext;
extern bool cfg_WantLowLatencySound;
extern int  cfg_Windowed_Resolution;
extern int  cfg_Fullscreen_Resolution;
extern int  cfg_VerticalOffset;

#define OPT_EMULATECRTGRAPHICS 30001
#define CFG_EMULATECRTGRAPHICS "EmulateCrtGraphics"
#define OPT_EMUALTECRTTELETEXT 30002
#define CFG_EMUALTECRTTELETEXT "EmulateCrtTeletext"
#define OPT_WANTLOWLATENCYSOUND 30003
#define CFG_WANTLOWLATENCYSOUND "WantLowLatencySound"

#define RESOLUTION_640X512   0
#define RESOLUTION_640X480_S 1
#define RESOLUTION_640X480_V 2
#define RESOLUTION_320X240_S 3
#define RESOLUTION_320X240_V 4
#define RESOLUTION_320X256   5

#define CFG_WINDOWEDRESOLUTION   "WindowedResolution"
#define CFG_FULLSCREENRESOLUTION "FullscreenResolution"

// Returns the number of pixel rows rendered by the CRTC in the last BBC frame.
// Defined in core/Video.cpp; used here to clip the GL texture to the active area.
int GetVideoRenderedHeight();

// Platform functions provided by android_platform.cpp.
unsigned char *GetSDLScreenLinePtr(int line);
void SetBeebEmEmulatorCoresPalette(unsigned char *cols, MonitorType Monitor);
void RenderLine(int line, bool isTeletext, int xoffset);
void SaferSleep(unsigned int ms);
void SetWindowTitle(const char *pszTitle);
void AddBytesToSDLSoundBuffer(void *p, int len);
void CatchupSound();
void ClearVideoWindow();

// SDL sound init/free — no-ops on Android (AAudio used in Phase 5).
static inline int  InitializeSDLSound(int /*samplerate*/) { return 0; }
static inline void FreeSDLSound() {}

// EG GUI system — all stubs (no SDL widget toolkit on Android).
// EG_Window / EG_Widget types are declared in BeebEmPages.h.
#define EG_MESSAGEBOX_STOP        0
#define EG_MESSAGEBOX_INFORMATION 1
#define EG_MESSAGEBOX_QUESTION    2
#ifndef EG_TRUE
#define EG_TRUE  1
#define EG_FALSE 0
#endif

// Called from posix/Windows.cpp (android_windows.cpp) and BeebWin::Report().
int EG_MessageBox(void *screen, int type, const char *title,
                  const char *msg, const char *btn1, const char *btn2,
                  int default_btn);

// EG tickbox — called from BeebWin::SetFullScreenMode().
typedef void EG_Widget;
void EG_TickBox_Tick(EG_Widget *widget);
void EG_TickBox_Untick(EG_Widget *widget);

// EG lifecycle — called from android_beebpages.cpp stubs.
static inline bool EG_Initialize()            { return true; }
static inline void EG_Draw_FlushEventQueue()  {}
static inline float EG_Draw_GetScale()        { return 1.0f; }
static inline void EG_Draw_SetToLowResolution()  {}
static inline void EG_Draw_SetToHighResolution() {}

// Timing helper
static inline uint32_t EG_Draw_CalcTimePassed(uint32_t start, uint32_t end) {
    return (end >= start) ? (end - start) : 0;
}

// Key conversion — stub (Android uses its own key mapping).
static inline int ConvertSDLKeyToBBCKey(int /*keysym*/,
                                        int * /*col*/, int * /*row*/) {
    return 0;
}

// SDL surface management stubs.
static inline void Destroy_Screen() {}
static inline int  Create_Screen()  { return 1; }

// Sleep-type options.
#define WAIT_IS_NICE      0
#define WAIT_IS_OPTIMISED 1
#define WAIT_IS_NASTY     2
extern int cfg_WaitType;

// FPS constant.
#define FRAMESPERSECOND 50

// RenderFullscreenFPS — no-op.
static inline void RenderFullscreenFPS(const char * /*str*/, int /*y*/) {}

#endif // SDL_HEADER
