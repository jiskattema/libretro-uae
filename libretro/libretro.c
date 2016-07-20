#include <stdio.h>
#include "libretro.h"
#include "libretro-glue.h"

// for struct uae_prefs (options.h) and serialization (savestate.h)
#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "savestate.h"

const char *fname = "/dev/shm/puae.asf";

cothread_t mainThread;
cothread_t emuThread;

int retrow=380; 
int retroh=256;
int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH;
int sndbufpos=0;
char key_state[512];
char key_state2[512];
bool opt_analog=false;
static int firstps=0;

extern unsigned short int  bmp[1024*1024];
extern unsigned short int  savebmp[1024*1024];
extern int pauseg;
extern int SND;
extern int SHIFTON;
extern int snd_sampler;
extern short signed int SNDBUF[1024*2];
extern char RPATH[512];
int ledtype;
int rqsmode;
int rconfig;
int rcompat;
int rres;

extern void update_input(void);

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

extern struct uae_prefs currprefs;

static struct retro_input_descriptor input_descriptors[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Fire" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Enter GUI" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Mouse mode toggle" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Keyboard overlay" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Toggle m/k status" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Joystick number" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Mouse speed" },
   // Terminate
   { 255, 255, 255, 255, NULL }
};


void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      { "analog","Use Analog; OFF|ON", },
      { "leds","Leds; Standard|Simplified|None", },
      { "rres","Resolution; Low|High|SuperHigh", },
      { "rqsmode","Machine; A500|A500+|A600|A1000|A1200|A3000|A4000|CD32|CDTV|ARCADIA", },
      { "rconfig","Configuration; 0|1|2|3|4|5", },
      { "rcompat","Compatibility; Exact|High|Low|Fast", },
      { NULL, NULL },

   };

   // bool no_rom = true;
   // cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

static void update_variables(void)
{
   struct retro_variable var = {0};

   var.key = "analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      fprintf(stderr, "value: %s\n", var.value);
      if (strcmp(var.value, "OFF") == 0)
        opt_analog = false;
      if (strcmp(var.value, "ON") == 0)
        opt_analog = true;

      fprintf(stderr, "[libretro-test]: Analog: %s.\n",opt_analog?"ON":"OFF");
   }
   
   var.key = "leds";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      fprintf(stderr, "value: %s\n", var.value);
      if (strcmp(var.value, "Standard") == 0)   ledtype = 0;
      if (strcmp(var.value, "Simplified") == 0) ledtype = 1;
      if (strcmp(var.value, "None") == 0)       ledtype = 2;
   }
   
   var.key = "rqsmode";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (strcmp("A500"   , var.value) == 0) {rqsmode = 0;}
      else if (strcmp("A500+"  , var.value) == 0) {rqsmode = 1;}
      else if (strcmp("A600"   , var.value) == 0) {rqsmode = 2;}
      else if (strcmp("A1000"  , var.value) == 0) {rqsmode = 3;}
      else if (strcmp("A1200"  , var.value) == 0) {rqsmode = 4;}
      else if (strcmp("A3000"  , var.value) == 0) {rqsmode = 5;}
      else if (strcmp("A4000"  , var.value) == 0) {rqsmode = 6;}
      else if (strcmp("CD32"   , var.value) == 0) {rqsmode = 8;}
      else if (strcmp("CDTV"   , var.value) == 0) {rqsmode = 9;}
      else if (strcmp("ARCADIA", var.value) == 0) {rqsmode = 10;}
      
      fprintf(stderr, "value: %s %i\n", var.value, rqsmode);
   }

   var.key = "rconfig";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      rconfig = atoi (var.value);
      fprintf(stderr, "value: %s %i\n", var.value, rconfig);
   }

   var.key = "rcompat";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (strcmp(var.value, "Exact") == 0) {rcompat = 0;}
      else if (strcmp(var.value, "High") == 0)  {rcompat = 1;}
      else if (strcmp(var.value, "Low") == 0)   {rcompat = 2;}
      else if (strcmp(var.value, "Fast") == 0)  {rcompat = 3;}
   }

   var.key = "rres";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (strcmp(var.value, "Low") == 0)       {rres = 0;}
      else if (strcmp(var.value, "High") == 0)      {rres = 1;}
      else if (strcmp(var.value, "SuperHigh") == 0) {rres = 2;}

      retrow = (752 / 2) << rres;
      if (rres == 0) {
          // retrow = 258 + 136;
          retroh = 400 + 36;
      } else if (rres == 1) {
          // retrow = 640 + 58;
          retroh = 480 + 36;
      } else if (rres == 2) {
          // retrow = 640 + 58;
          retroh = 480 + 36;
      }
      graphics_init();
   }
}

