#include <Arduino.h>
#include <SD.h>
#include <driver/dac.h>
#include <driver/timer.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include <soc/rtc_io_reg.h>
#include <soc/soc.h>
#include <xtensa/core-macros.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_audio_entrance_adpcm.h"
#include "esp_audio_clock_guard.h"
#include "esp_native_sfx_pcm_mix.h"
#include "esp_native_sfx_wav.h"
#include "esp_asset_pack.h"
#include "esp_audio_entrance_music.h"
#include "esp_native_audio_intent.h" /* session music volume preset owner */
#include "esp_player_view_state.h"
#include "native_intro_clock.h" /* native ST_INTRO active/exit boundary */

/* AUDIO-R2a: native SD music, not a desktop player.
 * M_STALKS.wav (user-supplied Doom II recording, NEVER in Git/PAK):
 * RIFF/WAV IMA-ADPCM mono, 22050 Hz, blocks <=1024 compressed bytes.
 * Producer: a single bounded SD block read + decode on Arduino main loop.
 * Consumer: 22050 Hz IRAM timer interrupt, only DRAM and DAC2 register.
 * No I2S, DMA, ZIP, music synth, PSRAM, heap allocation or SD from ISR.
 */
namespace {
constexpr char kPrimary[]="/doomrpg/music/opl/M_STALKS.wav";
constexpr char kRootFallback[]="/M_STALKS.wav";
constexpr char kIntroPrimary[]="/doomrpg/music/opl/M_IN_CIT.wav";
constexpr char kIntroFallback[]="/M_IN_CIT.wav";
constexpr uint32_t kRate=22050U;
constexpr uint32_t kTimerDiv=2U;
constexpr uint32_t kTimerAlarm=1814U;    /* 80MHz /2 /1814 ≈ 22050.72Hz */
constexpr uint32_t kRingCapacity=16384U; /* 743ms @22050; 8-bit internal RAM */
constexpr uint32_t kRingMask=kRingCapacity-1U;
constexpr uint32_t kPrimeFrames=8192U;  /* 371ms preflight reserve */
constexpr uint32_t kResumeFrames=4096U; /* 186ms refill before restart */
constexpr uint32_t kMaxSilentFrames=8U*kRate; /* 8s -> fail closed */
constexpr uint32_t kHeaderBytes=512U;
constexpr uint32_t kMaxBlockBytes=1024U;
constexpr uint32_t kMaxBlockSamples=2041U;
/* Independent track mastering: intro source is much hotter than Entrance.
 * /32 vs /14 = 14/32 amplitude, ~-7.2 dB, BEFORE the soft knee. */
constexpr uint16_t kIntroGainDiv=32U;
constexpr uint16_t kEntranceGainDiv=14U;
constexpr uint32_t kHashFrames=4096U;      /* first music frames, producer==ISR */
constexpr uint32_t kFnvBasis=2166136261U;
constexpr timer_group_t kGroup=TIMER_GROUP_1;
constexpr timer_idx_t kTimer=TIMER_1;
static_assert((kRingCapacity&(kRingCapacity-1U))==0U,"ring power of two");
static_assert(kPrimeFrames+kMaxBlockSamples<kRingCapacity,"prime + block");

enum MusicPhase : uint8_t {
    kSilent=0U, kPrime=1U, kClock=2U, kPlay=3U
};

/* ISR shared variables. Consume/writer are monotonic; ring is SPSC. */
static volatile uint8_t phase=kSilent;
static volatile uint32_t rd;
static volatile uint32_t wr;
static volatile uint32_t clockTicks;
static volatile uint32_t playedFrames;
static volatile uint32_t underruns;   /* actual DAC midpoint frames after starvation */
static volatile uint32_t recoveryStarts;
static volatile uint32_t recovering;
static volatile uint32_t silentStreak;
static volatile uint32_t maxSilentStreak;
static volatile uint32_t prevCycle;
static volatile uint32_t maxGapCycles;
static volatile uint32_t gapsOver90us;
static volatile uint32_t maxIsrCycles;
/* Distinguish known-good synthesis, actual ring consumption and DAC writes.
 * ISR writes only small volatile counters and registers; no printing/SD. */
static volatile uint32_t ringConsumed;
static volatile uint32_t consumedMin;
static volatile uint32_t consumedMax;
static volatile uint32_t consumedNonMid;
static volatile uint32_t consumedOver8;
static volatile uint32_t consumedOver16;
static volatile uint32_t consumedPrefixFnv;
static volatile uint32_t dacWrittenMin;
static volatile uint32_t dacWrittenMax;
static uint8_t* ring;

/* Single game-loop owner. These buffers do NOT occupy the audio ISR stack. */
static uint8_t raw[kMaxBlockBytes];
static uint8_t decoded[kMaxBlockSamples];
static File musicFile;
static EspEntranceAdpcmWav wav={};
static const char* path=nullptr;
static const char* failure="none";
static bool attempted;
static bool activeIntroTrack;
static bool introPrestartWindow; /* main-task only, prior to ST_INTRO clock arm */
static bool fileOpen;
static bool timerInit;
static bool callbackAdded;
static bool dacEnabled;
static uint32_t blocksRead;
static uint32_t producedInLoop;
static uint32_t loops;
static uint32_t reads;
static uint32_t maxReadUs;
static uint32_t slowReadCount;
static uint32_t minBuffered;
static uint32_t heapBefore;
static int64_t clockStartUs;
static uint32_t playStartMs;
static uint32_t nextReportMs;
static uint32_t lastFrameAtMs;
static uint32_t lastFrameSeen;
static bool firstSignalReported;
/* PCM16 / DAC8 statistics gathered exclusively on the game-loop producer. */
static int16_t pcmMin;
static int16_t pcmMax;
static uint32_t pcmNonZero;
static uint32_t gainLimited;
static uint32_t producedDacMin;
static uint32_t producedDacMax;
static uint32_t producedNonMid;
static uint32_t producedFrames;
static uint32_t publishedPrefixFnv;
static uint32_t pumpCalls;
static uint32_t pumpBlockReads;
/* SFX-P1: one brief PCM16 effect, directly overlaid onto the existing
 * not-yet-consumed music ring. No second timer, ISR edits or WAV allocation. */
static uint32_t sfxSeenSequence;
static uint32_t sfxBusyUntil;
static uint32_t sfxPlayedCount;
static uint32_t sfxRejectedCount;
constexpr uint32_t kSfxLeadFrames=1024U; /* ~46ms collision guard */
constexpr uint32_t kSfxMinLead=128U;
constexpr uint32_t kSfxReadBytes=512U;
/* P2: bounded continuation for 5045, whose 17640 output samples exceed
 * one 16384-byte ring. The PAK entry is only immutable offsets, not a lease. */
struct FireSfxVoice {
    EspAssetPackEntry entry;
    uint32_t start,output,next,srcIndex,srcPhase,readCount;
    uint32_t clippedLow,clippedHigh;
    bool active;
};
static FireSfxVoice fireSfx={};
static uint32_t sfxDeferredSequence;


static bool IRAM_ATTR onAudioTick(void*) {
    const uint8_t mode=phase;
    if (mode==kClock) {
        /* R2f: account for *all* real timer interrupts, including time
         * spent rendering before the game loop polls calibration. A capped
         * count divided by a delayed wall clock creates false FAIL-CLOCK. */
        if (clockTicks!=UINT32_MAX) ++clockTicks;
    } else if (mode==kPlay) {
        const uint32_t start=xthal_get_ccount();
        const uint32_t prev=prevCycle;
        if (prev!=0U) {
            const uint32_t gap=start-prev;
            if (gap>maxGapCycles) maxGapCycles=gap;
            if (gap>22000U) ++gapsOver90us;
        }
        prevCycle=start;
        const uint32_t r=__atomic_load_n(&rd,__ATOMIC_RELAXED);
        const uint32_t w=__atomic_load_n(&wr,__ATOMIC_ACQUIRE);
        uint32_t code=128U;
        /* Real music starts at sample zero; no 440Hz hardware witness.
         * When starved, hold the DAC midpoint until the ring is primed
         * again. Neither the read pointer nor clock is reset on resume. */
        if (recovering && w-r>=kResumeFrames) recovering=0U;
        if (!recovering && r!=w && ring!=nullptr) {
            code=ring[r&kRingMask];
            __atomic_store_n(&rd,r+1U,__ATOMIC_RELEASE);
            const uint32_t count=ringConsumed;
            if (count<kHashFrames)
                consumedPrefixFnv=(consumedPrefixFnv^(uint8_t)code)*16777619U;
            ringConsumed=count+1U;
            if (code<consumedMin) consumedMin=code;
            if (code>consumedMax) consumedMax=code;
            if (code!=128U) ++consumedNonMid;
            if (code<120U||code>136U) ++consumedOver8;
            if (code<112U||code>144U) ++consumedOver16;
            silentStreak=0U;
        } else {
            if (!recovering) { recovering=1U; ++recoveryStarts; }
            ++underruns;
            ++silentStreak;
            if (silentStreak>maxSilentStreak)
                maxSilentStreak=silentStreak;
        }
        if (code<dacWrittenMin) dacWrittenMin=code;
        if (code>dacWrittenMax) dacWrittenMax=code;
        REG_SET_FIELD(RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC,code);
        ++playedFrames;
        const uint32_t cost=xthal_get_ccount()-start;
        if (cost>maxIsrCycles) maxIsrCycles=cost;
    }
    return false;
}
static uint32_t queued() {
    return __atomic_load_n(&wr,__ATOMIC_ACQUIRE)-
           __atomic_load_n(&rd,__ATOMIC_ACQUIRE);
}
/* Printed on game loop only, never on the 22.05kHz interrupt. */
static void signalReport(const char* at) {
    printf("[ENTRANCEMUSIC] SIGNAL at=%s ringConsumed=%u "
           "isrRingMinMax=%u..%u isrNon128=%u isrOver8=%u "
           "isrOver16=%u dacWrittenMinMax=%u..%u "
           "pcm16MinMax=%d..%d pcm16NonZero=%u softKneeFrames=%u "
           "masterVolume=%u "
           "publishedDacMinMax=%u..%u publishedNon128=%u "
           "producerFrames=%u prefix4096Published=%08x "
           "prefix4096Consumed=%08x queued=%u underruns=%u "
           "recoveryStarts=%u recovering=%u maxSilentFrames=%u\n",
           at,(unsigned)ringConsumed,
           (unsigned)consumedMin,(unsigned)consumedMax,
           (unsigned)consumedNonMid,(unsigned)consumedOver8,
           (unsigned)consumedOver16,
           (unsigned)dacWrittenMin,(unsigned)dacWrittenMax,
           (int)pcmMin,(int)pcmMax,(unsigned)pcmNonZero,
           (unsigned)gainLimited,
           (unsigned)EspNativeAudioIntent_musicVolumePercent(),
           (unsigned)producedDacMin,(unsigned)producedDacMax,
           (unsigned)producedNonMid,(unsigned)producedFrames,
           (unsigned)publishedPrefixFnv,(unsigned)consumedPrefixFnv,
           (unsigned)queued(),(unsigned)underruns,
           (unsigned)recoveryStarts,(unsigned)recovering,
           (unsigned)maxSilentStreak);
}
static void closeMusic(const char* reason) {
    phase=kSilent;
    fireSfx.active=false;
    sfxDeferredSequence=0U;
    if (timerInit) (void)timer_pause(kGroup,kTimer);
    if (callbackAdded) (void)timer_isr_callback_remove(kGroup,kTimer);
    callbackAdded=false;
    if (timerInit) (void)timer_deinit(kGroup,kTimer);
    timerInit=false;
    if (playedFrames>0U) signalReport("FINAL");
    if (dacEnabled) {
        (void)dac_output_voltage(DAC_CHANNEL_2,128U);
        (void)dac_output_disable(DAC_CHANNEL_2);
    }
    dacEnabled=false;
    if (fileOpen) musicFile.close();
    fileOpen=false;
    if (ring) heap_caps_free(ring);
    ring=nullptr;
    printf("[ENTRANCEMUSIC] FINAL reason=%s path=%s frames=%u "
           "loops=%u underruns=%u maxGapCycles=%u gapsOver90us=%u "
           "maxIsrCycles=%u recoveries=%u maxSilentFrames=%u "
           "reads=%u maxSdReadUs=%u pumpCalls=%u pumpReads=%u "
           "slowReadsOver20ms=%u heap8Before=%u heap8After=%u "
           "largest8=%u timer=OFF dacPad=OFF file=CLOSED\n",
           reason,path?path:"none",(unsigned)playedFrames,
           (unsigned)loops,(unsigned)underruns,(unsigned)maxGapCycles,
           (unsigned)gapsOver90us,(unsigned)maxIsrCycles,
           (unsigned)recoveryStarts,(unsigned)maxSilentStreak,
           (unsigned)reads,(unsigned)maxReadUs,
           (unsigned)pumpCalls,(unsigned)pumpBlockReads,
           (unsigned)slowReadCount,(unsigned)heapBefore,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
/* A single compressed block per main-loop iteration, <=1024 bytes.
 * One decoding operation yields at most 2041 DAC samples. The renderer,
 * PAK owner and SD music File all live on the SAME task. */
static bool refill() {
    const uint32_t q=queued();
    if (q>kRingCapacity) { failure="ring_bounds";return false; }
    if (kRingCapacity-q<wav.samplesPerBlock) return true;
    if (producedInLoop==wav.sampleFrames) {
        if (!musicFile.seek(wav.dataOffset)) {
            failure="rewind_failed";return false;
        }
        blocksRead=0U;
        producedInLoop=0U;
        ++loops;
    }
    const uint32_t maxBlocks=wav.dataBytes/wav.blockAlign;
    if (blocksRead>=maxBlocks) {
        failure="fact_data_mismatch";return false;
    }
    const int64_t beginUs=esp_timer_get_time();
    const int got=musicFile.read(raw,wav.blockAlign);
    const int64_t elapsedUs=esp_timer_get_time()-beginUs;
    if (elapsedUs>(int64_t)maxReadUs) maxReadUs=(uint32_t)elapsedUs;
    if (elapsedUs>20000LL) ++slowReadCount;
    ++reads;
    if (got!=(int)wav.blockAlign) {
        failure="sd_short_read";return false;
    }
    const uint32_t available=wav.sampleFrames-producedInLoop;
    const uint32_t samples=available<wav.samplesPerBlock?
                           available:wav.samplesPerBlock;
    const uint32_t fadeFrame=(loops==0U)?producedInLoop:4410U;
    EspEntranceAdpcmDecodeStats stats={};
    const uint16_t gainDiv=activeIntroTrack?kIntroGainDiv:kEntranceGainDiv;
    if (!EspEntranceAdpcm_decodeBlockObservedGain(raw,wav.blockAlign,
        wav.samplesPerBlock,decoded,kMaxBlockSamples,fadeFrame,
        gainDiv,&stats)) {
        failure="adpcm_decode";return false;
    }
    if (stats.minPcm<pcmMin) pcmMin=stats.minPcm;
    if (stats.maxPcm>pcmMax) pcmMax=stats.maxPcm;
    pcmNonZero+=stats.nonZeroPcm;
    gainLimited+=stats.gainLimited;
    if (blocksRead==0U && loops==0U) {
        printf("[ENTRANCEMUSIC] BLOCK0 predictor=%d stepIndex=%u "
               "pcm16MinMax=%d..%d dacFirst=%u dacLast=%u "
               "softKneeFrames=%u gainDiv=%u peakCodes=112 track=%s\n",
               (int)(int16_t)EspEntranceAdpcm_u16(raw),
               (unsigned)raw[2],(int)stats.minPcm,(int)stats.maxPcm,
               (unsigned)decoded[0],(unsigned)decoded[samples-1U],
               (unsigned)stats.gainLimited,(unsigned)gainDiv,
               activeIntroTrack?"INTRO":"ENTRANCE");
    }
    const uint32_t start=__atomic_load_n(&wr,__ATOMIC_RELAXED);
    const uint8_t volume=EspNativeAudioIntent_musicVolumePercent();
    if (volume>100U) {failure="invalid_volume";return false;}
    for (uint32_t i=0U;i<samples;i++) {
        /* Main-loop only: never run volume scaling in the 22050Hz ISR. */
        const int32_t diff=(int32_t)decoded[i]-128;
        const uint8_t code=(uint8_t)(128+(diff*(int32_t)volume)/100);
        ring[(start+i)&kRingMask]=code;
        if (code<producedDacMin) producedDacMin=code;
        if (code>producedDacMax) producedDacMax=code;
        if (code!=128U) ++producedNonMid;
        if (producedFrames<kHashFrames)
            publishedPrefixFnv=(publishedPrefixFnv^code)*16777619U;
        ++producedFrames;
    }
    __atomic_store_n(&wr,start+samples,__ATOMIC_RELEASE);
    producedInLoop+=samples;
    ++blocksRead;
    return true;
}
/* SFX-P1: emit one actual original short PCM effect immediately after its
 * native gameplay intent. All reads happen here in the producer task, when
 * gameplay has closed its PAK lease. The 16KiB ring is shared with music:
 * overlay at rd+1024 (~46ms), NOT at wr (+~740ms latency).
 * Fail closed when the entire short SFX cannot fit ahead of the reader.
 * More SFX codecs, long effects, overlap, priorities and STOP_ALL belong to
 * later milestones, not this first real audible test. */
/* P2: Fire Ext WAV 5045, pure PCM16 PAK flash. Allocate nothing.
 * A short slice is mixed only into the future writer-published DAC bytes.
 * The main loop continues until all 17640 samples have been mixed. */
static void mixFireSfx() {
    if (!fireSfx.active || EspAssetPack_isOpen()) return;
    const uint32_t reader=__atomic_load_n(&rd,__ATOMIC_ACQUIRE);
    const uint32_t writer=__atomic_load_n(&wr,__ATOMIC_ACQUIRE);
    const uint32_t at=fireSfx.start+fireSfx.next;
    const uint32_t available=(int32_t)(writer-at)>0?writer-at:0U;
    const uint32_t remain=fireSfx.output-fireSfx.next;
    const uint32_t budget=available<remain?available:remain;
    if ((int32_t)(at-reader)<(int32_t)kSfxMinLead) {
        fireSfx.active=false;
        ++sfxRejectedCount;
        printf("[SFXP2] PARTIAL id=5045 reason=reader-caught-up totalMixed=%u\n",
               (unsigned)fireSfx.next);
        return;
    }
    if (budget==0U) return;
    if (!EspAssetPack_isMapFlashActive() ||
        !EspAssetPack_isResident() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        fireSfx.active=false;
        ++sfxRejectedCount;
        printf("[SFXP2] PARTIAL id=5045 reason=pak-unavailable totalMixed=%u\n",
               (unsigned)fireSfx.next);
        return;
    }
    uint8_t chunk[kSfxReadBytes];
    uint32_t cachedStart=UINT32_MAX,cachedFrames=0U,done=0U,reads=0U;
    const int64_t begun=esp_timer_get_time();
    const char* failure=nullptr;
    for (uint32_t i=0U;i<budget;i++) {
        if ((i&63U)==0U &&
            (int32_t)((fireSfx.start+fireSfx.next+i)-
                __atomic_load_n(&rd,__ATOMIC_ACQUIRE))<
                (int32_t)kSfxMinLead) {
            failure="reader-caught-up";break;
        }
        if (fireSfx.srcIndex>=6400U) {
            failure="source-bounds";break;
        }
        if (cachedStart==UINT32_MAX ||
            fireSfx.srcIndex<cachedStart ||
            fireSfx.srcIndex>=cachedStart+cachedFrames) {
            cachedStart=fireSfx.srcIndex;
            const uint32_t left=6400U-fireSfx.srcIndex;
            cachedFrames=left<(kSfxReadBytes/2U)?
                left:(kSfxReadBytes/2U);
            if (!EspAssetPack_readRange(&fireSfx.entry,
                    44U+fireSfx.srcIndex*2U,
                    chunk,cachedFrames*2U)) {
                failure="flash-read";break;
            }
            ++reads;
        }
        const int16_t pcm=EspNativeSfxPcm_le16(
            chunk+(fireSfx.srcIndex-cachedStart)*2U);
        const uint32_t slot=(fireSfx.start+fireSfx.next+i)&kRingMask;
        const int32_t unclamped=EspNativeSfxPcm_unclampedCode(
            ring[slot],pcm,5045U);
        if (unclamped<16) ++fireSfx.clippedLow;
        else if (unclamped>240) ++fireSfx.clippedHigh;
        ring[slot]=EspNativeSfxPcm_mixGain(ring[slot],pcm,5045U);
        ++done;
        fireSfx.srcPhase+=8000U;
        if (fireSfx.srcPhase>=kRate) {
            fireSfx.srcPhase-=kRate;
            ++fireSfx.srcIndex;
        }
    }
    EspAssetPack_close();
    fireSfx.next+=done;
    fireSfx.readCount+=reads;
    const bool complete=failure==nullptr && fireSfx.next==fireSfx.output;
    if (complete) {
        fireSfx.active=false;
        ++sfxPlayedCount;
    } else if (failure!=nullptr) {
        fireSfx.active=false;
        ++sfxRejectedCount;
    }
    printf("[SFXP2] %s id=5045 format=PCM16-mono-8000 "
           "outputFrames=%u mixed=%u totalMixed=%u gainDiv=%u "
           "readCalls=%u mixUs=%lld musicRing=shared heapAlloc=0%s%s\n",
           failure!=nullptr?"PARTIAL":complete?"MIX":"CHUNK",
           (unsigned)fireSfx.output,(unsigned)done,
           (unsigned)fireSfx.next,
           (unsigned)EspNativeSfxPcm_gainDiv(5045U),
           (unsigned)fireSfx.readCount,
           (long long)(esp_timer_get_time()-begun),
           failure!=nullptr?" reason=":"",
           failure!=nullptr?failure:"");
    if (complete || failure!=nullptr) {
        printf("[SFXGAIN] id=5045 mixed=%u clippedLow=%u "
               "clippedHigh=%u limits=16..240 source=producer-precise\n",
               (unsigned)fireSfx.next,(unsigned)fireSfx.clippedLow,
               (unsigned)fireSfx.clippedHigh);
    }
}
static void startFireSfx(uint32_t reader,uint32_t writer) {
    if (writer-reader>kRingCapacity ||
        writer-reader<kSfxLeadFrames+kSfxMinLead ||
        !EspAssetPack_isMapFlashActive() ||
        !EspAssetPack_isResident() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        ++sfxRejectedCount;
        printf("[SFXP2] DROP id=5045 reason=pak-or-ring-not-ready\n");
        return;
    }
    EspAssetPackEntry entry={};
    EspNativeSfxWavInfo meta={};
    uint8_t header[512];
    bool valid=EspAssetPack_findEntry("5045.wav",&entry)!=0 &&
               (entry.flags&ESP_ASSET_PACK_FLAG_DIRECTORY)==0U &&
               entry.size>=44U;
    if (valid) {
        const uint32_t count=entry.size<sizeof(header)?
                             entry.size:(uint32_t)sizeof(header);
        valid=EspAssetPack_readRange(&entry,0U,header,count)!=0 &&
              EspNativeSfxWav_probe(header,count,entry.size,&meta)==
                  ESP_NATIVE_SFX_WAV_PCM &&
              meta.formatTag==1U && meta.channels==1U &&
              meta.sampleRate==8000U && meta.bitsPerSample==16U &&
              meta.blockAlign==2U && meta.dataOffset==44U &&
              meta.sampleFrames==6400U && meta.dataBytes==12800U &&
              EspNativeSfxPcm_outputFrames(meta.sampleFrames,kRate)==17640U;
    }
    if (valid) {
        fireSfx.entry=entry;
        fireSfx.start=reader+kSfxLeadFrames;
        fireSfx.output=17640U;
        fireSfx.next=0U;
        fireSfx.srcIndex=0U;
        fireSfx.srcPhase=0U;
        fireSfx.readCount=0U;
        fireSfx.clippedLow=0U;
        fireSfx.clippedHigh=0U;
        fireSfx.active=true;
        sfxBusyUntil=fireSfx.start+fireSfx.output;
    } else {
        ++sfxRejectedCount;
        printf("[SFXP2] DROP id=5045 reason=invalid-original-pcm\n");
    }
    EspAssetPack_close();
}

static void mixPendingSfx() {
    if (phase!=kPlay || ring==nullptr) return;
    if (fireSfx.active) mixFireSfx();
    const EspNativeAudioIntent* intent=EspNativeAudioIntent_last();
    if (intent==nullptr || intent->sequence==sfxSeenSequence) return;
    const uint16_t id=intent->resourceId;
    if ((id!=5045U && id!=5046U && id!=5060U) ||
        (intent->flags&ESP_NATIVE_AUDIO_STOP_ALL_FLAG)!=0U) {
        sfxSeenSequence=intent->sequence;
        return;
    }
    if (EspAssetPack_isOpen()) {
        if (sfxDeferredSequence!=intent->sequence) {
            sfxDeferredSequence=intent->sequence;
            printf("[SFXP2] DEFER id=%u reason=pack-lease-open\n",
                   (unsigned)id);
        }
        return;
    }
    sfxDeferredSequence=0U;
    sfxSeenSequence=intent->sequence;
    const uint32_t reader=__atomic_load_n(&rd,__ATOMIC_ACQUIRE);
    const uint32_t writer=__atomic_load_n(&wr,__ATOMIC_ACQUIRE);
    if (id==5045U) {
        if (fireSfx.active || (int32_t)(sfxBusyUntil-reader)>0) {
            ++sfxRejectedCount;
            printf("[SFXP2] DROP id=5045 reason=one-channel-busy\n");
        } else {
            startFireSfx(reader,writer);
            mixFireSfx();
        }
        return;
    }
    if (fireSfx.active || (int32_t)(sfxBusyUntil-reader)>0) {
        ++sfxRejectedCount;
        printf("[SFXP1] DROP id=%u reason=one-channel-busy queued=%u\n",
               (unsigned)id,(unsigned)(writer-reader));
        return;
    }
    if (writer-reader>kRingCapacity || !EspAssetPack_isMapFlashActive() ||
        !EspAssetPack_isResident() || EspAssetPack_isOpen()) {
        ++sfxRejectedCount;
        printf("[SFXP1] DROP id=%u reason=pack-lease-or-ring-not-ready\n",
               (unsigned)id);
        return;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        ++sfxRejectedCount;
        printf("[SFXP1] DROP id=%u reason=pak-open-failed\n",(unsigned)id);
        return;
    }
    char filename[16];
    (void)snprintf(filename,sizeof(filename),"%04u.wav",(unsigned)id);
    EspAssetPackEntry entry={};
    EspNativeSfxWavInfo meta={};
    uint8_t header[512];
    bool valid=EspAssetPack_findEntry(filename,&entry)!=0 &&
               (entry.flags&ESP_ASSET_PACK_FLAG_DIRECTORY)==0U &&
               entry.size>=44U;
    if (valid) {
        const uint32_t count=entry.size<sizeof(header)?
                             entry.size:(uint32_t)sizeof(header);
        valid=EspAssetPack_readRange(&entry,0U,header,count)!=0 &&
              EspNativeSfxWav_probe(header,count,entry.size,&meta)==
                  ESP_NATIVE_SFX_WAV_PCM &&
              meta.formatTag==1U && meta.channels==1U &&
              meta.sampleRate==8000U && meta.bitsPerSample==16U &&
              meta.blockAlign==2U && meta.dataOffset==44U &&
              meta.dataBytes==meta.sampleFrames*2U;
    }
    const uint32_t output=valid?
        EspNativeSfxPcm_outputFrames(meta.sampleFrames,kRate):0U;
    if (!valid || output==0U ||
        output+kSfxLeadFrames>kRingCapacity ||
        writer-reader<output+kSfxLeadFrames) {
        EspAssetPack_close();
        ++sfxRejectedCount;
        printf("[SFXP1] DROP id=%u reason=%s sourceFrames=%u "
               "outputFrames=%u queued=%u\n",
               (unsigned)id,valid?"not-entirely-buffered":"invalid-pcm16",
               (unsigned)meta.sampleFrames,(unsigned)output,
               (unsigned)(writer-reader));
        return;
    }
    const uint32_t start=reader+kSfxLeadFrames;
    uint8_t chunk[kSfxReadBytes];
    uint32_t cachedStart=UINT32_MAX;
    uint32_t cachedFrames=0U;
    uint32_t srcIndex=0U;
    uint32_t srcPhase=0U;
    uint32_t mixed=0U;
    uint32_t cacheReads=0U;
    uint32_t clippedLow=0U,clippedHigh=0U;
    int16_t srcMin=32767,srcMax=-32768;
    const int64_t begun=esp_timer_get_time();
    const char* failed=nullptr;
    for (uint32_t i=0U;i<output;i++) {
        /* ISR only reads increasing absolute indices. Keep ahead of it
         * while the main task visits flash; never write a live DAC byte. */
        if ((i&63U)==0U) {
            const uint32_t liveRead=__atomic_load_n(&rd,__ATOMIC_ACQUIRE);
            if ((int32_t)((start+i)-liveRead)<(int32_t)kSfxMinLead) {
                failed="reader-caught-up";
                break;
            }
        }
        if (srcIndex>=meta.sampleFrames) {
            failed="source-bounds";
            break;
        }
        if (cachedStart==UINT32_MAX ||
            srcIndex<cachedStart ||
            srcIndex>=cachedStart+cachedFrames) {
            cachedStart=srcIndex;
            const uint32_t left=meta.sampleFrames-srcIndex;
            cachedFrames=left<(kSfxReadBytes/2U)?
                         left:(kSfxReadBytes/2U);
            if (!EspAssetPack_readRange(&entry,
                    meta.dataOffset+srcIndex*2U,
                    chunk,cachedFrames*2U)) {
                failed="flash-range-read";
                break;
            }
            ++cacheReads;
        }
        const int16_t pcm=EspNativeSfxPcm_le16(
            chunk+(srcIndex-cachedStart)*2U);
        if (pcm<srcMin) srcMin=pcm;
        if (pcm>srcMax) srcMax=pcm;
        const uint32_t slot=(start+i)&kRingMask;
        const int32_t unclamped=EspNativeSfxPcm_unclampedCode(
            ring[slot],pcm,id);
        if (unclamped<16) ++clippedLow;
        else if (unclamped>240) ++clippedHigh;
        ring[slot]=EspNativeSfxPcm_mixGain(ring[slot],pcm,id);
        ++mixed;
        srcPhase+=8000U;
        if (srcPhase>=kRate) {
            srcPhase-=kRate;
            ++srcIndex;
        }
    }
    EspAssetPack_close();
    if (mixed!=0U) sfxBusyUntil=start+mixed;
    if (failed!=nullptr || mixed!=output) ++sfxRejectedCount;
    else ++sfxPlayedCount;
    printf("[SFXP1] %s id=%u name=%s source=verified-flash-pak "
           "format=PCM16-mono-8000 sampleFrames=%u outputFrames=%u "
           "mixed=%u leadFrames=%u reads=%u pcmMinMax=%d..%d "
           "mixUs=%lld channel=1 heapAlloc=0 musicRing=shared "
           "dacTimer=unchanged totalPlayed=%u rejected=%u%s%s\n",
           failed!=nullptr||mixed!=output?"PARTIAL":"MIX",
           (unsigned)id,filename,(unsigned)meta.sampleFrames,
           (unsigned)output,(unsigned)mixed,(unsigned)kSfxLeadFrames,
           (unsigned)cacheReads,(int)srcMin,(int)srcMax,
           (long long)(esp_timer_get_time()-begun),
           (unsigned)sfxPlayedCount,(unsigned)sfxRejectedCount,
           failed!=nullptr?" reason=":"",failed!=nullptr?failed:"");
    printf("[SFXGAIN] id=%u mixed=%u clippedLow=%u "
           "clippedHigh=%u limits=16..240 source=producer-precise\n",
           (unsigned)id,(unsigned)mixed,(unsigned)clippedLow,
           (unsigned)clippedHigh);
}

static bool startClock() {
    timer_config_t cfg={};
    cfg.divider=kTimerDiv;
    cfg.counter_dir=TIMER_COUNT_UP;
    cfg.counter_en=TIMER_PAUSE;
    cfg.alarm_en=TIMER_ALARM_EN;
    cfg.auto_reload=TIMER_AUTORELOAD_EN;
    cfg.intr_type=TIMER_INTR_LEVEL;
    if (timer_init(kGroup,kTimer,&cfg)!=ESP_OK) {
        failure="timer_init";return false;
    }
    timerInit=true;
    if (timer_set_counter_value(kGroup,kTimer,0ULL)!=ESP_OK ||
        timer_set_alarm_value(kGroup,kTimer,kTimerAlarm)!=ESP_OK ||
        timer_enable_intr(kGroup,kTimer)!=ESP_OK) {
        failure="timer_config";return false;
    }
    if (timer_isr_callback_add(kGroup,kTimer,onAudioTick,nullptr,
        ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL1)!=ESP_OK) {
        failure="timer_callback";return false;
    }
    callbackAdded=true;
    clockTicks=0U;
    phase=kClock;
    clockStartUs=esp_timer_get_time();
    if (timer_start(kGroup,kTimer)!=ESP_OK) {
        failure="clock_start";return false;
    }
    printf("[ENTRANCEMUSIC] PREFLIGHT timer=TG1T1 dacStream=OFF dacPad=%s "
           "targetHz=22050 clockDiv=2 alarm=1814 ringQueued=%u "
           "guard=20900..23200\n",dacEnabled?"ON":"OFF",
           (unsigned)queued());
    return true;
}
static bool startPlaying() {
    /* First DAC tick consumes the already-primed SD music directly. */
    if (dac_output_enable(DAC_CHANNEL_2)!=ESP_OK) {
        failure="dac_enable";return false;
    }
    dacEnabled=true;
    if (dac_output_voltage(DAC_CHANNEL_2,128U)!=ESP_OK ||
        timer_set_counter_value(kGroup,kTimer,0ULL)!=ESP_OK) {
        failure="dac_midpoint";return false;
    }
    playedFrames=0U;
    underruns=0U;
    recoveryStarts=0U;
    recovering=0U;
    silentStreak=0U;
    maxSilentStreak=0U;
    ringConsumed=0U;
    consumedMin=255U;
    consumedMax=0U;
    consumedNonMid=0U;
    consumedOver8=0U;
    consumedOver16=0U;
    consumedPrefixFnv=kFnvBasis;
    dacWrittenMin=255U;
    dacWrittenMax=0U;
    firstSignalReported=false;
    prevCycle=0U;
    maxGapCycles=0U;
    gapsOver90us=0U;
    maxIsrCycles=0U;
    phase=kPlay;
    playStartMs=millis();
    nextReportMs=playStartMs+10000U;
    lastFrameAtMs=playStartMs;
    lastFrameSeen=0U;
    if (timer_start(kGroup,kTimer)!=ESP_OK) {
        failure="timer_play";return false;
    }
    printf("[ENTRANCEMUSIC] PLAY path=%s backend=timer-DAC2 "
           "format=IMA-ADPCM monoHz=22050 ring=%u primed=%u "
           "dacPeakCodes=112 gainDiv=%u track=%s softKneeAt=96 masterVolume=%u witness=none source=SD-ring "
           "dacEnabled=yes gpio25=untouched noI2S=yes noPSRAM=yes\n",
           path,(unsigned)kRingCapacity,(unsigned)queued(),
           (unsigned)(activeIntroTrack?kIntroGainDiv:kEntranceGainDiv),
           activeIntroTrack?"INTRO":"ENTRANCE",
           (unsigned)EspNativeAudioIntent_musicVolumePercent());
    return true;
}
static void begin() {
    attempted=true;
    failure="none";
    heapBefore=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (heapBefore<61440U||largest<24576U) {
        printf("[ENTRANCEMUSIC] SILENT reason=heap_guard "
               "heap8=%u largest8=%u\n",
               (unsigned)heapBefore,(unsigned)largest);
        return;
    }
    activeIntroTrack=Esp32IntroClock_isActive()!=0||introPrestartWindow;
    const char* requested=activeIntroTrack?kIntroPrimary:kPrimary;
    const char* fallback=activeIntroTrack?kIntroFallback:kRootFallback;
    path=requested;
    printf("[ENTRANCEMUSIC] OPEN path=%s fallback=%s "
           "expected=WAV-IMA-ADPCM-mono-22050\n",requested,fallback);
    musicFile=SD.open(requested,FILE_READ);
    if (!musicFile) {
        path=fallback;
        musicFile=SD.open(fallback,FILE_READ);
    }
    if (!musicFile) {
        printf("[ENTRANCEMUSIC] SILENT reason=missing_file "
               "gameplay=unaffected\n");
        return;
    }
    fileOpen=true;
    const uint32_t fileBytes=(uint32_t)musicFile.size();
    uint8_t header[kHeaderBytes];
    const uint32_t n=fileBytes<kHeaderBytes?fileBytes:kHeaderBytes;
    const int got=musicFile.read(header,n);
    if (got!=(int)n ||
        !EspEntranceAdpcm_parse(header,n,fileBytes,&wav) ||
        !musicFile.seek(wav.dataOffset)) {
        printf("[ENTRANCEMUSIC] SILENT reason=invalid_wav path=%s "
               "fileBytes=%u headerRead=%d "
               "expected=IMA-ADPCM-mono-22050\n",
               path,(unsigned)fileBytes,got);
        musicFile.close();
        fileOpen=false;
        return;
    }
    printf("[ENTRANCEMUSIC] WAV_OK path=%s frames=%u "
           "durationMs=%u dataBytes=%u dataOffset=%u "
           "blockAlign=%u samplesPerBlock=%u\n",
           path,(unsigned)wav.sampleFrames,
           (unsigned)((uint64_t)wav.sampleFrames*1000ULL/kRate),
           (unsigned)wav.dataBytes,(unsigned)wav.dataOffset,
           (unsigned)wav.blockAlign,(unsigned)wav.samplesPerBlock);

    ring=(uint8_t*)heap_caps_malloc(
        kRingCapacity,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if (!ring ||
        heap_caps_get_free_size(MALLOC_CAP_8BIT)<49152U ||
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)<16384U) {
        closeMusic("FAIL-HEAP");
        return;
    }
    __atomic_store_n(&rd,0U,__ATOMIC_RELEASE);
    __atomic_store_n(&wr,0U,__ATOMIC_RELEASE);
    blocksRead=0U;
    producedInLoop=0U;
    loops=0U;
    reads=0U;
    maxReadUs=0U;
    slowReadCount=0U;
    minBuffered=kRingCapacity;
    pcmMin=32767;
    pcmMax=-32768;
    pcmNonZero=0U;
    gainLimited=0U;
    producedDacMin=255U;
    producedDacMax=0U;
    producedNonMid=0U;
    producedFrames=0U;
    publishedPrefixFnv=kFnvBasis;
    pumpCalls=0U;
    pumpBlockReads=0U;
    sfxBusyUntil=0U;
    fireSfx.active=false;
    sfxDeferredSequence=0U;
    sfxPlayedCount=0U;
    sfxRejectedCount=0U;
    playedFrames=0U;
    underruns=0U;
    phase=kPrime;
    printf("[ENTRANCEMUSIC] PRIMING ringBytes=%u targetFrames=%u "
           "heap8=%u largest8=%u\n",
           (unsigned)kRingCapacity,(unsigned)kPrimeFrames,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
} // namespace

int EspEntranceMusic_prestartIntroBeforeFirstFrame(void) {
    /* I3: the menu action has already finished ST_INTRO asset preparation,
     * but first frame and clock have not been armed. One synchronous bounded
     * preflight, one existing ring, one SD file, and NO concurrent SPI owner.
     * Always fail softly into the original visible intro on any fault. */
    if (!EspNativeAudioIntent_isSoundEnabled()) {
        printf("[INTROMUSIC] PRESTART skipped reason=SOUND_OFF firstFrame=allowed\n");
        return 0;
    }
    if (EspAssetPack_isOpen() || Esp32IntroClock_isActive() ||
        phase!=kSilent || ring!=nullptr || fileOpen || timerInit) {
        printf("[INTROMUSIC] PRESTART skipped reason=owner_not_idle firstFrame=allowed\n");
        return 0;
    }
    const uint32_t startMs=millis();
    introPrestartWindow=true;
    while ((uint32_t)(millis()-startMs)<1800U) {
        EspEntranceMusic_service(false,true);
        if (phase==kPlay) break;
        if (phase==kSilent && attempted) break; /* format/memory/file/clock */
        delay(1); /* Arduino task yields; no spin in timer ISR */
    }
    const int ready=(phase==kPlay && activeIntroTrack);
    if (!ready && (ring!=nullptr || fileOpen || timerInit))
        closeMusic("INTRO_PRESTART_TIMEOUT");
    introPrestartWindow=false;
    /* Attempt latch stays true on failure: do not resume late in ST_INTRO. */
    if (!ready) attempted=true;
    printf("[INTROMUSIC] PRESTART ready=%d elapsedMs=%u firstFrame=NOT_PRESENTED "
           "phase=%u queued=%u ringBytes=%u source=one-SD-owner\n",
           ready,(unsigned)(millis()-startMs),(unsigned)phase,
           (unsigned)(ring!=nullptr?queued():0U),
           (unsigned)(ring!=nullptr?kRingCapacity:0U));
    return ready;
}

void EspEntranceMusic_syncIdleOutput(void) {
    /* No timer or ring in menus. Hold DAC2 at the exact digital midpoint.
     * Never modify the live timer's DAC output in gameplay. */
    if (phase!=kSilent || timerInit || ring!=nullptr) return;
    const int enabled=EspNativeAudioIntent_isSoundEnabled();
    if (!enabled) {
        if (dacEnabled) {
            (void)dac_output_voltage(DAC_CHANNEL_2,128U);
            (void)dac_output_disable(DAC_CHANNEL_2);
            dacEnabled=false;
            printf("[AUDIOIDLE] OFF dacPad=OFF timer=OFF music=STOPPED\n");
        }
        return;
    }
    if (!dacEnabled) {
        if (dac_output_enable(DAC_CHANNEL_2)!=ESP_OK) {
            printf("[AUDIOIDLE] FAILED reason=dac-enable timer=OFF\n");
            return;
        }
        dacEnabled=true;
        (void)dac_output_voltage(DAC_CHANNEL_2,128U);
        printf("[AUDIOIDLE] ON dacPad=ON code=128 timer=OFF buffer=0 music=STOPPED\n");
    }
}

void EspEntranceMusic_stopIntroAtHandoff(void) {
    /* Called before the parked intro clock can start BSP/map-flash bootstrap.
     * Never interrupt a still-active ST_INTRO playback. Pure Arduino task. */
    if (!activeIntroTrack || Esp32IntroClock_isActive()) return;
    if (phase!=kSilent || ring || timerInit || fileOpen) {
        closeMusic("INTRO_EXIT_BEFORE_MAP");
    }
    if (activeIntroTrack) {
        attempted=false; /* Entrance will own the next independent track. */
        activeIntroTrack=false;
        EspEntranceMusic_syncIdleOutput();
        printf("[INTROMUSIC] HANDOFF musicClosed=yes mapLoaderMayStart=yes\n");
    }
}

void EspEntranceMusic_service(bool gameplayActive,bool sdReady) {
    const EspPlayerViewState* view=gameplayActive?
                                   EspPlayerView_view():nullptr;
    const bool entrance=gameplayActive&&sdReady&&view&&
                        view->targetMapId==1U&&view->gameplayLoadMapId==1U;
    const bool intro=Esp32IntroClock_isActive()!=0||introPrestartWindow;
    const bool soundOn=EspNativeAudioIntent_isSoundEnabled()!=0;
    /* Intro and Entrance share one reader/ISR/ring, never coexist.
     * Only the confirmed intro handoff closes M_IN_CIT before map load. */
    if ((!entrance && !intro) || !soundOn) {
        if (phase!=kSilent||ring||timerInit||fileOpen)
            closeMusic(soundOn?"SESSION_EXIT":"SOUND_OFF");
        EspEntranceMusic_syncIdleOutput(); /* SOUND ON -> 128, OFF -> disabled */
        attempted=false;
        activeIntroTrack=false;
        return;
    }
    if (!attempted) begin();
    if (phase==kSilent) return;
    if (__atomic_load_n(&silentStreak,__ATOMIC_ACQUIRE)>kMaxSilentFrames) {
        closeMusic("FAIL-UNDERRUN-TIMEOUT");return;
    }
    /* At most two 1024B SD reads per main loop. No concurrent SPI owner. */
    for (uint32_t burst=0U;burst<2U;burst++) {
        if (!refill()) { closeMusic(failure);return; }
        if (queued()>kRingCapacity-kMaxBlockSamples) break;
    }
    if (phase==kPrime&&queued()>=kPrimeFrames) {
        if (!startClock()) closeMusic(failure);
        return;
    }
    if (phase==kClock) {
        const int64_t nowUs=esp_timer_get_time();
        const uint32_t seenTicks=__atomic_load_n(&clockTicks,__ATOMIC_ACQUIRE);
        if (seenTicks<kRate) {
            if (nowUs-clockStartUs>1500000LL)
                closeMusic("FAIL-CLOCK-TIMEOUT");
            return;
        }
        /* R2f: freeze both operands at the end of the SAME measured
         * interval. Gameplay animation may delay service hundreds of ms,
         * but the IRAM ISR keeps accumulating ticks throughout that delay.
         * Never relax the verified 20900..23200 hardware clock guard. */
        if (timer_pause(kGroup,kTimer)!=ESP_OK) {
            closeMusic("FAIL-CLOCK-PAUSE");return;
        }
        const int64_t elapsed=esp_timer_get_time()-clockStartUs;
        const uint32_t ticks=__atomic_load_n(&clockTicks,__ATOMIC_ACQUIRE);
        const uint32_t measured=EspAudioClockGuard_rate(
            ticks,elapsed>0?(uint64_t)elapsed:0ULL);
        printf("[ENTRANCEMUSIC] CLOCK measuredHz=%u ticks=%u "
               "elapsedUs=%lld guard=20900..23200 dacStream=OFF dacPad=%s sampleMode=uncapped-interval\n",
               (unsigned)measured,(unsigned)ticks,(long long)elapsed,
               dacEnabled?"ON":"OFF");
        if (measured<20900U||measured>23200U) {
            closeMusic("FAIL-CLOCK");return;
        }
        if (!startPlaying()) closeMusic(failure);
        return;
    }
    if (phase==kPlay) {
        if (!activeIntroTrack)
            mixPendingSfx(); /* SFX valid only over real gameplay music. */
        const uint32_t frames=__atomic_load_n(&playedFrames,__ATOMIC_ACQUIRE);
        const uint32_t now=millis();
        if (frames!=lastFrameSeen) {
            lastFrameSeen=frames;
            lastFrameAtMs=now;
        } else if (now-lastFrameAtMs>250U) {
            closeMusic("FAIL-TIMER-STOPPED");return;
        }
        const uint32_t count=queued();
        if (count<minBuffered) minBuffered=count;
        if (!firstSignalReported && now-playStartMs>=3000U) {
            firstSignalReported=true;
            signalReport("3s"); /* first 3 seconds are real SD music */
        }
        if ((int32_t)(now-nextReportMs)>=0) {
            nextReportMs=now+10000U;
            printf("[ENTRANCEMUSIC] STREAM elapsedMs=%u frames=%u "
                   "queued=%u minQueued=%u loops=%u underflows=%u "
                   "maxGapCycles=%u gapsOver90us=%u maxIsrCycles=%u "
                   "sdReads=%u maxReadUs=%u slowReadsOver20ms=%u "
                   "recoveries=%u maxSilentFrames=%u pumpCalls=%u pumpReads=%u heap8=%u\n",
                   (unsigned)(now-playStartMs),(unsigned)frames,
                   (unsigned)count,(unsigned)minBuffered,
                   (unsigned)loops,(unsigned)underruns,
                   (unsigned)maxGapCycles,(unsigned)gapsOver90us,
                   (unsigned)maxIsrCycles,(unsigned)reads,
                   (unsigned)maxReadUs,(unsigned)slowReadCount,
                   (unsigned)recoveryStarts,(unsigned)maxSilentStreak,
                   (unsigned)pumpCalls,(unsigned)pumpBlockReads,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
            signalReport("STREAM");
        }
    }
}

/* AUDIO-R2d cooperative yield called strictly after a completed native
 * gameplay frame, once EspAssetPack has released its backing store. This
 * is still the SAME Arduino main task and the SAME SD File instance.
 * No background thread, reentrant SD access, or renderer-side buffering.
 * A busy ring means O(1) no-I/O; otherwise publish up to 4082 samples. */
void EspEntranceMusic_pumpGameplay(void) {
    if (phase!=kPlay || !fileOpen || ring==nullptr) return;
    if (queued()>kRingCapacity-kMaxBlockSamples) return;
    ++pumpCalls;
    const uint32_t before=reads;
    for (uint32_t burst=0U;burst<2U;burst++) {
        if (!refill()) {
            closeMusic(failure);
            break;
        }
        if (queued()>kRingCapacity-kMaxBlockSamples) break;
    }
    pumpBlockReads+=reads-before;
}
