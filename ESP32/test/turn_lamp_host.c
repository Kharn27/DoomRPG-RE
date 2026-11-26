#include <assert.h>
#include <stdio.h>
#include "esp_turn_lamp.h"
int main(void) {
    EspTurnLampState s = {0};
    assert(EspTurnLamp_update(&s, 100, 0) == 0);
    assert(EspTurnLamp_update(&s, 100, 1) == 1);
    assert(EspTurnLamp_update(&s, 299, 1) == 0);
    EspTurnLamp_replaced(&s);
    assert(EspTurnLamp_update(&s, 299, 1) == 1);
    assert(EspTurnLamp_update(&s, 300, 1) == -1);
    EspTurnLamp_replaced(&s);
    assert(EspTurnLamp_update(&s, 499, 1) == 0);
    assert(EspTurnLamp_update(&s, 500, 1) == 1);
    assert(EspTurnLamp_update(&s, 501, 0) == -1);
    assert(EspTurnLamp_update(&s, 502, 0) == 0);
    assert(EspTurnLamp_update(&s, 503, 1) == 1);
    EspTurnLamp_reset(&s);
    assert(EspTurnLamp_update(&s, UINT32_MAX-100U, 1) == 1);
    assert(EspTurnLamp_update(&s, 99U, 1) == -1);
    assert(EspTurnLamp_update(&s, 299U, 1) == 1);
    assert(EspTurnLamp_update(&s, 300U, 0) == -1);
    puts("PASS turn lamp blink/full present/release/wrap");
    return 0;
}
