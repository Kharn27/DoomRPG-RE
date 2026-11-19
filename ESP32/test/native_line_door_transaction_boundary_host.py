#!/usr/bin/env python3
"""ARCH-RC15: real-C native line mutation/animation/line-death composition.

Compile the FIVE original production C bodies, not Python reimplementations:
raw line-command apply, public door animation, destructible-turn execute and
rollback (plus its real flush). Negative fixture mutates 12 exact contract
expressions and requires every mutated implementation to fail its same oracle.
No hardware/SD/map-specific claim: CYD Serial remains the final witness.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
RETIRED = (
    "EspMapLineState_applyDoorCommand",
    "EspNativeGameplayDestructible_executeLineDeath",
    "EspNativeGameplayDestructible_rollbackLineDeath",
)
SOURCES = {
    "line": (SRC / "esp_map_line_state.c").read_text(),
    "door": (SRC / "esp_native_door_animator.c").read_text(),
    "turn": (SRC / "esp_native_gameplay_destructible_turn_bridge.c").read_text(),
    "death": (SRC / "esp_native_gameplay_destructible.c").read_text(),
}
BODIES = (
    ("line", "EspMapLineDoorStatus EspMapLineState_applyDoorCommandBase("),
    ("door", "EspMapLineDoorStatus EspMapLineState_applyDoorCommand("),
    ("turn", "EspNativeGameplayDestructibleStatus\\nEspNativeGameplayDestructible_executeLineDeath("),
    ("turn", "int EspNativeGameplayDestructible_rollbackLineDeath("),
    ("turn", "int EspNativeGameplayDestructibleTurn_flush("),
)


def require(ok, msg):
    if not ok:
        raise AssertionError(msg)


def strip_comments(s):
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", s)


def extract(source, signature):
    signature = signature.replace("\\n", "\n")
    require(source.count(signature) == 1, "unique live C body missing: " + signature)
    start = source.index(signature)
    open_brace = source.index("{", start)
    depth = 0
    for i in range(open_brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError("unbalanced real C body " + signature)


def source_scan():
    all_src = {p.name: strip_comments(p.read_text())
               for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(all_src) >= 178, "unexpectedly small production corpus")
    config = (ROOT / "platformio.ini").read_text()
    normal = config.split("[env:esp32-cyd]", 1)[1].split("[env:esp-cyd-inverted]", 1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=", normal, re.M)) == 8,
            "expected 8 production wrappers after RC19")
    for symbol in RETIRED:
        require("-Wl,--wrap=" + symbol not in config, "retired flag " + symbol)
        for name, text in all_src.items():
            require(not re.search(r"\b__(?:wrap|real)_" + symbol + r"\b", text),
                    "GNU alias survives in " + name)
    for name in ("EspNativeGameplayFrame_renderTurn", "Esp32PlatformVideo_present"):
        require("-Wl,--wrap=" + name in config, "protected wrapper lost: " + name)
    require("EspMapLineState_applyDoorCommand(" not in all_src["esp_map_line_state.c"],
            "core door leaf must not bypass public animation")
    require("EspNativeGameplayDestructible_executeLineDeath(" not in
            all_src["esp_native_gameplay_destructible.c"],
            "raw death must not own turn bridge")
    require("EspNativeGameplayDestructible_rollbackLineDeath(" not in
            all_src["esp_native_gameplay_destructible.c"],
            "raw rollback must not own turn bridge")
    require("EspMapLineState_applyDoorCommandBase(&descriptor, commandOffset, &door)" in
            all_src["esp_native_gameplay_destructible.c"],
            "jammed door must bypass regular animation")
    require("EspMapLineState_applyDoorCommandBase(descriptor," in
            all_src["esp_native_door_animator.c"],
            "regular door must call raw line leaf")
    for owner, signature in BODIES:
        extract(SOURCES[owner], signature)
    print("PASS RC15 source: 178+ TUs, 3 retired GNU interpositions, "
          "door/line-death public vs raw boundaries explicit")


PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DESTRUCTIBLE_TURN_SEQ_TAG 0xd0000000UL
typedef struct { int marker; } EspMapEventDescriptor;
typedef enum { ESP_MAP_LINE_DOOR_OK=1, ESP_MAP_LINE_DOOR_INVALID=2,
               ESP_MAP_LINE_DOOR_NOT_READY=3 } EspMapLineDoorStatus;
typedef struct {
  uint16_t lineIndex;
  uint8_t openBefore, openAfter, mutated;
  uint32_t effectFlags, soundId;
} EspMapLineDoorResult;
typedef enum { ESP_NATIVE_DOOR_ANIMATION_OK=1,
               ESP_NATIVE_DOOR_ANIMATION_INVALID=2 } EspNativeDoorAnimationStatus;
typedef enum { ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK=1,
               ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT=2 }
               EspNativeGameplayDestructibleStatus;
typedef struct {
  uint16_t lineIndex;
  uint8_t mutated, rollbackAvailable;
} EspNativeGameplayDestructibleResult;
typedef struct {
  uint16_t eventTile, lineIndex;
  uint32_t armed, cancelled, requested;
  uint8_t pending, reserved[3];
} DestructibleTurnBridge;
static DestructibleTurnBridge destructibleTurn;
static uint8_t stateOpen;
static int rawCalls, setCalls, beginCalls, clearCalls, deathCalls, rollbackCalls;
static int requestCalls;
static uint32_t requestedSequence;
static int worldActive, failSetOnCall, allowTurnRequest=1;
static EspMapLineDoorStatus rawStatus=ESP_MAP_LINE_DOOR_OK;
static EspNativeDoorAnimationStatus animationResult=ESP_NATIVE_DOOR_ANIMATION_OK;
static EspNativeGameplayDestructibleStatus deathStatus=ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK;
static int deathMutated=1, deathRollback=1, rollbackOk=1;

static EspMapLineDoorStatus prepareDoorCommand(
  const EspMapEventDescriptor* desc, uint32_t offset,
  EspMapLineDoorResult* out, uint8_t* target) {
  ++rawCalls;
  if (desc==NULL || out==NULL || target==NULL || offset!=7)
    return ESP_MAP_LINE_DOOR_INVALID;
  if (rawStatus != ESP_MAP_LINE_DOOR_OK) return rawStatus;
  memset(out,0,sizeof(*out));
  out->lineIndex=42; out->openBefore=stateOpen;
  out->openAfter=(uint8_t)(!stateOpen);
  out->effectFlags=7; out->soundId=99;
  *target=out->openAfter;
  return ESP_MAP_LINE_DOOR_OK;
}
static int EspMapLineState_setOpen(uint16_t index,uint8_t value) {
  assert(index==42);
  ++setCalls;
  if (setCalls == failSetOnCall) return 0;
  stateOpen=value; return 1;
}
static int EspNativeResidentGameplay_isActive(void) { return worldActive; }
static EspNativeDoorAnimationStatus EspNativeDoorAnimator_begin(
  uint16_t idx,uint8_t before,uint8_t after) {
  assert(idx==42 && before!=after);
  ++beginCalls;
  return animationResult;
}
static void clearBatchPreserveCounters(void) { ++clearCalls; }
static EspNativeGameplayDestructibleStatus
EspNativeGameplayDestructible_executeLineDeathBase(
  uint16_t tile,uint16_t line,EspNativeGameplayDestructibleResult* out) {
  assert(tile==123 && line==42);
  ++deathCalls;
  if (out) { out->lineIndex=line; out->mutated=deathMutated;
             out->rollbackAvailable=deathRollback; }
  return deathStatus;
}
static int EspNativeGameplayDestructible_rollbackLineDeathBase(
  const EspNativeGameplayDestructibleResult* r) {
  ++rollbackCalls; (void)r; return rollbackOk;
}
static int EspNativeGameplayMonsterTurn_requestPassTurn(uint32_t seq) {
  ++requestCalls; requestedSequence=seq; return allowTurnRequest;
}
EspMapLineDoorStatus EspMapLineState_applyDoorCommandBase(
  const EspMapEventDescriptor*,uint32_t,EspMapLineDoorResult*);
EspNativeGameplayDestructibleStatus EspNativeGameplayDestructible_executeLineDeath(
  uint16_t,uint16_t,EspNativeGameplayDestructibleResult*);
int EspNativeGameplayDestructible_rollbackLineDeath(
  const EspNativeGameplayDestructibleResult*);
static void reset(void) {
  memset(&destructibleTurn,0,sizeof(destructibleTurn));
  stateOpen=0; rawCalls=setCalls=beginCalls=clearCalls=0;
  deathCalls=rollbackCalls=requestCalls=0; requestedSequence=0;
  worldActive=0; failSetOnCall=0; allowTurnRequest=1;
  rawStatus=ESP_MAP_LINE_DOOR_OK; animationResult=ESP_NATIVE_DOOR_ANIMATION_OK;
  deathStatus=ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK; deathMutated=1;
  deathRollback=1; rollbackOk=1;
}
"""

