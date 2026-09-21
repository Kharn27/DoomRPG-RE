#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_line_checkpoint.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"

static uint8_t bitGet(const uint8_t* bits, uint32_t index) {
    return (uint8_t)((bits[index >> 3] >> (index & 7U)) & 1U);
}

static uint32_t countBits(const uint8_t* bits, uint32_t bytes) {
    uint32_t count = 0U;
    uint32_t i;
    uint8_t value;

    if (bits == NULL) return 0U;
    for (i = 0U; i < bytes; ++i) {
        value = bits[i];
        while (value != 0U) {
            count += (uint32_t)(value & 1U);
            value >>= 1U;
        }
    }
    return count;
}

static int bitsetShapeValid(const uint8_t* bits,
                            uint32_t lineCount,
                            uint32_t bitsetBytes) {
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t i;

    if (bits == NULL || bitsetBytes == 0U ||
        bitsetBytes > ESP_MAP_LINE_CHECKPOINT_MAX_BYTES) {
        return 0;
    }

    validTailBits = lineCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((bits[bitsetBytes - 1U] & (uint8_t)~validTailMask) != 0U) {
            return 0;
        }
    }

    for (i = bitsetBytes; i < ESP_MAP_LINE_CHECKPOINT_MAX_BYTES; ++i) {
        if (bits[i] != 0U) return 0;
    }
    return 1;
}

int EspMapLineCheckpoint_shapeValid(
    const EspMapLineCheckpointSnapshot* snapshot,
    uint32_t expectedArenaFNV1a) {
    uint32_t expectedBytes;

    if (snapshot == NULL || expectedArenaFNV1a == 0U ||
        snapshot->reserved0 != 0U ||
        snapshot->sourceArenaFNV1a != expectedArenaFNV1a ||
        snapshot->lineCount == 0U ||
        snapshot->lineCount > ESP_MAP_LINE_CHECKPOINT_MAX_LINES) {
        return 0;
    }

    expectedBytes = (snapshot->lineCount + 7U) >> 3;
    if (expectedBytes == 0U ||
        expectedBytes > ESP_MAP_LINE_CHECKPOINT_MAX_BYTES ||
        snapshot->bitsetBytes != expectedBytes) {
        return 0;
    }

    return bitsetShapeValid(snapshot->openBits,
                            snapshot->lineCount,
                            expectedBytes) &&
           bitsetShapeValid(snapshot->lockedBits,
                            snapshot->lineCount,
                            expectedBytes) &&
           bitsetShapeValid(snapshot->texture10Bits,
                            snapshot->lineCount,
                            expectedBytes);
}

int EspMapLineCheckpoint_snapshot(EspMapLineCheckpointSnapshot* outSnapshot) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapLineStateView* lineState = EspMapLineState_view();
    const EspMapLineTextureStateView* textureState =
        EspMapLineTextureState_view();
    uint32_t bitsetBytes;

    if (outSnapshot == NULL || runtime == NULL || runtime->arena == NULL ||
        lineState == NULL || textureState == NULL ||
        runtime->arenaFNV1a == 0U || runtime->lineCount == 0U ||
        runtime->lineCount > ESP_MAP_LINE_CHECKPOINT_MAX_LINES ||
        lineState->lineCount != runtime->lineCount ||
        textureState->lineCount != runtime->lineCount ||
        lineState->bitsetBytes != textureState->bitsetBytes) {
        return 0;
    }

    bitsetBytes = (runtime->lineCount + 7U) >> 3;
    if (bitsetBytes == 0U ||
        bitsetBytes > ESP_MAP_LINE_CHECKPOINT_MAX_BYTES ||
        lineState->bitsetBytes != bitsetBytes ||
        lineState->openBits == NULL || lineState->lockedBits == NULL ||
        textureState->texture10Bits == NULL) {
        return 0;
    }

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->sourceArenaFNV1a = runtime->arenaFNV1a;
    outSnapshot->lineStateFNV1a = lineState->stateFNV1a;
    outSnapshot->textureStateFNV1a = textureState->stateFNV1a;
    outSnapshot->lineCount = runtime->lineCount;
    outSnapshot->bitsetBytes = (uint16_t)bitsetBytes;
    memcpy(outSnapshot->openBits, lineState->openBits, bitsetBytes);
    memcpy(outSnapshot->lockedBits, lineState->lockedBits, bitsetBytes);
    memcpy(outSnapshot->texture10Bits,
           textureState->texture10Bits,
           bitsetBytes);

    return EspMapLineCheckpoint_shapeValid(outSnapshot,
                                           runtime->arenaFNV1a);
}

