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

/* The only mutable UI-theme state: one byte, never allocated on the heap.
 * Boot's native config loader applies it before the first MENU_MAIN paint. */
static uint8_t selectedLetterTheme = ESP_MAIN_MENU_THEME_CLASSIC;

uint8_t EspMainMenuSkin_getTheme(void) { return selectedLetterTheme; }

int EspMainMenuSkin_setTheme(uint8_t theme) {
    if (theme > ESP_MAIN_MENU_THEME_PHOSPHORE) return 0;
    selectedLetterTheme = theme;
    return 1;
}

uint16_t EspMainMenuSkin_letterTone(int row, int rows) {
    if (rows != 5 && rows != 7 && rows != 9) return 0U;
    if (row < 0 || row >= rows) return 0U;
    if (selectedLetterTheme == ESP_MAIN_MENU_THEME_INFERNO) {
        /* Pale warm straw-yellow -> copper/bronze -> deep Doom red.
         * The previous 0xffe0 was pure saturated yellow. This is softer
         * without changing font masks, shadows or other theme colors. */
        if (row < (rows + 2) / 3) return 0xff75U;
        if (row < (2 * rows + 2) / 3) return 0xd344U;
        return 0xc002U;
    }
    if (selectedLetterTheme == ESP_MAIN_MENU_THEME_PHOSPHORE) {
        /* Pale mint -> acidic phosphor -> deep petrol green (RGB565).
         * Face pixels only, no offset halo, allocations or assets. */
        if (row < (rows + 2) / 3) return 0xdff6U;
        if (row < (2 * rows + 2) / 3) return 0x87e0U;
        return 0x246aU;
    }
    /* Exactly the former untouched MENU_MAIN idle tones. */
    if (row < (rows + 2) / 3) return 0xf6afU;
    if (row < (2 * rows + 2) / 3)
        return palettes[ESP_MAIN_MENU_SKIN_IDLE].typeFace;
    return 0xaa85U;
}

uint16_t EspMainMenuSkin_accentTone(EspMainMenuSkinState state) {
    /* Three ROM-only seam variants per theme: idle, armed, confirmed.
     * Metal, bevel, shadow, ink and hitbox geometry never change. */
    static const uint16_t ember[3] = {0xd1a2U, 0xfb25U, 0xb9a4U};
    static const uint16_t phosphor[3] = {0x5e2aU, 0xa7f1U, 0x3dc9U};
    if ((int)state < (int)ESP_MAIN_MENU_SKIN_IDLE ||
        (int)state > (int)ESP_MAIN_MENU_SKIN_CONFIRM) return 0U;
    if (selectedLetterTheme == ESP_MAIN_MENU_THEME_INFERNO)
        return ember[(int)state];
    if (selectedLetterTheme == ESP_MAIN_MENU_THEME_PHOSPHORE)
        return phosphor[(int)state];
    return palettes[(int)state].copper; /* CLASSIC unchanged, byte-exact */
}

static const char letters[] = "ADEHILNOPRSTBCFGJKMQUVWXYZ0123456789% ";
static const uint8_t glyphs[][9] = {
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
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {30,17,17,17,30,17,17,17,30}, /* B */
    {14,17,16,16,16,16,16,17,14}, /* C */
    {31,16,16,16,30,16,16,16,16}, /* F */
    {14,17,16,16,23,17,17,17,15}, /* G */
    {7,2,2,2,2,2,18,18,12}, /* J */
    {17,18,20,24,24,20,20,18,17}, /* K */
    {17,27,21,21,21,17,17,17,17}, /* M */
    {14,17,17,17,17,21,18,13,1}, /* Q */
    {17,17,17,17,17,17,17,17,14}, /* U */
    {17,17,17,17,17,17,10,10,4}, /* V */
    {17,17,17,17,21,21,21,27,17}, /* W */
    {17,17,10,10,4,10,10,17,17}, /* X */
    {17,17,10,10,4,4,4,4,4}, /* Y */
    {31,1,1,2,4,8,16,16,31}, /* Z */
    {14,17,19,21,21,25,17,17,14}, /* 0 */
    {4,12,4,4,4,4,4,4,14}, /* 1 */
    {14,17,1,1,2,4,8,16,31}, /* 2 */
    {30,1,1,1,14,1,1,1,30}, /* 3 */
    {2,6,10,18,31,2,2,2,2}, /* 4 */
    {31,16,16,16,30,1,1,17,14}, /* 5 */
    {14,17,16,16,30,17,17,17,14}, /* 6 */
    {31,1,1,2,2,4,4,8,8}, /* 7 */
    {14,17,17,17,14,17,17,17,14}, /* 8 */
    {14,17,17,17,15,1,1,17,14}, /* 9 */
    {25,25,2,2,4,8,8,19,19}, /* % */
    {0,0,0,0,0,0,0,0,0} /* space */
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
            for (j = 0; j < (int)(sizeof(glyphs) / sizeof(glyphs[0])); ++j)
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
                                EspMainMenuSkin_letterTone(row, 9));
                    }
        }
    }
}

