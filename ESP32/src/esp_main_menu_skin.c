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

/* Doom-gold 4x6 Idle glyphs: only a *sparse*, near-black lower-right
 * cast on terminal edges. Do not shift a full duplicate of the glyph:
 * that produces the ghost/double lettering seen on the physical CYD.
 * Armed/Confirm keep their original pure-white face and dark engraving.
 * Plate/rail/geometry/touch are unchanged. */
static const SkinPalette palettes[3] = {
    {0x1082U,0x2124U,0x6b4dU,0x0800U,0x2965U,0xd380U,0xa2a1U,0x2104U,0xff29U},
    {0x2021U,0x49c3U,0xfba9U,0x1800U,0xb981U,0xfe00U,0xffc0U,0xc180U,0xffffU},
    {0x1800U,0x3962U,0xb2e4U,0x0800U,0x9040U,0xfca0U,0xff80U,0x9800U,0xffffU}
};

/* Custom readable 4x6 stencil. Each pixel is 2x2 on the physical CYD.
 * Only the 16 uppercase glyphs used by MENU_MAIN: 96 bytes of ROM,
 * no generated assets, font cache, heap or second framebuffer. */
static const char letters[] = "ABDEGHILMNOPRSTU";
static const uint8_t glyphs[16][6] = {
    { 6, 9,15, 9, 9, 9}, /* A */
    {14, 9,14, 9, 9,14}, /* B */
    {14, 9, 9, 9, 9,14}, /* D */
    {15, 8,14, 8, 8,15}, /* E */
    { 7, 8,11, 9, 9, 7}, /* G */
    { 9, 9,15, 9, 9, 9}, /* H */
    {15, 6, 6, 6, 6,15}, /* I */
    { 8, 8, 8, 8, 8,15}, /* L */
    { 9,15,15, 9, 9, 9}, /* M */
    { 9,13,11, 9, 9, 9}, /* N */
    { 6, 9, 9, 9, 9, 6}, /* O */
    {14, 9,14, 8, 8, 8}, /* P */
    {14, 9,14,10, 9, 9}, /* R */
    { 7, 8, 6, 1, 1,14}, /* S */
    {15, 6, 6, 6, 6, 6}, /* T */
    { 9, 9, 9, 9, 9, 6}  /* U */
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

/* Shadows first, faces last. Idle: near-black underhang, only from
 * bottom-row pixels and exposed rightmost stroke termini. Never offset
 * the entire 4x6 glyph, since that reads as a second letter at x2.
 * Armed/Confirm retain BOTH original shadow passes byte-for-byte. */
static void word(uint16_t* fb, int stride, int l, int t, int r, int b,
                 const char* value, int baselineY, const SkinPalette* p) {
    int length = 0, pass, i, row, bit;
    int x0;
    const int idle = p == &palettes[ESP_MAIN_MENU_SKIN_IDLE];
    const uint16_t drop = idle ? 0x2800U : p->shadow;
    while (value[length] != '\0') ++length;
    x0 = l + ((r - l + 1) - (length * 5 - 1)) / 2;
    for (pass = 0; pass < 3; ++pass) {
        for (i = 0; i < length; ++i) {
            int letter = -1, j;
            for (j = 0; j < 16; ++j)
                if (letters[j] == value[i]) { letter = j; break; }
            if (letter < 0) continue;
            for (row = 0; row < 6; ++row)
                for (bit = 0; bit < 4; ++bit)
                    if ((glyphs[letter][row] & (uint8_t)(8U >> bit)) != 0U) {
                        const int x = x0 + i * 5 + bit;
                        const int y = baselineY + row;
                        if (pass == 2) {
                            dot(fb, stride, x, y, l, t, r, b, p->typeFace);
                        } else if (y + 1 < b) {
                            if (pass == 0) {
                                /* Only tiny bottom/right terminals cast:
                                 * not a full shifted 4x6 duplicate.
                                 * Row 5 gives a 1px underhang; an exposed
                                 * right edge gives a restrained side notch.
                                 * White states keep their old full shadow. */
                                const uint8_t mask = (uint8_t)(8U >> bit);
                                const int rightEdge =
                                    bit == 3 ||
                                    (glyphs[letter][row] & (mask >> 1)) == 0U;
                                const int downEdge =
                                    row == 5 ||
                                    (glyphs[letter][row + 1] & mask) == 0U;
                                if (!idle || (rightEdge && downEdge))
                                    dot(fb, stride, x + 1, y + 1,
                                        l, t, r, b, drop);
                            } else if (!idle)
                                /* White selected text stays pixel-identical. */
                                dot(fb, stride, x, y + 1,
                                    l, t, r, b, p->ink);
                        }
                    }
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

    DoomRPG_esp32IndustrialMainCardRect(item, &l, &t, &r, &b);
    if (l < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT ||
        r >= width || t < 0 || b >= height ||
        r - l + 1 != DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_WIDTH ||
        b - t + 1 != DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT) return 0;

    p = &palettes[(int)state];

    /* Forged outer mounting flange: vertical light falloff and dark,
     * diagonally bitten corners instead of a blue rectangular UI pane. */
    for (y = t; y <= b; ++y)
        for (x = l; x <= r; ++x) {
            const int dx = x - l;
            const int dy = y - t;
            uint16_t color = p->base;
            if (dy < 5) color = p->steel;
            else if (dy > b - t - 3) color = p->shadow;
            if ((dx == 0 || dx == r - l) && (dy < 2 || dy > b - t - 2))
                color = p->shadow;
            if ((dx < 2 || dx > r - l - 2) && (dy == 0 || dy == b - t))
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
         t + (second != NULL ? 5 : 9) +
         (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p);
    if (second != NULL)
        word(fb, stride, l, t, r, b, second,
             t + 13 + (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0), p);
    return 1;
}
