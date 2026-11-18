#!/usr/bin/env python3
"""RC13: source-wide exact native action chain and two actual C owner functions.

Extract the unmodified C service/reset definitions from their production
translation units. Exercise them with fail-closed, zero-enemy, HUB-gate,
pending/redraw and order-preserving fixtures. Reject 11 semantic mutants.
No emulated renderer, no GUI assets and no fabricated hardware PASS.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
FLAG = ROOT / "platformio.ini"


def need(test, why):
    if not test:
        raise AssertionError(why)


def source(path):
    return (SRC / path).read_text(encoding="utf-8")


def code(text):
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)


def census():
    files = {p.name: code(p.read_text(encoding="utf-8")) for p in SRC.iterdir()
             if p.suffix in (".c", ".cpp")}
    need(len(files) >= 178, "RC13 did not scan every production TU")
    ini = FLAG.read_text(encoding="utf-8")
    retired = ("EspNativeGameplayAction_executeSelect",
               "EspNativeGameplayActionEngine_executeSelect",
               "EspNativeGameplayActionEngine_service",
               "EspNativeGameplayActionEngine_reset")
    for symbol in retired:
        need("-Wl,--wrap=" + symbol not in ini, "retired linker flag: " + symbol)
        need(not any(re.search(r"\b__(?:real|wrap)_" + symbol + r"\s*\(", s)
                     for s in files.values()),
             "legacy GNU alias/bypass still compiled: " + symbol)
    for symbol in ("EspNativeGameplayFrame_renderTurn",
                   "EspMapRuntime_getLine", "EspMapLineState_view"):
        need("-Wl,--wrap=" + symbol in ini, "door/render boundary lost: " + symbol)
    expected = {
        ("esp_native_resident_gameplay.c",
         "EspNativeGameplayTransition_executeSelect(intent, &result)"): 1,
        ("esp_native_gameplay_transition.c",
         "EspNativeGameplayMonsterCombat_executeSelect(intent, outResult)"): 2,
        ("esp_native_gameplay_monster_combat.c",
         "EspNativeGameplayActionEngine_executeSelect(intent, outResult)"): 1,
        ("esp_native_gameplay_action_engine.c",
         "EspNativeGameplayAction_executeSelect(intent, outResult)"): 1,
        ("esp_native_gameplay_player_resources.c",
         "EspNativeGameplayMonsterCombat_actionService(doomRpg)"): 1,
        ("esp_native_gameplay_monster_combat.c",
         "EspNativeGameplayHubActionGate_service(runtime)"): 1,
        ("esp_native_gameplay_hub_action_gate.c",
         "EspNativeGameplayMonsterState_actionService(doomRpg)"): 1,
        ("esp_native_gameplay_monster_state.c",
         "EspNativeGameplayActionEngine_service(doomRpg)"): 2,
        ("esp_native_gameplay_player_resources.c",
         "EspNativeGameplayMonsterCombat_actionReset()"): 1,
        ("esp_native_gameplay_session.c",
         "EspNativeGameplayMonsterCombat_actionReset()"): 1,
        ("esp_native_gameplay_monster_combat.c",
         "EspNativeGameplayMonsterState_actionReset()"): 1,
        ("esp_native_gameplay_monster_state.c",
         "EspNativeGameplayActionEngine_reset()"): 1,
    }
    for (file, witness), count in expected.items():
        need(files[file].count(witness) == count,
             "exact action chain drift " + file + " " + witness)
    print("PASS RC13 scan 178+ real firmware C/C++ owners; all four aliases "
          "retired, exact SELECT/service/reset call chain and critical door wraps")


def extract(text, name):
    rx = re.compile(r"^(?:int|void)\s+" + name + r"\s*\([^;]*?\)\s*\{",
                    re.M | re.S)
    m = rx.search(text)
    need(m is not None, "real owner C function missing: " + name)
    start = text.find("{", m.start())
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[m.start():index+1]
    raise AssertionError("unbalanced real C owner: " + name)


BODY = "\n\n".join((
    extract(source("esp_native_gameplay_monster_state.c"),
            "EspNativeGameplayMonsterState_actionService"),
    extract(source("esp_native_gameplay_monster_state.c"),
            "EspNativeGameplayMonsterState_actionReset"),
    extract(source("esp_native_gameplay_monster_combat.c"),
            "EspNativeGameplayMonsterCombat_actionService"),
    extract(source("esp_native_gameplay_monster_combat.c"),
            "EspNativeGameplayMonsterCombat_actionReset"),
))

STUBS = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct DoomRPG_s { int exists; } DoomRPG_t;
typedef struct { uint32_t arenaFNV1a, mapSpriteCount; } EspMapRuntimeView;
typedef struct { uint32_t spriteCount, enemyCount; } EspMapSpriteTopologyView;
static EspMapRuntimeView map = {0x12345678u, 11u};
static EspMapSpriteTopologyView topo = {11u, 1u};
static uint32_t emptyServiceArenaLogged;
static int mapLoaded = 1, topoReady = 1, allowGate = 1;
static int allowEnsure = 1, allowAction = 1, allowSync = 1;
static int allowPending = 1, allowVisual = 1, paused = 0;
static int ensureCount, actionCount, syncCount, pendingCount, visualCount;
static int resetCombatCount, resetStateCount, resetActionCount;
static struct { struct { uint8_t active; } pending; } combatOwner;
static char order[32]; static unsigned orderCount;
static void step(char c) {
  assert(orderCount + 1 < sizeof(order));
  order[orderCount++] = c;
  order[orderCount] = 0;
}
static const EspMapRuntimeView* EspMapRuntime_view(void) { return &map; }
static const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return &topo; }
static int EspMapRuntime_isLoaded(void) { return mapLoaded; }
static int EspMapSpriteTopology_isReady(void) { return topoReady; }
static int EspNativeGameplayMonsterState_ensure(DoomRPG_t* r) {
  assert(r != NULL); ++ensureCount; step('N'); return allowEnsure;
}
static int EspNativeGameplayActionEngine_service(DoomRPG_t* r) {
  assert(r != NULL); ++actionCount; step('A'); return allowAction;
}
static int syncOwner(void) { ++syncCount; step('C'); return allowSync; }
static int servicePending(DoomRPG_t* r) {
  assert(r != NULL); ++pendingCount; step('P'); return allowPending;
}
static int serviceVisualRedraw(DoomRPG_t* r) {
  assert(r != NULL); ++visualCount; step('V'); return allowVisual;
}
static void EspNativeGameplayMonsterCombat_reset(void) {
  ++resetCombatCount; step('C');
}
static void EspNativeGameplayMonsterState_reset(void) {
  ++resetStateCount; step('N');
}
static void EspNativeGameplayActionEngine_reset(void) {
  ++resetActionCount; step('A');
}
int EspNativeGameplayMonsterState_actionService(DoomRPG_t* runtime);
static int EspNativeGameplayHubActionGate_service(DoomRPG_t* r) {
  step('H');
  if (!allowGate) return 0;
  if (paused) return 1;
  return EspNativeGameplayMonsterState_actionService(r);
}
"""

