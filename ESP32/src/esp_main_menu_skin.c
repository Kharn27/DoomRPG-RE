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
    uint16_t base, high, low, frame, copper, red, text;
} SkinPalette;

/* RGB565: graphite, cool upper bevel, black lower bevel, copper pilot strip.
 * Armed has a glowing red lip; CONFIRM is a pressed, darker permutation. */
static const SkinPalette palettes[3] = {
    {0x10a2U, 0x6b6dU, 0x0841U, 0x31a6U, 0x9b64U, 0x6002U, 0xe73cU},
    {0x2083U, 0xad14U, 0x3001U, 0xf9c3U, 0xfd48U, 0xf1c2U, 0xffdfU},
    {0x1082U, 0x51c9U, 0x0800U, 0xb101U, 0xe3a4U, 0xa082U, 0xff3aU}
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

static void word(uint16_t* fb, int stride, int l, int t, int r, int b,
                 const char* value, int baselineY, uint16_t color) {
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
                if ((glyphs[letter][row] & (uint8_t)(16U >> bit)) != 0U)
                    dot(fb, stride, x0 + i * 6 + bit, baselineY + row,
                        l, t, r, b, color);
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
    for (y = t; y <= b; ++y)
        for (x = l; x <= r; ++x)
            dot(fb, stride, x, y, l, t, r, b, p->base);

    /* Angular double rim, not a rounded glass phone button. */
    horizontal(fb, stride, l + 2, r - 2, t, l, t, r, b, p->high);
    vertical(fb, stride, l, t + 2, b - 2, l, t, r, b, p->high);
    horizontal(fb, stride, l + 2, r - 2, b, l, t, r, b, p->low);
    vertical(fb, stride, r, t + 2, b - 2, l, t, r, b, p->low);

    horizontal(fb, stride, l + 3, r - 3, t + 2, l, t, r, b, p->frame);
    vertical(fb, stride, l + 2, t + 3, b - 3, l, t, r, b, p->frame);
    horizontal(fb, stride, l + 3, r - 3, b - 2, l, t, r, b, p->low);
    vertical(fb, stride, r - 2, t + 3, b - 3, l, t, r, b, p->low);

    /* Brushed steel break lines and a short dim copper pilot lamp. */
    horizontal(fb, stride, l + 10, r - 10, t + 1, l, t, r, b, p->low);
    horizontal(fb, stride, l + 18, r - 10, b - 1, l, t, r, b, p->low);
    horizontal(fb, stride, r - 12, r - 7, t + 3, l, t, r, b, p->copper);
    vertical(fb, stride, l + 3, t + 7, b - 7, l, t, r, b, p->red);

    /* Two small screw heads outside the centered text-safe region. */
    dot(fb, stride, l + 4, t + 4, l, t, r, b, p->low);
    dot(fb, stride, l + 4, t + 3, l, t, r, b, p->high);
    dot(fb, stride, r - 4, b - 4, l, t, r, b, p->low);
    dot(fb, stride, r - 4, b - 5, l, t, r, b, p->copper);

    switch (item) {
        case 0: first = "START"; second = "GAME"; break;
        case 1: first = "LOAD"; second = "GAME"; break;
        case 2: first = "OPTIONS"; second = NULL; break;
        default: first = "HELP"; second = "ABOUT"; break;
    }
    word(fb, stride, l, t, r, b, first,
         t + (second != NULL ? 2 : 6) +
         (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p->text);
    if (second != NULL)
        word(fb, stride, l, t, r, b, second,
             t + 11 + (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p->text);
    return 1;
}
