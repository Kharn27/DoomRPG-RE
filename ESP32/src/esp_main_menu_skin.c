#include "esp_main_menu_skin.h"
#include "native_main_menu_160x120_layout.h"

#include <stddef.h>

/* The four original MENU_MAIN actions use a dedicated procedural plate.
 * Dimensions, hitboxes and logo exclusion belong to the shared layout header.
 * Each write is both framebuffer- and card-clipped; no heap or static state. */
enum {
    SKIN_W = 160, SKIN_H = 120
};

typedef struct {
    uint16_t base, steel, bevel, shadow, groove, copper, glow, ink, typeFace;
} SkinPalette;

/* Clean stencil: idle glyphs are weathered warm steel with a dark engraved
 * offset; armed/confirm glyphs remain exactly their previous bright white.
 * The four uniform copper/orange plate strokes remain unchanged. */
static const SkinPalette palettes[3] = {
    {0x1082U,0x2124U,0x6b4dU,0x0800U,0x2965U,0xd380U,0xa2a1U,0x2104U,0xd6b3U},
    {0x2021U,0x49c3U,0xfba9U,0x1800U,0xb981U,0xfe00U,0xffc0U,0xc180U,0xffffU},
    {0x1800U,0x3962U,0xb2e4U,0x0800U,0x9040U,0xfca0U,0xff80U,0x9800U,0xffffU}
};

/* Five-wide 7-high capitals: 16 glyphs actually used on four cards.
 * 112 immutable bytes instead of an asset, font heap or second framebuffer. */
static const char letters[] = "ABDEGHILMNOPRSTU";
static const uint8_t glyphs[16][7] = {
    {14,17,17,31,17,17,17}, /* A */
    {30,17,17,30,17,17,30}, /* B */
    {30,17,17,17,17,17,30}, /* D */
    {31,16,16,30,16,16,31}, /* E */
    {15,16,16,23,17,17,15}, /* G */
    {17,17,17,31,17,17,17}, /* H */
    {14, 4, 4, 4, 4, 4,14}, /* I */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {17,25,21,19,17,17,17}, /* N */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {30,17,17,30,20,18,17}, /* R */
    {15,16,16,14, 1, 1,30}, /* S */
    {31, 4, 4, 4, 4, 4, 4}, /* T */
    {17,17,17,17,17,17,14}  /* U */
};

static void dot(uint16_t* fb, int stride, int x, int y,
                int l, int t, int r, int b, uint16_t c) {
    if (x >= l && x <= r && y >= t && y <= b &&
        x >= 0 && x < SKIN_W && y >= 0 && y < SKIN_H)
        fb[y * stride + x] = c;
}

static void horizontal(uint16_t* fb, int stride, int x0, int x1, int y,
                       int l, int t, int r, int b, uint16_t c) {
    int x;
    for (x = x0; x <= x1; ++x)
        dot(fb, stride, x, y, l, t, r, b, c);
}

static void vertical(uint16_t* fb, int stride, int x, int y0, int y1,
                     int l, int t, int r, int b, uint16_t c) {
    int y;
    for (y = y0; y <= y1; ++y)
        dot(fb, stride, x, y, l, t, r, b, c);
}

/* Clean 5x7 stencil with a restrained dark, 1px lower-right engraving.
 * No hot copper anywhere inside the glyphs: copper belongs to the frame. */
static void word(uint16_t* fb, int stride, int l, int t, int r, int b,
                 const char* value, int baselineY, const SkinPalette* p) {
    int length = 0, i, row, bit;
    int x0;
    while (value[length] != '\0') ++length;
    x0 = l + ((r - l + 1) - (length * 6 - 1)) / 2;
    for (i = 0; i < length; ++i) {
        int letter = -1, j;
        for (j = 0; j < 16; ++j)
            if (letters[j] == value[i]) { letter = j; break; }
        if (letter < 0) continue;
        for (row = 0; row < 7; ++row)
            for (bit = 0; bit < 5; ++bit)
                if ((glyphs[letter][row] & (uint8_t)(16U >> bit)) != 0U) {
                    const int x = x0 + i * 6 + bit;
                    const int y = baselineY + row;
                    /* CONFIRM lowers text by 1px; prevent its shadow from
                     * punching holes through the continuous bottom copper. */
                    if (y + 1 < b) {
                        dot(fb, stride, x + 1, y + 1,
                            l, t, r, b, p->shadow);
                        dot(fb, stride, x, y + 1,
                            l, t, r, b, p->ink);
                    }
                    dot(fb, stride, x, y, l, t, r, b, p->typeFace);
                }
    }
}

