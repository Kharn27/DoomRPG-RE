#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_input.h"

#define SAVE_TOUCH_LEFT 91
#define SAVE_TOUCH_RIGHT 158
#define SAVE_TOUCH_TOP 70
#define SAVE_TOUCH_BOTTOM 84
#define LOAD_TOUCH_TOP 85
#define LOAD_TOUCH_BOTTOM 99

#define SAVE_TARGET_SAVE 0U
#define SAVE_TARGET_LOAD 1U

static uint8_t statusTouchCursor = SAVE_TARGET_SAVE;

int __real_EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHit);

EspNativeGameplayInputStatus __real_EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* outIntent);

static int targetForPoint(int logicalX,
                          int logicalY,
                          uint8_t* outTarget,
                          uint8_t* outTop,
                          uint8_t* outBottom) {
    if (outTarget == NULL || outTop == NULL || outBottom == NULL ||
        logicalX < SAVE_TOUCH_LEFT || logicalX > SAVE_TOUCH_RIGHT) {
        return 0;
    }
    if (logicalY >= SAVE_TOUCH_TOP && logicalY <= SAVE_TOUCH_BOTTOM) {
        *outTarget = SAVE_TARGET_SAVE;
        *outTop = SAVE_TOUCH_TOP;
        *outBottom = SAVE_TOUCH_BOTTOM;
        return 1;
    }
    if (logicalY >= LOAD_TOUCH_TOP && logicalY <= LOAD_TOUCH_BOTTOM) {
        *outTarget = SAVE_TARGET_LOAD;
        *outTop = LOAD_TOUCH_TOP;
        *outBottom = LOAD_TOUCH_BOTTOM;
        return 1;
    }
    return 0;
}

int __wrap_EspNativeGameplayHubTouchUi_classify(
    int logicalX,
    int logicalY,
    struct EspNativeGameplayTouchHit_s* outHitBase) {
    const EspNativeGameplayHubView* hub = EspNativeGameplayHub_view();
    EspNativeGameplayTouchHit* outHit =
        (EspNativeGameplayTouchHit*)outHitBase;
    uint8_t target;
    uint8_t top;
    uint8_t bottom;

    if (hub == NULL || hub->active == 0U ||
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
        statusTouchCursor = SAVE_TARGET_SAVE;
        return __real_EspNativeGameplayHubTouchUi_classify(
            logicalX, logicalY, outHitBase);
    }

    if (outHit != NULL &&
        targetForPoint(logicalX, logicalY, &target, &top, &bottom)) {
        memset(outHit, 0, sizeof(*outHit));
        outHit->action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
        outHit->zone = ESP_NATIVE_GAMEPLAY_ZONE_SELECT;
        outHit->left = SAVE_TOUCH_LEFT;
        outHit->top = top;
        outHit->right = SAVE_TOUCH_RIGHT;
        outHit->bottom = bottom;
        printf("[NATIVESAVE] TOUCH-HIT row=%s logical=%d,%d action=SELECT direct=yes\n",
               target == SAVE_TARGET_SAVE ? "SAVE" : "LOAD",
               logicalX, logicalY);
        return 1;
    }

    return __real_EspNativeGameplayHubTouchUi_classify(
        logicalX, logicalY, outHitBase);
}

EspNativeGameplayInputStatus __wrap_EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* outIntent) {
    EspNativeGameplayInputStatus inputStatus =
        __real_EspNativeGameplayInput_consume(outIntent);
    const EspNativeGameplayHubView* hub;
    uint8_t target;
    uint8_t top;
    uint8_t bottom;

    if (inputStatus != ESP_NATIVE_GAMEPLAY_INPUT_OK || outIntent == NULL) {
        return inputStatus;
    }

    hub = EspNativeGameplayHub_view();
    if (hub == NULL || hub->active == 0U ||
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
        statusTouchCursor = SAVE_TARGET_SAVE;
        return inputStatus;
    }

    if (outIntent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        !targetForPoint((int)outIntent->logicalX,
                        (int)outIntent->logicalY,
                        &target, &top, &bottom)) {
        return inputStatus;
    }

    if (target != statusTouchCursor) {
        const uint8_t cursorAction =
            target == SAVE_TARGET_LOAD
                ? ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK
                : ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
        EspNativeGameplayHubStatus hubStatus =
            EspNativeGameplayHub_handleAction(cursorAction);
        if (hubStatus != ESP_NATIVE_GAMEPLAY_HUB_REDRAWN &&
            hubStatus != ESP_NATIVE_GAMEPLAY_HUB_OK) {
            printf("[NATIVESAVE] TOUCH-PRESELECT row=%s status=%s direct=blocked\n",
                   target == SAVE_TARGET_SAVE ? "SAVE" : "LOAD",
                   EspNativeGameplayHub_statusName(hubStatus));
            outIntent->action = ESP_NATIVE_GAMEPLAY_ACTION_NONE;
            outIntent->zone = ESP_NATIVE_GAMEPLAY_ZONE_NONE;
            return inputStatus;
        }
        statusTouchCursor = target;
    }

    printf("[NATIVESAVE] TOUCH-DISPATCH row=%s logical=%u,%u action=SELECT direct=yes\n",
           target == SAVE_TARGET_SAVE ? "SAVE" : "LOAD",
           (unsigned int)outIntent->logicalX,
           (unsigned int)outIntent->logicalY);
    return inputStatus;
}
