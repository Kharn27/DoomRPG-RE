#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_script_state.h"
#include "esp_map_strings.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_status_message.h"
#include "esp_native_indexed_bmp.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define STATUS_REMOVE_FLAG 0x00000200UL
#define STATUS_TEXT_CAPACITY 384U
#define STATUS_TOP_HEIGHT 20U
#define STATUS_FONT_WIDTH 9U
#define STATUS_FONT_HEIGHT 12U
#define STATUS_FONT_ADVANCE 7
#define STATUS_FONT_SOURCE_WIDTH 144U
#define STATUS_FONT_SOURCE_HEIGHT 72U
#define STATUS_TEXT_X 1
#define STATUS_TEXT_Y 5
#define STATUS_MAX_VISIBLE_CHARS 21U
#define STATUS_TRANSPARENT 1U
#define STATUS_OPAQUE 0U

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native FORCE_MESSAGE top bar requires the 160x120 logical framebuffer"
#endif

typedef struct EspNativeGameplayStatusMessageOwner_s {
    EspMapStatusMessageState state;
    uint8_t ready;
    uint8_t dirty;
    uint8_t corpusLogged;
    uint8_t reserved;
} EspNativeGameplayStatusMessageOwner;

typedef struct StatusPaintScratch_s {
    EspNativeIndexedBmp bar;
    EspNativeIndexedBmp font;
} StatusPaintScratch;

static EspNativeGameplayStatusMessageOwner owner;
static char textScratch[STATUS_TEXT_CAPACITY];

static int sameState(const EspMapStatusMessageState* a,
                     const EspMapStatusMessageState* b) {
    return a != NULL && b != NULL && a->active == b->active &&
           a->text.index == b->text.index &&
           a->text.sourceOffset == b->text.sourceOffset &&
           a->text.length == b->text.length;
}

static int currentMapEntry(EspAssetPackEntry* outEntry) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const char* mapName;
    if (outEntry != NULL) memset(outEntry, 0, sizeof(*outEntry));
    if (outEntry == NULL || view == NULL || view->active != 1U) return 0;
    mapName = EspMapCatalog_nameForId(view->targetMapId);
    return mapName != NULL && EspAssetPack_findEntry(mapName, outEntry);
}

static int drawGlyph(const EspNativeIndexedBmp* font,
                     uint16_t* framebuffer,
                     uint8_t c,
                     int x,
                     int y,
                     EspNativeIndexedBmpStats* stats) {
    uint8_t glyph;
    if (c < 33U || c > 127U) return 0;
    glyph = (uint8_t)(c - 33U);
    return EspNativeIndexedBmp_blit(
               font, framebuffer,
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
               (uint16_t)(STATUS_FONT_WIDTH * (glyph & 0x0fU)),
               (uint16_t)(STATUS_FONT_HEIGHT * (glyph >> 4)),
               STATUS_FONT_WIDTH, STATUS_FONT_HEIGHT,
               (int16_t)x, (int16_t)y,
               STATUS_TRANSPARENT, stats) == ESP_NATIVE_INDEXED_BMP_OK;
}

static int paintTopBar(const char* text,
                       uint16_t textLength,
                       uint16_t stringIndex) {
    StatusPaintScratch scratch;
    EspNativeIndexedBmpStats stats;
    uint16_t* framebuffer;
    size_t framebufferBytes;
    uint16_t visible;
    uint16_t i;
    int x = STATUS_TEXT_X;
    int ok = 0;

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                                DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t) ||
        EspAssetPack_isOpen()) {
        return 0;
    }

    memset(&scratch, 0, sizeof(scratch));
    memset(&stats, 0, sizeof(stats));
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;

    if (EspNativeIndexedBmp_open("k.bmp", &scratch.bar, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        scratch.bar.width != 20U || scratch.bar.height != STATUS_TOP_HEIGHT ||
        EspNativeIndexedBmp_tile(
            &scratch.bar, framebuffer,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            0, 0, DOOMRPG_LOGICAL_WIDTH, STATUS_TOP_HEIGHT,
            STATUS_OPAQUE, &stats) != ESP_NATIVE_INDEXED_BMP_OK) {
        goto done;
    }

    visible = textLength > STATUS_MAX_VISIBLE_CHARS
                  ? STATUS_MAX_VISIBLE_CHARS
                  : textLength;
    if (visible != 0U) {
        if (text == NULL ||
            EspNativeIndexedBmp_open("a.bmp", &scratch.font, &stats) !=
                ESP_NATIVE_INDEXED_BMP_OK ||
            scratch.font.width != STATUS_FONT_SOURCE_WIDTH ||
            scratch.font.height != STATUS_FONT_SOURCE_HEIGHT) {
            goto done;
        }
        for (i = 0U; i < visible; ++i) {
            uint8_t c = (uint8_t)text[i];
            if (c == ' ') {
                x += STATUS_FONT_ADVANCE;
                continue;
            }
            if (!drawGlyph(&scratch.font, framebuffer, c,
                           x, STATUS_TEXT_Y, &stats)) {
                goto done;
            }
            x += STATUS_FONT_ADVANCE;
        }
    }

    printf("[STATUSBAR] PAINT active=%u string=%u bytes=%u visible=%u reads=%u resourceBytes=%u present=deferred-to-frame\n",
           (unsigned int)(visible != 0U ? 1U : 0U),
           (unsigned int)stringIndex,
           (unsigned int)textLength,
           (unsigned int)visible,
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead);
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    return ok;
}

