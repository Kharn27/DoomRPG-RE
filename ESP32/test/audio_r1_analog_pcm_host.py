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
    assert(EspAudioR1Pcm_word(0, 0) == 0x8000u);
    assert(EspAudioR1Pcm_word(0x40000000u, 24) == 0x9800u);
    assert(EspAudioR1Pcm_word(0x40000000u, 4) == 0x8400u);
    assert(EspAudioR1Pcm_word(0xC0000000u, 4) == 0x7C00u);
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t phase = i << 24;
        uint8_t quiet = EspAudioR1Pcm_dacCode(phase, 4);
        uint8_t mid = EspAudioR1Pcm_dacCode(phase, 12);
        uint8_t loud = EspAudioR1Pcm_dacCode(phase, 24);
        assert(quiet >= 124 && quiet <= 132);
        assert(mid >= 116 && mid <= 140);
        assert(loud >= 104 && loud <= 152);
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
assert "kFadeFrames = 1600U" in cpp
assert "playStage(kTone4, 3U, 4U" in cpp
assert "playStage(kGap4To12, 1U, 0U" in cpp
assert "playStage(kTone12, 3U, 12U" in cpp
assert "playStage(kGap12To24, 1U, 0U" in cpp
assert "playStage(kTone24, 3U, 24U" in cpp
assert "I2S_DAC_CHANNEL_LEFT_EN" in cpp
print("PASS AUDIO-R1b: exact DAC peak4/12/24, 100ms fades, midpoint gaps, GPIO26 only")
