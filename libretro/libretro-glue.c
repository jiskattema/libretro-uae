/********************************************

        RETRO GLUE

*********************************************/
 
#include <string.h>
#include <stdlib.h>

#include "sysdeps.h"

#include "options.h"

#include "xwin.h"
#include "custom.h"

#include "inputdevice.h"
#include "hotkeys.h"

#include "libretro.h"
#include "libretro-glue.h"

#include "audio.h"
#include "drawing.h"

// defined in sources/src/inputdevice.c, but in any header file
extern void inputdevice_release_all_keys();

static int prefs_changed = 0;

void gui_init (int argc, char **argv) {
}

void target_save_options (struct zfile* f, struct uae_prefs *p) {
}

int target_parse_option (struct uae_prefs *p, const char *option, const char *value) {
    return 0;
}

void target_default_options (struct uae_prefs *p, int type) {
  p->start_gui = 0;
  p->floppy_speed = 100;
  p->leds_on_screen = 1;

  p->produce_sound = 3;
  p->sound_freq = 22050;
  p->sound_stereo = SND_STEREO;
  p->sound_interpol = 0;

  p->ntscmode = 0;

  p->gfx_framerate = 1;
  p->gfx_xcenter = 1;
  p->gfx_ycenter = 1;
}

int retro_render_sound(short* samples, int sampleCount) {
   int i; 

   if (sampleCount < 1)
      return 0;

   for(i=0;i<sampleCount;i+=2)
   {
      retro_audio_cb(samples[i], samples[i+1]);
   }
}

void retro_flush_screen (struct vidbuf_description *gfxinfo, int ystart, int yend) {
    co_switch(mainThread);
}

void retro_flush_block (struct vidbuf_description *gfxinfo, int ystart, int yend) {
}

void retro_flush_line (struct vidbuf_description *gfxinfo, int y) {
}

void retro_flush_clear_screen(struct vidbuf_description *gfxinfo) {
}

int retro_lockscr(struct vidbuf_description *gfxinfo) {
   return 1;
}

void retro_unlockscr(struct vidbuf_description *gfxinfo) {
}

int graphics_init(void) {
    currprefs.gfx_resolution = rres;
    currprefs.gfx_size_win.width=retrow;
    currprefs.gfx_size_win.height=retroh;

    memset(&retro_bmp[0], 0x80, currprefs.gfx_size_win.width * currprefs.gfx_size_win.height * 2);

    gfxvidinfo.width_allocated = retrow;
    gfxvidinfo.height_allocated = retroh;
    gfxvidinfo.inwidth = retrow;
    gfxvidinfo.inwidth2 = gfxvidinfo.inwidth;

    gfxvidinfo.maxblocklines = 1000;
    gfxvidinfo.pixbytes = 2;
    gfxvidinfo.rowbytes = gfxvidinfo.width_allocated * gfxvidinfo.pixbytes ;
    gfxvidinfo.bufmem = (unsigned char*) &retro_bmp[0];
    gfxvidinfo.emergmem = 0;
    gfxvidinfo.linemem = 0;

    gfxvidinfo.lockscr = retro_lockscr;
    gfxvidinfo.unlockscr = retro_unlockscr;
    gfxvidinfo.flush_block = retro_flush_block;
    gfxvidinfo.flush_clear_screen = retro_flush_clear_screen;
    gfxvidinfo.flush_screen = retro_flush_screen;
    gfxvidinfo.flush_line = retro_flush_line;

    prefs_changed = 1;
    inputdevice_release_all_keys ();
    reset_hotkeys ();
    drawing_init ();

    return 1;
}

int graphics_setup(void) {
    //32bit mode
    //Rw, Gw, Bw,   Rs, Gs, Bs,   Aw, As, Avalue, swap
    alloc_colors64k (5, 6, 5, 11, 5, 0, 0, 0, 0, 0); 

    return 1;
}

void graphics_leave(void) {
}

int gfx_parse_option (struct uae_prefs *p, const char *option, const char *value) {
    return 0;
}

void gfx_default_options(struct uae_prefs *p) {
}

void screenshot (int type,int f) {
}

void toggle_fullscreen(int mode) {
}

int check_prefs_changed_gfx (void) {
    if (prefs_changed) {
        prefs_changed = 0;
        return 1;
    }
    return 0;
}

/***************************************************************
  Joystick functions
****************************************************************/

static int init_joysticks (void) {
   return 1;
}

static void close_joysticks (void) {
}

static int acquire_joystick (int num, int flags) {
    return 1;
}

static int get_joystick_flags (int num) {
    return 0;
}

static void unacquire_joystick (int num) {
}

static void read_joysticks (void) {
}

static int get_joystick_num (void) {
    return 2;
}

static int get_joystick_widget_num (int joy) {
    return 0;
}

static int get_joystick_widget_type (int joy, int num, char *name, uae_u32 *code) {
    return IDEV_WIDGET_BUTTON;
}

static int get_joystick_widget_first (int joy, int type) {
    return -1;
}

static TCHAR *get_joystick_friendlyname (int joy) {
  switch (joy) {
    case 0: return "Retro pad 0";
    case 1: return "Retro pad 1";
    default: return "Retro pad 2";
  }
}

static char *get_joystick_uniquename (int joy) {
  switch (joy) {
    case 0: return "retropad0";
    case 1: return "retropad1";
    default: return "retropad2";
  }
}

