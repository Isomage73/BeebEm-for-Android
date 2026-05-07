/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert

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

#include "Windows.h"

#include <stdint.h>

#include "gui/gui.h"
#include "gui/sdl.h"

#include "BeebEmPages.h"
#include "BeebEm.h"
#include "BeebWin.h"
#include "Main.h"
#include "Resource.h"
#include "Sdl.h"
#include "Sound.h"

#include <gtk/gtk.h>

#define MENU_COLORS CalcColor(127+64, 127+64, 127+64)
#define TITLE_COLORS CalcColor(255,0,0)

//#define MENU_COLORS CalcColor(191+32, 191+32, 0)
//#define TITLE_COLORS CalcColor(191+32, 64, 32)

static EG_BOOL Make_Main(SDL_Surface *dst_ptr);
static void Destroy_Main();

static EG_BOOL Make_System(SDL_Surface *dst_ptr);
static void Destroy_System();

static EG_BOOL Make_Video(SDL_Surface *dst_ptr);
static void Destroy_Video();

static EG_BOOL Make_Sound(SDL_Surface *dst_ptr);
static void Destroy_Sound();

static EG_BOOL Make_ROMs(SDL_Surface *dst_ptr);
static void Destroy_ROMs();

static EG_BOOL Make_Speed(SDL_Surface *dst_ptr);
static void Destroy_Speed();

static EG_BOOL Make_Devices(SDL_Surface *dst_ptr);
static void Destroy_Devices();

static EG_BOOL Make_Disks(SDL_Surface *dst_ptr);
static void Destroy_Disks();

static EG_BOOL Make_Tapes(SDL_Surface *dst_ptr);
static void Destroy_Tapes();

static EG_BOOL Make_Keyboard(SDL_Surface *dst_ptr);
static void Destroy_Keyboard();

static EG_BOOL Make_AMX(SDL_Surface *dst_ptr);
static void Destroy_AMX();

BeebEmGUI gui;

//==================================================================

#define MAX_WINDOWS_MENU_ITEMS 512

struct WindowsMenuItemBridge
{
	int windows_menu_id;
	EG_Widget *widget_ptr;
};

static WindowsMenuItemBridge win_menu[MAX_WINDOWS_MENU_ITEMS];

static int win_menu_count = 0;

static void ReCenterWindow(EG_Window *window_ptr)
{
	if (window_ptr->surface_ptr != NULL)
	{
		if (EG_Draw_GetScale() == 0.5)
		{
			window_ptr->dimension.y = window_ptr->surface_ptr->h -
			                          (int)(window_ptr->dimension.h * EG_Draw_GetScale());
		}
		else if (EG_Draw_GetScale() == 1.0)
		{
			window_ptr->dimension.y = (window_ptr->surface_ptr->h -
			                           window_ptr->dimension.h) /2;
		}
		else
		{
			window_ptr->dimension.y = 0;
		}
	}
}

static WindowsMenuItemBridge* CreateWindowsMenuItem(int win_id, EG_Widget *widget_ptr)
{
	if (win_menu_count >= MAX_WINDOWS_MENU_ITEMS)
	{
		printf("*** SDL GUI to Windows Menu bridge: exceeded max menu"
		 "items. (inc MAX_WINDOWS_MENU_ITEMS)\n");
		exit(2);
	}

	win_menu[win_menu_count].windows_menu_id = win_id;
	win_menu[win_menu_count].widget_ptr = widget_ptr;

	return &win_menu[win_menu_count++];
}

// [TODO] MenuItemCheck function etc (based on ID only, searches for widget
// in above array and then changes it's state based on the widget type).

// [TODO] one loop for three functions below.
int GetGUIOption(int windows_menu_id)
{
	for (int i = 0; i < win_menu_count; i++)
	{
		if (win_menu[i].windows_menu_id == windows_menu_id)
		{
			EG_Widget *ptr = win_menu[i].widget_ptr;

			switch (EG_Widget_GetType(ptr))
			{
				case EG_Widget_Type_TickBox:
					return EG_TickBox_IsTicked(ptr);

				/* Same as tickbox - should really wrap all functions for RadioButton.
				case EG_Widget_Type_RadioButton:
					return EG_RadioButton_IsSelected(ptr);
				*/
				case EG_Widget_Type_ToggleButton:
					return EG_ToggleButton_IsSelected(ptr);
			}

			// if (EG_Widget_GetType(ptr) == ((EG_StringHash) EG_Widget_Type_TickBox) )
			// 	return(EG_TickBox_IsTicked(ptr));
		}
	}

	return -1;
}

int SetGUIOptionCaption(int windows_menu_id, const char *str)
{
	for (int i = 0; i < win_menu_count; i++)
	{
		if (win_menu[i].windows_menu_id == windows_menu_id)
		{
			EG_Widget *ptr = win_menu[i].widget_ptr;

			switch (EG_Widget_GetType(ptr))
			{
				case EG_Widget_Type_TickBox:
					return EG_TickBox_SetCaption(ptr, str);

				/* (see above)
				case EG_Widget_Type_RadioButton:
					return EG_RadioButton_SetCaption(ptr);
				*/
				case EG_Widget_Type_ToggleButton:
					return EG_ToggleButton_SetCaption(ptr, str);
			}

			// if (EG_Widget_GetType(ptr) == ((EG_StringHash) EG_Widget_Type_TickBox) ){
			// 	return(EG_TickBox_SetCaption(ptr, str));
			// }
		}
	}

	return -1;
}

int UpdateGUIOption(int windows_menu_id, int is_selected)
{
	for (int i = 0; i < win_menu_count; i++)
	{
		if (win_menu[i].windows_menu_id == windows_menu_id)
		{
			EG_Widget *ptr = win_menu[i].widget_ptr;

			//printf("Found: %s is of type %X %X %X\n", EG_Widget_GetName(ptr), (EG_StringHash) EG_Widget_GetType(ptr) , (EG_StringHash) EG_Widget_Type_TickBox, EG_Widget_Type_RadioButton);

			if (EG_Widget_GetType(ptr) == (EG_StringHash)EG_Widget_Type_TickBox)
			{
				if (EG_TickBox_GetGroupParent(ptr) == NULL)
				{
					if (is_selected == 1)
					{
						EG_TickBox_Tick(ptr);
						EG_TickBox_RepaintLot(ptr);
					}
					else
					{
						EG_TickBox_Untick(ptr);
						EG_TickBox_RepaintLot(ptr);
					}
				}
				else
				{
					if (is_selected == 1)
						EG_RadioGroup_Select(ptr);
				}
			}

			if (EG_Widget_GetType(ptr) == (EG_StringHash)EG_Widget_Type_ToggleButton)
			{
				if (EG_ToggleButton_GetGroupParent(ptr) == NULL)
				{
					if (is_selected == 1)
					{
						EG_ToggleButton_SetSelected(ptr);
						EG_ToggleButton_RepaintLot(ptr);
					}
					else
					{
						EG_ToggleButton_SetUnselected(ptr);
						EG_ToggleButton_RepaintLot(ptr);
					}
				}
				else
				{
					if (is_selected == 1)
						EG_RadioGroup_Select(ptr);
				}
			}

			return 1;
		}
	}

	return 0;
}

static void ProcessGUIOption(EG_Widget * /* widget_ptr */, void *user_ptr)
{
	WindowsMenuItemBridge *ptr = (WindowsMenuItemBridge*)user_ptr;

	mainWin->HandleCommand(ptr->windows_menu_id);
}

static void RunDisc(EG_Widget * /* widget_ptr */, void *user_ptr)
{
	EG_Window *window_ptr = (EG_Window*) user_ptr;

	EG_Window_Hide(window_ptr);
	NoMenuShown();

	// Setup page for next visit.
	// EG_RadioButton_Enable(gui.widget_no_reset_ptr);
	// EG_RadioButton_Enable(gui.widget_reset_ptr);
	// EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	EG_Button_GetFocus(gui.widget_okay_ptr);

	mainWin->HandleCommand(IDM_RUNDISC);
}

static void SaveState(EG_Widget * /* widget_ptr */, void *user_ptr)
{
	EG_Window *window_ptr = (EG_Window*) user_ptr;

	EG_Window_Hide(window_ptr);
	NoMenuShown();

	// Setup page for next visit.
	EG_RadioButton_Enable(gui.widget_no_reset_ptr);
	EG_RadioButton_Enable(gui.widget_reset_ptr);
	EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	EG_Button_GetFocus(gui.widget_okay_ptr);

	mainWin->HandleCommand(IDM_SAVESTATE);
}

static void LoadState(EG_Widget * /* widget_ptr */, void *user_ptr)
{
	EG_Window *window_ptr = (EG_Window*) user_ptr;

	EG_Window_Hide(window_ptr);
	NoMenuShown();

	// Setup page for next visit.
	EG_RadioButton_Enable(gui.widget_no_reset_ptr);
	EG_RadioButton_Enable(gui.widget_reset_ptr);
	EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	EG_Button_GetFocus(gui.widget_okay_ptr);

	mainWin->HandleCommand(IDM_LOADSTATE);
}

/*
static SDL_Rect CalcRectCentered(int width, int height)
{
	SDL_Rect tmp;

	tmp.x = (SCREEN_WIDTH-width)/2;
	tmp.y = (SCREEN_HEIGHT-height)/2;

	tmp.w = width;
	tmp.h = height;

	return(tmp);
}

static SDL_Rect CalcRect(int x, int y, int w, int h)
{
	SDL_Rect tmp;
	tmp.x = x; tmp.y = y; tmp.w = w; tmp.h = h;
	return(tmp);
}

static SDL_Color CalcColor(int r, int g, int b)
{
	SDL_Color tmp;
	tmp.r = r; tmp.g = g; tmp.b = b;
	tmp.unused = 0;
	return(tmp);
}
*/

/*	=	=	=	=	=	=	=	=	=
 *	Initialize:
 *	=	=	=	=	=	=	=	=	=
 */



