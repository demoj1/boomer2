#include <sys/types.h>
#include <utility>

u_char*
take_screenshot(std::pair<uint, uint> display_size) noexcept;

std::pair<uint, uint>
get_screen_size() noexcept;

#ifndef XCB_SCREENSHOT
  void
  raise_window(void* handle);
#endif