struct inputdevice_functions inputdevicefunc_joystick = {
    init_joysticks, 
    close_joysticks, 
    acquire_joystick,
    unacquire_joystick,
    read_joysticks, 
    get_joystick_num, 
    get_joystick_friendlyname,
    get_joystick_uniquename,
    get_joystick_widget_num,
    get_joystick_widget_type,
    get_joystick_widget_first,
    get_joystick_flags
};

int input_get_default_joystick (struct uae_input_device *uid, int num, int port, int af, int mode, bool gp) {
/*
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   =  INPUTEVENT_JOY1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   =  INPUTEVENT_JOY1_VERT;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] =  INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] =  INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] =  INPUTEVENT_JOY1_3RD_BUTTON;
*/
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   =  INPUTEVENT_JOY2_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   =  INPUTEVENT_JOY2_VERT;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] =  INPUTEVENT_JOY2_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] =  INPUTEVENT_JOY2_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] =  INPUTEVENT_JOY2_3RD_BUTTON;

    uid[0].enabled = 1;

    return 1;
}


/***************************************************************
  Mouse functions
****************************************************************/

/*
 * Mouse inputdevice functions
 */

/* Hardwire for 3 axes and 3 buttons
 * There is no 3rd axis as such - mousewheel events are
 * supplied by X on buttons 4 and 5.
 */
#define MAX_BUTTONS     3
#define MAX_AXES        2
#define FIRST_AXIS      0
#define FIRST_BUTTON    MAX_AXES

static int init_mouse (void) {
   return 1;
}

static void close_mouse (void) {
   return;
}

static int acquire_mouse (int num, int flags) {
    return 1;
}

static void unacquire_mouse (int num) {
    return;
}

static int get_mouse_num (void) {
    return 2;
}

static TCHAR *get_mouse_friendlyname (int mouse) {
  switch (mouse) {
    case 0: return "Default mouse 0";
    case 1: return "Default mouse 1";
    default: return "Default mouse 2";
  };
}

static TCHAR *get_mouse_uniquename (int mouse) {
  switch (mouse) {
    case 0: return "defaultmouse0";
    case 1: return "defaultmouse1";
    default: return "defaultmouse2";
  };
}

static int get_mouse_widget_num (int mouse) {
    return MAX_AXES + MAX_BUTTONS;
}

static int get_mouse_widget_first (int mouse, int type) {
  switch (type) {
    case IDEV_WIDGET_BUTTON: return FIRST_BUTTON;
    case IDEV_WIDGET_AXIS: return FIRST_AXIS;
  }
  return -1;
}

static int get_mouse_widget_type (int mouse, int num, TCHAR *name, uae_u32 *code) {
    if (num >= MAX_AXES && num < MAX_AXES + MAX_BUTTONS) {
        if (name) sprintf (name, "Button %d", num + 1 + MAX_AXES);
        return IDEV_WIDGET_BUTTON;
    } else if (num < MAX_AXES) {
        if (name) sprintf (name, "Axis %d", num + 1);
        return IDEV_WIDGET_AXIS;
    }
    return IDEV_WIDGET_NONE;
}

static void read_mouse (void) {
}

static int get_mouse_flags (int num) {
    return 0;
}

struct inputdevice_functions inputdevicefunc_mouse = {
    init_mouse,
    close_mouse,
    acquire_mouse,
    unacquire_mouse,
    read_mouse,
    get_mouse_num,
    get_mouse_friendlyname,
    get_mouse_uniquename,
    get_mouse_widget_num,
    get_mouse_widget_type,
    get_mouse_widget_first,
    get_mouse_flags
};

int input_get_default_mouse (struct uae_input_device *uid, int num, int port, int af, bool gp, bool wheel) {
    /* Supports only one mouse */
    uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
    uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
    uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
    uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
    uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
    uid[0].enabled = 1;
    return 0;
}

/***************************************************************
  Keyboard functions
****************************************************************/

static int init_kb (void) {
    return 1;
}

static void close_kb (void) {
}

static int acquire_kb (int num, int flags)
{
    return 1;
}

static void unacquire_kb (int num) {
}

static void read_kb (void) {
}

static int get_kb_num (void) {
    return 1;
}

static char *get_kb_uniquename (int mouse)
{
    return "Default keyboard";
}

static char *get_kb_friendlyname (int mouse) {
    return "Default keyboard";
}

static int get_kb_widget_num (int mouse) {
    return 255; //fix me
}

static int get_kb_widget_type (int mouse, int num, char *name, uae_u32 *code) {
    return IDEV_WIDGET_NONE;
}

static int get_kb_widget_first (int mouse, int type) {
    return 0;
}

static int get_kb_flags (int num) {
    return 0;
}

struct inputdevice_functions inputdevicefunc_keyboard = {
    init_kb, 
    close_kb, 
    acquire_kb, 
    unacquire_kb,
    read_kb, 
    get_kb_num, 
    get_kb_friendlyname,
    get_kb_uniquename,
    get_kb_widget_num, 
    get_kb_widget_type,
    get_kb_widget_first,
    get_kb_flags
};

int getcapslockstate (void) {
  return retro_capslock;
}

void setcapslockstate (int state) {
  retro_capslock = state;
}

/********************************************************************
    Misc fuctions
*********************************************************************/

int needmousehack(void) {
    return 0;
}

void toggle_mousegrab(void) {
}

int handle_options_events() {
    return 0;
}

bool handle_events() {
    return 0;
}

void uae_pause (void) {
}

void uae_resume (void) {
}
