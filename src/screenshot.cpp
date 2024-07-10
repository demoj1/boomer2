#include "screenshot.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef DEBUG
  #include <chrono>
  #include <iostream>
#endif

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

  #ifdef DEBUG
    std::cout << "rmask: " << rmask << std::endl
      << "gmask: " << gmask << std::endl
      << "bmask: " << bmask << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
  #endif

  #pragma omp parallel for
  for (uint y = 0; y < display_size.second; y++) {
    for (uint x = 0; x < display_size.first; x++) {
      unsigned long pixel = XGetPixel(image, x, y);
      auto ii = y*display_size.first * 3 + x*3;

      data[ ii + 0 ] = (pixel & rmask) >> 16;
      data[ ii + 1 ] = (pixel & gmask) >> 8;
      data[ ii + 2 ] = (pixel & bmask) >> 0;
    }
  }

  #ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time = end_time - start_time;

    std::cout << "--------> took: " << time/std::chrono::milliseconds(1) << "ms.\n";
    fflush(stdout);
  #endif

  XCloseDisplay(display);
  XDestroyImage(image);

  return data;
}
