// Run under Xvfb: exercise the real shader and target lifetime with an
// asymmetric image, without capturing the desktop or touching the clipboard.
#define main boomer_app_main
#include "../src/main.cpp"
#undef main

int main() {
  SetTraceLogLevel(LOG_ERROR);
  InitWindow(256, 128, "boomer blur test");
  assert(IsWindowReady());
  assert(blurred_texture.id == 0 && blurred_render_target.id == 0);

  Image source = GenImageColor(256, 128, RED);
  ImageDrawRectangle(&source, 0, 64, 128, 64, BLUE);
  ImageDrawRectangle(&source, 128, 0, 128, 128, GREEN);
  auto texture = LoadTextureFromImage(source);
  build_blurred_screenshot(texture);
  assert(IsRenderTextureValid(blurred_render_target));
  auto result = LoadImageFromTexture(blurred_texture);
  assert(result.data && result.width == 256 && result.height == 128);
  // The spare target has been freed, but the final image must remain intact.
  const auto tl = GetImageColor(result, 48, 32);
  const auto bl = GetImageColor(result, 48, 96);
  const auto tr = GetImageColor(result, 192, 32);
  assert(tl.r > 200 && tl.b < 100);
  assert(bl.b > 200 && bl.r < 100);
  assert(tr.g > 200 && tr.r < 100);
  // The hard boundary must be blurred, while the original stays sharp.
  const auto seam = GetImageColor(result, 127, 32);
  assert(seam.r > 50 && seam.r < 200 && seam.g > 50);
  auto original = LoadImageFromTexture(texture);
  const auto sharp = GetImageColor(original, 10, 10);
  assert(sharp.r == RED.r && sharp.g == RED.g && sharp.b == RED.b);

  UnloadImage(original);
  UnloadImage(result);
  UnloadRenderTexture(blurred_render_target);
  UnloadTexture(texture);
  UnloadImage(source);
  CloseWindow();
  puts("PASS: blur orientation, softened edges, original texture and retained render target");
}
