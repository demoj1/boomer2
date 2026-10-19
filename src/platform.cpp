#include "platform.h"
#include "prelude.h"

#include <stdlib.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>

#ifdef XCB_SCREENSHOT
  #include <xcb/xcb.h>
  #include <xcb/xproto.h>
#endif

#ifdef WAYLAND_SCREENSHOT
  #include <raylib.h>
  #include <cstring>
  #include <cstdio>
  #include <cctype>
  #include <string>
  #include <vector>
  #include <algorithm>
  #include <array>
  #include <limits>
  #include <wayland-client.h>
  #include "xdg-output-unstable-v1-client-protocol.h"
#endif

struct _color { u_char r, g, b; };
static_assert(sizeof(_color) == 3);

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

  #ifdef DEBUG
    std::cout << "Screen rect [" << x << ", " << y << ", " << w << ", " << h << "];" << std::endl;
  #endif

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
#endif

#ifdef WAYLAND_SCREENSHOT
  // One monitor as the compositor lays it out: logical position/size, i.e. what
  // the user actually sees, already scaled and rotated.
  struct output_info {
    wl_output* output = nullptr;
    zxdg_output_v1* xdg = nullptr;
    std::string name;
    int32_t x = 0, y = 0, w = 0, h = 0;
  };

  static std::vector<output_info> wl_outputs;
  static zxdg_output_manager_v1* wl_xdg_output_manager = nullptr;

  static void on_logical_position(void* data, zxdg_output_v1*, int32_t x, int32_t y) {
    auto* o = (output_info*)data;
    o->x = x;
    o->y = y;
  }

  static void on_logical_size(void* data, zxdg_output_v1*, int32_t w, int32_t h) {
    auto* o = (output_info*)data;
    o->w = w;
    o->h = h;
  }

  static void on_output_name(void* data, zxdg_output_v1*, const char* name) {
    ((output_info*)data)->name = name;
  }

  static void on_output_done(void*, zxdg_output_v1*) {}
  static void on_output_description(void*, zxdg_output_v1*, const char*) {}

  static const zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = on_logical_position,
    .logical_size     = on_logical_size,
    .done             = on_output_done,
    .name             = on_output_name,
    .description      = on_output_description,
  };

  static void on_registry_global(void*, wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
    if (strcmp(interface, wl_output_interface.name) == 0) {
      auto* out = (wl_output*)wl_registry_bind(registry, id, &wl_output_interface, 1);
      wl_outputs.push_back(output_info{ .output = out });
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
      // Version 2 is the minimum that reports output names, which is what
      // `grim -o` needs. Cap at 3, everything past it is irrelevant here.
      wl_xdg_output_manager = (zxdg_output_manager_v1*)wl_registry_bind(
        registry, id, &zxdg_output_manager_v1_interface, version < 3 ? version : 3);
    }
  }

  static void on_registry_global_remove(void*, wl_registry*, uint32_t) {}

  static const wl_registry_listener registry_listener = {
    .global        = on_registry_global,
    .global_remove = on_registry_global_remove,
  };

  // Ask the compositor for the monitor layout via zxdg_output_manager_v1. Returns
  // an empty vector when anything is missing, callers fall back to a plain `grim`.
  static std::vector<output_info> query_outputs() noexcept {
    wl_outputs.clear();
    wl_xdg_output_manager = nullptr;

    wl_display* display = wl_display_connect(nullptr);
    if (!display) {
      ERR("wl_display_connect failed (not running under Wayland?)\n");
      return {};
    }

    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    // First roundtrip collects the globals: every wl_output and the xdg manager.
    wl_display_roundtrip(display);

    std::vector<output_info> result;

    if (wl_xdg_output_manager && !wl_outputs.empty()) {
      // wl_outputs is done growing, so pointers into it stay valid as listener data.
      for (auto& o : wl_outputs) {
        o.xdg = zxdg_output_manager_v1_get_xdg_output(wl_xdg_output_manager, o.output);
        zxdg_output_v1_add_listener(o.xdg, &xdg_output_listener, &o);
      }
      // Second roundtrip delivers name/logical_position/logical_size for each.
      wl_display_roundtrip(display);

      for (auto& o : wl_outputs)
        if (!o.name.empty() && o.w > 0 && o.h > 0) result.push_back(o);
    } else {
      ERR("compositor has no zxdg_output_manager_v1, falling back to a single grim capture\n");
    }

    for (auto& o : wl_outputs) {
      if (o.xdg) zxdg_output_v1_destroy(o.xdg);
      wl_output_destroy(o.output);
    }
    if (wl_xdg_output_manager) zxdg_output_manager_v1_destroy(wl_xdg_output_manager);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    wl_outputs.clear();
    wl_xdg_output_manager = nullptr;

    return result;
  }

  // Output names are compositor defined but the protocol limits them to
  // alphanumerics and dashes. Anything else would end up in a shell command.
  static bool is_safe_output_name(const std::string& name) noexcept {
    if (name.empty()) return false;
    for (unsigned char c : name)
      if (!isalnum(c) && c != '-' && c != '_') return false;
    return true;
  }

  // Copy src over dst at (dx, dy). Both are tightly packed RGB8, so this is a
  // per-row memcpy; raylib's ImageDraw would go pixel by pixel and blend.
  static void blit_rgb8(Image& dst, const Image& src, int dx, int dy) noexcept {
    for (int row = 0; row < src.height; ++row) {
      const int ty = dy + row;
      if (ty < 0 || ty >= dst.height) continue;

      int tx = dx, sx = 0, width = src.width;
      if (tx < 0) { sx = -tx; width += tx; tx = 0; }
      if (tx + width > dst.width) width = dst.width - tx;
      if (width <= 0) continue;

      memcpy((u_char*)dst.data + ((size_t)ty * dst.width + tx) * sizeof(_color),
             (u_char*)src.data + ((size_t)row * src.width + sx) * sizeof(_color),
             (size_t)width * sizeof(_color));
    }
  }

  static Image grab_output(const std::string& name = {}) noexcept {
    if (!name.empty() && !is_safe_output_name(name)) return {};

    // This PNG is immediately decoded, so compression wastes startup time.
    // A pipe also avoids shared temporary files when two instances overlap.
    std::string command = "grim -l 0";
    if (!name.empty()) command += " -o " + name;
    command += " -";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {};

    std::vector<unsigned char> bytes;
    std::array<unsigned char, 65536> buffer;
    size_t count;
    while ((count = fread(buffer.data(), 1, buffer.size(), pipe)) > 0)
      bytes.insert(bytes.end(), buffer.data(), buffer.data() + count);
    const bool read_failed = ferror(pipe);
    const int status = pclose(pipe);
    if (read_failed || status != 0 || bytes.empty() ||
        bytes.size() > (size_t)std::numeric_limits<int>::max()) return {};

    Image image = LoadImageFromMemory(".png", bytes.data(), (int)bytes.size());
    if (image.data) ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    return image;
  }

  // Wayland has no portable way to read the framebuffer or per-window geometry,
  // so we shell out to `grim` and decode the result with raylib. The selection of
  // an area happens later inside our own window.
  //
  // With more than one monitor a bare `grim` renders the whole layout at the
  // largest scale factor in use, which blurs every lower-scale monitor and leaves
  // garbage in the gaps between them. Instead we capture each output on its own
  // and compose them onto a canvas in logical coordinates, so the result matches
  // the physical arrangement and an unscaled monitor stays pixel exact.
  //
  // The capture is cached because get_screen_size() and take_screenshot() both
  // need it; the first call happens on the main thread (see main.cpp) before the
  // screenshot worker thread starts, so there is no data race on the static.
  static const Image& grab_screen() noexcept {
    static Image canvas = {};
    static bool captured = false;
    if (captured) return canvas;
    captured = true;

    auto outputs = query_outputs();

    if (outputs.size() < 2) {
      canvas = grab_output();
      if (!canvas.data) {
        ERR("grim failed or returned an invalid image\n");
        std::exit(EXIT_FAILURE);
      }
      return canvas;
    }

    int32_t min_x = outputs[0].x, min_y = outputs[0].y;
    int32_t max_x = outputs[0].x + outputs[0].w, max_y = outputs[0].y + outputs[0].h;
    for (const auto& o : outputs) {
      min_x = std::min(min_x, o.x);
      min_y = std::min(min_y, o.y);
      max_x = std::max(max_x, o.x + o.w);
      max_y = std::max(max_y, o.y + o.h);
    }

    canvas = GenImageColor(max_x - min_x, max_y - min_y, BLACK);
    ImageFormat(&canvas, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    for (const auto& o : outputs) {
      if (!is_safe_output_name(o.name)) {
        ERR("skipping output with a suspicious name\n");
        continue;
      }

      Image part = grab_output(o.name);
      if (part.data == nullptr) {
        ERR("could not decode the capture of output %s\n", o.name.c_str());
        continue;
      }

      // grim gives back the native mode, scale it down to the logical size the
      // compositor placed the monitor at.
      if (part.width != o.w || part.height != o.h) ImageResize(&part, o.w, o.h);

      blit_rgb8(canvas, part, o.x - min_x, o.y - min_y);
      UnloadImage(part);
    }

    return canvas;
  }

  std::pair<int16_t, int16_t> get_screen_size() noexcept {
    const auto& image = grab_screen();
    return { (int16_t)image.width, (int16_t)image.height };
  }

  std::tuple<int16_t, int16_t, int16_t, int16_t> get_window_dimensions_under_cursor() noexcept {
    // No per-window geometry under Wayland: fall back to the full screen.
    const auto& image = grab_screen();
    return { 0, 0, (int16_t)image.width, (int16_t)image.height };
  }

  u_char* take_screenshot(std::tuple<int16_t, int16_t, int16_t, int16_t> rectangle) noexcept {
    const auto& image = grab_screen();

    const auto w = std::get<2>(rectangle);
    const auto h = std::get<3>(rectangle);

    // main.cpp owns the returned buffer and frees it with delete[], so we cannot
    // hand back raylib's malloc'd image.data directly; copy it into a new[] buffer.
    const size_t length = (size_t)w * (size_t)h * sizeof(_color);
    u_char* data = new u_char[length];
    memcpy(data, image.data, length);

    return data;
  }
#endif
