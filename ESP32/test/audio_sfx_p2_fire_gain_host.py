#!/usr/bin/env python3
"""AUDIO-SFX-P2: full 5045 from verified PAK, dynamic bounded continuation."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
music=(src/"esp_audio_entrance_music.cpp").read_text()
action=(src/"esp_native_gameplay_action_engine.c").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()
assert 'EspAssetPack_findEntry("5045.wav",&entry)' in music
assert 'meta.sampleFrames==6400U && meta.dataBytes==12800U' in music
assert 'fireSfx.output=17640U;' in music
assert 'fireSfx.next+=done;' in music
assert 'const uint32_t budget=available<remain?available:remain;' in music
assert "kSfxMinLead" in music and 'reason=reader-caught-up' in music
assert '[SFXP2] DEFER id=%u reason=pack-lease-open' in music
assert 'mixFireSfx();' in music and 'startFireSfx(reader,writer);' in music
assert 'EspNativeSfxPcm_mixGain(ring[slot],pcm,id)' in music
assert action.index("[ACTIONENGINE] FIRE-COMMIT") < action.index(
    'EspNativeAudioIntent_publish(5045U,0U,3U)')
assert "Test SFX-P2" in ci
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):music.index("static uint32_t queued()")]
assert "sfx" not in isr.lower() and "EspAssetPack" not in isr
code=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_native_sfx_pcm_mix.h"
int main(void) {
    assert(EspNativeSfxPcm_outputFrames(6400,22050)==17640);
    assert(17640U+1024U>16384U);
    assert(EspNativeSfxPcm_outputFrames(4160,22050)==11466);
    assert(EspNativeSfxPcm_outputFrames(3520,22050)==9702);
    assert(EspNativeSfxPcm_gainDiv(5045)==224);
    assert(EspNativeSfxPcm_gainDiv(5046)==280);
    assert(EspNativeSfxPcm_gainDiv(5060)==112);
    assert(EspNativeSfxPcm_gainDiv(5061)==512);
    assert(EspNativeSfxPcm_mixGain(128,32000,5046)==240);
    assert(EspNativeSfxPcm_mixGain(128,4096,5060)==164);
    assert(EspNativeSfxPcm_mixGain(128,-4096,5060)==92);
    assert(EspNativeSfxPcm_mixGain(230,32767,5045)==240);
    assert(EspNativeSfxPcm_mixGain(25,-32768,5045)==16);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-sfx-p2-") as td:
    c=Path(td)/"mix.c"
    exe=Path(td)/"mix"
    c.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(c),"-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)
print("PASS AUDIO-SFX-P2/G1: original 5045 full stream and updated DAC family gains")
