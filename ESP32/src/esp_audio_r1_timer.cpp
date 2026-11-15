#include <Arduino.h>
#include <driver/dac.h>
#include <driver/timer.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/rtc_io_reg.h>
#include <soc/soc.h>

#include <stdint.h>
#include <stdio.h>

#include "esp_audio_r1_timer.h"
#include "esp_audio_r1_pcm.h"

/* AUDIO-R1k - a strictly bounded, temporary DAC2-only PCM witness.
 * No I2S, DMA, SD, TFT, game loop, dynamic song buffer, or GPIO25 DAC.
 *
 * ESP32 timer group 1 / timer 1 ticks at APB 80MHz/2=40MHz.
 * Reload every 2500 counts gives 16000 sample interrupts per second.
 * ISR reads a 400-byte DRAM table (440Hz = 11 periods / 400 frames).
 * It writes the 8-bit DAC2 pad register; does NOT call dac_output_voltage()
 * or other flash-resident driver APIs from the interrupt.
 *
 * This is NOT an approved gameplay player. Preflight checks actual sample
 * ISR cadence with the DAC disabled. Hardware counter / ISR jitter still
 * needs validation on the physical CYD before audio can be considered good.
 */
namespace {
constexpr timer_group_t kGroup = TIMER_GROUP_1;
constexpr timer_idx_t kTimer = TIMER_1;
constexpr uint32_t kTimerDivider = 2U;
constexpr uint32_t kTimerAlarm = 2500U;
constexpr uint32_t kRateHz = 16000U;
constexpr uint32_t kPreflightFrames = kRateHz;
constexpr uint32_t kToneFrames = 8U * kRateHz;
constexpr uint32_t kFadeFrames = 3200U;
constexpr uint32_t kWaveFrames = 400U;
constexpr uint32_t kStep440 = 118111601U;
constexpr uint32_t kPollMs = 10U;
enum : uint32_t { kIdle = 0, kClock = 1, kTone = 2, kDone = 3 };
static DRAM_ATTR uint8_t wave[kWaveFrames];
static volatile uint32_t isrStage;
static volatile uint32_t isrFrames;

static bool IRAM_ATTR onTick(void*) {
    const uint32_t state = isrStage;
    if (state == kClock) {
        if (isrFrames < kPreflightFrames) ++isrFrames;
    } else if (state == kTone) {
        const uint32_t frame = isrFrames;
        if (frame < kToneFrames) {
            int32_t delta = (int32_t)wave[frame % kWaveFrames] - 128;
            uint32_t gain = kFadeFrames;
            if (frame < gain) gain = frame;
            const uint32_t remaining = kToneFrames - frame - 1U;
            if (remaining < gain) gain = remaining;
            /* All intermediates fit 32-bit. No flash reads, FP or logging. */
            const int32_t code = 128 + (delta * (int32_t)gain) /
                                               (int32_t)kFadeFrames;
            REG_SET_FIELD(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC,
                          (uint32_t)code);
            isrFrames = frame + 1U;
        } else {
            REG_SET_FIELD(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, 128U);
            isrStage = kDone;
        }
    }
    return false;
}
} // namespace

