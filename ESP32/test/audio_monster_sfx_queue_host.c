#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_audio_intent.h"

static EspNativeAudioIntent nextAfter(uint32_t cursor) {
    EspNativeAudioIntent n={0};
    assert(EspNativeAudioIntent_nextAfter(cursor,&n));
    return n;
}
static EspNativeAudioIntentStats stats(void) {
    EspNativeAudioIntentStats s={0};
    EspNativeAudioIntent_getStats(&s);
    return s;
}

int main(void) {
    EspNativeAudioIntent n;
    EspNativeAudioIntentStats s;
    assert(sizeof(EspNativeAudioIntent)==8U);
    EspNativeAudioIntent_reset();
    assert(!EspNativeAudioIntent_nextAfter(0U,&n));
    assert(!EspNativeAudioIntent_publish(0U,0U,0U));

    /* One reader ACKing after each effect can play arbitrarily many
     * consecutive CRATE/PICKUP/DOOR SFX without a 17th-event false alarm. */
    for (uint32_t i=0U;i<64U;++i) {
        const uint16_t id=(i%3U==0U)?5046U:
                          ((i%3U==1U)?5060U:5063U);
        assert(EspNativeAudioIntent_publish(id,0U,(uint8_t)(i%5U+1U)));
        n=nextAfter(i);
        assert(n.sequence==i+1U && n.resourceId==id);
        assert(n.priority==(uint8_t)(i%5U+1U));
        /* Peeking (or a temporary PAK lease) MUST NOT consume the event. */
        assert(nextAfter(i).sequence==n.sequence);
        EspNativeAudioIntent_acknowledgeThrough(n.sequence);
        assert(!EspNativeAudioIntent_nextAfter(n.sequence,&n));
    }
    s=stats();
    assert(s.queueOverflow==0U && s.consumerCursorGap==0U);
    assert(s.audibleSfxDropped==0U && s.consumed==64U);
    assert(s.pending==0U && EspNativeAudioIntent_count()==64U);
    assert(EspNativeAudioIntent_last()->sequence==64U);

    /* Same-turn priority is metadata; CALL ORDER (including STOP_ALL)
     * is the semantic order, and an ACK must not skip the next command. */
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_publish(5046U,0U,5U)); /* weapon */
    assert(EspNativeAudioIntent_publish(5099U,0U,2U)); /* monster */
    assert(EspNativeAudioIntent_stopAll());
    assert(EspNativeAudioIntent_publish(5060U,0U,3U)); /* after stop */
    n=nextAfter(0U); assert(n.sequence==1U && n.priority==5U);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    n=nextAfter(1U); assert(n.sequence==2U && n.priority==2U);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    n=nextAfter(2U); assert(n.sequence==3U && n.resourceId==0U);
    assert(n.flags==ESP_NATIVE_AUDIO_STOP_ALL_FLAG);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    n=nextAfter(3U); assert(n.sequence==4U && n.resourceId==5060U);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    assert(stats().pending==0U && stats().queueOverflow==0U);

    /* EXIT -> MENU -> LOAD V11: the next track must NOT replay stale
     * effects/STOP_ALL or roll the monotonic sequence backwards. */
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_publish(5042U,0U,3U));
    assert(EspNativeAudioIntent_stopAll());
    EspNativeAudioIntent_discardThrough(EspNativeAudioIntent_count());
    s=stats();
    assert(s.handoffDiscarded==2U && s.consumed==0U && s.pending==0U);
    assert(EspNativeAudioIntent_publish(5063U,0U,2U));
    n=nextAfter(2U);
    assert(n.sequence==3U && n.resourceId==5063U && n.priority==2U);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    assert(stats().consumed==1U && stats().pending==0U);
    assert(EspNativeAudioIntent_count()==3U);

    /* A genuine burst >16 with no consumer is still an OVERFLOW, not a
     * success with more RAM or a silently hidden discard. */
    EspNativeAudioIntent_reset();
    for (uint32_t i=0U;i<20U;++i)
        assert(EspNativeAudioIntent_publish((uint16_t)(5061U+i),0U,3U));
    s=stats();
    assert(s.queueOverflow==4U && s.pending==16U);
    n=nextAfter(0U);
    assert(n.sequence==5U);
    EspNativeAudioIntent_noteConsumerGap(0U,n.sequence);
    EspNativeAudioIntent_acknowledgeThrough(n.sequence);
    for (uint32_t seq=6U;seq<=20U;++seq) {
        n=nextAfter(seq-1U);
        assert(n.sequence==seq);
        EspNativeAudioIntent_acknowledgeThrough(seq);
    }
    EspNativeAudioIntent_notePlaybackDrop(); /* one known PCM rejection */
    EspNativeAudioIntent_notePlaybackDrop(); /* one partial PCM mix */
    s=stats();
    assert(s.queueOverflow==4U && s.consumerCursorGap==4U);
    assert(s.audibleSfxDropped==2U && s.pending==0U);
    assert(s.consumed==16U && s.handoffDiscarded==0U);
    assert(EspNativeAudioIntent_count()==20U);

    EspNativeAudioIntent_reset();
    s=stats();
    assert(s.queueOverflow==0U && s.consumerCursorGap==0U);
    assert(s.audibleSfxDropped==0U && s.pending==0U);
    assert(s.consumed==0U && s.handoffDiscarded==0U);
    assert(EspNativeAudioIntent_last()==NULL);
    puts("[MONSTERSFX-FIFO] PASS ACK 64 events, deferral, priority/STOP, "
         "EXIT/LOAD, genuine burst gap and independent PCM-drop counters");
    return 0;
}
