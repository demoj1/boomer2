#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <sys/types.h>
#include <thread>

#include "font.h"
#include "screenshot.h"

//+Macros
static int __COUNTER = -1;

#ifdef DEBUG
  #define LOG(__format_string, ...) do { \
    printf("%s:%d (%s)@%d : " __format_string, __FILE__, __LINE__, __FUNCTION__, ++__COUNTER, ##__VA_ARGS__); \
    fflush(stdout); \
  } while (0)
#else
  #define LOG(__format_string, ...) {}
#endif

#define FF "(%06.1f; %06.1f)"
#define F(v) v.x, v.y
//-Macros

//+Extends default
inline bool operator==(Vector2 l, Vector2 r) { return l.x == r.x && l.y == r.y; }
inline Vector2 operator-(Vector2 l, Vector2 r) { return Vector2Subtract(l, r); }
inline Vector2 operator-(Vector2 l, float r) { return Vector2SubtractValue(l, r); }
inline Vector2 operator+(Vector2 l, Vector2 r) { return Vector2Add(l, r); }
inline Vector2 operator+(Vector2 l, float r) { return Vector2AddValue(l, r); }

Rectangle rect_from_vectors(Vector2 first_point, Vector2 second_point) {
  return Rectangle {
      first_point.x,
      first_point.y,
      second_point.x,
      second_point.y,
  };
}
//-Extends default

using namespace std;

static Font font;

enum Tools {
  CROSSHAIR = 1,
};
static int count_tools = 1;

struct State {
  std::pair<uint, uint> screen_size;
  u_char* screenshot_data;
  Texture2D screenshot_texture;
  Camera2D camera;

  std::optional<Vector2> first_point = nullopt;
  std::optional<Vector2> second_point = nullopt;

  std::optional<Vector2> min_point = nullopt;
  std::optional<Vector2> max_point = nullopt;

  bool select_area_in_progress = false;

  int tools;

  inline State* active_tools(Tools tool) { this->tools &= tool; return this; }
  inline bool toggle_tools(Tools tool) { this->tools = !(this->tools & tool); return this->tools & tool; }
  inline bool check_tools(Tools tool) { return this->tools & tool; }

  inline uint swidth() { return this->screen_size.first; }
  inline uint sheight() { return this->screen_size.second; }

  State* _recalc_min_max_points() {
    min_point = {
      min(first_point->x, second_point->x),
      min(first_point->y, second_point->y),
    };

    max_point = {
      max(first_point->x, second_point->x),
      max(first_point->y, second_point->y),
    };

    return this;
  }

  State* _fix_bound_point(std::optional<Vector2>* point) {
    assert(point);

    (*point)->x = (*point)->x <= 0 ? 0
      : (*point)->x >= swidth()
      ? swidth()
      : (*point)->x;

    (*point)->y = (*point)->y <= 0 ? 0
      : (*point)->y >= sheight()
      ? sheight()
      : (*point)->y;

    return this;
  }

  State* _set_point(Vector2 p, std::optional<Vector2>* target, std::optional<Vector2>* neighboring) {
    assert(target);
    assert(neighboring);

    if ((*target) == p) return this;
    p.x = round(p.x);
    p.y = round(p.y);

    if (!(*neighboring).has_value()) {
      (*target) = p;
      return this;
    }

    (*target) = p;
    return _fix_bound_point(target)
      ->_fix_bound_point(neighboring)
      ->_recalc_min_max_points();
  }

  State* set_first_point(Vector2 p) { return _set_point(p, &first_point, &second_point); }
  State* set_second_point(Vector2 p) { return _set_point(p, &second_point, &first_point); }

  State* draw_shading() {
    if (!this->first_point.has_value() || !this->second_point.has_value()) return this;
    if (*this->first_point == *this->second_point) return this;

    const int alpha = 180;
    const Color color = {10, 10, 10, alpha};

    const Vector2 points[] = {
      { 0                           , 0                        },
      { min_point->x                , (float)sheight()         },
      { min_point->x                , 0                        },
      { swidth() - min_point->x     , min_point->y             },
      { max_point->x                , min_point->y             },
      { swidth() - max_point->x     , sheight() - min_point->y },
      { min_point->x                , max_point->y             },
      { max_point->x - min_point->x , sheight() - max_point->y },
    };

    for (int i = 0; i < 8; i += 2)
      DrawRectangle( points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, color );

    return this;
  }

