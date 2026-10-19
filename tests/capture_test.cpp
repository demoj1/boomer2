#include "../src/platform.cpp"
#include <cassert>

int main() {
  SetTraceLogLevel(LOG_ERROR);
  Image source = GenImageColor(32, 16, RED);
  ImageDrawRectangle(&source, 16, 0, 16, 16, BLUE);
  assert(ExportImage(source, getenv("BOOMER_TEST_IMAGE")));
  UnloadImage(source);

  for (const char* output : {"", "DP-1"}) {
    Image image = grab_output(output);
    assert(image.data && image.width == 32 && image.height == 16);
    assert(image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    const auto red = GetImageColor(image, 2, 2);
    const auto blue = GetImageColor(image, 30, 2);
    assert(red.r == RED.r && red.g == RED.g && red.b == RED.b);
    assert(blue.r == BLUE.r && blue.g == BLUE.g && blue.b == BLUE.b);
    UnloadImage(image);
  }
  assert(!grab_output("invalid;name").data);
  setenv("BOOMER_TEST_CAPTURE", "failure", 1);
  assert(!grab_output().data);
  setenv("BOOMER_TEST_CAPTURE", "invalid", 1);
  assert(!grab_output().data);
  puts("PASS: PNG pipe, RGB conversion, named output, failed command, invalid image");
}
