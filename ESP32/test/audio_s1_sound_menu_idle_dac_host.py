#!/usr/bin/env python3
"""AUDIO-S1: SOUND ON/OFF state, bounded menu nesting, idle DAC invariant."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
menu=(src/"native_main_menu_options_back.c").read_text()
paint=(src/"native_main_menu_options_action.c").read_text()
sound=(src/"esp_native_audio_intent.c").read_text()
music=(src/"esp_audio_entrance_music.cpp").read_text()
main=(src/"main.cpp").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()

assert "static uint8_t soundMenuOpen = 0U;" in menu
assert "static uint8_t soundOpen = 0U;" in menu
assert "if (soundOpen) {" in menu and "if (soundMenuOpen) {" in menu
assert "DoomRPG_esp32PaintMainMenuSoundDashboardSelection" in menu
assert "soundMenuOpen=1U;" in menu and "soundOpen=1U;" in menu
assert 'VOLUME-BACK to=SOUND' in menu and 'autoBack=SOUND' in menu
assert 'TOGGLE %s->%s volumeRetained' in menu
assert 'paintSoundMenuState(0,0)' in menu and 'paintSoundState(-1)' in menu
assert '"BACK", EspNativeAudioIntent_isSoundEnabled()?"ON":"OFF", "VOLUME"' in paint
assert "validateOptionsModel(EspNativeRootOwners_live()->menuSystem)" in paint
assert 'EspNativeAudioIntent_setSoundEnabled' in menu
assert 'EspNativeAudioIntent_setMusicVolumePercent' in menu
assert "EspEntranceMusic_syncIdleOutput();" in main
assert main.index("EspEntranceMusic_syncIdleOutput();") < main.index("initializeGameAssets();")
assert 'if (!entrance || !soundOn)' in music
assert 'closeMusic(soundOn?"SESSION_EXIT":"SOUND_OFF")' in music
assert 'EspEntranceMusic_syncIdleOutput(); /* SOUND ON' in music
assert "phase!=kSilent || timerInit || ring!=nullptr" in music
assert "dac_output_voltage(DAC_CHANNEL_2,128U)" in music
assert "if (dacEnabled)" in music and "dac_output_disable(DAC_CHANNEL_2)" in music
assert 'kRingCapacity=16384U' in music and 'kPrimeFrames=8192U' in music
assert 'if (!attempted) begin();' in music
assert "malloc" not in sound and "SD." not in sound
isr=music[music.index("static bool IRAM_ATTR onAudioTick"):music.index("static uint32_t queued()")]
assert "EspNativeAudioIntent_isSoundEnabled" not in isr
assert "EspEntranceMusic_syncIdleOutput" not in isr
assert "Test AUDIO-S1" in ci

code=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_native_audio_intent.h"
int main(void) {
    assert(EspNativeAudioIntent_isSoundEnabled()==1);
    assert(EspNativeAudioIntent_musicVolumePercent()==100U);
    assert(EspNativeAudioIntent_setMusicVolumePercent(50U));
    assert(EspNativeAudioIntent_setSoundEnabled(0));
    assert(EspNativeAudioIntent_isSoundEnabled()==0);
    assert(EspNativeAudioIntent_musicVolumePercent()==50U);
    assert(!EspNativeAudioIntent_setSoundEnabled(2));
    assert(EspNativeAudioIntent_isSoundEnabled()==0);
    assert(EspNativeAudioIntent_setMusicVolumePercent(75U));
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_isSoundEnabled()==0);
    assert(EspNativeAudioIntent_musicVolumePercent()==75U);
    assert(EspNativeAudioIntent_setSoundEnabled(1));
    assert(EspNativeAudioIntent_isSoundEnabled()==1);
    assert(EspNativeAudioIntent_musicVolumePercent()==75U);
    assert(EspNativeAudioIntent_publish(5046U,0U,3U));
    assert(EspNativeAudioIntent_count()==1U);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-s1-") as td:
    c=Path(td)/"sound.c"
    exe=Path(td)/"sound"
    c.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(c),str(src/"esp_native_audio_intent.c"),
                    "-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)
print("PASS AUDIO-S1: independent ON/OFF + retained volume, nested native UI and quiet DAC2")
