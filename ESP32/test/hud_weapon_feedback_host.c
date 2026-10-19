/* Run the real bounded neon-feedback writer for the lower HUD weapon button.
 * No mock of the glyph, edit journal or revert logic: only framebuffer/clock. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_hud_weapon_button.h"
#include "platform_video_c_bridge.h"

static uint16_t framebuffer[160U * 120U];
static uint16_t baseline[160U * 120U];
static uint32_t currentMs;
static unsigned int presents;
void* Esp32PlatformVideo_framebuffer(void) { return framebuffer; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return sizeof(framebuffer);
}
int Esp32PlatformVideo_present(void) { ++presents; return 1; }
int64_t esp_timer_get_time(void) { return (int64_t)currentMs * 1000LL; }

int main(void) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayControlsStats painted, restored;
    size_t i;
    memset(&hit, 0, sizeof(hit));
    for (i = 0U; i < sizeof(framebuffer)/sizeof(framebuffer[0]); ++i) {
        framebuffer[i] = (uint16_t)(i * 73U + 103U);
    }
    memcpy(baseline, framebuffer, sizeof(baseline));
    hit.action = ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU;
    hit.zone = ESP_NATIVE_GAMEPLAY_ZONE_WEAPON_MENU;
    hit.left = ESP_NATIVE_HUD_WEAPON_LEFT;
    hit.top = ESP_NATIVE_HUD_WEAPON_TOP;
    hit.right = ESP_NATIVE_HUD_WEAPON_RIGHT;
    hit.bottom = ESP_NATIVE_HUD_WEAPON_BOTTOM;
    currentMs = 1000U;
    EspNativeGameplayControls_reset();
    assert(EspNativeGameplayControls_begin(&hit, &painted));
    assert(EspNativeGameplayControls_isActive());
    assert(painted.action == ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU);
    assert(painted.zone == ESP_NATIVE_GAMEPLAY_ZONE_WEAPON_MENU);
    assert(painted.edits > 300U);
    assert(painted.edits <= ESP_NATIVE_GAMEPLAY_FEEDBACK_MAX_EDITS);
    assert(painted.baselineFNV != 0U &&
           painted.overlayFNV != painted.baselineFNV);
    assert(memcmp(framebuffer, baseline, sizeof(framebuffer)) != 0);
    assert(framebuffer[108U * 160U + 129U] != baseline[108U * 160U + 129U]);
    assert(!EspNativeGameplayControls_isExpired());
    currentMs += ESP_NATIVE_GAMEPLAY_FEEDBACK_MS;
    assert(EspNativeGameplayControls_isExpired());
    assert(EspNativeGameplayControls_restore(1, &restored));
    assert(!EspNativeGameplayControls_isActive());
    assert(restored.conflicts == 0U);
    assert(restored.restoredFNV == painted.baselineFNV);
    assert(memcmp(framebuffer, baseline, sizeof(framebuffer)) == 0);
    assert(presents == 1U);
    puts("PASS HUD WEAPON_MENU yellow glyph + bounded edits, 120ms exact feedback rollback");
    return 0;
}
