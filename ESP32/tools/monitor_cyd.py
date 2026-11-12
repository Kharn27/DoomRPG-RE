#!/usr/bin/env python3
"""Log the classic CYD serial console from its reset vector onward.

Host-only tool. The log is opened BEFORE USB-UART and the RTS reset pulse.
Use the PlatformIO virtualenv Python (it already supplies pyserial).
"""

from __future__ import annotations

import argparse
from datetime import datetime
from pathlib import Path
import sys
import time
from typing import BinaryIO, Callable


RESET_HOLD_SECONDS = 0.1

# Stable ESP32/logs/ location independent of the invoking working directory.
# Open the file in this directory BEFORE triggering the normal RTS boot.
DEFAULT_LOG_DIRECTORY = Path(__file__).resolve().parents[1] / "logs"


def reset_cyd(device: object, sleep: Callable[[float], None] = time.sleep) -> None:
    """Pulse ESP32 EN/RTS while keeping GPIO0/DTR deasserted (normal boot)."""
    # DTR=True drives GPIO0 low on the standard ESP32 USB-UART circuit and
    # could boot the ROM downloader. Keep it high throughout the reset pulse.
    device.dtr = False
    device.rts = False
    device.rts = True  # EN low (reset asserted)
    sleep(RESET_HOLD_SECONDS)
    device.rts = False  # EN high (normal firmware boots)


def new_log(directory: Path) -> tuple[Path, BinaryIO]:
    directory.mkdir(parents=True, exist_ok=True)
    stem = datetime.now().strftime("platformio-device-monitor-%y%m%d-%H%M%S")
    for suffix in range(1000):
        name = f"{stem}.log" if suffix == 0 else f"{stem}-{suffix:03d}.log"
        path = directory / name
        try:
            return path, path.open("xb", buffering=0)
        except FileExistsError:
            continue
    raise RuntimeError("Too many monitor log files for the current second")


def capture(port: str, baud: int, directory: Path, *, do_reset: bool = True,
            serial_module: object | None = None, output: BinaryIO | None = None,
            sleep: Callable[[float], None] = time.sleep) -> Path:
    """Own the port and log; caller may interrupt with Ctrl+C."""
    if serial_module is None:
        try:
            import serial as serial_module  # type: ignore[no-redef]
        except ImportError as exc:
            raise RuntimeError("pyserial missing: use ~/.platformio/penv/bin/python") from exc
    if output is None:
        output = sys.stdout.buffer

    log_path, log = new_log(directory)
    # Never reset before opening the file (or before attaching the serial RX).
    with log:
        device = serial_module.Serial(port=None, baudrate=baud, timeout=0.1,
                                      exclusive=True)
        try:
            device.dtr = False
            device.rts = False
            device.port = port
            device.open()
            print(f"[CYD-MONITOR] Logging to {log_path.resolve()}", file=sys.stderr)
            if do_reset:
                print("[CYD-MONITOR] RTS reset; DTR=off (normal boot)",
                      file=sys.stderr)
                reset_cyd(device, sleep)
            else:
                print("[CYD-MONITOR] Attached without resetting CYD",
                      file=sys.stderr)
            while True:
                data = device.read(4096)
                if data:
                    log.write(data)
                    output.write(data)
                    output.flush()
        finally:
            device.close()
    return log_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIRECTORY)
    parser.add_argument("--no-reset", action="store_true",
                        help="Attach to a running session (do not reboot the CYD)")
    args = parser.parse_args(argv)
    try:
        capture(args.port, args.baud, args.log_dir, do_reset=not args.no_reset)
    except KeyboardInterrupt:
        print("\n[CYD-MONITOR] Stopped; log flushed and saved", file=sys.stderr)
    except (OSError, RuntimeError) as exc:
        print(f"[CYD-MONITOR] ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
