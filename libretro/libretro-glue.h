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

#define TEX_WIDTH 400
#define TEX_HEIGHT 300

extern int retrow; 
extern int retroh;
extern int CROP_WIDTH;
extern int CROP_HEIGHT;
extern int VIRTUAL_WIDTH ;
extern unsigned short int retro_bmp[1024*1024];

#define RETRO_LINE_LENGTH 255
extern struct retro_message rmsg;
extern retro_environment_t environ_cb; // used in sources/src/caps/caps.c
extern char rcapsdir[RETRO_LINE_LENGTH+1]; // for capsimg.so: retro_system_dir
extern int rqsmode;
extern int rconfig;
extern int rcompat;
extern int rres;
extern int rspeed; // 1 - 6

extern void check_roms(char *path);
extern void retro_audio_cb(short l, short r);

#define NPLGN 10
#define NLIGN 5
#define NLETT 5

#define XSIDE  (CROP_WIDTH/NPLGN -1)
#define YSIDE  (CROP_HEIGHT/8 -1)

#define YBASE0 (CROP_HEIGHT - NLIGN*YSIDE -8)
#define XBASE0 0+4+2
#define XBASE3 0
#define YBASE3 YBASE0 -4

#define STAT_DECX 120
#define STAT_YSZ  20

#define RGB565(r, g, b)  (((r) << (5+6)) | ((g) << 6) | (b))

#endif
