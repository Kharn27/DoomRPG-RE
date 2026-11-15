#include <Arduino.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdint.h>
#include <stdio.h>

#include "esp_audio_r1_cw.h"

/*
 * AUDIO-R1h: isolate I2S/DMA from the DAC2/GPIO26/SC8002B analog path.
 * The ESP32 hardware DAC cosine oscillator runs without any PCM buffer,
 * audio task, i2s_driver_install, ISR or per-sample CPU writes.
 *
 * All operations are confined to DAC_CHANNEL_2, physically GPIO26.
 * DAC_CHANNEL_1 / GPIO25 remains untouched for the CYD touch clock.
 * Boot-only test: there is intentionally no music/SD/gameplay owner.
 */
namespace {
constexpr uint32_t kCosineFrequencyHz = 440U;
constexpr uint32_t kCosineHoldMs = 8000U;
constexpr uint32_t kNeutralHoldMs = 2000U;
constexpr uint8_t kMidpointCode = 128U;
constexpr dac_channel_t kSpeakerDac = DAC_CHANNEL_2;
}

void EspAudioR1_runCosineColdBootProbe(void) {
    printf("[AUDIOCW] BEGIN trigger=COLD-BOOT after=I2S-R1g-released "
           "source=internal-DAC-cosine noI2S=yes noDMA=yes "
           "dac=GPIO26-only gpio25=untouched\n");
    bool padEnabled = false;
    bool oscillatorEnabled = false;
    esp_err_t failure = ESP_OK;
    const char* failAt = "none";

    do {
        /* The previous R1g probe has already uninstalled I2S0.
         * Start DC-centered and leave GPIO25 undisturbed. */
        failure = dac_output_enable(kSpeakerDac);
        if (failure != ESP_OK) { failAt = "dac_output_enable"; break; }
        padEnabled = true;

        failure = dac_output_voltage(kSpeakerDac, kMidpointCode);
        if (failure != ESP_OK) { failAt = "dac_output_voltage"; break; }
        vTaskDelay(pdMS_TO_TICKS(kNeutralHoldMs));

        dac_cw_config_t cfg = {};
        cfg.en_ch = kSpeakerDac;
        cfg.scale = DAC_CW_SCALE_2; // ~half full DAC swing, approximate +/-64 codes
        cfg.phase = DAC_CW_PHASE_0;
        cfg.freq = kCosineFrequencyHz;
        cfg.offset = 0; // no extra DC shift or saturation
        failure = dac_cw_generator_config(&cfg);
        if (failure != ESP_OK) { failAt = "dac_cw_generator_config"; break; }

        /* No serial output while the hardware oscillator is active.
         * Startup/stop edges may click; count any INTERNAL periodic crackle
         * during the steady 8-second middle of the tone instead. */
        printf("[AUDIOCW] TONE-ARM freq=440Hz source=hardware-cosine "
               "scale=1/2 offset=0 durationMs=8000\n");
        failure = dac_cw_generator_enable();
        if (failure != ESP_OK) { failAt = "dac_cw_generator_enable"; break; }
        oscillatorEnabled = true;
        vTaskDelay(pdMS_TO_TICKS(kCosineHoldMs));

        failure = dac_cw_generator_disable();
        oscillatorEnabled = false;
        if (failure != ESP_OK) { failAt = "dac_cw_generator_disable"; break; }
        failure = dac_output_voltage(kSpeakerDac, kMidpointCode);
        if (failure != ESP_OK) { failAt = "dac_midpoint_tail"; break; }
        vTaskDelay(pdMS_TO_TICKS(kNeutralHoldMs));
    } while (false);

    /* Fail closed; never leave the oscillator/analog output enabled
     * while the normal SD+TFT+Doom startup takes ownership of the board. */
    if (oscillatorEnabled) (void)dac_cw_generator_disable();
    if (padEnabled) (void)dac_output_disable(kSpeakerDac);
    printf("[AUDIOCW] END result=%s code=%d failAt=%s "
           "oscillator=OFF dacPad=OFF next=normal-game-boot\n",
           failure == ESP_OK ? "PASS-TRANSPORT" : "FAIL-CLOSED",
           (int)failure, failAt);
}
