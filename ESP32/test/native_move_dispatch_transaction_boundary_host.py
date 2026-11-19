#!/usr/bin/env python3
"""ARCH-RC16: compile the four ACTUAL MOVE dispatcher/owner C bodies.

The public owner preserves EXIT preflight/effects, raw position publication,
ENTER effects, deferred dialog/message leases, rejection of busy owners and
rollback ordering. No hardware assertion: the classic CYD Serial is final.
Every semantic mutant must compile but violate the same runtime assertions.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SOURCES = {
    "dispatch": (SRC / "esp_native_gameplay_dispatch.c").read_text(),
    "move": (SRC / "esp_native_gameplay_move_events.c").read_text(),
}
RETIRED = ("EspNativeGameplayDispatch_commitMove",
           "EspNativeGameplayDispatch_rollbackMove")
BODIES = (
    ("dispatch", "EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_commitMoveBase("),
    ("dispatch", "EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_rollbackMoveBase("),
    ("move", "EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_commitMove("),
    ("move", "EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_rollbackMove("),
)


def require(ok, reason):
    if not ok:
        raise AssertionError(reason)


def extract(source, signature):
    require(source.count(signature) == 1, "unique real C function: " + signature)
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError("unbalanced C body: " + signature)


def source_scan():
    all_src = {p.name: re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", p.read_text())
               for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(all_src) >= 178, "production corpus unexpectedly small")
    ini = (ROOT / "platformio.ini").read_text()
    normal = ini.split("[env:esp32-cyd]", 1)[1].split("[env:esp-cyd-inverted]", 1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=", normal, re.M)) == 14,
            "RC16 expects exactly 14 normal GNU wraps")
    for name in RETIRED:
        require("-Wl,--wrap=" + name not in ini, "retired flag: " + name)
        for filename, source in all_src.items():
            require(not re.search(r"\b__(?:wrap|real)_" + name + r"\b", source),
                    "retired GNU alias in " + filename)
        require("EspNativeGameplayDispatch_" in name, "dispatch scope drift")
    for name in ("EspNativeGameplayFrame_renderTurn", "EspMapRuntime_getLine",
                 "EspMapLineState_view", "EspPlayerView_commitPreparedMove",
                 "Esp32PlatformVideo_present"):
        require("-Wl,--wrap=" + name in ini, "protected wrapper retired: " + name)
    require("EspNativeGameplayDispatch_commitMove(" not in SOURCES["dispatch"],
            "raw dispatcher must not bypass public MOVE owner")
    require("EspNativeGameplayDispatch_rollbackMove(" not in SOURCES["dispatch"],
            "raw rollback must not bypass public MOVE owner")
    for kind, sig in BODIES:
        extract(SOURCES[kind], sig)
    require("EspNativeGameplayDispatch_commitMoveBase(" in SOURCES["move"],
            "event owner must call raw dispatch")
    require("EspNativeGameplayDispatch_rollbackMoveBase(" in SOURCES["move"],
            "event owner must call raw rollback")
    print("PASS RC16 source: 178+ C/C++ TUs, 2 retired wraps, 14 remaining")


PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_MAP_OPCODE_SHOW 7U
typedef struct { int32_t viewX; int32_t viewAngle; } EspPlayerViewState;
typedef struct {
  uint32_t sequence;
  uint16_t sourceTile, destTile;
  uint8_t prepared, committed, rolledBack;
} EspNativeGameplayMoveResult;
typedef enum {
  ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID=0,
  ESP_NATIVE_GAMEPLAY_DISPATCH_STALE=1,
  ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED=2,
  ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED=3,
  ESP_NATIVE_GAMEPLAY_DISPATCH_OK=4,
  ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK=5
} EspNativeGameplayDispatchStatus;
typedef enum { ESP_PLAYER_VIEW_MOVE_OK=0, ESP_PLAYER_VIEW_MOVE_STALE=1,
               ESP_PLAYER_VIEW_MOVE_INVALID=2 } EspPlayerViewMoveStatus;
typedef enum {
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT=0,
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED=1,
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX=2,
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY=3,
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_MESSAGE_READY=4,
  ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK=5
} EspNativeGameplayMoveEventStatus;
typedef struct {
  uint16_t eventIndex;
  uint8_t rollbackAvailable;
  uint8_t codeId;
  uint8_t commandOffset;
  uint8_t showBatchCount;
} EspNativeGameplayMoveEventResult;
typedef struct { uint8_t active; uint16_t eventIndex; uint8_t count; }
  ShowOwner;
typedef struct { uint8_t active; } MixedOwner;
typedef struct {
  uint32_t sequence;
  EspNativeGameplayMoveEventResult exitResult, enterResult;
  uint8_t exitRollback, enterRollback, dialogPending, messagePending;
  uint8_t active;
} MoveEventTransaction;
typedef struct { uint8_t destAngle; } TurnState;
static TurnState turnState;
static MoveEventTransaction transaction;
static ShowOwner showBatchOwner;
static MixedOwner mixedBatchOwner;
static uint32_t liveX;
static int allowReady, allowCandidate, allowFlags, allowEffectRollback;
static int allowTransactionRollback;
static int failViewOnCall;
static int viewCalls, rawCommitObserved, inspectCalls, executeCalls;
static int effectRollbackCalls, transactionRollbackCalls;
static int clearShowCalls, clearMixedCalls;
static uint8_t exitRollbackLease, enterRollbackLease, exitCode;
static EspNativeGameplayMoveEventStatus exitPreflightStatus, enterPreflightStatus;
static EspNativeGameplayMoveEventStatus exitExecutionStatus, enterExecutionStatus;

static int candidateMatchesMoveResult(const EspPlayerViewState* a,
                                      const EspPlayerViewState* b,
                                      const EspNativeGameplayMoveResult* m) {
  return allowCandidate && a && b && m && m->prepared==1 &&
         m->committed==0 && m->rolledBack==0 &&
         a->viewX==0 && b->viewX==64;
}
static int EspNativeGameplayDispatch_isReady(void) { return allowReady; }
static EspPlayerViewMoveStatus EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* before, const EspPlayerViewState* after) {
  ++viewCalls;
  if (failViewOnCall==viewCalls) return ESP_PLAYER_VIEW_MOVE_STALE;
  if (!before || !after || liveX!=(uint32_t)before->viewX)
    return ESP_PLAYER_VIEW_MOVE_STALE;
  liveX=(uint32_t)after->viewX;
  return ESP_PLAYER_VIEW_MOVE_OK;
}
static int movementFlags(const EspNativeGameplayMoveResult* m,
                         const EspPlayerViewState* after,
                         uint32_t* exitFlags, uint32_t* enterFlags) {
  if (!allowFlags || !m || !after || !exitFlags || !enterFlags) return 0;
  *exitFlags=0x20U; *enterFlags=0x08U; return 1;
}
static void clearShowBatchOwner(void) {
  ++clearShowCalls;
  memset(&showBatchOwner, 0, sizeof(showBatchOwner));
}
static void clearMixedBatchOwner(void) {
  ++clearMixedCalls;
  memset(&mixedBatchOwner, 0, sizeof(mixedBatchOwner));
}
static EspNativeGameplayMoveEventStatus inspectPhase(
  uint16_t tile, uint32_t flags, EspNativeGameplayMoveEventResult* result) {
  ++inspectCalls;
  (void)flags; memset(result,0,sizeof(*result));
  return tile==100U ? exitPreflightStatus : enterPreflightStatus;
}
static int phaseUnsafe(EspNativeGameplayMoveEventStatus s) {
  return s==ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED ||
         s==ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
}
static const char* EspNativeGameplayMoveEvents_statusName(
    EspNativeGameplayMoveEventStatus s) { (void)s; return "host-status"; }
static void logPhase(const char* label, uint32_t sequence,
   EspNativeGameplayMoveEventStatus status,
   const EspNativeGameplayMoveEventResult* result) {
  (void)label; (void)sequence; (void)status; (void)result;
}
static EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executePhase(
    uint16_t tile, uint32_t flags, EspNativeGameplayMoveEventResult* out) {
  ++executeCalls;
  (void)flags; memset(out, 0, sizeof(*out));
  if (tile==100U) {
    out->rollbackAvailable=exitRollbackLease;
    out->codeId=exitCode;
    out->eventIndex=17U;
    out->showBatchCount=1U;
    return exitExecutionStatus;
  }
  out->rollbackAvailable=enterRollbackLease;
  out->eventIndex=18U;
  return enterExecutionStatus;
}
static int EspNativeGameplayMoveEvents_rollbackPhase(
   const EspNativeGameplayMoveEventResult* result) {
  ++effectRollbackCalls;
  return result != NULL && allowEffectRollback;
}
static int rollbackTransaction(void) {
  ++transactionRollbackCalls;
  if (!allowTransactionRollback) return 0;
  memset(&transaction,0,sizeof(transaction));
  return 1;
}
"""

