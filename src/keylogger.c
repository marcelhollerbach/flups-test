#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>

#define INVALID_EVENT_TYPE  -1

static int key_press_type = INVALID_EVENT_TYPE;
static int key_release_type = INVALID_EVENT_TYPE;

static void
_press(const char *symbol)
{
   fprintf(stdout, ".%s\n", symbol);
   fflush(stdout);
}

static void
_release(const char *symbol)
{
   fprintf(stdout, ",%s\n", symbol);
   fflush(stdout);
}

int main(void) {
  int number = 0;
  int run = 1;
  int default_screen;

  Display * default_display;
  Window root_window;
  XEvent e;
  XDeviceKeyEvent * key;
  XEventClass           event_list[20];

  default_display = XOpenDisplay(getenv("DISPLAY"));
  default_screen = DefaultScreen(default_display);
  root_window = RootWindow(default_display, default_screen);

  int num = 0;

  XDeviceInfo *infos = XListInputDevices(default_display, &num);

  //find all devices that are doing input
  for (int i = 0; i < num; ++i) {
     XDevice * device;

     if (infos[i].id == IsXKeyboard) continue;
     if (infos[i].id == IsXPointer) continue;
     if (infos[i].id == IsXExtensionKeyboard) continue;
     if (infos[i].id == IsXExtensionPointer) continue;


     switch(infos[i].inputclassinfo->class) {
        case KeyClass:
          device = XOpenDevice(default_display, infos[i].id);

          if(!device){
            fprintf(stderr, "unable to open device\n");
            return 0;
          }

          DeviceKeyPress(device, key_press_type, event_list[number]); number++;
          DeviceKeyRelease(device, key_release_type, event_list[number]); number++;

        break;
     }
  }

  /* important */
  if(XSelectExtensionEvent(default_display, root_window, event_list, number)) {
    fprintf(stderr, "error selecting extended events\n");
    return 0;
  }

  while(run) {
    XNextEvent(default_display, &e);
    key = (XDeviceKeyEvent *) &e;
    const char *symbol;

    symbol = XKeysymToString(XKeycodeToKeysym(default_display, key->keycode, 0));

    if (key->type == key_press_type)
      {
         _press(symbol);
      }
    else if (key->type == key_release_type)
      {
         _release(symbol);
      }
  }

  /* Close the connection to the X server */
  XCloseDisplay(default_display);

  return 0;
}
