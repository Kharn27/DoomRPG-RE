/* Own actual native automap overlay pixels/phase logic. No world/PAK or
 * substitute combat state; external map runtime functions are dead-stripped. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/esp_native_gameplay_automap.c"

static uint32_t fakeClock;
unsigned int DoomRPG_GetUpTimeMS(void) { return fakeClock; }

static uint16_t pixels[160U * 120U + 2U];

static int hasGlyphColor(void) {
    for (unsigned y = 1U; y < 12U; ++y)
        for (unsigned x = 0U; x < 160U; ++x)
            if (pixels[1U + y * 160U + x] == 0xffe0U) return 1;
    return 0;
}

static void renderSimpleOverlay(void) {
    const uint8_t phase = damagePhase(fakeClock);
    uint16_t writes = 0;
    memset(pixels, 0, sizeof(pixels));
    pixels[0] = pixels[160U * 120U + 1U] = 0xdeadU;
    fillRect(&pixels[1], 0, 0, 160, 120,
             phase == 0U || phase == 2U ? AUTOMAP_DAMAGE_RED : AUTOMAP_BLACK,
             &writes);
    if (phase != 4U) paintDamageHeader(&pixels[1], &writes);
    assert(writes < UINT16_MAX);
    assert(pixels[0] == 0xdeadU && pixels[160U * 120U + 1U] == 0xdeadU);
}

int main(void) {
    fakeClock = 1000U;
    EspNativeGameplayAutomap_cancelDamage();
    EspNativeGameplayAutomap_noteDamage(0U, 0U);
    assert(!EspNativeGameplayAutomap_damageRedrawDue());

    EspNativeGameplayAutomap_noteDamage(5U, 0U);
    assert(mapDamage.active && mapDamage.total == 5U &&
           damagePhase(fakeClock) == 0U &&
           EspNativeGameplayAutomap_damageRedrawDue());
    assert(damageGlyph('D') && damageGlyph('A') && damageGlyph('M') &&
           damageGlyph('G') && damageGlyph('E') && damageGlyph('!'));
    renderSimpleOverlay();
    assert(pixels[1U + 119U * 160U] == AUTOMAP_DAMAGE_RED);
    assert(hasGlyphColor());
    mapDamage.renderedPhase = 0U;
    assert(!EspNativeGameplayAutomap_damageRedrawDue());

    fakeClock = 1120U;
    assert(damagePhase(fakeClock) == 1U &&
           EspNativeGameplayAutomap_damageRedrawDue());
    renderSimpleOverlay();
    assert(pixels[1U + 119U * 160U] == AUTOMAP_BLACK && hasGlyphColor());
    mapDamage.renderedPhase = 1U;

    fakeClock = 1220U;
    assert(damagePhase(fakeClock) == 2U &&
           EspNativeGameplayAutomap_damageRedrawDue());
    renderSimpleOverlay();
    assert(pixels[1U + 119U * 160U] == AUTOMAP_DAMAGE_RED && hasGlyphColor());
    mapDamage.renderedPhase = 2U;

    fakeClock = 1350U;
    assert(damagePhase(fakeClock) == 3U &&
           EspNativeGameplayAutomap_damageRedrawDue());
    renderSimpleOverlay();
    assert(pixels[1U + 119U * 160U] == AUTOMAP_BLACK && hasGlyphColor());
    mapDamage.renderedPhase = 3U;

    fakeClock = 1900U;
    assert(damagePhase(fakeClock) == 4U &&
           EspNativeGameplayAutomap_damageRedrawDue());
    renderSimpleOverlay();
    assert(!hasGlyphColor() && pixels[1U + 119U * 160U] == AUTOMAP_BLACK);

    EspNativeGameplayAutomap_cancelDamage();
    assert(!EspNativeGameplayAutomap_damageRedrawDue());
    fakeClock = UINT32_MAX - 30U;
    EspNativeGameplayAutomap_noteDamage(12U, 1U);
    assert(damageGlyph('C') && damageGlyph('R') && damageGlyph('I') &&
           damageGlyph('T'));
    renderSimpleOverlay();
    assert(hasGlyphColor());
    fakeClock += 220U; /* wrap */
    assert(damagePhase(fakeClock) == 2U);
    renderSimpleOverlay();
    assert(pixels[1U + 119U * 160U] == AUTOMAP_DAMAGE_RED);
    EspNativeGameplayAutomap_cancelDamage();
    assert(!EspNativeGameplayAutomap_damageRedrawDue());
    puts("automap_damage_host: PASS 2 red background pulses, DAMAGE/CRIT amount, exact blank expiry, 32-bit wrap, no framebuffer bounds escape");
    return 0;
}
