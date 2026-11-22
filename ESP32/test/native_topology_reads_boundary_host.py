#!/usr/bin/env python3
"""ARCH-RC20: compile nine REAL native topology/composition C bodies.

Checks the entire raw -> ActionEngine -> ProjectionOverlay -> MonsterCombat
read path, with death, gib, pain, resource, crate, malformed and fail-stop
branches. All mutants compile from the same actual C bodies. This is a host
oracle; the classic CYD remains the final hardware authority.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
INI = ROOT / "platformio.ini"
FILES = {
    "raw": (SRC / "esp_map_sprite_topology.c").read_text(),
    "action": (SRC / "esp_native_gameplay_action_engine.c").read_text(),
    "projection": (SRC / "esp_native_gameplay_projection_overlay.c").read_text(),
    "combat": (SRC / "esp_native_gameplay_monster_combat.c").read_text(),
    "crate": (SRC / "esp_native_gameplay_crate_state.c").read_text(),
}
SIGS = (
    ("raw", "int EspMapSpriteTopology_getVisualStateBase("),
    ("raw", "int EspMapSpriteTopology_getEntityBase("),
    ("action", "static int actionGetEntity("),
    ("action", "int EspNativeGameplayActionEngine_getVisualState("),
    ("action", "int EspNativeGameplayActionEngine_getEntity("),
    ("projection", "int EspNativeGameplayProjectionOverlay_getVisualState("),
    ("projection", "int EspNativeGameplayProjectionOverlay_getEntity("),
    ("combat", "int EspMapSpriteTopology_getVisualState("),
    ("combat", "int EspMapSpriteTopology_getEntity("),
)


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def extract(source, signature):
    require(source.count(signature) == 1, "missing/ambiguous real C " + signature)
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[start:end + 1]
    raise AssertionError("unclosed real C " + signature)


PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ENTITY_SUBTYPE_MASK 0x1fU
#define ACTION_VISUAL_HIDDEN 0x80U
#define PROJECTION_VISUAL_HIDDEN 0x80U
#define MONSTER_VISUAL_HIDDEN 0x80U
#define MONSTER_CORPSE_VISUAL 2U
#define MONSTER_DEATH_VISUAL 4U
#define MONSTER_PAIN_VISUAL 6U
#define MONSTER_TYPE_ENEMY 1U
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U

typedef struct { uint32_t spriteCount; } EspMapSpriteTopologyView;
typedef struct { uint8_t alive; } EspNativeGameplayMonsterRecord;
static EspMapSpriteTopologyView topologyView;
static uint8_t visualStates[4], entityTypes[4], entitySubTypes[4];
static uint16_t linkValues[4], orderValues[4];
static EspNativeGameplayMonsterRecord monster;
static struct { struct { uint16_t painSpriteIndex; } view; } combatOwner;
static char calls[128];
static size_t ncall;
static int rawReady, syncOk, monsterPresent, removedFlag, consumedFlag;
static int crateFailsAt, crateCalls, crateTransformed, gibFlag, corpseFlag;
static int attackFails;
static void mark(char c) {
    assert(ncall + 1U < sizeof(calls));
    calls[ncall++] = c; calls[ncall] = '\0';
}
static void reset(void) {
    memset(calls, 0, sizeof(calls)); ncall = 0U;
    memset(visualStates, 0, sizeof(visualStates));
    memset(entityTypes, 0, sizeof(entityTypes));
    memset(entitySubTypes, 0, sizeof(entitySubTypes));
    memset(linkValues, 0, sizeof(linkValues));
    memset(orderValues, 0, sizeof(orderValues));
    topologyView.spriteCount = 4U;
    rawReady = syncOk = monsterPresent = 1;
    removedFlag = consumedFlag = crateFailsAt = crateCalls = 0;
    crateTransformed = gibFlag = corpseFlag = attackFails = 0;
    monster.alive = 1U; combatOwner.view.painSpriteIndex = UINT16_MAX;
    visualStates[2] = 0x30U; entityTypes[2] = 1U;
    entitySubTypes[2] = 0x82U; linkValues[2] = 0x341aU;
    orderValues[2] = 7U;
}
int EspMapSpriteTopology_isReady(void) { mark('R'); return rawReady; }
static uint16_t linkStateAt(uint32_t i) { mark('L'); return linkValues[i]; }
static uint16_t linkOrderAt(uint32_t i) { mark('O'); return orderValues[i]; }
static int removed(uint32_t i) { assert(i == 2U); mark('M'); return removedFlag; }
int EspNativeGameplayCrateState_applyEntity(
    uint32_t i, uint8_t* t, uint8_t* s, uint16_t* l) {
    assert(i == 2U); mark('C'); ++crateCalls;
    if (crateTransformed) { *t = 12U; *s = 4U; *l |= 0x80U; }
    return crateFailsAt != crateCalls;
}
int EspNativeGameplayPlayerResources_isConsumed(uint32_t i) {
    assert(i == 2U); mark('P'); return consumedFlag;
}
int EspNativeGameplayMonsterAttackVisual_apply(uint32_t i, uint8_t* visual) {
    assert(i == 2U && visual); mark('A'); *visual |= 5U;
    return !attackFails;
}
static int syncOwner(void) { mark('S'); return syncOk; }
static const EspNativeGameplayMonsterRecord*
EspNativeGameplayMonsterState_find(uint16_t i) {
    assert(i == 2U); mark('F'); return monsterPresent ? &monster : NULL;
}
static int gibbed(uint16_t i) {
    assert(i == 2U); mark('G'); return gibFlag;
}
static int corpseReady(uint16_t i) {
    assert(i == 2U); mark('D'); return corpseFlag;
}
int EspMapSpriteTopology_getVisualStateBase(uint32_t, uint8_t*);
int EspMapSpriteTopology_getEntityBase(uint32_t, uint8_t*, uint8_t*,
                                       uint16_t*, uint16_t*);
int EspNativeGameplayActionEngine_getVisualState(uint32_t, uint8_t*);
int EspNativeGameplayActionEngine_getEntity(uint32_t, uint8_t*, uint8_t*,
                                           uint16_t*, uint16_t*);
int EspNativeGameplayProjectionOverlay_getVisualState(uint32_t, uint8_t*);
int EspNativeGameplayProjectionOverlay_getEntity(uint32_t, uint8_t*, uint8_t*,
                                                 uint16_t*, uint16_t*);
int EspMapSpriteTopology_getVisualState(uint32_t, uint8_t*);
int EspMapSpriteTopology_getEntity(uint32_t, uint8_t*, uint8_t*,
                                  uint16_t*, uint16_t*);
__attribute__((unused)) int NoAttack(uint32_t i, uint8_t* visual) {
    (void)i; (void)visual; return 1;
}
__attribute__((unused)) int SkipCrate(uint32_t i, uint8_t* t,
                                      uint8_t* s, uint16_t* l) {
    (void)i; (void)t; (void)s; (void)l; return 1;
}
"""

