#!/usr/bin/env python3
"""AUDIO-SFX-G1: uniform 8/7 PCM boost, bounded DAC, music/ISR unchanged."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
mix=(src/"esp_native_sfx_pcm_mix.h").read_text()
music=(src/"esp_audio_entrance_music.cpp").read_text()
assert "P2 families; G1:" in mix
assert "if (code<16) code=16;" in mix and "if (code>240) code=240;" in mix
assert "EspNativeSfxPcm_mixGain(ring[slot],pcm,id)" in music
assert music.count("EspNativeSfxPcm_unclampedCode(")==2
assert music.count("[SFXGAIN]")==2
assert "source=producer-precise" in music
assert "gainDiv=14 peakCodes=112" in music
assert "EspNativeAudioIntent_musicVolumePercent()" in music
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):music.index("static uint32_t queued()")]
assert "EspNativeSfxPcm" not in isr
assert "malloc" not in mix and "SD.open" not in mix
code=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_native_sfx_pcm_mix.h"
int main(void) {
    const uint16_t old[3]={320,128,256};
    const uint16_t ids[3]={5046,5060,5045};
    const uint16_t now[3]={280,112,224};
    for (unsigned i=0;i<3;i++) {
        assert(EspNativeSfxPcm_gainDiv(ids[i])==now[i]);
        assert(now[i]*8U==old[i]*7U);
        assert(EspNativeSfxPcm_mixGain(128,8192,ids[i])>
               128U+8192U/old[i]);
        assert(EspNativeSfxPcm_mixGain(128,-8192,ids[i])<
               128-(int)(8192U/old[i]));
        for (unsigned m=16;m<=240;m+=8) {
            for (int32_t p=-32768;p<32768;p+=257) {
                const uint8_t v=EspNativeSfxPcm_mixGain((uint8_t)m,(int16_t)p,ids[i]);
                assert(v>=16U && v<=240U);
            }
        }
    }
    assert(EspNativeSfxPcm_gainDiv(5081)==512U);
    assert(EspNativeSfxPcm_gainDiv(5061)==512U);
    assert(EspNativeSfxPcm_mix(128,4096)==136U);
    assert(EspNativeSfxPcm_mixGain(128,0,5045)==128U);
    assert(EspNativeSfxPcm_unclampedCode(230,32767,5046)>240);
    assert(EspNativeSfxPcm_unclampedCode(25,-32768,5060)<16);
    assert(EspNativeSfxPcm_unclampedCode(128,0,5045)==128);
    assert(EspNativeSfxPcm_mixGain(230,32767,5046)==240);
    assert(EspNativeSfxPcm_mixGain(25,-32768,5060)==16);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-g1-") as td:
    c=Path(td)/"sfx_gain.c"
    exe=Path(td)/"sfx_gain"
    c.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(c),"-o",str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],cwd=root,check=True)
print("PASS AUDIO-SFX-G1: +14.3% for 5045/5046/5060, DAC clamped, ISR untouched")
