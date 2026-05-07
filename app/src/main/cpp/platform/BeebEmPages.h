// Android shadow of core/BeebEmPages.h — no SDL dependency.

#ifndef BEEBEMPAGES_HEADER
#define BEEBEMPAGES_HEADER

// On Android we don't use the SDL widget toolkit.
// EG_Widget already declared in Sdl.h; EG_Window is a void alias.
typedef void EG_Window;
// EG_Widget also declared in Sdl.h; redeclare here for headers that don't include Sdl.h.
#ifndef EG_WIDGET_DEFINED
#define EG_WIDGET_DEFINED
typedef void EG_Widget;
#endif

struct BeebEmGUI {
    EG_Window *win_menu_ptr;
    EG_Window *win_system_ptr;
    EG_Window *win_screen_ptr;
    EG_Window *win_sound_ptr;
    EG_Window *win_roms_ptr;
    EG_Window *win_speed_ptr;
    EG_Window *win_about_ptr;
    EG_Window *win_devices_ptr;
    EG_Window *win_disks_ptr;
    EG_Window *win_tapes_ptr;
    EG_Window *win_keyboard_ptr;
    EG_Window *win_amx_ptr;

    EG_Widget *widget_okay_ptr;
    EG_Widget *widget_reset_ptr;
    EG_Widget *widget_no_reset_ptr;
    EG_Widget *fullscreen_widget_ptr;

    EG_Widget *widget_system_button;

    EG_Widget *widget_machine_bbc_b;
    EG_Widget *widget_machine_integra_b;
    EG_Widget *widget_machine_bbc_b_plus;
    EG_Widget *widget_machine_bbc_master_128;
    EG_Widget *widget_machine_bbc_master_et;

    EG_Widget *widget_fdc_label;
    EG_Widget *widget_system_back;
    EG_Widget *widget_about_slider;

    EG_Widget *widget_fdc_none;
    EG_Widget *widget_fdc_acorn_1770;
    EG_Widget *widget_fdc_watford;
    EG_Widget *widget_fdc_opus;

    EG_Widget *widget_about[24];

    EG_Widget *widget_windowed_640x512;
    EG_Widget *widget_windowed_640x480_S;
    EG_Widget *widget_windowed_640x480_V;
    EG_Widget *widget_windowed_320x240_S;
    EG_Widget *widget_windowed_320x240_V;
    EG_Widget *widget_windowed_320x256;

    EG_Widget *widget_fullscreen_640x512;
    EG_Widget *widget_fullscreen_640x480_S;
    EG_Widget *widget_fullscreen_640x480_V;
    EG_Widget *widget_fullscreen_320x240_S;
    EG_Widget *widget_fullscreen_320x240_V;
    EG_Widget *widget_fullscreen_320x256;

    EG_Widget *widget_eject_disc0;
    EG_Widget *widget_eject_disc1;
};

extern BeebEmGUI gui;

void Show_Main();
bool InitializeBeebEmGUI(void *screen_ptr);
void DestroyBeebEmGUI();

int  UpdateGUIOption(int windows_menu_id, int is_selected);
int  GetGUIOption(int windows_menu_id);
int  SetGUIOptionCaption(int windows_menu_id, const char *str);

void Update_FDC_Buttons();
void Update_Resolution_Buttons();
void SetNameForDisc(int drive, char *name_ptr);
void SetFullScreenTickbox(bool State);
void ClearWindowsBackgroundCacheAndResetSurface();

#endif // BEEBEMPAGES_HEADER
