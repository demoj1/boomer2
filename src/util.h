#pragma once

//+Macros
#ifdef DEBUG
  static int __COUNTER = -1;

  #define LOG(__format_string, ...) do { \
    printf("%s:%d (%s)@%d : " __format_string, __FILE__, __LINE__, __FUNCTION__, ++__COUNTER, ##__VA_ARGS__); \
    fflush(stdout); \
  } while (0)
#else
  #define LOG(__format_string, ...) {}
#endif

#ifdef BENCH
  #define __BENCH 1
#else
  #define __BENCH 0
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

enum Tools {
  CROSSHAIR = 1 << 0,
  LINE      = 1 << 1,
  RECTANGLE = 1 << 2,
  ARROW     = 1 << 3,
  BLUR_RECTANGLE = 1 << 4,
  COLOR_PICKER = 1 << 5,
};

static int count_tools = 6;
static Font font;

using vec2 = Vector2;

float clampf(float sv, float smin, float smax, float dmin, float dmax) {
  return ((sv - smin) / (smax - smin)) * (dmax - dmin);
}
