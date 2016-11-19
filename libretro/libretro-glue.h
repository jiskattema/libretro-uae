#ifndef LIBRETRO_HATARI_H
#define LIBRETRO_HATARI_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <libco/libco.h>
#include "libretro.h"

extern cothread_t mainThread;
extern cothread_t emuThread;

#define Uint8 unsigned char
#define Uint16 unsigned short int
#define Uint32 unsigned int

#include <stdbool.h>

#define LOGI printf
#define MAX_WIDTH (320 << 2)
#define MAX_HEIGHT 400

extern int retrow; 
extern int retroh;
extern unsigned short int retro_bmp[MAX_WIDTH * MAX_HEIGHT];

#define RETRO_LINE_LENGTH 255
extern struct retro_message rmsg;
extern retro_environment_t environ_cb; // used in sources/src/caps/caps.c
extern char rcapsdir[RETRO_LINE_LENGTH+1]; // for capsimg.so: retro_system_dir
extern int rqsmode;
extern int rconfig;
extern int rcompat;
extern int rres; // 0 - 2
extern int rspeed; // 1 - 6

extern void check_roms(char *path);
extern void retro_audio_cb(short l, short r);

extern bool retro_capslock;
extern const int keyboard_translation[512];

#endif
