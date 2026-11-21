#!/usr/bin/env python3
"""ARCH-RC22: exercise four ACTUAL C bodies and reject adversarial mutants.

A real-body host oracle, not firmware emulation or a CYD pass. Only the small
external PlayerState/weapon and ResourceOwner storage dependencies are faked;
the real PlayerView CAS helpers, raw HUD view, pending-MOVE facade and projected
HUD facade are compiled unmodified from the firmware sources on each run.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
FILES = {
    "view": (SRC / "esp_player_view_state.c").read_text(encoding="utf-8"),
    "hud": (SRC / "esp_native_gameplay_hud.c").read_text(encoding="utf-8"),
    "resources": (SRC / "esp_native_gameplay_player_resources.c").read_text(encoding="utf-8"),
}
RETIRED = ("EspPlayerView_commitPreparedMove", "EspNativeGameplayHud_view")
RAW = {
    "isReady": ("view", "int EspPlayerView_isReady(void)"),
    "getView": ("view", "const EspPlayerViewState* EspPlayerView_view(void)"),
    "center": ("view", "static int runtimeCenterCoordinate(int32_t value)"),
    "settled": ("view", "static int viewSettledForRuntimeAction(const EspPlayerViewState* view)"),
    "samePosition": ("view", "static int sameExceptRuntimePosition(const EspPlayerViewState* a,"),
    "step": ("view", "static int cardinalStep(int32_t deltaX, int32_t deltaY)"),
    "commitBase": ("view", "EspPlayerViewMoveStatus EspPlayerView_commitPreparedMoveBase("),
    "hudReady": ("hud", "int EspNativeGameplayHud_isReady(void)"),
    "hudBase": ("hud", "const EspNativeGameplayHudState* EspNativeGameplayHud_viewBase(void)"),
    "clearPending": ("resources", "static void clearPending(void)"),
    "commitPublic": ("resources", "EspPlayerViewMoveStatus EspPlayerView_commitPreparedMove("),
    "hudPublic": ("resources", "const EspNativeGameplayHudState* EspNativeGameplayHud_view(void)"),
}


def require(ok, why):
    if not ok:
        raise AssertionError(why)


def extract(src, signature):
    require(src.count(signature) == 1, "unique actual C body: " + signature)
    start = src.index(signature)
    brace = src.index("{", start)
    level = 0
    for idx in range(brace, len(src)):
        if src[idx] == "{":
            level += 1
        elif src[idx] == "}":
            level -= 1
            if level == 0:
                return src[start:idx + 1]
    raise AssertionError("unbalanced source C body: " + signature)


BODIES = {name: extract(FILES[owner], sig) for name, (owner, sig) in RAW.items()}
CONSTS = []
for name in ("RUNTIME_STEP_SIZE", "RUNTIME_MIN_CENTER", "RUNTIME_MAX_CENTER"):
    matches = re.findall(r"^#define " + name + r" .+$", FILES["view"], re.M)
    require(len(matches) == 1, "original runtime move constant changed: " + name)
    CONSTS.append(matches[0])

PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_player_view_state.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_native_gameplay_combat_math.h"

/* Only external test storage is trimmed: real C function bodies and runtime
 * owner struct definitions from public headers are NOT reimplemented. */
static EspPlayerViewState playerViewState;
static EspNativeGameplayHudState hudState;
typedef struct {
    EspNativeGameplayPlayerResourcesView view;
    uint8_t* consumedBits;
    uint32_t consumedCapacity;
    EspPlayerViewState pendingBefore;
    EspPlayerViewState pendingAfter;
    uint8_t corpusLogged;
} ResourceOwnerHostStorage;
static ResourceOwnerHostStorage resources;
static EspNativeGameplayHudState hudOverlay;
"""

