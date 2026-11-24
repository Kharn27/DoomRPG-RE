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
    EspPerfGameP1_inputCommit(6,138000);
    EspPerfGameP1_inputDispatchDone(142000);
    EspPerfGameP1_inputBlockedCombat();
    EspPerfGameP1_inputBusy();
    assert(EspPerfGameP1_snapshot(0,&pre));
    assert(pre.inputResponses==1 && pre.committedActions==1 && pre.commitMaxUs==8000);
    assert(pre.inputBlockedCombat==1 && pre.inputBusy==1);
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
    EspPerfGameP1_abort(); /* failure in the *first* body invocation */
    EspPerfGameP1_sequence(2000000,2000010,0,1,0,0,24,0,0);
    assert(EspPerfGameP1_snapshot(1,&mass));
    assert(mass.aborted==1 && mass.turns==0);
    EspPerfGameP1_reset();
    assert(EspPerfGameP1_snapshot(0,&pre) && pre.turns==0);
    {
        EspPerfGameP1Load p={0};
        EspPerfGameP1_serviceEnter();
        EspPerfGameP1_frame(2000,500,0,1,1);
        EspPerfGameP1_sequence(10000,15000,0,1,0,1,31,1,0);
        EspPerfGameP1_frame(3000,1000,1,1,1);
        EspPerfGameP1_serviceEnter();
        EspPerfGameP1_frame(1000,300,0,1,1);
        EspPerfGameP1_sequence(20000,24000,1,1,1,0,31,7,0);
        EspPerfGameP1_frame(7000,2000,1,1,1);
        assert(EspPerfGameP1_loadSnapshot(2,&p));
        assert(p.turns==1 && p.maxDelivered==7 && p.framesInside==2 && p.framesBetween==1);
        assert(p.serviceUs==9000 && p.frameInsideUs==3000 && p.frameBetweenUs==3000);
        assert(p.nonFrameServiceUs==6000 && p.lcdInsideUs==800);
        EspPerfGameP1_serviceEnter();
        EspPerfGameP1_sequence(30000,30100,1,2,0,0,31,0,0);
        assert(EspPerfGameP1_loadSnapshot(0,&p) && p.turns==1 && p.maxDelivered==0);
    }
    {
        EspPerfGameP1Phase p={0};
        EspPerfGameP1_reset();
        EspPerfGameP1_serviceEnter();
        EspPerfGameP1_phaseBegin(0,1000);
        EspPerfGameP1_phaseEnd(0,1200);
        EspPerfGameP1_phaseBegin(1,1300);
        EspPerfGameP1_phaseBegin(3,1400);
        EspPerfGameP1_frame(2000,500,0,1,1);
        EspPerfGameP1_phaseEnd(3,4000);
        EspPerfGameP1_phaseEnd(1,4300);
        assert(EspPerfGameP1_phaseSnapshot(3,&p) && p.calls==1 && p.wallUs==2600 && p.frameUs==2000);
        EspPerfGameP1_phaseEnd(1,4500);
        assert(EspPerfGameP1_phaseSnapshot(0,&p) && p.calls==1 && p.wallUs==200 && p.frameUs==0);
        assert(EspPerfGameP1_phaseSnapshot(1,&p) && p.calls==1 && p.wallUs==3000 && p.frameUs==2000 && p.frames==1 && p.maxUs==3000);
        EspPerfGameP1_sequence(1000,5000,0,1,0,0,31,1,0);
    }
    puts("perf_game_p1_host: PASS bounded histograms, cohorts, CPU/gap, attribution, abort");
    return 0;
}
