#!/usr/bin/env python3
"""ARCH-RC8 source-backed real C lifecycle oracle and adversarial mutations.

Compile three actual production function bodies from the monster-turn and
existing session coordinator. Only called leaves are stubbed; no body model.
"""
from pathlib import Path
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
TURN = ESP / "src/esp_native_gameplay_monster_turn.c"
FX = ESP / "src/esp_native_gameplay_gib_fx.c"
INI = ESP / "platformio.ini"
HEADER = ESP / "include/esp_native_gameplay_monster_turn.h"

def require(ok, reason):
    if not ok:
        raise AssertionError(reason)

def extract(source, signature):
    require(source.count(signature) == 1, "missing/duplicate C signature: " + signature)
    start = source.index(signature)
    left = source.index("{", start)
    nesting = 0
    for right in range(left, len(source)):
        if source[right] == "{":
            nesting += 1
        if source[right] == "}":
            nesting -= 1
            if nesting == 0:
                return source[start:right + 1]
    raise AssertionError("unbalanced C body: " + signature)

PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct DoomRPG_s { int tag; } DoomRPG_t;
static char trace[1024];
static int activationOk = 1;
static unsigned int impactLoggedProbe, impactUnavailableProbe;
static unsigned char impactLoggedFrame;
static void mark(const char* t) {
    assert(strlen(trace) + strlen(t) + 1U < sizeof(trace));
    strcat(trace, t);
}
static void clearTrace(void) { trace[0] = 0; }
static void decoratePlayerImpact(void) {}
static void observeAndProbe(DoomRPG_t* d) { (void)d; mark("|observe"); }
static int EspNativeGameplayMonsterActivation_serviceTurn(void) {
    mark("|activation"); return activationOk;
}
static void EspNativeGameplayActionEngine_setImpactPainter(void (*p)(void)) {
    mark(p == NULL ? "|impact0" : "|impact1");
}
static void EspNativeGameplayPlayerResources_sessionService(struct DoomRPG_s* d) {
    (void)d; mark("|resources");
}
static void EspNativeGameplayMonsterAttackVisual_service(struct DoomRPG_s* d) {
    (void)d; mark("|attackVisual");
}
static void EspNativeGameplayActionEngine_beginMonsterVisualWork(void) {
    mark("|begin");
}
static void EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* d) {
    (void)d; mark("|retaliation");
}
static void EspNativeGameplayMonsterMovementProbe_service(struct DoomRPG_s* d) {
    (void)d; mark("|movement");
}
static void serviceHitExpiry(struct DoomRPG_s* d) { (void)d; mark("|hit"); }
static void servicePlayerPainShake(struct DoomRPG_s* d) { (void)d; mark("|shake"); }
static void servicePlayerPainExpiry(struct DoomRPG_s* d) { (void)d; mark("|pain"); }
static void serviceGibAnimation(struct DoomRPG_s* d) { (void)d; mark("|gib"); }
static void serviceExpiry(struct DoomRPG_s* d) { (void)d; mark("|expiry"); }
static void EspNativeGameplayTransitionHandoff_service(void) { mark("|handoff"); }
static void EspNativeGameplayActionEngine_endMonsterVisualWork(void) { mark("|end"); }
static void EspNativeGameplayMonsterTurn_reset(void) { mark("|turnReset"); }
static void EspNativeGameplayPlayerResources_sessionReset(void) { mark("|resReset"); }
static void EspNativeGameplayMonsterAttackVisual_reset(void) { mark("|visReset"); }
static void EspNativeGameplayMonsterRetaliation_reset(void) { mark("|retReset"); }
static void EspNativePlayerPainVisual_reset(void) { mark("|painReset"); }
static void EspNativeGameplayMonsterMovementProbe_reset(void) { mark("|moveReset"); }
static void resetFx(void) { mark("|fxReset"); }
"""
CHECKS = r"""
int main(void) {
    DoomRPG_t owner = {7};
    const char* expectedService =
        "|impact1|resources|observe|activation|attackVisual|begin"
        "|retaliation|movement|hit|shake|pain|gib|expiry|handoff|end";
    const char* expectedReset =
        "|impact0|turnReset|resReset|visReset|retReset|painReset|moveReset|fxReset";

    clearTrace();
    EspNativeGameplaySessionCoordinator_service(&owner);
    assert(strcmp(trace, expectedService) == 0);
    clearTrace();
    activationOk = 0; /* failed admission is not retried or reordered */
    EspNativeGameplaySessionCoordinator_service(&owner);
    assert(strcmp(trace, expectedService) == 0);
    clearTrace();
    EspNativeGameplaySessionCoordinator_service(NULL);
    assert(strcmp(trace, expectedService) == 0);
    clearTrace();
    impactLoggedProbe = 5U; impactLoggedFrame = 2U; impactUnavailableProbe = 6U;
    EspNativeGameplaySessionCoordinator_reset();
    assert(strcmp(trace, expectedReset) == 0);
    assert(impactLoggedProbe == 0U && impactLoggedFrame == 0xffU &&
           impactUnavailableProbe == 0U);
    clearTrace();
    EspNativeGameplaySessionCoordinator_reset();
    assert(strcmp(trace, expectedReset) == 0);
    puts("PASS ARCH-RC8 compiled real C session lifecycle, 5 scenarios");
    return 0;
}
"""

def build_and_run(code):
    with tempfile.TemporaryDirectory(prefix="arch-rc8-") as d:
        source = Path(d) / "oracle.c"
        binary = Path(d) / "oracle"
        source.write_text(code, encoding="utf-8")
        compiled = subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(source),
             "-o", str(binary)], capture_output=True, text=True)
        if compiled.returncode:
            return False, compiled.stderr[:400]
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        return run.returncode == 0, (run.stdout + run.stderr)[:400]

def main():
    turn = TURN.read_text(encoding="utf-8")
    fx = FX.read_text(encoding="utf-8")
    ini = INI.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    for name in ("EspNativeGameplayPlayerResources_sessionService",
                 "EspNativeGameplayPlayerResources_sessionReset"):
        require("-Wl,--wrap=" + name not in ini, "linker flag resurrected")
        require("__wrap_" + name + "(" not in turn, "old wrapper body resurrected")
        require("__real_" + name + "(" not in turn, "old alias resurrected")
    require("void EspNativeGameplayMonsterTurn_afterPlayerResourcesService(" in header,
            "missing permanent public hook")
    require(fx.count("EspNativeGameplayMonsterTurn_afterPlayerResourcesService(doomRpg);") == 1,
            "missing/duplicate explicit resource posthook")
    require(fx.count("EspNativeGameplayMonsterTurn_reset();") == 1,
            "missing/duplicate reset hook")

    hook = extract(turn,
        "void EspNativeGameplayMonsterTurn_afterPlayerResourcesService(")
    service = extract(fx, "void EspNativeGameplaySessionCoordinator_service(")
    reset = extract(fx, "void EspNativeGameplaySessionCoordinator_reset(")
    source = PREFIX + hook + "\n" + service + "\n" + reset + "\n" + CHECKS
    success, detail = build_and_run(source)
    require(success, "real C failed: " + detail)
    print(detail.strip())

    mutants = (
        ("missing-observation", "    observeAndProbe((DoomRPG_t*)doomRpg);\n", ""),
        ("skip-activation", "    (void)EspNativeGameplayMonsterActivation_serviceTurn();\n", ""),
        ("duplicate-activation", "    (void)EspNativeGameplayMonsterActivation_serviceTurn();\n",
          "    (void)EspNativeGameplayMonsterActivation_serviceTurn();\n" * 2),
        ("null-gated", "    observeAndProbe((DoomRPG_t*)doomRpg);\n",
          "    if (doomRpg) observeAndProbe((DoomRPG_t*)doomRpg);\n"),
        ("observer-before-resources",
          "    EspNativeGameplayPlayerResources_sessionService(doomRpg);\n"
          "    EspNativeGameplayMonsterTurn_afterPlayerResourcesService(doomRpg);\n",
          "    EspNativeGameplayMonsterTurn_afterPlayerResourcesService(doomRpg);\n"
          "    EspNativeGameplayPlayerResources_sessionService(doomRpg);\n"),
        ("no-reset", "    EspNativeGameplayMonsterTurn_reset();\n", ""),
        ("wrong-reset-order",
          "    EspNativeGameplayMonsterTurn_reset();\n"
          "    EspNativeGameplayPlayerResources_sessionReset();\n",
          "    EspNativeGameplayPlayerResources_sessionReset();\n"
          "    EspNativeGameplayMonsterTurn_reset();\n"),
        ("no-primary-pose", "    EspNativeGameplayMonsterAttackVisual_service(doomRpg);\n", ""),
        ("retaliation-after-movement",
          "    EspNativeGameplayMonsterRetaliation_service(doomRpg);\n"
          "    EspNativeGameplayMonsterMovementProbe_service(doomRpg);\n",
          "    EspNativeGameplayMonsterMovementProbe_service(doomRpg);\n"
          "    EspNativeGameplayMonsterRetaliation_service(doomRpg);\n"),
        ("handoff-deleted", "    EspNativeGameplayTransitionHandoff_service();\n", ""),
        ("end-deleted", "    EspNativeGameplayActionEngine_endMonsterVisualWork();\n", ""),
        ("fx-reset-deleted", "    resetFx();\n", ""),
    )
    for name, old, new in mutants:
        require(source.count(old) == 1, name + " lacks unique mutation target")
        mutation_ok, result = build_and_run(source.replace(old, new, 1))
        require(not mutation_ok, name + " escaped oracle: " + result)
    print("PASS ARCH-RC8: 12/12 adversarial mutants rejected; no invented hardware proof")

if __name__ == "__main__":
    main()