SUFFIX = r"""
static EspNativeGameplayPlayerState player;
static EspNativeGameplayWeaponSpec weapon;
static int ensureAllowed, viewAllowed, ensureCalls;

int EspNativeGameplayPlayerState_ensure(void) {
    ++ensureCalls;
    return ensureAllowed;
}
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    return viewAllowed ? &player : NULL;
}
const EspNativeGameplayWeaponSpec* EspNativeGameplayCombatMath_weapon(uint8_t id) {
    return id == 3U && id == player.weapon ? &weapon : NULL;
}

static void resetAll(void) {
    memset(&playerViewState, 0, sizeof(playerViewState));
    memset(&hudState, 0, sizeof(hudState));
    memset(&resources, 0, sizeof(resources));
    memset(&hudOverlay, 0, sizeof(hudOverlay));
    memset(&player, 0, sizeof(player));
    memset(&weapon, 0, sizeof(weapon));
    ensureAllowed = 1;
    viewAllowed = 1;
    ensureCalls = 0;
    playerViewState.active = 1U;
    playerViewState.spawnApplied = 1U;
    playerViewState.viewX = 288;
    playerViewState.viewY = 352;
    playerViewState.destX = 288;
    playerViewState.destY = 352;
    playerViewState.viewAngle = 0;
    playerViewState.destAngle = 0;
    hudState.active = 1U;
    hudState.painted = 1U;
    hudState.model.health = 98U;
    hudState.model.ammo = 51U;
    hudState.model.weapon = 0U;
    hudState.model.weaponsPresent = 1U;
    player.param1 = (uint32_t)7U | ((uint32_t)49U << 8) |
                    ((uint32_t)13U << 16) | ((uint32_t)75U << 24);
    player.weapon = 3U;
    player.weapons = (uint16_t)(1U << 3);
    player.ammo[2] = 17U;
    weapon.ammoType = 2U;
}

static void moveCases(void) {
    EspPlayerViewState before, after, other, snapshot;
    resetAll();
    before = playerViewState;
    after = before;
    after.viewX += 64;
    after.destX = after.viewX;
    snapshot = playerViewState;

    assert(EspPlayerView_commitPreparedMove(NULL, &after) ==
           ESP_PLAYER_VIEW_MOVE_INVALID);
    assert(EspPlayerView_commitPreparedMove(&before, NULL) ==
           ESP_PLAYER_VIEW_MOVE_INVALID);
    assert(memcmp(&playerViewState, &snapshot, sizeof(snapshot)) == 0);
    assert(resources.view.pendingMove == 0U);

    other = before;
    other.viewX += 64;
    assert(EspPlayerView_commitPreparedMove(&other, &after) ==
           ESP_PLAYER_VIEW_MOVE_STALE);
    assert(resources.view.pendingMove == 0U);

    playerViewState.active = 0U;
    assert(EspPlayerView_commitPreparedMove(&before, &after) ==
           ESP_PLAYER_VIEW_MOVE_NOT_READY);
    playerViewState = before;
    other = before;
    other.facingRefreshPending = 1U;
    playerViewState = other;
    assert(EspPlayerView_commitPreparedMove(&other, &after) ==
           ESP_PLAYER_VIEW_MOVE_UNSETTLED);
    playerViewState = before;

    other = after;
    other.viewX += 64;
    other.destX = other.viewX;
    assert(EspPlayerView_commitPreparedMove(&before, &other) ==
           ESP_PLAYER_VIEW_MOVE_UNSUPPORTED);
    assert(resources.view.pendingMove == 0U);
    other = after;
    other.viewAngle = 64;
    other.destAngle = 64;
    assert(EspPlayerView_commitPreparedMove(&before, &other) ==
           ESP_PLAYER_VIEW_MOVE_UNSUPPORTED);

    assert(EspPlayerView_commitPreparedMove(&before, &after) ==
           ESP_PLAYER_VIEW_MOVE_OK);
    assert(memcmp(&playerViewState, &after, sizeof(after)) == 0);
    assert(resources.view.pendingMove == 1U);
    assert(memcmp(&resources.pendingBefore, &before, sizeof(before)) == 0);
    assert(memcmp(&resources.pendingAfter, &after, sizeof(after)) == 0);
    assert(EspPlayerView_commitPreparedMove(&before, &after) ==
           ESP_PLAYER_VIEW_MOVE_STALE);
    assert(resources.view.pendingMove == 1U);

    /* Exact reverse commit cancels only this pending pickup lease. */
    assert(EspPlayerView_commitPreparedMove(&after, &before) ==
           ESP_PLAYER_VIEW_MOVE_OK);
    assert(memcmp(&playerViewState, &before, sizeof(before)) == 0);
    assert(resources.view.pendingMove == 0U);
    assert(resources.pendingBefore.active == 0U);
    assert(resources.pendingAfter.active == 0U);

    /* Re-arm and then overwrite on next successful step (one-shot service). */
    assert(EspPlayerView_commitPreparedMove(&before, &after) ==
           ESP_PLAYER_VIEW_MOVE_OK);
    other = after;
    other.viewY -= 64;
    other.destY = other.viewY;
    assert(EspPlayerView_commitPreparedMove(&after, &other) ==
           ESP_PLAYER_VIEW_MOVE_OK);
    assert(memcmp(&playerViewState, &other, sizeof(other)) == 0);
    assert(memcmp(&resources.pendingBefore, &after, sizeof(after)) == 0);
    assert(memcmp(&resources.pendingAfter, &other, sizeof(other)) == 0);
}

static void hudCases(void) {
    EspNativeGameplayHudState retained, projected;
    const EspNativeGameplayHudState* result;
    resetAll();
    retained = hudState;

    hudState.painted = 0U;
    assert(EspNativeGameplayHud_viewBase() == NULL);
    assert(EspNativeGameplayHud_view() == NULL);
    assert(ensureCalls == 0);
    hudState = retained;

    ensureAllowed = 0;
    assert(EspNativeGameplayHud_view() == &hudState);
    assert(ensureCalls == 1);
    ensureAllowed = 1;
    viewAllowed = 0;
    assert(EspNativeGameplayHud_view() == &hudState);
    viewAllowed = 1;

    result = EspNativeGameplayHud_view();
    assert(result != NULL && result != &hudState);
    assert(result->active == 1U && result->painted == 1U);
    assert(result->model.health == 7U);
    assert(result->model.maxHealth == 49U);
    assert(result->model.armor == 13U);
    assert(result->model.maxArmor == 75U);
    assert(result->model.weapon == 3U);
    assert(result->model.weaponsPresent == 1U);
    assert(result->model.ammoType == 2U && result->model.ammo == 17U);
    assert(memcmp(&hudState, &retained, sizeof(retained)) == 0);
    projected = *result;

    player.weapons = 0U;
    result = EspNativeGameplayHud_view();
    assert(result->model.weaponsPresent == 0U);
    assert(memcmp(&hudState, &retained, sizeof(retained)) == 0);
    player.weapons = (uint16_t)(1U << 3);

    /* Unsupported ammo slot never reads outside the bounded ammo owner. */
    weapon.ammoType = ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES;
    result = EspNativeGameplayHud_view();
    assert(result->model.ammo == retained.model.ammo);
    assert(result->model.ammoType == retained.model.ammoType);
    weapon.ammoType = 2U;

    player.weapon = 1U;
    result = EspNativeGameplayHud_view();
    assert(result->model.weapon == 1U);
    assert(result->model.ammo == retained.model.ammo);
    assert(memcmp(&projected, &retained, sizeof(retained)) != 0);
    assert(memcmp(&hudState, &retained, sizeof(retained)) == 0);
}

int main(void) {
    moveCases();
    hudCases();
    puts("PASS ARCH-RC22 actual PlayerView/HUD C and resource facades");
    return 0;
}
"""