void EspNativeGameplayStatusMessage_reset(void) {
    memset(&owner, 0, sizeof(owner));
    memset(textScratch, 0, sizeof(textScratch));
    EspMapStatusMessage_reset(&owner.state);
    owner.ready = 1U;
}

int EspNativeGameplayStatusMessage_isReady(void) {
    return owner.ready == 1U;
}

const EspMapStatusMessageState* EspNativeGameplayStatusMessage_view(void) {
    return EspNativeGameplayStatusMessage_isReady() ? &owner.state : NULL;
}

EspNativeGameplayStatusMessageApplyStatus EspNativeGameplayStatusMessage_apply(
    const EspMapEventDescriptor* descriptor,
    uint8_t commandOffset,
    EspNativeGameplayStatusMessageResult* outResult) {
    EspMapByteCode command;
    EspMapUiIntent intent;
    EspAssetPackEntry mapEntry;
    EspMapStatusMessageState before;
    EspMapStatusMessageApplyStatus applyStatus;
    size_t readLength = 0U;
    uint32_t global;
    uint8_t removedBefore;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (descriptor == NULL || outResult == NULL ||
        commandOffset >= descriptor->commandCount) {
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID;
    }
    if (!owner.ready || !EspMapRuntime_isLoaded() ||
        !EspMapScriptState_isReady() || EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_NOT_READY;
    }
    if (!EspMapEvents_getCommand(descriptor, commandOffset, &command) ||
        command.id != ESP_MAP_OPCODE_FORCE_MESSAGE) {
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_UNSUPPORTED;
    }

    global = (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (global > UINT16_MAX ||
        !EspMapScriptState_isCommandRemoved(global, &removedBefore) ||
        removedBefore != 0U ||
        EspMapUiIntent_build(descriptor, commandOffset, &intent) !=
            ESP_MAP_UI_INTENT_OK ||
        intent.kind != ESP_MAP_UI_INTENT_FORCE_MESSAGE ||
        intent.codeId != ESP_MAP_OPCODE_FORCE_MESSAGE) {
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID;
    }

    before = owner.state;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_IO_FAILED;
    }
    if (!currentMapEntry(&mapEntry)) {
        EspAssetPack_close();
        return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_IO_FAILED;
    }
    memset(textScratch, 0, sizeof(textScratch));
    applyStatus = EspMapStatusMessage_apply(
        &owner.state, &mapEntry, &intent,
        textScratch, sizeof(textScratch), &readLength);
    EspAssetPack_close();
    if (applyStatus != ESP_MAP_STATUS_MESSAGE_APPLY_OK) {
        owner.state = before;
        return applyStatus == ESP_MAP_STATUS_MESSAGE_APPLY_IO_ERROR
                   ? ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_IO_FAILED
                   : ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID;
    }

    memset(outResult, 0, sizeof(*outResult));
    outResult->before = before;
    outResult->after = owner.state;
    outResult->eventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)global;
    outResult->commandOffset = commandOffset;
    outResult->codeId = command.id;
    outResult->removedBefore = removedBefore;
    outResult->removedAfter = removedBefore;
    outResult->ownerChanged = sameState(&before, &owner.state) ? 0U : 1U;
    outResult->removeIfHandled =
        (uint8_t)((command.arg2 & STATUS_REMOVE_FLAG) != 0U ? 1U : 0U);

    if (outResult->removeIfHandled != 0U) {
        if (!EspMapScriptState_setCommandRemoved(global, 1U)) {
            owner.state = before;
            return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID;
        }
        outResult->removedAfter = 1U;
    }

    if (outResult->ownerChanged != 0U) owner.dirty = 1U;
    outResult->rollbackAvailable =
        (uint8_t)((outResult->ownerChanged != 0U ||
                   outResult->removedBefore != outResult->removedAfter)
                      ? 1U
                      : 0U);
    return ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_OK;
}

