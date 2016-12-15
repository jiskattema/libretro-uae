#include "libretro.h"
#include "libretro-glue.h"

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "gui.h"
#include "xwin.h"
#include "disk.h"
#include "keyboard.h"

/*
 * External parameters
 */
extern struct uae_prefs currprefs, changed_prefs; // uae configuarion state

int controller_state[2][7]; // [port][property]
// 0:  0  joystick mode, 1 mouse mode 
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

// libretro controller port => UAE device
void retro_joystick (int port, int device) {
    int state;

    static int counter = 0;
    counter = (counter + 1) & 7;

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
}

// libretro controller port => UAE device
void retro_mouse (int port, int device) {
    int state;
    int mouse_l;
    int mouse_r;
    int fmousex;
    int fmousey;

    static int counter = 0;
    counter = (counter + 1) & 7;
    
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

    // update mouse with retro mouse
    if (rusemouse) {
       fmousex = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
       fmousey = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
       mouse_l = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
       mouse_r = input_state_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
    }

    // update mouse buttons
    // ...do autofire for first button
    if (rautofire) {
      mouse_l=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) && (counter & 4);
    } else {
      mouse_l=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    }
    // ...and normal presses for second button
    mouse_r=input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

    setmousebuttonstate (device, 0, mouse_l);
    setmousebuttonstate (device, 1, mouse_r);
    setmousestate (device, 0, fmousex, 0);
    setmousestate (device, 1, fmousey, 0);
}
//static void copyjport (const struct uae_prefs *src, struct uae_prefs *dst, int num)
//{
//	if (!src)
//		return;
//	freejport (dst, num);
//	_tcscpy (dst->jports[num].configname, src->jports[num].configname);
//	_tcscpy (dst->jports[num].name, src->jports[num].name);
//	dst->jports[num].id = src->jports[num].id;
//	dst->jports[num].mode = src->jports[num].mode;
//	dst->jports[num].autofire = src->jports[num].autofire;
//}

void reconnect_input_devices () {
  if (controller_state[0][0]) {
    sprintf(changed_prefs.jports[0].name, "Retro pad 0");
    changed_prefs.jports[0].mode = 3;
    inputdevice_joyport_config (&changed_prefs, "Retro pad 0", 0, 3, -1); // portnum, mode, type
  } else {
    sprintf(changed_prefs.jports[0].name, "Retro mouse 0");
    changed_prefs.jports[0].mode = 2;
    inputdevice_joyport_config (&changed_prefs, "Retro mouse 0", 0, 2, -1); // portnum, mode, type
  }
  if (controller_state[1][0]) {
    sprintf(changed_prefs.jports[1].name, "Retro pad 1");
    changed_prefs.jports[0].mode = 3;
    inputdevice_joyport_config (&changed_prefs, "Retro pad 1", 0, 3, -1); // portnum, mode, type
  } else {
    sprintf(changed_prefs.jports[1].name, "Retro mouse 1");
    changed_prefs.jports[0].mode = 2;
    inputdevice_joyport_config (&changed_prefs, "Retro mouse 1", 0, 2, -1); // portnum, mode, type
  }
  fprintf(stderr, "sending input devices\n");
  fprintf(stderr, "0: %s\n", changed_prefs.jports[0].name);
  fprintf(stderr, "1: %s\n", changed_prefs.jports[1].name);
  // inputdevice_updateconfig(&changed_prefs, &currprefs);
  inputdevice_devicechange (&changed_prefs);

  fprintf(stderr, "updated input devices\n");
  fprintf(stderr, "0: %s\n", currprefs.jports[0].name);
  fprintf(stderr, "1: %s\n", currprefs.jports[1].name);
}

void retro_update_input(void) {
  int state;
  int port;
  int device;

  input_poll_cb();

  for (port=0; port < 2; port++) {
    // mouse/joy toggle
    state = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
    if (state && ! controller_state[port][1]) {
      // button pressed
      controller_state[port][1] = 1;
    } else if (! state && controller_state[port][1]) {
      // button released
      controller_state[port][1] = 0;
      controller_state[port][0] = controller_state[port][0] ? 0 : 1;
      fprintf(stderr, "Port %i as Mode %i\n", port, controller_state[port][0] );
      reconnect_input_devices ();
    }


    if (controller_state[port][0]) {
      // dont cross joysticks
      retro_joystick(port, port);
    } else {
      // // but cross mouses
      // if (rcrossmouse) {
      //    device = 1 - port;
      // } else {
      //    device = port;
      // }
      retro_mouse(port, port);
    }
  }
}