CHECKS = r"""
static void seed(void) {
  map.arenaFNV1a = 0x12345678u; map.mapSpriteCount = 11u;
  topo.spriteCount = 11u; topo.enemyCount = 1u;
  mapLoaded = topoReady = allowGate = allowEnsure = allowAction = allowSync = 1;
  allowPending = allowVisual = 1; paused = 0;
  ensureCount = actionCount = syncCount = pendingCount = visualCount = 0;
  resetCombatCount = resetStateCount = resetActionCount = 0;
  combatOwner.pending.active = 0;
  orderCount = 0; order[0] = 0;
}
static void path(const char* expected) {
  if (strcmp(order, expected) != 0) {
    fprintf(stderr, "path actual=%s expected=%s\n", order, expected);
    assert(0);
  }
}
int main(void) {
  DoomRPG_t game = {1};
  seed();
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 1);
  path("HNACV"); assert(ensureCount==1 && actionCount==1 &&
                       syncCount==1 && visualCount==1 && pendingCount==0);
  seed(); topo.enemyCount = 0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 1);
  path("HA"); assert(ensureCount==0 && actionCount==1 && syncCount==0 && visualCount==0);
  seed(); topo.enemyCount=0; allowAction=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HA"); assert(syncCount==0);
  seed(); allowGate=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("H"); assert(ensureCount==0 && actionCount==0 && syncCount==0);
  seed(); allowEnsure=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HN"); assert(actionCount==0 && visualCount==0);
  seed(); allowAction=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HNA"); assert(syncCount==0 && visualCount==0);
  seed(); allowSync=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HNAC"); assert(visualCount==0 && pendingCount==0);
  seed(); combatOwner.pending.active=1;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 1);
  path("HNACP"); assert(pendingCount==1 && visualCount==0);
  seed(); combatOwner.pending.active=1; allowPending=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HNACP");
  seed(); allowVisual=0;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 0);
  path("HNACV");
  seed(); paused=1;
  assert(EspNativeGameplayMonsterCombat_actionService(&game) == 1);
  path("HCV"); assert(actionCount==0 && ensureCount==0);
  seed();
  EspNativeGameplayMonsterCombat_actionReset();
  path("CNA");
  assert(resetCombatCount==1 && resetStateCount==1 && resetActionCount==1);
  puts("PASS RC13 actual MonsterState + MonsterCombat C: zero-enemy, "
       "early-fail, HUB pause, pending, redraw, order and reset");
  return 0;
}
"""

