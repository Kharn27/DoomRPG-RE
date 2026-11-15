#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdint.h>
#include <stdio.h>

#include "esp_audio_r1_analog.h"
#include "esp_audio_r1_pcm.h"

/*
 * AUDIO-R1: hardware proof ONLY, normal esp32-cyd firmware, classic CYD.
 *
 * I2S0 built-in DAC2 / GPIO26 feeds the board's analog amplifier.
 * GPIO25 is the software-XPT2046 TOUCH CLOCK: NEVER enable DAC1, and NEVER
 * call i2s_set_pin(I2S_NUM_0, NULL), which enables both internal DACs.
 *
 * Producer task pinned to core 0, no SD/PAK access, dynamic song allocation,
 * ISR callback, Bluetooth, ZIP, Sound_t, SDL_mixer or MIDI synthesis.
 * One-shot on first active gameplay; stops immediately on exit.
 */
namespace {
constexpr uint32_t kHz = 16000U;
constexpr size_t kFrames = 160U;  // 10 ms
constexpr size_t kSlots = 2U;    // explicit L+R 16-bit physical slots
constexpr uint32_t kClockWarm = 8U;
constexpr uint32_t kClockBlocks = 80U;
constexpr uint32_t kFadeFrames = 1600U; // 100 ms R1b (soft transitions)
constexpr uint32_t kStep440 = 118111601U; // round(440 * 2^32 / 16000)

enum : uint32_t {
    kNever = 0U,
    kClock = 1U,
    kDacOff = 2U,
    kMidpoint = 3U,
    kTone4 = 4U,
    kGap4To12 = 5U,
    kTone12 = 6U,
    kGap12To24 = 7U,
    kTone24 = 8U,
    kSilence = 9U,
    kFinished = 10U,
    kStopped = 11U,
    kFailed = 12U
};
enum : uint32_t {
    kErrorNone = 0U, kErrorDriver = 1U, kErrorTask = 2U,
    kErrorClockWrite = 3U, kErrorClockRate = 4U,
    kErrorDacGate = 5U, kErrorPcmWrite = 6U,
    kErrorRam = 7U
};

struct R1State {
    uint32_t stage;
    uint32_t writes;
    uint32_t errors;
    uint32_t measuredHz;
    uint32_t framesWritten;
    uint32_t stopRequested;
    uint32_t errorCode;
    bool attempted;
};
static R1State state;
static uint32_t lastReportedStage = UINT32_MAX;
static uint32_t lastPulseMs;

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}
static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}
static uint32_t dmaFree(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA);
}
static uint32_t largestDma(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
}
static void publishStage(uint32_t stage) {
    __atomic_store_n(&state.stage, stage, __ATOMIC_RELEASE);
}
static uint32_t loadStage(void) {
    return __atomic_load_n(&state.stage, __ATOMIC_ACQUIRE);
}
static void markFailure(uint32_t code) {
    __atomic_store_n(&state.errorCode, code, __ATOMIC_RELEASE);
    __atomic_add_fetch(&state.errors, 1U, __ATOMIC_RELAXED);
    publishStage(kFailed);
}
static bool writeBlock(const uint16_t* pcm, size_t bytes) {
    size_t written = 0U;
    const esp_err_t result =
        i2s_write(I2S_NUM_0, pcm, bytes, &written, portMAX_DELAY);
    if (result != ESP_OK || written != bytes) {
        __atomic_add_fetch(&state.errors, 1U, __ATOMIC_RELAXED);
        return false;
    }
    __atomic_add_fetch(&state.writes, 1U, __ATOMIC_RELAXED);
    return true;
}
static void fillMidpoint(uint16_t* pcm) {
    for (size_t i = 0U; i < kFrames * kSlots; ++i) {
        pcm[i] = (uint16_t)(128U << 8);
    }
}
static bool clockWitness(uint16_t* pcm) {
    fillMidpoint(pcm);
    publishStage(kClock);
    for (uint32_t i = 0U; i < kClockWarm; ++i) {
        if (!writeBlock(pcm, kFrames * kSlots * sizeof(uint16_t))) {
            markFailure(kErrorClockWrite);
            return false;
        }
    }
    const int64_t t0 = esp_timer_get_time();
    for (uint32_t i = 0U; i < kClockBlocks; ++i) {
        if (!writeBlock(pcm, kFrames * kSlots * sizeof(uint16_t))) {
            markFailure(kErrorClockWrite);
            return false;
        }
    }
    const int64_t elapsed = esp_timer_get_time() - t0;
    const uint32_t hz = elapsed > 0LL ?
        (uint32_t)(((uint64_t)kClockBlocks * kFrames * 1000000ULL) /
                   (uint64_t)elapsed) : 0U;
    __atomic_store_n(&state.measuredHz, hz, __ATOMIC_RELEASE);
    /* Prior DAC experiment accidentally ran ~5.5x fast before APLL.
     * Never enable GPIO26 unless DMA's real drain rate is in range. */
    if (hz < 15000U || hz > 17000U) {
        markFailure(kErrorClockRate);
        return false;
    }
    return true;
}
static bool playStage(uint32_t stage, uint32_t seconds, uint8_t peak,
                      uint32_t* phase, uint16_t* pcm) {
    const uint32_t total = seconds * kHz;
    publishStage(stage);
    for (uint32_t at = 0U; at < total; at += kFrames) {
        if (__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) return false;
        for (uint32_t i = 0U; i < kFrames; ++i) {
            const uint32_t frame = at + i;
            uint8_t amplitude = peak;
            if (peak != 0U) {
                /* Short, deterministic fade to avoid artificial clicks when
                 * changing test loudness, independent of scheduling. */
                const uint32_t remain = total - frame - 1U;
                if (frame < kFadeFrames) {
                    amplitude = (uint8_t)((peak * frame) / kFadeFrames);
                }
                if (remain < kFadeFrames) {
                    const uint8_t release =
                        (uint8_t)((peak * remain) / kFadeFrames);
                    if (release < amplitude) amplitude = release;
                }
            }
            const uint16_t value = EspAudioR1Pcm_word(*phase, amplitude);
            pcm[(size_t)i * kSlots] = value;
            pcm[(size_t)i * kSlots + 1U] = value;
            *phase += kStep440;
        }
        if (!writeBlock(pcm, sizeof(uint16_t) * kFrames * kSlots)) {
            markFailure(kErrorPcmWrite);
            return false;
        }
        __atomic_add_fetch(&state.framesWritten, (uint32_t)kFrames,
                           __ATOMIC_RELAXED);
    }
    return true;
}
static void taskMain(void*) {
    uint16_t pcm[kFrames * kSlots]; // 640B bounded on the dedicated task stack
    uint32_t phase = 0U;
    bool ok = clockWitness(pcm);
    if (ok && !__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) {
        ok = playStage(kDacOff, 2U, 0U, &phase, pcm);
    }
    if (ok && !__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) {
        /* On classic ESP32 the IDF LEFT DAC channel maps to DAC2 GPIO26.
         * Never enable BOTH or RIGHT: GPIO25 drives the XPT2046 clock. */
        if (i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
            markFailure(kErrorDacGate);
            ok = false;
        }
    }
    if (ok) ok = playStage(kMidpoint, 2U, 0U, &phase, pcm);
    /* R1b: retain continuous DAC bias across all tones. Explicit midpoint
     * gaps distinguish a transient from steady-state analog noise.
     * Peaks 4/12/24 remain bounded; fail closed on any DMA error. */
    if (ok) ok = playStage(kTone4, 3U, 4U, &phase, pcm);
    if (ok) ok = playStage(kGap4To12, 1U, 0U, &phase, pcm);
    if (ok) ok = playStage(kTone12, 3U, 12U, &phase, pcm);
    if (ok) ok = playStage(kGap12To24, 1U, 0U, &phase, pcm);
    if (ok) ok = playStage(kTone24, 3U, 24U, &phase, pcm);
    if (ok) ok = playStage(kSilence, 1U, 0U, &phase, pcm);

    /* Fail closed on ANY error, cancellation or terminal success.
     * This task is the sole owner of the I2S driver after creation. */
    (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    (void)i2s_driver_uninstall(I2S_NUM_0);
    if (loadStage() != kFailed) {
        publishStage(ok ? kFinished : kStopped);
    }
    vTaskDelete(nullptr);
}
static const char* stageName(uint32_t stage) {
    switch (stage) {
        case kClock: return "CLOCK-DAC-OFF";
        case kDacOff: return "DAC-OFF";
        case kMidpoint: return "MIDPOINT-GPIO26";
        case kTone4: return "SINE-440Hz-PEAK4";
        case kGap4To12: return "MIDPOINT-GAP-4-12";
        case kTone12: return "SINE-440Hz-PEAK12";
        case kGap12To24: return "MIDPOINT-GAP-12-24";
        case kTone24: return "SINE-440Hz-PEAK24";
        case kSilence: return "MIDPOINT-TAIL";
        case kFinished: return "DONE";
        case kStopped: return "STOPPED";
        case kFailed: return "FAIL-CLOSED";
        default: return "IDLE";
    }
}
}  // namespace