int EspMapLineCheckpoint_restore(
    const EspMapLineCheckpointSnapshot* snapshot) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    EspMapLine line;
    uint32_t i;
    uint32_t openCount;
    uint32_t lockedCount;
    uint32_t texture10Count;

    if (runtime == NULL || runtime->arena == NULL || snapshot == NULL ||
        runtime->lineCount != snapshot->lineCount ||
        !EspMapLineCheckpoint_shapeValid(snapshot, runtime->arenaFNV1a)) {
        return 0;
    }

    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    if (lineState == NULL || textureState == NULL ||
        lineState->lineCount != snapshot->lineCount ||
        textureState->lineCount != snapshot->lineCount ||
        lineState->bitsetBytes != snapshot->bitsetBytes ||
        textureState->bitsetBytes != snapshot->bitsetBytes) {
        return 0;
    }

    /* Validate all immutable texture eligibility before mutating either owner. */
    for (i = 0U; i < snapshot->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) return 0;
        if (bitGet(snapshot->texture10Bits, i) != 0U &&
            line.texture != ESP_MAP_LINE_TEXTURE_LOCKED &&
            line.texture != ESP_MAP_LINE_TEXTURE_UNLOCKED) {
            return 0;
        }
    }

    for (i = 0U; i < snapshot->lineCount; ++i) {
        if (!EspMapLineState_setOpen(i, bitGet(snapshot->openBits, i)) ||
            !EspMapLineState_setLocked(i, bitGet(snapshot->lockedBits, i)) ||
            !EspMapRuntime_getLine(i, &line)) {
            return 0;
        }

        if (line.texture == ESP_MAP_LINE_TEXTURE_LOCKED ||
            line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) {
            const uint16_t targetTexture =
                bitGet(snapshot->texture10Bits, i) != 0U
                    ? ESP_MAP_LINE_TEXTURE_UNLOCKED
                    : ESP_MAP_LINE_TEXTURE_LOCKED;
            if (!EspMapLineTextureState_setDoorTexture(i, targetTexture)) {
                return 0;
            }
        }
    }

    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    if (lineState == NULL || textureState == NULL ||
        lineState->stateFNV1a != snapshot->lineStateFNV1a ||
        textureState->stateFNV1a != snapshot->textureStateFNV1a) {
        return 0;
    }

    openCount = countBits(snapshot->openBits, snapshot->bitsetBytes);
    lockedCount = countBits(snapshot->lockedBits, snapshot->bitsetBytes);
    texture10Count = countBits(snapshot->texture10Bits,
                               snapshot->bitsetBytes);
    printf("[MAPLINECHECKPOINT] RESTORE arena=%08x lines=%u bytes=%u open=%u locked=%u texture10=%u lineFNV=%08x textureFNV=%08x mutation=line-overlays-only allocation=no\n",
           (unsigned int)snapshot->sourceArenaFNV1a,
           (unsigned int)snapshot->lineCount,
           (unsigned int)snapshot->bitsetBytes,
           (unsigned int)openCount,
           (unsigned int)lockedCount,
           (unsigned int)texture10Count,
           (unsigned int)snapshot->lineStateFNV1a,
           (unsigned int)snapshot->textureStateFNV1a);
    return 1;
}
