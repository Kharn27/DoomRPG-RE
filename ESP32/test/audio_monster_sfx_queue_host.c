#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_audio_intent.h"

int main(void) {
    EspNativeAudioIntent n;
    assert(sizeof(EspNativeAudioIntent)==8U);
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_nextAfter(0U,&n)==0);
    assert(EspNativeAudioIntent_publish(5046U,0U,5U));
    assert(EspNativeAudioIntent_publish(5099U,0U,2U));
    assert(EspNativeAudioIntent_nextAfter(0U,&n) && n.sequence==1U && n.resourceId==5046U);
    assert(EspNativeAudioIntent_nextAfter(1U,&n) && n.sequence==2U && n.resourceId==5099U);
    assert(EspNativeAudioIntent_nextAfter(2U,&n)==0);
    for (int i=0;i<20;++i)
        assert(EspNativeAudioIntent_publish((uint16_t)(5061U+i),0U,3U));
    assert(EspNativeAudioIntent_count()==22U);
    /* 16 retained elements: first now sequence 7, no heap or dynamic list. */
    assert(EspNativeAudioIntent_nextAfter(0U,&n) && n.sequence==7U);
    assert(EspNativeAudioIntent_nextAfter(21U,&n) && n.sequence==22U);
    assert(EspNativeAudioIntent_stopAll());
    assert(EspNativeAudioIntent_nextAfter(22U,&n) &&
           n.sequence==23U && n.flags==ESP_NATIVE_AUDIO_STOP_ALL_FLAG);
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_nextAfter(0U,&n)==0);
    puts("[MONSTERSFX-FIFO] PASS two same-turn effects, 16-slot overflow, STOP_ALL, reset");
    return 0;
}
