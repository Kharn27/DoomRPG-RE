#!/usr/bin/env python3
"""Guard physical I2S frame packing and MIDI clock handoff for classic CYD.

No host emulation can prove the actual DAC rate. Hardware PULSE compares
sample time with wall time; a ratio near 100% is required on the CYD.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
midi = (ROOT / "ESP32/src/esp_native_audio_midi.cpp").read_text()
m = re.search(r"constexpr uint32_t kSampleRate = (\d+)U;", midi)
b = re.search(r"constexpr uint32_t kBufferSamples = (\d+)U;", midi)
slots = re.search(r"constexpr uint32_t kDacSlotsPerFrame = (\d+)U;", midi)
assert m and b and slots
hz, frames, lanes = int(m[1]), int(b[1]), int(slots[1])
assert (hz, frames, lanes) == (16000, 160, 2)
assert frames * 1000 // hz == 10
assert frames * lanes * 2 == 640  # 16-bit I2S stereo per 10ms
assert 'uint16_t pcm[kBufferSamples * kDacSlotsPerFrame]' in midi
assert 'pcm[2U * i] = dac;' in midi
assert 'pcm[2U * i + 1U] = dac;' in midi
assert 'const uint16_t dac = static_cast<uint16_t>(mixed + 128) << 8;' in midi
assert 'cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;' in midi
assert 'i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)' in midi
assert 'i2s_set_pin(I2S_NUM_0, nullptr) != ESP_OK' not in midi
assert 'i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN)' not in midi
assert 'i2s_write(' in midi and 'pcm, sizeof(pcm), &written, portMAX_DELAY' in midi
assert 'driverRequestedHz=%u' in midi
# Source-level drift guards for the classic DAC overclock found on hardware.
# The 160MHz D2CLK / 6-bit MCLK divider cannot produce 512kHz MCLK.
assert 'cfg.use_apll = true;' in midi
assert 'cfg.use_apll = false;' not in midi
assert 'verifyDacClock(pcm);' in midi
assert 'esp_timer_get_time()' in midi
assert 'kClockMeasuredWrites = 80U' in midi
assert 'kClockMinAcceptHz = 14000U' in midi
assert 'kClockMaxAcceptHz = 18000U' in midi
assert 'nominalI2sGetClk=not-authoritative' in midi
assert 'result=%s threshold=14000..18000' in midi
assert 'stopMusic("dac-clock-probe-invalid")' in midi
assert 'vTaskDelay(pdMS_TO_TICKS(100))' in midi
# Clock validation is performed on core 0, not from the game input loop.
assert midi.index('verifyDacClock(pcm);') < midi.index(
    'const uint32_t master = __atomic_load_n(')
assert 'speedPct=%u' in midi and 'wallMs=%u' in midi
start = midi[midi.index('static int startMusic('):midi.index('} // namespace')]
assert start.index('refillMusic(kEventQueueCapacity)') < start.index(
    '__atomic_store_n(&enabled, 1U, __ATOMIC_RELEASE);')
assert 'static int refillMusic(unsigned maxEvents = kMaxEventsPerService)' in midi
assert 'kEventQueueCapacity = 96U;' in midi
assert 'kMaxEventsPerService = 24U;' in midi
assert 'EspAssetPack_isSourceProbeActive()' in midi
assert '(void)EspNativeAudioMidi_setVolume(percent);' in (
    ROOT / "ESP32/src/esp_native_audio_volume.cpp").read_text()
assert 'EspNativeAudioMidi_service();' in (ROOT / "ESP32/src/main.cpp").read_text()
print("PASS AUDIO-A stereo 640B/160 frames/10ms, GPIO26-only, prefill-before-clock and physical ratio diagnostic")
