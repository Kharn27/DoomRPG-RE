#!/usr/bin/env python3
"""ARCH-RC12: verify explicit dialog transaction ownership and REAL NOTE begin C.

The source census rejects GNU alias resurrection/bypass in all firmware TUs.
The C probe compiles the actual NOTE+dialog begin body (not a Python rewrite)
with isolated fallible providers, and rejects negative mutations. The existing
paid_interaction_host.c independently executes the full real EventChain C unit.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
NOTE = SRC / "esp_native_gameplay_note_prefix.c"
FLAGS = ROOT / "platformio.ini"


def require(ok, description):
    if not ok:
        raise AssertionError(description)


def clean(text):
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)


def scan():
    files = {p.name: clean(p.read_text(encoding="utf-8"))
             for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(files) >= 178, "incomplete firmware source census")
    flags = FLAGS.read_text(encoding="utf-8")
    targets = ("EspNativeGameplayDialog_begin",
               "EspNativeGameplayDialog_resume",
               "EspNativeGameplayDialog_rollbackResume")
    for name in targets:
        require("-Wl,--wrap=" + name not in flags, "retired linker flag: " + name)
        require(not any("__wrap_" + name in f or "__real_" + name in f
                        for f in files.values()), "GNU alias retained: " + name)
    require("-Wl,--wrap=EspNativeGameplayFrame_renderTurn" not in flags,
            "RC23 retired cardinal render interposition resurrected")
    require("-Wl,--wrap=Esp32PlatformVideo_present" not in flags,
            "remaining physical present interposition missing")
    expected = {
        "EspNativeGameplayDialog_begin":
            {"esp_native_gameplay_dialog.c": 1,
             "esp_native_gameplay_event_chain.c": 2},
        "EspNativeGameplayDialog_resume":
            {"esp_native_gameplay_dialog.c": 1},
        "EspNativeGameplayDialog_rollbackResume":
            {"esp_native_gameplay_dialog.c": 1},
        "EspNativeGameplayNotePrefix_beginDialog":
            {"esp_native_gameplay_note_prefix.c": 1,
             "esp_native_resident_gameplay.c": 2},
        "EspNativeGameplayEventChain_beginDialogCommand":
            {"esp_native_gameplay_event_chain.c": 2,
             "esp_native_gameplay_note_prefix.c": 1,
             "esp_native_resident_gameplay.c": 1},
        "EspNativeGameplayEventChain_resumeDialog":
            {"esp_native_gameplay_event_chain.c": 1,
             "esp_native_resident_gameplay.c": 2},
        "EspNativeGameplayEventChain_rollbackDialogResume":
            {"esp_native_gameplay_event_chain.c": 3,
             "esp_native_resident_gameplay.c": 4},
    }
    for api, callers in expected.items():
        found = {name: len(re.findall(r"\b" + api + r"\s*\(", source))
                 for name, source in files.items()}
        found = {name: count for name, count in found.items() if count}
        require(found == callers, "unexpected owner/caller for " + api
                + ": " + repr(found))
    header = (ROOT / "include/esp_native_gameplay_event_chain.h").read_text()
    note_header = (ROOT / "include/esp_native_gameplay_note_prefix.h").read_text()
    require(header.count("EspNativeGameplayEventChain_resumeDialog(") == 1
            and header.count("EspNativeGameplayEventChain_rollbackDialogResume(") == 1,
            "EventChain permanent API changed")
    require(note_header.count("EspNativeGameplayNotePrefix_beginDialog(") == 1,
            "NOTE permanent API changed")
    print("PASS RC12 firmware census 178+ real TUs: three wraps retired, "
          "raw leaves private, NOTE/CHAIN callsites exact, cardinal door transaction wrapper intact")


def extract_actual_note_body(text):
    match = re.search(
        r"^EspNativeGameplayDialogBeginStatus\s+"
        r"EspNativeGameplayNotePrefix_beginDialog\s*\([^;]*?\)\s*\{",
        text, re.M | re.S)
    require(match is not None, "real NOTE begin C body missing")
    opening = text.index("{", match.start())
    depth = 0
    for offset in range(opening, len(text)):
        if text[offset] == "{":
            depth += 1
        if text[offset] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():offset + 1]
    raise AssertionError("unclosed NOTE C body")


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"
#define NOTE_SCRATCH_CAPACITY 256
typedef enum {
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID,
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY,
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE,
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME,
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED,
  ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK
} EspNativeGameplayDialogBeginStatus;
typedef struct { unsigned length; } EspMapNotebookState;
typedef struct { unsigned index; } EspMapStringRef;
typedef struct { unsigned sourceCommandOffset; EspMapStringRef text; } EspMapUiIntent;
typedef struct { int unused; } EspAssetPackEntry;
typedef enum {
  ESP_MAP_NOTEBOOK_APPLY_OK, ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL,
  ESP_MAP_NOTEBOOK_APPLY_IO_ERROR
} EspMapNotebookApplyStatus;
typedef struct { uint8_t active, targetMapId; } EspPlayerViewState;
typedef struct {
  EspMapNotebookState notebook, candidate;
  char scratch[NOTE_SCRATCH_CAPACITY];
  uint8_t active, targetMapId, busy, reserved;
} EspNativeGameplayNotePrefixState;
static EspNativeGameplayNotePrefixState* notePrefix;
static EspPlayerViewState view = {1U, 1U};
static int prefixResult = 1, beginStatus = ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
static int openAllowed = 1, entryAllowed = 1, mapNameAllowed = 1;
static int setAllowed = 1, allocAllowed = 1;
static int applyStatus = ESP_MAP_NOTEBOOK_APPLY_OK, packOpen;
static int begins, opens, closes, applyCalls, setCalls;
static uint8_t removed;
static void* test_calloc(size_t count, size_t size) {
  return allocAllowed ? calloc(count, size) : NULL;
}
#define calloc test_calloc
static const EspPlayerViewState* EspPlayerView_view(void) { return &view; }
static int EspMapCatalog_isValidId(uint8_t id) { return id == 1U; }
static const char* EspMapCatalog_nameForId(uint8_t id) {
  return id == 1U && mapNameAllowed ? "intro.bsp" : NULL;
}
static int EspAssetPack_isOpen(void) { return packOpen; }
static int EspAssetPack_open(const char* path) {
  assert(strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0);
  ++opens;
  if (!openAllowed) return 0;
  packOpen = 1;
  return 1;
}
static int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
  (void)entry;
  assert(strcmp(name, "intro.bsp") == 0);
  return entryAllowed;
}
static void EspAssetPack_close(void) { assert(packOpen); ++closes; packOpen = 0; }
static void EspMapNotebook_reset(EspMapNotebookState* n) { n->length = 0U; }
static unsigned EspMapNotebook_length(const EspMapNotebookState* n) {
  return n->length;
}
static EspMapNotebookApplyStatus EspMapNotebook_apply(
    EspMapNotebookState* notebook, const EspAssetPackEntry* entry,
    const EspMapUiIntent* intent, char* scratch, size_t capacity, size_t* outLen) {
  (void)entry; (void)intent; (void)scratch;
  assert(notePrefix && notePrefix->busy == 1U && capacity == 256U);
  ++applyCalls;
  if (applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK)
    return (EspMapNotebookApplyStatus)applyStatus;
  notebook->length += 5U;
  *outLen = 5U;
  return ESP_MAP_NOTEBOOK_APPLY_OK;
}
static int EspMapScriptState_setCommandRemoved(uint16_t global, uint8_t value) {
  assert(global == 11U);
  ++setCalls;
  if (!setAllowed) return 0;
  removed = value;
  return 1;
}
static int prepareNotePrefix(
    uint16_t eventIndex, uint8_t offset, uint32_t runFlags,
    EspMapUiIntent* intent, uint16_t* global, uint8_t* before, uint8_t* remove) {
  assert(eventIndex == 67U && offset == 3U && runFlags == 0x500U);
  intent->sourceCommandOffset = 2U;
  intent->text.index = 42U;
  *global = 11U;
  *before = removed;
  *remove = 1U;
  return prefixResult;
}
static EspNativeGameplayDialogBeginStatus beginWithChainPreflight(
    uint16_t eventIndex, uint8_t commandOffset, uint32_t runFlags) {
  assert(eventIndex == 67U && commandOffset == 3U && runFlags == 0x500U);
  ++begins;
  return (EspNativeGameplayDialogBeginStatus)beginStatus;
}
"""

