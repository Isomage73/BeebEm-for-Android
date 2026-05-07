// Android BeebEmPages + EG stubs — equivalent of macos/BeebEmPages.mm.
// Provides no-op implementations of the GUI/SDL widget functions.

#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>

#include "BeebEmPages.h"

#define LOG_TAG "BeebEm"

// --------------------------------------------------------------------------
// GUI struct and option table
// --------------------------------------------------------------------------

BeebEmGUI gui = {};

static int s_guiOptions[0x10000] = {};

int UpdateGUIOption(int windowsMenuId, int isSelected) {
    int prev = s_guiOptions[windowsMenuId & 0xFFFF];
    s_guiOptions[windowsMenuId & 0xFFFF] = isSelected;
    return prev;
}

int GetGUIOption(int windowsMenuId) {
    return s_guiOptions[windowsMenuId & 0xFFFF];
}

int SetGUIOptionCaption(int /*windowsMenuId*/, const char * /*str*/) {
    return 1;
}

// --------------------------------------------------------------------------
// GUI lifecycle
// --------------------------------------------------------------------------

void Show_Main() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Show_Main: no GUI on Android");
}

bool InitializeBeebEmGUI(void * /*screen_ptr*/) { return true; }
void DestroyBeebEmGUI() {}

// --------------------------------------------------------------------------
// Display helpers
// --------------------------------------------------------------------------

void Update_FDC_Buttons()        {}
void Update_Resolution_Buttons() {}
void SetNameForDisc(int /*drive*/, char * /*name_ptr*/) {}
void SetFullScreenTickbox(bool /*state*/) {}
void ClearWindowsBackgroundCacheAndResetSurface() {}

// --------------------------------------------------------------------------
// EG widget/dialog stubs — called from BeebWin::Report() and SetFullScreenMode()
// --------------------------------------------------------------------------

int EG_MessageBox(void * /*screen*/, int /*type*/, const char *title,
                  const char *msg, const char * /*btn1*/,
                  const char * /*btn2*/, int /*default_btn*/) {
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                        "[EG_MessageBox] %s: %s", title ? title : "", msg ? msg : "");
    return 1;  // Always OK/Yes.
}

void EG_TickBox_Tick(void * /*widget*/)   {}
void EG_TickBox_Untick(void * /*widget*/) {}

// screen_ptr global — opaque null pointer on Android.
void *screen_ptr = nullptr;
