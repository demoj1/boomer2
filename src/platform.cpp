#include "platform.h"
#include "prelude.h"

#include <stdlib.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>

#ifdef XCB_SCREENSHOT
  #include <xcb/xcb.h>
  #include <xcb/xproto.h>
#else
  #include <X11/X.h>
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
#endif

#pragma pack(1)
struct _color { u_char r, g, b; };

#ifdef XCB_SCREENSHOT
  class xcb_conn {
    xcb_connection_t* _conn;

  public:
    xcb_conn() {
      LOG("Constructor\n");
      _conn = xcb_connect(NULL, NULL);
    }

    ~xcb_conn() {
      LOG("Destructor\n");
      xcb_flush(_conn);
      xcb_disconnect(_conn);
    }

    operator xcb_connection_t*() const {
      LOG("Fetch value\n");
      return _conn;
    }
  };

  #define NAME2(A,B)         NAME2_HELPER(A,B)
  #define NAME2_HELPER(A,B)  A ## B
  #define CHECK_ERR(CONN, BODY) do { \
    const xcb_generic_error_t* NAME2(NAME2(__ERROR__, _), __LINE__) = xcb_request_check(CONN, BODY); \
    if (NAME2(NAME2(__ERROR__, _), __LINE__)) asm("int $3"); \
  } while (0)

  void raise_window(bool state) noexcept {
    auto conn = xcb_conn();

    auto* focusReply = xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    auto win = focusReply->focus;

    int vals[] = { state ? 1 : 0 };

    CHECK_ERR(conn, xcb_change_window_attributes(
      conn,
      win,
      XCB_CW_OVERRIDE_REDIRECT,
      vals
    ));

    CHECK_ERR(conn, xcb_unmap_window(conn, win));
    CHECK_ERR(conn, xcb_map_window(conn, win));

    CHECK_ERR(conn, xcb_set_input_focus (conn, {}, win, {}));

    uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    CHECK_ERR(conn, xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values));

    free(focusReply);
  }

  std::pair<int16_t, int16_t> get_screen_size() noexcept {
    auto conn = xcb_conn();
    auto screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    std::pair<int16_t, int16_t> pair = {
      screen->width_in_pixels,
      screen->height_in_pixels,
    };

    return pair;
  }

  std::tuple<int16_t, int16_t, int16_t, int16_t> get_window_dimensions_under_cursor() noexcept {
    std::tuple<int16_t, int16_t, int16_t, int16_t> result;

    auto conn = xcb_conn();
    auto screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    auto pointer_cookie = xcb_query_pointer(conn, screen->root);
    auto pointer_reply = xcb_query_pointer_reply(conn, pointer_cookie, NULL);

    if (pointer_reply) {
      xcb_window_t root_window = pointer_reply->root;
      int16_t root_x = pointer_reply->root_x;
      int16_t root_y = pointer_reply->root_y;

      auto translate_cookie = xcb_translate_coordinates(conn, root_window, root_window, root_x, root_y);
      auto translate_reply = xcb_translate_coordinates_reply(conn, translate_cookie, NULL);

      if (translate_reply && translate_reply->child != XCB_NONE) {
        xcb_window_t window_under_cursor = translate_reply->child;
        auto geometry_cookie = xcb_get_geometry(conn, window_under_cursor);
        auto geometry_reply = xcb_get_geometry_reply(conn, geometry_cookie, NULL);

        if (geometry_reply) {
          printf("Window under cursor ID: %u\n", window_under_cursor);
          printf("Dimensions: %hu x %hu\n", geometry_reply->width, geometry_reply->height);
          printf("Position (relative to parent): (%d, %d)\n", geometry_reply->x, geometry_reply->y);

          result = {geometry_reply->x, geometry_reply->y, geometry_reply->width, geometry_reply->height};
          free(geometry_reply);
        } else {
          ERR("Failed to get geometry of window under cursor.\n");
        }
      }

      free(translate_reply);
      free(pointer_reply);
    } else {
      ERR("No child window found at cursor position, or cursor is on root window.\n");
    }

    return result;
  }

  // DEBUG
  // 60, 62, 79, 98, 88 - without pragma
  // 46, 95, 49, 62, 48 - with pragma
  // RELASE
  // 75, 20, 28, 39, 27 - without pragma
  // 21, 46, 57, 35, 21 - with pragma
  // x, y, w, h
  u_char* take_screenshot(std::tuple<int16_t, int16_t, int16_t, int16_t> rectangle) noexcept {
  #ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
  #endif

    auto x = std::get<0>(rectangle);
    auto y = std::get<1>(rectangle);
    auto w = std::get<2>(rectangle);
    auto h = std::get<3>(rectangle);

    auto conn = xcb_conn();
    auto screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    auto get_image_task = xcb_get_image(
      conn,
      XCB_IMAGE_FORMAT_Z_PIXMAP,
      screen->root,
      x,
      y,
      w,
      h,
      ~0
    );

    // auto win_pixmap = xcb_generate_id(conn);
    auto* image_reply = xcb_get_image_reply(conn, get_image_task, 0);
    size_t l = xcb_get_image_data_length(image_reply);

    _color* data = new _color[w*h];

    // BGRA 8 bit
    const u_char* image_data = xcb_get_image_data(image_reply);

    // Convert GBRA to RGBA
    for (size_t i = 0; i < l; i += 4) {
      data[i/4].r = image_data[i + 2]; // r
      data[i/4].g = image_data[i + 1]; // g
      data[i/4].b = image_data[i + 0]; // b
    }

    free(image_reply);

  #ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time = end_time - start_time;

    std::cout << "--------> took: " << time/std::chrono::milliseconds(1) << "ms.\n";
    fflush(stdout);
  #endif

    return (u_char*)data;
  }
#else
  void raise_window(void* handle) {
    auto display = XOpenDisplay(NULL);
    Window* wid = (Window*)handle;
    XSetWindowAttributes attrs;
    attrs.override_redirect = true;

    XChangeWindowAttributes(
      display,
      *wid,
      CWOverrideRedirect,
      &attrs
    );

    XCloseDisplay(display);
  }

  std::pair<uint, uint> get_screen_size() noexcept
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

  // DEBUG
  // 104, 136, 109, 129, 108 - without pragma
  // 74, 74, 121, 91, 95     - with pragma
  // RELASE
  // 76, 55, 83, 61, 57 - without pragma
  // 63, 64, 70, 66, 46 - with pragma
  u_char* take_screenshot(std::pair<uint, uint> display_size) noexcept
  {
  #ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
  #endif

    auto display = XOpenDisplay(NULL);
    uint screen = DefaultScreen(display);

    XImage* image = XGetImage(
      display,
      RootWindow(display, screen),
      0,
      0,
      display_size.first,
      display_size.second,
      AllPlanes,
      ZPixmap
    );

    _color* data = new _color[display_size.first * display_size.second];

    for (uint y = 0; y < display_size.second; y++) {
      for (uint x = 0; x < display_size.first; x++) {
        unsigned long pixel = XGetPixel(image, x, y);
        auto ii = y*display_size.first + x;

        data[ ii ].r = (pixel & image->red_mask)   >> 16;
        data[ ii ].g = (pixel & image->green_mask) >> 8;
        data[ ii ].b = (pixel & image->blue_mask)  >> 0;
      }
    }

    XCloseDisplay(display);
    XDestroyImage(image);

  #ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time = end_time - start_time;

    std::cout << "--------> took: " << time/std::chrono::milliseconds(1) << "ms.\n";
    fflush(stdout);
  #endif

    return (u_char*)data;
  }
#endif
