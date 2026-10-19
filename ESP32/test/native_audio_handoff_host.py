#!/usr/bin/env python3
"""Native intro/music handoff: no desktop Sound ABI, bounded audio commands.

This test executes the production EspNativeAudioIntent owner with host gcc.
It does not validate physical playback, SD samples or I2S timing.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ESP = ROOT / "ESP32"
BRIDGE = ESP / "src/esp_native_canvas_text.c"
AUDIO = ESP / "src/esp_native_audio_intent.c"

bridge = BRIDGE.read_text(encoding="utf-8")
native_state = (ESP / "src/native_main_menu_model.c").read_text(encoding="utf-8")
assert "Sound_playSound(" not in bridge + native_state
assert "Sound_stopSounds(" not in bridge + native_state
assert "EspNativeAudioIntent_stopAll();" not in bridge
assert "EspNativeAudioIntent_publish(" not in bridge
assert native_state.count("EspNativeAudioIntent_stopAll();") == 2
assert native_state.count("EspNativeAudioIntent_publish(") == 3
assert "5039U" in native_state
assert "ESP32_SND_FLG_LOOP | ESP32_SND_FLG_STOPSOUNDS" in native_state

harness = r"""
#include <assert.h>
#include <stddef.h>
#include "esp_native_audio_intent.h"

int main(void) {
    const EspNativeAudioIntent *intent;
    assert(sizeof(EspNativeAudioIntent) == 8);
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_last() == NULL);
    assert(EspNativeAudioIntent_count() == 0U);
    assert(EspNativeAudioIntent_publish(0U, 0U, 0U) == 0);
    assert(EspNativeAudioIntent_count() == 0U);

    assert(EspNativeAudioIntent_publish(5039U, 11U, 5U) == 1);
    intent = EspNativeAudioIntent_last();
    assert(intent != NULL && intent->sequence == 1U);
    assert(intent->resourceId == 5039U && intent->flags == 11U);
    assert(intent->priority == 5U);

    assert(EspNativeAudioIntent_stopAll() == 1);
    intent = EspNativeAudioIntent_last();
    assert(intent != NULL && intent->sequence == 2U);
    assert(intent->resourceId == 0U);
    assert(intent->flags == ESP_NATIVE_AUDIO_STOP_ALL_FLAG);
    assert(intent->priority == 0U);

    assert(EspNativeAudioIntent_publish(5067U, 0U, 3U) == 1);
    intent = EspNativeAudioIntent_last();
    assert(intent != NULL && intent->sequence == 3U);
    assert(intent->resourceId == 5067U && intent->priority == 3U);

    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_count() == 0U);
    assert(EspNativeAudioIntent_last() == NULL);
    return 0;
}
"""

cc = shutil.which("cc")
assert cc is not None, "Host C compiler required for native audio test"
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "audio_handoff.c"
    binary = Path(tmp) / "audio_handoff"
    src.write_text(harness, encoding="utf-8")
    subprocess.run(
        [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-I", str(ESP / "src"), str(AUDIO), str(src), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
print("PASS native audio handoff: no Sound ABI in canvas, 8B intents, ordered PLAY/STOP/PLAY")
