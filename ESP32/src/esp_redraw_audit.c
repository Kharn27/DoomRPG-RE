#include "esp_redraw_audit.h"

/* One diagnostic lease, no framebuffer, allocation, hashes or new timer.
 * A lamp write is separate physical ownership, not a world composition.
 * We still record it; future physical reuse must honor its underlay. */
typedef struct {
    EspRedrawAuditStats stats;
    uint16_t gap, during, lastBlockers, beforeBlockers;
    uint8_t prior, open, full, pending;
} Audit;
static Audit audit;
_Static_assert(sizeof(Audit) <= 104U, "P5b static RAM budget");

#define SEMANTIC_KINDS (ESP_REDRAW_ACTIVATION | ESP_REDRAW_REVEAL | ESP_REDRAW_VISITED)

void EspRedrawAudit_event(uint16_t kinds) {
    if (kinds & SEMANTIC_KINDS) {
        audit.stats.semanticKinds |= kinds & SEMANTIC_KINDS;
        kinds |= ESP_REDRAW_SEMANTIC;
    }
    if (audit.open) {
        audit.during |= kinds;
        if ((kinds & ESP_REDRAW_FULL) && audit.full < 2U) ++audit.full;
    } else audit.gap |= kinds;
}
void EspRedrawAudit_blockers(uint32_t mask) {
    /* All 16 P2 blocker bits fit; unknown future bits fail closed here. */
    audit.lastBlockers = (uint16_t)(mask > UINT16_MAX ? UINT16_MAX : mask);
}
void EspRedrawAudit_first(int bspValid, int negative, int hard, int phase) {
    EspRedrawAuditStats* s = &audit.stats;
    ++s->first;
    audit.pending = 0U;
    s->gapKinds |= audit.gap;
    if (audit.gap & ESP_REDRAW_LAMP) ++s->lampGap;
    if (!audit.prior) ++s->noPrior;
    else if (audit.gap & ~ESP_REDRAW_LAMP) ++s->gap;
    else if (!bspValid) ++s->noPrior;
    else if (hard) ++s->hard;
    else if (!negative) ++s->leaf;
    else if (phase) ++s->phase;
    else { ++s->candidate; audit.pending = 1U; }
}
void EspRedrawAudit_begin(void) {
    if (audit.open) { audit.prior = 0U; audit.pending = 0U; }
    audit.open = 1U;
    audit.beforeBlockers = audit.lastBlockers;
    audit.during = 0U;
    audit.full = 0U;
}
void EspRedrawAudit_finish(int presented, int quiet, int rejected,
    int equalFNV) {
    EspRedrawAuditStats* s = &audit.stats;
    const int physical = audit.open && audit.full == 1U &&
        !(audit.during & ~(ESP_REDRAW_FULL | ESP_REDRAW_LAMP |
                          ESP_REDRAW_SEMANTIC | SEMANTIC_KINDS));
    if (presented && audit.open) {
        s->beforeMask |= audit.beforeBlockers;
        s->afterMask |= audit.lastBlockers;
        if (audit.beforeBlockers) ++s->beforeBusy;
        if (audit.lastBlockers) ++s->afterBusy;
        if (!quiet && !audit.beforeBlockers && !audit.lastBlockers) ++s->unsafeQuiet;
        if (audit.during & ESP_REDRAW_SEMANTIC) ++s->semanticFrames;
    }
    if (audit.pending) {
        ++s->checked;
        if (!presented || !rejected) ++s->notRejected;
        if (!equalFNV) ++s->changedFNV;
        if (!quiet || !physical) ++s->unsafeAfter;
        if (audit.during & ESP_REDRAW_SEMANTIC) ++s->semanticAfter;
    }
    if (presented && !physical) ++s->physicalRejected;
    audit.prior = (uint8_t)(presented && quiet && physical &&
        !(audit.during & ESP_REDRAW_SEMANTIC));
    if (audit.prior) ++s->seeded;
    audit.gap = 0U;
    audit.open = 0U;
    audit.pending = 0U;
}
void EspRedrawAudit_abort(void) {
    if (audit.open) {
        if (audit.pending) {
            ++audit.stats.checked;
            ++audit.stats.notRejected;
            ++audit.stats.changedFNV;
            ++audit.stats.unsafeAfter;
        }
        audit.open = 0U;
        audit.pending = 0U;
        audit.prior = 0U;
    }
    audit.gap |= ESP_REDRAW_UNKNOWN;
}
const EspRedrawAuditStats* EspRedrawAudit_stats(void) { return &audit.stats; }
