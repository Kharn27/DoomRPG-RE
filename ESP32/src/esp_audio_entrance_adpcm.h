#ifndef ESP_AUDIO_ENTRANCE_ADPCM_H
#define ESP_AUDIO_ENTRANCE_ADPCM_H
#include <stddef.h>
#include <stdint.h>

/* WAV IMA-ADPCM, mono, 22050 Hz only. Pure/host-testable, no SD or Arduino.
 * No dynamic memory. The producer decodes complete bounded ADPCM blocks.
 */
typedef struct EspEntranceAdpcmWav_s {
    uint32_t dataOffset;
    uint32_t dataBytes;
    uint32_t sampleFrames;
    uint16_t blockAlign;
    uint16_t samplesPerBlock;
} EspEntranceAdpcmWav;

/* Optional producer-only observation of decoded PCM16, before DAC scaling.
 * No instrumentation runs in the ISR and no audio samples are retained. */
typedef struct EspEntranceAdpcmDecodeStats_s {
    int16_t minPcm;
    int16_t maxPcm;
    uint32_t nonZeroPcm;
    uint32_t gainLimited;  /* PCM16 values limited before 200ms fade */
} EspEntranceAdpcmDecodeStats;

static inline void EspEntranceAdpcm_observePcm(
    EspEntranceAdpcmDecodeStats* stats,int16_t pcm) {
    if (!stats) return;
    if (pcm<stats->minPcm) stats->minPcm=pcm;
    if (pcm>stats->maxPcm) stats->maxPcm=pcm;
    if (pcm!=0) ++stats->nonZeroPcm;
    if (pcm < -2048 || pcm > 2048) ++stats->gainLimited;
}