void EspAudioR1_tryStartInGameplay(void) {
    if (state.attempted) return;
    state.attempted = true;

    const uint32_t before = heap8();
    const uint32_t largest = largest8();
    const uint32_t dma = dmaFree();
    const uint32_t dmaLargest = largestDma();
    printf("[AUDIOR1] PREFLIGHT heap8=%u largest8=%u dma=%u largestDMA=%u "
           "minimum=61440/24576/32768/16384 gameplay=active\n",
           (unsigned)before, (unsigned)largest, (unsigned)dma,
           (unsigned)dmaLargest);
    if (before < 61440U || largest < 24576U ||
        dma < 32768U || dmaLargest < 16384U) {
        markFailure(kErrorRam);
        return;
    }

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX |
                            I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = kHz;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = kFrames;
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = true;
    /* i2s_set_pin(NULL) is intentionally forbidden: GPIO25 TOUCH CLOCK. */
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
        markFailure(kErrorDriver);
        return;
    }
    if (i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE) != ESP_OK) {
        (void)i2s_driver_uninstall(I2S_NUM_0);
        markFailure(kErrorDacGate);
        return;
    }

    /* Preserve a meaningful gameplay reserve even AFTER the DMA allocator. */
    if (heap8() < 49152U || largest8() < 16384U ||
        dmaFree() < 16384U || largestDma() < 8192U) {
        (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        (void)i2s_driver_uninstall(I2S_NUM_0);
        markFailure(kErrorRam);
        return;
    }

    if (xTaskCreatePinnedToCore(taskMain, "audio-r1", 3072U,
                                nullptr, 2U, nullptr, 0) != pdPASS) {
        (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        (void)i2s_driver_uninstall(I2S_NUM_0);
        markFailure(kErrorTask);
        return;
    }
    printf("[AUDIOR1] START oneShot=1 trigger=gameplay i2s=0 dac=GPIO26-only "
           "gpio25=touch-untouched rate=16000 transport=2x16b apll=1 "
           "dma=4x160 task=core0/3072 sound=synthetic-no-SD "
           "calibration=R1b peaks=4,12,24 fadeMs=100 "
           "heap8=%u largest8=%u dma=%u largestDMA=%u\n",
           (unsigned)heap8(), (unsigned)largest8(),
           (unsigned)dmaFree(), (unsigned)largestDma());
}

void EspAudioR1_requestStop(void) {
    if (state.attempted) {
        __atomic_store_n(&state.stopRequested, 1U, __ATOMIC_RELEASE);
    }
}

void EspAudioR1_poll(void) {
    if (!state.attempted) return;
    const uint32_t stage = loadStage();
    const uint32_t now = millis();
    if (stage != lastReportedStage) {
        lastReportedStage = stage;
        printf("[AUDIOR1] STAGE %s measuredHz=%u writes=%u errors=%u "
               "frames=%u code=%u heap8=%u largest8=%u dma=%u\n",
               stageName(stage),
               (unsigned)__atomic_load_n(&state.measuredHz, __ATOMIC_ACQUIRE),
               (unsigned)__atomic_load_n(&state.writes, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&state.errors, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&state.framesWritten, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&state.errorCode, __ATOMIC_RELAXED),
               (unsigned)heap8(), (unsigned)largest8(), (unsigned)dmaFree());
    }
    if (stage >= kClock && stage <= kSilence &&
        (uint32_t)(now - lastPulseMs) >= 3000U) {
        lastPulseMs = now;
        printf("[AUDIOR1] PULSE stage=%s measuredHz=%u writes=%u errors=%u "
               "heap8=%u largest8=%u\n",
               stageName(stage),
               (unsigned)__atomic_load_n(&state.measuredHz, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&state.writes, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&state.errors, __ATOMIC_RELAXED),
               (unsigned)heap8(), (unsigned)largest8());
    }
}