CHECKS = r"""
static void expect(const char* trace) { assert(strcmp(calls, trace) == 0); }
int main(void) {
    uint8_t visual, type, sub;
    uint16_t link, order;

    reset(); visual = 0U;
    assert(EspMapSpriteTopology_getVisualStateBase(2U, &visual));
    assert(visual == 0x30U); expect("R");
    reset(); type = sub = 0U; link = order = 0U;
    assert(EspMapSpriteTopology_getEntityBase(2U, &type, &sub, &link, &order));
    assert(type == 1U && sub == 2U && link == 0x341aU && order == 7U);
    expect("RLO");

    reset(); visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x35U); expect("RMPASF");
    reset(); removedFlag = 1; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0xb5U); expect("RMPASF");
    reset(); consumedFlag = 1; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0xb5U); expect("RMPASF");
    reset(); combatOwner.view.painSpriteIndex = 2U; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x36U); expect("RMPASF");

    reset(); monster.alive = 0U; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x34U); expect("RMPASFGD");
    reset(); monster.alive = 0U; corpseFlag = 1; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x32U); expect("RMPASFGD");
    reset(); monster.alive = 0U; gibFlag = 1; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0xb5U); expect("RMPASFG");
    reset(); monsterPresent = 0; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x35U); expect("RMPASF");
    reset(); syncOk = 0; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x35U); expect("RMPAS");
    reset(); attackFails = 1; visual = 0U;
    assert(EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x35U); expect("RMPASF");

    reset(); type = sub = 0U; link = order = 0U;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(type == 1U && sub == 2U && link == 0x341aU && order == 7U);
    assert(crateCalls == 2); expect("RLOMCCPSF");
    reset(); removedFlag = 1;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(link == 0x201aU && order == 7U); expect("RLOMCCPSF");
    reset(); consumedFlag = 1;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(link == 0x201aU && order == 0U); expect("RLOMCCPSF");
    reset(); monster.alive = 0U;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(link == 0x201aU && order == 0U); expect("RLOMCCPSF");
    reset(); crateTransformed = 1; consumedFlag = 1;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(type == 12U && sub == 4U && link == 0x209aU && order == 0U);
    assert(crateCalls == 2); expect("RLOMCCP");
    reset(); monsterPresent = 0;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(link == 0x341aU && order == 7U); expect("RLOMCCPSF");
    reset(); syncOk = 0;
    assert(EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(link == 0x341aU && order == 7U); expect("RLOMCCPS");

    reset(); rawReady = 0; visual = 0x55U;
    assert(!EspMapSpriteTopology_getVisualState(2U, &visual));
    assert(visual == 0x55U); expect("R");
    reset(); rawReady = 0;
    type = sub = 55U; link = order = 55U;
    assert(!EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(type == 55U && sub == 55U && link == 55U && order == 55U);
    expect("R");
    reset(); visual = 0x55U;
    assert(!EspMapSpriteTopology_getVisualState(4U, &visual));
    assert(visual == 0x55U); expect("R");
    reset();
    assert(!EspMapSpriteTopology_getVisualState(2U, NULL)); expect("R");
    reset();
    assert(!EspMapSpriteTopology_getEntity(2U, NULL, &sub, &link, &order));
    expect("R");

    reset(); crateFailsAt = 1;
    assert(!EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(crateCalls == 1); expect("RLOMC");
    reset(); crateFailsAt = 2;
    assert(!EspMapSpriteTopology_getEntity(2U, &type, &sub, &link, &order));
    assert(crateCalls == 2); expect("RLOMCC");

    puts("PASS RC20 nine real C getters: canonical + composed layers, 26 cases");
    return 0;
}
"""


