#!/usr/bin/env python3
"""ARCH-RC7: source-backed projection read chain with adversarial C fixtures.

Compiles the exact ActionEngine + ProjectionOverlay functions from production
sources. Tests raw -> removed -> crate -> consumed -> pose ordering, failure
short-circuit, and stale / malformed reads. No CYD hardware claim.
"""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
ACTION = ESP / "src/esp_native_gameplay_action_engine.c"
PROJECTION = ESP / "src/esp_native_gameplay_projection_overlay.c"
PUBLIC = ESP / "include/esp_native_gameplay_projection_overlay.h"
INI = ESP / "platformio.ini"
COMBAT = ESP / "src/esp_native_gameplay_monster_combat.c"
TRACE = ESP / "src/esp_native_gameplay_monster_trace.c"

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)

def extract(source: str, signature: str) -> str:
    require(source.count(signature) == 1, "missing/duplicate real body " + signature)
    begin = source.index(signature)
    left = source.find("{", begin)
    require(left >= 0, "no body for " + signature)
    depth = 0
    for end in range(left, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:end + 1]
    raise AssertionError("unclosed real C body " + signature)

PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ACTION_VISUAL_HIDDEN 0x80U
#define PROJECTION_VISUAL_HIDDEN 0x80U
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U

static char calls[64];
static unsigned int ncall;
static int rawReady, isRemoved, isConsumed, attackFails, crateFailsAt;
static int crateCalls, crateTransformed;
static uint8_t initialVisual, rawType, rawSubtype;
static uint16_t rawLink, rawOrder;
static void mark(char c) {
    assert(ncall + 1 < sizeof(calls));
    calls[ncall++] = c;
    calls[ncall] = '\0';
}
static void reset(void) {
    memset(calls, 0, sizeof(calls));
    ncall = 0;
    rawReady = 1; isRemoved = 0; isConsumed = 0;
    attackFails = 0; crateFailsAt = 0; crateCalls = 0;
    crateTransformed = 0; initialVisual = 0x30U;
    rawType = 1U; rawSubtype = 2U; rawLink = 0x341aU; rawOrder = 7U;
}
int EspMapSpriteTopology_getVisualStateBase(uint32_t i, uint8_t* out) {
    mark('V');
    if (!rawReady || i != 2U || !out) return 0;
    *out = initialVisual;
    return 1;
}
int EspMapSpriteTopology_getEntityBase(uint32_t i, uint8_t* type,
                                           uint8_t* subtype, uint16_t* link,
                                           uint16_t* order) {
    mark('B');
    if (!rawReady || i != 2U || !type || !subtype || !link || !order) return 0;
    *type = rawType; *subtype = rawSubtype;
    *link = rawLink; *order = rawOrder;
    return 1;
}
static int removed(uint32_t i) {
    mark('M');
    assert(i == 2U);
    return isRemoved;
}
int EspNativeGameplayCrateState_applyEntity(uint32_t i, uint8_t* type,
                                             uint8_t* sub, uint16_t* link) {
    mark('C');
    assert(i == 2U);
    ++crateCalls;
    if (crateTransformed) {
        *type = 12U; *sub = 4U;
        *link |= 0x0080U;
    }
    return crateFailsAt != crateCalls;
}
int EspNativeGameplayPlayerResources_isConsumed(uint32_t i) {
    mark('R');
    assert(i == 2U);
    return isConsumed;
}
int EspNativeGameplayMonsterAttackVisual_apply(uint32_t i, uint8_t* visual) {
    mark('A');
    assert(i == 2U && visual != NULL);
    *visual |= 0x06U;
    return !attackFails;
}
__attribute__((unused)) int NeverConsumed(uint32_t i) {
    (void)i;
    return 0;
}
__attribute__((unused)) int NoAttack(uint32_t i, uint8_t* v) {
    (void)i; (void)v;
    return 1;
}
__attribute__((unused)) int SkipCrate(uint32_t i, uint8_t* type,
                                      uint8_t* sub, uint16_t* link) {
    (void)i; (void)type; (void)sub; (void)link;
    return 1;
}
__attribute__((unused)) int FakeVisualBase(uint32_t i, uint8_t* v) {
    (void)i; (void)v;
    return 1;
}
__attribute__((unused)) int FakeEntityBase(uint32_t i, uint8_t* t,
                                           uint8_t* s, uint16_t* l,
                                           uint16_t* o) {
    (void)i; (void)t; (void)s; (void)l; (void)o;
    return 1;
}
int EspNativeGameplayActionEngine_getVisualState(uint32_t, uint8_t*);
int EspNativeGameplayActionEngine_getEntity(uint32_t, uint8_t*, uint8_t*,
                                             uint16_t*, uint16_t*);
