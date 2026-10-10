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
    ("PLAYER_OVERLAP_RECOVERY", re.compile(
        r"^\[PLAYEROVERLAP\] ESCAPE\b")),
    # Firmware detects a replay mismatch before committing: never suppress it.
    ("RNG_REPLAY_DIVERGED", re.compile(
        r"^\[MONSTERRETAL\] REPLAY-DIVERGED\b")),
    ("RNG_GUARD_FAILURE", re.compile(
        r"^\[RNGGUARD\] (?:FATAL-|WORD-FATAL-|BARREL-PREVIEW-(?:FAILED|DIVERGED)\b|ATTACK-PROBE-DEFER\b|ATTACK-PROBE-RESTORE\b.*(?:preExact=NO|boundaryReservation=NO))")),
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
COMMIT_LINE = re.compile(r"^\[MONSTERRETAL\] (?:COMMIT|MISS-COMMIT|LETHAL-COMMIT)\b")
KEY_VALUE = re.compile(r"\b(producerProbe|probe|sprite|firstRandHit|crit)=(\d+)")
BLOCKED_STEP = re.compile(
    r"^\[RESIDENTGAMEPLAY\] MOVE-BLOCKED\b.*"
    r"\btile=(\d+)->(\d+)\b.*\bblocker=(\d+)\s+type=(\d+)\b")
