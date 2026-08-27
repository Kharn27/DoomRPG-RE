#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_dialog_owner.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_strings.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_indexed_bmp.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define DIALOG_REMOVE_FLAG 0x00000200UL
#define DIALOG_FONT_NAME "a.bmp"
#define DIALOG_FONT_WIDTH 9U
#define DIALOG_FONT_HEIGHT 12U
#define DIALOG_FONT_ADVANCE 7
#define DIALOG_FONT_SOURCE_WIDTH 144U
#define DIALOG_FONT_SOURCE_HEIGHT 72U
#define DIALOG_TRANSPARENT 1U

/* Native 160x120 adaptation of the legacy dialog displayRect: the box owns the
 * lower 54 rows of the 0..99 gameplay/display region and deliberately leaves
 * the permanent bottom HUD band (100..119) untouched. */
#define DIALOG_FILL_X 16
#define DIALOG_FILL_Y 46
#define DIALOG_FILL_W 128
#define DIALOG_FILL_H 54
#define DIALOG_BORDER_LEFT 15
#define DIALOG_BORDER_RIGHT 144
#define DIALOG_BORDER_TOP 45
#define DIALOG_BORDER_BOTTOM 99
#define DIALOG_TEXT_X 16
#define DIALOG_TEXT_Y 48

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native dialog presenter is defined for the 160x120 logical framebuffer"
#endif

typedef struct EspNativeGameplayDialogState_s {
    EspMapDialogOwnerState owner;
    EspNativeIndexedBmp font;
    char text[ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY];
    uint32_t runFlags;
    uint32_t lineStartMs;
    uint32_t lastPaintSignature;
    uint32_t paintCount;
    uint32_t fontPackReads;
    uint32_t fontBytesRead;
    uint16_t textLength;
    uint16_t lineCount;
    uint16_t currentDialogLine;
    uint16_t resumeGlobalCommandIndex;
    uint8_t dialogCodeId;
    uint8_t resumeCommandOffset;
    uint8_t resumeCodeId;
    uint8_t resumeHasCommand;
    uint8_t resumeRemovedBefore;
    uint8_t dialogTypeLineIdx;
    uint8_t backAllowed;
    uint8_t packOwned;
    uint8_t active;
    uint8_t reserved[3];
} EspNativeGameplayDialogState;

static EspNativeGameplayDialogState dialog;

static uint32_t nowMs(void) {
    int64_t micros = esp_timer_get_time();
    if (micros <= 0) return 0U;
    return (uint32_t)((uint64_t)micros / 1000ULL);
}