CHECKS = r"""
int main(void) {
  EspMapEventDescriptor descriptor={0};
  EspMapLineDoorResult door;
  EspNativeGameplayDestructibleResult result={0};
  reset();
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_OK);
  assert(stateOpen==1 && door.mutated==1 && rawCalls==1 &&
         setCalls==1 && beginCalls==0);

  reset(); worldActive=1;
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_OK);
  assert(stateOpen==1 && beginCalls==1 && clearCalls==0 &&
         door.effectFlags==7 && door.soundId==99);

  reset(); worldActive=1; animationResult=ESP_NATIVE_DOOR_ANIMATION_INVALID;
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_INVALID);
  assert(stateOpen==0 && setCalls==2 && beginCalls==1 && clearCalls==1);
  assert(door.mutated==0 && door.effectFlags==0 && door.soundId==0 &&
         door.openBefore==0 && door.openAfter==0);

  reset(); worldActive=1; animationResult=ESP_NATIVE_DOOR_ANIMATION_INVALID;
  failSetOnCall=2;
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_INVALID);
  assert(stateOpen==1 && setCalls==2 && clearCalls==1 && door.mutated==1);

  reset(); worldActive=1;
  assert(EspMapLineState_applyDoorCommandBase(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_OK);
  assert(beginCalls==0 && stateOpen==1 && door.mutated==1);

  reset(); worldActive=1; rawStatus=ESP_MAP_LINE_DOOR_NOT_READY;
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_NOT_READY);
  assert(beginCalls==0 && setCalls==0 && rawCalls==1);

  reset(); worldActive=1; failSetOnCall=1;
  memset(&door,0xff,sizeof(door));
  assert(EspMapLineState_applyDoorCommand(&descriptor,7,&door)==ESP_MAP_LINE_DOOR_INVALID);
  assert(beginCalls==0 && stateOpen==0 && door.mutated==0);
  reset(); assert(EspMapLineState_applyDoorCommand(&descriptor,7,NULL)==ESP_MAP_LINE_DOOR_INVALID);
  assert(setCalls==0 && beginCalls==0);

  reset();
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,&result)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK);
  assert(deathCalls==1 && destructibleTurn.pending==1 &&
         destructibleTurn.lineIndex==42 && destructibleTurn.eventTile==123 &&
         destructibleTurn.armed==1 && requestCalls==0);
  assert(EspNativeGameplayDestructible_rollbackLineDeath(&result)==1);
  assert(destructibleTurn.pending==0 && destructibleTurn.cancelled==1);
  assert(EspNativeGameplayDestructibleTurn_flush()==1 && requestCalls==0);

  reset(); deathMutated=0;
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,&result)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK);
  assert(destructibleTurn.pending==0 && destructibleTurn.armed==0);
  reset(); deathRollback=0;
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,&result)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK);
  assert(destructibleTurn.pending==0);
  reset(); deathStatus=ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT;
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,&result)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT);
  assert(destructibleTurn.pending==0);
  reset();
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,NULL)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK);
  assert(destructibleTurn.pending==0);

  reset();
  assert(EspNativeGameplayDestructible_executeLineDeath(123,42,&result)==
         ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK);
  rollbackOk=0;
  assert(EspNativeGameplayDestructible_rollbackLineDeath(&result)==0);
  assert(destructibleTurn.pending==1 && destructibleTurn.cancelled==0);
  result.lineIndex=41; rollbackOk=1;
  assert(EspNativeGameplayDestructible_rollbackLineDeath(&result)==1);
  assert(destructibleTurn.pending==1 && destructibleTurn.cancelled==0);
  allowTurnRequest=0;
  assert(EspNativeGameplayDestructibleTurn_flush()==0);
  assert(destructibleTurn.pending==1 && destructibleTurn.requested==0 &&
         requestCalls==1 && requestedSequence==(0xd0000000UL|42U));
  allowTurnRequest=1;
  assert(EspNativeGameplayDestructibleTurn_flush()==1);
  assert(destructibleTurn.pending==0 && destructibleTurn.requested==1 &&
         requestCalls==2 && requestedSequence==(0xd0000000UL|42U));
  assert(EspNativeGameplayDestructibleTurn_flush()==1 && requestCalls==2);
  puts("PASS RC15 real C: raw door, animation rollback, deferred attack-turn lease");
  return 0;
}
"""