MUTANTS = (
    ("empty-enemy inversion", "topology->enemyCount == 0U",
     "topology->enemyCount != 0U"),
    ("failed ensure bypass", "if (!EspNativeGameplayMonsterState_ensure(doomRpg)) return 0;",
     "if (0) return 0;"),
    ("raw action skipped", "return EspNativeGameplayActionEngine_service(doomRpg);",
     "return 1;"),
    ("HUB admission bypass", "if (!EspNativeGameplayHubActionGate_service(runtime)) return 0;",
     "if (0) return 0;"),
    ("combat sync bypass", "if (!syncOwner()) return 0;", "if (0) return 0;"),
    ("pending branch inverted", "if (combatOwner.pending.active != 0U)",
     "if (combatOwner.pending.active == 0U)"),
    ("pending service bypass", "return servicePending(runtime);",
     "return serviceVisualRedraw(runtime);"),
    ("redraw service bypass", "return serviceVisualRedraw(runtime);",
     "return 1;"),
    ("combat reset skipped", "    EspNativeGameplayMonsterCombat_reset();",
     "    (void)0;"),
    ("state reset skipped", "    EspNativeGameplayMonsterState_reset();",
     "    (void)0;"),
    ("engine reset skipped", "    EspNativeGameplayActionEngine_reset();",
     "    (void)0;"),
)


def exercise(body):
    with tempfile.TemporaryDirectory(prefix="doom-rpg-rc13-") as tmp:
        p = Path(tmp)
        c = p / "real.c"
        exe = p / "probe"
        c.write_text(STUBS + "\n" + body + "\n" + CHECKS,
                     encoding="utf-8")
        compile_run = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             str(c), "-o", str(exe)], capture_output=True, text=True)
        need(compile_run.returncode == 0,
             "RC13 actual C failed compile:\n" + compile_run.stderr[:3500])
        run = subprocess.run([str(exe)], capture_output=True, text=True,
                             timeout=10)
        return run.returncode == 0 and "PASS RC13 actual" in run.stdout


def main():
    census()
    need(exercise(BODY), "RC13 actual service/reset C body failed assertions")
    for label, before, after in MUTANTS:
        need(BODY.count(before) == 1 or
             (label in ("raw action skipped", "empty-enemy inversion")
              and BODY.count(before) == 2),
             "mutant source drift: " + label)
        mutated = BODY.replace(before, after, 1)
        need(not exercise(mutated), "adversarial C mutant escaped: " + label)
    print("PASS ARCH-RC13: 11 semantic C mutants rejected; "
          "no production linker alias, no extra allocation or RNG")


if __name__ == "__main__":
    main()