/* The old seven-row reduction skipped source glyph row 3, deleting
 * the A/E/H crossbars. Always render all original nine rows in submenus.
 * The 3-column compression for long labels keeps every horizontal stroke,
 * with no duplicate pass, halo or clipping outside the 54px plate. */
int EspMainMenuSkin_paintText(uint16_t* fb, int stride, int width, int height,
                              int l, int t, int r, int b,
                              const char* label, int selected, int enabled) {
    int length = 0, compact, advance, glyphWidth, x0, y0, i, row, col;
    if (fb == NULL || label == NULL || width != SKIN_W || height != SKIN_H ||
        stride < width || l < 0 || r >= width || t < 0 || b >= height ||
        r - l + 1 < 16 || b - t + 1 < 16 ||
        (selected != 0 && selected != 1) ||
        (enabled != 0 && enabled != 1)) return 0;
    while (label[length] != '\0') {
        int found = 0;
        for (i = 0; i < (int)(sizeof(glyphs) / sizeof(glyphs[0])); ++i)
            if (letters[i] == label[length]) { found = 1; break; }
        if (!found || ++length > 12) return 0;
    }
    if (length == 0) return 0;
    compact = (length * 6 - 1) > (r - l - 10);
    advance = compact ? 4 : 6;
    glyphWidth = compact ? 3 : 5;
    if (length * advance - 1 > r - l - 8) return 0;
    x0 = (l + r + 1 - (length * advance - 1)) / 2;
    y0 = t + ((b - t + 1) - 9) / 2;
    for (i = 0; i < length; ++i) {
        int glyph = 0;
        while (letters[glyph] != label[i]) ++glyph;
        for (row = 0; row < 9; ++row) {
            const uint8_t mask = glyphs[glyph][row];
            const uint16_t color = !enabled ? 0x9cd3U :
                selected ? 0xffffU : EspMainMenuSkin_letterTone(row, 9);
            for (col = 0; col < glyphWidth; ++col) {
                const int inputCol = compact ? col * 2 : col;
                if (mask & (uint8_t)(16U >> inputCol))
                    dot(fb, stride, x0 + i * advance + col, y0 + row,
                        l, t, r, b, color);
            }
        }
    }
    return 1;
}

/* Shared plate painter: exact menu chassis geometry and colors, extended to
 * bounded larger rectangles. No screen-wide clear or retained state. */
int EspMainMenuSkin_paintPlate(uint16_t* fb, int stride,
                               int width, int height,
                               int l, int t, int r, int b,
                               EspMainMenuSkinState state) {
    int x, y;
    uint16_t seam;
    const SkinPalette* p;
    if (fb == NULL || width != SKIN_W || height != SKIN_H ||
        stride < width || l < 0 || t < 0 ||
        r >= width || b >= height || r < l || b < t ||
        r - l + 1 < 16 || b - t + 1 < 16 ||
        (int)state < (int)ESP_MAIN_MENU_SKIN_IDLE ||
        (int)state > (int)ESP_MAIN_MENU_SKIN_CONFIRM) return 0;
    p = &palettes[(int)state];
    seam = EspMainMenuSkin_accentTone(state);

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

    /* Theme-matched internal engine-light seams. These are the ONLY
     * theme-dependent chassis pixels, including the LOADING title/bar.
     * Width, cap geometry and full-width gauge inset remain unchanged. */
    horizontal(fb, stride, l + 9, r - 9, t + 3,
               l, t, r, b, seam);
    horizontal(fb, stride, l + 9, r - 9, b - 3,
               l, t, r, b, seam);
    vertical(fb, stride, l + 3, t + 7, b - 7,
             l, t, r, b, seam);
    vertical(fb, stride, r - 3, t + 7, b - 7,
             l, t, r, b, seam);

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
