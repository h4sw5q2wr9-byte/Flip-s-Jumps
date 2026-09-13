#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct Canvas Canvas;
typedef struct Gui Gui;
typedef struct ViewPort ViewPort;
typedef enum { ColorWhite, ColorBlack } Color;
typedef enum { FontPrimary, FontSecondary } Font;
typedef enum { AlignLeft, AlignRight, AlignTop, AlignBottom, AlignCenter } Align;
void canvas_clear(Canvas* c);
void canvas_set_color(Canvas* c, Color color);
void canvas_set_font(Canvas* c, Font font);
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h);
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r);
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r);
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y);
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* str);
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* str);
uint16_t canvas_string_width(Canvas* c, const char* str);
