/* Real production RGB565 painter, compiled separately and exercised without
 * SDL/ESP32. Guards prove whole-frame memory and non-card pixels untouched. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_main_menu_skin.h"
#include "native_main_menu_160x120_layout.h"

#define W 160
#define H 120
#define GUARD 4
#define SENTINEL 0x5aa5U
static uint16_t arena[GUARD + W * H + GUARD];

static uint32_t cardHash(const uint16_t* frame, int item) {
    uint32_t hash = 2166136261U;
    int l, t, r, b, x, y;
    DoomRPG_esp32IndustrialMainCardRect(item, &l, &t, &r, &b);
    for (y = t; y <= b; ++y) for (x = l; x <= r; ++x) {
        const uint16_t pixel = frame[y * W + x];
        hash = (hash ^ (uint8_t)pixel) * 16777619U;
        hash = (hash ^ (uint8_t)(pixel >> 8)) * 16777619U;
    }
    return hash;
}

static void resetFrame(void) {
    size_t i;
    for (i = 0; i < sizeof(arena) / sizeof(arena[0]); ++i)
        arena[i] = SENTINEL;
}

static void assertOutsideCardUntouched(const uint16_t* fb, int card) {
    int l, t, r, b, x, y;
    int changed = 0;
    DoomRPG_esp32IndustrialMainCardRect(card, &l, &t, &r, &b);
    assert(l == 104 && r == 157 && t == 8 + 27 * card &&
           b == 31 + 27 * card);
    for (x = 0; x < GUARD; ++x) {
        assert(arena[x] == SENTINEL);
        assert(arena[GUARD + W * H + x] == SENTINEL);
    }
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            if (x >= l && x <= r && y >= t && y <= b) {
                if (fb[y * W + x] != SENTINEL) ++changed;
            } else {
                assert(fb[y * W + x] == SENTINEL);
            }
    assert(changed > 400);
    /* The PAK logo stage x=0..101 and rail margins remain byte-exact. */
    assert(fb[0] == SENTINEL);
    assert(fb[100 * W + 101] == SENTINEL);
    assert(fb[19 * W + 102] == SENTINEL);
}


/* Both caps must be genuinely mirrored before any label/gauge overlay.
 * Exercises all live card sizes, the narrow LOADING gauge, all state palettes
 * and all three themes. Full-frame guards also reject any overdraw. */
