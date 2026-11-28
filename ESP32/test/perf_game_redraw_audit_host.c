#include "esp_redraw_audit.h"
#include <assert.h>

static void frame(int quiet, int rejected, int equal, unsigned int extra) {
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL | extra);
    EspRedrawAudit_finish(1, quiet, rejected, equal);
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
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_event(ESP_REDRAW_ACTIVATION);
    EspRedrawAudit_finish(1,1,0,0);
    assert(s->semanticAfter == 1U && s->notRejected == 1U && s->changedFNV == 1U);
    EspRedrawAudit_first(1,1,0,0); assert(s->noPrior == 2U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin(); EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_finish(1,1,1,1);
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
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_blockers(0x0080U);
    EspRedrawAudit_finish(1,0,1,1);
    assert(s->beforeMask == 0x1000U && s->afterMask == 0x0080U);
    assert(s->beforeBusy == 1U && s->afterBusy == 1U);
    EspRedrawAudit_blockers(0U);
    frame(0,1,1,0);
    assert(s->unsafeQuiet == 1U);
    frame(1,1,1,0);
    EspRedrawAudit_first(1,1,0,0);
    EspRedrawAudit_begin();
    EspRedrawAudit_event(ESP_REDRAW_FULL);
    EspRedrawAudit_event(ESP_REDRAW_REVEAL);
    EspRedrawAudit_event(ESP_REDRAW_REVEAL);
    EspRedrawAudit_finish(1,1,1,1);
    assert(s->semanticFrames == 2U && s->semanticAfter == 2U);
    assert(s->physicalRejected == 1U);
    return 0;
}