def scan():
    ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    normal = ini.split("[env:esp32-cyd]", 1)[1].split("[env:esp-cyd-inverted]", 1)[0]
    bringup = ini.split("[env:esp32-cyd-bringup]", 1)[1].split("[env:esp32-cyd-trace]", 1)[0]
    active = lambda txt: re.findall(r"^\s+-Wl,--wrap=([A-Za-z_0-9]+)", txt, re.M)
    require(set(active(normal)) == {"Esp32PlatformVideo_present"},
        "RC23 must leave present as sole production interposition")
    require(set(active(bringup)) == {
        "EspNativeFirstFrame_route", "EspNativeGameplayInteractionInventory_log",
        "EspNativeResidentGameplay_service"}, "bringup diagnostics drifted")
    for old in RETIRED:
        require("-Wl,--wrap=" + old not in ini, "retired GNU linker flag: " + old)
        for path in SRC.iterdir():
            if path.suffix not in (".c", ".cpp"):
                continue
            clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", path.read_text())
            require(not re.search(r"\b__(?:real|wrap)_" + old + r"\b", clean),
                    "retired linker alias in " + path.name)
    for label in RAW:
        require(label in BODIES and BODIES[label], "missing actual C body " + label)
    for name in ("EspPlayerView_commitPreparedMoveBase",
                 "EspNativeGameplayHud_viewBase"):
        require(name in FILES["view"] + FILES["hud"], "raw leaf missing")
    require("EspPlayerView_commitPreparedMoveBase(expectedBefore, preparedAfter)" in BODIES["commitPublic"],
            "public MOVE must delegate to raw CAS")
    require("EspNativeGameplayHud_viewBase()" in BODIES["hudPublic"],
            "public HUD must delegate to raw retained snapshot")


