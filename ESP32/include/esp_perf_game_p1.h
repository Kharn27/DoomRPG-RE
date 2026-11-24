#ifndef ESP_PERF_GAME_P1_H
#define ESP_PERF_GAME_P1_H
#include <stdint.h>
#ifndef DOOMRPG_PERF_GAME_P1
#define DOOMRPG_PERF_GAME_P1 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct EspPerfGameP1Snapshot_s {
    uint32_t turns, services, busyCalls, pausedCalls, delivered, aborted;
    uint32_t memberSamples, frames, framePresents, inputResponses, committedActions;
    uint32_t inputBlockedCombat, inputBusy;
    uint32_t wallMaxUs, serviceMaxUs, gapMaxUs, memberMaxUs, frameMaxUs, inputMaxUs, commitMaxUs;
} EspPerfGameP1Snapshot;
#if DOOMRPG_PERF_GAME_P1
typedef struct EspPerfGameP1Load_s {
    uint32_t turns, framesInside, framesBetween, maxDelivered;
    uint64_t serviceUs, frameInsideUs, frameBetweenUs, nonFrameServiceUs, lcdInsideUs;
} EspPerfGameP1Load;
void EspPerfGameP1_serviceEnter(void);
void EspPerfGameP1_phaseBegin(uint8_t phase,uint64_t now);
void EspPerfGameP1_phaseEnd(uint8_t phase,uint64_t now);
typedef struct EspPerfGameP1Phase_s {
    uint64_t wallUs, frameUs;
    uint32_t calls, frames, maxUs;
} EspPerfGameP1Phase;
int EspPerfGameP1_phaseSnapshot(uint8_t phase,EspPerfGameP1Phase* out);
int EspPerfGameP1_loadSnapshot(uint8_t bucket, EspPerfGameP1Load* out);
void EspPerfGameP1_reset(void);
void EspPerfGameP1_sequence(uint64_t startUs, uint64_t endUs,
    uint32_t beforeTurn, uint32_t afterTurn, uint8_t beforeBusy,
    uint8_t afterBusy, uint32_t activeCount, uint32_t delivered,
    uint32_t pauseProbe);
void EspPerfGameP1_abort(void);
void EspPerfGameP1_member(uint64_t startUs,uint64_t endUs,uint32_t activeCount);
void EspPerfGameP1_inputAccepted(uint32_t sequence,uint64_t atUs);
void EspPerfGameP1_inputConsumed(uint32_t sequence);
void EspPerfGameP1_inputCommit(uint32_t sequence,uint64_t nowUs);
void EspPerfGameP1_inputDispatchDone(uint64_t nowUs);
void EspPerfGameP1_inputBlockedCombat(void);
void EspPerfGameP1_inputBusy(void);
void EspPerfGameP1_frame(uint32_t totalUs,uint32_t presentUs,uint8_t preview,
                         uint8_t publish,uint8_t presented);
int EspPerfGameP1_snapshot(uint8_t cohort,EspPerfGameP1Snapshot* out);
#else
static inline void EspPerfGameP1_reset(void) {}
static inline void EspPerfGameP1_sequence(uint64_t a,uint64_t b,uint32_t c,
    uint32_t d,uint8_t e,uint8_t f,uint32_t g,uint32_t h,uint32_t i) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;
}
static inline void EspPerfGameP1_abort(void) {}
static inline void EspPerfGameP1_member(uint64_t a,uint64_t b,uint32_t c) {
    (void)a;(void)b;(void)c;
}
static inline void EspPerfGameP1_inputAccepted(uint32_t a,uint64_t b) {(void)a;(void)b;}
static inline void EspPerfGameP1_inputConsumed(uint32_t a) {(void)a;}
static inline void EspPerfGameP1_inputCommit(uint32_t a,uint64_t b) {(void)a;(void)b;}
static inline void EspPerfGameP1_inputDispatchDone(uint64_t a) {(void)a;}
static inline void EspPerfGameP1_inputBlockedCombat(void) {}
static inline void EspPerfGameP1_inputBusy(void) {}
static inline void EspPerfGameP1_frame(uint32_t a,uint32_t b,uint8_t c,uint8_t d,uint8_t e) {
    (void)a;(void)b;(void)c;(void)d;(void)e;
}
static inline int EspPerfGameP1_snapshot(uint8_t a,EspPerfGameP1Snapshot* b) {
    (void)a;(void)b;return 0;
}
#endif
#ifdef __cplusplus
}
#endif
#endif