//static void SetState()
//{
//
//	/* Set machine type:
//	 */
//	switch(MachineType)
//	{
//		case 0:
//		EG_RadioGroup_Select(gui.widget_machine_bbc_b);
//		break;
//
//		case 1:
//		EG_RadioGroup_Select(gui.widget_machine_integra_b);
//		break;
//
//		case 2:
//		EG_RadioGroup_Select(gui.widget_machine_bbc_b_plus);
//		break;
//
//		case 3:
//		EG_RadioGroup_Select(gui.widget_machine_bbc_master_128);
//		break;
//	}
//	EG_Button_GetFocus(gui.widget_system_back);
//
//
//	/*
//	 */
//}

/* When windows are rendered on the SDL screen surface
 * they store their background so when they are removed they
 * don't corrupt the screen.  If we change resolution or
 * toggle fullscreen/windowed display then we need to clear
 * those background caches.
 *
 * When resolution changes the 'screen' SDL surface is
 * free'd and a new one created, so we need to update all
 * our EG windows to reference the new surface.
 */
void ClearWindowsBackgroundCacheAndResetSurface()
{
	// Clear the background cache:
	EG_Window_ClearBackgroundCache(gui.win_menu_ptr);
	EG_Window_ClearBackgroundCache(gui.win_system_ptr);
	EG_Window_ClearBackgroundCache(gui.win_screen_ptr);
	EG_Window_ClearBackgroundCache(gui.win_sound_ptr);
	EG_Window_ClearBackgroundCache(gui.win_roms_ptr);
	EG_Window_ClearBackgroundCache(gui.win_speed_ptr);
	EG_Window_ClearBackgroundCache(gui.win_devices_ptr);
	EG_Window_ClearBackgroundCache(gui.win_disks_ptr);
	EG_Window_ClearBackgroundCache(gui.win_tapes_ptr);
	EG_Window_ClearBackgroundCache(gui.win_keyboard_ptr);
	EG_Window_ClearBackgroundCache(gui.win_amx_ptr);

	// Set the windows surface to the new one:
	EG_Window_SetSurface(gui.win_menu_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_system_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_screen_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_sound_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_roms_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_speed_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_devices_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_disks_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_tapes_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_keyboard_ptr, screen_ptr);
	EG_Window_SetSurface(gui.win_amx_ptr, screen_ptr);
}

void Show_Main()
{
	if (gui.win_menu_ptr == NULL)
	{
		EG_Log(EG_LOG_ERROR, dL"Main window not made (cannot show)!", dR);
	}
	else
	{
		SetSound(SoundState::Muted);

		ReCenterWindow(gui.win_menu_ptr);
		EG_Window_Show(gui.win_menu_ptr);
		ShowingMenu();
		SetActiveWindow(gui.win_menu_ptr);
	}
}

bool InitializeBeebEmGUI(SDL_Surface *screen_ptr)
{
	// Set all gui structure elements to NULL:
	ZeroMemory(&gui, sizeof(gui));

	// Initialize gui structure:

	// [TODO] Calc. machine type.

	// Create configuration pages (the windows):

	// Build menus:
	if (!Make_Main(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Main Menu - exiting!", dR);
		return false;
	}

	if (!Make_System(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create System Menu - exiting!", dR);
		return false;
	}

	if (!Make_Video(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Screen Menu - exiting!", dR);
		return false;
	}

	if (!Make_Sound(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Sound Menu - exiting!", dR);
		return false;
	}

	if (!Make_ROMs(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create ROMs Menu - exiting!", dR);
		return false;
	}

	if (!Make_Speed(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Speed Menu - exiting!", dR);
		return false;
	}

	if (!Make_Devices(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Devices Menu - exiting!", dR);
		return false;
	}

	if (!Make_Disks(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Disks Menu - exiting!", dR);
		return false;
	}

	if (!Make_Tapes(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Tapes Menu - exiting!", dR);
		return false;
	}

	if (!Make_Keyboard(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create Keyboard Menu - exiting!", dR);
		return false;
	}

	if (!Make_AMX(screen_ptr))
	{
		EG_Log(EG_LOG_FATAL, dL"Unable to create AMX Menu - exiting!", dR);
		return false;
	}

	// Set state:
	// SetState();

	return true;
}

void DestroyBeebEmGUI()
{
	Destroy_Main();
	Destroy_System();
	Destroy_Video();
	Destroy_Sound();
	Destroy_ROMs();
	Destroy_Speed();
	Destroy_Devices();
	Destroy_Disks();
	Destroy_Tapes();
	Destroy_Keyboard();
	Destroy_AMX();
}

/*	=	=	=	=	=	=	=	=	=
 *	Shared events:
 *	=	=	=	=	=	=	=	=	=
 */

//static void Machine_Changed(EG_Widget *widget_ptr, void *user_ptr)
//{
//	static EG_BOOL i_disabled_it = EG_FALSE;
//
//	EG_BOOL force_reboot = EG_FALSE;
//	BeebEm_GUI *gui = (BeebEm_GUI*) user_ptr;
//
//	// Has the machine type changed?
//	if (strcmp(EG_Widget_GetName(widget_ptr), "machine_bbc_b") ==0 && MachineType != 0){
//		force_reboot = EG_TRUE;
//		printf("bbc_b\n");
//	}
//	if (strcmp(EG_Widget_GetName(widget_ptr), "machine_integra_b") ==0 && MachineType != 1){
//		force_reboot = EG_TRUE;
//		printf("integra_b\n");
//	}
//	if (strcmp(EG_Widget_GetName(widget_ptr), "machine_bbc_b_plus") == 0 && MachineType != 2){
//		force_reboot = EG_TRUE;
//		printf("b_plus\n");
//	}
//	if (strcmp(EG_Widget_GetName(widget_ptr), "machine_bbc_master_128") == 0 && MachineType != 3){
//		force_reboot = EG_TRUE;
//		printf("master_128\n");
//	}
//
//	// Yes, force reboot.
//	if (force_reboot == EG_TRUE){
//		if (EG_RadioButton_IsSelected(gui->widget_reset_ptr) != EG_TRUE){
//			EG_RadioGroup_Select(gui->widget_reset_ptr);
//			i_disabled_it = EG_TRUE;
//		}
//
//		EG_RadioButton_Disable(gui->widget_reset_ptr);
//		EG_RadioButton_Disable(gui->widget_no_reset_ptr);
//	}else{
//	// No, enable changes.
//		EG_RadioButton_Enable(gui->widget_reset_ptr);
//		EG_RadioButton_Enable(gui->widget_no_reset_ptr);
//
//		if ( i_disabled_it == EG_TRUE){
//			EG_RadioGroup_Select(gui->widget_no_reset_ptr);
//			i_disabled_it = EG_FALSE;
//		}
//	}
//
//	EG_Button_GetFocus(gui->widget_system_button);
//}

void SetFullScreenTickbox(bool State)
{
	if (State)
	{
		EG_TickBox_Tick(gui.fullscreen_widget_ptr);
	}
	else
	{
		EG_TickBox_Untick(gui.fullscreen_widget_ptr);
	}
}

/*	=	=	=	=	=	=	=	=	=
 *	Main Menu:
 *	=	=	=	=	=	=	=	=	=
 */

static void Main_Button_Reset(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);

	// [TODO] If 'reset BBC' ticked do it!

	NoMenuShown();

	// Setup page for next visit.

	// [XXX] IS THIS THE NULL?
	EG_RadioButton_Enable(gui.widget_no_reset_ptr);
	EG_RadioButton_Enable(gui.widget_reset_ptr);
	EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	EG_Button_GetFocus(gui.widget_okay_ptr);

	mainWin->HandleCommand(IDM_FILE_RESET);
}

static void Main_Button_Okay(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);

	// [TODO] If 'reset BBC' ticked do it!

	ClearVideoWindow();

	NoMenuShown();

	// Setup page for next visit.
	// EG_RadioButton_Enable(gui.widget_no_reset_ptr);
	// EG_RadioButton_Enable(gui.widget_reset_ptr);
	// EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	// EG_Button_GetFocus(gui.widget_okay_ptr);

	// Use this opportunity to dump some sound.
	CatchupSound();
	SetSound(SoundState::Unmuted);

	if (mainWin)
	{
		mainWin->ResetTiming();
	}
}

static void Main_Tick_FullScreen(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	// [TODO] Flush sound.

	ToggleFullScreen();

	EG_Window_RepaintLot(gui.win_menu_ptr);

	// Slight delay to allow hardware to catch up.
	SDL_Delay(200);
}

static void Main_Button_Quit(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	Quit();
	Main_Button_Okay(gui.widget_okay_ptr, &gui);
}

static void Main_Button_System(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_system_ptr);
	EG_Window_Show(gui.win_system_ptr);
	SetActiveWindow(gui.win_system_ptr);
}

static void Main_Button_Screen(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_screen_ptr);
	EG_Window_Show(gui.win_screen_ptr);
	SetActiveWindow(gui.win_screen_ptr);
}

static void Main_Button_Sound(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_sound_ptr);
	EG_Window_Show(gui.win_sound_ptr);
	SetActiveWindow(gui.win_sound_ptr);
}

static void Main_Button_ROMs(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_roms_ptr);
	EG_Window_Show(gui.win_roms_ptr);
	SetActiveWindow(gui.win_roms_ptr);
}

static void Main_Button_Speed(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_speed_ptr);
	EG_Window_Show(gui.win_speed_ptr);
	SetActiveWindow(gui.win_speed_ptr);
}

static void Main_Button_About(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	// EG_Window_Show(gui.win_about_ptr);
	// SetActiveWindow(gui.win_about_ptr);

	// EG_TextBox(screen_ptr,"BeebEm", ABOUT_MESSAGE, "Back", NULL, NULL, NULL, 1);

	EG_Draw_FlushEventQueue();

	EG_MessageBox(screen_ptr,
	              EG_MESSAGEBOX_INFORMATION,
	              "BeebEm for UNIX!",
	              ABOUT_MESSAGE,
	              "OK",
	              nullptr,
	              0);

	EG_Window_Show(gui.win_menu_ptr);
}

static void Main_Button_Devices(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_devices_ptr);
	EG_Window_Show(gui.win_devices_ptr);
	SetActiveWindow(gui.win_devices_ptr);
}

static void Main_Button_Disks(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_disks_ptr);
	EG_Window_Show(gui.win_disks_ptr);
	SetActiveWindow(gui.win_disks_ptr);
}

static void Main_Button_Tapes(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_tapes_ptr);
	EG_Window_Show(gui.win_tapes_ptr);
	SetActiveWindow(gui.win_tapes_ptr);
}