static uint32_t fnv1a32(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void fillRect(uint16_t* framebuffer,
                     int x,
                     int y,
                     int width,
                     int height,
                     uint16_t color) {
    int yy;
    int xx;
    if (framebuffer == NULL || width <= 0 || height <= 0) return;
    for (yy = 0; yy < height; ++yy) {
        for (xx = 0; xx < width; ++xx) {
            putPixel(framebuffer, x + xx, y + yy, color);
        }
    }
}

static void drawBorder(uint16_t* framebuffer, uint16_t color) {
    int x;
    int y;
    for (x = DIALOG_BORDER_LEFT; x <= DIALOG_BORDER_RIGHT; ++x) {
        putPixel(framebuffer, x, DIALOG_BORDER_TOP, color);
        putPixel(framebuffer, x, DIALOG_BORDER_BOTTOM, color);
    }
    for (y = DIALOG_BORDER_TOP; y <= DIALOG_BORDER_BOTTOM; ++y) {
        putPixel(framebuffer, DIALOG_BORDER_LEFT, y, color);
        putPixel(framebuffer, DIALOG_BORDER_RIGHT, y, color);
    }
}

static int lineSpan(uint16_t lineIndex,
                    uint16_t* outStart,
                    uint16_t* outLength) {
    uint16_t current = 0U;
    uint16_t start = 0U;
    uint16_t i;

    if (outStart != NULL) *outStart = 0U;
    if (outLength != NULL) *outLength = 0U;
    if (outStart == NULL || outLength == NULL ||
        lineIndex >= dialog.lineCount) {
        return 0;
    }

    for (i = 0U; i <= dialog.textLength; ++i) {
        if (i == dialog.textLength || dialog.text[i] == '|') {
            if (current == lineIndex) {
                *outStart = start;
                *outLength = (uint16_t)(i - start);
                return 1;
            }
            ++current;
            start = (uint16_t)(i + 1U);
        }
    }
    return 0;
}

static uint16_t pageLineCount(void) {
    uint16_t remaining;
    if (dialog.currentDialogLine >= dialog.lineCount) return 0U;
    remaining = (uint16_t)(dialog.lineCount - dialog.currentDialogLine);
    return remaining > ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES
               ? ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES
               : remaining;
}

static uint16_t typedCharsForCurrentLine(uint32_t now) {
    uint16_t start;
    uint16_t length;
    uint32_t elapsed;
    uint32_t chars;
    uint16_t pageLines = pageLineCount();

    if (dialog.dialogTypeLineIdx >= pageLines) return 0U;
    if (!lineSpan((uint16_t)(dialog.currentDialogLine +
                             dialog.dialogTypeLineIdx),
                  &start, &length)) {
        return 0U;
    }
    (void)start;
    elapsed = now >= dialog.lineStartMs ? now - dialog.lineStartMs : 0U;
    chars = elapsed / ESP_NATIVE_GAMEPLAY_DIALOG_TYPE_MS;
    if (chars > length) chars = length;
    return (uint16_t)chars;
}

static uint32_t paintSignature(uint32_t now) {
    uint16_t typed = typedCharsForCurrentLine(now);
    return ((uint32_t)dialog.currentDialogLine << 16) |
           ((uint32_t)dialog.dialogTypeLineIdx << 8) |
           (uint32_t)(typed & 0xffU);
}

static int drawFontSlice(uint16_t* framebuffer,
                         uint16_t start,
                         uint16_t count,
                         int x,
                         int y,
                         EspNativeIndexedBmpStats* stats) {
    uint16_t i;
    for (i = 0U; i < count; ++i) {
        uint8_t c = (uint8_t)dialog.text[start + i];
        if (c == ' ') {
            x += DIALOG_FONT_ADVANCE;
            continue;
        }
        if (c < 33U || c > 127U) return 0;
        {
            uint8_t glyph = (uint8_t)(c - 33U);
            uint16_t sourceX =
                (uint16_t)(DIALOG_FONT_WIDTH * (glyph & 0x0fU));
            uint16_t sourceY =
                (uint16_t)(DIALOG_FONT_HEIGHT * (glyph >> 4));
            EspNativeIndexedBmpStatus status = EspNativeIndexedBmp_blit(
                &dialog.font,
                framebuffer,
                DOOMRPG_LOGICAL_WIDTH,
                DOOMRPG_LOGICAL_HEIGHT,
                sourceX,
                sourceY,
                DIALOG_FONT_WIDTH,
                DIALOG_FONT_HEIGHT,
                (int16_t)x,
                (int16_t)y,
                DIALOG_TRANSPARENT,
                stats);
            if (status != ESP_NATIVE_INDEXED_BMP_OK) return 0;
        }
        x += DIALOG_FONT_ADVANCE;
    }
    return 1;
}

static int paintDialog(uint32_t now) {
    uint16_t* framebuffer =
        (uint16_t*)Esp32PlatformVideo_framebuffer();
    EspNativeIndexedBmpStats stats;
    uint16_t rows;
    uint16_t row;

    if (!dialog.active || !dialog.packOwned || !EspAssetPack_isOpen() ||
        framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) {
        return 0;
    }

    memset(&stats, 0, sizeof(stats));
    fillRect(framebuffer, DIALOG_FILL_X, DIALOG_FILL_Y,
             DIALOG_FILL_W, DIALOG_FILL_H, 0x0000U);
    drawBorder(framebuffer, 0xffffU);

    rows = pageLineCount();
    for (row = 0U; row < rows; ++row) {
        uint16_t start;
        uint16_t length;
        uint16_t visible = 0U;
        if (!lineSpan((uint16_t)(dialog.currentDialogLine + row),
                      &start, &length)) {
            return 0;
        }
        if (row < dialog.dialogTypeLineIdx) {
            visible = length;
        }
        else if (row == dialog.dialogTypeLineIdx) {
            visible = typedCharsForCurrentLine(now);
        }
        if (visible != 0U &&
            !drawFontSlice(framebuffer, start, visible,
                           DIALOG_TEXT_X,
                           DIALOG_TEXT_Y + (int)row * DIALOG_FONT_HEIGHT,
                           &stats)) {
            return 0;
        }
    }

    dialog.fontPackReads += stats.packReads;
    dialog.fontBytesRead += stats.bytesRead;
    if (!Esp32PlatformVideo_present()) return 0;
    ++dialog.paintCount;
    dialog.lastPaintSignature = paintSignature(now);
    return 1;
}

static int advanceTypewriter(uint32_t now) {
    uint16_t rows = pageLineCount();
    uint16_t start;
    uint16_t length;
    uint32_t elapsed;

    if (dialog.dialogTypeLineIdx >= rows) return 1;
    if (!lineSpan((uint16_t)(dialog.currentDialogLine +
                             dialog.dialogTypeLineIdx),
                  &start, &length)) {
        return 0;
    }
    (void)start;
    elapsed = now >= dialog.lineStartMs ? now - dialog.lineStartMs : 0U;
    if (elapsed / ESP_NATIVE_GAMEPLAY_DIALOG_TYPE_MS >= length) {
        ++dialog.dialogTypeLineIdx;
        dialog.lineStartMs = now;
    }
    return 1;
}

static int eventDescriptorForIndex(uint16_t eventIndex,
                                   EspMapEventDescriptor* outDescriptor) {
    uint32_t eventValue;
    EspMapEventRef ref;
    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (outDescriptor == NULL ||
        !EspMapRuntime_getEvent(eventIndex, &eventValue)) {
        return 0;
    }
    ref.index = eventIndex;
    ref.tileIndex = (uint16_t)(eventValue & ESP_MAP_EVENT_TILE_MASK);
    ref.value = eventValue;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int preflightResume(const EspMapEventDescriptor* descriptor,
                           uint8_t resumeOffset,
                           uint32_t runFlags,
                           uint16_t* outGlobal,
                           uint8_t* outCodeId,
                           uint8_t* outRemoved,
                           uint8_t* outHasCommand) {
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    uint8_t currentState;
    uint8_t found = 0U;
    uint32_t offset;

    if (outGlobal != NULL) *outGlobal = UINT16_MAX;
    if (outCodeId != NULL) *outCodeId = 0U;
    if (outRemoved != NULL) *outRemoved = 0U;
    if (outHasCommand != NULL) *outHasCommand = 0U;
    if (descriptor == NULL || outGlobal == NULL || outCodeId == NULL ||
        outRemoved == NULL || outHasCommand == NULL ||
        resumeOffset > descriptor->commandCount ||
        !EspMapScriptState_getEventState(descriptor->eventIndex,
                                         &currentState) ||
        !EspMapEventFilter_prepare(descriptor,
                                   currentState,
                                   resumeOffset,
                                   runFlags,
                                   0U,
                                   &plan)) {
        return 0;
    }

    for (offset = resumeOffset; offset < descriptor->commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor->firstCommandIndex + offset;
        uint8_t removed;
        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(descriptor, &plan, offset,
                                        removed, &filtered)) {
            return 0;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (found != 0U || !EspMapOpcodeExecutor_supports(filtered.codeId)) {
            return 0;
        }
        found = 1U;
        *outGlobal = filtered.globalCommandIndex;
        *outCodeId = filtered.codeId;
        *outRemoved = removed;
    }

    *outHasCommand = found;
    return 1;
}

void EspNativeGameplayDialog_reset(void) {
    if (dialog.packOwned != 0U && EspAssetPack_isOpen()) {
        EspAssetPack_close();
    }
    memset(&dialog, 0, sizeof(dialog));
    dialog.lastPaintSignature = UINT32_MAX;
}

int EspNativeGameplayDialog_isActive(void) {
    return dialog.active != 0U;
}

EspNativeGameplayDialogBeginStatus EspNativeGameplayDialog_begin(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    EspMapEventDescriptor descriptor;
    EspMapUiIntent intent;
    EspMapStringRef ref;
    EspAssetPackEntry mapEntry;
    EspNativeIndexedBmpStats fontStats;
    const EspPlayerViewState* view = EspPlayerView_view();
    const char* mapName;
    size_t textLength = 0U;
    uint16_t i;

    if (dialog.active || EspAssetPack_isOpen() || view == NULL ||
        view->active != 1U || !EspMapRuntime_isLoaded() ||
        !EspMapScriptState_isReady() ||
        !eventDescriptorForIndex(eventIndex, &descriptor) ||
        commandOffset >= descriptor.commandCount) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY;
    }

    memset(&intent, 0, sizeof(intent));
    if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
            ESP_MAP_UI_INTENT_OK ||
        (intent.codeId != ESP_MAP_OPCODE_DIALOG &&
         intent.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK)) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }

    EspNativeGameplayDialog_reset();
    if (EspMapDialogOwner_apply(&dialog.owner, &intent) !=
        ESP_MAP_DIALOG_OWNER_APPLY_OK) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }

    if (!preflightResume(&descriptor,
                         dialog.owner.resumeCommandOffset,
                         runFlags,
                         &dialog.resumeGlobalCommandIndex,
                         &dialog.resumeCodeId,
                         &dialog.resumeRemovedBefore,
                         &dialog.resumeHasCommand)) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME;
    }

    if (!EspMapDialogOwner_getRef(&dialog.owner, &ref) ||
        ref.length + 1U > ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE;
    }

    mapName = EspMapCatalog_nameForId(view->targetMapId);
    if (mapName == NULL || !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }
    dialog.packOwned = 1U;
    memset(&mapEntry, 0, sizeof(mapEntry));
    if (!EspAssetPack_findEntry(mapName, &mapEntry) ||
        EspMapStrings_read(&mapEntry, &ref,
                           dialog.text, sizeof(dialog.text),
                           &textLength) != ESP_MAP_STRING_READ_OK ||
        textLength != ref.length) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }

    memset(&fontStats, 0, sizeof(fontStats));
    if (EspNativeIndexedBmp_open(DIALOG_FONT_NAME,
                                 &dialog.font,
                                 &fontStats) != ESP_NATIVE_INDEXED_BMP_OK ||
        dialog.font.width != DIALOG_FONT_SOURCE_WIDTH ||
        dialog.font.height != DIALOG_FONT_SOURCE_HEIGHT) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }
    dialog.fontPackReads = fontStats.packReads;
    dialog.fontBytesRead = fontStats.bytesRead;

    dialog.textLength = (uint16_t)textLength;
    dialog.lineCount = 1U;
    for (i = 0U; i < dialog.textLength; ++i) {
        if (dialog.text[i] == '|') {
            if (dialog.lineCount == UINT16_MAX) {
                EspNativeGameplayDialog_reset();
                return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
            }
            ++dialog.lineCount;
        }
    }

    dialog.runFlags = runFlags;
    dialog.dialogCodeId = intent.codeId;
    dialog.resumeCommandOffset = dialog.owner.resumeCommandOffset;
    dialog.backAllowed =
        (uint8_t)((dialog.owner.flags & ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK) != 0U);
    dialog.currentDialogLine = 0U;
    dialog.dialogTypeLineIdx = 0U;
    dialog.lineStartMs = nowMs();
    dialog.lastPaintSignature = UINT32_MAX;
    dialog.active = 1U;

    if (!paintDialog(dialog.lineStartMs)) {
        EspNativeGameplayDialog_reset();
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }

    printf("[DIALOG] OPEN event=%u cmd=%u resume=%u opcode=%u string=%u bytes=%u lines=%u back=%u runFlags=%08x ownerBytes=%u textCap=%u frame=%08x pack=open\n",
           (unsigned int)dialog.owner.sourceEventIndex,
           (unsigned int)dialog.owner.sourceCommandOffset,
           (unsigned int)dialog.owner.resumeCommandOffset,
           (unsigned int)dialog.dialogCodeId,
           (unsigned int)dialog.owner.text.index,
           (unsigned int)dialog.textLength,
           (unsigned int)dialog.lineCount,
           (unsigned int)dialog.backAllowed,
           (unsigned int)dialog.runFlags,
           (unsigned int)sizeof(EspMapDialogOwnerState),
           (unsigned int)ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY,
           (unsigned int)frameFNV());
    return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
}

