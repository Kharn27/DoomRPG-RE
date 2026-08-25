#include <string.h>

#include "esp_native_gameplay_input.h"
#include "platform_video_config.h"

#define GAMEPLAY_TOP_HUD_HEIGHT 20
#define GAMEPLAY_VIEW_BOTTOM 99
#define GAMEPLAY_MENU_RIGHT 31
#define GAMEPLAY_PASS_TURN_LEFT 32
#define GAMEPLAY_PASS_TURN_RIGHT 127
#define GAMEPLAY_AUTOMAP_LEFT 128

static EspNativeGameplayInputState inputState;

/* Temporary milestone observer. Keeping this as an unresolved weak declaration
 * means the permanent input owner has no extra queue, callback storage or
 * runtime dependency: when no current probe defines the symbol, consume()
 * behaves exactly as before. */
extern void Esp32NativeGameplayInputProbe_observeConsumed(
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

static void setHit(EspNativeGameplayTouchHit* hit,
                   uint8_t action,
                   uint8_t zone,
                   uint8_t left,
                   uint8_t top,
                   uint8_t right,
                   uint8_t bottom) {
    hit->action = action;
    hit->zone = zone;
    hit->left = left;
    hit->top = top;
    hit->right = right;
    hit->bottom = bottom;
}

void EspNativeGameplayInput_reset(void) {
    memset(&inputState, 0, sizeof(inputState));
}

EspNativeGameplayInputStatus EspNativeGameplayInput_classify(
    int logicalX,
    int logicalY,
    EspNativeGameplayTouchHit* outHit) {
    static const uint8_t actions[3][3] = {
        {ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT,
         ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD,
         ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT},
        {ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT,
         ESP_NATIVE_GAMEPLAY_ACTION_SELECT,
         ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT},
        {ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON,
         ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK,
         ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON}
    };
    static const uint8_t zones[3][3] = {
        {ESP_NATIVE_GAMEPLAY_ZONE_MOVE_LEFT,
         ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD,
         ESP_NATIVE_GAMEPLAY_ZONE_MOVE_RIGHT},
        {ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT,
         ESP_NATIVE_GAMEPLAY_ZONE_SELECT,
         ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT},
        {ESP_NATIVE_GAMEPLAY_ZONE_PREV_WEAPON,
         ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK,
         ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON}
    };
    static const uint8_t xLeft[3] = {0, 53, 106};
    static const uint8_t xRight[3] = {52, 105, 159};
    static const uint8_t yTop[3] = {20, 46, 73};
    static const uint8_t yBottom[3] = {45, 72, 99};
    int column;
    int row;

    if (outHit == NULL) return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    memset(outHit, 0, sizeof(*outHit));

    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) {
        return ESP_NATIVE_GAMEPLAY_INPUT_INVALID;
    }

    if (logicalY < GAMEPLAY_TOP_HUD_HEIGHT) {
        if (logicalX <= GAMEPLAY_MENU_RIGHT) {
            setHit(outHit, ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN,
                   ESP_NATIVE_GAMEPLAY_ZONE_MENU,
                   0, 0, GAMEPLAY_MENU_RIGHT, GAMEPLAY_TOP_HUD_HEIGHT - 1);
            return ESP_NATIVE_GAMEPLAY_INPUT_OK;
        }
        if (logicalX >= GAMEPLAY_AUTOMAP_LEFT) {
            setHit(outHit, ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP,
                   ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP,
                   GAMEPLAY_AUTOMAP_LEFT, 0,
                   DOOMRPG_LOGICAL_WIDTH - 1, GAMEPLAY_TOP_HUD_HEIGHT - 1);
            return ESP_NATIVE_GAMEPLAY_INPUT_OK;
        }
        if (logicalX >= GAMEPLAY_PASS_TURN_LEFT &&
            logicalX <= GAMEPLAY_PASS_TURN_RIGHT) {
            setHit(outHit, ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN,
                   ESP_NATIVE_GAMEPLAY_ZONE_PASS_TURN,
                   GAMEPLAY_PASS_TURN_LEFT, 0,
                   GAMEPLAY_PASS_TURN_RIGHT, GAMEPLAY_TOP_HUD_HEIGHT - 1);
            return ESP_NATIVE_GAMEPLAY_INPUT_OK;
        }
        return ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT;
    }

    if (logicalY > GAMEPLAY_VIEW_BOTTOM) {
        return ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT;
    }

    if (logicalX <= xRight[0]) column = 0;
    else if (logicalX <= xRight[1]) column = 1;
    else column = 2;

    if (logicalY <= yBottom[0]) row = 0;
    else if (logicalY <= yBottom[1]) row = 1;
    else row = 2;

    setHit(outHit, actions[row][column], zones[row][column],
           xLeft[column], yTop[row], xRight[column], yBottom[row]);
    return ESP_NATIVE_GAMEPLAY_INPUT_OK;
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
