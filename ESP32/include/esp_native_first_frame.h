#ifndef DOOMRPG_ESP32_NATIVE_FIRST_FRAME_H
#define DOOMRPG_ESP32_NATIVE_FIRST_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct EspPlayerViewState_s;

typedef enum EspNativeFirstFrameStatus_e {
    ESP_NATIVE_FIRST_FRAME_INVALID = 0,
    ESP_NATIVE_FIRST_FRAME_NOT_READY = 1,
    ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE = 2,
    ESP_NATIVE_FIRST_FRAME_PACK_BUSY = 3,
    ESP_NATIVE_FIRST_FRAME_SOURCE_INVALID = 4,
    ESP_NATIVE_FIRST_FRAME_UNSUPPORTED_WORLD = 5,
    ESP_NATIVE_FIRST_FRAME_RENDER_FAILED = 6,
    ESP_NATIVE_FIRST_FRAME_PRESENT_FAILED = 7,
    ESP_NATIVE_FIRST_FRAME_OK = 8
} EspNativeFirstFrameStatus;

typedef struct EspNativeFirstFrameState_s {
    uint32_t frameBeforeFNV;
    uint32_t frameAfterFNV;
    uint32_t lineCandidates;
    uint32_t leafNodes;
    uint32_t wallRequests;
    uint32_t wallDraws;
    uint32_t spanCalls;
    uint32_t pixelsDrawn;
    uint32_t cacheHits;
    uint32_t cacheMisses;
    uint16_t ceilingRgb565;
    uint16_t floorRgb565;
    uint8_t targetMapId;
    uint8_t rendered;
    uint8_t presented;
    uint8_t active;
} EspNativeFirstFrameState;

void EspNativeFirstFrame_reset(void);
int EspNativeFirstFrame_isReady(void);
const EspNativeFirstFrameState* EspNativeFirstFrame_view(void);

/*
 * Render and present exactly one deterministic native Junction walls-only
 * gameplay frame from the compact EspMapRuntime and native graphics catalog.
 *
 * The function deliberately does not consume input, advance a turn, activate
 * legacy entities/monsters, call DoomCanvas_playingState()/Render_render(), or
 * retain texel payloads. Floor/ceiling textures, sprites and HUD painting stay
 * outside this first visual milestone.
 */
EspNativeFirstFrameStatus EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView);

/*
 * Gameplay-only world route over the same hardware-proven BSP/wall/plane path.
 * It writes only render->screenX/Y/Width/Height pixels, never clears pixels
 * outside that logical viewport, never presents, and never mutates the global
 * historical first-frame state.  The caller receives the local render witness
 * in outState and owns the later sprite/HUD/final-present composition.
 */
EspNativeFirstFrameStatus EspNativeFirstFrame_renderGameplayViewport(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView,
    EspNativeFirstFrameState* outState);

#ifdef __cplusplus
}
#endif

#endif
