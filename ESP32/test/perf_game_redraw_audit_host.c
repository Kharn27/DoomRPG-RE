#include "esp_redraw_audit.h"
#include <assert.h>

static void frame(int quiet, int rejected, int equal, unsigned int extra) {
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT | extra);
    EspRedrawAudit_finish(1, quiet, rejected, equal, 0U, 0U);
}
int main(void) {
    const EspRedrawAuditStats* s = EspRedrawAudit_stats();
    EspRedrawAudit_first(0,0,0,0);
    assert(s->noPrior == 1U);
    EspRedrawAudit_blockers(0U);
    frame(1,1,1,0);
    assert(s->seeded == 1U);
    EspRedrawAudit_event(ESP_REDRAW_LAMP);
    EspRedrawAudit_first(1,1,0,0);
    assert(s->candidate == 1U && s->lampGap == 1U);
    frame(1,1,1,0);
    assert(s->checked == 1U && !s->changedFNV && !s->notRejected);
    EspRedrawAudit_event(ESP_REDRAW_REGION);
    EspRedrawAudit_first(1,1,0,0);
    assert(s->gap == 1U && (s->gapKinds & ESP_REDRAW_REGION));
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,1,0); assert(s->hard == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,0,0,0); assert(s->leaf == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,1); assert(s->phase == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_event(ESP_REDRAW_ACTIVATION);
    EspRedrawAudit_finish(1,1,0,0, 0U, 0U);
    assert(s->semanticAfter == 1U && s->notRejected == 1U && s->changedFNV == 1U);
    EspRedrawAudit_first(1,1,0,0); assert(s->noPrior == 2U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin(); EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1, 0U, 0U);
    assert(s->physicalRejected == 1U && s->unsafeAfter == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin(); EspRedrawAudit_abort();
    assert(s->checked == 4U && s->unsafeAfter == 2U);
    EspRedrawAudit_first(1,1,0,0);
    assert(s->first == s->noPrior+s->gap+s->hard+s->leaf+s->phase+s->candidate);
    frame(1,1,1,0);
    EspRedrawAudit_event(ESP_REDRAW_COMPOSE | ESP_REDRAW_CLEAR | ESP_REDRAW_DISPLAY);
    EspRedrawAudit_first(0,1,0,0);
    assert(s->gap == 2U);
    frame(1,1,1,0);
    EspRedrawAudit_event(ESP_REDRAW_REVEAL);
    EspRedrawAudit_event(ESP_REDRAW_VISITED);
    EspRedrawAudit_first(1,1,0,0);
    assert(s->gap == 3U && (s->gapKinds & ESP_REDRAW_SEMANTIC));
    /* Equal-count reversal cannot erase a mutation, even if the next
     * full frame and all fingerprints happen to be equal. */
    assert((s->semanticKinds & (ESP_REDRAW_ACTIVATION | ESP_REDRAW_REVEAL |
        ESP_REDRAW_VISITED)) == (ESP_REDRAW_ACTIVATION | ESP_REDRAW_REVEAL |
        ESP_REDRAW_VISITED));
    EspRedrawAudit_blockers(0x1000U);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_blockers(0x0080U);
    EspRedrawAudit_finish(1,0,1,1, 0U, 0U);
    assert(s->beforeMask == 0x1000U && s->afterMask == 0x0080U);
    assert(s->beforeBusy == 1U && s->afterBusy == 1U);
    EspRedrawAudit_blockers(0U);
    frame(0,1,1,0);
    assert(s->unsafeQuiet == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_event(ESP_REDRAW_REVEAL);
    EspRedrawAudit_event(ESP_REDRAW_REVEAL);
    EspRedrawAudit_finish(1,1,1,1, 0U, 0U);
    assert(s->semanticFrames == 2U && s->semanticAfter == 2U);
    assert(s->physicalRejected == 1U);

    /* Attempts are not completed physical scanouts; commit without attempt,
     * double commits and partial writes cannot seed the witness. */
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_finish(1,1,1,1,0,0);
    EspRedrawAudit_first(1,1,0,0);
    assert(s->physicalRejected == 2U && s->noPrior > 2U);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1,0,0);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_event(ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1,0,0);
    assert(s->physicalRejected == 4U);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_finish(1,1,1,1,0,0);
    assert(s->physicalRejected == 5U);

    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_firstDetail(1,1,0,0,0,100U,150U);
    EspRedrawAudit_begin();
    EspRedrawAudit_renderSample(140U);
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1,70000U,40000U);
    assert(s->strictFrameUs == 70000U && s->strictPresentUs == 40000U);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_firstDetail(1,1,0,0,0,100U,150U);
    EspRedrawAudit_begin();
    EspRedrawAudit_renderSample(250U);
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1,70000U,40000U);
    assert(s->strictLate == 1U && s->strictConfirmed == 1U && s->strictFrameUs == 70000U);
    EspRedrawAudit_first(1,0,0,0);
    EspRedrawAudit_firstDetail(1,0,1,0,0,100U,150U);
    EspRedrawAudit_begin();
    EspRedrawAudit_renderSample(120U);
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,1,0,0);
    assert(s->crossChecked == 1U && s->crossMismatch == 0U);
    EspRedrawAudit_first(1,1,0,1);
    EspRedrawAudit_firstDetail(1,1,0,0,ESP_REDRAW_PHASE_DEADLINE,100U,20U);
    EspRedrawAudit_begin();
    EspRedrawAudit_renderSample(140U);
    EspRedrawAudit_event(ESP_REDRAW_FULL | ESP_REDRAW_FULL_COMMIT);
    EspRedrawAudit_finish(1,1,1,0,0,0);
    assert(s->phaseDeadline == 1U && s->dueChecked == 1U && s->dueMismatch == 1U);
    EspRedrawAudit_first(1,1,0,1);
    EspRedrawAudit_firstDetail(1,1,0,0,ESP_REDRAW_PHASE_AGE | ESP_REDRAW_PHASE_SCHEDULER,100U,20U);
    frame(1,1,1,0);
    assert(s->phaseAge == 1U && s->phaseScheduler == 1U && s->dueChecked == 1U);
    return 0;
}