def compile_run(bodies):
    actual = PREFIX + "\n" + "\n".join(CONSTS) + "\n"
    actual += "\n\n".join(bodies[k] for k in RAW) + "\n" + SUFFIX
    with tempfile.TemporaryDirectory(prefix="arch-rc22-") as temp:
        unit = Path(temp) / "actual.c"
        binary = Path(temp) / "actual"
        unit.write_text(actual, encoding="utf-8")
        build = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-function",  # mutant removing last raw helper call
             "-I" + str(ROOT / "include"), str(unit), "-o", str(binary)],
            capture_output=True, text=True, timeout=30)
        require(build.returncode == 0,
                "actual body failed to compile:\n" + build.stderr[:2800])
        run = subprocess.run([str(binary)], capture_output=True,
                             text=True, timeout=15)
        return run.returncode == 0 and "PASS ARCH-RC22 actual" in run.stdout


MUTANTS = (
    ("stale CAS", "commitBase",
     "memcmp(&playerViewState, expectedBefore, sizeof(playerViewState)) != 0",
     "memcmp(&playerViewState, expectedBefore, sizeof(playerViewState)) == 0"),
    ("no commit publication", "commitBase",
     "playerViewState = *preparedAfter;", "playerViewState = *expectedBefore;"),
    ("ignore nonposition mutation", "commitBase",
     "!sameExceptRuntimePosition(expectedBefore, preparedAfter)",
     "sameExceptRuntimePosition(expectedBefore, preparedAfter)"),
    ("reject orthogonal move", "step",
     "(deltaX == 0 && deltaY == -RUNTIME_STEP_SIZE)",
     "(deltaX == 0 && deltaY == -1)"),
    ("no pending lease", "commitPublic",
     "resources.view.pendingMove = 1U;", "resources.view.pendingMove = 0U;"),
    ("forget rollback", "commitPublic",
     "clearPending();", "resources.view.pendingMove = 1U;"),
    ("wrong before snapshot", "commitPublic",
     "resources.pendingBefore = *expectedBefore;",
     "resources.pendingBefore = *preparedAfter;"),
    ("wrong after snapshot", "commitPublic",
     "resources.pendingAfter = *preparedAfter;",
     "resources.pendingAfter = *expectedBefore;"),
    ("commit failure still staged", "commitPublic",
     "status != ESP_PLAYER_VIEW_MOVE_OK", "status == ESP_PLAYER_VIEW_MOVE_OK"),
    ("HUD painted guard", "hudPublic",
     "base->painted != 1U", "base->painted != 0U"),
    ("HUD ensure bypass", "hudPublic",
     "!EspNativeGameplayPlayerState_ensure()", "0"),
    ("HUD wrong health", "hudPublic",
     "player->param1 & 0xffU", "(player->param1 >> 8) & 0xffU"),
    ("HUD stale weapon", "hudPublic",
     "hudOverlay.model.weapon = player->weapon;",
     "hudOverlay.model.weapon = base->model.weapon;"),
    ("HUD stale ammo", "hudPublic",
     "hudOverlay.model.ammo = player->ammo[weapon->ammoType];",
     "hudOverlay.model.ammo = base->model.ammo;"),
    ("HUD false weapons-present", "hudPublic",
     "player->weapons != 0U ? 1U : 0U",
     "1U"),
)


def main():
    scan()
    require(compile_run(BODIES), "unmutated actual C bodies failed semantic tests")
    for label, unit, before, after in MUTANTS:
        require(BODIES[unit].count(before) == 1,
                "adversarial fixture drift: " + label)
        mutation = dict(BODIES)
        mutation[unit] = mutation[unit].replace(before, after)
        require(not compile_run(mutation), "adversarial mutation survived: " + label)
    print("PASS ARCH-RC22 12 real C helpers/facades + 15 compiled adversarial mutants; 2+3 GNU wrap inventory")


if __name__ == "__main__":
    main()
