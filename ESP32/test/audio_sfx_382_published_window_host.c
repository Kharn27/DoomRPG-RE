#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "esp_native_sfx_window.h"

enum { RING=16384, LEAD=1024, GUARD=128, REFILL=2041 };
typedef struct {
    uint16_t id;
    uint32_t start, output, mixed;
} Voice;

static int mix_one(Voice* voice, uint32_t reader, uint32_t writer) {
    if (voice->mixed==voice->output) return 1;
    const uint32_t first=voice->start+voice->mixed;
    if ((int32_t)(first-reader)<GUARD) return -1;
    const uint32_t n=EspNativeSfxPcm_publishedSpan(
        first,writer,voice->output-voice->mixed,RING);
    assert(n<=RING);
    assert(n<=voice->output-voice->mixed);
    assert(first+n<=writer);
    voice->mixed+=n;
    return voice->mixed==voice->output?1:0;
}

static void concurrent_sector_one(void) {
    /* Same published music ring as actual CYD (16KiB); start 12,500 queued
     * frames, then an illustrative 420ms service gap with music still live.
     * With the previous 8192 cap, the next service would hit the reader. */
    uint32_t rd=1000, wr=rd+12500;
    Voice v[]={{5046,rd+LEAD,11466,0},{5099,rd+LEAD,19404,0},
               {5069,rd+LEAD,20286,0}};
    enum { COUNT=sizeof(v)/sizeof(v[0]) };
    for (unsigned i=0;i<COUNT;i++) assert(mix_one(&v[i],rd,wr)>=0);
    assert(v[2].mixed==11476);
    const uint32_t delayedRd=rd+9261; /* 420ms @22050Hz */
    assert((int32_t)((rd+LEAD+8192)-delayedRd)<GUARD);
    assert((int32_t)((v[2].start+v[2].mixed)-delayedRd)>=GUARD);
    rd=delayedRd;
    assert(wr-rd>0U);
    wr+=4U*REFILL; /* existing four-block catch-up, 4*2041 samples */
    assert(wr-rd<RING);
    for (unsigned i=0;i<COUNT;i++) assert(mix_one(&v[i],rd,wr)>=0);
    for (unsigned tick=0;tick<12;tick++) {
        rd+=1985; /* ~90ms */
        assert(wr>rd); /* music remains non-starved in this oracle */
        while (wr-rd<=RING-REFILL && wr-rd<12500) wr+=REFILL;
        assert(wr-rd<=RING);
        unsigned complete=0;
        for (unsigned i=0;i<COUNT;i++) {
            const int status=mix_one(&v[i],rd,wr);
            assert(status>=0); /* no reader-caught-up */
            complete+=status==1;
        }
        if (complete==COUNT) break;
    }
    for (unsigned i=0;i<COUNT;i++) {
        assert(v[i].mixed==v[i].output);
        printf("id=%u mixed=%u/%u\n",v[i].id,v[i].mixed,v[i].output);
    }
}

int main(void) {
    assert(EspNativeSfxPcm_publishedSpan(1100,2500,3000,RING)==1400);
    assert(EspNativeSfxPcm_publishedSpan(1100,2500,500,RING)==500);
    assert(EspNativeSfxPcm_publishedSpan(2500,2500,500,RING)==0);
    assert(EspNativeSfxPcm_publishedSpan(2600,2500,500,RING)==0);
    assert(EspNativeSfxPcm_publishedSpan(100,100+RING+1,100+RING+1,RING)==0);
    assert(EspNativeSfxPcm_publishedSpan(UINT32_MAX-99U,20U,1000U,RING)==120U);
    concurrent_sector_one();
    puts("PASS AUDIO-382: 5046/5099/5069 complete after 420ms gap, music ring live");
    return 0;
}
