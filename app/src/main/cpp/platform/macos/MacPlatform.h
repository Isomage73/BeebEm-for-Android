// Android platform layer — replaces macos/MacPlatform.h.
// Provides the same interface so BeebWin.cpp / Sound.cpp / Video.cpp compile
// unmodified with -D__APPLE__.

#pragma once

#include <stdint.h>
#include "MonitorType.h"
#include "Types.h"

#define BEEBEM_VIDEO_CORE_SCREEN_WIDTH  800
#define BEEBEM_VIDEO_CORE_SCREEN_HEIGHT 600

#define BEEBEM_WINDOW_WIDTH  640
#define BEEBEM_WINDOW_HEIGHT 512

extern bool cfg_EmulateCrtGraphics;
extern bool cfg_EmulateCrtTeletext;
extern bool cfg_WantLowLatencySound;

#define RESOLUTION_640X512   0
#define RESOLUTION_640X480_S 1
#define RESOLUTION_640X480_V 2
#define RESOLUTION_320X240_S 3
#define RESOLUTION_320X240_V 4
#define RESOLUTION_320X256   5

extern int cfg_Windowed_Resolution;
extern int cfg_Fullscreen_Resolution;
extern int cfg_VerticalOffset;

// 8-bit indexed video buffer — BeebEm core writes here each scanline.
extern uint8_t g_videoBuffer[BEEBEM_VIDEO_CORE_SCREEN_WIDTH * BEEBEM_VIDEO_CORE_SCREEN_HEIGHT];

// 32-bit RGBA palette (256 entries) expanded by SetBeebEmEmulatorCoresPalette.
extern uint32_t g_paletteBGRA[256];

unsigned char *GetSDLScreenLinePtr(int line);
void SetBeebEmEmulatorCoresPalette(unsigned char *cols, MonitorType monitor);
void RenderLine(int line, bool isTeletext, int xoffset);
void ClearVideoWindow();

void SetWindowTitle(const char *pszTitle);
void SaferSleep(unsigned int milliseconds);

void AddBytesToSDLSoundBuffer(void *p, int len);
void CatchupSound();
void InitializeSoundBuffer();
unsigned long HowManyBytesLeftInSDLSoundBuffer();
int  GetBytesFromSDLSoundBuffer(int len, unsigned char *dst);

// type: 0=error, 1=info, 2=question, 3=confirm. Returns 1=OK/Yes, 0=No/Cancel.
int  MacReport(int type, const char *title, const char *message);

bool MacPlatformInit(int soundFrequency);
void MacPlatformFree();

// Returns the app's read-only data directory (filesDir on Android).
const char *GetBundleResourcesPath();

int  MacGetClipboardText(char *buf, int maxLen);

class RomConfigFile;
bool MacEditRomConfig(RomConfigFile& config, const char *userDataPath);

bool MacGetImageSavePath(char *outPath, int maxLen, const char *extension);
bool MacCaptureBitmap(const char *filename,
                      int srcX, int srcY, int srcW, int srcH,
                      int canvasW, int canvasH,
                      int dstX, int dstY, int dstW, int dstH,
                      const char *imageTypeUTI);

// LOCK/UNLOCK no-ops (no SDL surface to lock).
#define LOCK(s)   ((void)(s))
#define UNLOCK(s) ((void)(s))
