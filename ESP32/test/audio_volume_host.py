#!/usr/bin/env python3
"""AUDIO-A sound-volume compact codec, UI, and shared audio-owner contract."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
inc=root/"ESP32/include"
with tempfile.TemporaryDirectory(prefix="drpg-sound-volume-") as temp:
    binary=Path(temp)/"codec"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
        "-I",str(inc),str(root/"ESP32/test/audio_volume_codec_host.c"),
        str(src/"esp_native_audio_volume_codec.c"),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
ui=(src/"native_main_menu_options_back.c").read_text()
paint=(src/"native_main_menu_options_action.c").read_text()
audio=(src/"esp_native_audio_midi.cpp").read_text()
owner=(src/"esp_native_audio_volume.cpp").read_text()
boot=(src/"main.cpp").read_text()
video=(src/"esp_native_config.cpp").read_text()
assert 'mask |= (1U << OPTIONS_VIDEO_ITEM) | (1U << OPTIONS_SOUND_ITEM);' in paint
assert 'selectedItem > OPTIONS_SOUND_ITEM' in paint
assert 'DoomRPG_esp32PaintMainMenuSoundDashboard(' in paint
assert 'EspNativeAudioMidi_getVolume()' in paint
assert 'if (soundOpen) {' in ui
assert 'if (item == OPTIONS_SOUND_ITEM)' in ui
assert 'soundOpen = 1U;' in ui
assert 'EspNativeAudioVolume_savePercent(after)' in ui
assert 'paintArmedPreference(4, item)' in ui
assert 'autoBack=Options persistence=audio-sd-v1' in ui
assert 'masterPercent = 100U;' in audio
assert 'mixed = (mixed * static_cast<int32_t>(master)) / 100;' in audio
assert audio.index('mixed = (mixed * static_cast<int32_t>(master)) / 100;') < audio.index('if (mixed < -127)')
assert 'EspNativeAudioMidi_setVolume' in audio
assert 'EspNativeAudioVolume_loadAtBoot(sdReady ? 1 : 0);' in boot
assert '/DoomRPG-ESP32.audio.cfg' in owner
assert 'SD.rename(kTmp, kPrimary)' in owner
assert '(void)EspNativeAudioMidi_setVolume(percent);' in owner
assert '/DoomRPG-ESP32.cfg' in video
assert '/DoomRPG-ESP32.audio.cfg' not in video
assert 'ESP_NATIVE_CONFIG_VERSION 3U' in (inc/"esp_native_config_codec.h").read_text()
print("PASS AUDIO-A Options > Sound + 4 preset volume/atomic DAC gain + independent SD")