CHECKS = r"""
static void resetFixture(void) {
  memset(&turnState,0,sizeof(turnState));
  memset(&transaction,0,sizeof(transaction));
  memset(&showBatchOwner,0,sizeof(showBatchOwner));
  memset(&mixedBatchOwner,0,sizeof(mixedBatchOwner));
  liveX=0;
  allowReady=1; allowCandidate=1; allowFlags=1;
  allowEffectRollback=1; allowTransactionRollback=1;
  failViewOnCall=0;
  viewCalls=rawCommitObserved=inspectCalls=executeCalls=0;
  effectRollbackCalls=transactionRollbackCalls=0;
  clearShowCalls=clearMixedCalls=0;
  exitRollbackLease=enterRollbackLease=exitCode=0;
  exitPreflightStatus=enterPreflightStatus=
      ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT;
  exitExecutionStatus=enterExecutionStatus=
      ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT;
}
static EspNativeGameplayMoveResult candidate(uint32_t sequence) {
  EspNativeGameplayMoveResult m;
  memset(&m,0,sizeof(m));
  m.sequence=sequence; m.sourceTile=100; m.destTile=200; m.prepared=1;
  return m;
}
int main(void) {
  EspPlayerViewState before={0,0}, after={64,0};
  EspNativeGameplayMoveResult m;
  EspNativeGameplayDispatchStatus s;

  resetFixture(); m=candidate(4);
  s=EspNativeGameplayDispatch_commitMove(&before,&after,&m);
  assert(s==ESP_NATIVE_GAMEPLAY_DISPATCH_OK && m.committed==1 &&
         liveX==64 && viewCalls==1 && executeCalls==2 &&
         inspectCalls==2 && transaction.active==0);
  s=EspNativeGameplayDispatch_rollbackMove(&after,&before,&m);
  assert(s==ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK &&
         liveX==0 && m.committed==0 && m.rolledBack==1 &&
         viewCalls==2 && transactionRollbackCalls==0);

  resetFixture(); m=candidate(5); transaction.active=1; transaction.sequence=2;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);
  assert(liveX==0 && viewCalls==0 && inspectCalls==0);
  resetFixture(); m=candidate(5); showBatchOwner.active=1;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);
  resetFixture(); m=candidate(5); mixedBatchOwner.active=1;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);

  resetFixture(); m=candidate(6);
  exitPreflightStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED);
  assert(liveX==0 && executeCalls==0 && viewCalls==0 && m.committed==0);
  resetFixture(); m=candidate(6);
  exitPreflightStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED);
  assert(viewCalls==0);
  resetFixture(); m=candidate(7);
  enterPreflightStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED);
  assert(liveX==0 && executeCalls==0);

  resetFixture(); m=candidate(8);
  exitExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED);
  assert(liveX==0 && m.committed==0 && viewCalls==0 && executeCalls==1);

  resetFixture(); m=candidate(9);
  exitRollbackLease=1; failViewOnCall=1;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_STALE);
  assert(liveX==0 && m.committed==0 && effectRollbackCalls==1);

  resetFixture(); m=candidate(10);
  exitRollbackLease=1;
  enterExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED);
  assert(liveX==0 && m.rolledBack==1 && m.committed==0 &&
         effectRollbackCalls==1 && viewCalls==2);
  resetFixture(); m=candidate(11);
  exitRollbackLease=1; failViewOnCall=2;
  enterExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED);
  assert(liveX==64 && m.committed==1 && m.rolledBack==0 &&
         effectRollbackCalls==0);

  resetFixture(); m=candidate(12);
  exitExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK;
  exitCode=ESP_MAP_OPCODE_SHOW; exitRollbackLease=1;
  enterExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_OK);
  assert(transaction.active==1 && transaction.sequence==12 &&
         transaction.exitRollback==1 && transaction.dialogPending==1 &&
         transaction.messagePending==0 && liveX==64);
  m.sequence=99;
  assert(EspNativeGameplayDispatch_rollbackMove(&after,&before,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);
  assert(transaction.active==1 && liveX==64 && viewCalls==1);
  m.sequence=12;
  assert(EspNativeGameplayDispatch_rollbackMove(&after,&before,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK);
  assert(transaction.active==0 && transactionRollbackCalls==1 &&
         liveX==0 && m.rolledBack==1);

  resetFixture(); m=candidate(13);
  enterExecutionStatus=ESP_NATIVE_GAMEPLAY_MOVE_EVENT_MESSAGE_READY;
  assert(EspNativeGameplayDispatch_commitMove(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_OK);
  assert(transaction.active && transaction.messagePending &&
         !transaction.dialogPending);
  allowTransactionRollback=0;
  assert(EspNativeGameplayDispatch_rollbackMove(&after,&before,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED);
  assert(transaction.active==1 && liveX==0 &&
         transactionRollbackCalls==1 && m.rolledBack==1);

  resetFixture(); m=candidate(14);
  allowCandidate=0;
  assert(EspNativeGameplayDispatch_commitMoveBase(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);
  assert(viewCalls==0 && liveX==0);
  resetFixture(); m=candidate(14);
  turnState.destAngle=64;
  assert(EspNativeGameplayDispatch_commitMoveBase(&before,&after,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_STALE);
  assert(viewCalls==0);
  resetFixture(); m=candidate(14);
  m.committed=0;
  assert(EspNativeGameplayDispatch_rollbackMoveBase(&after,&before,&m)==
         ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID);
  assert(viewCalls==0);
  puts("PASS RC16 real C: EXIT/position/ENTER/rollback and fail-closed leases");
  return 0;
}
"""