static void assertPlateCapSymmetry(uint16_t* fb) {
    static const int boxes[][4] = {
        {104, 8, 157, 31},   /* MENU_MAIN */
        {104, 8, 157, 39},   /* OPTIONS */
        {104, 6, 157, 31},   /* VIDEO */
        {104, 6, 157, 26},   /* BRIGHTNESS */
        {16, 26, 143, 69},  /* LOADING title */
        {16, 82, 143, 99},  /* LOADING gauge chassis */
        {34, 102, 111, 118} /* MAIN LOAD BACK: 17px industrial plate */
    };
    static const uint16_t steel[3] = {0x4a48U, 0x632bU, 0x4248U};
    static const uint16_t bevel[3] = {0x838dU, 0xa4d1U, 0x83ceU};
    static const uint16_t shadow[3] = {0x0820U, 0x0800U, 0x0800U};
    static const uint16_t groove[3] = {0x3123U, 0x39a5U, 0x2903U};
    static const uint16_t base[3] = {0x0800U, 0x1000U, 0x0800U};

    for (int theme = 0; theme < 3; ++theme) {
        assert(EspMainMenuSkin_setTheme((uint8_t)theme));
        for (int state = 0; state < 3; ++state) {
            const EspMainMenuSkinState st = (EspMainMenuSkinState)state;
            for (int box = 0; box < (int)(sizeof(boxes)/sizeof(boxes[0])); ++box) {
                const int l = boxes[box][0], t = boxes[box][1];
                const int r = boxes[box][2], b = boxes[box][3];
                resetFrame();
                assert(EspMainMenuSkin_paintPlate(fb, W, W, H,
                                                  l, t, r, b, st));
                for (int y = 0; y < H; ++y) {
                    for (int x = 0; x < W; ++x) {
                        if (x < l || x > r || y < t || y > b) {
                            assert(fb[y*W+x] == SENTINEL);
                        } else {
                            assert(fb[y*W+x] == fb[(t+b-y)*W+x]);
                        }
                    }
                }
                for (int k = 0; k < GUARD; ++k) {
                    assert(arena[k] == SENTINEL);
                    assert(arena[GUARD+W*H+k] == SENTINEL);
                }
                /* Exact paired highlights, recessed inner grooves and
                 * theme seam. No replacement of palette or red gauge ink. */
                for (int cap = 0; cap < 2; ++cap) {
                    const int end = cap ? b : t;
                    const int step = cap ? -1 : 1;
                    assert(fb[end*W+l+10] == bevel[state]);
                    assert(fb[(end+step)*W+l+10] == bevel[state]);
                    assert(fb[(end+2*step)*W+l+10] == steel[state]);
                    assert(fb[(end+3*step)*W+l+10] ==
                           EspMainMenuSkin_accentTone(st));
                    assert(fb[(end+4*step)*W+l+10] == groove[state]);
                    assert(fb[end*W+l] == shadow[state]);
                    assert(fb[end*W+r] == shadow[state]);
                    if (b-t+1 >= 24) {
                        assert(fb[(end+5*step)*W+l+5] == shadow[state]);
                        assert(fb[(end+6*step)*W+l+5] == bevel[state]);
                        assert(fb[(end+5*step)*W+r-5] == shadow[state]);
                        assert(fb[(end+6*step)*W+r-5] == bevel[state]);
                    }
                }
                if (box == 5) {
                    /* The 120x6 LOADING track area is untouched by the
                     * chassis, including the previously unwanted end caps. */
                    for (int y = 88; y <= 93; ++y)
                        for (int x = 20; x <= 139; ++x)
                            assert(fb[y*W+x] == base[state]);
                    assert(fb[87*W+21] == base[state]);
                    assert(fb[94*W+138] == base[state]);
                }
            }
        }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
}


/* The 17px BACK chassis fits a 7-row crisp label wholly in its dark
 * inset: no top/bottom seam can touch the glyph corridor at x54..90,
 * y107..113. This also proves no extra bitmap is needed for the button. */
static void assertMainLoadBackCorridor(uint16_t* fb) {
    for (int theme = 0; theme < 3; ++theme) {
        resetFrame();
        assert(EspMainMenuSkin_setTheme((uint8_t)theme));
        assert(EspMainMenuSkin_paintPlate(fb,W,W,H,34,102,111,118,
                                          ESP_MAIN_MENU_SKIN_IDLE));
        for (int y = 107; y <= 113; ++y)
            for (int x = 54; x <= 90; ++x)
                assert(fb[y*W+x] == 0x0800U);
        for (int i = 0; i < GUARD; ++i) {
            assert(arena[i] == SENTINEL);
            assert(arena[GUARD + W*H + i] == SENTINEL);
        }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
}


/* LOAD GAME and BACK use the actual skin 5x9 face (not a single flat
 * EspMainMenuSkin_uiInk tint). Check every painted pixel, each of the
 * nine gradient rows, full-frame guards and the unchanged inset bounds. */
static void assertLoadTextGradients(uint16_t* fb) {
    static const struct {
        int l, t, r, b;
        const char* label;
    } cases[] = {
        {28, 7, 131, 23, "LOAD GAME"},
        {34, 102, 111, 118, "BACK"}
    };
    for (int theme = 0; theme < 3; ++theme) {
        assert(EspMainMenuSkin_setTheme((uint8_t)theme));
        for (int item = 0; item < 2; ++item) {
            const int l=cases[item].l, t=cases[item].t;
            const int r=cases[item].r, b=cases[item].b;
            const int y0 = t + ((b-t+1)-9)/2;
            resetFrame();
            assert(EspMainMenuSkin_paintText(
                fb,W,W,H,l,t,r,b,cases[item].label,0,1));
            int total = 0;
            for (int y=0;y<H;++y) {
                int rowPixels=0;
                for (int x=0;x<W;++x) {
                    const uint16_t pixel=fb[y*W+x];
                    if (pixel == SENTINEL) continue;
                    assert(x >= l && x <= r);
                    assert(y >= y0 && y < y0 + 9);
                    assert(pixel == EspMainMenuSkin_letterTone(y-y0,9));
                    ++rowPixels;
                    ++total;
                }
                if (y >= y0 && y < y0 + 9)
                    assert(rowPixels > 0);
            }
            assert(total > 25);
            assert(EspMainMenuSkin_letterTone(0,9) !=
                   EspMainMenuSkin_letterTone(8,9));
            for (int k=0;k<GUARD;++k) {
                assert(arena[k] == SENTINEL);
                assert(arena[GUARD+W*H+k] == SENTINEL);
            }
        }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
}

/* Regress the real production nav-key painter against its exact plate
 * background, across both LOAD GAME and HUB slot layouts. All chevrons
 * retain their original 2-pixel silhouette; no writes outside touch bounds. */
static void assertIndustrialNavKeys(uint16_t* fb) {
    static const int regions[][4] = {
        {118, 34, 156, 62},  /* MAIN LOAD UP: 39x29 */
        {118, 69, 156, 98},  /* MAIN LOAD DOWN: 39x30 */
        {118, 49, 156, 78},  /* HUB slot UP: 39x30 */
        {118, 83, 156, 112} /* HUB slot DOWN: 39x30 */
    };
    static uint16_t plate[W * H];
    for (int theme = 0; theme < 3; ++theme) {
        assert(EspMainMenuSkin_setTheme((uint8_t)theme));
        for (int rectangle = 0; rectangle < 4; ++rectangle) {
            const int l = regions[rectangle][0], t = regions[rectangle][1];
            const int r = regions[rectangle][2], b = regions[rectangle][3];
            const int cx = (l + r) / 2, cy = (t + b) / 2;
            resetFrame();
            assert(EspMainMenuSkin_paintPlate(fb,W,W,H,l,t,r,b,
                                              ESP_MAIN_MENU_SKIN_IDLE));
            memcpy(plate, fb, sizeof(plate));
            for (int next = 0; next < 2; ++next) {
                for (int enabled = 0; enabled < 2; ++enabled) {
                    const uint16_t arrow = enabled ?
                        EspMainMenuSkin_uiAccent() : 0x4a48U;
                    resetFrame();
                    assert(EspMainMenuSkin_paintNavKey(fb,W,W,H,l,t,r,b,
                                                      next,enabled));
                    for (int y = 0; y < H; ++y)
                        for (int x = 0; x < W; ++x) {
                            if (x < l || x > r || y < t || y > b) {
                                assert(fb[y*W+x] == SENTINEL);
                                continue;
                            }
                            int chevron = 0;
                            for (int i = 0; i < 7; ++i) {
                                const int yy = next ? cy - 3 + i : cy + 3 - i;
                                const int left = cx - 7 + i;
                                const int right = cx + 7 - i;
                                if ((y == yy || y == yy + 1) &&
                                    ((x >= left && x <= left + 2) ||
                                     (x >= right - 2 && x <= right)))
                                    chevron = 1;
                            }
                            assert(fb[y*W+x] ==
                                   (chevron ? arrow : plate[y*W+x]));
                        }
                    for (int guard = 0; guard < GUARD; ++guard) {
                        assert(arena[guard] == SENTINEL);
                        assert(arena[GUARD + W*H + guard] == SENTINEL);
                    }
                }
            }
        }
    }
    resetFrame();
    assert(!EspMainMenuSkin_paintNavKey(NULL,W,W,H,118,34,156,62,0,1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W-1,W,H,118,34,156,62,0,1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W,W,H,118,34,155,62,0,1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W,W,H,118,34,156,64,0,1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W,W,H,118,34,156,62,2,1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W,W,H,118,34,156,62,0,-1));
    assert(!EspMainMenuSkin_paintNavKey(fb,W,W,H,118,34,160,62,0,1));
    for (int i=0;i<(int)(sizeof(arena)/sizeof(arena[0]));++i)
        assert(arena[i] == SENTINEL);
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
}

int main(void) {
    uint16_t* fb = arena + GUARD;
    uint32_t fingerprints[4][3];
    int item, state;
    assertPlateCapSymmetry(fb);
    assertIndustrialNavKeys(fb);
    assertMainLoadBackCorridor(fb);
    assertLoadTextGradients(fb);
    assert(DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH == 102);
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_HEIGHT == 20);
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_PITCH == 25);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_WIDTH == 54);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT == 24);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_FIRST_TOP == 8);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_PITCH == 27);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(0) == 31);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(1) == 58);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(2) == 85);
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(3) == 112);
    /* Optical centering: exactly 4px at each end of the rail,
     * with three identically spaced 3px gaps between all four cards. */
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(0) -
           DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP == 4);
    assert(DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM -
           DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(3) == 4);
    for (int i = 0; i < 3; ++i)
        assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(i + 1) -
               DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(i) - 1 == 3);
    /* Spacious OPTIONS (three cards) and VIDEO (four) use distinct,
     * paired paint/touch rectangles. No collisions or logo overlap. */
    assert(DOOMRPG_ESP32_OPTIONS_CARD_COUNT == 3);
    assert(DOOMRPG_ESP32_VIDEO_CARD_COUNT == 4);
    for (int layer = 0; layer < 2; ++layer) {
        const int count = layer ? DOOMRPG_ESP32_VIDEO_CARD_COUNT :
                                  DOOMRPG_ESP32_OPTIONS_CARD_COUNT;
        int previousBottom = -1;
        for (int k = 0; k < count; ++k) {
            int l, t, r, b;
            if (layer)
                DoomRPG_esp32VideoCardRect(k, &l, &t, &r, &b);
            else
                DoomRPG_esp32OptionsCardRect(k, &l, &t, &r, &b);
            assert(l == 104 && r == 157 &&
                   b - t + 1 == (layer ? 26 : 32));
            assert(t > DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP);
            assert(b < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM);
            assert(previousBottom < t);
            if (k != 0) assert(t - previousBottom == (layer ? 3 : 5));
            previousBottom = b;
        }
        int firstL, firstT, firstR, firstB;
        int lastL, lastT, lastR, lastB;
        if (layer) {
            DoomRPG_esp32VideoCardRect(0, &firstL, &firstT, &firstR, &firstB);
            DoomRPG_esp32VideoCardRect(count-1, &lastL, &lastT, &lastR, &lastB);
            assert(firstT == 6 && lastT == 90 && lastB == 115);
        } else {
            DoomRPG_esp32OptionsCardRect(0, &firstL, &firstT, &firstR, &firstB);
            DoomRPG_esp32OptionsCardRect(count-1, &lastL, &lastT, &lastR, &lastB);
            assert(firstT == 8 && lastT == 80 && lastB == 111);
        }
        assert(firstL == lastL && firstR == lastR);
    }
    /* Narrow five-preset brightness rail: height raised to 21 without
     * changing the 22px pitch. Three-option choices have 28px height. */
    assert(DOOMRPG_ESP32_BRIGHTNESS_CARD_HEIGHT == 21);
    assert(DOOMRPG_ESP32_BRIGHTNESS_CARD_PITCH == 22);
    {
        int last = -1;
        for (int k = 0; k < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT; ++k) {
            int l,t,r,b;
            DoomRPG_esp32BrightnessCardRect(k,&l,&t,&r,&b);
            assert(l == 104 && r == 157 && b-t+1 == 21);
            assert(t == 6 + 22*k && b == 26+22*k);
            assert(last < t && t > 4 && b < 116);
            last=b;
        }
    }
    /* Historical main-compatibility card layout remains unchanged. */
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT == 104);
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT == 157);
    {
        int ol, ot, oright, ob;
        DoomRPG_esp32MainMenuCardRect(1, &ol, &ot, &oright, &ob);
        assert(ol == 104 && oright == 157 && ot == 38 && ob == 57);
    }
    /* Exact menu and LOADING face bands, no halo or second text pass. */
    {
        static const uint16_t menu[9] = {
            0xff34U, 0xff34U, 0xff34U,
            0xf5aaU, 0xf5aaU, 0xf5aaU,
            0xd225U, 0xd225U, 0xd225U
        };
        static const uint16_t loading[5] = {
            0xff34U, 0xff34U, 0xf5aaU, 0xf5aaU, 0xd225U
        };
        int row;
        for (row = 0; row < 9; ++row)
            assert(EspMainMenuSkin_letterTone(row, 9) == menu[row]);
        for (row = 0; row < 5; ++row)
            assert(EspMainMenuSkin_letterTone(row, 5) == loading[row]);
        assert(EspMainMenuSkin_letterTone(-1, 5) == 0U);
        assert(EspMainMenuSkin_letterTone(5, 5) == 0U);
        assert(EspMainMenuSkin_letterTone(9, 9) == 0U);
        assert(EspMainMenuSkin_letterTone(0, 0) == 0U);
        assert(EspMainMenuSkin_letterTone(0, 6) == 0U);
        assert(EspMainMenuSkin_letterTone(0, 7) == 0xff34U);
        assert(EspMainMenuSkin_letterTone(3, 7) == 0xf5aaU);
        assert(EspMainMenuSkin_letterTone(6, 7) == 0xd225U);
        static const uint16_t classicFill[6] = {
            0xff34U,0xf5aaU,0xf5aaU,0xd225U,0xd225U,0xb225U
        };
        for (int y=0;y<6;++y)
            assert(EspMainMenuSkin_progressTone(y,6) == classicFill[y]);
        assert(EspMainMenuSkin_progressTone(-1,6) == 0U);
        assert(EspMainMenuSkin_progressTone(0,5) == 0U);
        assert(EspMainMenuSkin_progressTone(6,6) == 0U);
    }
    assert(EspMainMenuSkin_getTheme() == ESP_MAIN_MENU_THEME_CLASSIC);
    assert(EspMainMenuSkin_uiAccent() == 0xfd20U);
    assert(EspMainMenuSkin_uiAccentDim() == 0x7a80U);
    assert(EspMainMenuSkin_uiInk() == 0xff34U);
    /* Directional RGB565 proof against the former CLASSIC tones:
     * yellow becomes paler, bronze more golden, airain visibly redder. */
    {
        const uint16_t yellow = EspMainMenuSkin_letterTone(0, 9);
        const uint16_t bronze = EspMainMenuSkin_letterTone(4, 9);
        const uint16_t copper = EspMainMenuSkin_letterTone(8, 9);
        assert((yellow & 31U) > (0xf6afU & 31U));
        assert(((yellow >> 5) & 63U) > ((0xf6afU >> 5) & 63U));
        assert(((bronze >> 5) & 63U) > ((0xe4e9U >> 5) & 63U));
        assert(((copper >> 11) & 31U) > ((0xaa85U >> 11) & 31U));
        assert(((copper >> 5) & 63U) < ((0xaa85U >> 5) & 63U));
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_INFERNO));
    assert(EspMainMenuSkin_uiAccent() == 0xf926U);
    assert(EspMainMenuSkin_uiAccentDim() == 0x9002U);
    /* INFERNO glyph ramp: warm pale red, true middle red, unchanged
     * deep red foot. Modal labels reuse the top; metallic seams stay put. */
    assert(EspMainMenuSkin_uiInk() == 0xfe78U);
    {
        static const uint16_t hot[5] = {0xfe78U,0xfe78U,0xe30cU,0xe30cU,0xc002U};
        static const uint16_t hot7[7] = {
            0xfe78U,0xfe78U,0xfe78U,0xe30cU,0xe30cU,0xc002U,0xc002U
        };
        static const uint16_t hot9[9] = {
            0xfe78U,0xfe78U,0xfe78U,0xe30cU,0xe30cU,0xe30cU,
            0xc002U,0xc002U,0xc002U
        };
        const uint16_t rose = EspMainMenuSkin_letterTone(0, 9);
        const uint16_t middleRed = EspMainMenuSkin_letterTone(4, 9);
        const uint16_t foot = EspMainMenuSkin_letterTone(8, 9);
        const unsigned green = (rose >> 5) & 63U;
        const unsigned blue = rose & 31U;
        assert(((rose >> 11) & 31U) == 31U);
        /* Lighter than the last pale-red candidate, but still red-biased.
         * R stays full, G and B rise and blue stays below half green. */
        assert(green > ((0xfd12U >> 5) & 63U));
        assert(blue > (0xfd12U & 31U));
        assert(blue * 2U < green);
        /* Middle must lie strictly between pale-red top and dark-red foot
         * in every RGB565 channel: unmistakable red, not purple/pink. */
        assert(((middleRed >> 11) & 31U) < ((rose >> 11) & 31U));
        assert(((middleRed >> 11) & 31U) > ((foot >> 11) & 31U));
        assert(((middleRed >> 5) & 63U) < ((rose >> 5) & 63U));
        assert(((middleRed >> 5) & 63U) > ((foot >> 5) & 63U));
        assert((middleRed & 31U) < (rose & 31U));
        assert((middleRed & 31U) > (foot & 31U));
        /* Less blue, more red than the previous raspberry candidate. */
        assert(((middleRed >> 11) & 31U) > ((0xd270U >> 11) & 31U));
        assert((middleRed & 31U) < (0xd270U & 31U));
        /* The lower red band is explicitly untouched. */
        assert(foot == 0xc002U);
        for (int row = 0; row < 5; ++row)
            assert(EspMainMenuSkin_letterTone(row, 5) == hot[row]);
        for (int row = 0; row < 7; ++row)
            assert(EspMainMenuSkin_letterTone(row, 7) == hot7[row]);
        for (int row = 0; row < 9; ++row)
            assert(EspMainMenuSkin_letterTone(row, 9) == hot9[row]);
        static const uint16_t emberFill[6] = {
            0xf946U,0xf905U,0xf804U,0xd004U,0xb003U,0x9002U
        };
        for (int y=0;y<6;++y) {
            const uint16_t tone = EspMainMenuSkin_progressTone(y,6);
            const unsigned red=(tone >> 11) & 31U;
            const unsigned green=(tone >> 5) & 63U;
            const unsigned blue=tone & 31U;
            assert(tone == emberFill[y]);
            /* Even the bright top row must read red, not amber/orange. */
            assert(red >= 18U && red > 2U*green && red > 3U*blue);
        }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_PHOSPHORE));
    assert(EspMainMenuSkin_uiAccent() == 0x87e0U);
    assert(EspMainMenuSkin_uiAccentDim() == 0x246aU);
    assert(EspMainMenuSkin_uiInk() == 0xdff6U);
    {
        static const uint16_t phosphor[9] = {
            0xdff6U,0xdff6U,0xdff6U,
            0x87e0U,0x87e0U,0x87e0U,
            0x246aU,0x246aU,0x246aU
        };
        for (int row = 0; row < 9; ++row)
            assert(EspMainMenuSkin_letterTone(row, 9) == phosphor[row]);
        static const uint16_t greenFill[6] = {
            0xdff6U,0xbff3U,0xa7f1U,0x87e0U,0x5e2aU,0x246aU
        };
        for (int y=0;y<6;++y)
            assert(EspMainMenuSkin_progressTone(y,6) == greenFill[y]);
    }
    assert(!EspMainMenuSkin_setTheme(3U));
    assert(EspMainMenuSkin_getTheme() == ESP_MAIN_MENU_THEME_PHOSPHORE);
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
    for (item = 0; item < 4; ++item) {
        int l, t, r, b;
        DoomRPG_esp32IndustrialMainCardRect(item, &l, &t, &r, &b);
        for (state = 0; state < 3; ++state) {
            uint32_t h;
            resetFrame();
            assert(EspMainMenuSkin_paint(fb, W, W, H, item,
                                        (EspMainMenuSkinState)state));
            assertOutsideCardUntouched(fb, item);
            /* One compact reference-inspired word, not two tiny lines.
             * Pixel counts are the independent masks from the 5x9 font,
             * so unwanted shadows/duplicate faces fail the host gate. */
            {
                static const int facePixels[4] = {88, 77, 130, 72};
                static const int leadingX[4] = {13, 15, 7, 15};
                const uint16_t copper =
                    state == ESP_MAIN_MENU_SKIN_IDLE ? 0xd344U :
                    state == ESP_MAIN_MENU_SKIN_ARMED ? 0xf447U : 0xdba5U;
                int faces = 0;
                int minX = W, maxX = 0, minY = H, maxY = 0;
                const int textY = t + 8 +
                    (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0);
                const uint16_t topColor =
                    state == ESP_MAIN_MENU_SKIN_IDLE ? 0xff34U : 0xffffU;
                assert(fb[t * W + l + 10] ==
                       (state == ESP_MAIN_MENU_SKIN_IDLE ? 0x838dU :
                        state == ESP_MAIN_MENU_SKIN_ARMED ? 0xa4d1U : 0x83ceU));
                assert(fb[(t + 3) * W + l + 10] == copper);
                assert(fb[(b - 3) * W + l + 10] == copper);
                assert(fb[(t + 10) * W + l + 3] == copper);
                assert(fb[(t + 10) * W + r - 3] == copper);
                assert(fb[(t + 18) * W + l + 7] ==
                       (state == ESP_MAIN_MENU_SKIN_ARMED ? 0x1000U : 0x0800U));
                assert(fb[textY * W + l + leadingX[item]] == topColor);
                for (int yy = t + 5; yy <= b - 4; ++yy)
                    for (int xx = l + 6; xx <= r - 6; ++xx) {
                        const uint16_t pixel = fb[yy * W + xx];
                        const int isFace = state == ESP_MAIN_MENU_SKIN_IDLE
                            ? pixel == 0xff34U || pixel == 0xf5aaU ||
                              pixel == 0xd225U
                            : pixel == 0xffffU;
                        if (isFace) {
                            ++faces;
                            if (xx < minX) minX = xx;
                            if (xx > maxX) maxX = xx;
                            if (yy < minY) minY = yy;
                            if (yy > maxY) maxY = yy;
                        }
                    }
                assert(faces == facePixels[item]);
                assert(maxX - minX < 43);
                assert(maxY - minY == 8);
                /* One-pass Idle must have no face-colored pixels outside
                 * the exact glyph masks and no white leakage. */
                if (state == ESP_MAIN_MENU_SKIN_IDLE) {
                    for (int yy = t + 5; yy <= b - 4; ++yy)
                        for (int xx = l + 6; xx <= r - 6; ++xx)
                            assert(fb[yy * W + xx] != 0xffffU);
                }
            }
            h = cardHash(fb, item);
            assert(h != 0);
            fingerprints[item][state] = h;
            /* Same inputs must be deterministic, never accumulate dirt. */
            assert(EspMainMenuSkin_paint(fb, W, W, H, item,
                                        (EspMainMenuSkinState)state));
            assert(cardHash(fb, item) == h);
        }
        assert(fingerprints[item][0] != fingerprints[item][1]);
        assert(fingerprints[item][1] != fingerprints[item][2]);
        assert(fingerprints[item][0] != fingerprints[item][2]);
        /* Every card's center remains its unchanged one-tap hit zone. */
        assert((l + r) / 2 >= l && (l + r) / 2 <= r);
        assert((t + b) / 2 >= t && (t + b) / 2 <= b);
    }

    /* The loading screen uses the exact same idle chassis at 128x44.
     * A larger plate cannot touch the starfield, progress bar or guards. */
    {
        uint32_t firstHash = 2166136261U;
        uint32_t nextHash = 2166136261U;
        int x, y, pass;
        for (pass = 0; pass < 2; ++pass) {
            resetFrame();
            assert(EspMainMenuSkin_paintPlate(fb, W, W, H,
                                               16, 32, 143, 75,
                                               ESP_MAIN_MENU_SKIN_IDLE));
            assert(fb[33 * W + 26] == 0x838dU);
            assert(fb[35 * W + 26] == 0xd344U);
            assert(fb[42 * W + 19] == 0xd344U);
            assert(fb[42 * W + 140] == 0xd344U);
            assert(fb[42 * W + 23] == 0x0800U);
            for (x = 0; x < GUARD; ++x) {
                assert(arena[x] == SENTINEL);
                assert(arena[GUARD + W * H + x] == SENTINEL);
            }
            for (y = 0; y < H; ++y) for (x = 0; x < W; ++x) {
                const uint16_t color = fb[y * W + x];
                uint32_t* hash = pass == 0 ? &firstHash : &nextHash;
                if (x < 16 || x > 143 || y < 32 || y > 75)
                    assert(color == SENTINEL);
                else {
                    *hash = (*hash ^ (uint8_t)color) * 16777619U;
                    *hash = (*hash ^ (uint8_t)(color >> 8)) * 16777619U;
                }
            }
        }
        assert(firstHash == nextHash);
        resetFrame();
        assert(!EspMainMenuSkin_paintPlate(NULL, W, W, H,
                                            16, 32, 143, 75, ESP_MAIN_MENU_SKIN_IDLE));
        assert(!EspMainMenuSkin_paintPlate(fb, W, W, H,
                                            -1, 32, 143, 75, ESP_MAIN_MENU_SKIN_IDLE));
        assert(!EspMainMenuSkin_paintPlate(fb, W, W, H,
                                            16, 32, 160, 75, ESP_MAIN_MENU_SKIN_IDLE));
        assert(!EspMainMenuSkin_paintPlate(fb, W, W, H,
                                            16, 32, 143, 42, ESP_MAIN_MENU_SKIN_IDLE));
        assert(!EspMainMenuSkin_paintPlate(fb, W, W, H,
                                            16, 32, 143, 75, (EspMainMenuSkinState)4));
        for (x = 0; x < (int)(sizeof(arena) / sizeof(arena[0])); ++x)
            assert(arena[x] == SENTINEL);
    }

    /* The companion 128x18 progress chassis uses identical bevel/copper
     * geometry, even at its minimum supported height. It must not touch the
     * raised card, the starfield, or the framebuffer guard cells. */
    {
        int x, y;
        resetFrame();
        assert(EspMainMenuSkin_paintPlate(fb, W, W, H,
                                           16, 82, 143, 99,
                                           ESP_MAIN_MENU_SKIN_IDLE));
        assert(fb[83 * W + 26] == 0x838dU);
        assert(fb[85 * W + 26] == 0xd344U);
        /* Common plate keeps its original copper seams; the transition
         * painter later overlays the full-width inset gauge over them. */
        assert(fb[89 * W + 19] == 0xd344U);
        assert(fb[92 * W + 19] == 0xd344U);
        assert(fb[89 * W + 140] == 0xd344U);
        assert(fb[92 * W + 140] == 0xd344U);
        assert(fb[89 * W + 26] == 0x0800U);
        /* On this thin gauge the apparent two end-cap "balls" are gone.
         * Both formerly bolted sides must be unmarked base gunmetal. */
        assert(fb[87 * W + 21] == 0x0800U);
        assert(fb[88 * W + 21] == 0x0800U);
        assert(fb[94 * W + 21] == 0x0800U);
        assert(fb[87 * W + 138] == 0x0800U);
        assert(fb[88 * W + 138] == 0x0800U);
        assert(fb[94 * W + 138] == 0x0800U);
        for (x = 0; x < GUARD; ++x) {
            assert(arena[x] == SENTINEL);
            assert(arena[GUARD + W * H + x] == SENTINEL);
        }
        for (y = 0; y < H; ++y) for (x = 0; x < W; ++x) {
            if (x >= 16 && x <= 143 && y >= 82 && y <= 99)
                assert(fb[y * W + x] != SENTINEL);
            else
                assert(fb[y * W + x] == SENTINEL);
        }
    }

    /* All three themes use exactly the same single-pass child glyph masks,
     * including the horizontal crossbars, within the same 54px rail. */
    {
        uint32_t originalHash = 2166136261U, infernoHash = 2166136261U;
        const char* labels[] = {"BACK","LUMINOSITE","ORIENTATION",
                                "THEME","SOUND","CLASSIC","INFERNO","PHOSPHORE","100%"};
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
            resetFrame();
            assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
            assert(EspMainMenuSkin_paintPlate(fb, W, W, H, 104, 6, 157, 25,
                                              ESP_MAIN_MENU_SKIN_IDLE));
            assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 25,
                                             labels[i], 0, 1));
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if ((x < 104 || x > 157 || y < 6 || y > 25))
                        assert(fb[y * W + x] == SENTINEL);
            assert(arena[0] == SENTINEL &&
                   arena[GUARD + W * H + GUARD - 1] == SENTINEL);
        }
        resetFrame();
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
        assert(EspMainMenuSkin_paintPlate(fb, W, W, H, 104, 6, 157, 25,
                                          ESP_MAIN_MENU_SKIN_IDLE));
        assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 25,
                                         "INFERNO", 0, 1));
        for (int y = 6; y <= 25; ++y)
            for (int x = 104; x <= 157; ++x) {
                const uint16_t c = fb[y * W + x];
                originalHash = (originalHash ^ (uint8_t)c) * 16777619U;
                originalHash = (originalHash ^ (uint8_t)(c >> 8)) * 16777619U;
            }
        resetFrame();
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_INFERNO));
        assert(EspMainMenuSkin_paintPlate(fb, W, W, H, 104, 6, 157, 25,
                                          ESP_MAIN_MENU_SKIN_IDLE));
        assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 25,
                                         "INFERNO", 0, 1));
        for (int y = 6; y <= 25; ++y)
            for (int x = 104; x <= 157; ++x) {
                const uint16_t c = fb[y * W + x];
                infernoHash = (infernoHash ^ (uint8_t)c) * 16777619U;
                infernoHash = (infernoHash ^ (uint8_t)(c >> 8)) * 16777619U;
            }
        assert(originalHash != infernoHash);
        assert(!EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 25,
                                          "UNKNOWN?", 0, 1));
        assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 25,
                                         "BACK", 1, 1));
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
    }

    /* The old seven-row downsampling removed source row 3. Prove every
     * crossbar of A, E and H is present in the 9-row child glyph render. */
    for (int letter = 0; letter < 3; ++letter) {
        const char* label = letter == 0 ? "A" : (letter == 1 ? "E" : "H");
        const int x0 = (104 + 157 + 1 - 5) / 2;
        const int y0 = 8 + (32 - 9) / 2;
        const int barLen = letter == 1 ? 4 : 5;
        resetFrame();
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
        assert(EspMainMenuSkin_paintPlate(fb, W, W, H, 104, 8, 157, 39,
                                          ESP_MAIN_MENU_SKIN_IDLE));
        assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 8, 157, 39,
                                         label, 0, 1));
        for (int col = 0; col < barLen; ++col)
            assert(fb[(y0+3)*W+x0+col] == EspMainMenuSkin_letterTone(3,9));
    }
    /* Long names retain three crossbar pixels even with compressed columns. */
    {
        const int x0 = (104 + 157 + 1 - (9 * 4 - 1)) / 2;
        const int y0 = 6 + (26 - 9) / 2;
        resetFrame();
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_PHOSPHORE));
        assert(EspMainMenuSkin_paintText(fb, W, W, H, 104, 6, 157, 31,
                                         "PHOSPHORE", 0, 1));
        for (int col = 0; col < 3; ++col)
            assert(fb[(y0+3)*W+x0+4+col] == EspMainMenuSkin_letterTone(3,9));
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
    }
    /* Strict theme-accent isolation across main, nested menus and both
     * LOADING plates. For each plate geometry and state, only the four exact
     * seam segments may differ from CLASSIC. No extra assets/heap in runtime. */
    {
        static uint16_t classicPlate[W * H];
        static const int regions[6][4] = {
            {104, 8, 157, 31},   /* MENU_MAIN 54x24 */
            {104, 8, 157, 39},   /* OPTIONS 54x32 */
            {104, 6, 157, 31},   /* VIDEO/PHOSPHORE 54x26 */
            {104, 6, 157, 26},   /* BRIGHTNESS 54x21 */
            {16, 26, 143, 69},   /* LOADING title 128x44 */
            {16, 82, 143, 99}    /* LOADING progress rim 128x18 */
        };
        static const uint16_t seamColors[3][3] = {
            {0xd344U, 0xf447U, 0xdba5U},
            {0xd1a2U, 0xfb25U, 0xb9a4U},
            {0x5e2aU, 0xa7f1U, 0x3dc9U}
        };
        assert(EspMainMenuSkin_accentTone((EspMainMenuSkinState)-1) == 0U);
        assert(EspMainMenuSkin_accentTone((EspMainMenuSkinState)3) == 0U);
        for (int box = 0; box < 6; ++box) {
            const int l = regions[box][0], t = regions[box][1];
            const int r = regions[box][2], b = regions[box][3];
            for (int stateIndex = 0; stateIndex < 3; ++stateIndex) {
                const EspMainMenuSkinState st =
                    (EspMainMenuSkinState)stateIndex;
                resetFrame();
                assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
                assert(EspMainMenuSkin_accentTone(st) ==
                       seamColors[0][stateIndex]);
                assert(EspMainMenuSkin_paintPlate(fb, W, W, H,
                                                  l, t, r, b, st));
                memcpy(classicPlate, fb, sizeof(classicPlate));
                for (int theme = 1; theme <= 2; ++theme) {
                    int seamPixels = 0;
                    resetFrame();
                    assert(EspMainMenuSkin_setTheme((uint8_t)theme));
                    assert(EspMainMenuSkin_accentTone(st) ==
                           seamColors[theme][stateIndex]);
                    assert(EspMainMenuSkin_paintPlate(fb, W, W, H,
                                                      l, t, r, b, st));
                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            const int isHorizontal =
                                x >= l + 9 && x <= r - 9 &&
                                (y == t + 3 || y == b - 3);
                            const int isVertical =
                                (x == l + 3 || x == r - 3) &&
                                y >= t + 7 && y <= b - 7;
                            const int seam = isHorizontal || isVertical;
                            if (seam) {
                                ++seamPixels;
                                assert(fb[y * W + x] ==
                                       seamColors[theme][stateIndex]);
                                assert(classicPlate[y * W + x] ==
                                       seamColors[0][stateIndex]);
                            } else {
                                /* Preserve exact steel/base/bevel/shadow/groove,
                                 * framebuffer sentinels and left logo. */
                                assert(fb[y * W + x] ==
                                       classicPlate[y * W + x]);
                            }
                        }
                    }
                    assert(seamPixels > 0);
                    for (int guard = 0; guard < GUARD; ++guard) {
                        assert(arena[guard] == SENTINEL);
                        assert(arena[GUARD + W * H + guard] == SENTINEL);
                    }
                }
            }
        }
        assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
    }

    /* Real shared title-stencil painter: neither the longest 5x7 report
     * header (MISSION COMPLETE spans x34..128 at y11..17) nor its
     * y20..26 subtitle can touch a steel rim, hatch or corner bolt. */
    for (int theme = 0; theme < 3; ++theme) {
        resetFrame();
        assert(EspMainMenuSkin_setTheme((uint8_t)theme));
        assert(!EspMainMenuSkin_paintModalTitleStencil(NULL,W,W,H));
        assert(!EspMainMenuSkin_paintModalTitleStencil(fb,W-1,W,H));
        assert(!EspMainMenuSkin_paintModalTitleStencil(fb,W,W-1,H));
        assert(!EspMainMenuSkin_paintModalTitleStencil(fb,W,W,H-1));
        for (int p = 0; p < W * H; ++p) assert(fb[p] == SENTINEL);
        assert(EspMainMenuSkin_paintModalTitleStencil(fb,W,W,H));
        assert(fb[7*W+9] == 0x6b4dU);
        assert(fb[7*W+150] == 0x6b4dU);
        assert(fb[27*W+47] == EspMainMenuSkin_uiAccentDim());
        assert(fb[27*W+80] == EspMainMenuSkin_uiAccent());
        assert(fb[11*W+11] != SENTINEL);
        assert(fb[11*W+147] != SENTINEL);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (y < 7 || y > 27 || x < 9 || x > 150 ||
                    (y >= 10 && y <= 26 && x >= 26 && x <= 133))
                    assert(fb[y*W+x] == SENTINEL);
            }
        }
        for (int guard=0;guard<GUARD;++guard) {
            assert(arena[guard]==SENTINEL);
            assert(arena[GUARD+W*H+guard]==SENTINEL);
        }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));

    resetFrame();
    assert(!EspMainMenuSkin_paint(NULL, W, W, H, 0, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W - 1, W, H, 0, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W, W - 1, H, 0, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W, W, H - 1, 0, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W, W, H, -1, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W, W, H, 4, ESP_MAIN_MENU_SKIN_IDLE));
    assert(!EspMainMenuSkin_paint(fb, W, W, H, 0, (EspMainMenuSkinState)-1));
    assert(!EspMainMenuSkin_paint(fb, W, W, H, 0, (EspMainMenuSkinState)3));
    for (item = 0; item < (int)(sizeof(arena) / sizeof(arena[0])); ++item)
        assert(arena[item] == SENTINEL);

    puts("[MENUSKIN-HOST] PASS four action cards, 3 states, deterministic pixels, "
         "frame guards, logo preservation, invalid-input fail-closed");
    return 0;
}
