#ifndef ESP_NATIVE_SFX_WAV_H
#define ESP_NATIVE_SFX_WAV_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* SFX-P0: bounded RIFF metadata scanner, NOT a playable-audio decoder.
 * Input bytes originate from the immutable native PAK (never runtime ZIP).
 * No heap, Arduino, SDL, SD, or sound output. */
typedef struct EspNativeSfxWavInfo_s {
    uint32_t sampleRate, byteRate, dataOffset, dataBytes, sampleFrames;
    uint16_t formatTag, channels, bitsPerSample, blockAlign, samplesPerBlock;
    uint8_t hasFact;
} EspNativeSfxWavInfo;
typedef enum EspNativeSfxWavResult_e {
    ESP_NATIVE_SFX_WAV_BAD=0,
    ESP_NATIVE_SFX_WAV_PCM=1,
    ESP_NATIVE_SFX_WAV_IMA_MONO=2,
    ESP_NATIVE_SFX_WAV_OTHER=3,
    ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR=4
} EspNativeSfxWavResult;

static inline uint16_t EspNativeSfxWav_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));
}
static inline uint32_t EspNativeSfxWav_u32(const uint8_t* p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline int EspNativeSfxWav_tag(const uint8_t* p,const char* t) {
    return p[0]==(uint8_t)t[0]&&p[1]==(uint8_t)t[1]&&
           p[2]==(uint8_t)t[2]&&p[3]==(uint8_t)t[3];
}
static inline const char* EspNativeSfxWav_status(EspNativeSfxWavResult r) {
    switch (r) {
        case ESP_NATIVE_SFX_WAV_PCM:return "pcm-candidate";
        case ESP_NATIVE_SFX_WAV_IMA_MONO:return "ima-candidate";
        case ESP_NATIVE_SFX_WAV_OTHER:return "unsupported";
        case ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR:return "header-too-far";
        default:return "invalid";
    }
}

/* At most 512 bytes of header, 24 RIFF chunks. File size comes from the
 * validated immutable PAK index. Data payload is NEVER inspected here.
 * Successful metadata classification does not enable playback. */
static inline EspNativeSfxWavResult EspNativeSfxWav_probe(
        const uint8_t* header,uint32_t headerBytes,uint32_t fileBytes,
        EspNativeSfxWavInfo* out) {
    if (!out) return ESP_NATIVE_SFX_WAV_BAD;
    memset(out,0,sizeof(*out));
    if (!header||headerBytes<12U||headerBytes>512U||
        fileBytes<44U||headerBytes>fileBytes||
        !EspNativeSfxWav_tag(header,"RIFF")||
        !EspNativeSfxWav_tag(header+8U,"WAVE"))
        return ESP_NATIVE_SFX_WAV_BAD;
    const uint64_t riffEnd=(uint64_t)EspNativeSfxWav_u32(header+4U)+8ULL;
    if (riffEnd<44ULL||riffEnd>(uint64_t)fileBytes)
        return ESP_NATIVE_SFX_WAV_BAD;
    uint64_t at=12ULL;
    int haveFmt=0,haveData=0;
    for (uint32_t i=0U;i<24U&&at+8ULL<=riffEnd;i++) {
        if (at+8ULL>(uint64_t)headerBytes)
            return ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR;
        const uint8_t* c=header+(uint32_t)at;
        const uint32_t length=EspNativeSfxWav_u32(c+4U);
        const uint64_t payload=at+8ULL;
        if (payload+(uint64_t)length>riffEnd)
            return ESP_NATIVE_SFX_WAV_BAD;
        if (EspNativeSfxWav_tag(c,"fmt ")) {
            if (haveFmt||length<16U)
                return ESP_NATIVE_SFX_WAV_BAD;
            if (payload+16ULL>(uint64_t)headerBytes)
                return ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR;
            const uint8_t* f=header+(uint32_t)payload;
            out->formatTag=EspNativeSfxWav_u16(f);
            out->channels=EspNativeSfxWav_u16(f+2U);
            out->sampleRate=EspNativeSfxWav_u32(f+4U);
            out->byteRate=EspNativeSfxWav_u32(f+8U);
            out->blockAlign=EspNativeSfxWav_u16(f+12U);
            out->bitsPerSample=EspNativeSfxWav_u16(f+14U);
            if (length>=20U) {
                if (payload+20ULL>(uint64_t)headerBytes)
                    return ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR;
                out->samplesPerBlock=EspNativeSfxWav_u16(f+18U);
            }
            haveFmt=1;
        } else if (EspNativeSfxWav_tag(c,"fact")) {
            if (length<4U) return ESP_NATIVE_SFX_WAV_BAD;
            if (payload+4ULL>(uint64_t)headerBytes)
                return ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR;
            out->sampleFrames=EspNativeSfxWav_u32(header+(uint32_t)payload);
            out->hasFact=1U;
        } else if (EspNativeSfxWav_tag(c,"data")) {
            if (haveData||!haveFmt||length==0U)
                return ESP_NATIVE_SFX_WAV_BAD;
            out->dataOffset=(uint32_t)payload;
            out->dataBytes=length;
            haveData=1;
            break;
        }
        const uint64_t next=payload+(uint64_t)length+
                            (uint64_t)(length&1U);
        if (next>riffEnd) return ESP_NATIVE_SFX_WAV_BAD;
        at=next;
    }
    if (!haveFmt||!haveData)
        return ((uint64_t)headerBytes<riffEnd)?
               ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR:ESP_NATIVE_SFX_WAV_BAD;
    if (out->sampleRate<8000U||out->sampleRate>48000U||
        out->channels==0U||out->blockAlign==0U)
        return ESP_NATIVE_SFX_WAV_OTHER;
    if (out->formatTag==1U&&out->channels<=2U&&
        (out->bitsPerSample==8U||out->bitsPerSample==16U)) {
        const uint32_t block=(uint32_t)out->channels*
                             (out->bitsPerSample/8U);
        if (out->blockAlign==block&&
            (uint64_t)out->byteRate==(uint64_t)out->sampleRate*block&&
            out->dataBytes%block==0U) {
            out->sampleFrames=out->dataBytes/block;
            return ESP_NATIVE_SFX_WAV_PCM;
        }
        return ESP_NATIVE_SFX_WAV_BAD;
    }
    if (out->formatTag==0x0011U&&out->channels==1U&&
        out->bitsPerSample==4U&&out->blockAlign>=8U&&
        out->blockAlign<=1024U&&out->hasFact&&
        out->dataBytes%out->blockAlign==0U) {
        const uint32_t samples=1U+2U*((uint32_t)out->blockAlign-4U);
        const uint64_t blocks=(uint64_t)out->dataBytes/out->blockAlign;
        if (out->samplesPerBlock==samples&&blocks>0U&&
            out->sampleFrames>(blocks-1ULL)*samples&&
            out->sampleFrames<=blocks*samples)
            return ESP_NATIVE_SFX_WAV_IMA_MONO;
        return ESP_NATIVE_SFX_WAV_BAD;
    }
    return ESP_NATIVE_SFX_WAV_OTHER;
}
#endif
