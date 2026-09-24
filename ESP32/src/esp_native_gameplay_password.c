#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_ui_intent.h"
#include "esp_map_strings.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_gameplay_modal_scratch.h"
#include "esp_native_gameplay_password.h"
#include "esp_native_indexed_bmp.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define PASSWORD_REMOVE_FLAG 0x00000200UL
#define PASSWORD_FONT_NAME "a.bmp"
#define PASSWORD_FONT_WIDTH 9U
#define PASSWORD_FONT_HEIGHT 12U
#define PASSWORD_FONT_ADVANCE 7
#define PASSWORD_FONT_SOURCE_WIDTH 144U
#define PASSWORD_FONT_SOURCE_HEIGHT 72U
#define PASSWORD_TRANSPARENT 1U

#define PASSWORD_KEYPAD_Y 44
#define PASSWORD_KEYPAD_ROWS 4
#define PASSWORD_KEYPAD_COLS 3
#define PASSWORD_KEY_ROW_HEIGHT 19
#define PASSWORD_KEY_COL_WIDTH 53

#define PASSWORD_COLOR_BLACK 0x0000U
#define PASSWORD_COLOR_WHITE 0xffffU
#define PASSWORD_COLOR_DIM 0x8410U
#define PASSWORD_COLOR_DEL 0xf800U
#define PASSWORD_COLOR_VALID 0x07e0U

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native password keypad is defined for the 160x120 logical framebuffer"
#endif

