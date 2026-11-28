#ifndef ESP_REDRAW_AUDIT_H
#define ESP_REDRAW_AUDIT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* P5a: diagnostic events, never a permission to omit a frame. All callers
 * belong to the cooperative gameplay/display task, not an ISR. */
enum {
    ESP_REDRAW_COMPOSE = 1U << 0,
    ESP_REDRAW_FULL = 1U << 1,
    ESP_REDRAW_REGION = 1U << 2,
    ESP_REDRAW_CLEAR = 1U << 3,
    ESP_REDRAW_DISPLAY = 1U << 4,
    ESP_REDRAW_LAMP = 1U << 5,
    ESP_REDRAW_DEBUG = 1U << 6,
    ESP_REDRAW_SEMANTIC = 1U << 7,
    ESP_REDRAW_UNKNOWN = 1U << 8,
    ESP_REDRAW_ACTIVATION = 1U << 9,
    ESP_REDRAW_REVEAL = 1U << 10,
    ESP_REDRAW_VISITED = 1U << 11
};
typedef struct EspRedrawAuditStats_s {
    uint32_t first, noPrior, gap, hard, leaf, phase, candidate;
    uint32_t checked, notRejected, changedFNV, unsafeAfter, semanticAfter;
    uint32_t physicalRejected, seeded, lampGap, gapKinds;
    uint32_t beforeMask, afterMask, beforeBusy, afterBusy, unsafeQuiet;
    uint32_t semanticFrames, semanticKinds;
} EspRedrawAuditStats;
void EspRedrawAudit_event(uint16_t kinds);
void EspRedrawAudit_blockers(uint32_t mask);
void EspRedrawAudit_first(int bspValid, int negative,
    int hard, int phase);
void EspRedrawAudit_begin(void);
void EspRedrawAudit_finish(int presented, int quiet, int rejected,
    int equalFNV);
void EspRedrawAudit_abort(void);
const EspRedrawAuditStats* EspRedrawAudit_stats(void);
#ifdef __cplusplus
}
#endif
#endif
