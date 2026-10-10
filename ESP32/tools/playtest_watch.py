#!/usr/bin/env python3
"""CYD playtest watcher: capture bounded context around actionable native defers.

No firmware instrumentation, no SD writes, no game-state mutations.
Requires pyserial only for --port mode; --stdin and --self-test are stdlib-only.
"""

from __future__ import annotations

import argparse
from collections import deque
from datetime import datetime
from pathlib import Path
import re
import sys
import tempfile
import time


# Intentionally explicit: generic "deferred" occurs in successful INFO messages,
# sound=...-deferred, the READY contract and harmless pending animation states.
# Match only meaningful runtime boundaries; add rules after observing actual logs.
RULES = (
    # Firmware detects a replay mismatch before committing: never suppress it.
    ("RNG_REPLAY_DIVERGED", re.compile(
        r"^\\[MONSTERRETAL\\] REPLAY-DIVERGED\\b")),
    ("INVENTORY_UNOWNED", re.compile(
        r"^\[HUB\] SELECT-DEFER page=inventory .*\bkind=item\b")),
    ("UNOWNED_INPUT", re.compile(
        r"^\[RESIDENTGAMEPLAY\] DEFER \b.*semantic-not-enabled\b")),
    # SELECT with no event/eligible command is not an incident: the action
    # engine handles its own entity/world fallback, including NOTHING_TO_USE.
    ("SELECT_DEFER", re.compile(
        r"^\[RESIDENTGAMEPLAY\] SELECT-(?:CHAIN|DIALOG|PASSWORD)-DEFER\b")),
    ("SELECT_UNSUPPORTED", re.compile(
        r"^\[RESIDENTGAMEPLAY\] SELECT-DEFER\b.*\bstatus=(?:UNSUPPORTED_EVENT|COMPLEX_EVENT)\b")),
    ("MOVE_EVENT_DEFER", re.compile(
        r"^\[RESIDENTGAMEPLAY\] MOVE-(?:EVENT|DIALOG|MESSAGE)-DEFER\b")),
    ("WORLD_BACKEND_DEFER", re.compile(
        r"^\[(?:ACTIONENGINE|BARRELRADIUS|BARREL|CRATE|CRATETRAP)\] "
        r"(?:BACKEND-|LETHAL-)?DEFER\b")),
    ("MONSTER_AI_UNOWNED", re.compile(
        r"^\[(?:MONSTERMOVE|MONSTERPOSTMOVE|MONSTER3ATTACK)\] DEFER\b")),
    ("MONSTER_ORDER_UNOWNED", re.compile(
        r"^\[MONSTERTURN\] (?:MEMBER-|ATTACK-)?DEFER\b")),
    ("WORLD_RENDER_FALLBACK", re.compile(
        r"^\[TURNFRAME\] DIAG fail=WORLD_RENDER\b")),
    # LEGACY_GUARD -> RETRY -> RECOVERED is a successful recovery, not a
    # defect. Alert only on an actual final native-frame failure.
    ("FIRSTFRAME_UNRECOVERED", re.compile(
        r"^\[NATIVEFRAME\] FAILED\b")),
    ("HUD_MISMATCH", re.compile(
        r"^\[HUB\] CLOSE\b.*\bexactHud=NO\b")),
)

# Probe/commit RNG parity: replay must keep the same hit and crit outcome.
# A mismatch can happen at a legacy random-table refill and changes damage.
PROBE_LINE = re.compile(r"^\[MONSTERTURN\] MEMBER-ATTACK-PROBE\b")
COMMIT_LINE = re.compile(r"^\[MONSTERRETAL\] COMMIT\b")
KEY_VALUE = re.compile(r"\b(producerProbe|probe|sprite|firstRandHit|crit)=(\d+)")

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def normalize(raw: str) -> str:
    cleaned = ANSI.sub("", raw).replace("\x00", "").strip("\r\n")
    return "".join(c if c == "\t" or ord(c) >= 32 else "?" for c in cleaned)[:800]


def classify(line: str) -> str | None:
    # The generic action engine intentionally defers monster combat to the
    # already-owned native monster backend. A following MONSTERCOMBAT COMMIT
    # is expected, not an unimplemented player action.
    if (line.startswith("[ACTIONENGINE] BACKEND-DEFER") and
            "family=monster-combat" in line):
        return None
    # After a kill, activationOrder intentionally retains historical slots;
    # the legacy fallback may report zero *living* candidates with a nonzero
    # activeCount. The ordered owner already skipped every dead member. Keep
    # failures with real candidates and every other MONSTERMOVE DEFER visible.
    if (line.startswith("[MONSTERMOVE] DEFER ") and
            "cause=active-order-not-owned" in line and
            "candidates=0 " in line):
        return None
    for name, pattern in RULES:
        if pattern.search(line):
            return name
    return None


