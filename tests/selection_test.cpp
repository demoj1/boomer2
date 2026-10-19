// Exercise the actual selection cache without opening a window.
#define main boomer_app_main
#include "../src/main.cpp"
#undef main

#include <chrono>
#include <random>

static bool same_color(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Color reference_average(Image image, int x0, int y0, int x1, int y1) {
  uint64_t r = 0, g = 0, b = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const auto color = GetImageColor(image, x, y);
      r += color.r; g += color.g; b += color.b;
    }
  }
  const uint64_t count = (uint64_t)(x1 - x0) * (y1 - y0);
  return {(uint8_t)(r/count), (uint8_t)(g/count), (uint8_t)(b/count), 255};
}

// Original traversal, retained only to measure the former per-frame cost.
static Color old_average(Image image) {
  float r = 0, g = 0, b = 0;
  for (int x = 0; x < image.width; ++x) {
    for (int y = 0; y < image.height; ++y) {
      const auto color = GetImageColor(image, x, y);
      r += color.r; g += color.g; b += color.b;
    }
  }
  const int count = image.width * image.height;
  return {(uint8_t)(r/count), (uint8_t)(g/count), (uint8_t)(b/count), 255};
}

int main() {
  SetTraceLogLevel(LOG_ERROR);
  constexpr int width = 3840, height = 2160;
  vector<unsigned char> pixels((size_t)width * height * 3);
  std::mt19937 random(42);
  for (auto& pixel : pixels) pixel = random() & 255;
  State selection{};
  selection.screenshot_data = pixels.data();
  selection.screenshot_image = {pixels.data(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8};
  assert(same_color(selection.selection_average_color(), Color{}));

  // Compare arbitrary areas with raylib's pixel reader, then hit the cache.
  for (int i = 0; i < 100; ++i) {
    const int x0 = random() % width, y0 = random() % height;
    const int x1 = x0 + 1 + random() % (width - x0);
    const int y1 = y0 + 1 + random() % (height - y0);
    selection.min_point = vec2{(float)x0, (float)y0};
    selection.max_point = vec2{(float)x1, (float)y1};
    const auto expected = reference_average(selection.screenshot_image, x0, y0, x1, y1);
    assert(same_color(selection.selection_average_color(), expected));
    assert(same_color(selection.selection_average_color(), expected));
  }
  selection.min_point = vec2{2, 3};
  selection.max_point = vec2{2, 100};
  assert(same_color(selection.selection_average_color(), Color{}));
  selection.max_point = vec2{200, 3};
  assert(same_color(selection.selection_average_color(), Color{}));
  selection.min_point = vec2{-10, -20};
  selection.max_point = vec2{width + 10, height + 20};
  assert(same_color(selection.selection_average_color(), reference_average(selection.screenshot_image, 0, 0, width, height)));

  selection.rectangles.push_back({vec2{1, 1}, vec2{2, 2}});
  selection.blur_rectangles.push_back({vec2{3, 3}, vec2{4, 4}});
  selection.remove_blur_rectangle();
  selection.remove_blur_rectangle();
  assert(selection.rectangles.size() == 1 && selection.blur_rectangles.empty());

  selection.min_point = vec2{0, 0};
  selection.max_point = vec2{width, height};
  const auto start = std::chrono::steady_clock::now();
  volatile auto old_color = old_average(selection.screenshot_image);
  const auto old_end = std::chrono::steady_clock::now();
  volatile auto new_color = selection.selection_average_color();
  const auto new_end = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    volatile auto cached = selection.selection_average_color();
    (void)cached;
  }
  const auto cache_end = std::chrono::steady_clock::now();
  const auto ms = [](auto duration) { return std::chrono::duration<double, std::milli>(duration).count(); };
  printf("4K selection: old %.3f ms/frame; new %.3f ms on change; cached %.6f ms/call\n",
         ms(old_end - start), ms(new_end - old_end), ms(cache_end - new_end)/100000);
  (void)old_color; (void)new_color;
  puts("PASS: 100 random areas, cached results, changed bounds, empty areas, clipped bounds, blur deletion");
}