int EspNativeGameplayStatusMessage_rollback(
    const EspNativeGameplayStatusMessageResult* result) {
    uint8_t removedNow;
    if (result == NULL || result->codeId != ESP_MAP_OPCODE_FORCE_MESSAGE ||
        result->rollbackAvailable != 1U || !owner.ready ||
        !sameState(&owner.state, &result->after) ||
        !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                             &removedNow) ||
        removedNow != result->removedAfter) {
        return 0;
    }

    owner.state = result->before;
    if (result->removedBefore != result->removedAfter &&
        !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                              result->removedBefore)) {
        owner.state = result->after;
        return 0;
    }
    if (result->ownerChanged != 0U) owner.dirty = 1U;
    return 1;
}

int EspNativeGameplayStatusMessage_paintIfDirty(void) {
    EspAssetPackEntry mapEntry;
    EspMapStringRef ref;
    size_t readLength = 0U;
    uint16_t stringIndex = 0U;
    uint16_t textLength = 0U;
    int ok;

    if (!owner.ready) return 0;
    if (!owner.dirty) return 1;
    memset(textScratch, 0, sizeof(textScratch));

    if (EspMapStatusMessage_isActive(&owner.state)) {
        if (!EspMapStatusMessage_getRef(&owner.state, &ref) ||
            ref.length + 1U > sizeof(textScratch) || EspAssetPack_isOpen() ||
            !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
            return 0;
        }
        if (!currentMapEntry(&mapEntry) ||
            EspMapStrings_read(&mapEntry, &ref,
                               textScratch, sizeof(textScratch),
                               &readLength) != ESP_MAP_STRING_READ_OK ||
            readLength != ref.length) {
            EspAssetPack_close();
            return 0;
        }
        EspAssetPack_close();
        stringIndex = ref.index;
        textLength = (uint16_t)readLength;
    }

    ok = paintTopBar(textScratch, textLength, stringIndex);
    if (!ok) return 0;
    owner.dirty = 0U;
    return 1;
}

void EspNativeGameplayStatusMessage_logCorpus(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t eventIndex;
    uint32_t refs = 0U;
    if (!owner.ready || owner.corpusLogged || runtime == NULL) return;

    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t value;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        uint32_t offset;
        if (!EspMapRuntime_getEvent(eventIndex, &value)) return;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
        ref.value = value;
        if (!EspMapEvents_describe(&ref, &descriptor)) return;
        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            EspMapUiIntent intent;
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return;
            if (command.id != ESP_MAP_OPCODE_FORCE_MESSAGE) continue;
            if (EspMapUiIntent_build(&descriptor, offset, &intent) !=
                ESP_MAP_UI_INTENT_OK) return;
            ++refs;
            printf("[STATUSCORPUS] n=%u event=%u tile=%u off=%u global=%u arg2=%08x string=%u@%u+%u\n",
                   (unsigned int)refs,
                   (unsigned int)descriptor.eventIndex,
                   (unsigned int)descriptor.tileIndex,
                   (unsigned int)offset,
                   (unsigned int)intent.globalCommandIndex,
                   (unsigned int)command.arg2,
                   (unsigned int)intent.text.index,
                   (unsigned int)intent.text.sourceOffset,
                   (unsigned int)intent.text.length);
        }
    }
    owner.corpusLogged = 1U;
    printf("[STATUSCORPUS] READY refs=%u ownerBytes=%u persistentTextBytes=0 generic=yes\n",
           (unsigned int)refs,
           (unsigned int)sizeof(EspMapStatusMessageState));
}

const char* EspNativeGameplayStatusMessage_applyStatusName(
    EspNativeGameplayStatusMessageApplyStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_UNSUPPORTED: return "UNSUPPORTED";
    case ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_IO_FAILED: return "IO_FAILED";
    case ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_OK: return "OK";
    default: return "UNKNOWN";
    }
}
