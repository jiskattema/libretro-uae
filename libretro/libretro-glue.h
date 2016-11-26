#ifndef LIBRETRO_GLUE_H
#define LIBRETRO_GLUE_H

#include <libco/libco.h>
#include "libretro.h"

extern cothread_t mainThread;
extern cothread_t emuThread;

#define RETRO_LINE_LENGTH 255
#define MAX_WIDTH (320 << 2)
#define MAX_HEIGHT 400

extern int retrow; 
extern int retroh;
extern unsigned short int retro_bmp[MAX_WIDTH * MAX_HEIGHT];

extern char rcapsdir[RETRO_LINE_LENGTH+1];

extern int retro_prefs_changed;
extern int rqsmode;
extern int rconfig;
extern int rcompat;
extern int rres;   // 0 - 2
extern int rspeed; // 1 - 6
extern int rdiskspeed; // 100 - 200 - 400 - 800 - 0

extern void check_roms(char *path);
extern void retro_audio_cb(short l, short r);
extern void retro_update_input(void);
extern void retro_keypress(bool down, unsigned keycode, uint32_t character, uint16_t mods);

extern bool retro_capslock;
extern const int keyboard_translation[512];

#endif