int EspNativeGameplayDialog_tick(void) {
    uint32_t now;
    uint32_t signature;
    if (!dialog.active) return 1;
    if (!dialog.packOwned || !EspAssetPack_isOpen()) return 0;

    now = nowMs();
    if (!advanceTypewriter(now)) return 0;
    signature = paintSignature(now);
    if (signature == dialog.lastPaintSignature) return 1;
    return paintDialog(now);
}

static void fillClose(EspNativeGameplayDialogClose* outClose,
                      uint8_t resumeRequested) {
    if (outClose == NULL) return;
    memset(outClose, 0, sizeof(*outClose));
    outClose->runFlags = dialog.runFlags;
    outClose->sourceEventIndex = dialog.owner.sourceEventIndex;
    outClose->resumeGlobalCommandIndex = dialog.resumeGlobalCommandIndex;
    outClose->sourceCommandOffset = dialog.owner.sourceCommandOffset;
    outClose->resumeCommandOffset = dialog.resumeCommandOffset;
    outClose->dialogCodeId = dialog.dialogCodeId;
    outClose->resumeCodeId = dialog.resumeCodeId;
    outClose->resumeHasCommand = dialog.resumeHasCommand;
    outClose->resumeRequested = resumeRequested;
    outClose->backAllowed = dialog.backAllowed;
    outClose->removedBefore = dialog.resumeRemovedBefore;
}

