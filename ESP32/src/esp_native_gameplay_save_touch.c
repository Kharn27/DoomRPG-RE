#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_save_ui.h"

static const char* targetName(uint8_t target) {
    return target == ESP_NATIVE_SYS_SAVE ? "SAVE" :
           target == ESP_NATIVE_SYS_LOAD ? "LOAD" : "EXIT";
}

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
        logicalX < ESP_NATIVE_SYS_BUTTON_LEFT ||
        logicalX > ESP_NATIVE_SYS_BUTTON_RIGHT) {
        return 0;
    }
    for (uint8_t row = 0U; row < ESP_NATIVE_SYS_COUNT; ++row) {
        const int top = ESP_NATIVE_SYS_BUTTON_TOP + row *
            (ESP_NATIVE_SYS_BUTTON_HEIGHT + ESP_NATIVE_SYS_BUTTON_GAP);
        const int bottom = top + ESP_NATIVE_SYS_BUTTON_HEIGHT - 1;
        if (logicalY >= top && logicalY <= bottom) {
            *outTarget = row;
            *outTop = (uint8_t)top;
            *outBottom = (uint8_t)bottom;
            return 1;
        }
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
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
        return __real_EspNativeGameplayHubTouchUi_classify(
            logicalX, logicalY, outHitBase);
    }

    if (outHit != NULL &&
        targetForPoint(logicalX, logicalY, &target, &top, &bottom)) {
        memset(outHit, 0, sizeof(*outHit));
        outHit->action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
        outHit->zone = ESP_NATIVE_GAMEPLAY_ZONE_SELECT;
        outHit->left = ESP_NATIVE_SYS_BUTTON_LEFT;
        outHit->top = top;
        outHit->right = ESP_NATIVE_SYS_BUTTON_RIGHT;
        outHit->bottom = bottom;
        printf("[NATIVESAVE] TOUCH-HIT row=%s logical=%d,%d action=SELECT direct=yes\n",
               targetName(target),
               logicalX, logicalY);
        return 1;
    }

    /* Preserve tabs/header Back, but never let blank SYS content synthesize a
     * generic SELECT that could confirm Exit outside its own button. */
    if (logicalY >= 35 && logicalY < 120) return -1;
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
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
        return inputStatus;
    }

    if (outIntent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        !targetForPoint((int)outIntent->logicalX,
                        (int)outIntent->logicalY,
                        &target, &top, &bottom)) {
        return inputStatus;
    }

    {
        const uint8_t current = EspNativeGameplaySave_statusCursor();
        if (current >= ESP_NATIVE_SYS_COUNT) {
            printf("[NATIVESAVE] TOUCH-PRESELECT row=%s cursor=%u direct=blocked reason=invalid-owner\n",
                   targetName(target),
                   (unsigned int)current);
            outIntent->action = ESP_NATIVE_GAMEPLAY_ACTION_NONE;
            outIntent->zone = ESP_NATIVE_GAMEPLAY_ZONE_NONE;
            return inputStatus;
        }
        if (target != current) {
            const uint8_t cursorAction =
                target == (uint8_t)((current + 1U) % ESP_NATIVE_SYS_COUNT)
                    ? ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK
                    : ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD;
            EspNativeGameplayHubStatus hubStatus =
                EspNativeGameplayHub_handleAction(cursorAction);
            const uint8_t after = EspNativeGameplaySave_statusCursor();
            if ((hubStatus != ESP_NATIVE_GAMEPLAY_HUB_REDRAWN &&
                 hubStatus != ESP_NATIVE_GAMEPLAY_HUB_OK) ||
                after != target) {
                printf("[NATIVESAVE] TOUCH-PRESELECT row=%s status=%s cursor=%u->%u direct=blocked\n",
                       targetName(target),
                       EspNativeGameplayHub_statusName(hubStatus),
                       (unsigned int)current, (unsigned int)after);
                outIntent->action = ESP_NATIVE_GAMEPLAY_ACTION_NONE;
                outIntent->zone = ESP_NATIVE_GAMEPLAY_ZONE_NONE;
                return inputStatus;
            }
        }
    }

    printf("[NATIVESAVE] TOUCH-DISPATCH row=%s logical=%u,%u action=SELECT direct=yes\n",
           targetName(target),
           (unsigned int)outIntent->logicalX,
           (unsigned int)outIntent->logicalY);
    return inputStatus;
}