"""

POST = r"""
int main(void) {
    uint8_t v, t, s;
    uint16_t l, o;
    reset(); v = 0U;
    assert(EspNativeGameplayProjectionOverlay_getVisualState(2U, &v));
    assert(v == 0x36U && strcmp(calls, "VMRA") == 0);

    reset(); isRemoved = 1; v = 0U;
    assert(EspNativeGameplayProjectionOverlay_getVisualState(2U, &v));
    assert(v == 0xb6U && strcmp(calls, "VMRA") == 0);

    reset(); isConsumed = 1; v = 0U;
    assert(EspNativeGameplayProjectionOverlay_getVisualState(2U, &v));
    assert(v == 0xb6U && strcmp(calls, "VMRA") == 0);

    reset(); rawReady = 0; v = 0x55U;
    assert(!EspNativeGameplayProjectionOverlay_getVisualState(2U, &v));
    assert(v == 0x55U && strcmp(calls, "V") == 0);

    reset(); v = 0x55U;
    assert(!EspNativeGameplayProjectionOverlay_getVisualState(3U, &v));
    assert(v == 0x55U && strcmp(calls, "V") == 0);

    reset();
    assert(!EspNativeGameplayProjectionOverlay_getVisualState(2U, NULL));
    assert(strcmp(calls, "V") == 0);

    reset(); attackFails = 1; v = 0U;
    assert(EspNativeGameplayProjectionOverlay_getVisualState(2U, &v));
    assert(v == 0x36U && strcmp(calls, "VMRA") == 0);

    reset(); t = 0U; s = 0U; l = 0U; o = 0U;
    assert(EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(t == 1U && s == 2U && l == 0x341aU && o == 7U);
    assert(crateCalls == 2 && strcmp(calls, "BMCCR") == 0);

    reset(); isRemoved = 1; l = 0U;
    assert(EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(l == 0x201aU && o == 7U);
    assert(crateCalls == 2 && strcmp(calls, "BMCCR") == 0);

    reset(); isConsumed = 1; l = 0U; o = 99U;
    assert(EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(l == 0x201aU && o == 0U && strcmp(calls, "BMCCR") == 0);

    reset(); crateTransformed = 1; isConsumed = 1;
    assert(EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(t == 12U && s == 4U && l == 0x209aU && o == 0U);
    assert(crateCalls == 2 && strcmp(calls, "BMCCR") == 0);

    reset(); rawReady = 0; t = 55U; s = 55U; l = 55U; o = 55U;
    assert(!EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(t == 55U && s == 55U && l == 55U && o == 55U);
    assert(strcmp(calls, "B") == 0);

    reset(); t = 55U;
    assert(!EspNativeGameplayProjectionOverlay_getEntity(3U, &t, &s, &l, &o));
    assert(t == 55U && strcmp(calls, "B") == 0);

    reset();
    assert(!EspNativeGameplayProjectionOverlay_getEntity(2U, NULL, &s, &l, &o));
    assert(strcmp(calls, "B") == 0);

    reset(); crateFailsAt = 1; t = 0U;
    assert(!EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(crateCalls == 1 && strcmp(calls, "BMC") == 0);

    reset(); crateFailsAt = 2; t = 0U;
    assert(!EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(crateCalls == 2 && strcmp(calls, "BMCC") == 0);

    /* The two crate applications are deliberate and must not collapse. */
    reset(); crateFailsAt = 2; crateTransformed = 1; t = 0U;
    assert(!EspNativeGameplayProjectionOverlay_getEntity(2U, &t, &s, &l, &o));
    assert(t == 12U && s == 4U && l == 0x349aU);
    assert(crateCalls == 2 && strcmp(calls, "BMCC") == 0);
    return 0;
}
"""

def compose(parts: list[str]) -> str:
    return "\n\n".join([PREFIX, *parts, POST])

def run(source: str, expected: bool) -> None:
    compiler = shutil.which("cc") or shutil.which("gcc")
    require(compiler is not None, "host C compiler absent")
    with tempfile.TemporaryDirectory(prefix="arch-rc7-projection-") as tmp:
        path = Path(tmp) / "projection.c"
        exe = Path(tmp) / "projection"
        path.write_text(source)
        built = subprocess.run(
            [compiler, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-function", str(path), "-o", str(exe)],
            text=True, capture_output=True)
        require(built.returncode == 0, "invalid oracle/mutant C: " + built.stderr)
        finished = subprocess.run([str(exe)], capture_output=True, text=True)
        require((finished.returncode == 0) == expected,
                "fixture unexpectedly " + ("passed" if expected is False else "failed")
                + ": " + finished.stderr)

def mutate(body: str, before: str, after: str) -> str:
    require(body.count(before) == 1, "unapplied/ambiguous mutation: " + before)
    return body.replace(before, after, 1)

def main() -> None:
    action = ACTION.read_text()
    projection = PROJECTION.read_text()
    header = PUBLIC.read_text()
    combat = COMBAT.read_text()
    trace = TRACE.read_text()
    ini = INI.read_text()
    retired = ("EspNativeGameplayActionEngine_getVisualState",
               "EspNativeGameplayActionEngine_getEntity")
    for name in retired:
        require("-Wl,--wrap=" + name not in ini, "retired flag reinstated " + name)
        require("__wrap_" + name not in projection and
                "__real_" + name not in projection,
                "retired projection link alias reinstated " + name)
        require("int EspNativeGameplayProjectionOverlay_get" +
                name.split("_get")[1] + "(" in header,
                "missing public explicit projected getter declaration " + name)
    require("EspNativeGameplayProjectionOverlay_getVisualState(" in combat and
            "EspNativeGameplayProjectionOverlay_getEntity(" in combat,
            "combat no longer consumes the complete projection")
    require("EspNativeGameplayProjectionOverlay_getEntity(" in trace,
            "monster trace bypasses the complete projected entity")
    require("EspNativeGameplayActionEngine_getEntity(" not in trace,
            "old implicit monster-trace interposition revived")
    require("EspNativeGameplayActionEngine_getVisualState(" not in combat and
            "EspNativeGameplayActionEngine_getEntity(" not in combat,
            "old implicit combat interposition revived")
    require("int EspNativeGameplayActionEngine_getVisualState(" in action and
            "int EspNativeGameplayActionEngine_getEntity(" in action,
            "ActionEngine private public-read layer was lost")
    for name in ("EspMapSpriteTopology_getVisualState",
                 "EspMapSpriteTopology_getEntity"):
        require("-Wl,--wrap=" + name not in ini,
                "RC20 topology GNU wrapper resurrected: " + name)
        for source in (action, combat):
            require("__wrap_" + name not in source and
                    "__real_" + name not in source,
                    "RC20 topology GNU alias resurrected: " + name)
    require("EspMapSpriteTopology_getVisualStateBase(" in action and
            "EspMapSpriteTopology_getEntityBase(" in action,
            "RC20 ActionEngine must read canonical Base leaves")

    a_entity_leaf = extract(action, "static int actionGetEntity(")
    a_visual = extract(action, "int EspNativeGameplayActionEngine_getVisualState(")
    a_entity = extract(action, "int EspNativeGameplayActionEngine_getEntity(")
    p_visual = extract(projection,
                       "int EspNativeGameplayProjectionOverlay_getVisualState(")
    p_entity = extract(projection,
                       "int EspNativeGameplayProjectionOverlay_getEntity(")
    original = [a_entity_leaf, a_visual, a_entity, p_visual, p_entity]
    run(compose(original), True)

    mutants = [
        (1, "ACTION_VISUAL_HIDDEN", "0x40U"),
        (1, "return 0;", "return 1;"),
        (3, "PROJECTION_VISUAL_HIDDEN", "0x40U"),
        (3, "EspNativeGameplayPlayerResources_isConsumed(", "NeverConsumed("),
        (3, "EspNativeGameplayMonsterAttackVisual_apply(", "NoAttack("),
        (3, "EspNativeGameplayActionEngine_getVisualState(", "FakeVisualBase("),
        (0, "EspNativeGameplayCrateState_applyEntity(", "SkipCrate("),
        (4, "EspNativeGameplayCrateState_applyEntity(", "SkipCrate("),
        (4, "EspNativeGameplayPlayerResources_isConsumed(", "NeverConsumed("),
        (4, "ESP_MAP_SPRITE_TOPOLOGY_ALIVE", "0U"),
        (4, "*outLinkOrder = 0U;", "*outLinkOrder = 7U;"),
        (4, "return 0;\n    }\n    if (!EspNativeGameplayCrateState_applyEntity(",
         "return 1;\n    }\n    if (!EspNativeGameplayCrateState_applyEntity("),
        (4, "EspNativeGameplayActionEngine_getEntity(", "FakeEntityBase("),
    ]
    for i, before, after in mutants:
        changed = list(original)
        changed[i] = mutate(changed[i], before, after)
        run(compose(changed), False)
    print("PASS ARCH-RC7 composed real-C 5-body oracle: raw/action/projection, "
          "17 checks, 13 adversarial mutants, no implicit projection --wrap")

if __name__ == "__main__":
    main()
