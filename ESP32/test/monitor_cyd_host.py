#!/usr/bin/env python3
"""Offline proof that logging is armed before the ESP32 reset pulse."""

from __future__ import annotations

import importlib.util
import io
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/monitor_cyd.py"
spec = importlib.util.spec_from_file_location("monitor_cyd", SOURCE)
assert spec is not None and spec.loader is not None
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)

# Default is ESP32/logs even when monitorCyd.sh is invoked from elsewhere.
assert monitor.DEFAULT_LOG_DIRECTORY == ROOT / "logs"


class FakeSerial:
    def __init__(self, *args, **kwargs):
        assert kwargs == {"port": None, "baudrate": 115200,
                          "timeout": 0.1, "exclusive": True}
        self.events = []
        self._dtr = None
        self._rts = None
        self.opened = False
        self.port = None
        self.chunks = [b"ets Jul 29 2019 12:21:46\r\n", b"rst:0x1 (POWERON_RESET)\r\n"]

    @property
    def dtr(self):
        return self._dtr

    @dtr.setter
    def dtr(self, value):
        self.events.append(("DTR", value))
        self._dtr = value

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self.events.append(("RTS", value))
        self._rts = value

    def open(self):
        self.opened = True
        self.events.append(("OPEN", self.port))

    def read(self, count):
        if self.chunks:
            self.events.append(("READ", count))
            return self.chunks.pop(0)
        raise KeyboardInterrupt

    def close(self):
        self.opened = False
        self.events.append(("CLOSE", True))


class FakeSerialModule:
    def __init__(self):
        self.device = None

    def Serial(self, *args, **kwargs):
        self.device = FakeSerial(*args, **kwargs)
        return self.device


with tempfile.TemporaryDirectory() as temp:
    temp_path = Path(temp)
    output = io.BytesIO()
    serial = FakeSerialModule()
    pulses = []

    def sleep(delay):
        assert serial.device is not None
        # This callback represents EN low: the log must already exist.
        assert list(temp_path.glob("*.log")), "reset happened before log creation"
        assert serial.device.opened, "reset happened before UART RX attached"
        assert serial.device.dtr is False, "GPIO0 must remain HIGH for normal boot"
        assert serial.device.rts is True, "EN must be LOW during reset"
        pulses.append(delay)

    try:
        monitor.capture("/dev/ttyUSB0", 115200, temp_path,
                        serial_module=serial, output=output, sleep=sleep)
    except KeyboardInterrupt:
        pass
    dev = serial.device
    assert dev is not None
    assert pulses == [0.1]
    assert dev.events[:3] == [("DTR", False), ("RTS", False),
                              ("OPEN", "/dev/ttyUSB0")]
    assert dev.events[3:7] == [("DTR", False), ("RTS", False),
                               ("RTS", True), ("RTS", False)]
    assert dev.events[-1] == ("CLOSE", True)
    expected = b"ets Jul 29 2019 12:21:46\r\nrst:0x1 (POWERON_RESET)\r\n"
    assert output.getvalue() == expected
    assert len(list(temp_path.glob("*.log"))) == 1
    assert next(temp_path.glob("*.log")).read_bytes() == expected

    # --no-reset must never pulse RTS. The log must still contain all bytes.
    output = io.BytesIO()
    serial = FakeSerialModule()
    try:
        monitor.capture("/dev/ttyUSB0", 115200, temp_path, do_reset=False,
                        serial_module=serial, output=output,
                        sleep=lambda _: (_ for _ in ()).throw(AssertionError("slept")))
    except KeyboardInterrupt:
        pass
    assert ("RTS", True) not in serial.device.events
    assert output.getvalue() == expected
    assert len(list(temp_path.glob("*.log"))) == 2

# Check public CLI defaults and explicit --log-dir override without opening UART.
previous_capture = monitor.capture
cli_calls = []
try:
    def fake_capture(port, baud, directory, *, do_reset=True):
        cli_calls.append((port, baud, directory, do_reset))

    monitor.capture = fake_capture
    assert monitor.main(["--no-reset"]) == 0
    assert cli_calls[-1] == ("/dev/ttyUSB0", 115200, ROOT / "logs", False)
    assert monitor.main(["--log-dir", str(ROOT / "custom"), "--no-reset"]) == 0
    assert cli_calls[-1] == ("/dev/ttyUSB0", 115200, ROOT / "custom", False)
finally:
    monitor.capture = previous_capture

print("monitor_cyd_host PASS: log before reset, normal boot, bytes preserved, "
      "default ESP32/logs and --log-dir override")
