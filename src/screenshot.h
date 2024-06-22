#include <sys/types.h>
#include <utility>

u_char* take_screenshot(std::pair<uint, uint> display_size);
std::pair<uint, uint> get_screen_size();
void raise_window(uint* handle);