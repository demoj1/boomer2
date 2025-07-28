#include <sys/types.h>
#include <utility>
#include <tuple>

std::tuple<int16_t, int16_t, int16_t, int16_t> get_window_dimensions_under_cursor() noexcept;
u_char* take_screenshot(std::tuple<int16_t, int16_t, int16_t, int16_t> rectangle) noexcept;

std::pair<int16_t, int16_t> get_screen_size() noexcept;

#ifdef XCB_SCREENSHOT
  #include <xcb/xcb.h>
  void raise_window(bool) noexcept;
#else
  void raise_window(void* handle);
#endif
