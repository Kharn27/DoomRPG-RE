/* P4b: real activation owner, identical INFO/TRACE state and ordered logs. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp_map_runtime.h"
#include "esp_native_gameplay_monster_activation.h"

static EspMapRuntimeView runtime;
const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void) { return NULL; }
int EspNativeGameplayDestructibleTurn_flush(void) { return 1; }

int main(void) {
    EspNativeGameplayMonsterActivationSnapshot before, after;
    uint16_t sprite = 0;
    runtime.arenaFNV1a = 0x12345678U;
    EspNativeGameplayMonsterActivation_reset();
    assert(EspNativeGameplayMonsterActivation_observeVisible(83,4,231));
    assert(EspNativeGameplayMonsterActivation_observeVisible(202,4,274));
    assert(EspNativeGameplayMonsterActivation_observeVisible(83,4,199));
    assert(EspNativeGameplayMonsterActivation_count() == 2);
    assert(EspNativeGameplayMonsterActivation_getOrdered(0,&sprite) && sprite == 83);
    assert(EspNativeGameplayMonsterActivation_getOrdered(1,&sprite) && sprite == 202);
    assert(!EspNativeGameplayMonsterActivation_getOrdered(2,&sprite));
    assert(EspNativeGameplayMonsterActivation_snapshot(&before));
    assert(EspNativeGameplayMonsterActivation_restoreSnapshot(&before));
    assert(EspNativeGameplayMonsterActivation_snapshot(&after));
    assert(memcmp(&before,&after,sizeof before)==0);
    for (uint16_t i=0;i<62;++i)
        assert(EspNativeGameplayMonsterActivation_observeVisible(i,1,i));
    assert(EspNativeGameplayMonsterActivation_count()==64);
    assert(EspNativeGameplayMonsterActivation_snapshot(&before));
    assert(!EspNativeGameplayMonsterActivation_observeVisible(100,2,4));
    assert(!EspNativeGameplayMonsterActivation_observeVisible(1024,2,4));
    assert(EspNativeGameplayMonsterActivation_snapshot(&after));
    assert(memcmp(&before,&after,sizeof before)==0);
    printf("P4b state=%08x count=%u\n",(unsigned)after.stateFNV1a,(unsigned)after.activatedCount);
    return 0;
}