typedef struct EspNativeGameplayPasswordState_s {
    EspNativeGameplayPasswordCompletion completion;
    char prompt[ESP_NATIVE_GAMEPLAY_PASSWORD_PROMPT_CAPACITY];
    char expected[ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE + 1U];
    char entered[ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE + 1U];
    uint32_t runFlags;
    uint32_t paintCount;
    uint32_t fontPackReads;
    uint32_t fontBytesRead;
    uint16_t eventIndex;
    uint16_t globalCommandIndex;
    uint16_t expectedLength;
    uint16_t promptLength;
    uint16_t resumeDialogGlobalCommandIndex;
    uint8_t commandOffset;
    uint8_t resumeOffset;
    uint8_t resumeDialogOffset;
    uint8_t resumeDialogCodeId;
    uint8_t resumeIsDialog;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t removeChanged;
    uint8_t packOwned;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayPasswordState;

/*
 * Password state leases the already-existing NOTE transient owner. No new BSS
 * pointer is introduced: this is required by the hardware-proven 44,832-byte
 * static-RAM startup boundary. The persistent NOTE notebook sits outside the
 * leased span and is preserved.
 */
static EspNativeGameplayPasswordState* passwordState(void) {
    return (EspNativeGameplayPasswordState*)
        EspNativeGameplayModalScratch_view(
            ESP_NATIVE_GAMEPLAY_MODAL_SCRATCH_PASSWORD);
}

#define password (*passwordState())

static int eventDescriptorForIndex(uint16_t eventIndex,
                                   EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t value;

    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (outDescriptor == NULL ||
        !EspMapRuntime_getEvent(eventIndex, &value)) {
        return 0;
    }
    ref.index = eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int firstDialogContinuation(
    const EspMapEventDescriptor* descriptor,
    uint8_t resumeOffset,
    uint32_t runFlags,
    uint8_t* outOffset,
    uint16_t* outGlobal,
    uint8_t* outCodeId) {
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    uint8_t currentState;
    uint32_t offset;

    if (outOffset != NULL) *outOffset = UINT8_MAX;
    if (outGlobal != NULL) *outGlobal = UINT16_MAX;
    if (outCodeId != NULL) *outCodeId = 0U;
    if (descriptor == NULL || outOffset == NULL || outGlobal == NULL ||
        outCodeId == NULL || resumeOffset > descriptor->commandCount ||
        !EspMapScriptState_getEventState(descriptor->eventIndex,
                                         &currentState) ||
        !EspMapEventFilter_prepare(descriptor, currentState,
                                   resumeOffset, runFlags, 0U, &plan)) {
        return -1;
    }

    for (offset = resumeOffset; offset < descriptor->commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor->firstCommandIndex + offset;
        uint8_t removed;
        if (offset > UINT8_MAX || global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(descriptor, &plan, offset,
                                        removed, &filtered)) {
            return -1;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (filtered.codeId != ESP_MAP_OPCODE_DIALOG &&
            filtered.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK) {
            continue;
        }
        *outOffset = (uint8_t)offset;
        *outGlobal = filtered.globalCommandIndex;
        *outCodeId = filtered.codeId;
        return 1;
    }
    return 0;
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL ||
        x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
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

static void drawRect(uint16_t* framebuffer,
                     int x,
                     int y,
                     int width,
                     int height,
                     uint16_t color) {
    int i;
    if (framebuffer == NULL || width <= 1 || height <= 1) return;
    for (i = 0; i < width; ++i) {
        putPixel(framebuffer, x + i, y, color);
        putPixel(framebuffer, x + i, y + height - 1, color);
    }
    for (i = 0; i < height; ++i) {
        putPixel(framebuffer, x, y + i, color);
        putPixel(framebuffer, x + width - 1, y + i, color);
    }
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
               font,
               framebuffer,
               DOOMRPG_LOGICAL_WIDTH,
               DOOMRPG_LOGICAL_HEIGHT,
               (uint16_t)(PASSWORD_FONT_WIDTH * (glyph & 0x0fU)),
               (uint16_t)(PASSWORD_FONT_HEIGHT * (glyph >> 4)),
               PASSWORD_FONT_WIDTH,
               PASSWORD_FONT_HEIGHT,
               (int16_t)x,
               (int16_t)y,
               PASSWORD_TRANSPARENT,
               stats) == ESP_NATIVE_INDEXED_BMP_OK;
}

static int drawLabel(const EspNativeIndexedBmp* font,
                     uint16_t* framebuffer,
                     const char* text,
                     int x,
                     int y,
                     EspNativeIndexedBmpStats* stats) {
    const unsigned char* p = (const unsigned char*)text;
    if (framebuffer == NULL || text == NULL) return 0;
    while (*p != '\0') {
        if (*p == ' ') {
            x += PASSWORD_FONT_ADVANCE;
        }
        else {
            if (!drawGlyph(font, framebuffer, *p, x, y, stats)) return 0;
            x += PASSWORD_FONT_ADVANCE;
        }
        ++p;
    }
    return 1;
}

static int drawPrompt(const EspNativeIndexedBmp* font,
                      uint16_t* framebuffer,
                      EspNativeIndexedBmpStats* stats) {
    uint16_t i;
    int x = 2;
    int y = 2;

    for (i = 0U; i < password.promptLength && y <= 14; ++i) {
        uint8_t c = (uint8_t)password.prompt[i];
        if (c == '|' || c == '\n') {
            x = 2;
            y += PASSWORD_FONT_HEIGHT;
            continue;
        }
        if (x + PASSWORD_FONT_WIDTH > DOOMRPG_LOGICAL_WIDTH - 2) {
            x = 2;
            y += PASSWORD_FONT_HEIGHT;
            if (y > 14) break;
        }
        if (c == ' ') {
            x += PASSWORD_FONT_ADVANCE;
            continue;
        }
        if (!drawGlyph(font, framebuffer, c, x, y, stats)) return 0;
        x += PASSWORD_FONT_ADVANCE;
    }
    return 1;
}

static int drawCode(const EspNativeIndexedBmp* font,
                    uint16_t* framebuffer,
                    EspNativeIndexedBmpStats* stats) {
    char display[ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE + 1U];
    uint16_t enteredLength = (uint16_t)strlen(password.entered);
    uint16_t i;
    int width;
    int x;

    if (password.expectedLength == 0U ||
        password.expectedLength > ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE ||
        enteredLength > password.expectedLength) {
        return 0;
    }

    for (i = 0U; i < password.expectedLength; ++i) {
        display[i] = i < enteredLength ? password.entered[i] : '_';
    }
    display[password.expectedLength] = '\0';
    width = (int)password.expectedLength * PASSWORD_FONT_ADVANCE;
    x = (DOOMRPG_LOGICAL_WIDTH - width) / 2;
    drawRect(framebuffer, 1, 27, DOOMRPG_LOGICAL_WIDTH - 2, 15,
             PASSWORD_COLOR_DIM);
    return drawLabel(font, framebuffer, display, x, 29, stats);
}

static const char* keyLabel(int row, int col) {
    static const char* const labels[PASSWORD_KEYPAD_ROWS][PASSWORD_KEYPAD_COLS] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"DEL", "0", "VALID"}
    };
    if (row < 0 || row >= PASSWORD_KEYPAD_ROWS ||
        col < 0 || col >= PASSWORD_KEYPAD_COLS) {
        return NULL;
    }
    return labels[row][col];
}

static int paintKeypad(void) {
    uint16_t* framebuffer =
        (uint16_t*)Esp32PlatformVideo_framebuffer();
    EspNativeIndexedBmp font;
    EspNativeIndexedBmpStats stats;
    int row;
    int col;

    if (!password.active || !password.packOwned ||
        !EspAssetPack_isOpen() || framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH *
            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0;
    }

    memset(&font, 0, sizeof(font));
    memset(&stats, 0, sizeof(stats));
    if (EspNativeIndexedBmp_open(PASSWORD_FONT_NAME, &font, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        font.width != PASSWORD_FONT_SOURCE_WIDTH ||
        font.height != PASSWORD_FONT_SOURCE_HEIGHT) {
        return 0;
    }

    fillRect(framebuffer, 0, 0,
             DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
             PASSWORD_COLOR_BLACK);

    if (!drawPrompt(&font, framebuffer, &stats) ||
        !drawCode(&font, framebuffer, &stats)) {
        return 0;
    }

    for (row = 0; row < PASSWORD_KEYPAD_ROWS; ++row) {
        for (col = 0; col < PASSWORD_KEYPAD_COLS; ++col) {
            const char* label = keyLabel(row, col);
            int x = col * PASSWORD_KEY_COL_WIDTH + 1;
            int y = PASSWORD_KEYPAD_Y + row * PASSWORD_KEY_ROW_HEIGHT;
            int width = col == PASSWORD_KEYPAD_COLS - 1
                            ? DOOMRPG_LOGICAL_WIDTH - x - 1
                            : PASSWORD_KEY_COL_WIDTH - 2;
            int labelWidth;
            int labelX;
            uint16_t border =
                row == 3 && col == 0
                    ? PASSWORD_COLOR_DEL
                    : (row == 3 && col == 2
                           ? PASSWORD_COLOR_VALID
                           : PASSWORD_COLOR_WHITE);
            if (label == NULL || width <= 2) return 0;
            drawRect(framebuffer, x, y, width, PASSWORD_KEY_ROW_HEIGHT - 1,
                     border);
            labelWidth = (int)strlen(label) * PASSWORD_FONT_ADVANCE;
            labelX = x + (width - labelWidth) / 2;
            if (!drawLabel(&font, framebuffer, label, labelX, y + 2, &stats)) {
                return 0;
            }
        }
    }

    password.fontPackReads += stats.packReads;
    password.fontBytesRead += stats.bytesRead;
    if (!Esp32PlatformVideo_present()) return 0;
    ++password.paintCount;
    return 1;
}

static void rollbackOpenMutation(void) {
    if (passwordState() != NULL &&
        password.active && password.removeChanged != 0U &&
        EspMapScriptState_isReady()) {
        if (!EspMapScriptState_setCommandRemoved(password.globalCommandIndex,
                                                  password.removedBefore)) {
            printf("[PASSWORD] FAILED source-remove rollback event=%u global=%u\n",
                   (unsigned int)password.eventIndex,
                   (unsigned int)password.globalCommandIndex);
        }
    }
}

void EspNativeGameplayPassword_reset(void) {
    EspNativeGameplayPasswordState* state = passwordState();
    if (state == NULL) return;
    rollbackOpenMutation();
    if (state->packOwned != 0U && EspAssetPack_isOpen()) {
        EspAssetPack_close();
    }
    (void)EspNativeGameplayModalScratch_release(
        ESP_NATIVE_GAMEPLAY_MODAL_SCRATCH_PASSWORD, state);
}

int EspNativeGameplayPassword_isActive(void) {
    EspNativeGameplayPasswordState* state = passwordState();
    return state != NULL && state->active != 0U;
}

int EspNativeGameplayPassword_hasPendingCompletion(void) {
    EspNativeGameplayPasswordState* state = passwordState();
    return state != NULL && state->completion.pending != 0U;
}

EspNativeGameplayPasswordBeginStatus EspNativeGameplayPassword_begin(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filtered;
    EspMapByteCode command;
    EspMapStringRef codeRef;
    EspMapStringRef promptRef;
    EspAssetPackEntry mapEntry;
    EspNativeGameplayEventChainPreflightStatus chainStatus;
    EspNativeGameplayDialogBeginStatus dialogChainStatus;
    const char* mapName;
    size_t codeLength = 0U;
    size_t promptLength = 0U;
    uint8_t currentState;
    uint8_t removed;
    uint8_t resumeDialogOffset = UINT8_MAX;
    uint8_t resumeDialogCodeId = 0U;
    uint16_t resumeDialogGlobal = UINT16_MAX;
    uint32_t global;
    uint16_t i;
    int continuationFound;

    if ((passwordState() != NULL &&
         (password.active || password.completion.pending)) ||
        EspAssetPack_isOpen() || view == NULL || view->active != 1U ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0 ||
        !eventDescriptorForIndex(eventIndex, &descriptor) ||
        commandOffset >= descriptor.commandCount ||
        !EspMapEvents_getCommand(&descriptor, commandOffset, &command) ||
        command.id != ESP_MAP_OPCODE_PASSWORD) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY;
    }

    global = (uint32_t)descriptor.firstCommandIndex + commandOffset;
    if (global > UINT16_MAX ||
        !EspMapScriptState_getEventState(eventIndex, &currentState) ||
        !EspMapScriptState_isCommandRemoved(global, &removed) ||
        !EspMapEventFilter_prepare(&descriptor, currentState, 0U,
                                   runFlags, 0U, &filterPlan) ||
        !EspMapEventFilter_evaluate(&descriptor, &filterPlan,
                                    commandOffset, removed, &filtered) ||
        filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE ||
        filtered.codeId != ESP_MAP_OPCODE_PASSWORD ||
        removed != 0U) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
    }

    continuationFound = firstDialogContinuation(
        &descriptor, (uint8_t)(commandOffset + 1U), runFlags,
        &resumeDialogOffset, &resumeDialogGlobal, &resumeDialogCodeId);
    if (continuationFound < 0) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
    }

    if (continuationFound > 0 &&
        (resumeDialogCodeId == ESP_MAP_OPCODE_DIALOG ||
         resumeDialogCodeId == ESP_MAP_OPCODE_DIALOG_NO_BACK)) {
        chainStatus = EspNativeGameplayEventChain_preflightRange(
            eventIndex, (uint8_t)(commandOffset + 1U),
            resumeDialogOffset, runFlags);
        if (chainStatus != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
            printf("[PASSWORD] BEGIN-DEFER event=%u cmd=%u continuation=sync-prefix+dialog prefix=%u..%u opcode=%u reason=prefix-status-%d mutation=no\n",
                   (unsigned int)eventIndex,
                   (unsigned int)commandOffset,
                   (unsigned int)(commandOffset + 1U),
                   (unsigned int)resumeDialogOffset,
                   (unsigned int)resumeDialogCodeId,
                   (int)chainStatus);
            return chainStatus ==
                           ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY
                       ? ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY
                       : ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_UNSUPPORTED_RESUME;
        }
        dialogChainStatus =
            EspNativeGameplayEventChain_preflightDialogCommand(
                eventIndex, resumeDialogOffset, runFlags);
        if (dialogChainStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            printf("[PASSWORD] BEGIN-DEFER event=%u cmd=%u continuation=sync-prefix+dialog opcode=%u dialogCmd=%u status=%s mutation=no\n",
                   (unsigned int)eventIndex,
                   (unsigned int)commandOffset,
                   (unsigned int)resumeDialogCodeId,
                   (unsigned int)resumeDialogOffset,
                   EspNativeGameplayDialog_beginStatusName(dialogChainStatus));
            return dialogChainStatus == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY
                       ? ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY
                       : dialogChainStatus ==
                                 ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE
                             ? ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_TEXT_TOO_LARGE
                             : ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_UNSUPPORTED_RESUME;
        }
    }
    else {
        resumeDialogOffset = UINT8_MAX;
        resumeDialogGlobal = UINT16_MAX;
        resumeDialogCodeId = 0U;
        chainStatus = EspNativeGameplayEventChain_preflight(
            eventIndex, (uint8_t)(commandOffset + 1U), runFlags);
        if (chainStatus != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
            printf("[PASSWORD] BEGIN-DEFER event=%u cmd=%u continuation=%d mutation=no\n",
                   (unsigned int)eventIndex,
                   (unsigned int)commandOffset,
                   (int)chainStatus);
            return chainStatus ==
                           ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY
                       ? ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY
                       : ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_UNSUPPORTED_RESUME;
        }
    }

    if (!EspMapStrings_getRef(command.arg1 & 0xffU, &codeRef) ||
        !EspMapStrings_getRef((command.arg1 >> 8) & 0xffU, &promptRef) ||
        codeRef.length == 0U ||
        codeRef.length > ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE ||
        promptRef.length + 1U >
            ESP_NATIVE_GAMEPLAY_PASSWORD_PROMPT_CAPACITY) {
        return codeRef.length > ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE ||
                       promptRef.length + 1U >
                           ESP_NATIVE_GAMEPLAY_PASSWORD_PROMPT_CAPACITY
                   ? ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_TEXT_TOO_LARGE
                   : ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
    }

    if (EspNativeGameplayModalScratch_acquire(
            ESP_NATIVE_GAMEPLAY_MODAL_SCRATCH_PASSWORD,
            sizeof(EspNativeGameplayPasswordState)) == NULL) {
        printf("[PASSWORD] DEFER reason=shared-modal-scratch bytes=%u mutation=no\n",
               (unsigned int)sizeof(EspNativeGameplayPasswordState));
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY;
    }
    mapName = EspMapCatalog_nameForId(view->targetMapId);
    if (mapName == NULL ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        EspNativeGameplayPassword_reset();
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_IO_FAILED;
    }
    password.packOwned = 1U;

    memset(&mapEntry, 0, sizeof(mapEntry));
    if (!EspAssetPack_findEntry(mapName, &mapEntry) ||
        EspMapStrings_read(&mapEntry, &codeRef,
                           password.expected, sizeof(password.expected),
                           &codeLength) != ESP_MAP_STRING_READ_OK ||
        EspMapStrings_read(&mapEntry, &promptRef,
                           password.prompt, sizeof(password.prompt),
                           &promptLength) != ESP_MAP_STRING_READ_OK ||
        codeLength != codeRef.length ||
        promptLength != promptRef.length) {
        EspNativeGameplayPassword_reset();
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_IO_FAILED;
    }

    for (i = 0U; i < codeLength; ++i) {
        if (password.expected[i] < '0' || password.expected[i] > '9') {
            EspNativeGameplayPassword_reset();
            return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
        }
    }

    password.eventIndex = eventIndex;
    password.globalCommandIndex = (uint16_t)global;
    password.expectedLength = (uint16_t)codeLength;
    password.promptLength = (uint16_t)promptLength;
    password.commandOffset = commandOffset;
    password.resumeOffset = (uint8_t)(commandOffset + 1U);
    password.resumeDialogOffset = resumeDialogOffset;
    password.resumeDialogGlobalCommandIndex = resumeDialogGlobal;
    password.resumeDialogCodeId = resumeDialogCodeId;
    password.resumeIsDialog =
        (uint8_t)(resumeDialogOffset != UINT8_MAX ? 1U : 0U);
    password.runFlags = runFlags;
    password.removedBefore = removed;
    password.removedAfter = removed;
    password.fontPackReads = 0U;
    password.fontBytesRead = 0U;

    if ((command.arg2 & PASSWORD_REMOVE_FLAG) != 0U) {
        if (!EspMapScriptState_setCommandRemoved(global, 1U)) {
            EspNativeGameplayPassword_reset();
            return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID;
        }
        password.removedAfter = 1U;
        password.removeChanged = 1U;
    }

    password.active = 1U;
    if (!paintKeypad()) {
        EspNativeGameplayPassword_reset();
        return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_IO_FAILED;
    }

    printf("[PASSWORD] OPEN event=%u cmd=%u resume=%u codeString=%u promptString=%u digits=%u promptBytes=%u removed=%u->%u keypad=3x4 keys=0-9+DEL+VALID continuation=%s resumeOpcode=%u resumeCmd=%u framePaint=%u pack=open\n",
           (unsigned int)eventIndex,
           (unsigned int)commandOffset,
           (unsigned int)password.resumeOffset,
           (unsigned int)codeRef.index,
           (unsigned int)promptRef.index,
           (unsigned int)password.expectedLength,
           (unsigned int)password.promptLength,
           (unsigned int)password.removedBefore,
           (unsigned int)password.removedAfter,
           password.resumeIsDialog ? "dialog-pause" : "sync-chain",
           (unsigned int)password.resumeDialogCodeId,
           (unsigned int)(password.resumeIsDialog
                              ? password.resumeDialogOffset
                              : password.resumeOffset),
           (unsigned int)password.paintCount);
    return ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_OK;
}