static void closeActive(EspNativeGameplayDialogClose* outClose,
                        uint8_t resumeRequested,
                        const char* reason) {
    uint32_t frame = frameFNV();
    uint32_t paints = dialog.paintCount;
    uint32_t reads = dialog.fontPackReads;
    uint32_t bytes = dialog.fontBytesRead;
    uint16_t eventIndex = dialog.owner.sourceEventIndex;
    uint8_t resumeOffset = dialog.resumeCommandOffset;

    fillClose(outClose, resumeRequested);
    if (dialog.packOwned != 0U && EspAssetPack_isOpen()) {
        EspAssetPack_close();
    }
    memset(&dialog, 0, sizeof(dialog));
    dialog.lastPaintSignature = UINT32_MAX;
    printf("[DIALOG] CLOSE event=%u resume=%u mode=%s paints=%u fontReads=%u bytes=%u frame=%08x packClosed=yes\n",
           (unsigned int)eventIndex,
           (unsigned int)resumeOffset,
           reason != NULL ? reason : (resumeRequested ? "resume" : "cancel"),
           (unsigned int)paints,
           (unsigned int)reads,
           (unsigned int)bytes,
           (unsigned int)frame);
}

EspNativeGameplayDialogInputStatus EspNativeGameplayDialog_handleAction(
    uint8_t action,
    EspNativeGameplayDialogClose* outClose) {
    uint32_t now;
    uint16_t rows;
    uint16_t maxStart;

    if (outClose != NULL) memset(outClose, 0, sizeof(*outClose));
    if (!dialog.active || outClose == NULL) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID;
    }

    now = nowMs();
    rows = pageLineCount();

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        action == ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN) {
        if (dialog.dialogTypeLineIdx < rows) {
            dialog.dialogTypeLineIdx = ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES;
            dialog.lineStartMs = now;
            dialog.lastPaintSignature = UINT32_MAX;
            if (!paintDialog(now)) return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID;
            printf("[DIALOG] FASTFORWARD pageStart=%u lines=%u frame=%08x\n",
                   (unsigned int)dialog.currentDialogLine,
                   (unsigned int)rows,
                   (unsigned int)frameFNV());
            return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN;
        }
        if (dialog.currentDialogLine + ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES <
            dialog.lineCount) {
            dialog.currentDialogLine = (uint16_t)(
                dialog.currentDialogLine + ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES);
            dialog.dialogTypeLineIdx = 0U;
            dialog.lineStartMs = now;
            dialog.lastPaintSignature = UINT32_MAX;
            if (!paintDialog(now)) return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID;
            printf("[DIALOG] PAGE start=%u/%u frame=%08x\n",
                   (unsigned int)dialog.currentDialogLine,
                   (unsigned int)dialog.lineCount,
                   (unsigned int)frameFNV());
            return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN;
        }
        closeActive(outClose, 1U, "resume");
        return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_RESUME;
    }

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD) {
        if (dialog.currentDialogLine > 0U) --dialog.currentDialogLine;
        dialog.lastPaintSignature = UINT32_MAX;
        if (!paintDialog(now)) return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID;
        return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN;
    }

    if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK) {
        maxStart = dialog.lineCount > ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES
                       ? (uint16_t)(dialog.lineCount -
                                    ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES)
                       : 0U;
        if (dialog.currentDialogLine < maxStart) {
            ++dialog.currentDialogLine;
            dialog.lineStartMs = now;
            dialog.dialogTypeLineIdx = 3U;
        }
        dialog.lastPaintSignature = UINT32_MAX;
        if (!paintDialog(now)) return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_INVALID;
        return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN;
    }

    if (dialog.backAllowed != 0U &&
        (action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ||
         action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT ||
         action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN)) {
        closeActive(outClose, 0U, "back-cancel");
        return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_CANCEL;
    }

    return ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_IGNORED;
}

