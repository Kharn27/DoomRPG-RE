#!/usr/bin/env python3
"""Root-J: MENU_MAIN tap gate uses its captured process owner, not global doomRpg.

Pin the exact preceding hardware-tested source as a Git blob after reversing
only this milestone's substitutions, then execute the *production* dispatch
function with two independent owner identities and fail-closed cases.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/native_main_menu_touch.c").read_text(encoding="utf-8")
BEFORE_GIT_BLOB = "7c6ddf89a4bbbd108f4e99b82b6de7fa312efeab"

# Strict ownership: the root token was already captured by TouchPrepare.
assert "static DoomRPG_t* touchDoomRpg = NULL;" in source
assert "touchDoomRpg = doomRpg;" in source
assert "touchActive = 1;\n    PlatformInput_setTapCallback(" in source
assert "touchDoomRpg = NULL;\n    PlatformInput_setTapCallback(NULL);" in source
assert not re.search(r"\bextern\s+DoomRPG_t\s*\*\s*doomRpg\s*;", source)
assert source.count("DoomRPG_t* const doomRpg = touchActive ? touchDoomRpg : NULL;") == 1
assert "else if (result == -1 && touchActive && touchDoomRpg != NULL)" in source
assert '(void)DoomRPG_esp32MainMenuRecover(touchDoomRpg, "slot-selector-back");' in source
assert ("if (touchActive && touchDoomRpg != NULL && "
        "EspNativeRootOwners_live()->doomCanvas != NULL &&") in source
assert ("EspNativeMainMenu_transitionCanvasState("
        "EspNativeRootOwners_live()->doomCanvas, touchDoomRpg, ST_MENU);") in source

# The source must be byte-for-byte identical to the previous Root-I + SDL-F4
# implementation after restoring only the six local Root-J edits.
restore = [
    (
        "static uint32_t gateTapCount = 0;\n\n"
        "void __real_PlatformInput_setTapCallback",
        "static uint32_t gateTapCount = 0;\n\n"
        "extern DoomRPG_t* doomRpg;\n\n"
        "void __real_PlatformInput_setTapCallback",
    ),
    (
        "    /* MENU_MAIN already owns a validated process token from TouchPrepare. */\n"
        "    DoomRPG_t* const doomRpg = touchActive ? touchDoomRpg : NULL;\n",
        "",
    ),
    ("FAILED captured DoomRPG unavailable", "FAILED global DoomRPG unavailable"),
    (
        "else if (result == -1 && touchActive && touchDoomRpg != NULL)",
        "else if (result == -1 && doomRpg != NULL)",
    ),
    (
        'DoomRPG_esp32MainMenuRecover(touchDoomRpg, "slot-selector-back")',
        'DoomRPG_esp32MainMenuRecover(doomRpg, "slot-selector-back")',
    ),
    (
        "if (touchActive && touchDoomRpg != NULL && "
        "EspNativeRootOwners_live()->doomCanvas != NULL &&",
        "if (doomRpg != NULL && EspNativeRootOwners_live()->doomCanvas != NULL &&",
    ),
    (
        "EspNativeRootOwners_live()->doomCanvas, touchDoomRpg, ST_MENU);",
        "EspNativeRootOwners_live()->doomCanvas, doomRpg, ST_MENU);",
    ),
]
normalized = source
for old, new in restore:
    if normalized.count(old) != 1:
        raise AssertionError(f"Expected one Root-J replacement: {old[:80]!r}")
    normalized = normalized.replace(old, new, 1)
blob = normalized.encode("utf-8")
blob_hash = hashlib.sha1(b"blob " + str(len(blob)).encode("ascii") + b"\0" + blob).hexdigest()
assert blob_hash == BEFORE_GIT_BLOB, (
    f"Root-J touched code outside its bounded replacements: {blob_hash}"
)

start = source.index("static void executeConfirmedAction(int item) {")
end = source.index("\n\nstatic void gatedTap(", start)
production_dispatch = source[start:end]
assert production_dispatch.count("DoomRPG_esp32MainMenuDispatchConfirmed(") == 1
assert production_dispatch.count("DoomRPG_esp32MainMenuRecover(") == 1
assert production_dispatch.count("disableMainMenuTouchForTransition()") == 1

c = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct DoomRPG_s { unsigned char nativeProcessIdentity; } DoomRPG_t;
typedef enum {
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED = -1,
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN = 0,
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED = 1
} DoomRpgEsp32MainMenuDispatchResult;
enum { DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD = 1 };
typedef struct { int state; } Canvas;
typedef struct { int menu; } Menu;
typedef struct { Canvas* doomCanvas; Menu* menuSystem; } Owners;
static Canvas canvas = {9};
static Menu menu = {1};
static Owners owners = {&canvas, &menu};
static DoomRPG_t rootA = {0xa5}, rootB = {0x5a};
static DoomRPG_t* touchDoomRpg;
static int touchActive;
static int dispatchCount, recoveryCount, disabledCount;
static int nextResult, nextRecover;
static int lastItem;
static DoomRPG_t* lastDispatchOwner;
static DoomRPG_t* lastRecoveryOwner;

static Owners* EspNativeRootOwners_live(void) { return &owners; }
static void disableMainMenuTouchForTransition(void) { ++disabledCount; }
static DoomRpgEsp32MainMenuDispatchResult
DoomRPG_esp32MainMenuDispatchConfirmed(DoomRPG_t* root, int item) {
    assert(root != NULL);
    lastDispatchOwner = root;
    lastItem = item;
    ++dispatchCount;
    return (DoomRpgEsp32MainMenuDispatchResult)nextResult;
}
static int DoomRPG_esp32MainMenuRecover(DoomRPG_t* root, const char* reason) {
    assert(root != NULL && strcmp(reason, "dispatch-failed") == 0);
    lastRecoveryOwner = root;
    ++recoveryCount;
    return nextRecover;
}

__PRODUCTION_DISPATCH__

int main(void) {
    for (int i = 0; i < 4096; ++i) {
        DoomRPG_t* owner = (i & 1) ? &rootA : &rootB;
        int action = (i % 4 == 0) ? DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD : 0;
        touchDoomRpg = owner;
        touchActive = 1;
        dispatchCount = recoveryCount = disabledCount = 0;
        lastDispatchOwner = lastRecoveryOwner = NULL;
        lastItem = -9;
        nextResult = (i % 3) - 1;
        nextRecover = i & 1;
        owners.doomCanvas = (i % 19 == 0) ? NULL : &canvas;
        owners.menuSystem = (i % 17 == 0) ? NULL : &menu;

        executeConfirmedAction(action);
        assert(dispatchCount == 1 && lastDispatchOwner == owner);
        assert(lastItem == action);
        assert(disabledCount == (action != DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD));
        assert(recoveryCount == (nextResult == DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED));
        if (recoveryCount) assert(lastRecoveryOwner == owner);

        // No bound owner, or a gate no longer active: neither dispatch nor
        // callback removal may occur. No global fallback exists to rescue it.
        touchDoomRpg = NULL;
        dispatchCount = recoveryCount = disabledCount = 0;
        executeConfirmedAction(action);
        assert(dispatchCount == 0 && recoveryCount == 0 && disabledCount == 0);
        touchDoomRpg = owner;
        touchActive = 0;
        executeConfirmedAction(action);
        assert(dispatchCount == 0 && recoveryCount == 0 && disabledCount == 0);
    }
    puts("PASS Root-J captured touch owner: 4096 cycles, 2 roots, LOAD/START,");
    puts("     fail/recover, NULL/inactive fail-closed; no global owner ABI");
    return 0;
}
'''
c = c.replace("__PRODUCTION_DISPATCH__", production_dispatch)
with tempfile.TemporaryDirectory(prefix="doomrpg-root-j-") as temp:
    input_path = Path(temp) / "root_j_touch.c"
    executable = Path(temp) / "root_j_touch"
    input_path.write_text(c, encoding="utf-8")
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         str(input_path), "-o", str(executable)],
        check=True,
    )
    completed = subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True
    )
    assert "PASS Root-J captured touch owner" in completed.stdout

print("PASS Root-J: exact source equivalence, all 3 callback paths,")
print("     production dispatch 4096 cycles across 2 roots, NULL/inactive")
