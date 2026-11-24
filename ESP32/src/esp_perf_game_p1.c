#include "esp_perf_game_p1.h"
#if DOOMRPG_PERF_GAME_P1
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define BINS 15U
#define MASS_COUNT 16U
#define REPORT_PERIOD 8U
typedef struct {
    uint32_t bins[BINS], count, maxUs;
} Dist;
typedef struct {
    EspPerfGameP1Snapshot totals;
    Dist wall, service, gap, member, frame, lcd, input;
    uint32_t previewFrames;
} Cohort;
typedef struct {
    uint64_t start, lastEnd;
    uint32_t id, serviceUs, gapUs, services, busy, paused;
    uint8_t group, open;
} Turn;
static Cohort cohort[2];
static Turn turn;
static uint32_t completed, inputSeq;
static uint64_t inputAt;
static uint8_t inputConsumed, currentGroup;
static uint32_t delta(uint64_t end,uint64_t start) {
    uint64_t d = end >= start ? end-start : 0;
    return d > UINT32_MAX ? UINT32_MAX : (uint32_t)d;
}
static uint32_t sat(uint32_t a,uint32_t b) {
    return UINT32_MAX-a < b ? UINT32_MAX : a+b;
}
static uint8_t groupFor(uint32_t count) {return count>=MASS_COUNT?1U:0U;}
static void record(Dist* d,uint32_t us) {
    uint32_t ceiling=1000U;
    unsigned i=0;
    while(i+1U<BINS && us>ceiling) {++i;ceiling*=2U;}
    if (d->count<UINT32_MAX) ++d->count;
    if (d->bins[i]<UINT32_MAX) ++d->bins[i];
    if (us>d->maxUs) d->maxUs=us;
}
static uint32_t qms(const Dist* d,uint32_t percentage) {
    uint64_t need, seen=0;
    uint32_t ceiling=1;
    unsigned i;
    if (!d->count) return 0;
    need=((uint64_t)d->count*percentage+99U)/100U;
    for(i=0;i<BINS;i++,ceiling*=2U) {
        seen+=d->bins[i];
        if (seen>=need) return ceiling;
    }
    return ceiling;
}
static void report(unsigned n) {
    Cohort* c=&cohort[n];
    if (!c->totals.turns) return;
    printf("[PERFGAMEP1] group=%s historicalThreshold=%u turns=%u delivered=%u serviceCalls=%u busyCalls=%u pausedCalls=%u aborted=%u wallP50LeMs=%u wallP95LeMs=%u wallMaxUs=%u serviceP50LeMs=%u serviceP95LeMs=%u serviceMaxUs=%u gapP50LeMs=%u gapP95LeMs=%u gapMaxUs=%u memberSamples=%u memberMaxUs=%u\n",
        n?"mass":"pre", (unsigned)MASS_COUNT, (unsigned)c->totals.turns,
        (unsigned)c->totals.delivered,(unsigned)c->totals.services,
        (unsigned)c->totals.busyCalls,(unsigned)c->totals.pausedCalls,
        (unsigned)c->totals.aborted,(unsigned)qms(&c->wall,50),
        (unsigned)qms(&c->wall,95),(unsigned)c->wall.maxUs,
        (unsigned)qms(&c->service,50),(unsigned)qms(&c->service,95),
        (unsigned)c->service.maxUs,(unsigned)qms(&c->gap,50),
        (unsigned)qms(&c->gap,95),(unsigned)c->gap.maxUs,
        (unsigned)c->totals.memberSamples,(unsigned)c->member.maxUs);
    printf("[PERFGAMEP1] group=%s composedFrames=%u previewFrames=%u frameP50LeMs=%u frameP95LeMs=%u frameMaxUs=%u framePresents=%u lcdMaxUs=%u inputResponses=%u inputP50LeMs=%u inputP95LeMs=%u inputMaxUs=%u combatBlocked=%u inputBusy=%u cumulative=yes\n",
        n?"mass":"pre",(unsigned)c->totals.frames,(unsigned)c->previewFrames,
        (unsigned)qms(&c->frame,50),(unsigned)qms(&c->frame,95),
        (unsigned)c->frame.maxUs,(unsigned)c->totals.framePresents,
        (unsigned)c->lcd.maxUs,(unsigned)c->totals.inputResponses,
        (unsigned)qms(&c->input,50),(unsigned)qms(&c->input,95),
        (unsigned)c->input.maxUs,(unsigned)c->totals.inputBlockedCombat,
        (unsigned)c->totals.inputBusy);
}
void EspPerfGameP1_reset(void) {
    memset(cohort,0,sizeof(cohort));
    memset(&turn,0,sizeof(turn));
    completed=0;inputSeq=0;inputAt=0;inputConsumed=0;currentGroup=0;
}
void EspPerfGameP1_abort(void) {
    if (turn.open) ++cohort[turn.group].totals.aborted;
    turn.open=0;
}
void EspPerfGameP1_sequence(uint64_t start,uint64_t end,uint32_t previous,
    uint32_t next,uint8_t beforeBusy,uint8_t afterBusy,
    uint32_t historical,uint32_t delivered,uint32_t pauseProbe) {
    Cohort* c;
    if (next!=previous && next!=0) {
        if(turn.open) EspPerfGameP1_abort();
        memset(&turn,0,sizeof(turn));
        turn.start=start;turn.lastEnd=start;turn.id=next;
        turn.group=groupFor(historical);turn.open=1;
        currentGroup=turn.group;
    }
    if (!turn.open || next!=turn.id) return;
    c=&cohort[turn.group];
    if(turn.services && start>=turn.lastEnd)
        turn.gapUs=sat(turn.gapUs,delta(start,turn.lastEnd));
    turn.serviceUs=sat(turn.serviceUs,delta(end,start));
    turn.lastEnd=end;
    ++turn.services;
    if(beforeBusy) ++turn.busy;
    if(pauseProbe) ++turn.paused;
    if(afterBusy) return;
    record(&c->wall,delta(end,turn.start));
    record(&c->service,turn.serviceUs);
    record(&c->gap,turn.gapUs);
    ++c->totals.turns;
    c->totals.services=sat(c->totals.services,turn.services);
    c->totals.busyCalls=sat(c->totals.busyCalls,turn.busy);
    c->totals.pausedCalls=sat(c->totals.pausedCalls,turn.paused);
    c->totals.delivered=sat(c->totals.delivered,delivered);
    c->totals.wallMaxUs=c->wall.maxUs;
    c->totals.serviceMaxUs=c->service.maxUs;
    c->totals.gapMaxUs=c->gap.maxUs;
    turn.open=0;
    if (++completed % REPORT_PERIOD==0U) {report(0);report(1);}
}
void EspPerfGameP1_member(uint64_t start,uint64_t end,uint32_t historical) {
    Cohort* c=&cohort[groupFor(historical)];
    ++c->totals.memberSamples;
    record(&c->member,delta(end,start));
    c->totals.memberMaxUs=c->member.maxUs;
}
void EspPerfGameP1_inputAccepted(uint32_t seq,uint64_t at) {
    inputSeq=seq;inputAt=at;inputConsumed=0;
}
void EspPerfGameP1_inputConsumed(uint32_t seq) {
    if (inputSeq && seq==inputSeq) inputConsumed=1;
}
void EspPerfGameP1_inputDispatchDone(uint64_t now) {
    Cohort* c;
    if(!inputConsumed) return;
    c=&cohort[currentGroup];
    record(&c->input,delta(now,inputAt));
    ++c->totals.inputResponses;
    c->totals.inputMaxUs=c->input.maxUs;
    inputConsumed=0;inputSeq=0;
}
void EspPerfGameP1_inputBlockedCombat(void) {
    ++cohort[currentGroup].totals.inputBlockedCombat;
}
void EspPerfGameP1_inputBusy(void) {++cohort[currentGroup].totals.inputBusy;}
void EspPerfGameP1_frame(uint32_t total,uint32_t lcd,uint8_t preview,
                         uint8_t publish,uint8_t presented) {
    Cohort* c=&cohort[currentGroup];
    (void)publish;
    ++c->totals.frames;
    if(preview) ++c->previewFrames;
    if(presented) ++c->totals.framePresents;
    record(&c->frame,total);
    if(presented)record(&c->lcd,lcd);
    c->totals.frameMaxUs=c->frame.maxUs;
}
int EspPerfGameP1_snapshot(uint8_t which,EspPerfGameP1Snapshot* out) {
    if(which>1 || !out) return 0;
    *out=cohort[which].totals;
    return 1;
}
#endif
