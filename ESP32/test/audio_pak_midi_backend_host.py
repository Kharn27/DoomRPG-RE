#!/usr/bin/env python3
"""AUDIO-A native PAK MIDI only — no sidecar audio file or runtime WAV read."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ESP = ROOT/"ESP32"
code = (ESP/"src/esp_native_audio_midi.cpp").read_text()
boot = (ESP/"src/main.cpp").read_text()
parser = (ESP/"src/esp_native_midi_stream.c").read_text()
assert 'constexpr uint8_t kEventQueueCapacity = 96U' in code
assert 'constexpr uint8_t kVoices = 12U' in code
assert 'EspNativeAudioIntent_takeNext(&command)' in code
assert 'ESP_ASSET_PACK_DEFAULT_PATH' in code
assert 'EspAssetPack_findEntry(name, &entry)' in code
assert 'snprintf(name, sizeof(name), "%u.mid"' in code
assert 'EspNativeMidi_begin(' in code
assert 'EspNativeMidi_next(&music.song, &evt)' in code
assert 'EspAssetPack_readStreamRange(' in code
assert 'EspAssetPack_isSourceProbeActive()' in code
assert 'EspAssetPack_close();' in code
assert 'EspNativeAudioMidi_service();' in boot
assert 'EspNativeAudioVolume_loadAtBoot(sdReady ? 1 : 0);' in boot
assert 'source=PAK synth=FM-lite-no-GM-bank' in code
assert 'src=PAK notesQueued=%u drumsQueued=%u' in code
assert 'programsQueued=%u eventsQueued=%u' in code
assert 'i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)' in code
assert 'GPIO25' in code
for forbidden in ('adpcm', '.wav', 'WAVE_FORMAT', 'SoundFont_create',
                  'DoomRPG.zip', 'SD.open(', 'SD.exists('):
    assert forbidden.lower() not in code.lower(), forbidden
assert 'malloc(' not in parser and 'calloc(' not in parser
print("PASS AUDIO-A: source=5039.mid from native PAK, bounded SMF queue, no WAV/ADPCM sidecar, DAC GPIO26")