MUTANTS = (
    ("skip-active-door", "door", "!EspNativeResidentGameplay_isActive()",
     "EspNativeResidentGameplay_isActive()"),
    ("animate-on-invalid-status", "door", "status != ESP_MAP_LINE_DOOR_OK",
     "status == ESP_MAP_LINE_DOOR_OK"),
    ("fail-to-rollback-animation", "door",
     "animationStatus == ESP_NATIVE_DOOR_ANIMATION_INVALID",
     "animationStatus != ESP_NATIVE_DOOR_ANIMATION_INVALID"),
    ("invert-rollback-failure", "door", "if (!EspMapLineState_setOpen(outResult->lineIndex,",
     "if (EspMapLineState_setOpen(outResult->lineIndex,"),
    ("stale-effect-flags", "door", "outResult->effectFlags = 0U;",
     "outResult->effectFlags = 1U;"),
    ("stale-sound", "door", "outResult->soundId = 0U;",
     "outResult->soundId = 1U;"),
    ("no-line-commit", "line", "outResult->mutated = 1U;",
     "outResult->mutated = 0U;"),
    ("skip-death-turn", "turn", "outResult->mutated != 0U",
     "outResult->mutated == 0U"),
    ("arm-without-lease", "turn", "outResult->rollbackAvailable != 0U",
     "outResult->rollbackAvailable == 0U"),
    ("cancel-when-raw-failed", "turn", "if (rolledBack &&",
     "if (!rolledBack &&"),
    ("cancel-wrong-line", "turn", "result->lineIndex == destructibleTurn.lineIndex",
     "result->lineIndex != destructibleTurn.lineIndex"),
    ("lose-deferred-retry", "turn",
     "if (!EspNativeGameplayMonsterTurn_requestPassTurn(sequence))",
     "if (EspNativeGameplayMonsterTurn_requestPassTurn(sequence))"),
)


def exercise(sources):
    with tempfile.TemporaryDirectory(prefix="rc15-door-") as dirname:
        p=Path(dirname); src=p/"fixture.c"; exe=p/"fixture"
        bodies=[extract(sources[owner], signature) for owner, signature in BODIES]
        src.write_text(PREFIX + "\n\n".join(bodies) + CHECKS)
        built=subprocess.run(
            ["cc","-std=c11","-O0","-Wall","-Wextra","-Werror",
             str(src),"-o",str(exe)],capture_output=True,text=True)
        require(built.returncode==0,
                "actual C/mutant failed compilation: "+built.stderr[:2500])
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
        return run.returncode==0 and "PASS RC15 real C" in run.stdout


def main():
    source_scan()
    require(exercise(SOURCES), "actual production C violated line/death contract")
    for label, owner, before, after in MUTANTS:
        require(SOURCES[owner].count(before)==1,
                "mutant production anchor drift: "+label)
        changed=dict(SOURCES)
        changed[owner]=changed[owner].replace(before,after,1)
        require(not exercise(changed), "semantic mutation survived: "+label)
    print("PASS ARCH-RC15: 5 real C bodies + 12 adversarial semantic mutants rejected")


if __name__ == "__main__":
    main()