int EspMainMenuSkin_paint(uint16_t* fb, int stride,
                          int width, int height, int item,
                          EspMainMenuSkinState state) {
    int l, t, r, b, x, y;
    const SkinPalette* p;
    const char* first;
    const char* second;

    if (fb == NULL || width != SKIN_W || height != SKIN_H ||
        stride < width || item < 0 ||
        item >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
        (int)state < (int)ESP_MAIN_MENU_SKIN_IDLE ||
        (int)state > (int)ESP_MAIN_MENU_SKIN_CONFIRM) return 0;

    DoomRPG_esp32MainMenuCardRect(item, &l, &t, &r, &b);
    if (l < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT ||
        r >= width || t < 0 || b >= height ||
        r - l + 1 != 54 || b - t + 1 != 20) return 0;

    p = &palettes[(int)state];

    /* Forged outer mounting flange: vertical light falloff and dark,
     * diagonally bitten corners instead of a blue rectangular UI pane. */
    for (y = t; y <= b; ++y)
        for (x = l; x <= r; ++x) {
            const int dx = x - l;
            const int dy = y - t;
            uint16_t color = p->base;
            if (dy < 5) color = p->steel;
            else if (dy > 16) color = p->shadow;
            if ((dx == 0 || dx == 53) && (dy < 2 || dy > 17))
                color = p->shadow;
            if ((dx < 2 || dx > 51) && (dy == 0 || dy == 19))
                color = p->shadow;
            dot(fb, stride, x, y, l, t, r, b, color);
        }

    /* Raised upper/left steel bevel, recessed black lower/right edge. */
    horizontal(fb, stride, l + 3, r - 3, t, l, t, r, b, p->bevel);
    horizontal(fb, stride, l + 2, r - 2, t + 1, l, t, r, b, p->steel);
    vertical(fb, stride, l, t + 3, b - 3, l, t, r, b, p->bevel);
    vertical(fb, stride, l + 1, t + 2, b - 2, l, t, r, b, p->steel);
    horizontal(fb, stride, l + 3, r - 3, b, l, t, r, b, p->shadow);
    horizontal(fb, stride, l + 3, r - 3, b - 1, l, t, r, b, p->groove);
    vertical(fb, stride, r, t + 3, b - 3, l, t, r, b, p->shadow);
    vertical(fb, stride, r - 1, t + 2, b - 2, l, t, r, b, p->groove);
    dot(fb, stride, l + 2, t + 2, l, t, r, b, p->bevel);
    dot(fb, stride, r - 2, t + 2, l, t, r, b, p->groove);
    dot(fb, stride, l + 2, b - 2, l, t, r, b, p->shadow);
    dot(fb, stride, r - 2, b - 2, l, t, r, b, p->copper);

    /* V4: one repeatable industrial circuit on EVERY card.
     * Straight, symmetric channels: same coordinates in Idle/Armed/Confirm.
     * The state changes only RGB565 intensity, never stroke placement.
     * Bottom strip sits on b (below the lower glyph's last pixel), top on
     * t+2 (above the first glyph). No orange strokes behind text. */
    horizontal(fb, stride, l + 7, r - 7, t + 2,
               l, t, r, b, p->copper);
    horizontal(fb, stride, l + 7, r - 7, b,
               l, t, r, b, p->copper);
    vertical(fb, stride, l + 2, t + 6, b - 6,
             l, t, r, b, p->copper);
    vertical(fb, stride, r - 2, t + 6, b - 6,
             l, t, r, b, p->copper);

    /* One recessed rivet at each diagonal corner, away from labels. */
    dot(fb, stride, l + 5, t + 5, l, t, r, b, p->shadow);
    dot(fb, stride, l + 5, t + 4, l, t, r, b, p->bevel);
    dot(fb, stride, r - 5, b - 4, l, t, r, b, p->shadow);
    dot(fb, stride, r - 5, b - 5, l, t, r, b, p->bevel);

    switch (item) {
        case 0: first = "START"; second = "GAME"; break;
        case 1: first = "LOAD"; second = "GAME"; break;
        case 2: first = "OPTIONS"; second = NULL; break;
        default: first = "HELP"; second = "ABOUT"; break;
    }
    word(fb, stride, l, t, r, b, first,
         t + (second != NULL ? 3 : 6) +
         (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p);
    if (second != NULL)
        word(fb, stride, l, t, r, b, second,
             t + 11 + (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p);
    return 1;
}
