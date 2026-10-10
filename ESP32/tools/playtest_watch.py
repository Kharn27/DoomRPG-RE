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
    ("INVENTORY_UNOWNED", re.compile(
        r"^\[HUB\] SELECT-DEFER page=inventory .*\bkind=item\b")),
    ("UNOWNED_INPUT", re.compile(
        r"^\[RESIDENTGAMEPLAY\] DEFER \b.*semantic-not-enabled\b")),
    ("SELECT_DEFER", re.compile(
        r"^\[RESIDENTGAMEPLAY\] (?:SELECT-(?:CHAIN|DIALOG|PASSWORD)-DEFER|SELECT-DEFER)\b")),
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
    ("RENDER_GUARD", re.compile(
        r"^\[NATIVEFRAME\] LEGACY_GUARD\b")),
    ("HUD_MISMATCH", re.compile(
        r"^\[HUB\] CLOSE\b.*\bexactHud=NO\b")),
)

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def normalize(raw: str) -> str:
    cleaned = ANSI.sub("", raw).replace("\x00", "").strip("\r\n")
    return "".join(c if c == "\t" or ord(c) >= 32 else "?" for c in cleaned)[:800]


def classify(line: str) -> str | None:
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
        rule = classify(line)
        if rule is None or now - self.last_seen.get(rule, -1e12) < self.cooldown:
            return

        self.last_seen[rule] = now
        self.count += 1
        when = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        path = self.output / f"incident-{when}-{rule}.log"
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
        assert classify('[HUB] CLOSE exactHud=NO expectedHud=123') == "HUD_MISMATCH"
        assert classify('[PASSTURN] REQUEST sound=deferred turnAdvance=deferred') is None
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
