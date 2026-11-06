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

int main(void) {
    uint16_t* fb = arena + GUARD;
    uint32_t fingerprints[4][3];
    int item, state;
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
                   b - t + 1 == (layer ? 24 : 28));
            assert(t > DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP);
            assert(b < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM);
            assert(previousBottom < t);
            if (k != 0) assert(t - previousBottom == (layer ? 4 : 7));
            previousBottom = b;
        }
        int firstL, firstT, firstR, firstB;
        int lastL, lastT, lastR, lastB;
        if (layer) {
            DoomRPG_esp32VideoCardRect(0, &firstL, &firstT, &firstR, &firstB);
            DoomRPG_esp32VideoCardRect(count-1, &lastL, &lastT, &lastR, &lastB);
            assert(firstT == 8 && lastT == 89 && lastB == 112);
        } else {
            DoomRPG_esp32OptionsCardRect(0, &firstL, &firstT, &firstR, &firstB);
            DoomRPG_esp32OptionsCardRect(count-1, &lastL, &lastT, &lastR, &lastB);
            assert(firstT == 12 && lastT == 80 && lastB == 107);
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
            0xf6afU, 0xf6afU, 0xf6afU,
            0xe4e9U, 0xe4e9U, 0xe4e9U,
            0xaa85U, 0xaa85U, 0xaa85U
        };
        static const uint16_t loading[5] = {
            0xf6afU, 0xf6afU, 0xe4e9U, 0xe4e9U, 0xaa85U
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
        assert(EspMainMenuSkin_letterTone(0, 7) == 0xf6afU);
        assert(EspMainMenuSkin_letterTone(3, 7) == 0xe4e9U);
        assert(EspMainMenuSkin_letterTone(6, 7) == 0xaa85U);
    }
    assert(EspMainMenuSkin_getTheme() == ESP_MAIN_MENU_THEME_CLASSIC);
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_INFERNO));
    {
        static const uint16_t hot[5] = {0xff75U,0xff75U,0xd344U,0xd344U,0xc002U};
        for (int row = 0; row < 5; ++row)
            assert(EspMainMenuSkin_letterTone(row, 5) == hot[row]);
    }
    assert(!EspMainMenuSkin_setTheme(2U));
    assert(EspMainMenuSkin_getTheme() == ESP_MAIN_MENU_THEME_INFERNO);
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
                    state == ESP_MAIN_MENU_SKIN_IDLE ? 0xf6afU : 0xffffU;
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
                            ? pixel == 0xf6afU || pixel == 0xe4e9U ||
                              pixel == 0xaa85U
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

    /* Exact child geometry: two themes paint the same glyph positions,
     * only the face colors differ. All 54x20 cards stay rail-clipped. */
    {
        uint32_t originalHash = 2166136261U, infernoHash = 2166136261U;
        const char* labels[] = {"BACK","LUMINOSITE","ORIENTATION",
                                "THEME","SOUND","CLASSIC","INFERNO","100%"};
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
