#include "esp_redraw_audit.h"

/* One diagnostic lease, no framebuffer, allocation, hashes or new timer.
 * A lamp write is separate physical ownership, not a world composition.
 * We still record it; future physical reuse must honor its underlay. */
typedef struct {
    EspRedrawAuditStats stats;
    uint32_t activation, lines, sprites;
    uint16_t gap, during;
    uint8_t known, prior, open, full, pending;
} Audit;
static Audit audit;
_Static_assert(sizeof(Audit) <= 96U, "P5a static RAM budget");

void EspRedrawAudit_event(uint16_t kinds) {
    if (audit.open) {
        audit.during |= kinds;
        if ((kinds & ESP_REDRAW_FULL) && audit.full < 2U) ++audit.full;
    } else audit.gap |= kinds;
}
void EspRedrawAudit_sampleOwners(int known, uint32_t activation,
    uint32_t lines, uint32_t sprites) {
    /* Count changes are a lower-bound observation, not exact owner identity.
     * Equal counts cannot establish a proof (including visited tiles). */
    if (!known || !audit.known)
        EspRedrawAudit_event(ESP_REDRAW_UNKNOWN);
    else if (audit.activation != activation || audit.lines != lines ||
             audit.sprites != sprites)
        EspRedrawAudit_event(ESP_REDRAW_SEMANTIC);
    audit.known = (uint8_t)(known != 0);
    audit.activation = activation;
    audit.lines = lines;
    audit.sprites = sprites;
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
    audit.during = 0U;
    audit.full = 0U;
}
void EspRedrawAudit_finish(int presented, int quiet, int rejected,
    int equalFNV) {
    EspRedrawAuditStats* s = &audit.stats;
    const int physical = audit.open && audit.full == 1U &&
        !(audit.during & ~(ESP_REDRAW_FULL | ESP_REDRAW_LAMP |
                          ESP_REDRAW_SEMANTIC));
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