def source_scan():
    ini = INI.read_text()
    base = ini.split("[env:esp32-cyd]", 1)[1].split(
        "[env:esp-cyd-inverted]", 1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=", base, re.M)) == 0,
            "RC24 must retire all production GNU wrappers")
    require("-Wl,--wrap=EspNativeGameplayFrame_renderTurn" not in base,
            "RC23 door transaction must not use linker interposition")
    for name in ("EspMapSpriteTopology_getEntity",
                 "EspMapSpriteTopology_getVisualState"):
        require("-Wl,--wrap=" + name not in ini,
                "RC20 resurrected topology GNU flag")
        for filename, source in FILES.items():
            require(not re.search(r"\b__(?:real|wrap)_" + name + r"\b", source),
                    "obsolete GNU alias in " + filename)
    require("EspMapSpriteTopology_getEntityBase(" in FILES["action"] and
            "EspMapSpriteTopology_getVisualStateBase(" in FILES["action"],
            "ActionEngine must read the raw topology leaves")
    require("EspMapSpriteTopology_getEntityBase(" in FILES["crate"] and
            "EspMapSpriteTopology_getEntity(" not in FILES["crate"],
            "CrateState lazy ensure must read raw Base to avoid recursive projection")
    require("EspNativeGameplayProjectionOverlay_getEntity(" in FILES["combat"] and
            "EspNativeGameplayProjectionOverlay_getVisualState(" in FILES["combat"],
            "public owner lost complete projection")
    require("#define __wrap_EspMapSpriteTopology" not in (
                ROOT / "include/esp_native_gameplay_action_engine.h").read_text(),
            "old linker token macros returned")
    print("PASS RC20 source inventory: 4+3 GNU wraps, no topology aliases")


def compose(bodies):
    return "\n\n".join((PREFIX, *bodies, CHECKS))


def execute(bodies, expected):
    compiler = shutil.which("cc") or shutil.which("gcc")
    require(compiler is not None, "C11 host compiler missing")
    with tempfile.TemporaryDirectory(prefix="arch-rc20-topology-") as temp:
        file = Path(temp) / "chain.c"
        exe = Path(temp) / "chain"
        file.write_text(compose(bodies))
        built = subprocess.run(
            [compiler, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-function", str(file), "-o", str(exe)],
            text=True, capture_output=True)
        require(built.returncode == 0, "invalid real-body fixture: " + built.stderr)
        outcome = subprocess.run([str(exe)], capture_output=True, text=True)
        require((outcome.returncode == 0) == expected,
                "topology chain " + ("mutant passed" if not expected else "real failed")
                + ": " + outcome.stdout + outcome.stderr)


def mutant(body, original, changed):
    require(body.count(original) == 1,
            "mutant anchor drifted: " + original)
    return body.replace(original, changed, 1)


def main():
    source_scan()
    bodies = [extract(FILES[owner], signature) for owner, signature in SIGS]
    execute(bodies, True)
    changes = (
        (0, "visualStates[spriteIndex]", "visualStates[0U]"),
        (1, "ENTITY_SUBTYPE_MASK", "0x01U"),
        (1, "*outLinkOrder = linkOrderAt(spriteIndex);",
         "*outLinkOrder = 0U;"),
        (2, "removed(spriteIndex)", "0"),
        (3, "ACTION_VISUAL_HIDDEN", "0x40U"),
        (5, "PROJECTION_VISUAL_HIDDEN", "0x40U"),
        (5, "EspNativeGameplayMonsterAttackVisual_apply(",
         "NoAttack("),
        (6, "EspNativeGameplayCrateState_applyEntity(",
         "SkipCrate("),
        (6, "*outLinkOrder = 0U;", "*outLinkOrder = 7U;"),
        (7, "MONSTER_CORPSE_VISUAL", "MONSTER_DEATH_VISUAL"),
        (7, "*outVisualState |= MONSTER_VISUAL_HIDDEN;",
         "*outVisualState |= 0x40U;"),
        (7, "MONSTER_PAIN_VISUAL", "MONSTER_CORPSE_VISUAL"),
        (8, "*outLinkOrder = 0U;", "*outLinkOrder = 7U;"),
        (8, "~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |",
         "~(0U |"),
    )
    for i, (body_index, original, changed) in enumerate(changes):
        altered = list(bodies)
        altered[body_index] = mutant(altered[body_index], original, changed)
        execute(altered, False)
    print("PASS RC20 9 real C bodies, 26+ cases, "
          + str(len(changes)) + " adversarial mutants rejected")


if __name__ == "__main__":
    main()
