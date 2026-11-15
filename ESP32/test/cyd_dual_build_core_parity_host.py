#!/usr/bin/env python3
"""R-A2b: prevent classic and alternate-panel CYD builds from forking engine code.

Read two *actual* PlatformIO compile_commands.json databases. Verify that each
compiles the identical set of ESP32/src C/C++ translation units with identical
preprocessor defines other than the intentional LCD panel selection. Core
source changes should thus always reach BOTH release firmware variants.
This complements, rather than replaces, real CI builds and CYD Serial tests.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shlex

ROOT = Path(__file__).resolve().parents[2]
NATIVE = (ROOT / "ESP32/src").resolve()
LCD_MACRO = "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION"
REQUIRED = {
    "main.cpp",
    "platform_video.cpp",
    "esp_asset_pack.cpp",
    "esp_native_gameplay_frame.c",
    "esp_native_gameplay_monster_drop.c",
}
# PlatformIO may encode the environment name into a diagnostic macro. It is
# not part of firmware gameplay, and varies solely with the environment name.
ENV_METADATA_DEFINES = {"PIOENV"}


def require(condition: bool, msg: str) -> None:
    if not condition:
        raise ValueError(msg)


def command_defines(entry: dict) -> dict[str, str]:
    argv = (entry["arguments"] if "arguments" in entry
            else shlex.split(entry["command"]))
    out = {}
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "-D":
            i += 1
            require(i < len(argv), "truncated -D compiler flag")
            define = argv[i]
        elif arg.startswith("-D") and len(arg) > 2:
            define = arg[2:]
        else:
            i += 1
            continue
        key, _, val = define.partition("=")
        require(key and key not in out,
                f"duplicate or malformed compiler define {define}")
        out[key] = val or "1"
        i += 1
    return out


def load_native(path: Path) -> dict[str, dict[str, str]]:
    payload = json.loads(path.read_text())
    require(isinstance(payload, list) and payload,
            f"{path}: missing compilation database")
    project = {}
    for entry in payload:
        directory = Path(entry.get("directory") or ROOT)
        src = Path(entry["file"])
        if not src.is_absolute():
            src = directory / src
        src = src.resolve()
        try:
            local = src.relative_to(NATIVE).as_posix()
        except ValueError:
            continue
        require(local not in project, f"{path}: duplicated native TU {local}")
        require(src.suffix.lower() in (".c", ".cc", ".cpp", ".cxx"),
                f"{path}: unexpected native compile unit {local}")
        project[local] = command_defines(entry)
    require(len(project) >= 140,
            f"{path}: native compilation database unexpectedly small: {len(project)}")
    require(REQUIRED.issubset(project),
            f"{path}: missing important production C/C++ owners: "
            f"{sorted(REQUIRED - project.keys())}")
    return project


def compare(normal: dict, alternate: dict) -> None:
    missing = sorted(normal.keys() - alternate.keys())
    added = sorted(alternate.keys() - normal.keys())
    require(not missing and not added,
            f"native engine source fork: missing={missing} extra={added}")
    for name in sorted(normal):
        baseline = {k: v for k, v in normal[name].items()
                    if k not in ENV_METADATA_DEFINES}
        variant = {k: v for k, v in alternate[name].items()
                   if k not in ENV_METADATA_DEFINES}
        require(LCD_MACRO not in baseline or baseline[LCD_MACRO] == "0",
                f"{name}: normal screen incorrectly selects alternate LCD")
        require(variant.get(LCD_MACRO) == "1",
                f"{name}: alternate LCD selection flag missing")
        baseline.pop(LCD_MACRO, None)
        variant.pop(LCD_MACRO, None)
        require(baseline == variant,
                f"{name}: engine defines forked between both release profiles: "
                f"normalOnly={sorted(set(baseline.items())-set(variant.items()))} "
                f"alternateOnly={sorted(set(variant.items())-set(baseline.items()))}")
    print(f"PASS dual CYD core: {len(normal)} identical native C/C++ TUs, "
          "same compile defines except LCD hardware selector; "
          "shared engine source changes enter both firmware variants")


def self_test() -> None:
    main = {"main.cpp": {"DOOMRPG_ESP32": "1"},
            "platform_video.cpp": {"DOOMRPG_ESP32": "1"}}
    variant = {name: {**defs, LCD_MACRO: "1"}
               for name, defs in main.items()}
    compare(main, variant)
    for wrong in (
        {"main.cpp": variant["main.cpp"]},
        {"main.cpp": variant["main.cpp"],
         "platform_video.cpp": {**variant["platform_video.cpp"],
                                "DOOMRPG_ENGINE_MODE": "4"}},
        {"main.cpp": variant["main.cpp"],
         "platform_video.cpp": {"DOOMRPG_ESP32": "1"}},
    ):
        try:
            compare(main, wrong)
        except ValueError:
            continue
        raise AssertionError("forked build source/flags accepted")
    print("PASS dual CYD core self-test rejects missing TU, extra game define, "
          "missing alternate panel selection")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--normal", type=Path)
    ap.add_argument("--inverted", type=Path)
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return
    require(args.normal and args.inverted,
            "compiledb check requires --normal and --inverted")
    compare(load_native(args.normal), load_native(args.inverted))


if __name__ == "__main__":
    main()
