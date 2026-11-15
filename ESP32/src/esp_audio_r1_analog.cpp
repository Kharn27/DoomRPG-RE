#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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
constexpr uint32_t kFadeFrames = 3200U; // 200 ms R1f BASE comparison
constexpr uint32_t kStep440 = 118111601U; // round(440 * 2^32 / 16000)
constexpr uint32_t kStep220 = 59055800U;  // round(220 * 2^32 / 16000)

enum : uint32_t {
    kNever = 0U,
    kClock = 1U,
    kDacOff = 2U,
    kMidpoint = 3U,
    kTone440Hold = 4U,
    kGap440To220 = 5U,
    kTone220Hold = 6U,
    kSilence = 7U,
    kDacOffTail = 8U,
    kFinished = 9U,
    kStopped = 10U,
    kFailed = 11U
};
enum : uint32_t {
    kErrorNone = 0U, kErrorDriver = 1U, kErrorTask = 2U,
    kErrorClockWrite = 3U, kErrorClockRate = 4U,
    kErrorDacGate = 5U, kErrorPcmWrite = 6U,
    kErrorRam = 7U, kErrorDmaEvent = 8U
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
/* R1e: observe the original R1d PCM without any Serial writes while
 * I2S is active. Worker records bounded timing witnesses in static memory.
 * The main loop prints them ONLY after DAC off and driver uninstall. */
struct StageTelemetry {
    uint32_t startMs;
    uint32_t maxFillUs;
    uint32_t maxFillFrame;
    uint32_t maxInterWriteGapUs;
    uint32_t maxGapFrame;
    uint32_t over15ms;
    uint32_t maxWriteUs;
};
static StageTelemetry stageTelemetry[kFailed + 1U];
static QueueHandle_t i2sEvents;
static uint32_t txDoneEvents;
static uint32_t txQueueOverflowEvents;
static uint32_t dmaErrorEvents;
static bool terminalReportDone;
static bool coldBootProbe;
static bool taskCreated;
static uint32_t taskReleased; // atomic completion handshake, AFTER I2S uninstall
static void consumeDmaEvents(void) {
    i2s_event_t event;
    while (i2sEvents && xQueueReceive(i2sEvents, &event, 0) == pdTRUE) {
        if (event.type == I2S_EVENT_TX_DONE) ++txDoneEvents;
        if (event.type == I2S_EVENT_TX_Q_OVF) ++txQueueOverflowEvents;
        if (event.type == I2S_EVENT_DMA_ERROR) ++dmaErrorEvents;
    }
}

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
    consumeDmaEvents();
    if (dmaErrorEvents != 0U) {
        markFailure(kErrorDmaEvent);
        return false;
    }
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
                      uint32_t step, uint32_t* phase, uint16_t* pcm) {
    const uint32_t total = seconds * kHz;
    /* R1f: same BASE quantizer for both pitches, no SHAPED variable. */
    StageTelemetry& witness = stageTelemetry[stage];
    witness.startMs = (uint32_t)(esp_timer_get_time() / 1000LL);
    int64_t previousWriteEndUs = 0;
    publishStage(stage);
    for (uint32_t at = 0U; at < total; at += kFrames) {
        if (__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) return false;
        const int64_t fillBeginUs = esp_timer_get_time();
        if (previousWriteEndUs != 0) {
            const uint32_t gapUs = (uint32_t)(fillBeginUs - previousWriteEndUs);
            if (gapUs > witness.maxInterWriteGapUs) {
                witness.maxInterWriteGapUs = gapUs;
                witness.maxGapFrame = at;
            }
            if (gapUs > 15000U) ++witness.over15ms;
        }
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
            *phase += step;
        }
        const int64_t fillEndUs = esp_timer_get_time();
        const uint32_t fillUs = (uint32_t)(fillEndUs - fillBeginUs);
        if (fillUs > witness.maxFillUs) {
            witness.maxFillUs = fillUs;
            witness.maxFillFrame = at;
        }
        if (!writeBlock(pcm, sizeof(uint16_t) * kFrames * kSlots)) {
            if (loadStage() != kFailed) markFailure(kErrorPcmWrite);
            return false;
        }
        const int64_t writeEndUs = esp_timer_get_time();
        const uint32_t writeUs = (uint32_t)(writeEndUs - fillEndUs);
        if (writeUs > witness.maxWriteUs) witness.maxWriteUs = writeUs;
        previousWriteEndUs = writeEndUs;
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
        ok = playStage(kDacOff, 3U, 0U, kStep440, &phase, pcm);
    }
    if (ok && !__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) {
        /* On classic ESP32 the IDF LEFT DAC channel maps to DAC2 GPIO26.
         * Never enable BOTH or RIGHT: GPIO25 drives the XPT2046 clock. */
        if (i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
            markFailure(kErrorDacGate);
            ok = false;
        }
    }
    /* R1f: real 8-ohm CYD report described crackle at the tone's
     * beginning and approximately 1s later, possibly periodic.
     * R1e's last (quieter/cleaner) clip changed BOTH pitch and quantizer.
     * Hold each BASE tone 8s at identical +/-64 DAC amplitude, giving
     * enough time to hear a 1s pattern while isolating pitch vs quantizer.
     * No Audio-R1 Serial printing until the final DMA shutdown. */
    if (ok) ok = playStage(kMidpoint, 2U, 0U, kStep440, &phase, pcm);
    if (ok) ok = playStage(kTone440Hold, 8U, 64U, kStep440, &phase, pcm);
    /* In R1g cold-boot mode this is an isolated 440 Hz-only witness:
     * no gameplay, SD, TFT or touch service has been started.
     * Keep the sample generator, level, fade, DMA and APLL of R1f. */
    if (ok && !coldBootProbe)
        ok = playStage(kGap440To220, 2U, 0U, kStep440, &phase, pcm);
    if (ok && !coldBootProbe)
        ok = playStage(kTone220Hold, 8U, 64U, kStep220, &phase, pcm);
    if (ok) ok = playStage(kSilence, 2U, 0U, kStep440, &phase, pcm);
    if (ok && !__atomic_load_n(&state.stopRequested, __ATOMIC_ACQUIRE)) {
        /* Keep I2S DMA feeding midpoint but disable only DAC2/GPIO26
         * for a final 3-second OFF witness before uninstall. */
        if (i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE) != ESP_OK) {
            markFailure(kErrorDacGate);
            ok = false;
        }
    }
    if (ok) ok = playStage(kDacOffTail, 3U, 0U, kStep440, &phase, pcm);

    /* Fail closed on ANY error, cancellation or terminal success.
     * This task is the sole owner of the I2S driver after creation. */
    (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    consumeDmaEvents();
    (void)i2s_driver_uninstall(I2S_NUM_0);
    i2sEvents = nullptr;
    if (loadStage() != kFailed) {
        publishStage(ok ? kFinished : kStopped);
    }
    /* Only the boot task waits for this flag. No pause, print or mutex is
     * introduced into the audio transport or gameplay. */
    __atomic_store_n(&taskReleased, 1U, __ATOMIC_RELEASE);
    vTaskDelete(nullptr);
}
static const char* stageName(uint32_t stage) {
    switch (stage) {
        case kClock: return "CLOCK-DAC-OFF";
        case kDacOff: return "DAC-OFF";
        case kMidpoint: return "MIDPOINT-GPIO26";
        case kTone440Hold: return "440Hz-BASE-HOLD8S-PEAK64";
        case kGap440To220: return "MIDPOINT-GAP-440-220";
        case kTone220Hold: return "220Hz-BASE-HOLD8S-PEAK64";
        case kSilence: return "MIDPOINT-TAIL";
        case kDacOffTail: return "DAC-OFF-TAIL";
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
           "minimum=61440/24576/32768/16384 trigger=%s\n",
           (unsigned)before, (unsigned)largest, (unsigned)dma,
           (unsigned)dmaLargest, coldBootProbe ? "COLD-BOOT" : "gameplay");
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
    /* R1i: only tested variable versus R1h, use the normal PLL_D2
     * instead of APLL for the identical 16kHz PCM, DMA and DAC2 path.
     * The clockWitness still refuses playback if drain rate is unsafe. */
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    /* i2s_set_pin(NULL) is intentionally forbidden: GPIO25 TOUCH CLOCK. */
    /* IDF 4.4 event queue: DMA_ERROR is a real driver error, while
     * TX_Q_OVF means the *telemetry queue* filled, NOT a PCM underrun.
     * Queue size 8 is bounded. No serial tracing from the audio task. */
    if (i2s_driver_install(I2S_NUM_0, &cfg, 8, &i2sEvents) != ESP_OK) {
        markFailure(kErrorDriver);
        return;
    }
    if (i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE) != ESP_OK) {
        (void)i2s_driver_uninstall(I2S_NUM_0);
        i2sEvents = nullptr;
        markFailure(kErrorDacGate);
        return;
    }

    /* Preserve a meaningful gameplay reserve even AFTER the DMA allocator. */
    if (heap8() < 49152U || largest8() < 16384U ||
        dmaFree() < 16384U || largestDma() < 8192U) {
        (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        (void)i2s_driver_uninstall(I2S_NUM_0);
        i2sEvents = nullptr;
        markFailure(kErrorRam);
        return;
    }

    if (xTaskCreatePinnedToCore(taskMain, "audio-r1", 3072U,
                                nullptr, 2U, nullptr, 0) != pdPASS) {
        (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        (void)i2s_driver_uninstall(I2S_NUM_0);
        i2sEvents = nullptr;
        markFailure(kErrorTask);
        return;
    }
    taskCreated = true;
    printf("[AUDIOR1] START oneShot=1 trigger=%s i2s=0 dac=GPIO26-only "
           "gpio25=touch-untouched rate=16000 transport=2x16b apll=0 "
           "clock=PLL_D2 dma=4x160 task=core0/3072 sound=synthetic-no-SD "
           "calibration=R1i waveform=BASE-coldboot-440-only "
           "holdSeconds=8 peak=64 fadeMs=200 "
           "liveAudioLog=disabled dmaEvents=bounded "
           "heap8=%u largest8=%u dma=%u largestDMA=%u\n",
           coldBootProbe ? "COLD-BOOT" : "gameplay",
           (unsigned)heap8(), (unsigned)largest8(),
           (unsigned)dmaFree(), (unsigned)largestDma());
}

void EspAudioR1_runColdBootProbe(void) {
    if (state.attempted) return;
    coldBootProbe = true;
    /* Intentionally before TFT / SD / native engine initialization.
     * Boot probe adds a bounded ~19s pre-menu diagnostic delay.
     * This is one-shot: gameplay won't start audio a second time. */
    printf("[AUDIOR1] COLD_BOOT BEGIN state=before-TFT-SD-PAK "
           "audio=440Hz-BASE-8s startupDelay=yes\n");
    EspAudioR1_tryStartInGameplay();
    if (taskCreated) {
        while (__atomic_load_n(&taskReleased, __ATOMIC_ACQUIRE) == 0U) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        /* Give idle task a chance to reclaim the deleted audio task stack. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    EspAudioR1_poll(); // emit only the final, post-DAC diagnostic report
    printf("[AUDIOR1] COLD_BOOT END taskCreated=%u driverReleased=%u "
           "next=normal-game-boot audioReplay=disabled\n",
           taskCreated ? 1U : 0U,
           (unsigned)__atomic_load_n(&taskReleased, __ATOMIC_ACQUIRE));
}

void EspAudioR1_requestStop(void) {
    if (state.attempted) {
        __atomic_store_n(&state.stopRequested, 1U, __ATOMIC_RELEASE);
    }
}

void EspAudioR1_poll(void) {
    if (!state.attempted || terminalReportDone) return;
    const uint32_t stage = loadStage();
    /* No serial I/O while DAC is streaming. Stage counters are gathered
     * in RAM and published after terminal stage (acquire-release). */
    if (stage < kFinished) return;
    terminalReportDone = true;
    printf("[AUDIOR1] FINAL stage=%s measuredHz=%u writes=%u errors=%u "
           "frames=%u code=%u heap8=%u largest8=%u dma=%u "
           "dmaErrorEvents=%u txQueueOverflowEvents=%u txDoneEvents=%u\n",
           stageName(stage),
           (unsigned)__atomic_load_n(&state.measuredHz, __ATOMIC_ACQUIRE),
           (unsigned)__atomic_load_n(&state.writes, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&state.errors, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&state.framesWritten, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&state.errorCode, __ATOMIC_RELAXED),
           (unsigned)heap8(), (unsigned)largest8(), (unsigned)dmaFree(),
           (unsigned)dmaErrorEvents, (unsigned)txQueueOverflowEvents,
           (unsigned)txDoneEvents);
    for (uint32_t at = kDacOff; at <= kDacOffTail; ++at) {
        const StageTelemetry& w = stageTelemetry[at];
        if (w.startMs == 0U) continue;
        printf("[AUDIOR1] TIMING stage=%s startMs=%u "
               "maxFillUs=%u fillAtFrame=%u "
               "maxProducerGapUs=%u gapAtFrame=%u gapsOver15ms=%u "
               "maxWriteUs=%u\n",
               stageName(at), (unsigned)w.startMs,
               (unsigned)w.maxFillUs, (unsigned)w.maxFillFrame,
               (unsigned)w.maxInterWriteGapUs, (unsigned)w.maxGapFrame,
               (unsigned)w.over15ms, (unsigned)w.maxWriteUs);
    }
}
