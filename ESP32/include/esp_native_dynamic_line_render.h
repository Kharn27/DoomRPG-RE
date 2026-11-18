#ifndef DOOMRPG_ESP32_NATIVE_DYNAMIC_LINE_RENDER_H
#define DOOMRPG_ESP32_NATIVE_DYNAMIC_LINE_RENDER_H

#include <stdint.h>
#include "esp_native_gameplay_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct EspPlayerViewState_s;

/* RC10: explicit preview composition owner. Scope dynamic door-line reads
 * around the *unaltered* render-only camera leaf; reject nested scopes and
 * close the scope on both success and failure. No animation frame consumed,
 * gameplay/event mutation, automap or monster activation publication. */
int EspNativeDynamicLineRender_renderVisualPose(
    struct Render_s* render,
    const struct EspPlayerViewState_s* visual,
    uint8_t settledAngle,
    EspNativeGameplayFrameStats* outStats);
int EspNativeDynamicLineRender_renderVisualMove(
    struct Render_s* render,
    const struct EspPlayerViewState_s* before,
    const struct EspPlayerViewState_s* after,
    uint8_t step,
    uint8_t denominator,
    EspNativeGameplayFrameStats* outStats);

#ifdef __cplusplus
}
#endif
#endif