EspNativeGameplayDialogResumeStatus EspNativeGameplayDialog_resume(
    const EspNativeGameplayDialogClose* close,
    EspNativeGameplayDialogResumeResult* outResult) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapByteCode command;
    EspMapOpcodeExecResult exec;
    EspMapOpcodeExecStatus execStatus;
    uint8_t currentState;
    uint8_t removed;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (close == NULL || outResult == NULL ||
        close->resumeRequested != 1U || EspNativeGameplayDialog_isActive() ||
        EspAssetPack_isOpen() ||
        !eventDescriptorForIndex(close->sourceEventIndex, &descriptor) ||
        close->resumeCommandOffset > descriptor.commandCount) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    }

    if (close->resumeHasCommand == 0U) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND;
    }
    if (close->resumeCommandOffset >= descriptor.commandCount ||
        !EspMapScriptState_getEventState(descriptor.eventIndex, &currentState) ||
        !EspMapEventFilter_prepare(&descriptor,
                                   currentState,
                                   close->resumeCommandOffset,
                                   close->runFlags,
                                   0U,
                                   &plan) ||
        !EspMapScriptState_isCommandRemoved(close->resumeGlobalCommandIndex,
                                             &removed) ||
        removed != close->removedBefore ||
        !EspMapEventFilter_evaluate(&descriptor,
                                    &plan,
                                    close->resumeCommandOffset,
                                    removed,
                                    &filtered) ||
        filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE ||
        filtered.globalCommandIndex != close->resumeGlobalCommandIndex ||
        filtered.codeId != close->resumeCodeId ||
        !EspMapOpcodeExecutor_supports(filtered.codeId) ||
        !EspMapEvents_getCommand(&descriptor,
                                 close->resumeCommandOffset,
                                 &command) ||
        command.id != close->resumeCodeId) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    }

    memset(&exec, 0, sizeof(exec));
    execStatus = EspMapOpcodeExecutor_execute(&command, &exec);
    if (execStatus != ESP_MAP_OPCODE_EXEC_OK) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
    }

    outResult->opcode = exec;
    outResult->globalCommandIndex = close->resumeGlobalCommandIndex;
    outResult->codeId = close->resumeCodeId;
    outResult->removedBefore = close->removedBefore;
    outResult->removedAfter = close->removedBefore;
    outResult->mutated = exec.mutated;

    if ((command.arg2 & DIALOG_REMOVE_FLAG) != 0U) {
        if (!EspMapScriptState_setCommandRemoved(close->resumeGlobalCommandIndex,
                                                  1U)) {
            if (exec.mutated != 0U) {
                (void)EspMapScriptState_setEventState(exec.targetEventIndex,
                                                       exec.stateBefore);
            }
            memset(outResult, 0, sizeof(*outResult));
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
        }
        outResult->removedAfter = 1U;
    }

    outResult->rollbackAvailable =
        (uint8_t)((outResult->mutated != 0U ||
                   outResult->removedBefore != outResult->removedAfter)
                      ? 1U
                      : 0U);

    printf("[DIALOG] RESUME event=%u offset=%u opcode=%u global=%u targetEvent=%u state=%u->%u removed=%u->%u mutation=%u\n",
           (unsigned int)close->sourceEventIndex,
           (unsigned int)close->resumeCommandOffset,
           (unsigned int)outResult->codeId,
           (unsigned int)outResult->globalCommandIndex,
           (unsigned int)outResult->opcode.targetEventIndex,
           (unsigned int)outResult->opcode.stateBefore,
           (unsigned int)outResult->opcode.stateAfter,
           (unsigned int)outResult->removedBefore,
           (unsigned int)outResult->removedAfter,
           (unsigned int)outResult->mutated);
    return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK;
}

