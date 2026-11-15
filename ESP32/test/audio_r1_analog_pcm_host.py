#!/usr/bin/env python3
"""AUDIO-R1 host: exercise the REAL PCM header and enforce narrow ownership."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = root / "src"
cpp = (src / "esp_audio_r1_analog.cpp").read_text(encoding="utf-8")
main = (src / "main.cpp").read_text(encoding="utf-8")
assert "EspAudioR1_tryStartInGameplay();" in main
assert "EspAudioR1_requestStop();" in main
assert "EspNativeGameplaySession_isActive()" in main
assert "I2S_DAC_CHANNEL_LEFT_EN" in cpp
assert "I2S_DAC_CHANNEL_DISABLE" in cpp
assert "I2S_CHANNEL_FMT_RIGHT_LEFT" in cpp
assert "cfg.use_apll = true;" in cpp
assert "GPIO25" in cpp
assert "i2s_set_pin(" not in cpp.replace("i2s_set_pin(NULL)", "").replace("i2s_set_pin(I2S_NUM_0, NULL)", "")
assert "SD.open(" not in cpp and "EspAssetPack_" not in cpp
assert "ESP_NATIVE_AUDIO" not in cpp and "MIDI" not in cpp.replace("MIDI synthesis", "")
assert "xTaskCreatePinnedToCore" in cpp and "4x160" in cpp
cc = shutil.which("cc")
assert cc
harness = r"""
#include <assert.h>
#include <stdint.h>
#include "esp_audio_r1_pcm.h"
int main(void) {
    assert(sizeof(kEspAudioR1Sine256) == 256);
    assert(EspAudioR1Pcm_dacCode(0, 0) == 128);
    assert(EspAudioR1Pcm_dacCode(0, 8) == 128);
    assert(EspAudioR1Pcm_dacCode(0x40000000u, 8) == 136);
    assert(EspAudioR1Pcm_dacCode(0xC0000000u, 8) == 120);
    assert(EspAudioR1Pcm_dacCode(0x40000000u, 12) == 140);
    assert(EspAudioR1Pcm_dacCode(0xC0000000u, 12) == 116);
    assert(EspAudioR1Pcm_dacCode(0x40000000u, 24) == 152);
    assert(EspAudioR1Pcm_dacCode(0xC0000000u, 24) == 104);
    assert(EspAudioR1Pcm_dacCode(0x40000000u, 32) == 160);
    assert(EspAudioR1Pcm_dacCode(0xC0000000u, 32) == 96);
    assert(EspAudioR1Pcm_dacCode(0x40000000u, 80) == 208);
    assert(EspAudioR1Pcm_dacCode(0xC0000000u, 80) == 48);
    assert(EspAudioR1Pcm_word(0, 0) == 0x8000u);
    assert(EspAudioR1Pcm_word(0x40000000u, 24) == 0x9800u);
    assert(EspAudioR1Pcm_word(0x40000000u, 80) == 0xD000u);
    assert(EspAudioR1Pcm_word(0x40000000u, 4) == 0x8400u);
    assert(EspAudioR1Pcm_word(0xC0000000u, 4) == 0x7C00u);
    EspAudioR1PcmShapeState sh = {0};
    assert(EspAudioR1Pcm_shapedWord(0, 0, &sh) == 0x8000u);
    assert(sh.errorQ16 == 0);
    assert(EspAudioR1Pcm_shapedWord(0x40000000u, 64, &sh) == 0xC000u);
    assert(EspAudioR1Pcm_shapedWord(0xC0000000u, 64, &sh) == 0x4000u);
    assert(sh.errorQ16 == 0);
    for (uint32_t p = 0; p < 256; ++p) {
        uint32_t phase = p << 24;
        uint16_t shaped = EspAudioR1Pcm_shapedWord(phase, 64, &sh);
        assert((shaped & 255u) == 0);
        assert((shaped >> 8) >= 64u && (shaped >> 8) <= 192u);
        assert(sh.errorQ16 >= -32768 && sh.errorQ16 <= 32768);
    }
    sh.errorQ16 = 0;
    uint32_t phase440 = 0;
    uint32_t phase220 = 0;
    uint32_t differences440 = 0;
    uint32_t differences220 = 0;
    for (int n = 0; n < 16000; ++n) {
        uint16_t original = EspAudioR1Pcm_word(phase440, 64);
        uint16_t shaped = EspAudioR1Pcm_shapedWord(phase440, 64, &sh);
        if (original != shaped) ++differences440;
        assert(shaped >= 0x4000u && shaped <= 0xC000u);
        assert(sh.errorQ16 >= -32768 && sh.errorQ16 <= 32768);
        phase440 += 118111601u;
        phase220 += 59055800u;
        (void)phase220;
    }
    assert(differences440 > 0u && differences440 < 16000u);
    sh.errorQ16 = 0;
    uint32_t phase = 0;
    for (int n = 0; n < 16000; ++n) {
        uint16_t original = EspAudioR1Pcm_word(phase, 64);
        uint16_t shaped = EspAudioR1Pcm_shapedWord(phase, 64, &sh);
        if (original != shaped) ++differences220;
        phase += 59055800u;
    }
    assert(differences220 > 0u && differences220 < 16000u);
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t phase = i << 24;
        uint8_t quiet = EspAudioR1Pcm_dacCode(phase, 8);
        uint8_t mid = EspAudioR1Pcm_dacCode(phase, 32);
        uint8_t loud = EspAudioR1Pcm_dacCode(phase, 80);
        assert(quiet >= 120 && quiet <= 136);
        assert(mid >= 96 && mid <= 160);
        assert(loud >= 48 && loud <= 208);
        assert(EspAudioR1Pcm_dacCode(phase, 250) == loud);
        assert((EspAudioR1Pcm_word(phase, 8) & 255u) == 0u);
    }
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    c.write_text(harness, encoding="utf-8")
    subprocess.run([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(src), str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
assert "kFadeFrames = 3200U" in cpp
assert "kStep440 = 118111601U" in cpp
assert "kStep220 = 59055800U" in cpp
assert "playStage(kDacOff, 3U, 0U" in cpp
assert "playStage(kMidpoint, 2U, 0U" in cpp
assert "playStage(kTone440Hold, 8U, 64U, kStep440" in cpp
assert "playStage(kTone220Hold, 8U, 64U, kStep220" in cpp
assert "playStage(kSilence, 2U, 0U" in cpp
assert "playStage(kDacOffTail, 3U, 0U" in cpp
assert cpp.count("i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE)") >= 2
assert "I2S_DAC_CHANNEL_LEFT_EN" in cpp
# The R1d SHAPED math remains covered by the C harness above, but R1f
# deliberately runs only the same baseline quantizer at both pitches.
print("PASS AUDIO-R1f PCM regression: baseline and shaped quantizers bounded")

# R1f: same DAC path, identical numeric amplitude, 8s sustained frequency A/B.
assert "calibration=R1f waveform=BASE-both" in cpp
assert "i2s_driver_install(I2S_NUM_0, &cfg, 8, &i2sEvents)" in cpp
assert "I2S_EVENT_DMA_ERROR" in cpp
assert "I2S_EVENT_TX_Q_OVF" in cpp
assert "i2sEvents = nullptr" in cpp
assert "if (stage < kFinished) return;" in cpp
assert "maxFillUs" in cpp and "maxInterWriteGapUs" in cpp
assert "maxProducerGapUs=" in cpp and "fillAtFrame=" in cpp
assert "for (uint32_t at = kDacOff; at <= kDacOffTail; ++at)" in cpp
assert "playStage(kTone440Hold, 8U, 64U, kStep440" in cpp
assert "playStage(kTone220Hold, 8U, 64U, kStep220" in cpp
assert "playStage(kGap440To220, 2U, 0U, kStep440" in cpp
assert "440Hz-BASE-HOLD8S-PEAK64" in cpp
assert "220Hz-BASE-HOLD8S-PEAK64" in cpp
assert "EspAudioR1Pcm_shapedWord(" not in cpp
print("PASS AUDIO-R1f: two sustained pitch controls, same DAC quantizer, deferred DMA logs")
