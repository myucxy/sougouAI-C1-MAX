#include "battery.h"

#define BLACK 0x0000u
#define WHITE 0xffffu
#define RED 0xf800u
#define GREEN 0x07e0u
#define YELLOW 0xffe0u
#define CYAN 0x07ffu

/* Five-bit rows: digits 0..9, dash, percent. Text is scaled 2x. */
static const uint8_t font[12][7] = {
    {14, 17, 17, 17, 17, 17, 14},
    { 4, 12,  4,  4,  4,  4, 14},
    {14, 17,  1,  2,  4,  8, 31},
    {30,  1,  1, 14,  1,  1, 30},
    { 2,  6, 10, 18, 31,  2,  2},
    {31, 16, 16, 30,  1,  1, 30},
    {14, 16, 16, 30, 17, 17, 14},
    {31,  1,  2,  4,  8,  8,  8},
    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15,  1,  1, 14},
    { 0,  0,  0, 14,  0,  0,  0},
    {25, 26,  2,  4,  8, 11, 19}
};

/* Seven-bit rows: bolt, check, plug. Shape and color both identify the state. */
static const uint8_t marks[3][9] = {
    {24, 24, 48, 48, 126, 12, 24, 24, 48},
    { 0,  1,  3,  6, 108, 120, 48,  0,  0},
    {34, 34, 127, 127, 62, 28, 8, 8, 8}
};

static void rectangle(uint16_t *pixels, unsigned x, unsigned y,
                      unsigned width, unsigned height, uint16_t color)
{
    if (x >= AI_BATTERY_WIDTH || y >= AI_BATTERY_HEIGHT) return;
    if (width > AI_BATTERY_WIDTH - x) width = AI_BATTERY_WIDTH - x;
    if (height > AI_BATTERY_HEIGHT - y) height = AI_BATTERY_HEIGHT - y;
    for (unsigned row = y; row < y + height; ++row)
        for (unsigned col = x; col < x + width; ++col)
            pixels[row * AI_BATTERY_WIDTH + col] = color;
}

static void digit(uint16_t *pixels, unsigned x, unsigned glyph)
{
    for (unsigned row = 0; row < 7; ++row)
        for (unsigned col = 0; col < 5; ++col)
            if (font[glyph][row] & (1u << (4u - col)))
                rectangle(pixels, x + col * 2u, 8u + row * 2u, 2, 2, WHITE);
}

static void mark(uint16_t *pixels, unsigned glyph, uint16_t color)
{
    /* A black halo keeps the symbol legible over every fill color and level. */
    for (unsigned row = 0; row < 9; ++row)
        for (unsigned col = 0; col < 7; ++col)
            if (marks[glyph][row] & (1u << (6u - col)))
                rectangle(pixels, 59u + col, 9u + row, 3, 3, BLACK);
    for (unsigned row = 0; row < 9; ++row)
        for (unsigned col = 0; col < 7; ++col)
            if (marks[glyph][row] & (1u << (6u - col)))
                rectangle(pixels, 60u + col, 10u + row, 1, 1, color);
}

void ai_battery_draw(uint16_t *pixels, unsigned percent,
                     enum ai_battery_charge charging)
{
    unsigned glyphs[4], count = 0;
    if (!pixels) return;
    for (unsigned i = 0; i < AI_BATTERY_PIXELS; ++i) pixels[i] = BLACK;

    if (percent > 100u) {
        glyphs[count++] = 10;
        glyphs[count++] = 10;
    } else {
        if (percent == 100u) glyphs[count++] = 1;
        if (percent >= 10u) glyphs[count++] = (percent / 10u) % 10u;
        glyphs[count++] = percent % 10u;
    }
    glyphs[count++] = 11;
    for (unsigned i = 0; i < count; ++i)
        digit(pixels, 50u - count * 12u + i * 12u, glyphs[i]);

    rectangle(pixels, 52, 6, 24, 18, WHITE);
    rectangle(pixels, 54, 8, 20, 14, BLACK);
    rectangle(pixels, 76, 12, 3, 6, WHITE);
    if (percent <= 100u) {
        unsigned fill = percent * 18u / 100u;
        if (percent && !fill) fill = 1;
        rectangle(pixels, 55, 9, fill, 12, percent <= 15u ? RED : GREEN);
    }
    switch (charging) {
    case AI_BATTERY_CHARGING: mark(pixels, 0, YELLOW); break;
    case AI_BATTERY_FULL: mark(pixels, 1, WHITE); break;
    case AI_BATTERY_EXTERNAL_UNKNOWN: mark(pixels, 2, CYAN); break;
    default: break;
    }
}
