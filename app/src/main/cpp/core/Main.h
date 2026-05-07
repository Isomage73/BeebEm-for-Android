/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

/* Mike Wyatt and NRM's port to win32 - 7/6/97 */

#ifndef MAIN_HEADER
#define MAIN_HEADER

#ifdef MULTITHREAD
#undef MULTITHREAD
#endif
//#define MULTITHREAD

#include "Windows.h"
#include "BeebWin.h"
#include "Model.h"

#if defined(__APPLE__) || defined(BEEBEM_ANDROID)
// macOS and Android: no SDL GUI toolkit
#else
#include "gui/functions.h"
#include "gui/sdl.h"
#include "gui/gui.h"
#endif

extern char FDCDLL[MAX_PATH];

extern int done;

extern Model MachineType;
extern BeebWin *mainWin;
extern HINSTANCE hInst;
extern HWND hCurrentDialog;
extern HACCEL hCurrentAccelTable;

void Quit();
bool ToggleFullScreen();
void ShowingMenu();
void NoMenuShown();

#ifdef __APPLE__
void ProcessKeyRepeat();
void AddHeldKey(int row, int col);
void RemoveHeldKey(int row, int col);
#elif defined(BEEBEM_ANDROID)
static inline void ProcessKeyRepeat() {}
static inline void SetActiveWindow(void *) {}
#else
void SetActiveWindow(EG_Window *window_ptr);
#endif

#endif
