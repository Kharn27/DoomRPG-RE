#!/usr/bin/env python3
"""AUDIO-I2: attenuate ONLY M_IN_CIT before the soft knee, keep Entrance /14."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
header=(src/"esp_audio_entrance_adpcm.h").read_text()
music=(src/"esp_audio_entrance_music.cpp").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()

assert "constexpr uint16_t kIntroGainDiv=33U;" in music
assert "constexpr uint16_t kEntranceGainDiv=15U;" in music
assert "const uint16_t gainDiv=activeIntroTrack?kIntroGainDiv:kEntranceGainDiv;" in music
assert "EspEntranceAdpcm_decodeBlockObservedGain(raw,wav.blockAlign," in music
assert "gainDiv,&stats)" in music
assert "gainDiv=%u track=%s" in music
assert 'activeIntroTrack?"INTRO":"ENTRANCE"' in music
assert "EspNativeAudioIntent_musicVolumePercent()" in music
assert "kRingCapacity=16384U" in music
assert "static inline uint8_t EspEntranceAdpcm_limitedDac" in header
assert "static inline uint8_t EspEntranceAdpcm_toDacGain" in header
assert "static inline int EspEntranceAdpcm_decodeBlockObservedGain" in header
assert "static inline int EspEntranceAdpcm_decodeBlockObserved(" in header
assert "if (gainDiv!=14U && gainDiv!=15U && gainDiv!=32U && gainDiv!=33U)" in header
assert "if (scaled < -96 || scaled > 96) ++stats->gainLimited;" in header
assert 'Test AUDIO-I2 intro-only' in ci
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):music.index("static uint32_t queued()")]
assert "gainDiv" not in isr and "SD." not in isr
code=r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_entrance_adpcm.h"
int main(void) {
    /* 8 bytes -> 9 samples, predictor 3000 and zero deltas at step index 0.
     * Old /14 saturates the soft knee; new /32 stays entirely linear. */
    uint8_t block[8]={0xb8,0x0b,0,0,0,0,0,0};
    uint8_t legacy[9],baseline[9],intro[9];
    EspEntranceAdpcmDecodeStats a,b,c;
    assert(EspEntranceAdpcm_decodeBlockObserved(
        block,8,9,legacy,9,4410,&a));
    assert(EspEntranceAdpcm_decodeBlockObservedGain(
        block,8,9,baseline,9,4410,14U,&b));
    assert(EspEntranceAdpcm_decodeBlockObservedGain(
        block,8,9,intro,9,4410,32U,&c));
    assert(memcmp(legacy,baseline,9)==0);
    assert(memcmp(legacy,intro,9)!=0);
    assert(a.minPcm==3000 && a.maxPcm==3000);
    assert(c.minPcm==3000 && c.maxPcm==3000);
    assert(a.nonZeroPcm==9 && b.nonZeroPcm==9 && c.nonZeroPcm==9);
    assert(a.gainLimited==9 && b.gainLimited==9 && c.gainLimited==0);
    for (int i=0;i<9;i++) {
        assert(legacy[i]==238);
        assert(intro[i]==221);
    }
    assert(EspEntranceAdpcm_toDacGain(3000,4410,14U)==238);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,32U)==221);
    assert(EspEntranceAdpcm_toDacGain(3000,0,32U)==128);
    assert(EspEntranceAdpcm_toDacGain(32767,4410,32U)<240);
    assert(EspEntranceAdpcm_toDacGain(-32768,4410,32U)>16);
    assert(!EspEntranceAdpcm_decodeBlockObservedGain(
        block,8,9,intro,9,4410,0U,&c));
    assert(!EspEntranceAdpcm_decodeBlockObservedGain(
        block,8,9,intro,9,4410,33U,&c));
    /* No buffer or data offsets are changed by track mastering. */
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-i2-") as td:
    source=Path(td)/"test.c"
    binary=Path(td)/"test"
    source.write_text(code)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra",
                    "-Werror","-pedantic","-I",str(src),
                    str(source),"-o",str(binary)],check=True,cwd=root)
    subprocess.run([str(binary)],check=True,cwd=root)
print("PASS AUDIO-I2: /32 intro pre-knee, old /14 byte-identical, no ISR/SD changes")
