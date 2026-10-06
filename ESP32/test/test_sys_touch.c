#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_save_ui.h"

int __wrap_EspNativeGameplayHubTouchUi_classify(
    int x, int y, EspNativeGameplayTouchHit* hit);
EspNativeGameplayInputStatus __wrap_EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* intent);

static EspNativeGameplayHubView hub;
static EspNativeGameplayInputState pending;
static uint8_t cursor;
static int navigationCalls;
static int navigationFails;
static EspNativeGameplayInputStatus consumeStatus;

const EspNativeGameplayHubView* EspNativeGameplayHub_view(void) { return &hub; }
uint8_t EspNativeGameplaySave_statusCursor(void) { return cursor; }
const char* EspNativeGameplayHub_statusName(EspNativeGameplayHubStatus status) {
    (void)status;
    return "mock";
}
EspNativeGameplayHubStatus EspNativeGameplayHub_handleAction(uint8_t action) {
    ++navigationCalls;
    if (navigationFails) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    assert(action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK ||
           action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD);
    cursor = (uint8_t)((cursor + (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK
                                 ? 1 : ESP_NATIVE_SYS_COUNT - 1)) %
                      ESP_NATIVE_SYS_COUNT);
    return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
}
int __real_EspNativeGameplayHubTouchUi_classify(
    int x, int y, EspNativeGameplayTouchHit* hit) {
    (void)x; (void)y; (void)hit;
    return 0; /* Distinct fallback witness: actual header/tab logic owns these. */
}
EspNativeGameplayInputStatus __real_EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* intent) {
    if (intent != NULL) *intent = pending;
    return consumeStatus;
}

static int rowTop(int row) {
    return ESP_NATIVE_SYS_BUTTON_TOP + row *
        (ESP_NATIVE_SYS_BUTTON_HEIGHT + ESP_NATIVE_SYS_BUTTON_GAP);
}

int main(void) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState intent;
    hub.active = 1;
    hub.page = ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM;

    /* Every logical content pixel must belong to exactly its painted card or
     * be rejected, especially gaps/margins while Exit is armed. */
    for (int y = 35; y < 120; ++y) {
        for (int x = 0; x < 160; ++x) {
            int expectedRow = -1;
            for (int row = 0; row < ESP_NATIVE_SYS_COUNT; ++row) {
                if (x >= ESP_NATIVE_SYS_BUTTON_LEFT &&
                    x <= ESP_NATIVE_SYS_BUTTON_RIGHT && y >= rowTop(row) &&
                    y < rowTop(row) + ESP_NATIVE_SYS_BUTTON_HEIGHT)
                    expectedRow = row;
            }
            const int result = __wrap_EspNativeGameplayHubTouchUi_classify(x, y, &hit);
            assert(result == (expectedRow < 0 ? -1 : 1));
            if (expectedRow >= 0) {
                assert(hit.action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT);
                assert(hit.top == rowTop(expectedRow));
                assert(hit.bottom == rowTop(expectedRow) + ESP_NATIVE_SYS_BUTTON_HEIGHT - 1);
            }
        }
    }
    assert(__wrap_EspNativeGameplayHubTouchUi_classify(10, 10, &hit) == 0);
    assert(__wrap_EspNativeGameplayHubTouchUi_classify(80, 25, &hit) == 0);

    consumeStatus = ESP_NATIVE_GAMEPLAY_INPUT_OK;
    pending.action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
    pending.zone = ESP_NATIVE_GAMEPLAY_ZONE_SELECT;
    pending.logicalX = 80;
    /* All nine cursor/target pairs: row changes cancel the old confirmation
     * through the normal owner, then preserve SELECT for the new target. */
    for (int from = 0; from < ESP_NATIVE_SYS_COUNT; ++from) {
        for (int to = 0; to < ESP_NATIVE_SYS_COUNT; ++to) {
            cursor = (uint8_t)from;
            pending.logicalY = (uint8_t)(rowTop(to) + 10);
            navigationCalls = 0;
            assert(__wrap_EspNativeGameplayInput_consume(&intent) == ESP_NATIVE_GAMEPLAY_INPUT_OK);
            assert(cursor == to);
            assert(navigationCalls == (from == to ? 0 : 1));
            assert(intent.action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT);
        }
    }
    cursor = ESP_NATIVE_SYS_COUNT;
    assert(__wrap_EspNativeGameplayInput_consume(&intent) == ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(intent.action == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    cursor = ESP_NATIVE_SYS_SAVE;
    navigationFails = 1;
    assert(__wrap_EspNativeGameplayInput_consume(&intent) == ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(intent.action == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    assert(cursor == ESP_NATIVE_SYS_SAVE);
    consumeStatus = ESP_NATIVE_GAMEPLAY_INPUT_EMPTY;
    navigationCalls = 0;
    assert(__wrap_EspNativeGameplayInput_consume(&intent) == ESP_NATIVE_GAMEPLAY_INPUT_EMPTY);
    assert(navigationCalls == 0);
    hub.page = ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS;
    assert(__wrap_EspNativeGameplayHubTouchUi_classify(80, rowTop(2), &hit) == 0);
    hub.page = ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM;
    hub.active = 0;
    assert(__wrap_EspNativeGameplayHubTouchUi_classify(80, rowTop(2), &hit) == 0);
    puts("SYS touch: PASS (all card pixels, margins, nine routes, fail-closed)");
    return 0;
}
