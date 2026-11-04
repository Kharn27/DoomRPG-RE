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
    uint16_t base, steel, bevel, shadow, groove, copper, ink, typeFace;
} SkinPalette;

/* Compact original Doom-like pixel-metal lettering. The reference is
 * ONE BIG WORD per control (not two tiny lines). The 5x9 masks use 108 bytes
 * of flash, no font atlas, texture, cache, mutable state or heap. */
static const SkinPalette palettes[3] = {
    /*      base    steel   top     dark    groove  copper  ink     face  */
    {0x0800U,0x4a48U,0x838dU,0x0820U,0x3123U,0xd344U,0x2103U,0xe4e9U},
    {0x1000U,0x632bU,0xa4d1U,0x0800U,0x39a5U,0xf447U,0x3001U,0xffffU},
    {0x0800U,0x4248U,0x83ceU,0x0800U,0x2903U,0xdba5U,0x2800U,0xffffU}
};

static const char letters[] = "ADEHILNOPRST";
static const uint8_t glyphs[12][9] = {
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11,0x11,0x11}, /* A */
    {0x1e,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x1e}, /* D */
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10,0x10,0x1f}, /* E */
    {0x11,0x11,0x11,0x1f,0x11,0x11,0x11,0x11,0x11}, /* H */
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x1f}, /* I */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x1f}, /* L */
    {0x11,0x19,0x19,0x15,0x15,0x13,0x13,0x11,0x11}, /* N */
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, /* O */
    {0x1e,0x11,0x11,0x11,0x1e,0x10,0x10,0x10,0x10}, /* P */
    {0x1e,0x11,0x11,0x11,0x1e,0x14,0x12,0x11,0x11}, /* R */
    {0x0f,0x10,0x10,0x0e,0x01,0x01,0x01,0x11,0x1e}, /* S */
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04}  /* T */
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

/* Exactly one word per 54x24 plate. All row-dependent amber tones
 * replace pixels IN the glyph mask, never an offset duplicate/halo.
 * Selected white keeps its engraved dark shadow, but the face stays clean. */
static void word(uint16_t* fb, int stride, int l, int t, int r, int b,
                 const char* value, const SkinPalette* p, int armed) {
    int length = 0, i, row, bit, pass;
    int x0, y0;
    while (value[length] != '\0') ++length;
    x0 = l + ((r - l + 1) - (length * 6 - 1)) / 2;
    y0 = t + 8;
    for (pass = armed ? 0 : 1; pass < 2; ++pass) {
        for (i = 0; i < length; ++i) {
            int letter = -1, j;
            for (j = 0; j < 12; ++j)
                if (letters[j] == value[i]) { letter = j; break; }
            if (letter < 0) continue;
            for (row = 0; row < 9; ++row)
                for (bit = 0; bit < 5; ++bit)
                    if ((glyphs[letter][row] & (uint8_t)(16U >> bit)) != 0U) {
                        const int x = x0 + i * 6 + bit;
                        const int y = y0 + row +
                            (p == &palettes[ESP_MAIN_MENU_SKIN_CONFIRM]);
                        if (pass == 0)
                            dot(fb, stride, x + 1, y + 1,
                                l, t, r, b, p->ink);
                        else
                            dot(fb, stride, x, y, l, t, r, b,
                                armed ? p->typeFace :
                                (row < 3 ? 0xf6afU :
                                 row < 6 ? p->typeFace : 0xaa85U));
                    }
        }
    }
}

/* Shared plate painter: exact menu chassis geometry and colors, extended to
 * bounded larger rectangles. No screen-wide clear or retained state. */
int EspMainMenuSkin_paintPlate(uint16_t* fb, int stride,
                               int width, int height,
                               int l, int t, int r, int b,
                               EspMainMenuSkinState state) {
    int x, y;
    const SkinPalette* p;
    if (fb == NULL || width != SKIN_W || height != SKIN_H ||
        stride < width || l < 0 || t < 0 ||
        r >= width || b >= height || r < l || b < t ||
        r - l + 1 < 16 || b - t + 1 < 16 ||
        (int)state < (int)ESP_MAIN_MENU_SKIN_IDLE ||
        (int)state > (int)ESP_MAIN_MENU_SKIN_CONFIRM) return 0;
    p = &palettes[(int)state];

    /* Gunmetal UAC plate: large bevelled chassis, deep inset black panel.
     * Geometry stays within the existing 54x24 touch rectangle. */
    for (y = t; y <= b; ++y)
        for (x = l; x <= r; ++x) {
            const int dx = x - l;
            const int dy = y - t;
            const int corner = dx + dy < 3 ||
                (r - x) + dy < 3 ||
                dx + (b - y) < 3 ||
                (r - x) + (b - y) < 3;
            uint16_t color = p->base;
            if (dx <= 2 || x >= r - 2 ||
                dy <= 3 || y >= b - 3)
                color = p->steel;
            if (dy <= 1 && dx > 3 && x < r - 3)
                color = p->bevel;
            if (dy >= b - t - 2 || dx == r)
                color = p->shadow;
            if (dx == 3 || x == r - 3 ||
                dy == 4 || y == b - 4)
                color = p->groove;
            if (corner) color = p->shadow;
            dot(fb, stride, x, y, l, t, r, b, color);
        }

    /* Copper rails remain unchanged on MENU_MAIN and the tall loading card.
     * On the 18px loading gauge, the two short vertical end squares looked
     * like empty progress caps. Tint ONLY those seams dark red-bronze. */
    {
        const uint16_t sideSeam = b - t + 1 < 24 ? 0xa1c3U : p->copper;
        horizontal(fb, stride, l + 9, r - 9, t + 3,
                   l, t, r, b, p->copper);
        horizontal(fb, stride, l + 9, r - 9, b - 3,
                   l, t, r, b, p->copper);
        vertical(fb, stride, l + 3, t + 7, b - 7,
                 l, t, r, b, sideSeam);
        vertical(fb, stride, r - 3, t + 7, b - 7,
                 l, t, r, b, sideSeam);
    }

    /* Bolt heads belong to the 24px+ menu/loading cards, not the 18px
     * progress chassis: on a thin gauge they look like two unfilled end caps.
     * Keep every MENU_MAIN pixel exactly as before. */
    if (b - t + 1 >= 24) {
        dot(fb, stride, l + 5, t + 5, l, t, r, b, p->shadow);
        dot(fb, stride, l + 5, t + 6, l, t, r, b, p->bevel);
        dot(fb, stride, r - 5, t + 5, l, t, r, b, p->shadow);
        dot(fb, stride, r - 5, t + 6, l, t, r, b, p->bevel);
        dot(fb, stride, l + 5, b - 5, l, t, r, b, p->shadow);
        dot(fb, stride, r - 5, b - 5, l, t, r, b, p->shadow);
    }

    return 1;
}

int EspMainMenuSkin_paint(uint16_t* fb, int stride,
                          int width, int height, int item,
                          EspMainMenuSkinState state) {
    int l, t, r, b;
    const SkinPalette* p;
    const char* first;

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

    if (!EspMainMenuSkin_paintPlate(fb, stride, width, height,
                                     l, t, r, b, state)) return 0;

    /* The model/action labels are untouched; the industrial UI follows
     * the reference's one-word visual hierarchy. */
    switch (item) {
        case 0: first = "START"; break;
        case 1: first = "LOAD"; break;
        case 2: first = "OPTIONS"; break;
        default: first = "HELP"; break;
    }
    word(fb, stride, l, t, r, b, first, p,
         state != ESP_MAIN_MENU_SKIN_IDLE);
    return 1;
}
