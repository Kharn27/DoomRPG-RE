#!/usr/bin/env python3
"""AUDIO-DOOR: original 5063/5064 from successful native door commits only."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = root / "ESP32/src"
inc = root / "ESP32/include"
door = (src / "esp_native_door_sfx.h").read_text()
select = (src / "esp_native_resident_gameplay.c").read_text()
move = (src / "esp_native_gameplay_move_events.c").read_text()
map_state = (inc / "esp_map_line_state.h").read_text()
legacy = (root / "src/Game.c").read_text()

# Do not invent sounds: original desktop Game_performDoorEvent uses exactly
# 5063 open, 5064 close, flags=0 and priority=2.
legacy_door = legacy.split("boolean Game_performDoorEvent(", 1)[1].split(
    "void Game_deleteSaveFiles(", 1)[0]
assert "sound = 5064;" in legacy_door and "sound = 5063;" in legacy_door
assert "Sound_playSound(game->doomRpg->sound, sound, 0, 2);" in legacy_door
assert "#define ESP_MAP_LINE_SOUND_OPEN 5063U" in map_state
assert "#define ESP_MAP_LINE_SOUND_CLOSE 5064U" in map_state
assert "EspNativeAudioIntent_publish(soundId, 0U, 2U)" in door
assert "openBefore == openAfter" in door and "soundId != expected" in door
assert "(effectFlags & ESP_MAP_LINE_EFFECT_PLAY_SOUND) == 0U" in door

assert '#include "esp_native_door_sfx.h"' in select
assert '#include "esp_native_door_sfx.h"' in move
sel = select.split("if (status == ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK) {", 1)[1]
sel = sel.split("if (status == ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED", 1)[0]
assert sel.index("EspNativeGameplayAction_rollbackSelect") < sel.index(
    "EspNativeDoorSfx_publishCommitted")
assert sel.index("EspNativeGameplayTransition_finishDoor") < sel.index(
    "EspNativeDoorSfx_publishCommitted")
assert sel.index("EspNativeGameplayMonsterTurn_requestSelectDoor") < sel.index(
    "EspNativeDoorSfx_publishCommitted")
assert "doorIndex < result.doorCount" in sel

# The MOVE wrapper may speculative-apply both EXIT/ENTER commands; all SFX
# must follow the final successfully published frame or modal/message.
assert "static void publishCommittedMoveDoorPhase(" in move
assert "mixedBatchOwner.committedCount != mixedBatchOwner.count" in move
assert "step->handled != 0U && step->mutated != 0U" in move
assert "publishCommittedMoveDoorPhase(&transaction.exitResult);" in move
assert "publishCommittedMoveDoorPhase(&transaction.enterResult);" in move
frame = move.split("void EspNativeGameplayMoveEvents_onFrameResult(", 1)[1]
frame = frame.split("int EspNativeGameplayMoveEvents_pendingDialog(", 1)[0]
assert frame.index("if (renderOk)") < frame.index(
    'printf("[MOVEEVENT] COMMIT')
assert frame.index('printf("[MOVEEVENT] COMMIT') < frame.index(
    "publishCommittedMoveDoorSounds();")
assert frame.index("publishCommittedMoveDoorSounds();") < frame.index(
    'releaseShowBatchOwnerForTransaction("frame-commit")')
dialog = move.split("int EspNativeGameplayMoveEvents_finishPendingDialog(", 1)[1]
dialog = dialog.split("static int currentMoveMapEntry(", 1)[0]
assert dialog.index('printf("[MOVEEVENT] COMMIT') < dialog.index(
    "publishCommittedMoveDoorSounds();")
message = move.split("int EspNativeGameplayMoveEvents_finishPendingMessage(", 1)[1]
message = message.split("EspNativeGameplayDispatchStatus __wrap_", 1)[0]
assert message.index("if (!Esp32PlatformVideo_present())") < message.index(
    "publishCommittedMoveDoorSounds();")
assert message.index('printf("[MOVEEVENT] MESSAGE') < message.index(
    "publishCommittedMoveDoorSounds();")
# Failed frame and rollback branches must not leak audio.
assert "publishCommittedMoveDoorSounds();" not in frame.split(
    "else {", 1)[-1]

code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_door_sfx.h"

static uint16_t ids[16];
static uint8_t priorities[16], flags[16];
static uint8_t n;
int EspNativeAudioIntent_publish(uint16_t id, uint8_t flag, uint8_t priority) {
    assert(n < 16U);
    ids[n] = id;
    flags[n] = flag;
    priorities[n] = priority;
    ++n;
    return 1;
}
int main(void) {
    assert(ESP_MAP_LINE_SOUND_OPEN == 5063U);
    assert(ESP_MAP_LINE_SOUND_CLOSE == 5064U);
    assert(!EspNativeDoorSfx_publishCommitted(7, 0, 0, 5063,
            ESP_MAP_LINE_EFFECT_ALL)); /* no state change */
    assert(!EspNativeDoorSfx_publishCommitted(7, 0, 1, 5064,
            ESP_MAP_LINE_EFFECT_ALL)); /* mismatched resource */
    assert(!EspNativeDoorSfx_publishCommitted(7, 1, 0, 5063,
            ESP_MAP_LINE_EFFECT_ALL)); /* mismatched direction */
    assert(!EspNativeDoorSfx_publishCommitted(7, 0, 1, 5063, 0));
    assert(!EspNativeDoorSfx_publishCommitted(7, 2, 1, 5063,
            ESP_MAP_LINE_EFFECT_ALL)); /* corrupt old state */
    assert(!EspNativeDoorSfx_publishCommitted(7, 1, 2, 5063,
            ESP_MAP_LINE_EFFECT_ALL)); /* corrupt new state */
    assert(n == 0U);
    assert(EspNativeDoorSfx_publishCommitted(7, 0, 1, 5063,
            ESP_MAP_LINE_EFFECT_ALL));
    assert(EspNativeDoorSfx_publishCommitted(7, 1, 0, 5064,
            ESP_MAP_LINE_EFFECT_PLAY_SOUND));
    assert(EspNativeDoorSfx_publishCommitted(13, 0, 1, 5063,
            ESP_MAP_LINE_EFFECT_ALL)); /* script multi-line */
    assert(n == 3U);
    assert(ids[0] == 5063U && ids[1] == 5064U && ids[2] == 5063U);
    for (uint8_t i=0; i<n; ++i)
        assert(priorities[i] == 2U && flags[i] == 0U);
    puts("audio_door_sfx_host: PASS real 5063/5064 direction, priority, ordering, noop/rollback gating");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="doom-audio-door-") as td:
    source = Path(td) / "door_sfx.c"
    binary = Path(td) / "door_sfx"
    source.write_text(code)
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(src), "-I", str(inc),
        str(source), "-o", str(binary)
    ], check=True, cwd=root)
    subprocess.run([str(binary)], check=True, cwd=root)
print("PASS AUDIO-DOOR: legacy IDs, all final commit hooks, bounded native post-commit intents")
