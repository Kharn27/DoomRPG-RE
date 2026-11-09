#!/usr/bin/env python3
"""SDL-A compiledb, final ELF symbols, ALLOC sections and baseline audit.

The old generated object is *never* an input to the candidate. Counts, sizes,
and section deltas are reported (not claimed as RAM or runtime performance gains).
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "ESP32/tools"))
from compare_elf_alloc import allocated_sections  # noqa: E402


def linked_sdl(elf):
    nm = (shutil.which("xtensa-esp32-elf-nm") or
          str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm"))
    lines = subprocess.check_output([nm, "-S", "--defined-only", str(elf)], text=True).splitlines()
    names = {}
    for line in lines:
        parts = line.split()
        if len(parts) == 4 and parts[3].startswith("SDL_") and parts[2].lower() == "t":
            assert parts[3] not in names, "duplicate SDL provider: " + parts[3]
            names[parts[3]] = int(parts[1], 16)
    return names


def allocated(elf):
    sections = allocated_sections(elf.read_bytes())[2]
    ram = sum(size for name, (_, _, _, size, _, _) in sections.items()
              if name.startswith((".dram", ".noinit", ".rtc.data", ".rtc.bss")))
    load = sum(size for _, (kind, _, _, size, _, _) in sections.items() if kind != 8)
    return {"ram_named": ram, "loaded_sections": load,
            "total_alloc": sum(s[3] for s in sections.values()),
            "sections": {name: entry[3] for name, entry in sorted(sections.items())}}


def candidate_sources(profile):
    db = ROOT / "ESP32/compile_commands.json"
    assert db.is_file(), "PlatformIO -t compiledb is required"
    entries = json.loads(db.read_text())
    assert entries, "empty compiledb"
    candidates = []
    for entry in entries:
        p = (Path(entry["directory"]) / entry["file"]).resolve()
        if p.name == "esp32_sdl.cpp" or "doomrpg_esp32_sdl_patched" in str(p):
            candidates.append(p)
    expected = (ROOT / "ESP32/src/esp32_sdl.cpp").resolve()
    assert candidates == [expected], f"{profile}: expected one permanent TU, got {candidates}"
    build = ROOT / "ESP32/.pio/build" / profile
    assert not (build / "doomrpg_patched_sources/esp32_sdl.cpp").exists()
    assert not (build / "doomrpg_esp32_sdl_patched").exists()
    assert not (build / "doomrpg_patched_sources/DoomRPG.c").exists()
    assert (build / "firmware.map").is_file()
    return build / "firmware.elf"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile", choices=("esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    parser.add_argument("--baseline-dir", type=Path)
    args = parser.parse_args()
    elf = candidate_sources(args.profile)
    assert elf.is_file()
    current = linked_sdl(elf)
    # SDL-B3: only retire symbols that were already absent from actual ELFs.
    dormant_render_helpers = {"SDL_RenderDrawRect", "SDL_RenderSetIntegerScale"}
    dormant_memory_hooks = {"SDL_SetMemoryFunctions", "SDL_GetMemoryFunctions"}
    # SDL_ShowMessageBox has an explicit DoomRPG_Error caller and remains.
    dormant_ui_stubs = {"SDL_GetScancodeName", "SDL_ShowSimpleMessageBox"}
    assert not dormant_ui_stubs.intersection(current), (
        f"{args.profile}: SDL-B5 obsolete UI facade linked in candidate: "
        f"{sorted(dormant_ui_stubs.intersection(current))}")
    assert not dormant_memory_hooks.intersection(current), (
        f"{args.profile}: SDL-B4 hook is linked in candidate: {sorted(dormant_memory_hooks.intersection(current))}")
    assert not dormant_render_helpers.intersection(current), (
        f"{args.profile}: SDL-B3 removed symbol was linked: {sorted(dormant_render_helpers.intersection(current))}")
    assert "SDL_RenderCopy" in current and "SDL_CreateTextureFromSurface" in current
    assert "SDL_DestroyTexture" in current and "SDL_FreeSurface" in current
    report = {"profile": args.profile, "linked_sdl": current,
              "linked_sdl_count": len(current), "linked_sdl_body_bytes": sum(current.values()),
              "elf": allocated(elf), "source": "ESP32/src/esp32_sdl.cpp"}
    if args.baseline_dir is not None:
        baseline_elf = args.baseline_dir / "ESP32/.pio/build" / args.profile / "firmware.elf"
        assert baseline_elf.is_file(), baseline_elf
        before = linked_sdl(baseline_elf)
        assert not dormant_ui_stubs.intersection(before), (
            f"{args.profile}: SDL-B5 obsolete UI facade linked in SDL-A baseline: "
            f"{sorted(dormant_ui_stubs.intersection(before))}")
        assert not dormant_memory_hooks.intersection(before), (
            f"{args.profile}: SDL-B4 hook was linked in SDL-A baseline: {sorted(dormant_memory_hooks.intersection(before))}")
        assert not dormant_render_helpers.intersection(before), (
            f"{args.profile}: SDL-B3 symbol was ALREADY linked in SDL-A baseline: {sorted(dormant_render_helpers.intersection(before))}")
        assert set(before) == set(current), (
            f"{args.profile}: changed SDL ABI/linkage "
            f"removed={sorted(set(before)-set(current))} "
            f"added={sorted(set(current)-set(before))}")
        report["baseline_linked_sdl"] = before
        report["baseline_sdl_count"] = len(before)
        report["baseline_sdl_body_bytes"] = sum(before.values())
        report["baseline_elf"] = allocated(baseline_elf)
        report["deltas"] = {
            "linked_sdl_body_bytes": sum(current.values()) - sum(before.values()),
            "loaded_sections_bytes": report["elf"]["loaded_sections"] -
                                     report["baseline_elf"]["loaded_sections"],
            "ram_named_bytes": report["elf"]["ram_named"] -
                               report["baseline_elf"]["ram_named"],
        }
    output = elf.parent / "sdl_a_elf_report.json"
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print("SDL-A " + json.dumps({k: v for k, v in report.items()
                                 if k not in ("elf", "baseline_elf", "linked_sdl",
                                              "baseline_linked_sdl")}, sort_keys=True))
    print(f"SDL-A PASS {args.profile}: exactly one native shim TU, no generated source/obj, "
          f"{len(current)} linked SDL functions / {sum(current.values())} symbolic bytes")


if __name__ == "__main__":
    main()
