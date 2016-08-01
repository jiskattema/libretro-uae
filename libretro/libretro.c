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

struct retro_message rmsg; // used in sources/src/caps/caps.c
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
retro_environment_t environ_cb; // used in sources/src/caps/caps.c

/* uae configuarion state
 */
extern struct uae_prefs currprefs;

/* Callbacks for RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE.
 * Should be set for implementations which can swap out multiple disk 
 * images in runtime.
 *
 * If the implementation can do this automatically, it should strive to do so.
 * However, there are cases where the user must manually do so.
 *
 * To swap a disk image:
 * set_eject_state(true)    eject the disk image
 * set_image_index(index)   set the disk index
 * set_eject_state(false)   insert the disk again 
 */
int rnumimages = 0;
int rimageidx = 0;
bool rejectstate = true;

#define RETRO_MAX_IMAGES 10
char retro_image[RETRO_MAX_IMAGES][RETRO_LINE_LENGTH+1];
char retro_uae_file[RETRO_LINE_LENGTH+1];

/*
 * This directory can be used to store system specific 
 * content such as BIOSes, configuration data, etc.
 * The returned value can be NULL.
 * If so, no such directory is defined,
 * and it's up to the implementation to find a suitable directory.
 */
char rkickdir[RETRO_LINE_LENGTH+1]; // where the kickstart roms are: retro_system_dir/kickstart
char rcapsdir[RETRO_LINE_LENGTH+1]; // for capsimg.so: retro_system_dir


bool retro_set_eject_state(bool ejected) {
    fprintf(stderr, "=============\n");
    fprintf(stderr, "Number of disks: %d\n", rnumimages);
    fprintf(stderr, "Active disk: %d\n", rimageidx);
    fprintf(stderr, "- - - - - - -\n");
    int i;
    for (i=0; i<rnumimages;i++){
    fprintf(stderr, "disk %d: %s\n", i, retro_image[i]);
    }
    fprintf(stderr, "=============\n");

    /* If ejected is true, "ejects" the virtual disk tray.
     * When ejected, the disk image index can be set.
     */
    if (ejected)
    {
        if (! rejectstate)
        {
            disk_eject(0);
        }
        rejectstate = true;
    }
    else
    {
        // add current disk image as DF0: for P-UAE
        if (rimageidx < rnumimages)
        {
            disk_insert (0, retro_image[rimageidx], false);
        }
        else 
        {
           disk_eject(0);
        }
        rejectstate = false; // disk inserted
    }
    return true;
}

bool retro_get_eject_state() {
    /* Gets current eject state. The initial state is 'not ejected'. */
    return rejectstate;
}

unsigned retro_get_image_index() {
    /* Gets current disk index. First disk is index 0.
     * If return value is >= get_num_images(), no disk is currently inserted.
     */
    return rimageidx;
}

bool retro_set_image_index(unsigned index) {
    /* Sets image index. Can only be called when disk is ejected.
     * The implementation supports setting "no disk" by using an 
     * index >= get_num_images().
     */
    if (rejectstate == false)
    {
        return false;
    }
    rimageidx = index;
    return true;
}

unsigned retro_get_num_images() {
    /* Gets total number of images which are available to use. */
    return rnumimages;
}


//struct retro_game_info
//{
//   const char *path;       // Path to game, UTF-8 encoded. Usually used as a reference.
//                           // May be NULL if rom was loaded from stdin or similar.
//                           // retro_system_info::need_fullpath guaranteed that this path is valid.
//   const void *data;       // Memory buffer of loaded game. Will be NULL if need_fullpath was set.
//   size_t      size;       // Size of memory buffer.
//   const char *meta;       // String of implementation specific meta-data.
//};

bool retro_replace_image_index(unsigned index, const struct retro_game_info *info) {
    /* Replaces the disk image associated with index.
     * Arguments to pass in info have same requirements as retro_load_game().
     * Virtual disk tray must be ejected when calling this.
     *
     * Replacing a disk image with info = NULL will remove the disk image 
     * from the internal list.
     * As a result, calls to get_image_index() can change.
     *
     * E.g. replace_image_index(1, NULL), and previous get_image_index() 
     * returned 4 before.
     * Index 1 will be removed, and the new index is 3.
     */
    if (rejectstate == false)
    {
        return false;
    }
    if (! info)
    {
        // remove image, move other images down to fill the gap
        int i;
        for (i = index; i < rnumimages - 1; i++ ) {
            strcpy(retro_image[i], retro_image[i+1]);
        }

        // reduce image count
        rnumimages--;
    } else {
        strncpy(retro_image[index], info->path, RETRO_LINE_LENGTH);
        retro_image[index][RETRO_LINE_LENGTH] = '\0';
    }

    return true;
}

bool retro_add_image_index() {
    /* Adds a new valid index (get_num_images()) to the internal disk list.
     * This will increment subsequent return values from get_num_images() by 1.
     * This image index cannot be used until a disk image has been set 
     * with replace_image_index. */
    if (rnumimages == RETRO_MAX_IMAGES) {
        fprintf(stderr, "maximum number of images (%d) reached\n", RETRO_MAX_IMAGES);
        return false;
    }

    rnumimages++;
    return true;
}

static struct retro_disk_control_callback disk_control_cb = {  
   retro_set_eject_state,
   retro_get_eject_state,

   retro_get_image_index,
   retro_set_image_index,
   retro_get_num_images,

   retro_replace_image_index,
   retro_add_image_index
};

