#include "screenshot.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <sys/types.h>

void raise_window(uint* handle) {
  auto display = XOpenDisplay(NULL);
  XRaiseWindow(display, *handle);
  XCloseDisplay(display);
}

std::pair<uint, uint> get_screen_size()
{
  auto display = XOpenDisplay(NULL);
  auto screen = DefaultScreen(display);

  std::pair<uint, uint> pair = {
    (uint)DisplayWidth(display, screen),
    (uint)DisplayHeight(display, screen)
  };

  XCloseDisplay(display);

  return pair;
}

u_char* take_screenshot(std::pair<uint, uint> display_size)
{
  auto display = XOpenDisplay(NULL);

  uint screen = DefaultScreen(display);

  XImage* image = XGetImage(display,
                            RootWindow(display, screen),
                            0,
                            0,
                            display_size.first,
                            display_size.second,
                            AllPlanes,
                            ZPixmap);

  uint bytes = display_size.first * display_size.second * 3;

  u_char* data = new u_char[bytes];

  ulong rmask = image->red_mask;
  ulong gmask = image->green_mask;
  ulong bmask = image->blue_mask;
  uint ii = 0;

  for (uint y = 0; y < display_size.second; y++) {
    for (uint x = 0; x < display_size.first; x++) {
      unsigned long pixel = XGetPixel(image, x, y);

      data[ii++] = (pixel & rmask) >> 16;
      data[ii++] = (pixel & gmask) >> 8;
      data[ii++] = (pixel & bmask) >> 0;
    }
  }

  XCloseDisplay(display);
  XDestroyImage(image);

  return data;
}
