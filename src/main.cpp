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
#include <vector>

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

  #define BINARY_F "%c%c%c%c%c%c%c%c"
  #define BYTE_TO_BIN(byte)  \
    ((byte) & 0x80 ? '1' : '0'), \
    ((byte) & 0x40 ? '1' : '0'), \
    ((byte) & 0x20 ? '1' : '0'), \
    ((byte) & 0x10 ? '1' : '0'), \
    ((byte) & 0x08 ? '1' : '0'), \
    ((byte) & 0x04 ? '1' : '0'), \
    ((byte) & 0x02 ? '1' : '0'), \
    ((byte) & 0x01 ? '1' : '0')
//-Macros

//+Extends default
  using vec2 = Vector2;

  inline bool operator==(const vec2& l, const vec2& r) noexcept { return l.x == r.x && l.y == r.y; }
  inline vec2 operator-(const vec2& l, const vec2& r) noexcept { return Vector2Subtract(l, r); }
  inline vec2 operator-(const vec2& l, const float& r) noexcept { return Vector2SubtractValue(l, r); }
  inline vec2 operator+(const vec2& l, const vec2& r) noexcept { return Vector2Add(l, r); }
  inline vec2 operator+(const vec2& l, const float& r) noexcept { return Vector2AddValue(l, r); }
  inline vec2& round(vec2& l) noexcept {
    l.x = round(l.x);
    l.y = round(l.y);
    return l;
  }

  inline vec2 Vector2AddXValue(vec2 v, float val) { return { v.x + val, v.y }; }
  inline vec2 Vector2AddYValue(vec2 v, float val) { return { v.x, v.y + val }; }
  inline vec2 Vector2MultiplyValue(vec2 v, float val) { return { v.x * val, v.y * val }; }

  Rectangle rect_from_vectors(vec2 first_point, vec2 second_point) noexcept {
    vec2 min_point = {
      fmin(first_point.x, second_point.x),
      fmin(first_point.y, second_point.y),
    };

    vec2 max_point = {
      fmax(first_point.x, second_point.x),
      fmax(first_point.y, second_point.y),
    };

    auto sizes = max_point - min_point;

    return Rectangle {
        min_point.x,
        min_point.y,
        sizes.x,
        sizes.y
    };
  }
//-Extends default

using namespace std;

static Font font;

enum Tools {
  CROSSHAIR = 1,
  LINE      = 2,
  RECTANGLE = 4,
  ARROW     = 8,
};
static int count_tools = 4;

struct State {
  pair<uint, uint> screen_size;
  u_char* screenshot_data;
  Texture2D screenshot_texture;
  Camera2D camera = {};

  optional<vec2> first_point = nullopt;
  optional<vec2> second_point = nullopt;

  optional<vec2> min_point = nullopt;
  optional<vec2> max_point = nullopt;

  bool select_area_in_progress = false;

  char tools;

  vector<vec2> crosshairs = {};
  vector<pair<optional<vec2>, optional<vec2>>> lines = {};
  vector<pair<optional<vec2>, optional<vec2>>> arrows = {};
  vector<pair<optional<vec2>, optional<vec2>>> rectangles = {};

  inline State* activate_tools(Tools tool) { this->tools |= tool; return this; }
  inline State* deactivate_tools(Tools tool) { this->tools &= ~tool; return this; }
  inline State* reset_tools() { this->tools = 0; return this; }
  inline bool toggle_tools(Tools tool) { this->tools ^= tool; return this->tools & tool; }
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

