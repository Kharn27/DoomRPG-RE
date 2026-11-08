/* Tiny real-view HELP contract: no PAK world, no retained graphics owner.
 * Compile alongside the production scene painter with plain host C. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#define W DOOMRPG_LOGICAL_WIDTH
#define H DOOMRPG_LOGICAL_HEIGHT
static uint16_t frame[W * H];
static uint16_t source[W * H];

static uint16_t sampleSource(int lx, int ly) {
    const int cacheX = lx * 128 / 144;
    const int cacheY = ly * 64 / 68;
    const int originalX = cacheX * 160 / 128;
    const int originalY = 20 + cacheY * 80 / 64;
    return source[originalY * W + originalX];
}
int main(void) {
    int x, y, previous = -1;
    unsigned int mapColor;
    uint16_t expected;

    assert(EspNativeHelpScene_breathStep(0) == 0);
    assert(EspNativeHelpScene_breathStep(2560) == 0);
    assert(EspNativeHelpScene_breathLevel(0) == 0);
    assert(EspNativeHelpScene_breathLevel(16) == 16);
    assert(EspNativeHelpScene_breathLevel(31) == 1);
    for (x = 0; x <= 16; ++x) {
        const int power = EspNativeHelpScene_breathPower((uint8_t)x);
        assert(power >= previous && power - previous < 26);
        previous = power;
    }
    assert(previous == 255);
    assert(EspNativeHelpScene_breathPower(17) == 0);
    assert(!EspNativeHelpScene_isReady());
    assert(!EspNativeHelpScene_hasGameplaySnapshot());

    EspNativeHelpScene_begin(1);
    assert(EspNativeHelpScene_isReady());
    assert(!EspNativeHelpScene_hasGameplaySnapshot());
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x) frame[y * W + x] = 0x1830;
    /* Cold boot: the screenshot area MUST remain untouched by a fake scene
     * at breath zero. Button captions may modify the weapon cells only. */
    assert(EspNativeHelpScene_paint(frame, 0));
    assert(frame[35 * W + 76] == 0x1830);
    assert(!EspNativeHelpScene_paintRow(frame, 3, 0));
    assert(!EspNativeHelpScene_paint(frame, 17));

    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x) {
            source[y * W + x] =
                (uint16_t)((x * 113U + y * 271U) & 0xffffU);
            frame[y * W + x] = source[y * W + x];
        }
    assert(EspNativeHelpScene_captureGameplayViewport(frame, 1));
    assert(EspNativeHelpScene_hasGameplaySnapshot());
    assert(EspNativeHelpScene_snapshotMapId() == 1);
    expected = sampleSource(72, 24);
    memset(frame, 0, sizeof(frame));
    assert(EspNativeHelpScene_paint(frame, 0));
    /* Mid-top cell: no neon at step zero, so exact RGB565 pixel survives. */
    assert(frame[(29 + 24) * W + (8 + 72)] == expected);

    /* Non-matching map: never invent content from a different level. */
    EspNativeHelpScene_begin(2);
    assert(!EspNativeHelpScene_hasGameplaySnapshot());
    assert(EspNativeHelpScene_snapshotMapId() == 0);
    mapColor = frame[35 * W + 76] = 0x2755U;
    assert(EspNativeHelpScene_paint(frame, 0));
    assert(frame[35 * W + 76] == mapColor);

    /* Once per map: subsequent turn renders do not allocate/copy again. */
    memset(frame, 0, sizeof(frame));
    assert(EspNativeHelpScene_captureGameplayViewport(frame, 1));
    EspNativeHelpScene_begin(1);
    assert(EspNativeHelpScene_paint(frame, 0));
    assert(frame[(29 + 24) * W + (8 + 72)] == expected);

    /* Reset closes only the HELP display lease; picture survives, and a
     * different map may take over this bounded snapshot on next present. */
    EspNativeHelpScene_reset();
    assert(!EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_captureGameplayViewport(source, 2));
    EspNativeHelpScene_begin(2);
    assert(EspNativeHelpScene_hasGameplaySnapshot());
    assert(EspNativeHelpScene_snapshotMapId() == 2);
    assert(EspNativeHelpScene_paintRow(frame, 2, 16));
    assert(EspNativeHelpScene_paintRow(frame, 0, 0));
    puts("PASS native help: real 128x64 viewport, 32-step smooth breath, cold-boot fallback");
    return 0;
}