static inline uint16_t EspEntranceAdpcm_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));
}
static inline uint32_t EspEntranceAdpcm_u32(const uint8_t* p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline int EspEntranceAdpcm_eq4(const uint8_t* p, const char* s) {
    return p[0]==(uint8_t)s[0]&&p[1]==(uint8_t)s[1]&&
           p[2]==(uint8_t)s[2]&&p[3]==(uint8_t)s[3];
}

/* Scans <=512 bytes, <=24 chunks and RIFF bounds. Explicitly reject
 * PCM16, stereo, 16000Hz, extensible WAV and unsupported block layouts.
 * Require valid fact sample count so padding from last ADPCM block is skipped. */
static inline int EspEntranceAdpcm_parse(const uint8_t* p, uint32_t headerBytes,
                                        uint32_t fileBytes,
                                        EspEntranceAdpcmWav* out) {
    if (!p||!out||headerBytes<48U||headerBytes>512U||
        headerBytes>fileBytes||fileBytes<48U||
        !EspEntranceAdpcm_eq4(p,"RIFF")||
        !EspEntranceAdpcm_eq4(p+8,"WAVE")) return 0;
    const uint32_t riff=EspEntranceAdpcm_u32(p+4);
    if (riff<40U||riff>fileBytes-8U) return 0;
    const uint32_t end=riff+8U;
    uint32_t at=12U, factFrames=0U, dataOffset=0U, dataBytes=0U;
    uint16_t align=0U, samples=0U;
    int fmtFound=0, factFound=0, dataFound=0;
    for (uint32_t i=0;i<24U;i++) {
        if (at>end||end-at<8U||at>headerBytes||
            headerBytes-at<8U) break;
        const uint8_t* c=p+at;
        const uint32_t length=EspEntranceAdpcm_u32(c+4);
        const uint32_t payload=at+8U;
        if (length>end-payload) return 0;
        if (EspEntranceAdpcm_eq4(c,"fmt ")) {
            if (fmtFound||length<20U||length>64U||
                length>headerBytes-payload) return 0;
            const uint8_t* f=p+payload;
            align=EspEntranceAdpcm_u16(f+12U);
            samples=EspEntranceAdpcm_u16(f+18U);
            if (EspEntranceAdpcm_u16(f)!=0x0011U || /* IMA ADPCM */
                EspEntranceAdpcm_u16(f+2U)!=1U ||
                EspEntranceAdpcm_u32(f+4U)!=22050U ||
                EspEntranceAdpcm_u32(f+8U)==0U ||
                EspEntranceAdpcm_u16(f+14U)!=4U ||
                EspEntranceAdpcm_u16(f+16U)<2U ||
                align<8U||align>1024U||samples<9U||
                samples!=(uint16_t)(1U+2U*(align-4U)))
                return 0;
            fmtFound=1;
        } else if (EspEntranceAdpcm_eq4(c,"fact")) {
            if (factFound||length<4U||length>headerBytes-payload)
                return 0;
            factFrames=EspEntranceAdpcm_u32(p+payload);
            if (factFrames==0U) return 0;
            factFound=1;
        } else if (EspEntranceAdpcm_eq4(c,"data")) {
            if (dataFound||!fmtFound||!factFound||
                length==0U||length%align!=0U||
                payload>headerBytes) return 0;
            dataOffset=payload;
            dataBytes=length;
            dataFound=1;
            break;
        }
        const uint32_t padded=length+(length&1U);
        if (padded<length||padded>end-payload) return 0;
        at=payload+padded;
    }
    if (!fmtFound||!factFound||!dataFound) return 0;
    const uint32_t blocks=dataBytes/align;
    const uint64_t maxFrames=(uint64_t)blocks*(uint64_t)samples;
    const uint64_t minFrames=(uint64_t)(blocks-1U)*(uint64_t)samples;
    if ((uint64_t)factFrames>maxFrames||
        (uint64_t)factFrames<=minFrames) return 0;
    out->dataOffset=dataOffset;
    out->dataBytes=dataBytes;
    out->sampleFrames=factFrames;
    out->blockAlign=align;
    out->samplesPerBlock=samples;
    return 1;
}

static inline int EspEntranceAdpcm_clamp(int v,int minV,int maxV) {
    return v<minV?minV:(v>maxV?maxV:v);
}
static inline uint8_t EspEntranceAdpcm_toDac(int16_t pcm,
                                            uint32_t firstFrame) {
    /* AUDIO-R2c: real early-track PCM16 was -1689..2252, which
     * collapsed near 128 in the original /512 output. Use /32 (~+24dB
     * over the original), clamp to +/-64 codes before the 200ms fade.
     * UI volume is applied by the main-loop producer, not in the ISR. */
    int32_t diff=(int32_t)pcm/32;
    diff=EspEntranceAdpcm_clamp(diff,-64,64);
    const uint32_t fade=firstFrame<4410U?firstFrame:4410U;
    diff=diff*(int32_t)fade/4410;
    return (uint8_t)(128+diff);
}
/* One complete mono block, <=1024 compressed bytes, <=2041 output samples.
 * caller output must have capacity >= samplesPerBlock.
 * All step/index updates and padding counts are explicit. */
static inline int EspEntranceAdpcm_decodeBlockObserved(
    const uint8_t* block,uint32_t n,uint16_t samplesPerBlock,
    uint8_t* out,uint32_t outCapacity,uint32_t firstFrame,
    EspEntranceAdpcmDecodeStats* stats) {
    if (!block||!out||n<8U||n>1024U||samplesPerBlock!=1U+2U*(n-4U)||
        outCapacity<samplesPerBlock) return 0;
    static const int16_t steps[89]={
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
        34,37,41,45,50,55,60,66,73,80,88,97,107,118,
        130,143,157,173,190,209,230,253,279,307,337,
        371,408,449,494,544,598,658,724,796,876,963,
        1060,1166,1282,1411,1552,1707,1878,2066,2272,
        2499,2749,3024,3327,3660,4026,4428,4871,5358,
        5894,6484,7132,7845,8630,9493,10442,11487,
        12635,13899,15289,16818,18500,20350,22385,
        24623,27086,29794,32767
    };
    static const int8_t offsets[16]={
        -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
    };
    int predictor=(int16_t)EspEntranceAdpcm_u16(block);
    int index=(int)block[2U];
    if (index>88||block[3U]!=0U) return 0;
    if (stats) {
        stats->minPcm=32767;
        stats->maxPcm=-32768;
        stats->nonZeroPcm=0U;
        stats->gainLimited=0U;
    }
    EspEntranceAdpcm_observePcm(stats,(int16_t)predictor);
    out[0]=EspEntranceAdpcm_toDac((int16_t)predictor,firstFrame);
    uint32_t produced=1U;
    for (uint32_t i=4U;i<n;i++) {
        const uint8_t packed=block[i];
        for (uint32_t half=0;half<2U;half++) {
            const int nibble=(packed>>(half*4U))&15;
            const int step=(int)steps[index];
            int delta=step>>3;
            if (nibble&1) delta+=step>>2;
            if (nibble&2) delta+=step>>1;
            if (nibble&4) delta+=step;
            predictor=EspEntranceAdpcm_clamp(
                predictor+((nibble&8)?-delta:delta),-32768,32767);
            index=EspEntranceAdpcm_clamp(index+offsets[nibble],0,88);
            EspEntranceAdpcm_observePcm(stats,(int16_t)predictor);
            if (produced>=outCapacity) return 0;
            out[produced]=EspEntranceAdpcm_toDac(
                (int16_t)predictor,firstFrame+produced);
            produced++;
        }
    }
    return produced==samplesPerBlock;
}
/* Preserve the original pure decoder API for existing host callers. */
static inline int EspEntranceAdpcm_decodeBlock(
    const uint8_t* block,uint32_t n,uint16_t samplesPerBlock,
    uint8_t* out,uint32_t outCapacity,uint32_t firstFrame) {
    return EspEntranceAdpcm_decodeBlockObserved(block,n,samplesPerBlock,
                                               out,outCapacity,firstFrame,NULL);
}
#endif
