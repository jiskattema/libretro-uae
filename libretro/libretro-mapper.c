#include "libretro.h"
#include "libretro-glue.h"
#include "keyboard.h"
#include "libretro-keymap.h"

int SHIFTON=-1,MOUSEMODE=-1;

int gmx=320,gmy=240; //gui mouse

int al[2];//left analog1
int ar[2];//right analog1
unsigned char MXjoy0; // joy
int fmousex,fmousey; // emu mouse

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

long frame=0;
unsigned long  Ktime=0 , LastFPSTime=0;

int BOXDEC= 32+2;
int STAT_BASEY;

extern char key_state[512];
extern char key_state2[512];

extern bool opt_analog;

static retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

#define MDEBUG
#ifdef MDEBUG
#define mprintf printf
#else
#define mprintf(...) 
#endif


#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "gui.h"
#include "xwin.h"
#include "disk.h"

/*
   R   mouse speed(gui/emu)
   SEL toggle mouse/joy mode
   A   fire/mousea/valid key in vkbd
   B   mouseb
   Y   Emu Gui
*/

void process_key(void)
{
   int i;
   for(i=0;i<320;i++)
   {
      key_state[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i)?0x80:0;

      if(keyboard_translation[i]==0x60/*AK_LSH*/ )
      {  //SHIFT CASE
         if( key_state[i] && key_state2[i]==0 )
         {
            if(SHIFTON == 1)
               retro_key_up(keyboard_translation[i] );
            else if(SHIFTON == -1) 
               retro_key_down(keyboard_translation[i] );

            SHIFTON=-SHIFTON;

            key_state2[i]=1;

         }
         else if (!key_state[i] && key_state2[i]==1)
            key_state2[i]=0;
      }
      else
      {

         if(key_state[i] && keyboard_translation[i]!=-1  && key_state2[i] == 0)
         {
            retro_key_down(keyboard_translation[i]);
            key_state2[i]=1;
         }
         else if ( !key_state[i] && keyboard_translation[i]!=-1 && key_state2[i]==1 )
         {
            retro_key_up(keyboard_translation[i]);
            key_state2[i]=0;
         }

      }
   }
}

void update_input(void)
{
    int i;
    //   RETRO      B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
    //   INDEX      0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
    static int vbt[16]={0x1C,0x39,0x01,0x3B,0x01,0x02,0x04,0x08,0x80,0x6D,0x15,0x31,0x24,0x1F,0x6E,0x6F};

    MXjoy0=0;

    input_poll_cb();
    process_key();

    i=2; // mouse/joy toggle
    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
        mbt[i]=1;
    else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
    {
        mbt[i]=0;
        MOUSEMODE=-MOUSEMODE;
    }

    static int mbL=0,mbR=0;
    int mouse_l;
    int mouse_r;
    int16_t mouse_x;
    int16_t mouse_y;

    if(MOUSEMODE==-1)
    { 
        // Joy mode
        al[0] =(input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X));
        al[1] =(input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y));

        setjoybuttonstate (0,0,input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A));
        setjoybuttonstate (0,1,input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B));

        if(opt_analog) {
            setjoystickstate  (0, 0, al[0], 32767);
            setjoystickstate  (0, 1, al[1], 32767);   
        } else {
            for(i=4;i<8/*9*/;i++)
                if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
                    MXjoy0 |= vbt[i]; // Joy press
            retro_joy0(MXjoy0);
        }

        mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

        fmousex=mouse_x;
        fmousey=mouse_y;
    }
    else
    {
        //Mouse mode
        fmousex=fmousey=0;

        //ANALOG RIGHT
        ar[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X));
        ar[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y));

        if(ar[0]<=-1024)
            fmousex-=(-ar[0])/1024;
        if(ar[0]>= 1024)
            fmousex+=( ar[0])/1024;
        if(ar[1]<=-1024)
            fmousey-=(-ar[1])/1024;
        if(ar[1]>= 1024)
            fmousey+=( ar[1])/1024;

        //PAD
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
            fmousex += rspeed;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
            fmousex -= rspeed;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
            fmousey += rspeed;
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
            fmousey -= rspeed;

        mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
    }

    if(mbL==0 && mouse_l)
    {
        mbL=1;
        setmousebuttonstate (0, 0, 1);
    }
    else if(mbL==1 && !mouse_l)
    {
        setmousebuttonstate (0, 0, 0);
        mbL=0;
    }

    if(mbR==0 && mouse_r)
    {
        mbR=1;
        setmousebuttonstate (0, 1, 1);
    }
    else if(mbR==1 && !mouse_r)
    {
        setmousebuttonstate (0, 1, 0);
        mbR=0;
    }

    setmousestate (0, 0, fmousex, 0);
    setmousestate (0, 1, fmousey, 0);
}