COMMITTED_MOVE = re.compile(r"^\[RESIDENTGAMEPLAY\] MOVE n=\d+\b")


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
        self.blocked_dests: dict[tuple[int, int], set[int]] = {}
        self.stuck_collision_alerted: set[tuple[int, int]] = set()

    def replay_status(self, line: str) -> str | None:
        if "[ENGINESESSION] READY" in line or "[RESIDENTRESET]" in line:
            self.attack_probes.clear()
            return None
        if not (PROBE_LINE.search(line) or COMMIT_LINE.search(line)):
            return None
        values = {name: int(value) for name, value in KEY_VALUE.findall(line)}
        if PROBE_LINE.search(line):
            if any(k not in values for k in
                   ("producerProbe", "sprite", "firstRandHit", "crit")):
                return None
            key = (values["producerProbe"], values["sprite"])
            if len(self.attack_probes) >= 16:
                self.attack_probes.pop(next(iter(self.attack_probes)))
            self.attack_probes[key] = (values["firstRandHit"], values["crit"])
            return None
        if "probe" not in values or "sprite" not in values:
            return None
        key = (values["probe"], values["sprite"])
        expected = self.attack_probes.pop(key, None)
        if expected is None:
            return None  # No paired probe seen by this monitor session.
        if "firstRandHit" not in values:
            # Old LETHAL-COMMIT firmware omits the real first roll. Alert
            # honestly as unverified instead of silently skipping it.
            return "RNG_REPLAY_UNVERIFIED"
        # MISS-COMMIT has no crit field and semantically means crit=0.
        if expected != (values["firstRandHit"], values.get("crit", 0)):
            return "RNG_REPLAY_DIVERGED"
        return None

    def stuck_collision_status(self, line: str) -> str | None:
        if COMMITTED_MOVE.search(line) or (
                "[ENGINESESSION] READY" in line or "[RESIDENTRESET]" in line):
            self.blocked_dests.clear()
            self.stuck_collision_alerted.clear()
            return None
        match = BLOCKED_STEP.search(line)
        if match is None:
            return None
        source_tile, dest_tile, blocker, entity_type = map(int, match.groups())
        if entity_type != 1:
            return None
        key = (source_tile, blocker)
        if key in self.stuck_collision_alerted:
            return None
        # One sprite blocking DIFFERENT destinations from the same source is
        # a strong indicator of source-cell overlap. Ordinary front-only
        # monster collision does not qualify, regardless of repeated taps.
        dests = self.blocked_dests.setdefault(key, set())
        dests.add(dest_tile)
        if len(self.blocked_dests) > 8:
            self.blocked_dests.pop(next(iter(self.blocked_dests)))
        if len(dests) > 1:
            self.stuck_collision_alerted.add(key)
            return "PLAYER_STUCK_COLLISION"
        return None

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
        replay_alert = self.replay_status(line)
        collision_alert = self.stuck_collision_status(line)
        rule = (replay_alert if replay_alert is not None else
                collision_alert if collision_alert is not None else
                classify(line))
        if rule is None:
            return
        # Never coalesce RNG integrity faults. A rapid series of mismatched
        # attacks must produce one durable capture per detected occurrence,
        # even when the user sets --cooldown to hours. No user-selectable
        # option can disable this exception.
        if (rule not in ("RNG_REPLAY_DIVERGED", "RNG_REPLAY_UNVERIFIED",
                         "PLAYER_STUCK_COLLISION", "PLAYER_OVERLAP_RECOVERY",
                         "RNG_GUARD_FAILURE") and
                now - self.last_seen.get(rule, -1e12) < self.cooldown):
            return

        if rule not in ("RNG_REPLAY_DIVERGED", "RNG_REPLAY_UNVERIFIED",
                         "PLAYER_STUCK_COLLISION", "PLAYER_OVERLAP_RECOVERY",
                         "RNG_GUARD_FAILURE"):
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
        # Misses must also preserve the preview roll, despite having no crit
        # field in the MISS-COMMIT format. Lethal commits expose firstRandHit
        # once the new serial-only firmware instrumentation is flashed.
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=264 firstRandHit=241 crit=0 producerProbe=6\n')
        watcher.feed('[MONSTERRETAL] MISS-COMMIT probe=6 sprite=264 firstRandHit=198\n')
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=220 firstRandHit=180 crit=0 producerProbe=7\n')
        watcher.feed('[MONSTERRETAL] LETHAL-COMMIT probe=7 sprite=220 firstRandHit=4 crit=1\n')
        assert watcher.count == 7
        assert len(list(output.glob("incident-*RNG_REPLAY_DIVERGED.log"))) == 6
        # With old lethal firmware, the first roll was never logged: warn
        # rather than silently losing a potentially divergent critical hit.
        watcher.feed('[MONSTERTURN] MEMBER-ATTACK-PROBE sprite=220 firstRandHit=180 crit=0 producerProbe=8\n')
        watcher.feed('[MONSTERRETAL] LETHAL-COMMIT probe=8 sprite=220 totalDamage=4 crit=1\n')
        assert watcher.count == 8
        assert len(list(output.glob("incident-*RNG_REPLAY_UNVERIFIED.log"))) == 1
        # Actual CYD key-area snapshot: the same living monster blocked
        # source tile=665 -> two destinations, without any DEFER log.
        watcher.feed('[RESIDENTGAMEPLAY] MOVE-BLOCKED n=9 seq=520 action=FORWARD tile=665->697 blocker=286 type=1 context=WORLD legacyAdvance=no\n')
        assert watcher.count == 8
        watcher.feed('[RESIDENTGAMEPLAY] MOVE-BLOCKED n=10 seq=521 action=BACK tile=665->633 blocker=286 type=1 context=WORLD legacyAdvance=no\n')
        assert watcher.count == 9
        assert len(list(output.glob("incident-*PLAYER_STUCK_COLLISION.log"))) == 1
        watcher.feed('[RESIDENTGAMEPLAY] MOVE-BLOCKED n=11 seq=522 action=FORWARD tile=665->697 blocker=286 type=1 context=WORLD legacyAdvance=no\n')
        assert watcher.count == 9
        watcher.feed('[RESIDENTGAMEPLAY] MOVE n=62 seq=523 action=FORWARD tile=665->697 committed=yes\n')
        watcher.feed('[RESIDENTGAMEPLAY] MOVE-BLOCKED n=1 seq=524 action=FORWARD tile=697->729 blocker=286 type=1 context=WORLD legacyAdvance=no\n')
        watcher.feed('[RESIDENTGAMEPLAY] MOVE-BLOCKED n=2 seq=525 action=BACK tile=697->665 blocker=286 type=1 context=WORLD legacyAdvance=no\n')
        assert watcher.count == 10
        assert len(list(output.glob("incident-*PLAYER_STUCK_COLLISION.log"))) == 2
        assert classify('[PLAYEROVERLAP] ESCAPE source=665 dest=697 sprite=286 subtype=1') == "PLAYER_OVERLAP_RECOVERY"
        assert classify('[RNGGUARD] FATAL-RESERVATION-MISMATCH next=127 ptrMatch=yes sequenceExact=NO') == "RNG_GUARD_FAILURE"
        assert classify('[RNGGUARD] ATTACK-PROBE-RESTORE preExact=NO boundaryReservation=yes') == "RNG_GUARD_FAILURE"
        assert classify('[RNGGUARD] ATTACK-PROBE-RESTORE preExact=yes boundaryReservation=NO') == "RNG_GUARD_FAILURE"
        assert classify('[RNGGUARD] ATTACK-PROBE-RESTORE preExact=yes boundaryReservation=yes') is None
        assert classify('[RNGGUARD] BARREL-PREVIEW-FAILED seq=49 stateExact=NO action=fail-closed') == "RNG_GUARD_FAILURE"
        assert classify('[RNGGUARD] BARREL-PREVIEW-DIVERGED seq=49 expected=13 actual=20 sequenceExact=NO') == "RNG_GUARD_FAILURE"
        assert classify('[BARRELRADIUS] RNG-PREFLIGHT seq=49 words=3 refill=reserved exact=yes') is None
        # A true barrel replay mismatch is never rate-limited away.
        watcher.feed('[RNGGUARD] BARREL-PREVIEW-DIVERGED seq=49 root=283 expected=11 actual=23 sequenceExact=NO\n')
        watcher.feed('[RNGGUARD] BARREL-PREVIEW-DIVERGED seq=50 root=283 expected=10 actual=24 sequenceExact=NO\n')
        assert watcher.count == 12
        assert len(list(output.glob("incident-*RNG_GUARD_FAILURE.log"))) == 2
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
