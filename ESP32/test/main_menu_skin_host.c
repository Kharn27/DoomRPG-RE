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
    assert(l == 103 && r == 158 && t == 13 + 25 * card &&
           b == 32 + 25 * card);
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
    assert(DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_WIDTH == 56);
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
            /* Exact V4 RGB565 witnesses: steel bevel and continuous copper lip. */
            assert(fb[t * W + l + 10] ==
                   (state == 0 ? 0x6b4dU :
                    state == 1 ? 0xfba9U : 0xb2e4U));
            assert(fb[(b - 1) * W + l + 8] ==
                   (state == 0 ? 0x2965U :
                    state == 1 ? 0xb981U : 0x9040U));
            /* Orange covers all four strokes at an identical RGB565 value.
             * Doom red 0xf102 belongs exclusively to the idle lettering. */
            {
                const uint16_t copper = state == 0 ? 0xd380U :
                                        state == 1 ? 0xfe00U : 0xfca0U;
                for (int x = l + 7; x <= r - 7; ++x) {
                    assert(fb[(t + 2) * W + x] == copper);
                    assert(fb[b * W + x] == copper);
                }
                for (int y = t + 7; y <= b - 7; ++y) {
                    assert(fb[y * W + l + 2] == copper);
                    assert(fb[y * W + r - 2] == copper);
                }
            }
            /* The new 4x6 words are centered inside exactly the same cards:
             * first glyph sample at a known set bit, never a font-bound guess. */
            {
                const int firstX = l + (item == 0 ? 17 :
                                           item == 1 ? 18 :
                                           item == 2 ? 12 : 18);
                const int firstY = t + (item == 2 ? 6 : 3) +
                                   (state == ESP_MAIN_MENU_SKIN_CONFIRM ? 1 : 0);
                assert(fb[firstY * W + firstX] ==
                       (state == ESP_MAIN_MENU_SKIN_IDLE
                            ? 0xe902U : 0xffffU));
            }
            /* Deep-red face with sparse pale red-cream undersides only
             * at exposed bottoms, not a halo. Armed white keeps its own
             * dark engraving and has no cream pixels inside the text. */
            {
                int face = 0, cream = 0, darkDrop = 0;
                int minX = W, maxX = 0;
                const uint16_t faceColor =
                    state == ESP_MAIN_MENU_SKIN_IDLE ? 0xe902U : 0xffffU;
                for (int yy = t + 3; yy <= b - 2; ++yy)
                    for (int xx = l + 6; xx <= r - 6; ++xx) {
                        const uint16_t pixel = fb[yy * W + xx];
                        if (pixel == faceColor) {
                            ++face;
                            if (xx < minX) minX = xx;
                            if (xx > maxX) maxX = xx;
                        }
                        if (pixel == 0xfdd7U) ++cream;
                        if (pixel == 0x5100U) ++darkDrop;
                    }
                assert(face > 20);
                assert(maxX >= minX && maxX - minX < 34);
                if (state == ESP_MAIN_MENU_SKIN_IDLE) {
                    assert(cream > 0);
                    assert(darkDrop > 0);
                } else {
                    assert(cream == 0 && darkDrop == 0);
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