static void Main_Button_Keyboard(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_keyboard_ptr);
	EG_Window_Show(gui.win_keyboard_ptr);
	SetActiveWindow(gui.win_keyboard_ptr);
}

static void Main_Button_AMX(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_menu_ptr);
	SDL_Delay(100);
	ReCenterWindow(gui.win_amx_ptr);
	EG_Window_Show(gui.win_amx_ptr);
	SetActiveWindow(gui.win_amx_ptr);
}

static EG_BOOL Make_Main(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(320, 262+16);
	window_ptr = EG_Window_Create("win_menu", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// OK button
	loc = CalcRect(win.w-110, win.h-10-17-17-17, 100, 14*3);
	gui.widget_okay_ptr = EG_Button_Create("main_okay", col, EG_BUTTON_ALIGN_CENTER, "OK", loc);
	EG_Button_SetMyCallback_OnClick(gui.widget_okay_ptr, Main_Button_Okay, (void*) window_ptr);
	EG_Window_AddWidget(window_ptr, gui.widget_okay_ptr);

	// // Reset BBC
	// col = CalcColor(191+32, 191+32, 0);
	// widget_ptr = EG_RadioGroup_Create("main_group1");
	//
	// loc = CalcRect(10, win.h-10-17-17-17, 150, 16);
	// gui.widget_reset_ptr = EG_RadioButton_Create("main_reset", col, "Reset BBC", loc);
	//
	// loc = CalcRect(10, win.h-10-17-17, 150, 16);
	// gui.widget_no_reset_ptr = EG_RadioButton_Create("main_noreset", col, "No Reset", loc);
	//
	// EG_RadioGroup_AddButton(widget_ptr, gui.widget_reset_ptr);
	// EG_RadioGroup_AddButton(widget_ptr, gui.widget_no_reset_ptr);
	// EG_RadioGroup_Select(gui.widget_no_reset_ptr);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// Full screen toggle tickbox
	loc = CalcRect(10, win.h-10-17, 150, 16);
	widget_ptr = EG_TickBox_Create("main_fullscreen", col, "Fullscreen", loc);
	gui.fullscreen_widget_ptr = widget_ptr;
	// if (mainWin->IsFullScreen()) EG_TickBox_Tick(widget_ptr); // TODO
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Main_Tick_FullScreen, &gui);
	//EG_TickBox_Disable(widget_ptr);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	#define MENU_CALC_LOC(c, r, b) loc = CalcRect(10+c*(win.w/2-30)+10+(14*c), 30 + (r+1)*20 + b*10, win.w/2-20-5, 19)

	// Top title label
	loc = CalcRect(0,10, win.w, 20);
	col = TITLE_COLORS; //CalcColor(191+32, 64, 32);
	widget_ptr = EG_Label_Create("lab_title", col, EG_LABEL_ALIGN_CENTER, "BeebEm UNIX (" VERSION ")", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	loc = CalcRect(10, 40, win.w-20, 26*6);
	widget_ptr = EG_Box_Create("menu_box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// System menu
	MENU_CALC_LOC(0,0,0);

	// loc = CalcRect(10, 40, (win.w-20)/2-5, 20);
	widget_ptr = EG_Button_Create("but_system", col, EG_BUTTON_ALIGN_CENTER, "System", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_System, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	gui.widget_system_button = widget_ptr;

	// Screen menu
	MENU_CALC_LOC(1,0,0);
	// loc = CalcRect(win.w-((win.w-20)/2)-10+5, 40, (win.w-20)/2-5, 20);
	// col = CalcColor(191, 191, 191);

	widget_ptr = EG_Button_Create("but_screen", col, EG_BUTTON_ALIGN_CENTER, "Screen", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Screen, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Sound menu
	MENU_CALC_LOC(0,1,0);
	// loc = CalcRect(10, 40+10+20*1, (win.w-20)/2-5, 20);
	// col = CalcColor(191, 191, 191);
	widget_ptr = EG_Button_Create("but_sound", col, EG_BUTTON_ALIGN_CENTER, "Sound", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Sound, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// ROMs menu
	MENU_CALC_LOC(1,1,0);
	widget_ptr = EG_Button_Create("but_roms", col, EG_BUTTON_ALIGN_CENTER, "ROM writes", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_ROMs, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	// EG_Button_Disable(widget_ptr);

	// Speed menu
	MENU_CALC_LOC(0,2,0);
	widget_ptr = EG_Button_Create("but_speed", col, EG_BUTTON_ALIGN_CENTER, "Speed", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Speed, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// About menu
	MENU_CALC_LOC(1,2,0);
	widget_ptr = EG_Button_Create("but_about", col, EG_BUTTON_ALIGN_CENTER, "About", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_About, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	// EG_Button_Disable(widget_ptr);

	// Devices menu
	MENU_CALC_LOC(0,3,0);
	widget_ptr = EG_Button_Create("but_devices", col, EG_BUTTON_ALIGN_CENTER, "Devices", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Devices, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	// EG_Button_Disable(widget_ptr);

	// Keyboard menu
	MENU_CALC_LOC(1,3,0);
	widget_ptr = EG_Button_Create("but_keyboard", col, EG_BUTTON_ALIGN_CENTER, "Discs", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Keyboard, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	// EG_Button_Disable(widget_ptr);

	// Disks menu
	MENU_CALC_LOC(0,4,0);

	widget_ptr = EG_Button_Create("but_disks", col, EG_BUTTON_ALIGN_CENTER, "Keyboard", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Disks, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_Disable(widget_ptr);

	// AMX menu
	MENU_CALC_LOC(0,5,0);
	widget_ptr = EG_Button_Create("but_amx", col, EG_BUTTON_ALIGN_CENTER, "AMX", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_AMX, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	// EG_Button_Disable(widget_ptr);

	// Tape menu
	MENU_CALC_LOC(0,6,0);
	widget_ptr = EG_Button_Create("but_tapes", col, EG_BUTTON_ALIGN_CENTER, "Tapes", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Tapes, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Reset
	MENU_CALC_LOC(1,5,0);
	widget_ptr = EG_Button_Create("but_reset", col, EG_BUTTON_ALIGN_CENTER, "Reset", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Reset, NULL);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Quit button
	MENU_CALC_LOC(1,6,0);
	widget_ptr = EG_Button_Create("but_quit", col, EG_BUTTON_ALIGN_CENTER, "Quit", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Main_Button_Quit, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_menu_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_Main()
{
	EG_Window_DestroyAllChildWidgets(gui.win_menu_ptr);
	EG_Window_Destroy(gui.win_menu_ptr);
}

/*      =       =       =       =       =       =       =       =       =
 *	System:
 *	=	=	=	=	=	=	=	=	=
 */

static void Disks_AcornDFS(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	mainWin->LoadFDC("Acorn", true);
}

static void Disks_WatfordDFS(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	mainWin->LoadFDC("Watford", true);
}

static void Disks_OpusDFS(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	mainWin->LoadFDC("Opus", true);
}

static void Disks_NativeDFS(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	mainWin->HandleCommand(IDM_8271);
}

void Update_FDC_Buttons()
{
	char CfgName[20]="";
	char buffer[200]="";

	if (MachineType == Model::B)
		strcpy(CfgName, "FDCDLL");
	else
		sprintf(CfgName, "FDCDLL%d", (int)MachineType);

	bool Found = mainWin->m_Preferences.GetStringValue(CfgName, buffer);

	if (!Found)
	{
		strcpy(buffer,"None");
		if (MachineType == Model::BPlus)
			sprintf(buffer, "Acorn");
	}

	if (MachineType != Model::Master128 && MachineType != Model::MasterET)
	{
		EG_RadioButton_Enable(gui.widget_fdc_none);
		EG_RadioButton_Enable(gui.widget_fdc_acorn_1770);
		EG_RadioButton_Enable(gui.widget_fdc_watford);
		EG_RadioButton_Enable(gui.widget_fdc_opus);
	}

	//printf("FDC IS: '%s' '%s'\n", buffer, CfgName);

	if (strcasecmp(buffer, "none") == 0)
	{
		EG_RadioGroup_Select(gui.widget_fdc_none);
	}
	else if (strcasecmp(buffer, "acorn") == 0)
	{
		EG_RadioGroup_Select(gui.widget_fdc_acorn_1770);
	}
	else if (strcasecmp(buffer, "watford") == 0)
	{
		EG_RadioGroup_Select(gui.widget_fdc_watford);
	}
	else if (strcasecmp(buffer, "opus") == 0)
	{
		EG_RadioGroup_Select(gui.widget_fdc_opus);
	}

	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		EG_RadioButton_Disable(gui.widget_fdc_none);
		EG_RadioButton_Disable(gui.widget_fdc_acorn_1770);
		EG_RadioButton_Disable(gui.widget_fdc_watford);
		EG_RadioButton_Disable(gui.widget_fdc_opus);
	}

	// Set focus back to correct widget
	switch (MachineType)
	{
		case Model::B:
			EG_Window_SetFocusToThisWidget(gui.widget_machine_bbc_b);
			break;

		case Model::IntegraB:
			EG_Window_SetFocusToThisWidget(gui.widget_machine_integra_b);
			break;

		case Model::BPlus:
			EG_Window_SetFocusToThisWidget(gui.widget_machine_bbc_b_plus);
			break;

		case Model::Master128:
			EG_Window_SetFocusToThisWidget(gui.widget_machine_bbc_master_128);
			break;

		case Model::MasterET:
			EG_Window_SetFocusToThisWidget(gui.widget_machine_bbc_master_et);
			break;
	}
}

static void System_Change_Model(EG_Widget *widget_ptr, void *user_ptr)
{
	ProcessGUIOption(widget_ptr, user_ptr);
	Update_FDC_Buttons();
}

static void System_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_system_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_System(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;
	EG_Widget *group1_ptr, *group3_ptr;

	SDL_Rect loc;

	// Window
	SDL_Color col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);

	/*
	#ifdef WITH_ECONET
	#define econet_addition 17*4
	#else
	#define econet_addition 0
	#endif
	*/

	SDL_Rect win = CalcRectCentered(256+48+32, 17 * 17 - 8 /* + econet_addition */);
	window_ptr = EG_Window_Create("system:window", dst_ptr, col, win);
	win.x = 0; win.y = 0; win.w -= 4; win.h -= 4;

	// Title label
	#define SYSTEM_CALC_LOC(r, b) loc = CalcRect(20, 10+r*17+b*10, win.w-40, 16)

	SYSTEM_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("system:model:title", col, EG_LABEL_ALIGN_CENTER, "BBC model:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	group1_ptr = EG_RadioGroup_Create("system:model:group");

	SYSTEM_CALC_LOC(1, 0); loc.x -= 10; loc.y += 8; loc.h = 17 * 6; loc.w += 20;
	widget_ptr = EG_Box_Create("system:model:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SYSTEM_CALC_LOC(2, 0);
	widget_ptr = EG_RadioButton_Create("bbc_b", col, "BBC Model B", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 System_Change_Model,
	                                 CreateWindowsMenuItem(IDM_MODELB, widget_ptr));
	gui.widget_machine_bbc_b = widget_ptr;

	SYSTEM_CALC_LOC(3, 0);
	widget_ptr = EG_RadioButton_Create("integra_b", col, "BBC Model B + Integra-B", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 System_Change_Model,
	                                 CreateWindowsMenuItem(IDM_MODELBINT, widget_ptr));
	gui.widget_machine_integra_b = widget_ptr;

	SYSTEM_CALC_LOC(4, 0);
	widget_ptr = EG_RadioButton_Create("bbc_b_plus", col, "BBC Model B Plus", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 System_Change_Model,
	                                 CreateWindowsMenuItem(IDM_MODELBPLUS, widget_ptr));
	gui.widget_machine_bbc_b_plus = widget_ptr;

	SYSTEM_CALC_LOC(5, 0);
	widget_ptr = EG_RadioButton_Create("bbc_master_128", col, "BBC Master 128", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 System_Change_Model,
	                                 CreateWindowsMenuItem(IDM_MASTER128, widget_ptr));
	gui.widget_machine_bbc_master_128 = widget_ptr;

	SYSTEM_CALC_LOC(6, 0);
	widget_ptr = EG_RadioButton_Create("bbc_master_et", col, "BBC Master ET", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 System_Change_Model,
	                                 CreateWindowsMenuItem(IDM_MASTER_ET, widget_ptr));
	gui.widget_machine_bbc_master_et = widget_ptr;

	EG_Window_AddWidget(window_ptr, group1_ptr);

	SYSTEM_CALC_LOC(8, 0);
	widget_ptr = EG_Label_Create("system:6502:title", col, EG_LABEL_ALIGN_CENTER, "Floppy Disc Controller:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SYSTEM_CALC_LOC(9,0); loc.x-=10; loc.y+=8; loc.h = 17*3; loc.w+=20;
	widget_ptr = EG_Box_Create("system:6502:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	group3_ptr = EG_RadioGroup_Create("system:FDC");

	SYSTEM_CALC_LOC(10, 0); loc.w /=2; loc.w -=5;
	widget_ptr = EG_ToggleButton_Create("disks:nativeFDC", col, "Native", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr, Disks_NativeDFS, NULL);
	gui.widget_fdc_none = widget_ptr;

	loc.x += loc.w; loc.x += 10;
	widget_ptr = EG_ToggleButton_Create("disks:acorn", col, "Acorn 1770", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr, Disks_AcornDFS, NULL);
	gui.widget_fdc_acorn_1770 = widget_ptr;

	SYSTEM_CALC_LOC(11, 0); loc.w /=2; loc.w -=5;
	widget_ptr = EG_ToggleButton_Create("disks:watford", col, "Watford", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr, Disks_WatfordDFS, NULL);
	gui.widget_fdc_watford = widget_ptr;

	loc.x += loc.w; loc.x +=10;
	widget_ptr = EG_ToggleButton_Create("disks:opus", col, "Opus", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr, Disks_OpusDFS, NULL);
	gui.widget_fdc_opus = widget_ptr;

	EG_Window_AddWidget(window_ptr, group3_ptr);

	// col = CalcColor(255, 255, 255);
	// SYSTEM_CALC_LOC(10, 1); loc.y--;
	// widget_ptr = EG_Label_Create("disks:fdc_label", col, EG_LABEL_ALIGN_CENTER, "", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);
	// gui.widget_fdc_label = widget_ptr;

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_system_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, System_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	gui.widget_system_back = widget_ptr;

	gui.win_system_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_System()
{
	EG_Window_DestroyAllChildWidgets(gui.win_system_ptr);
	EG_Window_Destroy(gui.win_system_ptr);
}

/*	=	=	=	=	=	=	=	=	=
 *	Screen:
 *	=	=	=	=	=	=	=	=	=
 */

static void Screen_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_screen_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static void Change_Windowed_Resolution(EG_Widget * /* widget_ptr */, void * user_ptr)
{
	intptr_t value = (intptr_t) user_ptr;
	cfg_Windowed_Resolution = value;

	EG_Window_Hide(gui.win_screen_ptr);

	// Recalc display.
	Destroy_Screen();

	if (!Create_Screen())
	{
		qFATAL("Could not recreate screen! (Bailing out).");
		exit(10);
	}

	// Update GUI windows as SDL_Surface has changed:
	ClearWindowsBackgroundCacheAndResetSurface();

	// Repaint screen.
	ReCenterWindow(gui.win_screen_ptr);
	EG_Window_Show(gui.win_screen_ptr);
}

static void Change_Fullscreen_Resolution(EG_Widget * /* widget_ptr */, void *user_ptr)
{
	intptr_t value = (intptr_t) user_ptr;
	cfg_Fullscreen_Resolution = value;

	EG_Window_Hide(gui.win_screen_ptr);

	// Recalc display.
	Destroy_Screen();

	if (!Create_Screen())
	{
		qFATAL("Could not recreate screen! (Bailing out).");
		exit(10);
	}

	// Update GUI windows as SDL_Surface has changed:
	ClearWindowsBackgroundCacheAndResetSurface();

	// Repaint screen.
	ReCenterWindow(gui.win_screen_ptr);
	EG_Window_Show(gui.win_screen_ptr);
}

void Update_Resolution_Buttons()
{
	switch (cfg_Windowed_Resolution)
	{
		case RESOLUTION_640X512:
			EG_RadioGroup_Select(gui.widget_windowed_640x512);
			break;

		case RESOLUTION_640X480_S:
			EG_RadioGroup_Select(gui.widget_windowed_640x480_S);
			break;

		case RESOLUTION_640X480_V:
			EG_RadioGroup_Select(gui.widget_windowed_640x480_V);
			break;

		case RESOLUTION_320X240_S:
			EG_RadioGroup_Select(gui.widget_windowed_320x240_S);
			break;

		case RESOLUTION_320X240_V:
			EG_RadioGroup_Select(gui.widget_windowed_320x240_V);
			break;

		case RESOLUTION_320X256:
			EG_RadioGroup_Select(gui.widget_windowed_320x256);
			break;
	}

	switch (cfg_Fullscreen_Resolution)
	{
		case RESOLUTION_640X512:
			EG_RadioGroup_Select(gui.widget_fullscreen_640x512);
			break;

		case RESOLUTION_640X480_S:
			EG_RadioGroup_Select(gui.widget_fullscreen_640x480_S);
			break;

		case RESOLUTION_640X480_V:
			EG_RadioGroup_Select(gui.widget_fullscreen_640x480_V);
			break;

		case RESOLUTION_320X240_S:
			EG_RadioGroup_Select(gui.widget_fullscreen_320x240_S);
			break;

		case RESOLUTION_320X240_V:
			EG_RadioGroup_Select(gui.widget_fullscreen_320x240_V);
			break;

		case RESOLUTION_320X256:
			EG_RadioGroup_Select(gui.widget_fullscreen_320x256);
			break;
	}

	// SDL_Delay(4000);
}

static EG_BOOL Make_Video(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	EG_Widget *group1_ptr, *group2_ptr, *group3_ptr, *group4_ptr, *group5_ptr;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	// win = CalcRectCentered(480, 512-256-32+14+14-32  +32+32);
	win = CalcRectCentered(480, 480-64);

	window_ptr = EG_Window_Create("screen:window", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	loc = CalcRect(0,10, win.w, 16);
	widget_ptr = EG_Label_Create("screen:title", col, EG_LABEL_ALIGN_CENTER, "Screen configuration:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Options box
	loc = CalcRect(10, 31, win.w - 20, win.h -31-40);
	widget_ptr = EG_Box_Create("screen:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	#define SCREEN_CALC_LOC(c, r, b) loc = CalcRect(20+c*(win.w/2-20), 26 + (r+1)*17 + b*10, win.w/2-20-5, 16)

	SCREEN_CALC_LOC(0, 0, 0);
	widget_ptr = EG_TickBox_Create("screen:low_grap", col, "Grille Graphics", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(OPT_EMULATECRTGRAPHICS, widget_ptr));
	// EG_TickBox_Disable(widget_ptr);

	SCREEN_CALC_LOC(1, 0, 0);
	widget_ptr = EG_TickBox_Create("screen:low_text", col, "Grille Teletext", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(OPT_EMUALTECRTTELETEXT, widget_ptr));
	// EG_TickBox_Disable(widget_ptr);

	SCREEN_CALC_LOC(0, 1, 0);
	widget_ptr = EG_TickBox_Create("screen:cursor", col, "Hide mouse pointer", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_HIDECURSOR, widget_ptr));

	SCREEN_CALC_LOC(1, 1, 0);
	widget_ptr = EG_TickBox_Create("screen:display_fps", col, "Display FPS", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SHOW_SPEED_AND_FPS, widget_ptr));

	SCREEN_CALC_LOC(0, 2, 0);
	widget_ptr = EG_TickBox_Create("screen:keyboard_leds",  col, "Show keyboard LEDs", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SHOW_KBLEDS, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	SCREEN_CALC_LOC(1, 2, 0);
	widget_ptr = EG_TickBox_Create("screen:disc_leds", col, "Show disc LEDs", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SHOW_DISCLEDS, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	// Sleep type:
	group3_ptr = EG_RadioGroup_Create("screen:fps");

	SCREEN_CALC_LOC(0, 3, 1); loc.w = 100;
	widget_ptr = EG_Label_Create("screen:fps:title", col, EG_LABEL_ALIGN_LEFT, "Busy-wait:",loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	loc.w = 45+10; loc.x = 145;
	widget_ptr = EG_ToggleButton_Create("screen:timing:0",  col, "0%", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(OPT_SLEEP_OS, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 145+((55+5)*1); // 145;
	widget_ptr = EG_ToggleButton_Create("screen:timing:1",  col, "25%", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(OPT_SLEEP_F1, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 145+((55+5)*2); //255;
	widget_ptr = EG_ToggleButton_Create("screen:timing:2",  col,"50%", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(OPT_SLEEP_F2, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 145+((55+5)*3); //335;
	widget_ptr = EG_ToggleButton_Create("screen:timing:3",  col, "75%", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(OPT_SLEEP_F3, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 145+((55+5)*4); //415;
	loc.w +=10;
	widget_ptr=EG_ToggleButton_Create("screen:timing:4",col,"100%",loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(OPT_SLEEP_BW, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	EG_Window_AddWidget(window_ptr, group3_ptr);

	// LED colors
	group1_ptr = EG_RadioGroup_Create("screen:leds");

	SCREEN_CALC_LOC(0, 4, 2); loc.w = 90;
	widget_ptr = EG_Label_Create("screen:leds:title", col, EG_LABEL_ALIGN_LEFT, "LEDs are:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	loc.w = 55; loc.x = 125; loc.x +=20;
	widget_ptr = EG_RadioButton_Create("screen:red", col, "Red", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_RED_LEDS, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.w = 75; loc.x = 185; loc.x +=20;
	widget_ptr = EG_RadioButton_Create("screen:green", col, "Green", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_GREEN_LEDS, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	EG_Window_AddWidget(window_ptr, group1_ptr);

	// Monitor:
	group2_ptr = EG_RadioGroup_Create("screen:monitor");

	SCREEN_CALC_LOC(0, 5,2); loc.w = 120;
	widget_ptr = EG_Label_Create("screen:monitor:title", col, EG_LABEL_ALIGN_LEFT, "Monitor is:",loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	loc.w = 55; loc.x = 125; loc.x +=20;
	widget_ptr = EG_RadioButton_Create("screen:m_rgb", col, "RGB", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_MONITOR_RGB, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.w = 65; loc.x = 185; loc.x +=20;
	widget_ptr = EG_RadioButton_Create("screen:m_white", col, "Mono", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_MONITOR_BW, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 265; loc.w = 75; loc.x +=10;
	widget_ptr = EG_RadioButton_Create("screen:m_amber", col, "Amber", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_MONITOR_AMBER, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	loc.x = 345; loc.x +=10;
	widget_ptr = EG_RadioButton_Create("screen:m_green", col, "Green", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_MONITOR_GREEN, widget_ptr));
	// EG_RadioButton_Disable(widget_ptr);

	EG_Window_AddWidget(window_ptr, group2_ptr);

	// Resolution:
	group4_ptr = EG_RadioGroup_Create("resolution");

	SCREEN_CALC_LOC(0, 7, 3); loc.w *=2;
	widget_ptr = EG_Label_Create("xxx", col, EG_LABEL_ALIGN_CENTER, "Fullscreen Resolution", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SCREEN_CALC_LOC(0, 8, 4);
	widget_ptr = EG_ToggleButton_Create("res:640x512",  col, "640x512", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_640X512);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_640x512 = widget_ptr;

	SCREEN_CALC_LOC(1, 8, 4);
	widget_ptr = EG_ToggleButton_Create("res:640X480_S",  col, "640x480 (scaled)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_640X480_S);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_640x480_S = widget_ptr;

	SCREEN_CALC_LOC(0, 9, 4);
	widget_ptr = EG_ToggleButton_Create("res:640x480_V",  col, "640x480 (centered)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_640X480_V);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_640x480_V = widget_ptr;

	SCREEN_CALC_LOC(1, 9, 4);
	widget_ptr = EG_ToggleButton_Create("res:320X240_S",  col, "320x240 (scaled)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_320X240_S);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_320x240_S = widget_ptr;

	SCREEN_CALC_LOC(0, 10, 4);
	widget_ptr = EG_ToggleButton_Create("res:320x240_V",  col, "320x240 (centered)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_320X240_V);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_320x240_V = widget_ptr;

	SCREEN_CALC_LOC(1, 10, 4);
	widget_ptr = EG_ToggleButton_Create("res:320X256",  col, "320x256", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Fullscreen_Resolution,
	                                      (void*)RESOLUTION_320X256);
	EG_RadioGroup_AddButton(group4_ptr, widget_ptr);
	gui.widget_fullscreen_320x256 = widget_ptr;

	EG_Window_AddWidget(window_ptr, group4_ptr);

	// Resolution:
	group5_ptr = EG_RadioGroup_Create("resolution");

	SCREEN_CALC_LOC(0, 11, 5); loc.w *=2;
	widget_ptr = EG_Label_Create("xxx", col, EG_LABEL_ALIGN_CENTER, "Windowed Resolution", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SCREEN_CALC_LOC(0, 12, 6);
	widget_ptr = EG_ToggleButton_Create("res:640x512",  col, "640x512", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_640X512);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	gui.widget_windowed_640x512 = widget_ptr;

	SCREEN_CALC_LOC(1, 12, 6);
	widget_ptr = EG_ToggleButton_Create("res:640X480_S",  col, "640x480 (scaled)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_640X480_S);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	gui.widget_windowed_640x480_S = widget_ptr;

	SCREEN_CALC_LOC(0, 13, 6);
	widget_ptr = EG_ToggleButton_Create("res:640x480_V",  col, "640x480 (centered)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_640X480_V);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	gui.widget_windowed_640x480_V = widget_ptr;

	SCREEN_CALC_LOC(1, 13, 6);
	widget_ptr = EG_ToggleButton_Create("res:320X240_S",  col, "320x240 (scaled)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_320X240_S);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	gui.widget_windowed_320x240_S = widget_ptr;

	SCREEN_CALC_LOC(0, 14, 6);
	widget_ptr = EG_ToggleButton_Create("res:320x240_V",  col, "320x240 (centered)", loc);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_320X240_V);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	gui.widget_windowed_320x240_V = widget_ptr;

	SCREEN_CALC_LOC(1, 14, 6);
	widget_ptr = EG_ToggleButton_Create("res:320X256",  col, "320x256", loc);
	EG_RadioGroup_AddButton(group5_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      Change_Windowed_Resolution,
	                                      (void*)RESOLUTION_320X256);
	EG_Window_AddWidget(window_ptr, group5_ptr);
	gui.widget_windowed_320x256 = widget_ptr;


	/* FPS:
	 * MOVE THIS TO SPEED
	 */
	/*
	group3_ptr = EG_RadioGroup_Create("screen:fps");

	SCREEN_CALC_LOC(0, 6, 2); loc.w = 90;
	widget_ptr = EG_Label_Create("screen:fps:title", col, EG_LABEL_ALIGN_LEFT, "FPS:",loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	loc.w = 55; loc.x = 115;
	widget_ptr = EG_RadioButton_Create("screen:fps:50",  col, "50", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_50FPS, widget_ptr));
	EG_RadioButton_Disable(widget_ptr);

	loc.w = 75; loc.x = 175;
	widget_ptr = EG_RadioButton_Create("screen:fps:25",  col, "25", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_25FPS, widget_ptr));
	EG_RadioButton_Disable(widget_ptr);

	loc.x = 255; loc.w = 75;
	widget_ptr = EG_RadioButton_Create("screen:fps:10",  col, "10", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_10FPS, widget_ptr));
	EG_RadioButton_Disable(widget_ptr);

	loc.x = 335;
	widget_ptr = EG_RadioButton_Create("screen:fps:5",  col, "5", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_5FPS, widget_ptr));
	EG_RadioButton_Disable(widget_ptr);

	loc.x = 415; loc.w = 35;
	widget_ptr = EG_RadioButton_Create("screen:fps:1",  col, "1", loc);
	EG_RadioGroup_AddButton(group3_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_1FPS, widget_ptr));
	EG_RadioButton_Disable(widget_ptr);

	EG_Window_AddWidget(window_ptr, group3_ptr);
	*/

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_screen_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Screen_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_GetFocus(widget_ptr);

	gui.win_screen_ptr = window_ptr;
	return EG_TRUE;
}

static void Destroy_Video()
{
	EG_Window_DestroyAllChildWidgets(gui.win_screen_ptr);
	EG_Window_Destroy(gui.win_screen_ptr);
}

/*	=	=	=	=	=	=	=	=	=
 *	Sound:
 *	=	=	=	=	=	=	=	=	=
 */

static void Sound_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_sound_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_Sound(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	EG_Widget *group1_ptr, *group2_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(256+64, 17*25);
	window_ptr = EG_Window_Create("sound:window", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define SOUND_CALC_LOC(r, b) loc = CalcRect(20, 10+r*17+b*10, win.w-40, 16)

	SOUND_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("sound:sound:title", col, EG_LABEL_ALIGN_CENTER, "Sound:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SOUND_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 17*7; loc.w+=20;
	widget_ptr = EG_Box_Create("sound:sound:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SOUND_CALC_LOC(2, 0);
	widget_ptr = EG_TickBox_Create("sound:sound:yes/no", col, "Sound support", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SOUNDONOFF, widget_ptr));

	SOUND_CALC_LOC(3, 0);
	widget_ptr = EG_TickBox_Create("sound:expvol", col, "Exponential Volume", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_EXPVOLUME, widget_ptr));

	SOUND_CALC_LOC(4, 0);
	widget_ptr = EG_TickBox_Create("sound:latacy:yes/no", col, "Sound is low latency", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(OPT_WANTLOWLATENCYSOUND, widget_ptr));

	SOUND_CALC_LOC(5, 0);
	widget_ptr = EG_TickBox_Create("sound:speech:yes/no", col, "Speech support", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SPEECH, widget_ptr));

	SOUND_CALC_LOC(6, 0);
	widget_ptr = EG_TickBox_Create("sound:Relay:yes/no", col, "Cassette relay effects", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_SFX_RELAY, widget_ptr));

	SOUND_CALC_LOC(7, 0);
	widget_ptr = EG_TickBox_Create("sound:Input:yes/no", col, "Cassette input effects", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_TAPESOUND, widget_ptr));

	SOUND_CALC_LOC(9, 0);
	widget_ptr = EG_Label_Create("sound:quality:title", col, EG_LABEL_ALIGN_CENTER, "Quality:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	group1_ptr = EG_RadioGroup_Create("quality");

	SOUND_CALC_LOC(10,0); loc.x-=10; loc.y+=8; loc.h = 17*4; loc.w+=20;
	widget_ptr = EG_Box_Create("sound:quality:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SOUND_CALC_LOC(11, 0);
	widget_ptr = EG_RadioButton_Create("sound:quality:11_025", col, "11.025 kHz", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_11025KHZ, widget_ptr));

	SOUND_CALC_LOC(12, 0);
	widget_ptr = EG_RadioButton_Create("sound:quality:22_050", col, "22.050 kHz", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	// EG_RadioGroup_Select(widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_22050KHZ, widget_ptr));

	SOUND_CALC_LOC(13, 0);
	widget_ptr = EG_RadioButton_Create("sound:quality:44_100", col, "44.100 kHz", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_44100KHZ, widget_ptr));

	EG_Window_AddWidget(window_ptr, group1_ptr);

	// EG_TickBox_Tick(widget_ptr);
	// EG_TickBox_Disable(widget_ptr);

	SOUND_CALC_LOC(15, 0);
	widget_ptr = EG_Label_Create("sound:volume:label", col, EG_LABEL_ALIGN_CENTER, "Volume:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	group2_ptr = EG_RadioGroup_Create("volume");

	SOUND_CALC_LOC(16,0); loc.x-=10; loc.y+=8; loc.h = 17*5; loc.w+=20;
	widget_ptr = EG_Box_Create("sound:volume:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SOUND_CALC_LOC(17, 0);
	widget_ptr = EG_RadioButton_Create("sound:volume:full", col, "Full volume", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);

	EG_RadioButton_SetMyCallback_OnClick(widget_ptr,
	                                     ProcessGUIOption,
	                                     CreateWindowsMenuItem(IDM_FULLVOLUME, widget_ptr));

	SOUND_CALC_LOC(18, 0);
	widget_ptr = EG_RadioButton_Create("sound:volume:high", col, "High volume", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_RadioButton_SetMyCallback_OnClick(widget_ptr,
	                                     ProcessGUIOption,
	                                     CreateWindowsMenuItem(IDM_HIGHVOLUME, widget_ptr));

	SOUND_CALC_LOC(19, 0);
	widget_ptr = EG_RadioButton_Create("sound:volume:medium", col, "Medium volume", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_RadioButton_SetMyCallback_OnClick(widget_ptr,
	                                     ProcessGUIOption,
	                                     CreateWindowsMenuItem(IDM_MEDIUMVOLUME, widget_ptr));

	SOUND_CALC_LOC(20, 0);
	widget_ptr = EG_RadioButton_Create("sound:volume:low", col, "Low volume", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_RadioButton_SetMyCallback_OnClick(widget_ptr,
	                                     ProcessGUIOption,
	                                     CreateWindowsMenuItem(IDM_LOWVOLUME, widget_ptr));

	EG_Window_AddWidget(window_ptr, group2_ptr);

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_sound_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Sound_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_GetFocus(widget_ptr);

	gui.win_sound_ptr = window_ptr;
	return EG_TRUE;
}

static void Destroy_Sound()
{
	EG_Window_DestroyAllChildWidgets(gui.win_sound_ptr);
	EG_Window_Destroy(gui.win_sound_ptr);
}

/*	=	=	=	=	=	=	=	=	=
 *	ROMs:
 *	=	=	=	=	=	=	=	=	=
 */

static void ROMs_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_roms_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_ROMs(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// char rom_caption[64+1];
	int i;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(480/2, 22*17);
	window_ptr = EG_Window_Create("win_roms", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define ROMS_CALC_LOC(r, b) loc = CalcRect(20, 10+r*17+b*10, win.w-40, 16)

	ROMS_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_sound", col, EG_LABEL_ALIGN_CENTER, "Allow ROM writes:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	ROMS_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 17*17; loc.w+=20;
	widget_ptr = EG_Box_Create("rom_box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// ROMS_CALC_LOC(2, 0);
	// widget_ptr = EG_Label_Create("lab_caption", col, EG_LABEL_ALIGN_LEFT, "R/W", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// TODO: These are dependent on the model.
	for (i = 0; i < 16; i++)
	{
		// sprintf(rom_caption, "  %X %s", i, "[TODO]");
		// rom_caption[0] = "";

		ROMS_CALC_LOC((i + 2), 0);

		widget_ptr = EG_TickBox_Create(NULL, col, "", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);
		EG_TickBox_SetMyCallback_OnClick(widget_ptr,
		                                 ProcessGUIOption,
		                                 CreateWindowsMenuItem(IDM_ALLOWWRITES_ROM0 + i, widget_ptr));
	}

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_roms_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, ROMs_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_GetFocus(widget_ptr);

	gui.win_roms_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_ROMs()
{
	EG_Window_DestroyAllChildWidgets(gui.win_roms_ptr);
	EG_Window_Destroy(gui.win_roms_ptr);
}


/*	=	=	=	=	=	=	=	=	=
 *	Speed:
 *	=	=	=	=	=	=	=	=	=
 */


static void Speed_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_speed_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_Speed(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	EG_Widget *group_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	#define SPEED_COUNT 15

	static const float speed[] =
	{
		100,
		50,
		10,
		5,
		2,
		1.5,
		1.25,
		1.1,
		1,
		1,
		0.9,
		0.75,
		0.5,
		0.25,
		0.1
	};

	static const int speed_menu_id[] =
	{
		IDM_FIXEDSPEED100,
		IDM_FIXEDSPEED50,
		IDM_FIXEDSPEED10,
		IDM_FIXEDSPEED5,
		IDM_FIXEDSPEED2,
		IDM_FIXEDSPEED1_5,
		IDM_FIXEDSPEED1_25,
		IDM_FIXEDSPEED1_1,
		IDM_FIXEDSPEED1,
		IDM_REALTIME,
		IDM_FIXEDSPEED0_9,
		IDM_FIXEDSPEED0_75,
		IDM_FIXEDSPEED0_5,
		IDM_FIXEDSPEED0_25,
		IDM_FIXEDSPEED0_1
	};

	char speed_caption[64+1];
	int i;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(256-64, 17*20);
	window_ptr = EG_Window_Create("speed:window", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define SPEED_CALC_LOC(r, b) loc = CalcRect(20, 10+r*17+b*10, win.w-40, 16)

	SPEED_CALC_LOC(0, 0); loc.x-=10; loc.w+=20;
	widget_ptr = EG_Label_Create("speed:title", col, EG_LABEL_ALIGN_CENTER, "Emulator speed:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	SPEED_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 15*17; loc.w+=20;
	widget_ptr = EG_Box_Create("speed:box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// [TODO] These are dependent on the model.
	group_ptr = EG_RadioGroup_Create("speed:group");

	for (i = 0; i < SPEED_COUNT; i++)
	{
		if (speed[i] == 1.0)
		{
			strcpy(speed_caption, "Real time");
		}
		else
		{
			sprintf(speed_caption, "%-1.1f", speed[i]);
		}

		SPEED_CALC_LOC((i + 2), 0);

		widget_ptr = EG_RadioButton_Create(NULL, col, speed_caption, loc);
		EG_RadioGroup_AddButton(group_ptr, widget_ptr);

		EG_RadioButton_SetMyCallback_OnClick(widget_ptr,
		                                     ProcessGUIOption,
		                                     CreateWindowsMenuItem(speed_menu_id[i], widget_ptr));

		if (speed[i] == 1.0)
		{
			EG_RadioGroup_Select(widget_ptr);
		}
	}

	EG_Window_AddWidget(window_ptr, group_ptr);

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_speed_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Speed_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_GetFocus(widget_ptr);

	gui.win_speed_ptr = window_ptr;
	return EG_TRUE;
}

static void Destroy_Speed()
{
	EG_Window_DestroyAllChildWidgets(gui.win_speed_ptr);
	EG_Window_Destroy(gui.win_speed_ptr);
}

/*      =       =       =       =       =       =       =       =       =
 *	Devices:
 *	=	=	=	=	=	=	=	=	=
 */

static void Devices_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_devices_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_Devices(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	int peripherals_count = 0;

	// Calc height of peripherals box

	#ifdef WITH_ECONET
	peripherals_count++;
	#endif

	// Harddrive enabled
	peripherals_count++;

	// Teletext
	peripherals_count++;

	// Border/Title
	if (peripherals_count > 0)
	{
		peripherals_count += 3;
	}

	// Adjust window size by peripherals box size
	peripherals_count *= 17;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(480-64, 17*8 + peripherals_count);
	window_ptr = EG_Window_Create("win_devices", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define DEVICES_CALC_LOC(r, b) loc = CalcRect(20, 10+r*17+b*8, win.w-40, 16)

	DEVICES_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_devices", col, EG_LABEL_ALIGN_CENTER, "Cheese Wedge:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DEVICES_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 17*3; loc.w+=20;
	widget_ptr = EG_Box_Create("system_model_box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DEVICES_CALC_LOC(2, 0); loc.w /= 2; loc.w -=5;
	widget_ptr = EG_ToggleButton_Create("system:tube:65C02", col, "65C02", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(IDM_TUBE_ACORN65C02, widget_ptr));

	//DEVICES_CALC_LOC(3, 0);
	loc.x+=loc.w + 10;
	widget_ptr = EG_ToggleButton_Create("system:tube:TorchZ80", col, "Torch Z80", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(IDM_TUBE_TORCHZ80, widget_ptr));

	DEVICES_CALC_LOC(3, 0); loc.w /= 2; loc.w -=5;
	widget_ptr = EG_ToggleButton_Create("system:tube:AcornZ80", col, "Acorn Z80", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(IDM_TUBE_ACORNZ80, widget_ptr));

	//DEVICES_CALC_LOC(5, 0);
	loc.x+=loc.w + 10;
	widget_ptr = EG_ToggleButton_Create("system:tube:i86", col, "Master 512 80186", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_ToggleButton_SetMyCallback_OnClick(widget_ptr,
	                                      ProcessGUIOption,
	                                      CreateWindowsMenuItem(IDM_TUBE_MASTER512, widget_ptr));

	if (peripherals_count > 0)
	{
		DEVICES_CALC_LOC(5, 0);
		widget_ptr = EG_Label_Create("system:network:title", col, EG_LABEL_ALIGN_CENTER, "Devices:", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);

		DEVICES_CALC_LOC(6,0); loc.x-=10; loc.y+=8; loc.h = peripherals_count-(17*2); loc.w+=20;
		widget_ptr = EG_Box_Create("system:network:box", EG_BOX_BORDER_SUNK, col, loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);

		int y = 7;

		#ifdef WITH_ECONET

		DEVICES_CALC_LOC(y, 0);
		widget_ptr = EG_TickBox_Create("per:econet", col, "Econet active", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);
		EG_TickBox_SetMyCallback_OnClick(widget_ptr,
		                                 ProcessGUIOption,
		                                 CreateWindowsMenuItem(IDM_ECONET, widget_ptr));
		y++;

		#endif

		DEVICES_CALC_LOC(y, 0);
		widget_ptr = EG_TickBox_Create("per:harddrive", col, "Hard drives active", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);
		EG_TickBox_SetMyCallback_OnClick(widget_ptr,
		                                 ProcessGUIOption,
		                                 CreateWindowsMenuItem(IDM_SCSI_HARD_DRIVE, widget_ptr));
		y++;

		// TODO: IDE hard drive

		DEVICES_CALC_LOC(y, 0);
		widget_ptr = EG_TickBox_Create("per:touchscreen", col, "Microvitec Touch Screen active", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);
		EG_TickBox_SetMyCallback_OnClick(widget_ptr,
		                                 ProcessGUIOption,
		                                 CreateWindowsMenuItem(IDC_SERIAL_PORT_DESTINATION_TOUCH_SCREEN, widget_ptr));
		y++;

		/*
		DEVICES_CALC_LOC(y, 0);
		widget_ptr = EG_TickBox_Create("per:teletext", col, "Teletext adapter active", loc);
		EG_Window_AddWidget(window_ptr, widget_ptr);
		EG_TickBox_SetMyCallback_OnClick(widget_ptr,
		                                 ProcessGUIOption,
		                                 CreateWindowsMenuItem(ID_TELETEXT, widget_ptr));
		y++;
		*/
	}

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_devices_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Devices_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_devices_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_Devices()
{
	EG_Window_DestroyAllChildWidgets(gui.win_devices_ptr);
	EG_Window_Destroy(gui.win_devices_ptr);
}


/*      =       =       =       =       =       =       =       =       =
 *	Tapes:
 *	=	=	=	=	=	=	=	=	=
 */

static void Tapes_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_tapes_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_Tapes(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;
	EG_Widget *group1_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(240, 240+64-16);
	window_ptr = EG_Window_Create("win_tapes", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define TAPES_CALC_LOC(r, b) loc = CalcRect(20, 10+r*20+b*10, win.w-40, 19)

	TAPES_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_tapes", col, EG_LABEL_ALIGN_CENTER, "Tape configuration:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	TAPES_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 16*8+4; loc.w+=10*2;
	widget_ptr = EG_Box_Create("tapes_box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Unlock Tape
	TAPES_CALC_LOC(2, 0);
	widget_ptr = EG_TickBox_Create("disks:Unlock", col, "Unlock tape", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_UNLOCKTAPE, widget_ptr));

	// Tape Speed
	group1_ptr = EG_RadioGroup_Create("tapespeed:group");

	TAPES_CALC_LOC(3, 1);
	widget_ptr = EG_RadioButton_Create("tapespeed:fast", col, "Fastest", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_TAPE_FAST, widget_ptr));

	TAPES_CALC_LOC(4, 1);
	widget_ptr = EG_RadioButton_Create("tapespeed:mfast", col, "Faster", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_TAPE_MFAST, widget_ptr));

	TAPES_CALC_LOC(5, 1);
	widget_ptr = EG_RadioButton_Create("tapespeed:mslow", col, "Fast", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_TAPE_MSLOW, widget_ptr));

	TAPES_CALC_LOC(6, 1);
	widget_ptr = EG_RadioButton_Create("tapespeed:normal", col, "Normal", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_TAPE_NORMAL, widget_ptr));

	// Insert tape
	TAPES_CALC_LOC(9, 0);
	widget_ptr = EG_Button_Create("disks:LoadTape", col, EG_BUTTON_ALIGN_CENTER, "Insert tape", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_LOADTAPE, widget_ptr));

	// Rewind tape
	TAPES_CALC_LOC(10, 0);
	widget_ptr = EG_Button_Create("disks:Rewind", col, EG_BUTTON_ALIGN_CENTER, "Rewind tape", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_REWINDTAPE, widget_ptr));

	// EG_RadioGroup_Select(widget_ptr);

	EG_Window_AddWidget(window_ptr, group1_ptr);


	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_tapes_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Tapes_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_tapes_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_Tapes()
{
	EG_Window_DestroyAllChildWidgets(gui.win_tapes_ptr);
	EG_Window_Destroy(gui.win_tapes_ptr);
}

/*      =       =       =       =       =       =       =       =       =
 *	Disks:
 *	=	=	=	=	=	=	=	=	=
 */

static void Disks_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_disks_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_Disks(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(480, 480);
	window_ptr = EG_Window_Create("win_disks", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define DISKS_CALC_LOC(r, b) loc = CalcRect(20, 10+r*20+b*10, win.w-40, 19)

	DISKS_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_disks", col, EG_LABEL_ALIGN_CENTER, "Disks:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_disks_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Disks_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_disks_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_Disks()
{
	EG_Window_DestroyAllChildWidgets(gui.win_disks_ptr);
	EG_Window_Destroy(gui.win_disks_ptr);
}


/*      =       =       =       =       =       =       =       =       =
 *	Keyboard:
 *	=	=	=	=	=	=	=	=	=
 */

static void Keyboard_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_keyboard_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static void Disks_SaveRegistry(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	mainWin->SavePreferences(true); // TODO: Save all?
}

void SetNameForDisc(int drive, char *name_ptr)
{
	unsigned int i;
	unsigned int f = 0;
	char buf[10*1024];
	char *buf_ptr;

	strcpy(buf, name_ptr);
	buf_ptr = buf + strlen(buf); // Not off by one, we can safely test /0x00
	qDEBUG("-> [Building short name]\n");
	for (i = 0; i <= strlen(buf); i++) // '<=' as we start from buf+strlen(buf)
	{
		if (*buf_ptr == '/' || *buf_ptr == '\\')
		{
			f=1;
			buf_ptr++;
			break;
		}
		else
		{
			buf_ptr--;
		}
	}
	if (f != 1) buf_ptr = buf;
	pDEBUG(dL"<- [Short name equals '%s']", dR, buf_ptr);

	if (drive == 0)
	{
		EG_Button_SetCaption(gui.widget_eject_disc0, buf_ptr);
	}
	else if (drive == 1)
	{
		EG_Button_SetCaption(gui.widget_eject_disc1, buf_ptr);
	}
}

static EG_BOOL Make_Keyboard(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(480, 480);
	window_ptr = EG_Window_Create("win_keyboard", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define KEYBOARD_CALC_LOC(r, b) loc = CalcRect(20, 10+r*20+b*10, win.w-40, 16)

	KEYBOARD_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_keyboard", col, EG_LABEL_ALIGN_CENTER, "Keyboard:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC(3, 0);
	widget_ptr = EG_Button_Create("disks:Drive:run_disc", col, EG_BUTTON_ALIGN_CENTER, "Select, load and run a disc", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, RunDisc, window_ptr);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC(4, 0);
	widget_ptr = EG_Button_Create("disks:Drive0:load_state", col, EG_BUTTON_ALIGN_CENTER, "Load state", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, LoadState, window_ptr);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC(5, 0);
	widget_ptr = EG_Button_Create("disks:Drive0:load_disk", col, EG_BUTTON_ALIGN_CENTER, "Change disk in drive 0", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_LOADDISC0, widget_ptr));

	DISKS_CALC_LOC(6, 0);
	widget_ptr = EG_Button_Create("disks:Drive1:load_disk", col, EG_BUTTON_ALIGN_CENTER, "Change disk in drive 1", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_LOADDISC1, widget_ptr));

	DISKS_CALC_LOC(8, 0);
	widget_ptr = EG_Button_Create("disks:Drive0:new_disk", col, EG_BUTTON_ALIGN_CENTER, "Create new disc image in drive 0", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_NEWDISC0, widget_ptr));

	DISKS_CALC_LOC(9, 0);
	widget_ptr = EG_Button_Create("disks:Drive1:new_disk", col, EG_BUTTON_ALIGN_CENTER, "Create new disc image in drive 1", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_NEWDISC1, widget_ptr));


	DISKS_CALC_LOC( (8+3), 0);
	widget_ptr = EG_Button_Create("disks:Drive0:save_state", col, EG_BUTTON_ALIGN_CENTER, "Save state", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, SaveState, window_ptr);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC( (10+3), 0);
	widget_ptr = EG_TickBox_Create("disks:WriteProtect0:yes/no", col, "Disk 0: Read only (click below to eject)", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_WRITE_PROTECT_DISC0, widget_ptr));

	DISKS_CALC_LOC( (11+3), 0);
	widget_ptr = EG_Button_Create("disks:Eject:Disc0", col, EG_BUTTON_ALIGN_CENTER, "", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_SetMyCallback_OnClick(widget_ptr,
	                                ProcessGUIOption,
	                                CreateWindowsMenuItem(IDM_EJECTDISC0, widget_ptr));
	gui.widget_eject_disc0 = widget_ptr;

	DISKS_CALC_LOC( (13+3), 0);
	widget_ptr = EG_TickBox_Create("disks:WriteProtect1:yes/no", col, "Disk 1: Read only (click below to eject)", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_WRITE_PROTECT_DISC1, widget_ptr));

	DISKS_CALC_LOC( (14+3), 0);
	widget_ptr = EG_Button_Create("disks:Eject:Disc1", col, EG_BUTTON_ALIGN_CENTER, "", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_Button_SetMyCallback_OnClick(widget_ptr, ProcessGUIOption
	, CreateWindowsMenuItem(IDM_EJECTDISC1, widget_ptr));
	gui.widget_eject_disc1 = widget_ptr;


	col = CalcColor(191+32, 0, 0);
	DISKS_CALC_LOC( (17+1), 0);
	widget_ptr = EG_Label_Create("disks:lab_1", col, EG_LABEL_ALIGN_LEFT, "Watch out for GTK opening the file selector", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC( (18+1), 0); loc.y--;
	widget_ptr = EG_Label_Create("disks:lab_2", col, EG_LABEL_ALIGN_LEFT, "behind the emulator window!                ", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);

	/*
	DISKS_CALC_LOC(15, 0);
	widget_ptr = EG_Button_Create("disks:nativeFDC", col, EG_BUTTON_ALIGN_CENTER, "Native 8271", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Disks_NativeDFS, NULL);

	DISKS_CALC_LOC(16, 0);
	widget_ptr = EG_Button_Create("disks:acorn", col, EG_BUTTON_ALIGN_CENTER, "Acorn 1770 DFS Board Drive Controller", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Disks_AcornDFS, NULL);

	DISKS_CALC_LOC(17, 0);
	widget_ptr = EG_Button_Create("disks:watford", col, EG_BUTTON_ALIGN_CENTER, "Watford DDFS Ext. board for Model B", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Disks_WatfordDFS, NULL);

	DISKS_CALC_LOC(18, 0);
	widget_ptr = EG_Button_Create("disks:opus", col, EG_BUTTON_ALIGN_CENTER, "Opus DDFS Board Drive Controller", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Disks_OpusDFS, NULL);

	DISKS_CALC_LOC(19, 0); loc.y--;
	widget_ptr = EG_Label_Create("disks:fdc_label", col, EG_LABEL_ALIGN_CENTER, "", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	gui.widget_fdc_label = widget_ptr;
	*/

	DISKS_CALC_LOC(2, 0); loc.y-=8;
	widget_ptr = EG_Label_Create("disks:lab_2", col, EG_LABEL_ALIGN_CENTER, "(this mess is only temporary!)", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	DISKS_CALC_LOC(20, 0);
	widget_ptr = EG_Button_Create("disks:save_config", col, EG_BUTTON_ALIGN_CENTER, "Save configuration", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr, Disks_SaveRegistry, NULL);

	// SYSTEM_CALC_LOC(21, 0);
	// widget_ptr = EG_TickBox_Create("sys_eco", col, "Econet", loc);
	// EG_TickBox_Disable(widget_ptr);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// group1_ptr = EG_RadioGroup_Create("system_model");

	// SYSTEM_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 20*5; loc.w+=20;
	// widget_ptr = EG_Box_Create("system_model_box", EG_BOX_BORDER_SUNK, col, loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(2, 0);
	// widget_ptr = EG_RadioButton_Create("model_b", col, "BBC Model B", loc);
	// EG_RadioGroup_AddButton(group1_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(3, 0);
	// widget_ptr = EG_RadioButton_Create("model_b_and_integra_b", col, "BBC Model B + Integra-B", loc);
	// EG_RadioButton_Disable(widget_ptr);
	// EG_RadioGroup_AddButton(group1_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(4, 0);
	// widget_ptr = EG_RadioButton_Create("model_b_plus", col, "BBC Model B Plus", loc);
	// EG_RadioButton_Disable(widget_ptr);
	// EG_RadioGroup_AddButton(group1_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(5, 0);
	// widget_ptr = EG_RadioButton_Create("model_master_128", col, "BBC Master 128", loc);
	// EG_RadioGroup_AddButton(group1_ptr, widget_ptr);

	// EG_Window_AddWidget(window_ptr, group1_ptr);


	// SYSTEM_CALC_LOC(19, 0);
	// widget_ptr = EG_Label_Create("lab_econet", col, EG_LABEL_ALIGN_CENTER, "Network:", loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(20,0); loc.x-=10; loc.y+=8; loc.h = 20*2; loc.w+=20;
	// widget_ptr = EG_Box_Create("eco_box", EG_BOX_BORDER_SUNK, col, loc);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// SYSTEM_CALC_LOC(21, 0);
	// widget_ptr = EG_TickBox_Create("sys_eco", col, "Econet", loc);
	// EG_TickBox_Disable(widget_ptr);
	// EG_Window_AddWidget(window_ptr, widget_ptr);

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_keyboard_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, Keyboard_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_keyboard_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_Keyboard()
{
	EG_Window_DestroyAllChildWidgets(gui.win_keyboard_ptr);
	EG_Window_Destroy(gui.win_keyboard_ptr);
}

/*      =       =       =       =       =       =       =       =       =
 *	AMX:
 *	=	=	=	=	=	=	=	=	=
 */

static void AMX_Button_Back(EG_Widget * /* widget_ptr */, void * /* user_ptr */)
{
	EG_Window_Hide(gui.win_amx_ptr);
	SDL_Delay(100);
	EG_Window_Show(gui.win_menu_ptr);
	SetActiveWindow(gui.win_menu_ptr);
}

static EG_BOOL Make_AMX(SDL_Surface *dst_ptr)
{
	EG_Window *window_ptr = NULL;
	EG_Widget *widget_ptr;
	EG_Widget *group1_ptr, *group2_ptr;

	SDL_Rect win, loc;
	SDL_Color col;

	// Window
	col = MENU_COLORS; //CalcColor(191+32, 191+32, 0);
	win = CalcRectCentered(480-256, 512-128-16-8);
	window_ptr = EG_Window_Create("win_amx", dst_ptr, col, win);
	win.x=0; win.y=0; win.w-=4; win.h-=4;

	// Title label
	#define AMX_CALC_LOC(r, b) loc = CalcRect(20, 10+r*20+b*10, win.w-40, 16)

	AMX_CALC_LOC(0, 0);
	widget_ptr = EG_Label_Create("lab_amx", col, EG_LABEL_ALIGN_CENTER, "AMX:", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	AMX_CALC_LOC(1,0); loc.x-=10; loc.y+=8; loc.h = 16*17; loc.w+=20;
	widget_ptr = EG_Box_Create("system_model_box", EG_BOX_BORDER_SUNK, col, loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	AMX_CALC_LOC(2, 0);
	widget_ptr = EG_TickBox_Create("devices:AMX", col, "On/Off", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMXONOFF, widget_ptr));

	AMX_CALC_LOC(3, 1);
	widget_ptr = EG_TickBox_Create("amx:LR_middle", col, "L+R for Middle", loc);
	EG_Window_AddWidget(window_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_LRFORMIDDLE, widget_ptr));

	group1_ptr = EG_RadioGroup_Create("amx:map");

	AMX_CALC_LOC(4, 2);
	widget_ptr = EG_RadioButton_Create("amx:map160x256", col, "Map to 160x256", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_160X256, widget_ptr));

	AMX_CALC_LOC(5, 2);
	widget_ptr = EG_RadioButton_Create("amx:map320x256", col, "Map to 320x256", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_320X256, widget_ptr));

	AMX_CALC_LOC(6, 2);
	widget_ptr = EG_RadioButton_Create("amx:map540x256", col, "Map to 640x256", loc);
	EG_RadioGroup_AddButton(group1_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_640X256, widget_ptr));

	EG_Window_AddWidget(window_ptr, group1_ptr);


	group2_ptr = EG_RadioGroup_Create("amx:adjust");

	AMX_CALC_LOC(7, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust+50%", col, "Adjust +50%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTP50, widget_ptr));

	AMX_CALC_LOC(8, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust+30%", col, "Adjust +30%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTP30, widget_ptr));

	AMX_CALC_LOC(9, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust+10%", col, "Adjust +10%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTP10, widget_ptr));

	AMX_CALC_LOC(10, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust-10%", col, "Adjust -10%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTM10, widget_ptr));

	AMX_CALC_LOC(11, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust-30%", col, "Adjust -30%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTM30, widget_ptr));

	AMX_CALC_LOC(12, 3);
	widget_ptr = EG_RadioButton_Create("amx:adjust-50%", col, "Adjust -50%", loc);
	EG_RadioGroup_AddButton(group2_ptr, widget_ptr);
	EG_TickBox_SetMyCallback_OnClick(widget_ptr,
	                                 ProcessGUIOption,
	                                 CreateWindowsMenuItem(IDM_AMX_ADJUSTM50, widget_ptr));

	EG_Window_AddWidget(window_ptr, group2_ptr);

	// Back button
	loc = CalcRect(win.w - 10 - 64, win.h-10-20, 64, 20);
	widget_ptr = EG_Button_Create("but_amx_back", col, EG_BUTTON_ALIGN_CENTER, "Back", loc);
	EG_Button_SetMyCallback_OnClick(widget_ptr, AMX_Button_Back, &gui);
	EG_Window_AddWidget(window_ptr, widget_ptr);

	gui.win_amx_ptr = window_ptr;

	return EG_TRUE;
}

static void Destroy_AMX()
{
	EG_Window_DestroyAllChildWidgets(gui.win_amx_ptr);
	EG_Window_Destroy(gui.win_amx_ptr);
}
