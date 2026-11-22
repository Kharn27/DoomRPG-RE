#!/usr/bin/env python3
"""PERF-PORT-P1: keep the CYD ON/OFF hardware comparison reproducible.

This is a *configuration* oracle. The real renderer's C code and palette
semantics are separately compiled/tested ON and OFF by perf_port_palette_host.py.
"""
from __future__ import annotations

import argparse
import configparser
import json
from pathlib import Path
import shlex

ROOT = Path(__file__).resolve().parents[1]
INI = ROOT / "platformio.ini"
RENDERER = ROOT / "src" / "esp_native_first_frame.c"


def assert_profiles() -> None:
    parser = configparser.ConfigParser(interpolation=None)
    if not parser.read(INI, encoding="utf-8"):
        raise AssertionError(f"missing {INI}")

    normal = parser["env:esp32-cyd"]
    off = parser["env:esp32-cyd-perf-off"]

    assert parser["platformio"]["default_envs"].strip() == "esp32-cyd"
    assert off["extends"].strip() == "env:esp32-cyd"
    assert "${env:esp32-cyd.build_flags}" in off["build_flags"]
    assert off["build_flags"].count("-D DOOMRPG_PERF_PORT_PALETTE_REUSE=0") == 1
    assert "-I.pio/libdeps/esp32-cyd-perf-off/TFT_eSPI" in off["build_flags"]
    assert "DOOMRPG_PERF_PORT_PROFILE=1" in normal["build_flags"]
    assert "DOOMRPG_PERF_PORT_PALETTE_REUSE=" not in normal["build_flags"]
    assert "DOOMRPG_RA2B_CAPTURE=1" in normal["build_flags"]
    assert "DOOMRPG_ESP32_BRINGUP_PROBES" not in off["build_flags"]
    assert "DOOMRPG_LOG_LEVEL=4" not in off["build_flags"]

    renderer = RENDERER.read_text(encoding="utf-8")
    assert "#ifndef DOOMRPG_PERF_PORT_PALETTE_REUSE\n#define DOOMRPG_PERF_PORT_PALETTE_REUSE 1\n#endif" in renderer
    assert "#if DOOMRPG_PERF_PORT_PROFILE" in renderer
    print("PERF-PORT-P1 A/B config PASS: normal ON, perf-off OFF, same INFO/probe/base")


def assert_compiled_reference(path: Path) -> None:
    db = json.loads(path.read_text(encoding="utf-8"))
    matched = [row for row in db if Path(row["file"]).name == "esp_native_first_frame.c"]
    assert len(matched) == 1, f"expected one real production renderer compile: {len(matched)}"

    row = matched[0]
    args = row.get("arguments")
    if args is None:
        args = shlex.split(row["command"])
    definitions = []
    for i, arg in enumerate(args):
        if arg == "-D":
            assert i + 1 < len(args), "truncated -D"
            definitions.append(args[i + 1])
        elif arg.startswith("-D"):
            definitions.append(arg[2:])

    assert "DOOMRPG_PERF_PORT_PROFILE=1" in definitions, "OFF witness must remain instrumented"
    assert "DOOMRPG_PERF_PORT_PALETTE_REUSE=0" in definitions, "OFF witness compiled with reuse ON"
    assert "DOOMRPG_ESP32=1" in definitions, "not the normal native CYD engine"
    assert "DOOMRPG_RA2B_CAPTURE=1" in definitions, "baseline capture not preserved"
    assert "DOOMRPG_ESP32_BRINGUP_PROBES=1" not in definitions, "diagnostic build invalidates A/B"
    print("PERF-PORT-P1 A/B compiledb PASS: genuine renderer C built OFF with profile=1")


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--compiledb", type=Path, help="PlatformIO OFF compile_commands.json")
    args = cli.parse_args()
    assert_profiles()
    if args.compiledb:
        assert_compiled_reference(args.compiledb)