class PlaytestWatcher:
    def __init__(self, output: Path, before: int, after: int,
                 cooldown: float, bell: bool, echo: bool) -> None:
        self.output = output
        self.output.mkdir(parents=True, exist_ok=True)
        self.ring: deque[str] = deque(maxlen=before)
        self.after = after
        self.cooldown = cooldown
        self.bell = bell
        self.echo = echo
        self.last_seen: dict[str, float] = {}
        self.pending: list[list] = []
        self.count = 0
        self.attack_probes: dict[tuple[int, int], tuple[int, int]] = {}

    def replay_diverged(self, line: str) -> bool:
        if "[ENGINESESSION] READY" in line or "[RESIDENTRESET]" in line:
            self.attack_probes.clear()
            return False
        if not (PROBE_LINE.search(line) or COMMIT_LINE.search(line)):
            return False
        values = {name: int(value) for name, value in KEY_VALUE.findall(line)}
        required = ("sprite", "firstRandHit", "crit")
        if any(field not in values for field in required):
            return False
        if PROBE_LINE.search(line):
            if "producerProbe" not in values:
                return False
            key = (values["producerProbe"], values["sprite"])
            if len(self.attack_probes) >= 16:
                self.attack_probes.pop(next(iter(self.attack_probes)))
            self.attack_probes[key] = (values["firstRandHit"], values["crit"])
            return False
        if "probe" not in values:
            return False
        key = (values["probe"], values["sprite"])
        expected = self.attack_probes.pop(key, None)
        return expected is not None and expected != (
            values["firstRandHit"], values["crit"])

    def feed(self, raw: str) -> None:
        line = normalize(raw)
        if not line:
            return
        now = time.monotonic()
        stamped = f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {line}\n"
        if self.echo:
            print(line, flush=True)

        waiting = []
        for file, remaining in self.pending:
            file.write(stamped)
            file.flush()
            if remaining > 1:
                waiting.append([file, remaining - 1])
            else:
                file.close()
        self.pending = waiting
        self.ring.append(stamped)
        replay_mismatch = self.replay_diverged(line)
        rule = "RNG_REPLAY_DIVERGED" if replay_mismatch else classify(line)
        if rule is None:
            return
        # Never coalesce RNG integrity faults. A rapid series of mismatched
        # attacks must produce one durable capture per detected occurrence,
        # even when the user sets --cooldown to hours. No user-selectable
        # option can disable this exception.
        if (rule != "RNG_REPLAY_DIVERGED" and
                now - self.last_seen.get(rule, -1e12) < self.cooldown):
            return

        if rule != "RNG_REPLAY_DIVERGED":
            self.last_seen[rule] = now
        self.count += 1
        when = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        # Strictly unique within one run, including simultaneous detections.
        path = self.output / f"incident-{when}-{self.count:05d}-{rule}.log"
        file = path.open("w", encoding="utf-8")
        file.write(f"# CYD playtest incident {self.count}\n")
        file.write(f"# category={rule}; before={self.ring.maxlen}; after={self.after}\n")
        file.write("# NOTE: only selected runtime boundaries are trapped; the CYD is NOT paused.\n")
        file.write("# --- preceding lines + trigger ---\n")
        file.writelines(self.ring)
        file.flush()  # Usable even if the CYD produces no more output.
        if self.after:
            self.pending.append([file, self.after])
        else:
            file.close()
        print(f"\a" if self.bell else "", end="")
        print(f"[WATCH] {rule}: saved {path}", flush=True)

    def snapshot(self, reason: str = "manual") -> Path:
        path = self.output / f"snapshot-{datetime.now().strftime('%Y%m%d-%H%M%S-%f')}.log"
        with path.open("w", encoding="utf-8") as file:
            file.write(f"# CYD recent context ({reason}); no device pause\n")
            file.writelines(self.ring)
        print(f"[WATCH] Recent context: {path}", flush=True)
        return path

    def close(self) -> None:
        for file, _ in self.pending:
            file.close()
        self.pending.clear()