CHECK = r"""
static void reset(void) {
  free(notePrefix);
  notePrefix = NULL;
  view.active = view.targetMapId = 1U;
  prefixResult = 1;
  beginStatus = ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
  openAllowed = entryAllowed = mapNameAllowed = 1;
  setAllowed = allocAllowed = 1;
  applyStatus = ESP_MAP_NOTEBOOK_APPLY_OK;
  packOpen = begins = opens = closes = applyCalls = setCalls = 0;
  removed = 0;
}
static EspNativeGameplayDialogBeginStatus openDialog(void) {
  return EspNativeGameplayNotePrefix_beginDialog(67U, 3U, 0x500U);
}
int main(void) {
  reset(); prefixResult = -1;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID);
  assert(!notePrefix && !opens && !begins && !setCalls);
  reset(); prefixResult = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK);
  assert(!notePrefix && !opens && begins == 1);
  reset();
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK);
  assert(notePrefix && notePrefix->notebook.length == 5U);
  assert(notePrefix->busy == 0U && removed == 1U);
  assert(opens == 1 && closes == 1 && applyCalls == 1 && begins == 1);
  assert(setCalls == 1 && !packOpen);
  reset(); notePrefix = calloc(1U, sizeof(*notePrefix)); notePrefix->busy = 1U;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID);
  assert(!opens && !begins && notePrefix->busy == 1U);
  reset(); view.active = 0U;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY);
  assert(!notePrefix && !opens);
  reset(); packOpen = 1;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY);
  assert(!notePrefix && opens == 0);
  reset(); allocAllowed = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY);
  assert(!notePrefix && !opens);
  reset(); openAllowed = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED);
  assert(notePrefix && !notePrefix->busy && !notePrefix->notebook.length);
  assert(!removed && !packOpen && begins == 0);
  reset(); entryAllowed = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED);
  assert(!notePrefix->busy && !removed && closes == 1 && begins == 0);
  reset(); applyStatus = ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE);
  assert(notePrefix && !notePrefix->busy && !notePrefix->notebook.length);
  assert(!removed && begins == 0 && closes == 1);
  reset(); setAllowed = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID);
  assert(notePrefix && !notePrefix->busy && !notePrefix->notebook.length);
  assert(!removed && begins == 0 && setCalls == 1);
  reset(); beginStatus = ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY);
  assert(notePrefix && !notePrefix->busy && !notePrefix->notebook.length);
  assert(!removed && begins == 1 && setCalls == 2 && !packOpen);
  reset(); mapNameAllowed = 0;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED);
  assert(!notePrefix->busy && !removed && !notePrefix->notebook.length);
  reset(); removed = 1U;
  assert(openDialog() == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK);
  assert(notePrefix->notebook.length == 5U && notePrefix->busy == 0);
  assert(setCalls == 0 && removed == 1U);
  puts("PASS RC12 real NOTE begin C: success, defer, PAK, rollback, reentrance");
  reset();
  return 0;
}
"""


