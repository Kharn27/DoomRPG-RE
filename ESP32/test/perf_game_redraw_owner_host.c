/* Compile the actual device mutation sites. Only map input and the audit
 * event sink are mocked; read-only/no-op/invalid calls must emit nothing. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_map_automap_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_redraw_audit.h"

static uint8_t arenaByte;
static EspMapRuntimeView runtime = {
    .arena = &arenaByte, .arenaBytes = 1U, .arenaFNV1a = 123U,
    .lineCount = 8U, .mapSpriteCount = 8U
};
static unsigned reveals, visited, activations;
void EspRedrawAudit_event(uint16_t kinds) {
    switch (kinds) {
    case ESP_REDRAW_REVEAL: ++reveals; break;
    case ESP_REDRAW_VISITED: ++visited; break;
    case ESP_REDRAW_ACTIVATION: ++activations; break;
    default: assert(0);
    }
}
void* heap_caps_malloc(size_t bytes, unsigned int caps) { (void)caps; return malloc(bytes); }
void heap_caps_free(void* ptr) { free(ptr); }
const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
int EspMapRuntime_getBlockCell(uint32_t i, uint8_t* out) {
    assert(i < 1024U); *out = 0U; return 1;
}
int EspMapRuntime_getLine(uint32_t i, EspMapLine* out) {
    assert(i < 8U); memset(out,0,sizeof(*out)); return 1;
}
int EspMapRuntime_getMapSprite(uint32_t i, EspMapSprite* out) {
    assert(i < 8U); memset(out,0,sizeof(*out)); return 1;
}
int EspMapRuntime_getEvent(uint32_t i, uint32_t* out) {
    (void)i; (void)out; assert(0); return 0;
}
int main(void) {
    uint8_t value;
    EspMapAutomapSnapshot map;
    EspNativeGameplayMonsterActivationSnapshot activation;
    assert(EspMapState_buildFromRuntime() && EspMapAutomapState_buildFromRuntime());
    assert(visited == 1U && reveals == 1U);
    assert(EspMapAutomapState_snapshot(&map));
    const uint32_t initial = EspMapAutomapState_view()->stateFNV1a;
    const uint32_t tiles = EspMapState_view()->stateFNV1a;
    assert(EspMapAutomapState_setLineRevealed(3,0));
    assert(EspMapAutomapState_getLineRevealed(3,&value) && !value);
    assert(!EspMapAutomapState_setLineRevealed(8,1));
    assert(!EspMapAutomapState_setSpriteRevealed(2,2));
    assert(EspMapState_setVisited(42,0));
    assert(!EspMapState_setVisited(1024,1));
    assert(reveals == 1U && visited == 1U);
    assert(EspMapAutomapState_setLineRevealed(3,1));
    assert(EspMapAutomapState_setLineRevealed(3,0));
    assert(EspMapAutomapState_setSpriteRevealed(7,1));
    assert(EspMapAutomapState_setSpriteRevealed(7,0));
    assert(EspMapState_setVisited(42,1) && EspMapState_setVisited(42,0));
    assert(reveals == 5U && visited == 3U);
    assert(EspMapAutomapState_view()->stateFNV1a == initial);
    assert(EspMapState_view()->stateFNV1a == tiles);
    assert(EspMapAutomapState_restore(&map)); /* identical restore is conservative */
    assert(reveals == 6U && visited == 3U);
    map.sourceArenaFNV1a = 999U;
    assert(!EspMapAutomapState_restore(&map));
    assert(reveals == 6U && visited == 3U);
    EspNativeGameplayMonsterActivation_reset();
    assert(activations == 1U);
    assert(EspNativeGameplayMonsterActivation_observeVisible(3,1,42));
    assert(activations == 3U); /* arena reset + first activation */
    assert(EspNativeGameplayMonsterActivation_observeVisible(3,1,43));
    assert(!EspNativeGameplayMonsterActivation_observeVisible(1024,1,42));
    assert(activations == 3U);
    assert(EspNativeGameplayMonsterActivation_snapshot(&activation));
    assert(EspNativeGameplayMonsterActivation_restoreSnapshot(&activation));
    assert(activations == 4U);
    activation.sourceArenaFNV1a = 999U;
    assert(!EspNativeGameplayMonsterActivation_restoreSnapshot(&activation));
    assert(activations == 4U);
    EspNativeGameplayMonsterActivation_reset();
    EspMapAutomapState_reset();
    EspMapState_reset();
    assert(activations == 5U && reveals == 7U && visited == 4U);
    return 0;
}