def self_test() -> None:
    with tempfile.TemporaryDirectory() as folder:
        output = Path(folder)
        watcher = PlaytestWatcher(output, 12, 2, 60.0, False, False)
        watcher.feed('[ENGINESESSION] READY other-actions-deferred sound=5045-deferred\n')
        watcher.feed('[MONSTERRETAL] WAIT resolution=after-animation deferred-player-death\n')
        assert watcher.count == 0
        watcher.feed('[RESIDENTGAMEPLAY] QUEUE tap=12 action=SELECT zone=5\n')
        watcher.feed('[HUB] SELECT-DEFER page=inventory entry=2 kind=item source=3 cause=unsupported-entry mutation=no turn=no\n')
        assert watcher.count == 1
        watcher.feed('[HUB] SELECT-DEFER page=inventory entry=2 kind=item source=3 cause=unsupported-entry mutation=no turn=no\n')
        assert watcher.count == 1  # Cooldown avoids flooding captures.
        watcher.feed('[RESIDENTGAMEPLAY] HUB-INPUT seq=12 status=IGNORED\n')
        files = list(output.glob("incident-*.log"))
        assert len(files) == 1
        clip = files[0].read_text(encoding="utf-8")
        assert "QUEUE tap=12" in clip and "unsupported-entry" in clip
        assert "HUB-INPUT seq=12" in clip
        assert classify('[TURNFRAME] DIAG fail=WORLD_RENDER player=100,200') == "WORLD_RENDER_FALLBACK"
        assert classify('[MONSTERMOVE] DEFER trigger=NO-IMMEDIATE-ATTACK cause=active-order-not-owned') == "MONSTER_AI_UNOWNED"
        # User CYD case: last living member just died, 0 movement candidates
        # even though two historical activation slots remain.
        assert classify('[MONSTERMOVE] DEFER trigger=NO-IMMEDIATE-ATTACK n=50 candidates=0 activeCount=2 cause=active-order-not-owned mutation=no rngConsumed=0') is None
        assert classify('[MONSTERMOVE] DEFER trigger=NO-IMMEDIATE-ATTACK n=50 candidates=2 activeCount=2 cause=active-order-not-owned mutation=no rngConsumed=0') == "MONSTER_AI_UNOWNED"
        # A destructible enemy/line not yet owned remains an actionable alert.
        assert classify('[ACTIONENGINE] BACKEND-DEFER seq=235 sprite=65535 line=201 family=destructible-combat reason=generic-hit+hp/subtype-consequence-not-owned mutation=no') == "WORLD_BACKEND_DEFER"
        assert classify('[HUB] CLOSE exactHud=NO expectedHud=123') == "HUD_MISMATCH"
        assert classify('[PASSTURN] REQUEST sound=deferred turnAdvance=deferred') is None
        # Real CYD playtest at 04:01: user selects empty space; the action
        # engine presents NOTHING_TO_USE and no unsupported opcode is present.
        assert classify('[ACTIONENGINE] ROUTE seq=4 weapon=2 target=none distance=0 route=NOTHING_TO_USE feedback=screen turnAdvance=deferred') is None
        assert classify('[ACTION] SELECT seq=4 status=NO_EVENT tile=776 event=65535 eligible=0 unsupported=0') is None
        assert classify('[RESIDENTGAMEPLAY] SELECT-DEFER n=1 seq=4 status=NO_EVENT unsupported=0 entity/otherSemantics=deferred mutation=no') is None
        assert classify('[RESIDENTGAMEPLAY] SELECT-DEFER n=3 seq=21 status=NO_ELIGIBLE unsupported=0 entity/otherSemantics=deferred mutation=no') is None
        assert classify('[RESIDENTGAMEPLAY] SELECT-DEFER n=7 seq=25 status=UNSUPPORTED_EVENT unsupported=41 entity/otherSemantics=deferred mutation=no') == "SELECT_UNSUPPORTED"
        assert classify('[RESIDENTGAMEPLAY] SELECT-DEFER n=7 seq=25 status=COMPLEX_EVENT unsupported=0 entity/otherSemantics=deferred mutation=no') == "SELECT_UNSUPPORTED"
        assert classify('[RESIDENTGAMEPLAY] SELECT-DIALOG-DEFER n=1 seq=5 event=12 cmd=0 status=bad-state mutation=no') == "SELECT_DEFER"
        assert classify('[ACTIONENGINE] BACKEND-DEFER seq=30 sprite=200 family=monster-combat reason=native-monster-hp+attack-state-not-owned mutation=no') is None
        assert classify('[NATIVEFRAME] LEGACY_GUARD logical=15 actual=40 owner=BSS bytes=16') is None
        assert classify('[NATIVEFRAME] RECOVERED legacy compact guard actual=40 successorActual=68') is None
        assert classify('[NATIVEFRAME] FAILED route=gameplay code=3/SPAN_OOB') == "FIRSTFRAME_UNRECOVERED"
        # No incident must be written for a normal empty-space SELECT.
        countBeforeEmptySelect = watcher.count
        watcher.feed('[RESIDENTGAMEPLAY] SELECT-DEFER n=1 seq=4 status=NO_EVENT unsupported=0 entity/otherSemantics=deferred mutation=no\n')
        assert watcher.count == countBeforeEmptySelect
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=220 firstRandHit=177 crit=0 producerProbe=1\n')
        watcher.feed('[MONSTERRETAL] COMMIT probe=1 sprite=220 firstRandHit=5 crit=1\n')
        assert watcher.count == 2
        assert len(list(output.glob("*RNG_REPLAY_DIVERGED.log"))) == 1
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=220 firstRandHit=42 crit=0 producerProbe=2\n')
        watcher.feed('[MONSTERRETAL] COMMIT probe=2 sprite=220 firstRandHit=42 crit=0\n')
        assert watcher.count == 2
        # Same category, same session and same second: RNG alerts must not
        # inherit the 60-second general cooldown and must never overwrite.
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=264 firstRandHit=199 crit=0 producerProbe=3\n')
        watcher.feed('[MONSTERRETAL] COMMIT probe=3 sprite=264 firstRandHit=71 crit=0\n')
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=264 firstRandHit=12 crit=0 producerProbe=4\n')
        watcher.feed('[MONSTERRETAL] COMMIT probe=4 sprite=264 firstRandHit=3 crit=1\n')
        assert watcher.count == 4
        rng_files = list(output.glob("incident-*RNG_REPLAY_DIVERGED.log"))
        assert len(rng_files) == 3
        assert len({p.name for p in rng_files}) == 3
        assert all("MONSTERRETAL" in p.read_text(encoding="utf-8") for p in rng_files)
        # A firmware-side replay guard is independently actionable.
        watcher.feed('[MONSTERRETAL] REPLAY-DIVERGED probe=5 reason=MOVE sprite=264 weapon=15 aiRand=245 expected=<217 rngRollback=yes mutation=no\n')
        assert watcher.count == 5
        assert len(list(output.glob("incident-*RNG_REPLAY_DIVERGED.log"))) == 4
        watcher.close()
    print("[WATCH] SELF-TEST PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--stdin", action="store_true",
                        help="Read an existing serial stream from standard input.")
    source.add_argument("--port", default=None,
                        help="Serial port, e.g. /dev/ttyUSB0; requires pyserial.")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=Path, default=Path("playtest-captures"))
    parser.add_argument("--before", type=int, default=75)
    parser.add_argument("--after", type=int, default=20)
    parser.add_argument("--cooldown", type=float, default=45.0,
                        help="Minimum seconds between alerts of the same category.")
    parser.add_argument("--bell", action="store_true", help="Terminal bell on alert.")
    parser.add_argument("--echo", action="store_true", help="Also print every serial line.")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.before < 1 or args.after < 0 or args.cooldown < 0 or args.baud <= 0:
        parser.error("before/baud must be positive and after/cooldown nonnegative")
    if not args.stdin and not args.port:
        parser.error("Choose --port /dev/ttyUSB0 or --stdin")
    watcher = PlaytestWatcher(args.output, args.before, args.after,
                              args.cooldown, args.bell, args.echo)
    try:
        if args.stdin:
            print("[WATCH] Reading stdin; Ctrl+C writes a recent-context snapshot.", flush=True)
            for line in sys.stdin:
                watcher.feed(line)
        else:
            try:
                import serial
            except ImportError:
                print("[WATCH] pyserial required: python3 -m pip install pyserial", file=sys.stderr)
                return 2
            print(f"[WATCH] Listening {args.port} @ {args.baud}. "
                  "Incidents captured automatically; Ctrl+C saves last lines.", flush=True)
            with serial.Serial(args.port, args.baud, timeout=0.5) as connection:
                while True:
                    raw = connection.readline()
                    if raw:
                        watcher.feed(raw.decode("utf-8", errors="replace"))
    except KeyboardInterrupt:
        watcher.snapshot("Ctrl+C")
    except (OSError, ValueError) as error:
        print(f"[WATCH] ERROR: {error}", file=sys.stderr)
        return 1
    finally:
        watcher.close()
    print(f"[WATCH] Done: {watcher.count} incident(s) captured in {args.output}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
