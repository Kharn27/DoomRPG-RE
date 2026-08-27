#include <string.h>

#include "esp_native_gameplay_input.h"
#include "platform_video_config.h"

#define GAMEPLAY_ZONE_COUNT 12U

static EspNativeGameplayInputState inputState;

/* The one permanent native CYD gameplay geometry. Hit-testing and the visible
 * control compositor both enumerate this exact table so touch semantics cannot
 * drift away from what is drawn on screen. Bottom HUD y=100..119 deliberately
 * has no gameplay hit zone. */
static const EspNativeGameplayTouchHit gameplayZones[GAMEPLAY_ZONE_COUNT] = {
    {ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN,   ESP_NATIVE_GAMEPLAY_ZONE_MENU,        0,   0,  31, 19},
    {ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN,   ESP_NATIVE_GAMEPLAY_ZONE_PASS_TURN,  32,   0, 127, 19},
    {ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP,     ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP,    128,  0, 159, 19},
    {ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT,   ESP_NATIVE_GAMEPLAY_ZONE_MOVE_LEFT,   0,  20,  52, 45},
    {ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD,ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD,53, 20, 105, 45},
    {ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT,  ESP_NATIVE_GAMEPLAY_ZONE_MOVE_RIGHT,106, 20, 159, 45},
    {ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT,   ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT,   0,  46,  52, 72},
    {ESP_NATIVE_GAMEPLAY_ACTION_SELECT,      ESP_NATIVE_GAMEPLAY_ZONE_SELECT,     53,  46, 105, 72},
    {ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT,  ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT,106, 46, 159, 72},
    {ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_PREV_WEAPON, 0,  73,  52, 99},
    {ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK,   ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK,  53,  73, 105, 99},
    {ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON,106, 73, 159, 99}
};

/* Temporary milestone observer. Keeping this as an unresolved weak declaration
 * means the permanent input owner has no extra queue, callback storage or
 * runtime dependency: when no current probe defines the symbol, consume()
 * behaves exactly as before. */
extern void Esp32NativeGameplayInputProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) __attribute__((weak));

/* Read-only SELECT milestone observer. This second weak hook deliberately does
 * not replace the existing TURN/MOVE observer: current probes can inspect a
 * consumed SELECT without adding state, callbacks or dependencies to the
 * permanent input owner. */
extern void Esp32NativeGameplaySelectProbe_observeConsumed(
    const EspNativeGameplayInputState* intent) __attribute__((weak));

static int supportedAction(uint8_t action) {
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN:
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON:
    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON:
    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:
        return 1;
    default:
        return 0;
    }
}

void EspNativeGameplayInput_reset(void) {
    memset(&inputState, 0, sizeof(inputState));
}

uint8_t EspNativeGameplayInput_zoneCount(void) {
    return GAMEPLAY_ZONE_COUNT;
}

EspNativeGameplayInputStatus EspNativeGameplayInput_zoneAt(
    uint8_t ordinal,
    EspNativeGameplayTouchHit* outHit) {
    if (outHit == NULL || ordinal >= GAMEPLAY_ZONE_COUNT) {
        if (outHit != NULL) memset(outHit, 0, sizeof(*outHit));
        return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    }
    *outHit = gameplayZones[ordinal];
    return ESP_NATIVE_GAMEPLAY_INPUT_OK;
}

EspNativeGameplayInputStatus EspNativeGameplayInput_classify(
    int logicalX,
    int logicalY,
    EspNativeGameplayTouchHit* outHit) {
    uint8_t i;

    if (outHit == NULL) return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    memset(outHit, 0, sizeof(*outHit));

    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) {
        return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    }

    for (i = 0U; i < GAMEPLAY_ZONE_COUNT; ++i) {
        const EspNativeGameplayTouchHit* hit = &gameplayZones[i];
        if (logicalX >= hit->left && logicalX <= hit->right &&
            logicalY >= hit->top && logicalY <= hit->bottom) {
            *outHit = *hit;
            return ESP_NATIVE_GAMEPLAY_INPUT_OK;
        }
    }
    return ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT;
}

EspNativeGameplayInputStatus EspNativeGameplayInput_route(
    const EspNativeGameplayTouchHit* hit,
    int logicalX,
    int logicalY) {
    if (hit == NULL || !supportedAction(hit->action) ||
        hit->zone == ESP_NATIVE_GAMEPLAY_ZONE_NONE ||
        logicalX < hit->left || logicalX > hit->right ||
        logicalY < hit->top || logicalY > hit->bottom ||
        logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) {
        return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    }
    if (inputState.pending) return ESP_NATIVE_GAMEPLAY_INPUT_BUSY;

    inputState.action = hit->action;
    inputState.zone = hit->zone;
    inputState.logicalX = (uint8_t)logicalX;
    inputState.logicalY = (uint8_t)logicalY;
    ++inputState.sequence;
    if (inputState.sequence == 0U) ++inputState.sequence;
    inputState.pending = 1U;
    inputState.active = 1U;
    inputState.reserved[0] = 0U;
    inputState.reserved[1] = 0U;
    return ESP_NATIVE_GAMEPLAY_INPUT_OK;
}

const EspNativeGameplayInputState* EspNativeGameplayInput_peek(void) {
    return &inputState;
}

EspNativeGameplayInputStatus EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* outIntent) {
    if (outIntent == NULL) return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    memset(outIntent, 0, sizeof(*outIntent));
    if (!inputState.pending) return ESP_NATIVE_GAMEPLAY_INPUT_EMPTY;
    *outIntent = inputState;
    inputState.pending = 0U;
    if (Esp32NativeGameplayInputProbe_observeConsumed != NULL) {
        Esp32NativeGameplayInputProbe_observeConsumed(outIntent);
    }
    if (Esp32NativeGameplaySelectProbe_observeConsumed != NULL) {
        Esp32NativeGameplaySelectProbe_observeConsumed(outIntent);
    }
    return ESP_NATIVE_GAMEPLAY_INPUT_OK;
}

const char* EspNativeGameplayInput_actionName(uint8_t action) {
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD: return "FORWARD";
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK: return "BACK";
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT: return "TURN_LEFT";
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT: return "TURN_RIGHT";
    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN: return "MENU";
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT: return "SELECT";
    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP: return "AUTOMAP";
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT: return "STRAFE_LEFT";
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT: return "STRAFE_RIGHT";
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON: return "PREV_WEAPON";
    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON: return "NEXT_WEAPON";
    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN: return "PASS_TURN";
    default: return "NONE";
    }
}
