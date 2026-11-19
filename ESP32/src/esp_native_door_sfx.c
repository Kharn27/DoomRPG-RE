#include <stdio.h>
#include <string.h>

#include "esp_map_line_state.h"
#include "esp_native_audio_intent.h"
#include "esp_native_door_sfx.h"
#include "esp_native_gameplay_action.h"

/*
 * AUDIO-DOOR timing: stage a validated SELECT open batch only while its
 * canonical line transitions are already applied and secret/key preflight
 * succeeded. Never enqueue here; the animation renderer alone knows when
 * a real opening frame has appeared. Small fixed BSS, no heap/SD ownership.
 */
typedef struct DoorOpeningCue_s {
    uint16_t lineIndex;
    uint8_t published;
    uint8_t reserved;
} DoorOpeningCue;

static struct {
    DoorOpeningCue cues[ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS];
    uint8_t count;
    uint8_t firstFrameSeen;
    uint8_t armed;
    uint8_t reserved;
} openingLease;

void EspNativeDoorSfx_cancelSelect(void) {
    memset(&openingLease, 0, sizeof(openingLease));
}

void EspNativeDoorSfx_stageSelectOpenings(
    const EspNativeGameplayActionResult* result) {
    uint8_t i;
    EspNativeDoorSfx_cancelSelect();
    if (result == NULL || result->handled != 1U ||
        result->mutated != 1U || result->rollbackAvailable != 1U ||
        result->doorCount == 0U ||
        result->doorCount > ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS) {
        return;
    }

    for (i = 0U; i < result->doorCount; ++i) {
        const EspNativeGameplayActionDoorStep* step = &result->doors[i];
        if (step->openBefore != 0U || step->openAfter != 1U ||
            step->soundId != ESP_MAP_LINE_SOUND_OPEN ||
            (step->effectFlags & ESP_MAP_LINE_EFFECT_PLAY_SOUND) == 0U) {
            continue;
        }
        openingLease.cues[openingLease.count].lineIndex = step->lineIndex;
        ++openingLease.count;
    }
    openingLease.armed = openingLease.count != 0U ? 1U : 0U;
}

uint8_t EspNativeDoorSfx_onFirstPresentedOpeningFrame(void) {
    uint8_t i;
    uint8_t queued = 0U;

    if (openingLease.armed == 0U || openingLease.firstFrameSeen != 0U) {
        return 0U;
    }
    openingLease.firstFrameSeen = 1U;

    for (i = 0U; i < openingLease.count; ++i) {
        DoorOpeningCue* cue = &openingLease.cues[i];
        uint8_t openNow = 0U;
        if (!EspMapLineState_getOpen(cue->lineIndex, &openNow) ||
            openNow != 1U) {
            continue;
        }
        if (EspNativeAudioIntent_publish(ESP_MAP_LINE_SOUND_OPEN, 0U, 2U)) {
            cue->published = 1U;
            ++queued;
            printf("[DOORSFX] OPEN-START line=%u open=0->1 sound=5063 priority=2 queued=yes phase=first-visible-frame rollback=visual-confirmed\n",
                   (unsigned int)cue->lineIndex);
        }
    }
    return queued;
}

void EspNativeDoorSfx_finishSelect(
    const EspNativeGameplayActionResult* result) {
    uint8_t i;
    if (result == NULL || result->doorCount == 0U ||
        result->doorCount > ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS ||
        result->handled != 1U || result->mutated != 1U) {
        EspNativeDoorSfx_cancelSelect();
        return;
    }

    for (i = 0U; i < result->doorCount; ++i) {
        const EspNativeGameplayActionDoorStep* step = &result->doors[i];
        uint8_t early = 0U;
        uint8_t j;
        if (step->openBefore == 0U && step->openAfter == 1U) {
            for (j = 0U; j < openingLease.count; ++j) {
                if (openingLease.cues[j].lineIndex == step->lineIndex &&
                    openingLease.cues[j].published != 0U) {
                    early = 1U;
                    break;
                }
            }
        }
        if (early == 0U) {
            (void)EspNativeDoorSfx_publishCommitted(
                step->lineIndex, step->openBefore, step->openAfter,
                step->soundId, step->effectFlags);
        }
    }
    EspNativeDoorSfx_cancelSelect();
}
