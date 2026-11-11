#!/usr/bin/env python3
"""Execute the production DAC Q8 math, not a Python approximation."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
cc = shutil.which("cc")
assert cc, "host C compiler required"
source = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_audio_pcm_math.h"
int main(void) {
    uint8_t limited = 9, clipped = 9;
    assert(EspNativeAudioPcm_toDacWord(0) == (128U << 8));
    assert(EspNativeAudioPcm_voiceQ8(127, 127, 128) == 4032);
    assert(EspNativeAudioPcm_voiceQ8(-127, 127, 128) == -4032);
    /* Low-level notes at 25% must retain fractional signal until DAC output. */
    assert(EspNativeAudioPcm_toDacWord(EspNativeAudioPcm_scaleLimitQ8(
        EspNativeAudioPcm_voiceQ8(50, 80, 128), 25, &limited, &clipped
    )) == (129U << 8));
    assert(!limited && !clipped);
    /* 12 simultaneous MIDI voices must enter a soft knee, not hit the rail. */
    const int32_t allVoices = 12 * EspNativeAudioPcm_voiceQ8(127, 127, 128);
    const int32_t value = EspNativeAudioPcm_scaleLimitQ8(
        allVoices, 100, &limited, &clipped);
    assert(limited == 1 && clipped == 0);
    assert(value == 30528);
    assert(EspNativeAudioPcm_toDacWord(value) == (247U << 8));
    /* Negative knee must be symmetric with positive, preserving DAC center. */
    const int32_t negative = EspNativeAudioPcm_scaleLimitQ8(
        -allVoices, 100, &limited, &clipped);
    assert(negative == -value && limited && !clipped);
    assert(EspNativeAudioPcm_toDacWord(negative) == (9U << 8));
    assert(EspNativeAudioPcm_scaleLimitQ8(0, 25, &limited, &clipped) == 0);
    assert(!limited && !clipped);
    /* Defensive rail applies to malformed/unbounded caller arithmetic. */
    assert(EspNativeAudioPcm_scaleLimitQ8(300000, 100, &limited, &clipped) ==
        (127 * 256) && limited && clipped);
    assert(EspNativeAudioPcm_scaleLimitQ8(-300000, 100, &limited, &clipped) ==
        (-127 * 256) && limited && clipped);
    for (int i = -(127 * 256); i <= (127 * 256); i += 127) {
        const uint16_t word = EspNativeAudioPcm_toDacWord(i);
        assert((word & 255U) == 0);
        assert((word >> 8) >= 1U && (word >> 8) <= 255U);
        const uint16_t mirror = EspNativeAudioPcm_toDacWord(-i);
        assert((int)(word >> 8) + (int)(mirror >> 8) == 256);
    }
    puts("PASS AUDIO-A production C Q8 volume, symmetric knee, no 12-voice clip");
    return 0;
}
"""
with tempfile.TemporaryDirectory() as work:
    src = Path(work) / "audio.c"
    elf = Path(work) / "audio"
    src.write_text(source)
    subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-O2", "-I", str(ROOT / "src"), str(src),
                    "-o", str(elf)], check=True)
    subprocess.run([str(elf)], check=True)
cpp = (ROOT / "src/esp_native_audio_midi.cpp").read_text()
assert "EspNativeAudioPcm_voiceQ8(" in cpp
assert "EspNativeAudioPcm_scaleLimitQ8(" in cpp
assert "EspNativeAudioPcm_toDacWord(" in cpp
assert "i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE)" in cpp
assert "i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)" in cpp
assert cpp.index("__atomic_store_n(&clockProbeState, accepted") < cpp.index(
    "i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)")
assert "kAudioDiagStageSamples = 2U * kSampleRate" in cpp
assert "diag == kAudioDiagDacOff || diag == kAudioDiagDacMidpoint" in cpp
assert "pcm[i] = (uint16_t)(128U << 8)" in cpp
assert "kAudioDiagMidpoint" not in cpp
assert cpp.index("kAudioDiagDacOff") < cpp.index("kAudioDiagDacMidpoint") < cpp.index("kAudioDiagMidi")
assert 'stage=%s gpio26=%s' in cpp
assert "v.patch < 48U && v.note <= 105U" in cpp
assert "v.patch >= 80U && v.note <= 97U" in cpp
assert "pcmClipped=%u" in cpp and "pcmQuiet=%u" in cpp
assert "cfg.use_apll = true;" in cpp
print("PASS AUDIO-A DAC muted in warmup; bandlimiting and PCM diagnostics wired")