EspNativeGameplayPasswordTapStatus EspNativeGameplayPassword_handleTap(
    int logicalX,
    int logicalY) {
    int row;
    int col;
    uint16_t enteredLength;

    if (passwordState() == NULL ||
        !password.active || !password.packOwned || !EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_INVALID;
    }
    if (logicalX < 0 || logicalX >= DOOMRPG_LOGICAL_WIDTH ||
        logicalY < 0 || logicalY >= DOOMRPG_LOGICAL_HEIGHT ||
        logicalY < PASSWORD_KEYPAD_Y) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_IGNORED;
    }

    row = (logicalY - PASSWORD_KEYPAD_Y) / PASSWORD_KEY_ROW_HEIGHT;
    col = logicalX / PASSWORD_KEY_COL_WIDTH;
    if (col >= PASSWORD_KEYPAD_COLS) col = PASSWORD_KEYPAD_COLS - 1;
    if (row < 0 || row >= PASSWORD_KEYPAD_ROWS ||
        col < 0 || col >= PASSWORD_KEYPAD_COLS) {
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_IGNORED;
    }

    enteredLength = (uint16_t)strlen(password.entered);
    if (row == 3 && col == 0) {
        if (enteredLength != 0U) {
            password.entered[enteredLength - 1U] = '\0';
        }
        if (!paintKeypad()) return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_INVALID;
        printf("[PASSWORD] KEY key=DEL entered=%u/%u\n",
               (unsigned int)(enteredLength != 0U ? enteredLength - 1U : 0U),
               (unsigned int)password.expectedLength);
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_REDRAWN;
    }

    if (row == 3 && col == 2) {
        EspNativeGameplayPasswordCompletion completion;
        int correct =
            enteredLength == password.expectedLength &&
            memcmp(password.entered, password.expected,
                   password.expectedLength) == 0;

        memset(&completion, 0, sizeof(completion));
        completion.close.runFlags = password.runFlags;
        completion.close.sourceEventIndex = password.eventIndex;
        completion.close.resumeGlobalCommandIndex = UINT16_MAX;
        completion.close.sourceCommandOffset = password.commandOffset;
        completion.close.resumeCommandOffset = password.resumeOffset;
        completion.close.dialogCodeId = ESP_MAP_OPCODE_PASSWORD;
        completion.close.resumeGlobalCommandIndex =
            password.resumeDialogGlobalCommandIndex;
        completion.close.resumeCodeId = password.resumeDialogCodeId;
        completion.close.resumeHasCommand = password.resumeIsDialog;
        completion.resumeDialogOffset = password.resumeDialogOffset;
        completion.close.resumeRequested = correct ? 1U : 0U;
        completion.close.backAllowed = 0U;
        completion.close.removedBefore = password.removedBefore;
        completion.expectedLength = password.expectedLength;
        completion.enteredLength = enteredLength;
        completion.correct = correct ? 1U : 0U;
        completion.hadInput = enteredLength != 0U ? 1U : 0U;
        completion.pending = 1U;

        if (password.packOwned != 0U && EspAssetPack_isOpen()) {
            EspAssetPack_close();
        }
        password.packOwned = 0U;
        password.active = 0U;
        password.removeChanged = 0U;
        password.completion = completion;

        printf("[PASSWORD] SUBMIT event=%u cmd=%u entered=%u/%u result=%s continuation=%s pack=closed\n",
               (unsigned int)completion.close.sourceEventIndex,
               (unsigned int)completion.close.sourceCommandOffset,
               (unsigned int)completion.enteredLength,
               (unsigned int)completion.expectedLength,
               completion.correct ? "correct" : "invalid",
               completion.correct ? "resume" : "blocked");
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_SUBMITTED;
    }

    {
        int digit;
        if (row == 3 && col == 1) digit = 0;
        else digit = row * 3 + col + 1;

        if (digit < 0 || digit > 9) {
            return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_IGNORED;
        }
        if (enteredLength < password.expectedLength &&
            enteredLength < ESP_NATIVE_GAMEPLAY_PASSWORD_MAX_CODE) {
            password.entered[enteredLength] = (char)('0' + digit);
            password.entered[enteredLength + 1U] = '\0';
            ++enteredLength;
        }
        if (!paintKeypad()) return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_INVALID;
        printf("[PASSWORD] KEY key=%d entered=%u/%u\n",
               digit,
               (unsigned int)enteredLength,
               (unsigned int)password.expectedLength);
        return ESP_NATIVE_GAMEPLAY_PASSWORD_TAP_REDRAWN;
    }
}

