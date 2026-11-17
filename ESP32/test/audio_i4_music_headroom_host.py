#!/usr/bin/env python3
"""AUDIO-I4: tiny per-track gain reduction without touching original decoder ABI."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
music=(src/"esp_audio_entrance_music.cpp").read_text()
header=(src/"esp_audio_entrance_adpcm.h").read_text()
assert "kIntroGainDiv=33U;" in music
assert "kEntranceGainDiv=15U;" in music
assert "activeIntroTrack ? kIntroGainDiv" in music
assert "activeMenuTrack ? kMenuGainDiv : kEntranceGainDiv" in music
assert "if (gainDiv==14U) return EspEntranceAdpcm_toDac(pcm,firstFrame);" in header
assert "gainDiv!=15U && gainDiv!=32U && gainDiv!=33U" in header
assert "if (gainDiv!=14U && gainDiv!=15U && gainDiv!=32U && gainDiv!=33U)" in header
assert "kRingCapacity=16384U" in music
assert "Test AUDIO-I4" in (root/".github/workflows/esp32-cyd.yml").read_text()
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):
          music.index("static uint32_t queued()")]
assert "gainDiv" not in isr and "SD." not in isr and "esp_timer" not in isr
test=r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_entrance_adpcm.h"
int main(void) {
    uint8_t block[8]={0xb8,0x0b,0,0,0,0,0,0};
    uint8_t oldEnt[9],oldIntro[9],ent[9],intro[9];
    EspEntranceAdpcmDecodeStats a,b,c,d;
    assert(EspEntranceAdpcm_decodeBlockObserved(block,8,9,oldEnt,9,4410,&a));
    assert(EspEntranceAdpcm_decodeBlockObservedGain(block,8,9,oldIntro,9,4410,32,&b));
    assert(EspEntranceAdpcm_decodeBlockObservedGain(block,8,9,ent,9,4410,15,&c));
    assert(EspEntranceAdpcm_decodeBlockObservedGain(block,8,9,intro,9,4410,33,&d));
    for (int i=0;i<9;i++) {
        assert(oldEnt[i]==238 && ent[i]==237);
        assert(oldIntro[i]==221 && intro[i]==218);
    }
    assert(a.minPcm==3000 && b.minPcm==3000 &&
           c.minPcm==3000 && d.minPcm==3000);
    assert(a.gainLimited==9 && b.gainLimited==0 &&
           c.gainLimited==9 && d.gainLimited==0);
    assert(EspEntranceAdpcm_toDac(3000,4410)==238);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,14)==238);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,32)==221);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,15)==237);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,33)==218);
    for (int p=-32768;p<=32767;p+=71) {
        uint8_t x=EspEntranceAdpcm_toDacGain((int16_t)p,4410,15);
        uint8_t y=EspEntranceAdpcm_toDacGain((int16_t)p,4410,33);
        assert(x>=16 && x<=240 && y>=16 && y<=240);
    }
    assert(EspEntranceAdpcm_toDacGain(3000,4410,0)==128);
    assert(EspEntranceAdpcm_toDacGain(3000,4410,34)==128);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="audio-i4-") as d:
    f=Path(d)/"gain.c"; e=Path(d)/"gain"
    f.write_text(test)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(f),"-o",str(e)],check=True,cwd=root)
    subprocess.run([str(e)],check=True,cwd=root)
print("PASS AUDIO-I4: tiny track-only gain margin, original reference byte-exact")