MUTANTS = (
    ("ignore-busy-owner", "move",
     "if (transaction.active || showBatchOwner.active || mixedBatchOwner.active) {",
     "if (transaction.active && showBatchOwner.active && mixedBatchOwner.active) {"),
    ("defer-on-safe", "move",
     "if (phaseUnsafe(exitPreflightStatus) ||",
     "if (!phaseUnsafe(exitPreflightStatus) ||"),
    ("exit-dialog-admission", "move",
     "exitPreflightStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY ||",
     "exitPreflightStatus != ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY ||"),
    ("skip-exit-failure", "move",
     "if (phaseUnsafe(exitStatus) ||",
     "if (!phaseUnsafe(exitStatus) ||"),
    ("wrong-raw-leaf", "move",
     "dispatchStatus = EspNativeGameplayDispatch_commitMoveBase(",
     "dispatchStatus = EspNativeGameplayDispatch_rollbackMoveBase("),
    ("ignore-raw-failure", "move",
     "if (dispatchStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {",
     "if (dispatchStatus == ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {"),
    ("invert-enter-rollback", "move",
     "if (phaseUnsafe(enterStatus)) {",
     "if (!phaseUnsafe(enterStatus)) {"),
    ("accept-failed-view-rollback", "move",
     "rollbackStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||",
     "rollbackStatus == ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||"),
    ("lose-enter-lease", "move",
     "transaction.active = 1U;",
     "transaction.active = 0U;"),
    ("allow-wrong-rollback-sequence", "move",
     "if (transaction.active && transaction.sequence != sequence) {",
     "if (transaction.active && transaction.sequence == sequence) {"),
    ("ignore-raw-rollback-failure", "move",
     "if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK) return status;",
     "if (status == ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK) return status;"),
    ("ignore-transaction-failure", "move",
     "if (transaction.active && !rollbackTransaction()) {",
     "if (transaction.active && rollbackTransaction()) {"),
    ("drop-position-commit-bit", "dispatch",
     "ioResult->committed = 1U;",
     "ioResult->committed = 0U;"),
    ("drop-position-rollback-bit", "dispatch",
     "ioResult->rolledBack = 1U;",
     "ioResult->rolledBack = 0U;"),
    ("ignore-candidate-guard", "dispatch",
     "if (!candidateMatchesMoveResult(expectedBeforeView, preparedAfterView, ioResult)) {",
     "if (candidateMatchesMoveResult(expectedBeforeView, preparedAfterView, ioResult)) {"),
)


