/* Minimal canvas emulator: renders Flipper draw calls into a 128x64 bitmap. */
#include <string.h>
#include <stdio.h>
#include <gui/gui.h>

#define W 128
#define H 64
unsigned char fb[H][W];
static Color cur_color = ColorBlack;
static Font cur_font = FontSecondary;

/* 3x5 glyphs; advance matches the width function so layout is faithful. */
static const char* glyph(char ch) {
    switch(ch) {
    case 'A': return "###" "# #" "###" "# #" "# #";
    case 'B': return "## " "# #" "## " "# #" "## ";
    case 'C': return "###" "#  " "#  " "#  " "###";
    case 'D': return "## " "# #" "# #" "# #" "## ";
    case 'E': return "###" "#  " "## " "#  " "###";
    case 'F': return "###" "#  " "## " "#  " "#  ";
    case 'G': return "###" "#  " "# #" "# #" "###";
    case 'H': return "# #" "# #" "###" "# #" "# #";
    case 'I': return "###" " # " " # " " # " "###";
    case 'J': return "  #" "  #" "  #" "# #" "###";
    case 'K': return "# #" "# #" "## " "# #" "# #";
    case 'L': return "#  " "#  " "#  " "#  " "###";
    case 'M': return "# #" "###" "###" "# #" "# #";
    case 'N': return "## " "# #" "# #" "# #" "# #";
    case 'O': return "###" "# #" "# #" "# #" "###";
    case 'P': return "###" "# #" "###" "#  " "#  ";
    case 'Q': return "###" "# #" "# #" "###" "  #";
    case 'R': return "###" "# #" "## " "# #" "# #";
    case 'S': return "###" "#  " "###" "  #" "###";
    case 'T': return "###" " # " " # " " # " " # ";
    case 'U': return "# #" "# #" "# #" "# #" "###";
    case 'V': return "# #" "# #" "# #" "# #" " # ";
    case 'W': return "# #" "# #" "###" "###" "# #";
    case 'X': return "# #" "# #" " # " "# #" "# #";
    case 'Y': return "# #" "# #" " # " " # " " # ";
    case 'Z': return "###" "  #" " # " "#  " "###";
    case '0': return "###" "# #" "# #" "# #" "###";
    case '1': return " # " "## " " # " " # " "###";
    case '2': return "###" "  #" "###" "#  " "###";
    case '3': return "###" "  #" "###" "  #" "###";
    case '4': return "# #" "# #" "###" "  #" "  #";
    case '5': return "###" "#  " "###" "  #" "###";
    case '6': return "###" "#  " "###" "# #" "###";
    case '7': return "###" "  #" "  #" "  #" "  #";
    case '8': return "###" "# #" "###" "# #" "###";
    case '9': return "###" "# #" "###" "  #" "###";
    case ':': return "   " " # " "   " " # " "   ";
    case '!': return " # " " # " " # " "   " " # ";
    case '\'': return " # " " # " "   " "   " "   ";
    case '<': return "  #" " # " "#  " " # " "  #";
    case '>': return "#  " " # " "  #" " # " "#  ";
    case '.': return "   " "   " "   " "   " " # ";
    case '-': return "   " "   " "###" "   " "   ";
    case '?': return "###" "  #" " ##" "   " " # ";
    default:  return "   " "   " "   " "   " "   ";
    }
}

static int advance(void) { return cur_font == FontPrimary ? 7 : 5; }

static void px(int x, int y) {
    if(x < 0 || x >= W || y < 0 || y >= H) return;
    fb[y][x] = (cur_color == ColorBlack) ? 1 : 0;
}

void canvas_clear(Canvas* c) { (void)c; memset(fb, 0, sizeof(fb)); }
void canvas_set_color(Canvas* c, Color color) { (void)c; cur_color = color; }
void canvas_set_font(Canvas* c, Font f) { (void)c; cur_font = f; }

void canvas_draw_box(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)c;
    for(int32_t j = 0; j < h; j++)
        for(int32_t i = 0; i < w; i++) px(x + i, y + j);
}

void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) {
    (void)c;
    for(int32_t j = 0; j < h; j++)
        for(int32_t i = 0; i < w; i++) {
            bool corner = (i < r && j < r && i + j < r) ||
                          (i >= w - r && j < r && (w - 1 - i) + j < r) ||
                          (i < r && j >= h - r && i + (h - 1 - j) < r) ||
                          (i >= w - r && j >= h - r && (w - 1 - i) + (h - 1 - j) < r);
            if(!corner) px(x + i, y + j);
        }
}

void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) {
    (void)c;
    for(int32_t i = r; i < w - r; i++) { px(x + i, y); px(x + i, y + h - 1); }
    for(int32_t j = r; j < h - r; j++) { px(x, y + j); px(x + w - 1, y + j); }
}

void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)c;
    int32_t dx = x2 - x1, dy = y2 - y1;
    int32_t steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if(steps == 0) { px(x1, y1); return; }
    for(int32_t i = 0; i <= steps; i++)
        px(x1 + dx * i / steps, y1 + dy * i / steps);
}

void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) { (void)c; px(x, y); }

void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    (void)c;
    /* y is the baseline, as on the Flipper. */
    for(const char* p = s; *p; p++) {
        const char* g = glyph(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
        for(int row = 0; row < 5; row++)
            for(int col = 0; col < 3; col++)
                if(g[row * 3 + col] == '#') px(x + col, y - 4 + row);
        x += advance();
    }
}

uint16_t canvas_string_width(Canvas* c, const char* s) {
    (void)c;
    return (uint16_t)(strlen(s) * advance());
}

void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s) {
    int32_t w = canvas_string_width(c, s);
    if(h == AlignCenter) x -= w / 2;
    else if(h == AlignRight) x -= w;
    if(v == AlignCenter) y += 2;
    else if(v == AlignTop) y += 5;
    canvas_draw_str(c, x, y, s);
}

void screen_dump(const char* title) {
    printf("\n%s\n   +", title);
    for(int x = 0; x < W; x++) printf("-");
    printf("+\n");
    for(int y = 0; y < H; y++) {
        printf("%2d |", y);
        for(int x = 0; x < W; x++) printf("%c", fb[y][x] ? '#' : ' ');
        printf("|\n");
    }
    printf("   +");
    for(int x = 0; x < W; x++) printf("-");
    printf("+\n");
}
