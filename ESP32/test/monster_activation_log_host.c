/* Compile the real activation/capture seam at INFO and TRACE. Lower position
 * and activation owners are mocked; admission/capture results must not change
 * when successful diagnostic output is removed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_monster_movement_activation.h"

static int active, prepared, prepares, captures;
static EspNativeGameplayMonsterPositionRecord before, after;

int EspNativeGameplayMonsterActivation_isActive(uint16_t sprite) {
    assert(sprite == 96U); return active;
}
int EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t sprite, int32_t dx, int32_t dy,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter) {
    assert(sprite == 96U && dx == 0 && dy == 64);
    ++prepares;
    if (!prepared) return 0;
    *outBefore = (EspNativeGameplayMonsterPositionRecord){96,168,544,352};
    *outAfter = (EspNativeGameplayMonsterPositionRecord){96,200,544,416};
    return 1;
}
void EspNativeGameplayMonsterMovementPublish_capturePrepared(
    const EspNativeGameplayMonsterPositionRecord* inBefore,
    const EspNativeGameplayMonsterPositionRecord* inAfter) {
    ++captures; before = *inBefore; after = *inAfter;
}
int main(void) {
    EspNativeGameplayMonsterPositionRecord outBefore, outAfter;
    memset(&outBefore, 0, sizeof(outBefore));
    memset(&outAfter, 0, sizeof(outAfter));
    assert(!EspNativeGameplayMonsterMovementActivation_prepareCardinalMove(
        96,0,64,&outBefore,&outAfter));
    assert(prepares == 0 && captures == 0);
    active = 1;
    assert(!EspNativeGameplayMonsterMovementActivation_prepareCardinalMove(
        96,0,64,&outBefore,&outAfter));
    assert(prepares == 1 && captures == 0);
    prepared = 1;
    assert(EspNativeGameplayMonsterMovementActivation_prepareCardinalMove(
        96,0,64,&outBefore,&outAfter));
    assert(prepares == 2 && captures == 1);
    assert(memcmp(&before,&outBefore,sizeof(before)) == 0);
    assert(memcmp(&after,&outAfter,sizeof(after)) == 0);
    assert(before.tileIndex == 168 && after.tileIndex == 200);
    puts("monster_activation_log_host: PASS admission/capture preserved");
    return 0;
}
