#include "libretro.h"
#include "libretro-glue.h"

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "gui.h"
#include "xwin.h"
#include "disk.h"
#include "keyboard.h"

int MOUSEMODE=-1;

int al[2]; // left analog1
int ar[2]; // right analog1
unsigned char MXjoy0; // joy
int fmousex, fmousey; // emu mouse

static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int jflag[5]={0,0,0,0,0};

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

void retro_keypress(bool down, unsigned keycode, uint32_t character, uint16_t mods) {
    int translated;

    // real keyboard only sends proper events on key down, and empty up events
    // onscreen keyboard sends proper up and down events
    // we're using the inputdevice_do_keyboard function, see inputdevice.c:2660

    if (character) {
      // fprintf(stderr, "Character input not implemented yet\n");
    } else if (keycode) {
       translated = keyboard_translation[keycode];

       // simulate capslock by holding right shift
       if (translated == AK_CAPSLOCK) {
         if (down) {
           // fprintf(stderr, "Capslock pressed, is %i\n", retro_capslock);
         } else {
           retro_capslock = retro_capslock ? 0 : 1;
           // fprintf(stderr, "Capslock released, set to %i\n", retro_capslock);
         }
         inputdevice_do_keyboard (AK_RSH, retro_capslock);
       } else {
         if (down) {
           inputdevice_do_keyboard (translated, 1);
         } else {
           inputdevice_do_keyboard (translated, 0);
         }
         // fprintf(stderr, "down=%i keycode=%u character=%d (%c) mods=%i trans=%x\n", down, keycode, character, character, mods,translated);
       }
    }
};

/*
   R   mouse speed(gui/emu)
   SEL toggle mouse/joy mode
   A   fire/mousea/valid key in vkbd
   B   mouseb
   Y   Emu Gui
*/

void retro_update_input(void)
{
    int i;

    MXjoy0=0;

    input_poll_cb();

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
            //   RETRO      B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
            //   INDEX      0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15
            //             0x1C 0x39 0x01 0x3B 0x01 0x02 0x04 0x08 0x80 0x6D 0x15 0x31 0x24 0x1F 0x6E 0x6F
            if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 4) ) { // UP
                if(jflag[0]==0) { setjoystickstate(0, 1, -1, 1); jflag[0]=1; }
            } else {
                if(jflag[0]==1) { setjoystickstate(0, 1, 0, 1); jflag[0]=0; }
            }
            if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 5) ) { // DOWN
                if(jflag[1]==0) { setjoystickstate(0, 1, 1, 1); jflag[1]=1; }
            } else {
                if(jflag[1]==1) { setjoystickstate(0, 1, 0, 1); jflag[1]=0; }
            }

            if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 6) )  { // LEFT
                if(jflag[2]==0) { setjoystickstate(0, 0, -1, 1); jflag[2]=1; }
            } else {
                if (jflag[2]==1) { setjoystickstate(0, 0, 0, 1); jflag[2]=0; }
            }


            if( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 7) ) { // RIGHT
                if(jflag[3]==0) { setjoystickstate(0, 0, 1, 1); jflag[3]=1; }
            } else {
                if(jflag[3]==1) { setjoystickstate(0, 0, 0, 1); jflag[3]=0; }
            }
        }

        mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

        fmousex=mouse_x;
        fmousey=mouse_y;
    } else {
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

    if(mbL==0 && mouse_l) {
        mbL=1;
        setmousebuttonstate (0, 0, 1);
    } else if(mbL==1 && !mouse_l) {
        setmousebuttonstate (0, 0, 0);
        mbL=0;
    }

    if(mbR==0 && mouse_r) {
        mbR=1;
        setmousebuttonstate (0, 1, 1);
    } else if(mbR==1 && !mouse_r) {
        setmousebuttonstate (0, 1, 0);
        mbR=0;
    }

    setmousestate (0, 0, fmousex, 0);
    setmousestate (0, 1, fmousey, 0);
}