static void retro_wrap_emulator(void)
{
   static const char* argv[2];
   argv[0] = "puae";
   argv[1] = RPATH;
   umain(2,argv);

   pauseg=-1;

   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

   /* We're done here */
   co_switch(mainThread);

   /* Dead emulator, 
    * but libco says not to return. */
   while(true)
   {
      LOGI("Running a dead emulator.");
      co_switch(mainThread);
   }
}

void retro_init(void)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   memset(key_state,0,512);
   memset(key_state2,0,512);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "RGB565 is not supported.\n");
      exit(0);//return false;
   }

   InitOSGLU();
   memset(bmp, 0, sizeof(bmp));

   update_variables();

   CROP_WIDTH    = retrow;
   CROP_HEIGHT   = (retroh - 80);
   VIRTUAL_WIDTH = retrow;

   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(65536 * sizeof(void*), retro_wrap_emulator);
   }
}

void retro_deinit(void)
{	
   UnInitOSGLU();	

   if(emuThread)
      co_delete(emuThread);
   emuThread = 0;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "PUAE";
   info->library_version  = "v2.6.1mod";
   info->need_fullpath    = true;
   info->block_extract    = false;	
   info->valid_extensions = "adf|dms|fdi|ipf|zip|uae";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   struct retro_game_geometry geom   = { 640,480,1024,1024,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_shutdown_uae(void)
{
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void){
   uae_reset(1,1);
}

void retro_audio_cb( short l, short r)
{
   audio_cb(l,r);
}

extern unsigned short * sndbuffer;
extern int sndbufsize;
signed short rsnd=0;

static firstpass=1;

int save_bkg(void)
{
   memcpy(savebmp, bmp,sizeof(bmp));
}

int restore_bkg(void)
{
   memcpy(bmp,savebmp,sizeof(bmp));
}

void enter_gui0(void)
{
   save_bkg();

   Dialog_DoProperty();
   pauseg=0;

   restore_bkg();
}

void pause_select(void)
{
   if(pauseg==1 && firstps==0)
   {
      firstps=1;
      enter_gui0();
      firstps=0;
   }
}

void retro_run(void)
{
   int x;

   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if(pauseg==0)
   {
      if(firstpass)
      {
         firstpass=0;
         goto sortie;
      }
      update_input();	
   }

sortie:

   video_cb(bmp,retrow,retroh , retrow << 1);

   co_switch(emuThread);
}

bool retro_load_game(const struct retro_game_info *info)
{
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_descriptors);

   RPATH[0] = "\0";

   if (*info->path)
   {
      const char *full_path = (const char*)info->path;
      strcpy(RPATH,full_path);
      fprintf(stderr, "FIXME load game '%s'\n", RPATH);
   }
  
   return true;
}

void retro_unload_game(void)
{
   pauseg=0;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   size_t size = 0;   

   // add size system memory
   size += currprefs.z3fastmem_size;
   size += currprefs.rtgmem_size;
   size += currprefs.fastmem_size;
   size += currprefs.bogomem_size;
   size += currprefs.chipmem_size;

   // add some extra for other state
   size += 0x100000; // FIME Way too much

   // fprintf(stderr, "Size retro_serialize_size: %i\n", size);
   return size;
}

bool retro_serialize(void *data_, size_t size)
{
   // have UAE write to file
   save_state (fname, "e-uae");

   // copy file to memoory for libretro
   FILE *fp = fopen(fname, "rb");
   if (fp != NULL) {
      fread(data_, sizeof(char), size, fp);
      fclose(fp);
      return true;
   }
   return false; 
}

bool retro_unserialize(const void *data_, size_t size)
{
   // copy to file for UAE
   FILE *fp = fopen(fname, "wb");
   if (fp != NULL) {
      fwrite(data_, sizeof(char), size, fp);
      fclose(fp);
      strcpy(savestate_fname,fname);
      savestate_state = STATE_DORESTORE;
      return true;
   } else {
      fprintf(stderr, "Cannot open temprorary file '%s' for writing state file\n", fname);
   }
    
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