static struct retro_input_descriptor input_descriptors[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Fire" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Mouse mode toggle" },
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
   cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_control_cb);
}

static void update_variables(void)
{
   struct retro_variable var = {0};

   var.key = "analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
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
      if (strcmp(var.value, "Standard") == 0)   ledtype = 0;
      if (strcmp(var.value, "Simplified") == 0) ledtype = 1;
      if (strcmp(var.value, "None") == 0)       ledtype = 2;

      fprintf(stderr, "value: %s\n", var.value);
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

#ifdef DEBUG
#define mystr(s) #s
#define INIT_MSSG_1(x,y) {      fprintf(stderr, "%25s  %s\n", x, y);                            }
#define INIT_VOID_0(x)   {x( ); fprintf(stderr, "%25s  %5s\n", mystr(x),        "OK"          );}
#define INIT_VOID_1(x,y) {x(y); fprintf(stderr, "%25s  %5s\n", mystr(x),        "OK"          );}
#define INIT_BOOL_0(x)   {      fprintf(stderr, "%25s  %5s\n", mystr(x), x()  ? "OK" : "ERROR");}
#define INIT_BOOL_1(x,y) {      fprintf(stderr, "%25s  %5s\n", mystr(x), x(y) ? "OK" : "ERROR");}
#else
#define INIT_MSSG_1(x,y) {}
#define INIT_VOID_0(x)   {x();}
#define INIT_VOID_1(x,y) {x(y);}
#define INIT_BOOL_0(x)   {x();}
#define INIT_BOOL_1(x,y) {x(y);}
#endif

void my_main()
{
    INIT_VOID_0(keyboard_settrans);

    default_prefs (&currprefs, 0);
    INIT_MSSG_1("Default romfile", currprefs.romfile);

    built_in_prefs(&currprefs, rqsmode, rconfig, rcompat, 1);
    INIT_MSSG_1("Matching romfile", currprefs.romfile);

    // insert images, if present
    // The disk image interface only controls drive 0, but put disks in other drives too
    // It probably wont matter if the same disk is in multiple drives
    if (rnumimages) {
        if(rnumimages > 0) disk_insert (0, retro_image[0], false);
        if(rnumimages > 1) disk_insert (1, retro_image[1], false);
        if(rnumimages > 2) disk_insert (2, retro_image[2], false);
        if(rnumimages > 3) disk_insert (3, retro_image[3], false);
    }

    // load cfg file, if present
    if (retro_uae_file[0]) {
#ifdef RETRO
    // indicate that the settings are not controlled by libretro
    rmsg.frames = 60 * 3;
    rmsg.msg = "Using config from file";
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
#endif // RETRO
        target_cfgfile_load (&currprefs, retro_uae_file, 0, 1);
    }

    fixup_prefs (&currprefs);
    changed_prefs = currprefs;

    INIT_BOOL_0(graphics_setup);
    INIT_BOOL_0(machdep_init);
    INIT_BOOL_0(setup_sound);
    INIT_VOID_0(inputdevice_init);
    INIT_VOID_0(savestate_init);
    INIT_VOID_0(keybuf_init);
    INIT_VOID_1(memory_hardreset, 2);
    INIT_VOID_0(memory_reset);
    INIT_VOID_0(native2amiga_install);
    INIT_BOOL_0(custom_init);
    INIT_VOID_0(DISK_init);
    INIT_VOID_0(reset_frame_rate_hack);
    INIT_VOID_0(init_m68k);
    INIT_BOOL_0(graphics_init);
    INIT_BOOL_0(init_audio);
#ifdef DEBUGGER
    INIT_VOID_0(setup_brkhandler);
#endif

    do_start_program ();
}

static void retro_wrap_emulator(void)
{
    my_main();

    pauseg=-1;

    environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

    /* We're done here */
    co_switch(mainThread);

    /* Dead emulator, 
    * but libco says not to return. */
    while(true)
    {
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

   char *c;
   environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &c);
   strcpy(rkickdir, c);
   strcat(rkickdir, "/kickstart");
   check_roms(rkickdir);

   strcpy(rcapsdir, c);
   strcat(rcapsdir, "/capsimg.so");
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
   info->block_extract    = true;
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

   video_cb(bmp, retrow, retroh, retrow << 1);

   co_switch(emuThread);
}

bool retro_add_and_replace_image(const struct retro_game_info *info) {
    // get next available index
    int idx = retro_get_num_images ();

    // request index
    if (! retro_add_image_index())
    {
        fprintf(stderr, "Cannot create image index %d\n", idx);
        return false;
    }

    // replace image
    if (! retro_replace_image_index(idx, info))
    {
        fprintf(stderr, "Cannot replace image index %d with %s\n", idx, *info->path);
    }
    return true;
}

bool retro_load_game(const struct retro_game_info *info)
{
    retro_uae_file[0] = '\0';
    const char *suffix = info->path + strlen(info->path) - 3;

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_descriptors);

    if (strlen(info->path) < 5 || strncasecmp(suffix, "uae", 3) == 0)
    {
        strncpy(retro_uae_file, info->path, RETRO_LINE_LENGTH);
        retro_uae_file[RETRO_LINE_LENGTH] = '\0';
    }
    else if (strncasecmp(suffix, "zip", 3) == 0)
    {
        fprintf(stderr, "Extracting %s\n", info->path);
        stripped_miniunz(info->path);
    }
    else
    {
        fprintf(stderr, "Adding disk image %s\n", info->path);
        return retro_add_and_replace_image(info);
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
