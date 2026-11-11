#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_audio_intent.h"
#include "esp_native_audio_midi.h"
#include "esp_native_audio_pcm_math.h"
#include "esp_native_midi_stream.h"

/* AUDIO-A: authentic PAK MIDI notes, intentionally tiny synthesized timbres.
 * No SoundFont, desktop FluidSynth, whole-song allocation or runtime ZIP.
 *
 * Main loop owns the PAK lease and parses at most 24 events per service.
 * Core 0 owns the phase accumulators and 16kHz I2S0 built-in DAC on GPIO26
 * (the classic ESP32-2432S028R's SC8002B amplifier / P4 speaker socket).
 * This is not a reproduction of the desktop's sampled GM instruments.
 */
namespace {

constexpr uint32_t kSampleRate = 16000U;
constexpr uint32_t kBufferSamples = 160U;       // 10ms @ 16kHz
constexpr uint32_t kDacSlotsPerFrame = 2U; // explicit stereo I2S slots, one GPIO26 DAC
constexpr uint32_t kLookaheadSamples = 12000U;  // 750ms
constexpr uint8_t kClockWarmupWrites = 8U;
constexpr uint8_t kClockMeasuredWrites = 80U;
constexpr uint32_t kClockMinAcceptHz = 14000U;
constexpr uint32_t kClockMaxAcceptHz = 18000U;
constexpr uint8_t kEventQueueCapacity = 96U;
constexpr uint8_t kMaxEventsPerService = 24U;
constexpr uint8_t kVoices = 12U;
constexpr uint16_t kReleaseSamples = 1200U;
/* One-time non-musical hardware differential: stream a bounded 2-second
 * midpoint with DAC disabled, then two seconds of midpoint with GPIO26
 * enabled, then start authentic MIDI at sample zero. No tones are injected.
 * Core0 never acquires the SD/PAK lease. */
constexpr uint32_t kAudioDiagStageSamples = 2U * kSampleRate;
enum : uint32_t {
    kAudioDiagAwaitClock = 0U,
    kAudioDiagDacOff = 1U,
    kAudioDiagDacMidpoint = 2U,
    kAudioDiagMidi = 3U,
    kAudioDiagFailed = 4U
};

struct NoteMessage {
    uint32_t atSample;
    uint32_t epoch;
    uint8_t type;
    uint8_t channel;
    uint8_t a;
    uint8_t b;
};
static_assert(sizeof(NoteMessage) == 12U, "MIDI queue must remain bounded");

struct Voice {
    uint32_t phase;
    uint32_t step;
    uint32_t born;
    uint16_t releaseRemaining;
    uint16_t releaseTotal;
    uint8_t note;
    uint8_t channel;
    uint8_t gain;
    uint8_t attack;
    uint8_t patch;
    uint8_t active;
    uint8_t drum;
};

struct Channel {
    uint8_t volume;
    uint8_t expression;
    uint8_t program;
};

struct Music {
    EspNativeMidiSong song;
    EspAssetPackEntry entry;
    EspNativeMidiEvent held;
    uint32_t baseSamples;
    uint32_t lastQueuedSample;
    uint32_t wallStartMs;    // monotonic reference for DMA speed verification
    uint32_t queuedEvents;
    uint32_t queuedNotes;
    uint32_t queuedDrums;
    uint32_t queuedPrograms;
    uint16_t resourceId;
    uint8_t active;
    uint8_t loop;
    uint8_t hasHeld;
    uint8_t ended;
};

static Music music;
static QueueHandle_t eventQueue;
static TaskHandle_t audioTaskHandle;
static uint8_t driverReady;
static uint8_t driverAttempted;
static EspNativeAudioIntent command;
static uint8_t commandPending;

/* Only these aligned atomics are shared with the core-0 PCM producer. */
static uint32_t sharedEpoch = 1U;
/* Safe to update at any time, even before I2S is initialized. */
static uint32_t masterPercent = 100U;
static uint32_t enabled;
static uint32_t sampleClock;
static uint32_t lateEvents;
static uint32_t audioWrites;
static uint32_t audioErrors;
static uint32_t pcmLimitedSamples;
static uint32_t pcmClippedSamples;
static uint32_t pcmQuietSamples;
static uint32_t pcmPeak8;
static uint32_t audioDiagStage; // atomic publication by audio core
static uint32_t audioDiagFrames; // audio task only
static uint32_t audioDiagMidiEpochMs; // atomic wall-clock point, MIDI t=0
static uint32_t audioDiagReported; // loop thread only
static uint32_t audioGainReportedPhase = UINT32_MAX; // loop-only; one line/phase
/* I2S's i2s_get_clk() reports the requested rate; the DAC clock can
 * physically run much faster if the 160 MHz clock divider saturates.
 * Measure DMA backpressure on the real CYD before playing any notes. */
static uint32_t clockProbeState; // 0=awaiting, 1=verified, 2=fail-closed
static uint32_t clockProbeHz;
static uint32_t clockProbeUs;
static uint8_t clockReported;
static int8_t sinus[256];
static uint32_t previousReport;

static uint32_t loadEpoch() {
    return __atomic_load_n(&sharedEpoch, __ATOMIC_ACQUIRE);
}
static uint32_t currentSample() {
    return __atomic_load_n(&sampleClock, __ATOMIC_ACQUIRE);
}

static uint32_t stepFor(uint8_t note) {
    const float hz = 440.0f *
        powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    const float capped = hz > 6000.0f ? 6000.0f : hz;
    return static_cast<uint32_t>(
        capped * (4294967296.0 / static_cast<double>(kSampleRate)));
}

static void allNotesOff(Voice voices[kVoices], uint8_t channel) {
    for (unsigned i = 0; i < kVoices; ++i) {
        if (voices[i].active && voices[i].channel == channel) {
            voices[i].releaseTotal = kReleaseSamples;
            voices[i].releaseRemaining = kReleaseSamples;
        }
    }
}

static void interpret(const NoteMessage& e,
                      Voice voices[kVoices], Channel channels[16],
                      uint32_t sample) {
    const uint8_t ch = e.channel & 15U;
    Channel& c = channels[ch];
    if (e.type == ESP_NATIVE_MIDI_PROGRAM) {
        c.program = e.a;
        return;
    }
    if (e.type == ESP_NATIVE_MIDI_CONTROL) {
        if (e.a == 7U) c.volume = e.b;
        else if (e.a == 11U) c.expression = e.b;
        else if (e.a == 120U || e.a == 123U) allNotesOff(voices, ch);
        else if (e.a == 121U) {
            c.volume = 100U;
            c.expression = 127U;
        }
        return;
    }
    if (e.type == ESP_NATIVE_MIDI_NOTE_OFF ||
        (e.type == ESP_NATIVE_MIDI_NOTE_ON && e.b == 0U)) {
        for (unsigned i = 0; i < kVoices; ++i) {
            Voice& v = voices[i];
            if (v.active && !v.drum && v.channel == ch &&
                v.note == e.a && v.releaseRemaining == 0U) {
                v.releaseTotal = kReleaseSamples;
                v.releaseRemaining = kReleaseSamples;
            }
        }
        return;
    }
    if (e.type != ESP_NATIVE_MIDI_NOTE_ON) return;

    unsigned slot = kVoices;
    uint32_t oldest = 0U;
    for (unsigned i = 0; i < kVoices; ++i) {
        if (!voices[i].active) { slot = i; break; }
        if (slot == kVoices || sample - voices[i].born > oldest) {
            oldest = sample - voices[i].born;
            slot = i;
        }
    }
    Voice& v = voices[slot];
    memset(&v, 0, sizeof(v));
    v.active = 1U;
    v.channel = ch;
    v.note = e.a;
    v.patch = c.program;
    v.born = sample;
    v.gain = static_cast<uint8_t>(
        (static_cast<uint32_t>(e.b) * c.volume * c.expression) /
        (127U * 127U));
    v.drum = ch == 9U ? 1U : 0U;
    v.step = stepFor(e.a);
    if (v.drum) {
        /* Noise/percussion lasts only a bounded pulse, with private RNG. */
        v.releaseTotal = (e.a == 35U || e.a == 36U) ? 1800U : 750U;
        v.releaseRemaining = v.releaseTotal;
    }
}


/* Runs exactly once on core 0, reusing the PCM task's existing 640-byte
 * buffer. No extra buffer, task, timer ISR or SD lease.
 * Warm the 4-buffer DMA ring, then measure 80 complete buffer writes.
 * In ESP-IDF v4.4 the built-in DAC clock uses mclk = 2*rate*16.
 * With the default 160MHz source the requested 512kHz mclk needs a divider
 * >63, so i2s_get_clk() can falsely report 16kHz while DMA drains ~5x fast.
 * Using APLL keeps the divisor within its hardware range. */
static void verifyDacClock(uint16_t pcm[kBufferSamples * kDacSlotsPerFrame]) {
    for (unsigned i = 0; i < kBufferSamples * kDacSlotsPerFrame; ++i)
        pcm[i] = static_cast<uint16_t>(128U << 8); // DAC midpoint silence
    bool ok = true;
    for (unsigned i = 0; i < kClockWarmupWrites; ++i) {
        size_t written = 0U;
        const esp_err_t err = i2s_write(
            I2S_NUM_0, pcm, sizeof(uint16_t) * kBufferSamples *
            kDacSlotsPerFrame, &written, portMAX_DELAY);
        if (err != ESP_OK || written != sizeof(uint16_t) *
            kBufferSamples * kDacSlotsPerFrame) { ok = false; break; }
    }
    const int64_t startUs = esp_timer_get_time();
    unsigned measured = 0U;
    for (; ok && measured < kClockMeasuredWrites; ++measured) {
        size_t written = 0U;
        const esp_err_t err = i2s_write(
            I2S_NUM_0, pcm, sizeof(uint16_t) * kBufferSamples *
            kDacSlotsPerFrame, &written, portMAX_DELAY);
        if (err != ESP_OK || written != sizeof(uint16_t) *
            kBufferSamples * kDacSlotsPerFrame) { ok = false; break; }
    }
    const int64_t elapsed = esp_timer_get_time() - startUs;
    const uint32_t us = elapsed > 0 && elapsed <= UINT32_MAX
        ? static_cast<uint32_t>(elapsed) : 0U;
    const uint32_t hz = ok && us != 0U
        ? static_cast<uint32_t>(
            (static_cast<uint64_t>(measured) * kBufferSamples *
             1000000ULL) / us) : 0U;
    uint32_t accepted = ok && measured == kClockMeasuredWrites &&
        hz >= kClockMinAcceptHz && hz <= kClockMaxAcceptHz ? 1U : 2U;
    /* Calibration is digital-only: GPIO26 remains disabled. The two
     * isolated silent stages below start only after rate verification. */
    __atomic_store_n(&audioDiagStage,
                     accepted == 1U ? kAudioDiagDacOff : kAudioDiagFailed,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&clockProbeUs, us, __ATOMIC_RELEASE);
    __atomic_store_n(&clockProbeHz, hz, __ATOMIC_RELEASE);
    __atomic_store_n(&clockProbeState, accepted, __ATOMIC_RELEASE);
    if (accepted != 1U)
        __atomic_store_n(&enabled, 0U, __ATOMIC_RELEASE);
}

static void taskBody(void*) {
    Voice voices[kVoices] = {};
    Channel channels[16] = {};
    NoteMessage pending = {};
    /* ESP32 I2S v1 has a broken 16-bit mono FIFO layout.  Use full
     * stereo DMA frames even though ONLY GPIO26's left DAC is enabled.
     * Two identical slots keep the analog channel correct and unambiguous. */
    uint16_t pcm[kBufferSamples * kDacSlotsPerFrame] = {};
    uint32_t epoch = 0U;
    uint32_t sample = 0U;
    uint32_t noise = 0x6a09e667U; /* audio-only; never gameplay RNG */
    bool havePending = false;

    for (;;) {
        const uint32_t liveEpoch = loadEpoch();
        if (epoch != liveEpoch) {
            epoch = liveEpoch;
            sample = 0U;
            havePending = false;
            memset(voices, 0, sizeof(voices));
            memset(channels, 0, sizeof(channels));
            for (unsigned ch = 0; ch < 16U; ++ch) {
                channels[ch].volume = 100U;
                channels[ch].expression = 127U;
            }
            __atomic_store_n(&sampleClock, 0U, __ATOMIC_RELEASE);
        }
        /* First play silent frames to verify actual DMA drain rate. The
         * hardware is authoritative, not the nominal i2s_get_clk().
         * This runs in the isolated audio task; gameplay never blocks. */
        const uint32_t probe = __atomic_load_n(
            &clockProbeState, __ATOMIC_ACQUIRE);
        if (probe == 0U) {
            verifyDacClock(pcm);
            continue;
        }
        if (probe != 1U) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const uint32_t diag = __atomic_load_n(
            &audioDiagStage, __ATOMIC_ACQUIRE);
        if (diag == kAudioDiagDacOff || diag == kAudioDiagDacMidpoint) {
            /* Stage 1: DAC OFF while the I2S FIFO streams digital
             * silence. Stage 2: only GPIO26 DAC ON, exact same PCM. */
            for (unsigned i = 0; i < kBufferSamples * kDacSlotsPerFrame; ++i)
                pcm[i] = (uint16_t)(128U << 8);
            size_t written = 0U;
            const esp_err_t err = i2s_write(
                I2S_NUM_0, pcm, sizeof(pcm), &written, portMAX_DELAY);
            if (err != ESP_OK || written != sizeof(pcm)) {
                __atomic_add_fetch(&audioErrors, 1U, __ATOMIC_RELAXED);
                (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
                __atomic_store_n(&enabled, 0U, __ATOMIC_RELEASE);
                __atomic_store_n(&audioDiagStage, kAudioDiagFailed,
                                 __ATOMIC_RELEASE);
                __atomic_store_n(&clockProbeState, 2U, __ATOMIC_RELEASE);
                continue;
            }
            __atomic_add_fetch(&audioWrites, 1U, __ATOMIC_RELAXED);
            audioDiagFrames += kBufferSamples;
            if (audioDiagFrames >= kAudioDiagStageSamples) {
                audioDiagFrames = 0U;
                if (diag == kAudioDiagDacOff) {
                    /* The FIFO is already full of 0x8000 PCM. */
                    if (i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
                        (void)i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
                        __atomic_store_n(&enabled, 0U, __ATOMIC_RELEASE);
                        __atomic_store_n(&audioDiagStage, kAudioDiagFailed,
                                         __ATOMIC_RELEASE);
                        __atomic_store_n(&clockProbeState, 2U, __ATOMIC_RELEASE);
                    } else {
                        __atomic_store_n(&audioDiagStage,
                                         kAudioDiagDacMidpoint,
                                         __ATOMIC_RELEASE);
                    }
                } else {
                    /* MIDI time begins strictly after BOTH silent probes. */
                    __atomic_store_n(&audioDiagMidiEpochMs, millis(),
                                     __ATOMIC_RELEASE);
                    __atomic_store_n(&audioDiagStage, kAudioDiagMidi,
                                     __ATOMIC_RELEASE);
                }
            }
            continue;
        }
        const uint32_t master = __atomic_load_n(
            &masterPercent, __ATOMIC_ACQUIRE);
        /* Diagnostic gain is clamped below the persisted user volume.
         * One value per 10-ms buffer, exact 8-second sample boundaries. */
        const uint8_t diagnosticGain = EspNativeAudioPcm_gainTestPercent(
            sample, master);
        uint32_t blockLimited = 0U, blockClipped = 0U;
        uint32_t blockQuiet = 0U, blockPeak = 0U;
        for (unsigned i = 0; i < kBufferSamples; ++i) {
            int32_t mixedQ8 = 0;
            bool voiceSound = false;
            const bool playing = __atomic_load_n(
                &enabled, __ATOMIC_ACQUIRE) != 0U;
            if (playing) {
                if (!havePending &&
                    xQueueReceive(eventQueue, &pending, 0) == pdTRUE)
                    havePending = true;

                for (unsigned attempts = 0U;
                     havePending && attempts < 24U; ++attempts) {
                    if (pending.epoch != epoch) {
                        havePending = false;
                    } else if (pending.atSample > sample) {
                        break;
                    } else {
                        if (pending.atSample + kBufferSamples < sample)
                            __atomic_add_fetch(&lateEvents, 1U, __ATOMIC_RELAXED);
                        interpret(pending, voices, channels, sample);
                        havePending = false;
                    }
                    if (!havePending &&
                        xQueueReceive(eventQueue, &pending, 0) == pdTRUE)
                        havePending = true;
                }

                for (unsigned vIndex = 0; vIndex < kVoices; ++vIndex) {
                    Voice& v = voices[vIndex];
                    if (!v.active) continue;
                    voiceSound = true;
                    v.phase += v.step;
                    int32_t wave;
                    if (v.drum) {
                        noise ^= noise << 13;
                        noise ^= noise >> 17;
                        noise ^= noise << 5;
                        wave = static_cast<int32_t>((noise >> 24) & 255U) - 128;
                    } else {
                        const uint8_t phase = static_cast<uint8_t>(v.phase >> 24);
                        wave = sinus[phase];
                        /* Tiny FM-like harmonic variation from real Program
                         * Change events; no large sampled instrument bank. */
                        /* Harmonics above ~7kHz fold into harsh alias tones
                         * at 16kHz. Use the fundamental for upper notes. */
                        if (v.patch < 48U && v.note <= 105U)
                            wave = (wave * 3 +
                                sinus[static_cast<uint8_t>(phase * 2U)]) / 4;
                        else if (v.patch >= 80U && v.note <= 97U)
                            wave = (wave * 2 +
                                sinus[static_cast<uint8_t>(phase * 3U)]) / 3;
                    }
                    int32_t envelope = 128;
                    if (v.attack < 128U && !v.drum) {
                        envelope = v.attack;
                        ++v.attack;
                    }
                    if (v.releaseTotal != 0U) {
                        envelope = (envelope * v.releaseRemaining) /
                                   v.releaseTotal;
                        if (v.releaseRemaining != 0U)
                            --v.releaseRemaining;
                        if (v.releaseRemaining == 0U) {
                            v.active = 0U;
                        }
                    }
                    mixedQ8 += EspNativeAudioPcm_voiceQ8(
                        wave, v.gain, envelope);
                }
                ++sample;
            }
            /* Preserve 8 fractional bits THROUGH volume control and the
             * soft limiter; round only at the final 8-bit DAC boundary. */
            uint8_t limited = 0U, clipped = 0U;
            const int32_t finalQ8 = EspNativeAudioPcm_scaleLimitQ8(
                mixedQ8, diagnosticGain, &limited, &clipped);
            const uint16_t dac = EspNativeAudioPcm_toDacWord(finalQ8);
            blockLimited += limited;
            blockClipped += clipped;
            if (!playing || !voiceSound) ++blockQuiet;
            const int32_t actual = (int32_t)(dac >> 8) - 128;
            const uint32_t amplitude = actual < 0 ? (uint32_t)-actual : (uint32_t)actual;
            if (amplitude > blockPeak) blockPeak = amplitude;
            pcm[2U * i] = dac;
            pcm[2U * i + 1U] = dac; // identical slots; GPIO25 DAC remains OFF
        }
        __atomic_store_n(&sampleClock, sample, __ATOMIC_RELEASE);
        __atomic_fetch_add(&pcmLimitedSamples, blockLimited, __ATOMIC_RELAXED);
        __atomic_fetch_add(&pcmClippedSamples, blockClipped, __ATOMIC_RELAXED);
        __atomic_fetch_add(&pcmQuietSamples, blockQuiet, __ATOMIC_RELAXED);
        uint32_t previousPeak = __atomic_load_n(&pcmPeak8, __ATOMIC_RELAXED);
        while (blockPeak > previousPeak &&
               !__atomic_compare_exchange_n(&pcmPeak8, &previousPeak,
                    blockPeak, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
        size_t written = 0U;
        const esp_err_t result = i2s_write(
            I2S_NUM_0, pcm, sizeof(pcm), &written, portMAX_DELAY);
        if (result != ESP_OK || written != sizeof(pcm))
            __atomic_add_fetch(&audioErrors, 1U, __ATOMIC_RELAXED);
        else
            __atomic_add_fetch(&audioWrites, 1U, __ATOMIC_RELAXED);
    }
}

static int initOutput() {
    if (driverReady) return 1;
    if (driverAttempted) return 0;
    driverAttempted = 1U;
    eventQueue = xQueueCreate(kEventQueueCapacity, sizeof(NoteMessage));
    if (eventQueue == nullptr) {
        printf("[AUDIO] FAIL reason=event-queue-memory gameplay=unaffected\n");
        return 0;
    }

    i2s_config_t cfg = {};
    cfg.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = kSampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    /* Always send full LEFT+RIGHT I2S frames. Mono FIFO packing on original
     * ESP32 is unreliable and can consume samples too quickly. The enabled
     * analog DAC is selected separately (GPIO26 only). */
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = kBufferSamples;
    /* Source: ESP-IDF v4.4 I2S DAC computes mclk=2*rate*chanBits.
     * Default 160MHz clock cannot divide down to 512kHz within 6-bit
     * divider range. APLL avoids the observed ~5.5x DMA overclock. */
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = true;
    /* IMPORTANT: i2s_set_pin(I2S_NUM_0, NULL) would enable BOTH DACs,
     * including GPIO25, which is the classic CYD's XPT2046 touch clock.
     * Configure ONLY the GPIO26/left DAC via the explicit IDF API. */
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK ||
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE) != ESP_OK) {
        printf("[AUDIO] FAIL reason=i2s-dac-init gpio=26 gameplay=unaffected\n");
        i2s_driver_uninstall(I2S_NUM_0);
        return 0;
    }
    for (unsigned n = 0; n < 256U; ++n) {
        sinus[n] = static_cast<int8_t>(
            lroundf(127.0f * sinf((2.0f * 3.14159265358979323846f * n) / 256.0f)));
    }
    if (xTaskCreatePinnedToCore(taskBody, "drpg-audio", 3072U, nullptr,
                                2U, &audioTaskHandle, 0) != pdPASS) {
        i2s_driver_uninstall(I2S_NUM_0);
        printf("[AUDIO] FAIL reason=i2s-task-memory gameplay=unaffected\n");
        return 0;
    }
    driverReady = 1U;
    printf("[AUDIO] READY output=GPIO26/P4 dac=I2S0-left "
           "sampleRate=16000 driverRequestedHz=%u source=APLL "
           "realClock=DMA-probe-pending dacGate=muted-until-clock-pass "
           "transport=stereo-2x16bit "
           "framesPerWrite=%u bytesPerWrite=%u voices=12 queue=96 dma=4x160 "
           "task=core0 synth=bounded-FM-like backend=native\n",
           (unsigned)(i2s_get_clk(I2S_NUM_0) + 0.5f),
           (unsigned)kBufferSamples,
           (unsigned)sizeof(uint16_t) * kBufferSamples * kDacSlotsPerFrame);
    return 1;
}

static void stopMusic(const char* why) {
    __atomic_store_n(&enabled, 0U, __ATOMIC_RELEASE);
    (void)__atomic_add_fetch(&sharedEpoch, 1U, __ATOMIC_ACQ_REL);
    __atomic_store_n(&sampleClock, 0U, __ATOMIC_RELEASE);
    if (eventQueue) xQueueReset(eventQueue);
    memset(&music, 0, sizeof(music));
    audioGainReportedPhase = UINT32_MAX;
    printf("[AUDIO] STOP reason=%s playback=idle gameplay=unchanged\n", why);
}

static int readMidiRange(void* context, uint32_t offset,
                         void* destination, size_t bytes) {
    return EspAssetPack_readStreamRange(
        static_cast<const EspAssetPackEntry*>(context),
        offset, destination, bytes);
}

static uint32_t toSamples(uint64_t micros) {
    /* Divide before multiplying: pathological timestamp cannot wrap uint64. */
    const uint64_t seconds = micros / 1000000ULL;
    if (seconds > UINT32_MAX / kSampleRate) return UINT32_MAX;
    const uint64_t samples = seconds * kSampleRate +
        ((micros % 1000000ULL) * kSampleRate) / 1000000ULL;
    return samples > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(samples);
}

static int refillMusic(unsigned maxEvents = kMaxEventsPerService) {
    unsigned sent = 0U;
    if (!music.active || music.ended || !eventQueue) return 1;
    const uint32_t wanted = currentSample() + kLookaheadSamples;
    if (music.hasHeld &&
        (toSamples(music.held.atUs) > UINT32_MAX - music.baseSamples ||
         toSamples(music.held.atUs) + music.baseSamples > wanted)) return 1;
    if (EspAssetPack_isOpen() || EspAssetPack_isSourceProbeActive()) return 1;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 1;
    int result = 1;
    while (sent < maxEvents &&
           uxQueueSpacesAvailable(eventQueue) > 0U) {
        EspNativeMidiEvent evt = {};
        if (music.hasHeld) {
            evt = music.held;
            music.hasHeld = 0U;
        } else {
            const int status = EspNativeMidi_next(&music.song, &evt);
            if (status < 0) { result = 0; break; }
            if (status == 0) {
                if (!music.loop) { music.ended = 1U; break; }
                uint32_t duration = toSamples(
                    EspNativeMidi_durationUs(&music.song));
                if (duration < 1U || UINT32_MAX - music.baseSamples < duration) {
                    result = 0;
                    break;
                }
                music.baseSamples += duration;
                if (!EspNativeMidi_begin(
                        &music.song, readMidiRange, &music.entry,
                        music.entry.size)) {
                    result = 0;
                    break;
                }
                ++sent;
                continue;
            }
        }
        const uint32_t when = toSamples(evt.atUs);
        if (UINT32_MAX - music.baseSamples < when) { result = 0; break; }
        const uint32_t at = music.baseSamples + when;
        if (at > wanted) {
            music.held = evt;
            music.hasHeld = 1U;
            break;
        }
        if (evt.type != ESP_NATIVE_MIDI_TEMPO) {
            NoteMessage out = {};
            out.atSample = at;
            out.epoch = loadEpoch();
            out.type = evt.type;
            out.channel = evt.channel;
            out.a = evt.a;
            out.b = evt.b;
            if (xQueueSend(eventQueue, &out, 0) != pdTRUE) {
                music.held = evt;
                music.hasHeld = 1U;
                break;
            }
            music.lastQueuedSample = at;
            ++music.queuedEvents;
            if (evt.type == ESP_NATIVE_MIDI_PROGRAM)
                ++music.queuedPrograms;
            if (evt.type == ESP_NATIVE_MIDI_NOTE_ON && evt.b != 0U) {
                ++music.queuedNotes;
                if (evt.channel == 9U) ++music.queuedDrums;
            }
        }
        ++sent;
    }
    EspAssetPack_close();
    return result;
}

static int startMusic(uint16_t resourceId, uint8_t flags) {
    char name[16];
    if (!initOutput()) return 0;
    if (EspAssetPack_isOpen() || EspAssetPack_isSourceProbeActive())
        return -1; /* Preserve exclusive map-transition PAK preflight. */
    snprintf(name, sizeof(name), "%u.mid", static_cast<unsigned>(resourceId));
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return -1;
    EspAssetPackEntry entry = {};
    int ok = EspAssetPack_findEntry(name, &entry) &&
             (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) == 0U &&
             entry.size > 14U && entry.size <= 1024U * 1024U;
    if (ok) {
        /* STOP before changing reader/queue generation; PAK is still owned
         * by the main loop alone, never by taskBody(). */
        stopMusic("replace");
        music.entry = entry;
        music.resourceId = resourceId;
        music.loop = (flags & 1U) ? 1U : 0U;
        ok = EspNativeMidi_begin(
            &music.song, readMidiRange, &music.entry, entry.size);
        if (ok) {
            music.active = 1U;
        }
    }
    EspAssetPack_close();
    if (!ok) {
        stopMusic("invalid-midi");
        printf("[AUDIO] FAIL resource=%u file=%s reason=header-or-pak "
               "gameplay=unaffected\n", (unsigned)resourceId, name);
        return 0;
    }
    /* Publish the initial event lookahead BEFORE enabling the PCM clock:
     * notes at t=0 must not be dropped while the main loop does PAK reads.
     * No extra heap, fixed 96-message queue and finite work. */
    if (!refillMusic(kEventQueueCapacity)) {
        stopMusic("midi-preload-invalid");
        return 0;
    }
    music.wallStartMs = millis();
    previousReport = music.wallStartMs;
    __atomic_store_n(&enabled, 1U, __ATOMIC_RELEASE);
    printf("[AUDIO] MIDI-START resource=%u file=%s bytes=%u tracks=%u "
           "ppqn=%u loop=%u source=PAK synth=FM-lite-no-GM-bank\n",
           (unsigned)resourceId, name, (unsigned)entry.size,
           (unsigned)music.song.trackCount, (unsigned)music.song.ppqn,
           (unsigned)music.loop);
    return 1;
}

} // namespace

extern "C" void EspNativeAudioMidi_service(void) {
    /* Audio parsing runs at the tail of loop(), never inside the input
     * callback, TFT present or the real-time audio task. */
    for (unsigned i = 0U; i < 4U; ++i) {
        if (!commandPending) {
            if (!EspNativeAudioIntent_takeNext(&command)) break;
            commandPending = 1U;
        }
        if (command.resourceId == 0U &&
            command.flags == ESP_NATIVE_AUDIO_STOP_ALL_FLAG) {
            stopMusic("intent-stop-all");
            commandPending = 0U;
            continue;
        }
        /* AUDIO-A only: do not hijack the other 92 original SFX intents. */
        if ((command.flags & 8U) == 0U ||
            (command.resourceId != 5039U &&
             command.resourceId != 5040U &&
             command.resourceId != 5043U)) {
            commandPending = 0U;
            continue;
        }
        if (EspAssetPack_isOpen() || EspAssetPack_isSourceProbeActive()) break;
        const int started = startMusic(command.resourceId, command.flags);
        if (started < 0) break;
        commandPending = 0U;
    }

    if (music.active && !refillMusic())
        stopMusic("midi-stream-invalid");

    const uint32_t state = __atomic_load_n(
        &clockProbeState, __ATOMIC_ACQUIRE);
    if (driverReady && state != 0U && !clockReported) {
        clockReported = 1U;
        const uint32_t measured = __atomic_load_n(
            &clockProbeHz, __ATOMIC_ACQUIRE);
        const uint32_t us = __atomic_load_n(
            &clockProbeUs, __ATOMIC_ACQUIRE);
        printf("[AUDIOCLK] VERIFY source=APLL targetHz=16000 "
               "measuredHz=%u elapsedUs=%u frames=%u "
               "result=%s threshold=14000..18000 "
               "dacGate=%s nominalI2sGetClk=not-authoritative\n",
               (unsigned)measured, (unsigned)us,
               (unsigned)kClockMeasuredWrites * kBufferSamples,
               state == 1U ? "PASS" : "FAIL-CLOSED",
               state == 1U ? "both-disabled-during-probe" : "both-disabled");
        if (state != 1U && music.active)
            stopMusic("dac-clock-probe-invalid");
        else if (state == 1U && music.active) {
            /* No MIDI clock is started by the first silent DAC probe. */
            music.wallStartMs = millis();
            previousReport = music.wallStartMs;
        }
    }
    if (driverReady) {
        const uint32_t diag = __atomic_load_n(
            &audioDiagStage, __ATOMIC_ACQUIRE);
        if (diag != 0U && diag != audioDiagReported) {
            audioDiagReported = diag;
            printf("[AUDIODIAG] stage=%s gpio26=%s "
                   "silencePcm=0x8000 silenceStageMs=2000 midiTimer=%s\n",
                   diag == kAudioDiagDacOff ? "DAC-OFF" :
                   diag == kAudioDiagDacMidpoint ? "DAC-MIDPOINT" :
                   diag == kAudioDiagMidi ? "MIDI" : "FAIL-CLOSED",
                   diag == kAudioDiagDacMidpoint || diag == kAudioDiagMidi ?
                       "enabled" : "disabled",
                   diag == kAudioDiagMidi ? "ended" : "held");
            if (diag == kAudioDiagMidi && music.active) {
                music.wallStartMs = __atomic_load_n(
                    &audioDiagMidiEpochMs, __ATOMIC_ACQUIRE);
                previousReport = music.wallStartMs;
            }
        }
    }

    const uint32_t diagNow = __atomic_load_n(
        &audioDiagStage, __ATOMIC_ACQUIRE);
    if (driverReady && music.active && diagNow == kAudioDiagMidi) {
        const uint32_t midiSample = currentSample();
        const uint32_t phase = EspNativeAudioPcm_gainTestPhase(midiSample);
        if (phase != audioGainReportedPhase) {
            audioGainReportedPhase = phase;
            const uint32_t preset = __atomic_load_n(
                &masterPercent, __ATOMIC_ACQUIRE);
            const uint32_t applied = EspNativeAudioPcm_gainTestPercent(
                midiSample, preset);
            printf("[AUDIOGAIN] step=%u/4 elapsedMs=%u masterPct=%u "
                   "effectivePct=%u nextAfterMs=%u "
                   "source=original-MIDI test=temporary-analog-headroom\n",
                   (unsigned)(phase + 1U),
                   (unsigned)(midiSample / 16U), (unsigned)preset,
                   (unsigned)applied,
                   (unsigned)(phase < 3U ? (phase + 1U) * 8000U : 0U));
        }
    }

    const uint32_t now = millis();
    if (driverReady && music.active &&
        now - previousReport >= 10000U) {
        previousReport = now;
        const uint32_t wallMs = now - music.wallStartMs;
        const uint32_t audioMs =
            (uint32_t)(currentSample() * 1000ULL / kSampleRate);
        const uint32_t ratioPct = wallMs > 0U
            ? (uint32_t)(((uint64_t)audioMs * 100U) / wallMs) : 0U;
        printf("[AUDIO] PULSE resource=%u timeMs=%u wallMs=%u "
               "speedPct=%u fifo=%u late=%u dmaWrites=%u dmaErrors=%u "
               "pakLease=loop-only src=PAK notesQueued=%u drumsQueued=%u "
               "programsQueued=%u eventsQueued=%u "
           "pcmPeak8=%u pcmLimited=%u pcmClipped=%u pcmQuiet=%u\n",
               (unsigned)music.resourceId,
               (unsigned)audioMs, (unsigned)wallMs, (unsigned)ratioPct,
               (unsigned)uxQueueMessagesWaiting(eventQueue),
               (unsigned)__atomic_load_n(&lateEvents, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&audioWrites, __ATOMIC_RELAXED),
               (unsigned)__atomic_load_n(&audioErrors, __ATOMIC_RELAXED),
               (unsigned)music.queuedNotes, (unsigned)music.queuedDrums,
               (unsigned)music.queuedPrograms, (unsigned)music.queuedEvents,
           (unsigned)__atomic_load_n(&pcmPeak8, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&pcmLimitedSamples, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&pcmClippedSamples, __ATOMIC_RELAXED),
           (unsigned)__atomic_load_n(&pcmQuietSamples, __ATOMIC_RELAXED));
    }
}


extern "C" uint8_t EspNativeAudioMidi_getVolume(void) {
    return (uint8_t)__atomic_load_n(&masterPercent, __ATOMIC_ACQUIRE);
}
extern "C" int EspNativeAudioMidi_setVolume(uint8_t percent) {
    if (percent != 25U && percent != 50U &&
        percent != 75U && percent != 100U) return 0;
    __atomic_store_n(&masterPercent, (uint32_t)percent, __ATOMIC_RELEASE);
    printf("[AUDIOVOLUME] APPLY percent=%u output=dac26 mixed-before-clip\n",
           (unsigned)percent);
    return 1;
}
