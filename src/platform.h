#include <sys/types.h>
#include <utility>
#include <tuple>

std::tuple<int16_t, int16_t, int16_t, int16_t> get_window_dimensions_under_cursor() noexcept;
u_char* take_screenshot(std::tuple<int16_t, int16_t, int16_t, int16_t> rectangle) noexcept;

std::pair<int16_t, int16_t> get_screen_size() noexcept;

#ifdef XCB_SCREENSHOT
  #include <xcb/xcb.h>
  void raise_window(bool) noexcept;
#endif

// Clipboard backend: Wayland uses wl-copy, X11 uses xclip.
#ifdef WAYLAND_SCREENSHOT
  #define CLIPBOARD_IMAGE_CMD "wl-copy --type image/png < /tmp/__out_image.png"
  #define CLIPBOARD_TEXT_PIPE "wl-copy"
#else
  #define CLIPBOARD_IMAGE_CMD "xclip -selection clipboard -t image/png -i /tmp/__out_image.png"
  #define CLIPBOARD_TEXT_PIPE "xclip -selection clipboard"
#endif