  State* _fix_bound_point(std::optional<vec2>* point) {
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

  State* _set_point(vec2 p, std::optional<vec2>* target, std::optional<vec2>* neighboring) {
    assert(target);
    assert(neighboring);

    if ((*target) == p) return this;

    if (!(*neighboring).has_value()) {
      (*target) = round(p);
      return this;
    }

    (*target) = p;
    return _fix_bound_point(target)
      ->_fix_bound_point(neighboring)
      ->_recalc_min_max_points();
  }

  State* set_first_point(vec2 p) { return _set_point(p, &first_point, &second_point); }
  State* set_second_point(vec2 p) { return _set_point(p, &second_point, &first_point); }

  State* draw_shading() {
    if (!this->first_point.has_value() || !this->second_point.has_value()) return this;
    if (*this->first_point == *this->second_point) return this;

    const int alpha = 180;
    const Color color = {10, 10, 10, alpha};

    const vec2 points[] = {
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
    auto size = *max_point - *min_point;

    DrawRectangleLinesEx( { min_point->x, min_point->y, size.x, size.y }, 2 / camera.zoom, BLUE );

    if (select_area_in_progress) {
      const vec2 points[] = {
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
    const auto radius = 20;
    const auto space = radius * count_tools;

    for (int i = 0; i < count_tools; i++) {
      const float x = GetMousePosition().x + cos(i) * radius*4;
      const float y = GetMousePosition().y + sin(i) * radius*4;

      DrawCircleLines(x, y, radius, BLACK);
      DrawCircleGradient(x, y, radius, {230, 230, 230, 100}, {255, 255, 255, 100});

      if (tools & (1 << i)) {
        DrawCircleGradient(x, y, radius, GREEN, {230, 230, 230, 255});
      }

      switch (i) {
        case 0: DrawTextEx(font, "Cr", { static_cast<float>(x - radius/2.0f), y - radius/2.0f }, radius, 1, BLACK); break;
        case 1: DrawTextEx(font, "Li", { static_cast<float>(x - radius/2.0f), y - radius/2.0f }, radius, 1, BLACK); break;
        case 2: DrawTextEx(font, "Re", { static_cast<float>(x - radius/2.0f), y - radius/2.0f }, radius, 1, BLACK); break;
        case 3: DrawTextEx(font, "Ar", { static_cast<float>(x - radius/2.0f), y - radius/2.0f }, radius, 1, BLACK); break;
      }
    }

    return this;
  }

  State* draw_debug_line() {
    static char debug_buffer[2048];

    auto texture_pos = GetScreenToWorld2D(GetMousePosition(), camera);
    auto selection_pos = texture_pos - *first_point;

    sprintf(debug_buffer,
      "Mouse: " FF ", "
      "Mouse to texture: " FF ", "
      "Mouse into selection: " FF ", "
      "FPS: %d, "
      "Tools: " BINARY_F
      "\n\n"
      "Last rectangle: [" FF ", " FF "], Crosshair: " FF,
      F(GetMousePosition()),
      F(texture_pos),
      F(selection_pos),
      GetFPS(),
      BYTE_TO_BIN(tools),
      rectangles.size() > 0 ? rectangles.back().first->x : 0.0,
      rectangles.size() > 0 ? rectangles.back().first->y : 0.0,
      rectangles.size() > 0 ? rectangles.back().second->x : 0.0,
      rectangles.size() > 0 ? rectangles.back().second->y : 0.0,
      crosshairs.size() > 0 ? crosshairs.back().x : 0.0,
      crosshairs.size() > 0 ? crosshairs.back().y : 0.0
    );

    DrawRectangle(0, 0, swidth(), 80, {40, 40, 40, 150});
    DrawTextEx(font, debug_buffer, {5, 5}, 32, 1, GREEN);

    return this;
  }

  State* draw_crosshairs() {
    int i = 0;
    for (auto& c : crosshairs) {
      if (i++ == crosshairs.size() - 1 && !check_tools(Tools::CROSSHAIR)) break;

      DrawLineEx(
        { 0, c.y },
        { (float)screenshot_texture.width, c.y},
        3,
        MAGENTA
      );

      DrawLineEx(
        { c.x, 0 },
        { c.x, (float)screenshot_texture.height},
        3,
        MAGENTA
      );
    }

    return this;
  }

  State* draw_lines() {
      int i = 0;
      for (auto& [fp, sp] : lines) {
        if (i++ == lines.size() - 1 && !check_tools(Tools::LINE)) break;
        if (!fp.has_value() || !sp.has_value()) break;

        DrawLineEx(
          { fp->x, fp->y },
          { sp->x, sp->y },
          5,
          MAGENTA
        );
      }

      return this;
  }

  State* draw_arrows() {
      int i = 0;
      for (auto& [fp, sp] : arrows) {
        if (i++ == arrows.size() - 1 && !check_tools(Tools::ARROW)) break;
        if (!fp.has_value() || !sp.has_value()) break;

        DrawLineEx(
          { fp->x, fp->y },
          { sp->x, sp->y },
          5,
          MAGENTA
        );

        vec2 v = Vector2Normalize(*sp - *fp);
        auto a_ = Vector2Rotate(v,  -PI/0.30);
        auto b_ = Vector2Rotate(v,   PI/0.30);

        a_ = Vector2MultiplyValue(Vector2Normalize(a_ - v), 45.0f) + *sp;
        b_ = Vector2MultiplyValue(Vector2Normalize(b_ - v), 45.0f) + *sp;

        DrawLineEx(
          a_,
          *sp,
          7,
          MAGENTA
        );
        DrawLineEx(
          b_,
          *sp,
          7,
          MAGENTA
        );
      }

      return this;
  }

  State* draw_rectangles() {
      int i = 0;
      for (auto& [fp, sp] : rectangles) {
        if (i++ == rectangles.size() - 1 && !check_tools(Tools::RECTANGLE)) break;
        if (!fp.has_value() || !sp.has_value()) break;

        DrawRectangleRoundedLines(rect_from_vectors(*fp, *sp), 0.05, 10, 5, MAGENTA);
      }

      return this;
  }


  State* update_last_rectangle_first_point(vec2 l) {
    if (rectangles.size() < 1) rectangles.push_back({});

    auto& last_rectangle = rectangles.back();

    last_rectangle.first = round(l);
    return this;
  }

  State* update_last_rectangle_second_point(vec2 l) {
    if (rectangles.size() < 1) rectangles.push_back({});
    round(l);

    auto& last_rectangle = rectangles.back();

    last_rectangle.second = round(l);
    return this;
  }

  State* add_new_rectangle() {
    rectangles.push_back({});
    return this;
  }

  State* remove_rectangle() {
    if (rectangles.size() > 0) rectangles.pop_back();
    return this;
  }


  State* update_last_line_first_point(vec2 l) {
    if (lines.size() < 1) lines.push_back({});
    auto& last_line = lines.back();

    last_line.first = round(l);
    return this;
  }

  State* update_last_line_second_point(vec2 l) {
    if (lines.size() < 1) lines.push_back({});
    auto& last_line = lines.back();

    last_line.second = round(l);
    return this;
  }

  State* add_new_line() {
    lines.push_back({});
    return this;
  }

  State* remove_line() {
    if (lines.size() > 0) lines.pop_back();
    return this;
  }


  State* update_last_arrow_first_point(vec2 l) {
    if (arrows.size() < 1) arrows.push_back({});
    auto& last_arrow = arrows.back();

    last_arrow.first = round(l);
    return this;
  }

  State* update_last_arrow_second_point(vec2 l) {
    if (arrows.size() < 1) arrows.push_back({});
    auto& last_arrow = arrows.back();

    last_arrow.second = round(l);
    return this;
  }

  State* add_new_arrow() {
    arrows.push_back({});
    return this;
  }

  State* remove_arrow() {
    if (arrows.size() > 0) arrows.pop_back();
    return this;
  }


  State* update_last_crosshair(vec2 v) {
    if (crosshairs.size() < 1) crosshairs.push_back({});
    crosshairs.back() = round(v);
    return this;
  }

  State* add_new_crosshair() {
    crosshairs.push_back({});
    return this;
  }

  State* remove_crosshair() {
    if (crosshairs.size() > 0) crosshairs.pop_back();
    return this;
  }

  Image render_screenshot_and_close() {
    LOG("Begin load image from texture\n");

    auto screen_first_point = min_point.value_or(vec2{ 0, 0 });
    auto screen_second_point = max_point.value_or(vec2{
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

          int i = 0;
          for (auto& c : crosshairs) {
            if (i++ == crosshairs.size() - 1 && !check_tools(Tools::CROSSHAIR)) break;
            auto selection_pos = c - *min_point;

            DrawLineEx(
              { 0,     height - selection_pos.y },
              { width, height - selection_pos.y },
              3,
              MAGENTA
            );

            DrawLineEx(
              { selection_pos.x, 0},
              { selection_pos.x, height },
              3,
              MAGENTA
            );
          }

          i = 0;
          for (auto& [f, s] : lines) {
            if (i++ == lines.size() - 1 && !check_tools(Tools::LINE)) break;
            auto f_ = *f - *min_point;
            auto s_ = *s - *min_point;

            DrawLineEx(
              { f_.x, height - f_.y },
              { s_.x, height - s_.y },
              5,
              MAGENTA
            );
          }

          i = 0;
          for (auto& [f, s] : rectangles) {
            if (i++ == rectangles.size() - 1 && !check_tools(Tools::RECTANGLE)) break;
            auto f_ = *f - *min_point;
            auto s_ = *s - *min_point;

            DrawRectangleRoundedLines(rect_from_vectors(
              { f_.x, height - f_.y },
              { s_.x, height - s_.y }
            ), 0.05, 10, 5, MAGENTA);
          }

          i = 0;
          for (auto& [f, s] : arrows) {
            if (i++ == arrows.size() - 1 && !check_tools(Tools::ARROW)) break;
            auto f_ = *f - *min_point;
            auto s_ = *s - *min_point;

            DrawLineEx(
              { f_.x, height - f_.y },
              { s_.x, height - s_.y },
              5,
              MAGENTA
            );

            vec2 v = Vector2Normalize(s_ - f_);
            auto a_ = Vector2Rotate(v,  -PI/0.30);
            auto b_ = Vector2Rotate(v,   PI/0.30);

            a_ = Vector2MultiplyValue(Vector2Normalize(a_ - v), 45.0f) + s_;
            b_ = Vector2MultiplyValue(Vector2Normalize(b_ - v), 45.0f) + s_;

            DrawLineEx(
              { a_.x, height - a_.y },
              { s_.x, height - s_.y },
              7,
              MAGENTA
            );
            DrawLineEx(
              { b_.x, height - b_.y },
              { s_.x, height - s_.y },
              7,
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
  auto load_screenshot_thread = std::thread([]() { state->screenshot_data = take_screenshot(state->screen_size); });
  std::thread export_thread;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  SetTargetFPS(80);

#ifndef DEBUG
  SetTraceLogLevel(LOG_ERROR);
#endif

  InitWindow(state->swidth(), state->sheight(), "_");
  // SetExitKey(0);
  SetWindowFocused();
  BeginDrawing(); ClearBackground({0, 0, 0, 0}); EndDrawing();

  font = LoadFont_Terminus();

  load_screenshot_thread.join();

  state->screenshot_texture = LoadTextureFromImage((Image){
      .data = state->screenshot_data,
      .width = (int)state->swidth(),
      .height = (int)state->sheight(),
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
  });

  state->camera.zoom = 1.0;

  vec2 prevMousePos = GetMousePosition();

  SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
  while (!WindowShouldClose()) {
    auto thisPos = GetMousePosition();
    auto wheel   = GetMouseWheelMove();

    if (wheel != 0) {
      vec2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), state->camera);

      state->camera.offset = GetMousePosition();
      state->camera.target = mouseWorldPos;

      state->camera.zoom += wheel * log(state->camera.zoom + 0.9);

      if (state->camera.zoom <= 0.3f) state->camera.zoom = 0.3f;
      if (state->camera.zoom >= 100) state->camera.zoom = 100.0f;
    }

    vec2 delta = prevMousePos - thisPos;
    prevMousePos = thisPos;

    if (IsMouseButtonDown(0)) {
      SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
      state->camera.target = GetScreenToWorld2D(state->camera.offset + delta, state->camera);
    } else {
      SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      state->reset_tools();
    }

    // Tools::CROSSHAIR
      if (IsKeyDown( KEY_F )) {
        state->activate_tools(Tools::CROSSHAIR);
        if (IsMouseButtonPressed(0)) state->add_new_crosshair();
        if (IsMouseButtonPressed(1)) state->remove_crosshair();
      } else { state->deactivate_tools(Tools::CROSSHAIR); }

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

        if (IsMouseButtonDown(1) && IsKeyUp(KEY_LEFT_SHIFT)) {
          if (!state->select_area_in_progress) state->first_point = nullopt;
          state->select_area_in_progress = true;
          if (!state->first_point.has_value()) state->set_first_point(GetScreenToWorld2D(thisPos, state->camera));

          state->set_second_point(GetScreenToWorld2D(thisPos, state->camera));
        } else {
          state->select_area_in_progress = false;
        }

        if (state->check_tools(Tools::CROSSHAIR)) {
          state->update_last_crosshair(GetScreenToWorld2D(thisPos, state->camera));
        }

        // Tools::LINE
          if (IsKeyDown( KEY_S ) && !(state->check_tools(Tools::LINE))) {
            state->activate_tools(Tools::LINE);
            state->update_last_line_first_point(GetScreenToWorld2D(thisPos, state->camera));
          }

          if (IsKeyUp( KEY_S ) && state->check_tools(Tools::LINE)) state->deactivate_tools(Tools::LINE);

          if (state->check_tools(Tools::LINE)) {
            state->update_last_line_second_point(GetScreenToWorld2D(thisPos, state->camera));

            if (IsMouseButtonPressed(0)) {
              state->add_new_line();
              state->deactivate_tools(Tools::LINE);
            }

            if (IsMouseButtonPressed(1)) state->remove_line();
          }

        // Tools::ARROW
          if (IsKeyDown( KEY_A ) && !(state->check_tools(Tools::ARROW))) {
            state->activate_tools(Tools::ARROW);
            state->update_last_arrow_first_point(GetScreenToWorld2D(thisPos, state->camera));
          }

          if (IsKeyUp( KEY_A ) && state->check_tools(Tools::ARROW)) state->deactivate_tools(Tools::ARROW);

          if (state->check_tools(Tools::ARROW)) {
            state->update_last_arrow_second_point(GetScreenToWorld2D(thisPos, state->camera));

            if (IsMouseButtonPressed(0)) {
              state->add_new_arrow();
              state->deactivate_tools(Tools::ARROW);
            }

            if (IsMouseButtonPressed(1)) state->remove_arrow();
          }

        // Tools::RECTANGLE
          if (IsKeyDown( KEY_R ) && !(state->check_tools(Tools::RECTANGLE))) {
            state->activate_tools(Tools::RECTANGLE);
            LOG("Update first rect point before: " FF "\n", F(GetScreenToWorld2D(thisPos, state->camera)));
            state->update_last_rectangle_first_point(GetScreenToWorld2D(thisPos, state->camera));
          }

          if (IsKeyUp( KEY_R ) && state->check_tools(Tools::RECTANGLE)) state->deactivate_tools(Tools::RECTANGLE);

          if (state->check_tools(Tools::RECTANGLE)) {
            LOG("Update last rect point before: " FF "\n", F(GetScreenToWorld2D(thisPos, state->camera)));
            state->update_last_rectangle_second_point(GetScreenToWorld2D(thisPos, state->camera));

            if (IsMouseButtonPressed(0)) {
              state->add_new_rectangle();
              state->deactivate_tools(Tools::RECTANGLE);
            }

            if (IsMouseButtonPressed(1)) state->remove_rectangle();
          }

        state
          ->draw_shading()
          ->draw_selection_box()
          ->draw_crosshairs()
          ->draw_lines()
          ->draw_arrows()
          ->draw_rectangles();
      EndMode2D();

      #ifdef DEBUG
      state->draw_debug_line();
      #endif

      state->draw_tool_pallete();
    EndDrawing();
  }

close:
  UnloadTexture(state->screenshot_texture);
  delete[] state->screenshot_data;
  CloseWindow();
  if (export_thread.joinable()) export_thread.join();
}