int EspNativeGameplayPassword_takeCompletion(
    EspNativeGameplayPasswordCompletion* outCompletion) {
    EspNativeGameplayPasswordState* state = passwordState();
    if (outCompletion != NULL) memset(outCompletion, 0, sizeof(*outCompletion));
    if (outCompletion == NULL || state == NULL ||
        state->completion.pending != 1U ||
        state->active != 0U || state->packOwned != 0U ||
        EspAssetPack_isOpen()) {
        return 0;
    }
    *outCompletion = state->completion;
    memset(&state->completion, 0, sizeof(state->completion));
    if (!EspNativeGameplayModalScratch_release(
            ESP_NATIVE_GAMEPLAY_MODAL_SCRATCH_PASSWORD, state)) {
        memset(outCompletion, 0, sizeof(*outCompletion));
        return 0;
    }
    return 1;
}

const char* EspNativeGameplayPassword_beginStatusName(
    EspNativeGameplayPasswordBeginStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_TEXT_TOO_LARGE: return "TEXT_TOO_LARGE";
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_UNSUPPORTED_RESUME:
        return "UNSUPPORTED_RESUME";
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_IO_FAILED: return "IO_FAILED";
    case ESP_NATIVE_GAMEPLAY_PASSWORD_BEGIN_OK: return "OK";
    default: return "UNKNOWN";
    }
}