int EspNativeGameplayDialog_rollbackResume(
    const EspNativeGameplayDialogResumeResult* result) {
    uint8_t stateNow;
    uint8_t removedNow;
    if (result == NULL || result->rollbackAvailable != 1U) return 0;
    if (!EspMapScriptState_getEventState(result->opcode.targetEventIndex,
                                         &stateNow) ||
        !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                             &removedNow) ||
        stateNow != result->opcode.stateAfter ||
        removedNow != result->removedAfter) {
        return 0;
    }
    if (result->mutated != 0U &&
        !EspMapScriptState_setEventState(result->opcode.targetEventIndex,
                                         result->opcode.stateBefore)) {
        return 0;
    }
    if (result->removedBefore != result->removedAfter &&
        !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                              result->removedBefore)) {
        if (result->mutated != 0U) {
            (void)EspMapScriptState_setEventState(result->opcode.targetEventIndex,
                                                   result->opcode.stateAfter);
        }
        return 0;
    }
    return 1;
}

const char* EspNativeGameplayDialog_beginStatusName(
    EspNativeGameplayDialogBeginStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE: return "TEXT_TOO_LARGE";
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME: return "UNSUPPORTED_RESUME";
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED: return "IO_FAILED";
    case ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK: return "OK";
    default: return "UNKNOWN";
    }
}

const char* EspNativeGameplayDialog_resumeStatusName(
    EspNativeGameplayDialogResumeStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND: return "NO_COMMAND";
    case ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED: return "EXEC_FAILED";
    case ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK: return "OK";
    default: return "UNKNOWN";
    }
}
