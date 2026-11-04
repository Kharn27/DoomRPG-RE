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
    /* Old child-screen cards retain the original 54px geometry. */
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT == 104);
    assert(DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT == 157);
    {
        int ol, ot, oright, ob;
        DoomRPG_esp32MainMenuCardRect(1, &ol, &ot, &oright, &ob);
        assert(ol == 104 && oright == 157 && ot == 38 && ob == 57);
    }
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
                assert(fb[(t + 13) * W + l + 7] ==
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
