#!/usr/bin/env python3
"""ARCH-RC0: inventory declared GNU ld --wrap boundaries against REAL CYD ELFs.

A wrapper definition present in an ELF is a linkage witness, NOT proof that a
call executes at runtime. Disassembled direct calls are lower-bound witnesses:
indirect calls, pointers and Xtensa tail/inlined paths may not be visible.
Never infer that an absent symbol or absent direct-call witness is safe to
delete without a separate contract/behavior parity milestone.

No firmware code, source rewriting, or emulated native execution here.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "ESP32"
PROFILES = ("esp32-cyd", "esp-cyd-inverted",
            "esp32-cyd-bringup", "esp32-cyd-trace")
WRAP = re.compile(r"^-Wl,--wrap=([A-Za-z_][A-Za-z0-9_]*)$")
SYMBOL = re.compile(
    r"^([0-9a-fA-F]+)\s+(?:([0-9a-fA-F]+)\s+)?([A-Za-z])\s+([^\s]+)$")
FUNCTION = re.compile(r"^\s*[0-9a-fA-F]+ <([^>]+)>:$")
CALL = re.compile(
    r"\b(?:call0|call4|call8|call12|j)\s+[0-9a-fA-F]+\s+"
    r"<(__wrap_[A-Za-z_][A-Za-z0-9_]*)(?:\+[^>]*)?>")
REQUIRED = {"EspNativeGameplayFrame_renderTurn",
            "EspAssetPack_readRange", "DoomRPG_randNextByte",
            "Esp32PlatformVideo_present"}
# Arduino/ESP-IDF log adapters are provided by the toolchain, not the
# DoomRPG platformio.ini composition. Their exact linked symbols were
# confirmed in all four real Xtensa ELF artifacts on ARCH-RC0.
TOOLCHAIN_LOG_WRAPPERS = frozenset((
    "__wrap_esp_log_write", "__wrap_esp_log_writev", "__wrap_log_printf"))
# ARCH-RC1: these were declared in the diagnostic build but none of the
# four ARCH-RC0 real ELFs contained their linked __wrap_ bodies.
RETIRED_BRINGUP_WRAPS = frozenset((
    "DoomRPG_probeNativeMenuSpriteFrame",
    "DoomRPG_probeNativeMainMenuOverlay"))
RETAINED_BRINGUP_WRAPS = frozenset((
    "EspNativeFirstFrame_route",
    "EspNativeGameplayInteractionInventory_log",
    "EspNativeResidentGameplay_service"))


def require(ok: bool, why: str) -> None:
    if not ok:
        raise ValueError(why)


def declarations(config: str) -> dict[str, list[str]]:
    """Read the actual ini sections; never mistake commented flags for flags."""
    sections: dict[str, list[str]] = {}
    section = ""
    for raw in config.splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            require(section not in sections, f"duplicate ini section: {section}")
            sections[section] = []
            continue
        if line.startswith("-Wl,--wrap"):
            match = WRAP.fullmatch(line)
            require(match is not None, f"malformed ld --wrap: {line}")
            require(section in sections and section.startswith("env:"),
                    f"orphan linker wrap: {line}")
            symbol = match.group(1)
            require(symbol not in sections[section],
                    f"duplicate ld --wrap in {section}: {symbol}")
            sections[section].append(symbol)
    base = sections.get("env:esp32-cyd")
    require(base is not None and len(base) >= 40,
            "missing canonical normal build or unexpectedly small --wrap inventory")
    require(REQUIRED.issubset(base), "missing required native boundary")
    require(len(sections.get("env:esp32-cyd-bringup", [])) >= 3,
            "bringup-only diagnostic boundary inventory unexpectedly small")
    resurrected = sorted(RETIRED_BRINGUP_WRAPS.intersection(
        name for flags in sections.values() for name in flags))
    require(not resurrected,
            f"retired diagnostic --wrap flags resurrected: {resurrected}")
    # Release inverted/diagnostic profiles inherit the normal flags; any
    # local additions are reported separately rather than silently assumed.
    return sections


def parse_nm(raw: str) -> dict[str, dict[str, int | str]]:
    found: dict[str, dict[str, int | str]] = {}
    for line in raw.splitlines():
        match = SYMBOL.fullmatch(line.strip())
        if not match:
            continue
        address, size, kind, name = match.groups()
        if not name.startswith("__wrap_"):
            continue
        require(kind in ("T", "t", "W", "w"),
                f"linked wrapper is not code: {name} {kind}")
        require(name not in found, f"duplicate linked wrapper symbol: {name}")
        found[name] = {"address": int(address, 16),
                       "bytes": int(size, 16) if size is not None else 0,
                       "type": kind}
    return found


def direct_calls(disassembly: str) -> dict[str, list[str]]:
    result: dict[str, set[str]] = {}
    caller = "<unknown>"
    for line in disassembly.splitlines():
        function = FUNCTION.match(line)
        if function:
            caller = function.group(1)
        call = CALL.search(line)
        if call is not None:
            result.setdefault(call.group(1), set()).add(caller)
    return {name: sorted(callers) for name, callers in sorted(result.items())}


def tool(name: str) -> Path:
    binary = shutil.which("xtensa-esp32-elf-" + name)
    path = Path(binary) if binary else (
        Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin"
        / ("xtensa-esp32-elf-" + name))
    require(path.is_file(), f"Xtensa tool missing: {path}")
    return path


def profile_report(profile: str, declared: dict[str, list[str]]) -> dict:
    require(profile in PROFILES, f"unexpected firmware profile: {profile}")
    elf = PROJECT / ".pio/build" / profile / "firmware.elf"
    require(elf.is_file(), f"real firmware ELF missing: {elf}")
    symbols = parse_nm(subprocess.check_output(
        [str(tool("nm")), "-S", "--defined-only", str(elf)], text=True))
    retired_linked = sorted("__wrap_" + name for name in RETIRED_BRINGUP_WRAPS
                            if "__wrap_" + name in symbols)
    require(not retired_linked,
            f"retired diagnostic wrappers linked in {profile}: {retired_linked}")
    callsites = direct_calls(subprocess.check_output(
        [str(tool("objdump")), "-d", str(elf)], text=True))
    base = declared["env:esp32-cyd"]
    own = declared.get("env:" + profile, [])
    names = list(base) if profile == "esp32-cyd" else list(base) + list(own)
    require(len(names) == len(set(names)),
            f"duplicate inherited --wrap boundary in {profile}")
    rows = []
    for name in names:
        key = "__wrap_" + name
        definition = symbols.get(key)
        callers = callsites.get(key, [])
        require(not callers or definition is not None,
                f"disassembly references wrapper with no linked definition: {key}")
        rows.append({
            "target": name, "scope": "normal" if name in base else profile,
            "wrapperLinked": definition is not None,
            "wrapperTextBytes": definition["bytes"] if definition else 0,
            # Lower-bound evidence only. Do NOT call a zero proof of dormancy.
            "observedDirectCallers": callers,
            "classification": (
                "direct-call-witness" if callers else
                "linked-no-direct-call-witness" if definition else
                "no-linked-wrapper-symbol")
        })
    # Every configured native interposition must now have a corresponding
    # linked body: ARCH-RC1 removed the last two dead diagnostic declarations.
    missing = [row["target"] for row in rows if not row["wrapperLinked"]]
    require(not missing,
            f"declared --wrap has no linked owner in {profile}: {missing}")
    surplus = sorted(set(symbols) - {"__wrap_" + name for name in names})
    unexpected = sorted(set(surplus) - TOOLCHAIN_LOG_WRAPPERS)
    require(not unexpected,
            f"unknown linked __wrap_ owner absent from platformio.ini: {unexpected}")
    report = {
        "schema": "doomrpg-arch-rc0-native-wraps-v1",
        "profile": profile, "declaredNormal": len(base),
        "declaredProfileExtra": len(names) - len(base),
        "linked": sum(row["wrapperLinked"] for row in rows),
        "directCallWitness": sum(bool(row["observedDirectCallers"]) for row in rows),
        "toolchainLogWrapperSymbols": surplus,
        "retiredBringupWrappers": sorted(RETIRED_BRINGUP_WRAPS),
        "unknownLinkedWrapperSymbols": unexpected,
        "boundaries": rows,
        "caution": "ELF linkage != executed call; direct-call witnesses are incomplete"
    }
    path = elf.parent / "native_wrap_inventory.json"
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"ARCH-RC0 {profile} declared={len(names)} "
          f"linked={report['linked']} directCallWitness={report['directCallWitness']} "
          f"unlisted={len(surplus)} output={path}")
    if surplus:
        print(f"ARCH-RC0 {profile} toolchainLogWrapperSymbols={','.join(surplus)} "
              "(ESP32 toolchain logging, not native DoomRPG --wrap flags)")
    return report


def compare_release(normal: dict, inverted: dict) -> None:
    require(normal.get("profile") == "esp32-cyd" and
            inverted.get("profile") == "esp-cyd-inverted",
            "release reports have wrong profile IDs")
    require(normal.get("declaredProfileExtra") == 0 and
            inverted.get("declaredProfileExtra") == 0,
            "alternate release has extra linker interpositions")
    a = {row["target"]: row for row in normal["boundaries"]}
    b = {row["target"]: row for row in inverted["boundaries"]}
    require(a.keys() == b.keys(), "release linker flag sets differ")
    # Symbol admission should agree with the proven common engine, but a
    # changed disassembly call count is NOT a correctness failure or proof of
    # different gameplay (LCD code/layout can affect compiler/link results).
    mismatch = [name for name in a
                if a[name]["wrapperLinked"] != b[name]["wrapperLinked"]]
    require(not mismatch, f"native wrapper linkage differs across LCD: {mismatch}")
    print(f"PASS ARCH-RC0 common release engine wraps={len(a)} "
          "linked wrapper admission identical (runtime invocation unproven)")


def self_test() -> None:
    real = declarations((PROJECT / "platformio.ini").read_text())
    require(len(real["env:esp32-cyd"]) == 45, "expected baseline 45 normal wraps")
    require(len(real["env:esp32-cyd-bringup"]) == 3,
            "expected 3 surviving diagnostic extras after ARCH-RC1")
    require(set(real["env:esp32-cyd-bringup"]) == RETAINED_BRINGUP_WRAPS,
            "bringup diagnostic --wrap set drifted")
    require(not RETIRED_BRINGUP_WRAPS.intersection(
        name for flags in real.values() for name in flags),
        "retired diagnostic interpositions still declared")
    require(not real.get("env:esp-cyd-inverted") and
            not real.get("env:esp32-cyd-trace"),
            "LCD/trace unexpectedly add wrappers")
    for name in sorted(RETIRED_BRINGUP_WRAPS):
        bad = ((PROJECT / "platformio.ini").read_text() +
               "\\n[env:retired-negative]\\n-Wl,--wrap=" + name + "\\n")
        try:
            declarations(bad)
        except ValueError as error:
            require("resurrected" in str(error),
                    f"wrong retirement guard failure for {name}: {error}")
        else:
            raise AssertionError(f"retired diagnostic wrap {name} accepted")
    for bad in (
        (PROJECT / "platformio.ini").read_text() +
            "\n[env:unexpected]\n-Wl,--wrap=EspAssetPack_readRange\n"
            "-Wl,--wrap=EspAssetPack_readRange\n",
        (PROJECT / "platformio.ini").read_text() +
            "\n[env:unexpected]\n-Wl,--wrap=\n",
    ):
        try:
            declarations(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid duplicate/malformed wrap accepted")
    fixture = (
        "40000000 00000030 T __wrap_EspAssetPack_readRange\n"
        "40000100 00000010 T __wrap_Esp32PlatformVideo_present\n")
    parsed = parse_nm(fixture)
    require(len(TOOLCHAIN_LOG_WRAPPERS) == 3 and
            "__wrap_esp_log_write" in TOOLCHAIN_LOG_WRAPPERS and
            "__wrap_EspAssetPack_readRange" not in TOOLCHAIN_LOG_WRAPPERS,
            "unexpected native/toolchain wrapper allowlist")
    require(parsed["__wrap_EspAssetPack_readRange"]["bytes"] == 48,
            "nm symbol size incorrect")
    for bad in (
        fixture + "40000200 00000008 T __wrap_EspAssetPack_readRange\n",
        "40000000 00000030 B __wrap_EspAssetPack_readRange\n",
    ):
        try:
            parse_nm(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("duplicate/non-code wrapper was accepted")
    assembly = ("40000120 <caller_one>:\n"
                "  40000125: call8 40000000 <__wrap_EspAssetPack_readRange>\n"
                "40000220 <caller_two>:\n"
                "  40000225: call0 40000004 <__wrap_EspAssetPack_readRange+0x4>\n")
    require(direct_calls(assembly)["__wrap_EspAssetPack_readRange"] ==
            ["caller_one", "caller_two"], "failed to attribute direct-call witnesses")
    def report(profile: str, linked: bool) -> dict:
        return {"profile": profile, "declaredProfileExtra": 0,
                "boundaries": [{"target": "foo", "wrapperLinked": linked}]}
    compare_release(report("esp32-cyd", True),
                    report("esp-cyd-inverted", True))
    try:
        compare_release(report("esp32-cyd", True),
                        report("esp-cyd-inverted", False))
    except ValueError:
        pass
    else:
        raise AssertionError("release wrapper divergence accepted")
    print("PASS ARCH-RC1 host self-test: 45+3 surviving declarations, 2 retired "
          "bringup-only flags blocked, malformed/duplicate rejected, nm syntax, "
          "direct-call attribution, release mismatch rejected; linked != executed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--profile", choices=PROFILES)
    parser.add_argument("--compare-release", action="store_true")
    args = parser.parse_args()
    try:
        require(sum(bool(x) for x in (args.self_test, args.profile,
                                     args.compare_release)) == 1,
                "choose exactly one of --self-test, --profile, --compare-release")
        if args.self_test:
            self_test()
        elif args.profile:
            profile_report(args.profile,
                           declarations((PROJECT / "platformio.ini").read_text()))
        else:
            root = PROJECT / ".pio/build"
            a = json.loads((root / "esp32-cyd/native_wrap_inventory.json").read_text())
            b = json.loads((root / "esp-cyd-inverted/native_wrap_inventory.json").read_text())
            compare_release(a, b)
    except (ValueError, OSError, subprocess.CalledProcessError, KeyError) as error:
        print("[ARCH-RC0] FAIL", error)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
