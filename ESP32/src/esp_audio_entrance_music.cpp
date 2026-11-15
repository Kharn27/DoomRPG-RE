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
#include "esp_audio_entrance_music.h"
#include "esp_player_view_state.h"

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
constexpr uint32_t kRate=22050U;
constexpr uint32_t kTimerDiv=2U;
constexpr uint32_t kTimerAlarm=1814U;    /* 80MHz /2 /1814 ≈ 22050.72Hz */
constexpr uint32_t kRingCapacity=8192U; /* 371ms */
constexpr uint32_t kRingMask=kRingCapacity-1U;
constexpr uint32_t kPrimeFrames=4096U;  /* 186ms */
constexpr uint32_t kHeaderBytes=512U;
constexpr uint32_t kMaxBlockBytes=1024U;
constexpr uint32_t kMaxBlockSamples=2041U;
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
static volatile uint32_t underruns;
static volatile uint32_t prevCycle;
static volatile uint32_t maxGapCycles;
static volatile uint32_t gapsOver90us;
static volatile uint32_t maxIsrCycles;
static uint8_t* ring;

/* Single game-loop owner. These buffers do NOT occupy the audio ISR stack. */
static uint8_t raw[kMaxBlockBytes];
static uint8_t decoded[kMaxBlockSamples];
static File musicFile;
static EspEntranceAdpcmWav wav={};
static const char* path=nullptr;
static const char* failure="none";
static bool attempted;
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

static bool IRAM_ATTR onAudioTick(void*) {
    const uint8_t mode=phase;
    if (mode==kClock) {
        if (clockTicks<kRate) ++clockTicks;
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
        if (r!=w && ring!=nullptr) {
            code=ring[r&kRingMask];
            __atomic_store_n(&rd,r+1U,__ATOMIC_RELEASE);
        } else {
            ++underruns;
        }
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
static void closeMusic(const char* reason) {
    phase=kSilent;
    if (timerInit) (void)timer_pause(kGroup,kTimer);
    if (callbackAdded) (void)timer_isr_callback_remove(kGroup,kTimer);
    callbackAdded=false;
    if (timerInit) (void)timer_deinit(kGroup,kTimer);
    timerInit=false;
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
           "maxIsrCycles=%u reads=%u maxSdReadUs=%u "
           "slowReadsOver20ms=%u heap8Before=%u heap8After=%u "
           "largest8=%u timer=OFF dacPad=OFF file=CLOSED\n",
           reason,path?path:"none",(unsigned)playedFrames,
           (unsigned)loops,(unsigned)underruns,(unsigned)maxGapCycles,
           (unsigned)gapsOver90us,(unsigned)maxIsrCycles,
           (unsigned)reads,(unsigned)maxReadUs,
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
    if (!EspEntranceAdpcm_decodeBlock(raw,wav.blockAlign,
        wav.samplesPerBlock,decoded,kMaxBlockSamples,fadeFrame)) {
        failure="adpcm_decode";return false;
    }
    const uint32_t start=__atomic_load_n(&wr,__ATOMIC_RELAXED);
    for (uint32_t i=0U;i<samples;i++)
        ring[(start+i)&kRingMask]=decoded[i];
    __atomic_store_n(&wr,start+samples,__ATOMIC_RELEASE);
    producedInLoop+=samples;
    ++blocksRead;
    return true;
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
    printf("[ENTRANCEMUSIC] PREFLIGHT timer=TG1T1 dac=OFF "
           "targetHz=22050 clockDiv=2 alarm=1814 ringQueued=%u "
           "guard=20900..23200\n",(unsigned)queued());
    return true;
}
static bool startPlaying() {
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
           "dacPeakCodes=64 gpio25=untouched noI2S=yes noPSRAM=yes\n",
           path,(unsigned)kRingCapacity,(unsigned)queued());
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
    path=kPrimary;
    printf("[ENTRANCEMUSIC] OPEN path=%s fallback=%s "
           "expected=WAV-IMA-ADPCM-mono-22050\n",kPrimary,kRootFallback);
    musicFile=SD.open(kPrimary,FILE_READ);
    if (!musicFile) {
        path=kRootFallback;
        musicFile=SD.open(kRootFallback,FILE_READ);
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
        heap_caps_get_free_size(MALLOC_CAP_8BIT)<57344U ||
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

void EspEntranceMusic_service(bool gameplayActive,bool sdReady) {
    const EspPlayerViewState* view=gameplayActive?
                                   EspPlayerView_view():nullptr;
    const bool entrance=gameplayActive&&sdReady&&view&&
                        view->targetMapId==1U&&view->gameplayLoadMapId==1U;
    if (!entrance) {
        if (phase!=kSilent||ring||timerInit||fileOpen)
            closeMusic("SESSION_EXIT");
        if (!gameplayActive||!view||view->targetMapId!=1U)
            attempted=false;  /* one chance per real Entrance session */
        return;
    }
    if (!attempted) begin();
    if (phase==kSilent) return;
    if (__atomic_load_n(&underruns,__ATOMIC_ACQUIRE)!=0U) {
        closeMusic("FAIL-UNDERRUN");return;
    }
    if (!refill()) {
        closeMusic(failure);return;
    }
    if (phase==kPrime&&queued()>=kPrimeFrames) {
        if (!startClock()) closeMusic(failure);
        return;
    }
    if (phase==kClock) {
        const int64_t elapsed=esp_timer_get_time()-clockStartUs;
        if (clockTicks<kRate) {
            if (elapsed>1500000LL) closeMusic("FAIL-CLOCK-TIMEOUT");
            return;
        }
        (void)timer_pause(kGroup,kTimer);
        const uint32_t measured=elapsed>0?
            (uint32_t)((uint64_t)kRate*1000000ULL/
                       (uint64_t)elapsed):0U;
        printf("[ENTRANCEMUSIC] CLOCK measuredHz=%u ticks=%u "
               "elapsedUs=%lld guard=20900..23200 dac=OFF\n",
               (unsigned)measured,(unsigned)clockTicks,
               (long long)elapsed);
        if (measured<20900U||measured>23200U) {
            closeMusic("FAIL-CLOCK");return;
        }
        if (!startPlaying()) closeMusic(failure);
        return;
    }
    if (phase==kPlay) {
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
        if ((int32_t)(now-nextReportMs)>=0) {
            nextReportMs=now+10000U;
            printf("[ENTRANCEMUSIC] STREAM elapsedMs=%u frames=%u "
                   "queued=%u minQueued=%u loops=%u underflows=%u "
                   "maxGapCycles=%u gapsOver90us=%u maxIsrCycles=%u "
                   "sdReads=%u maxReadUs=%u slowReadsOver20ms=%u "
                   "heap8=%u\n",
                   (unsigned)(now-playStartMs),(unsigned)frames,
                   (unsigned)count,(unsigned)minBuffered,
                   (unsigned)loops,(unsigned)underruns,
                   (unsigned)maxGapCycles,(unsigned)gapsOver90us,
                   (unsigned)maxIsrCycles,(unsigned)reads,
                   (unsigned)maxReadUs,(unsigned)slowReadCount,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
        }
    }
}
