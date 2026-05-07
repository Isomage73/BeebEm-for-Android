// Android equivalent of macos/MacMain.h — provides the same extern globals
// as posix/Main.h (which is included via Main.h on __APPLE__ builds).
// This file is pulled in when Main.h does #include "macos/MacMain.h".
//
// Note: this is not used in the Android build because we don't define __APPLE__.
// Main.h on non-Apple includes gui/functions.h etc., which we shadow via Sdl.h.
// This file exists only for completeness.

#pragma once

#include "Windows.h"
#include "BeebWin.h"
#include "Model.h"

extern char FDCDLL[MAX_PATH];
extern int    done;
extern Model  MachineType;
extern BeebWin *mainWin;
extern HINSTANCE hInst;
extern HWND      hCurrentDialog;
extern HACCEL    hCurrentAccelTable;

void Quit();
bool ToggleFullScreen();
void ShowingMenu();
void NoMenuShown();

static inline void SetActiveWindow(void *) {}