  State* draw_selection_box() {
    if (!min_point.has_value() || !max_point.has_value()) return this;

    DrawRectangleLinesEx( rect_from_vectors( *min_point, *max_point - *min_point ), 2 / camera.zoom, BLUE );

    if (select_area_in_progress) {
      const Vector2 points[] = {
        { 0                               , first_point->y                   },
        { (float)screenshot_texture.width , first_point->y                   },
        { first_point->x                  , 0                                },
        { first_point->x                  , (float)screenshot_texture.height },
        { 0                               , second_point->y                  },
        { (float)screenshot_texture.width , second_point->y                  },
        { second_point->x                 , 0                                },
        { second_point->x                 , (float)screenshot_texture.height },
      };

      for (int i = 0; i < 8; i += 2)
        DrawLineEx( points[i], points[i + 1], 1 / camera.zoom, BLUE );
    }

    return this;
  }

  State* draw_tool_pallete() {
    // if (!tools) return this;

    // int tools_count = __builtin_popcount(tools);

    for (int i = 0; i < count_tools; i++) {
      DrawCircleGradient(400 + 50*i, 400, 40, {230, 230, 230, 255}, WHITE);

      switch (tools & (int)pow(2.0, (float)i)) {
      case 1:
        DrawText("CR", 400 + 50*i - 40/2, 400 - 40/2, 36, BLACK);
        break;
      }
    }

    return this;
  }

  State* draw_debug_line() {
    static char* debug_buffer = (char*)malloc(2048);

    auto texture_pos = GetScreenToWorld2D(GetMousePosition(), camera);
    auto selection_pos = texture_pos - *first_point;

    sprintf(debug_buffer,
      "Mouse: " FF ", "
      "Mouse to texture: " FF ", "
      "Mouse into selection: " FF ", "
      "FPS: %d",
      F(GetMousePosition()),
      F(texture_pos),
      F(selection_pos),
      GetFPS()
    );

    DrawRectangle(0, 0, swidth(), 40, {40, 40, 40, 150});
    DrawTextEx(font, debug_buffer, {5, 5}, 32, 1, GREEN);

    return this;
  }

  Image render_screenshot_and_close() {
    LOG("Begin load image from texture\n");

    auto screen_first_point = min_point.value_or(Vector2{ 0, 0 });
    auto screen_second_point = max_point.value_or(Vector2{
      static_cast<float>(swidth()),
      static_cast<float>(sheight())
    });

    if (screen_first_point == screen_second_point) {
      screen_first_point = {0, 0};
      screen_second_point = {
        static_cast<float>(swidth()),
        static_cast<float>(sheight())
      };
    }

    auto width = screen_second_point.x - screen_first_point.x;
    auto height = screen_second_point.y - screen_first_point.y;

    auto render_screenshot_texture = LoadRenderTexture(screen_second_point.x - screen_first_point.x, screen_second_point.y - screen_first_point.y);

    // Render all objects into texture
    BeginDrawing();
      BeginTextureMode(render_screenshot_texture);
        DrawTextureRec(screenshot_texture, {
          screen_first_point.x,
          screen_first_point.y,
          width,
          -height,
        }, {0, 0}, {255, 255, 255, 255});

          if (check_tools(Tools::CROSSHAIR)) {
            auto texture_pos = GetScreenToWorld2D(GetMousePosition(), camera);
            auto selection_pos = texture_pos - *min_point;

            DrawLineEx(
              { 0,     height - selection_pos.y },
              { width, height - selection_pos.y },
              2,
              MAGENTA
            );

            DrawLineEx(
              { selection_pos.x, 0},
              { selection_pos.x, height },
              2,
              MAGENTA
            );
          }
      EndTextureMode();
    EndDrawing();

    return LoadImageFromTexture(render_screenshot_texture.texture);;
  }
};
static State* state = new State{};