def exercise(body):
    with tempfile.TemporaryDirectory(prefix="doomrpg-rc12-") as work:
        src = Path(work) / "test.c"
        exe = Path(work) / "test"
        src.write_text(PREFIX + "\n" + body + "\n" + CHECK, encoding="utf-8")
        compiled = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             str(src), "-o", str(exe)], capture_output=True, text=True)
        require(compiled.returncode == 0,
                "real extracted NOTE C fails to compile:\n" + compiled.stderr[:5000])
        ran = subprocess.run([str(exe)], capture_output=True, text=True,
                             timeout=10)
        return ran.returncode == 0 and "PASS RC12 real NOTE" in ran.stdout


def main():
    scan()
    body = extract_actual_note_body(NOTE.read_text(encoding="utf-8"))
    require(exercise(body), "unmodified production NOTE begin failed")
    mutants = (
        ("busy reentrance", "if (state != NULL && state->busy != 0U)",
         "if (0)"),
        ("unsupported prefix admitted", "if (prefixStatus < 0)", "if (0)"),
        ("NOTE prefix bypassed", "if (prefixStatus == 0)",
         "if (prefixStatus >= 0)"),
        ("candidate unleased", "state->busy = 1U;\n    state->candidate",
         "state->busy = 0U;\n    state->candidate"),
        ("script remove skipped", "if (removeIfHandled != 0U && removedBefore == 0U)",
         "if (0)"),
        ("setter error ignored", "if (!EspMapScriptState_setCommandRemoved(global, 1U))",
         "if (0)"),
        ("dialog failure ignored", "if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK)",
         "if (0)"),
        ("rollback skipped", "if (removedChanged != 0 &&", "if (0 &&"),
        ("note commit dropped", "state->notebook = state->candidate;",
         "(void)state->candidate;"),
        ("busy leaked after success", "state->busy = 0U;\n    return dialogStatus;",
         "state->busy = 1U;\n    return dialogStatus;"),
        ("apply failure accepted",
         "if (applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK || EspAssetPack_isOpen())",
         "if (0)"),
    )
    for label, before, after in mutants:
        require(body.count(before) == 1, "mutant anchor missing: " + label)
        bad = body.replace(before, after, 1)
        require(not exercise(bad), "undetected NOTE mutant: " + label)
    print("PASS ARCH-RC12 real C NOTE begin: " + str(len(mutants)) +
          " negative mutants rejected, paid EventChain C exercised separately")


if __name__ == "__main__":
    main()
