/* Host: cc -std=c11 -DDOOMRPG_PERF_GAME_P1=1 -IESP32/include ESP32/src/esp_perf_game_p1.c ESP32/test/perf_game_p1_host.c -o /tmp/perf_game_p1 && /tmp/perf_game_p1 */
#include <assert.h>
#include <stdio.h>
#include "esp_perf_game_p1.h"
int main(void) {
    EspPerfGameP1Snapshot pre={0}, mass={0};
    unsigned i;
    EspPerfGameP1_reset();
    EspPerfGameP1_inputAccepted(5,10000);
    EspPerfGameP1_inputConsumed(4);
    EspPerfGameP1_inputDispatchDone(30000);
    EspPerfGameP1_sequence(100000,105000,0,1,0,1,7,1,0);
    EspPerfGameP1_sequence(110000,112000,1,1,1,1,7,1,23);
    EspPerfGameP1_sequence(122000,126000,1,1,1,0,7,7,0);
    assert(EspPerfGameP1_snapshot(0,&pre));
    assert(pre.turns==1 && pre.services==3 && pre.busyCalls==2);
    assert(pre.pausedCalls==1 && pre.delivered==7);
    assert(pre.wallMaxUs==26000 && pre.serviceMaxUs==11000 && pre.gapMaxUs==15000);
    assert(pre.inputResponses==0);
    EspPerfGameP1_member(0,1500,7);
    EspPerfGameP1_frame(50000,10000,1,1,1);
    EspPerfGameP1_inputAccepted(6,130000);
    EspPerfGameP1_inputConsumed(6);
    EspPerfGameP1_inputDispatchDone(142000);
    EspPerfGameP1_inputBlockedCombat();
    EspPerfGameP1_inputBusy();
    assert(EspPerfGameP1_snapshot(0,&pre));
    assert(pre.inputResponses==1 && pre.inputBlockedCombat==1 && pre.inputBusy==1);
    assert(pre.frames==1 && pre.framePresents==1 && pre.memberSamples==1);
    for(i=0;i<7;i++) {
        const uint32_t id=2+i;
        const uint64_t t=1000000ULL+i*100000ULL;
        EspPerfGameP1_sequence(t,t+100,id-1,id,0,1,24,1,0);
        EspPerfGameP1_sequence(t+500,t+900,id,id,1,0,24,7,0);
    }
    assert(EspPerfGameP1_snapshot(1,&mass));
    assert(mass.turns==7 && mass.services==14 && mass.delivered==49);
    EspPerfGameP1_sequence(2000000,2000010,8,9,0,1,24,1,0);
    EspPerfGameP1_abort();
    EspPerfGameP1_sequence(2000020,2000030,9,9,1,0,24,1,0);
    assert(EspPerfGameP1_snapshot(1,&mass) && mass.aborted==1 && mass.turns==7);
    EspPerfGameP1_reset();
    assert(EspPerfGameP1_snapshot(0,&pre) && pre.turns==0);
    puts("perf_game_p1_host: PASS bounded histograms, cohorts, CPU/gap, attribution, abort");
    return 0;
}
