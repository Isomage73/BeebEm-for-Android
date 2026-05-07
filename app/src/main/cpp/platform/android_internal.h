// Internal Android platform helpers — not visible to BeebEm core.
#pragma once

#include <android/asset_manager.h>
#include <stdint.h>

bool ExtractBeebAssets(AAssetManager *mgr, const char *filesDir);
void AndroidSetBundlePath(const char *path);

// Snapshot the current BBC pixel buffer as a complete frame (call after
// VideoAddCursor runs, i.e. from BeebWin::UpdateLines).
void CaptureFrame();

// Expand the last complete captured frame into dst (RGBA8, width×512×4 bytes).
// Returns true if a new frame was available and dst was written; false means
// no new frame since the last call — caller should keep the existing GL texture.
bool GetVideoBufferRGBA(uint8_t *dst);

int GetVideoWidth();
int GetVideoHeight();      // height of the last captured complete frame
int GetVideoActiveWidth(); // active pixel columns of the last captured frame

// Sound ring buffer consumer — called from AAudio data callback.
int GetBytesFromSDLSoundBuffer(int len, unsigned char *dst);

// AAudio lifecycle — called from JNI.
void AndroidAudioStart(unsigned int sampleRate);
void AndroidAudioStop();
