#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_hud_weapon_button.h"

static void check(int x, int y, EspNativeGameplayInputStatus expected,
                  uint8_t action, uint8_t zone) {
    EspNativeGameplayTouchHit hit;
    const EspNativeGameplayInputStatus status =
        EspNativeGameplayInput_classify(x, y, &hit);
    assert(status == expected);
    if (status == ESP_NATIVE_GAMEPLAY_INPUT_OK) {
        assert(hit.action == action && hit.zone == zone);
        assert(x >= hit.left && x <= hit.right);
        assert(y >= hit.top && y <= hit.bottom);
    }
}
int main(void) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState intent;
    EspNativeGameplayInput_reset();
    check(100, 100, ESP_NATIVE_GAMEPLAY_INPUT_OK,
          ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU, ESP_NATIVE_GAMEPLAY_ZONE_WEAPON_MENU);
    check(159, 119, ESP_NATIVE_GAMEPLAY_INPUT_OK,
          ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU, ESP_NATIVE_GAMEPLAY_ZONE_WEAPON_MENU);
    check(99, 110, ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT, 0, 0);
    check(106, 99, ESP_NATIVE_GAMEPLAY_INPUT_OK,
          ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON, ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON);
    check(0, 0, ESP_NATIVE_GAMEPLAY_INPUT_OK,
          ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN, ESP_NATIVE_GAMEPLAY_ZONE_MENU);
    check(128, 0, ESP_NATIVE_GAMEPLAY_INPUT_OK,
          ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP, ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP);
    check(159, 120, ESP_NATIVE_GAMEPLAY_INPUT_INVALID, 0, 0);
    assert(ESP_NATIVE_HUD_WEAPON_LEFT == 100 &&
           ESP_NATIVE_HUD_WEAPON_RIGHT == 159 &&
           ESP_NATIVE_HUD_WEAPON_TOP == 100 &&
           ESP_NATIVE_HUD_WEAPON_BOTTOM == 119);
    assert(EspNativeGameplayInput_classify(140, 110, &hit) ==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(EspNativeGameplayInput_route(&hit, 140, 110) ==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(EspNativeGameplayInput_route(&hit, 140, 110) ==
           ESP_NATIVE_GAMEPLAY_INPUT_BUSY);
    assert(EspNativeGameplayInput_consume(&intent) ==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(intent.action == ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU);
    assert(intent.zone == ESP_NATIVE_GAMEPLAY_ZONE_WEAPON_MENU);
    assert(EspNativeGameplayInput_consume(&intent) ==
           ESP_NATIVE_GAMEPLAY_INPUT_EMPTY);
    puts("PASS HUD weapons touch routing 100..159/100..119, world/top unchanged");
    return 0;
}