int main() {
  state->screen_size = get_screen_size();
  auto thread = std::thread([]() { state->screenshot_data = take_screenshot(state->screen_size); });
  std::thread export_thread;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  SetTargetFPS(80);

  #ifndef DEBUG
  SetTraceLogLevel(LOG_ERROR);
  #endif

  InitWindow(state->swidth(), state->sheight(), "_");
  SetWindowFocused();
  BeginDrawing(); ClearBackground({0, 0, 0, 0}); EndDrawing();

  #ifdef DEBUG
  font = LoadFont_Terminus();
  #endif

  thread.join();

  state->screenshot_texture = LoadTextureFromImage((Image){
      .data = state->screenshot_data,
      .width = (int)state->swidth(),
      .height = (int)state->sheight(),
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
  });

  state->camera = {0};
  state->camera.zoom = 1.0;

  Vector2 prevMousePos = GetMousePosition();

  SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
  while (!WindowShouldClose()) {
    Vector2 thisPos = GetMousePosition();

    float wheel = GetMouseWheelMove();

    if (wheel != 0) {
      Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), state->camera);

      state->camera.offset = GetMousePosition();
      state->camera.target = mouseWorldPos;

      state->camera.zoom += wheel * log(state->camera.zoom + 0.9);

      if (state->camera.zoom <= 0.6f) state->camera.zoom = 0.6f;
      if (state->camera.zoom >= 80) state->camera.zoom = 80.0f;
    }

    Vector2 delta = prevMousePos - thisPos;
    prevMousePos = thisPos;

    if (IsMouseButtonDown(0)) {
      SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
      state->camera.target = GetScreenToWorld2D(state->camera.offset + delta, state->camera);
    }

    if (IsMouseButtonUp(0)) SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);

    if (IsKeyPressed(KEY_R)) {
      // auto w = GetScreenWidth();
      // auto h = GetScreenHeight();
      // SetWindowSize(0, 0);
      // state->screenshot_data = take_screenshot(state->screen_size);
      // state->screenshot_texture = LoadTextureFromImage((Image){
      //     .data = state->screenshot_data,
      //     .width = (int)state->swidth(),
      //     .height = (int)state->sheight(),
      //     .mipmaps = 1,
      //     .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
      // });
      // SetWindowSize(w, h);
    }

    if (IsKeyPressed(KEY_F)) state->toggle_tools(Tools::CROSSHAIR);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_C)) {
      auto image = state->render_screenshot_and_close();

      export_thread = std::thread([image]() {
        if (ExportImage(image, "/tmp/__out_image.png")) {
          if (system("xclip -selection clipboard -t image/png -i /tmp/__out_image.png") != 0) {
            LOG("xclip failed");
            assert(false);
          }

          UnloadImage(image);
        }
      });

      goto close;
    }

    BeginDrawing();
      ClearBackground((Color){0, 42, 90, 255});

      BeginMode2D(state->camera);
        DrawTexture(state->screenshot_texture, 0, 0, WHITE);

        if (IsMouseButtonDown(1)) {
          if (!state->select_area_in_progress) state->first_point = nullopt;
          state->select_area_in_progress = true;
          if (!state->first_point.has_value()) state->set_first_point(GetScreenToWorld2D(thisPos, state->camera));

          state->set_second_point(GetScreenToWorld2D(thisPos, state->camera));
        } else {
          state->select_area_in_progress = false;
        }

        state->draw_shading();
        state->draw_selection_box();
      EndMode2D();

      #ifdef DEBUG
      state->draw_debug_line();
      #endif

      // state->draw_tool_pallete();

      if (state->check_tools(Tools::CROSSHAIR)) {
        DrawLineEx(
          { 0,                                      thisPos.y },
          { (float)state->screenshot_texture.width, thisPos.y},
          2,
          MAGENTA
        );
        DrawLineEx(
          { thisPos.x, 0 },
          { thisPos.x, (float)state->screenshot_texture.height},
          2,
          MAGENTA
        );
      }
    EndDrawing();
  }

close:
  UnloadTexture(state->screenshot_texture);
  delete[] state->screenshot_data;
  CloseWindow();
  export_thread.join();
}
