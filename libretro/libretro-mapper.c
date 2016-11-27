#include "libretro.h"
#include "libretro-glue.h"

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "gui.h"
#include "xwin.h"
#include "disk.h"
#include "keyboard.h"

int controller_state[2][7]; // [port][propery]
// 0:  0  joystick mode, 0 mouse mode 
// 1 toggle mouse mode
// 2 joy button a
// 3 joy button b
// 4 joy left/right
// 5 joy up/down
// 6 mouse button a
// 7 mouse button b

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

// libretro controller port => UAE device
void retro_update_port (int port, int device) {
    int state;
    int mouse_l;
    int mouse_r;
    int fmousex;
    int fmousey;

    static int counter = 0;
    counter = (counter + 1) & 7;
    
    input_poll_cb();

    // mouse/joy toggle
    state = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
    
    if (state && ! controller_state[port][1]) {
      // button pressed
      controller_state[port][1] = 1;
    } else if (! state && controller_state[port][1]) {
      // button released
      controller_state[port][1] = 0;
      controller_state[port][0] = controller_state[port][0] ? 0 : 1;
    }

    // Joystick mode
    if(controller_state[port][0] == 0) { 

        // primary joystick button
        state = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        if (rautofire) {
          state = state && (counter & 4);
        }
        if (state != controller_state[port][2]) {
          setjoybuttonstate(device, 0, state);
          controller_state[port][2] = state;
        }

        // secondary joystick button
        state = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
        if (state != controller_state[port][3]) {
          setjoybuttonstate(device, 1, state);
          controller_state[port][3] = state;
        }

        // update joystick
        if(opt_analog) {
            // ...with analog stick
            state = input_state_cb(0, RETRO_DEVICE_ANALOG, 
                RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
            if (state != controller_state[port][4]) {
               setjoystickstate  (device, 0, state, 32767);
               controller_state[port][4] = state;
            }

            state = input_state_cb(0, RETRO_DEVICE_ANALOG,
                RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
            if (state != controller_state[port][5]) {
               setjoystickstate  (device, 1, state, 32767);   
               controller_state[port][5] = state;
            }
        } else {
            // ...with dpad
            state = 0;
            state += input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ? -1 : 0;
            state += input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0;
            if (state != controller_state[port][5]) {
               setjoystickstate  (device, 1, state, 1);   
               controller_state[port][5] = state;
            }

            state = 0;
            state += input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ? -1 : 0;
            state += input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0;
            if (state != controller_state[port][4]) {
               setjoystickstate  (device, 0, state, 1);
               controller_state[port][4] = state;
            }
        }

        // update mouse with mouse
        fmousex = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        fmousey = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        mouse_l = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        mouse_r = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
    } else {
        // update mouse position with right analog
        fmousex = input_state_cb(port, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 1024;
        fmousey = input_state_cb(port, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 1024;

        // update mouse position with dpad
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
            fmousex += rspeed;
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
            fmousex -= rspeed;
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
            fmousey += rspeed;
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
            fmousey -= rspeed;

        // update mouse buttons
        // do autofire for first button
        if (rautofire) {
          mouse_l=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) && (counter & 4);
        } else {
          mouse_l=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        }
        // and normal presses for second button
        mouse_r=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
    }

    // Update mouse
    setmousebuttonstate (device, 0, mouse_l);
    setmousebuttonstate (device, 1, mouse_r);
    if (fmousex) setmousestate (device, 0, fmousex, 0);
    if (fmousey) setmousestate (device, 1, fmousey, 0);
}

void retro_update_input(void) {
  retro_update_port(0,0);
  retro_update_port(1,1);
}