def exercise(sources):
    with tempfile.TemporaryDirectory(prefix="rc16-move-") as dirname:
        base = Path(dirname)
        fixture, binary = base / "transaction.c", base / "transaction"
        bodies = [extract(sources[owner], sig) for owner, sig in BODIES]
        # Place the raw leaves before the public facade, exactly as prototypes
        # in the permanent native dispatch header promise.
        fixture.write_text(PREFIX + "\n\n".join(bodies) + CHECKS)
        built = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             str(fixture), "-o", str(binary)],
            capture_output=True, text=True)
        require(built.returncode == 0,
                "actual/mutant C failed compilation: " + built.stderr[:2500])
        run = subprocess.run([str(binary)], capture_output=True, text=True,
                             timeout=10)
        return (run.returncode == 0 and
                "PASS RC16 real C:" in run.stdout)


def main():
    source_scan()
    require(exercise(SOURCES), "production MOVE owner violated its real C oracle")
    for name, owner, before, after in MUTANTS:
        require(SOURCES[owner].count(before) == 1,
                "mutant anchor drift: " + name)
        changed = dict(SOURCES)
        changed[owner] = changed[owner].replace(before, after, 1)
        require(not exercise(changed), "semantic mutant survived: " + name)
    print("PASS ARCH-RC16: four actual C bodies, 15 compiled adversarial mutants")


if __name__ == "__main__":
    main()