void EspAudioR1_runTimerColdBootProbe(void) {
    printf("[AUDIOTIMER] BEGIN trigger=COLD-BOOT source=TG1T1 "
           "dac=GPIO26-only gpio25=untouched noI2S=yes noDMA=yes "
           "targetHz=16000 sampleTable=400B\n");
    bool initialized = false;
    bool callbackAdded = false;
    bool outputEnabled = false;
    bool passed = false;
    const char* reason = "none";
    int32_t errCode = 0;
    uint32_t rateHz = 0U;
    uint32_t finished = 0U;
    int64_t toneElapsedUs = 0LL;

    do {
        /* Wave table is in DRAM, filled before registering an IRAM ISR. */
        uint32_t phase = 0U;
        for (uint32_t i = 0U; i < kWaveFrames; ++i) {
            wave[i] = EspAudioR1Pcm_dacCode(phase, 64U);
            phase += kStep440;
        }

        timer_config_t cfg = {};
        cfg.divider = kTimerDivider;
        cfg.counter_dir = TIMER_COUNT_UP;
        cfg.counter_en = TIMER_PAUSE;
        cfg.alarm_en = TIMER_ALARM_EN;
        cfg.auto_reload = TIMER_AUTORELOAD_EN;
        cfg.intr_type = TIMER_INTR_LEVEL;
        esp_err_t ret = timer_init(kGroup, kTimer, &cfg);
        if (ret != ESP_OK) {
            reason = "timer_init"; errCode = (int32_t)ret; break;
        }
        initialized = true;
        if ((ret = timer_set_counter_value(kGroup, kTimer, 0ULL)) != ESP_OK ||
            (ret = timer_set_alarm_value(kGroup, kTimer, kTimerAlarm)) != ESP_OK ||
            (ret = timer_enable_intr(kGroup, kTimer)) != ESP_OK) {
            reason = "timer_config"; errCode = (int32_t)ret; break;
        }

        isrFrames = 0U;
        isrStage = kClock;
        ret = timer_isr_callback_add(
            kGroup, kTimer, onTick, nullptr,
            ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1);
        if (ret != ESP_OK) {
            reason = "callback_add"; errCode = (int32_t)ret; break;
        }
        callbackAdded = true;
        const int64_t clockStartUs = esp_timer_get_time();
        if ((ret = timer_start(kGroup, kTimer)) != ESP_OK) {
            reason = "clock_start"; errCode = (int32_t)ret; break;
        }
        while (isrFrames < kPreflightFrames) {
            vTaskDelay(pdMS_TO_TICKS(kPollMs));
        }
        const int64_t clockElapsedUs = esp_timer_get_time() - clockStartUs;
        (void)timer_pause(kGroup, kTimer);
        rateHz = clockElapsedUs > 0 ?
            (uint32_t)((uint64_t)kPreflightFrames * 1000000ULL /
                       (uint64_t)clockElapsedUs) : 0U;
        printf("[AUDIOTIMER] CLOCK measuredHz=%u frames=%u elapsedUs=%lld "
               "bounds=15000..17000 dac=OFF\n",
               (unsigned)rateHz, (unsigned)isrFrames,
               (long long)clockElapsedUs);
        if (rateHz < 15000U || rateHz > 17000U) {
            reason = "clock_range"; break;
        }

        ret = dac_output_enable(DAC_CHANNEL_2);
        if (ret != ESP_OK) {
            reason = "dac_enable"; errCode = (int32_t)ret; break;
        }
        outputEnabled = true;
        if ((ret = dac_output_voltage(DAC_CHANNEL_2, 128U)) != ESP_OK) {
            reason = "dac_midpoint"; errCode = (int32_t)ret; break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        if ((ret = timer_set_counter_value(kGroup, kTimer, 0ULL)) != ESP_OK) {
            reason = "timer_reset"; errCode = (int32_t)ret; break;
        }
        isrFrames = 0U;
        isrStage = kTone;
        printf("[AUDIOTIMER] TONE-ARM 440Hz peak64 durationMs=8000 "
               "source=direct-RTC-DAC2+hardware-timer fadeMs=200\n");
        const int64_t toneStartUs = esp_timer_get_time();
        if ((ret = timer_start(kGroup, kTimer)) != ESP_OK) {
            reason = "tone_start"; errCode = (int32_t)ret; break;
        }
        while (isrFrames < kToneFrames) {
            vTaskDelay(pdMS_TO_TICKS(kPollMs));
        }
        toneElapsedUs = esp_timer_get_time() - toneStartUs;
        finished = isrFrames;
        (void)timer_pause(kGroup, kTimer);
        isrStage = kIdle;
        ret = dac_output_voltage(DAC_CHANNEL_2, 128U);
        if (ret != ESP_OK) {
            reason = "dac_tail_midpoint"; errCode = (int32_t)ret; break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        if (finished != kToneFrames ||
            toneElapsedUs < 7600000LL || toneElapsedUs > 8400000LL) {
            reason = "tone_range"; break;
        }
        passed = true;
    } while (false);

    /* Shutdown happens before launching the separate hardware CW probe.
     * No callback survives into TFT/SD/gameplay init, even on failure. */
    isrStage = kIdle;
    if (initialized) (void)timer_pause(kGroup, kTimer);
    if (callbackAdded) (void)timer_isr_callback_remove(kGroup, kTimer);
    if (initialized) (void)timer_deinit(kGroup, kTimer);
    if (outputEnabled) {
        (void)dac_output_voltage(DAC_CHANNEL_2, 128U);
        (void)dac_output_disable(DAC_CHANNEL_2);
    }
    printf("[AUDIOTIMER] END result=%s reason=%s code=%ld "
           "measuredHz=%u playedFrames=%u toneElapsedUs=%lld "
           "timer=OFF dacPad=OFF next=hardware-cosine\n",
           passed ? "PASS-TRANSPORT" : "FAIL-CLOSED",
           reason, (long)errCode, (unsigned)rateHz, (unsigned)finished,
           (long long)toneElapsedUs);
}
