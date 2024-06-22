#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include "./screenshot.h"

using namespace std;

int
main()
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(120);

  auto screen_size = get_screen_size();
  u_char* data = take_screenshot(screen_size);

  SetTraceLogLevel(LOG_ERROR);
  InitWindow(screen_size.first, screen_size.second, "_");
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  // ToggleFullscreen();

  Image image = { .data = data,
                  .width = (int)screen_size.first,
                  .height = (int)screen_size.second,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
                  .mipmaps = 1 };

  Texture2D texture = LoadTextureFromImage(image);

  Camera2D cam = { 0 };
  cam.zoom = 1;
  cam.offset.x = (int)screen_size.first/2;
  cam.offset.y = (int)screen_size.second/2;
  Vector2 prevMousePos = GetMousePosition();

  while (!WindowShouldClose()) {
    Vector2 thisPos = GetMousePosition();

    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
      Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), cam);

      cam.offset = GetMousePosition();
      cam.target = mouseWorldPos;

      cam.zoom += wheel * 0.7f;
      if (cam.zoom <= 0.6f) cam.zoom = 0.6f;
      if (cam.zoom >= 15) cam.zoom = 15.0f;
    }


    Vector2 delta = Vector2Subtract(prevMousePos, thisPos);
    prevMousePos = thisPos;

    if (IsMouseButtonDown(0)) {
      cam.target = GetScreenToWorld2D(Vector2Add(cam.offset, delta), cam);
    }

    BeginDrawing();
      ClearBackground((Color){ 0, 42, 90, 255 });
      BeginMode2D(cam);
        rlPushMatrix();
          rlTranslatef(0, 3000, 0);
          rlRotatef(90, 1, 0, 0);
          DrawGrid(texture.width / 10 , 30);
        rlPopMatrix();
        DrawTexture(texture, -texture.width/2, -texture.height/2, WHITE);
      EndMode2D();
    EndDrawing();
  }

  CloseWindow();
}