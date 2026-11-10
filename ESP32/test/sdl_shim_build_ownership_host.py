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
    # SDL-F1 replaces the former DoomRPG_Error SDL modal with native Serial.
    dormant_ui_stubs = {"SDL_GetScancodeName", "SDL_ShowSimpleMessageBox"}
    dormant_bmp_decoder = {"SDL_LoadBMP_RW"}
    assert not dormant_bmp_decoder.intersection(current), (
        f"{args.profile}: SDL-B7 duplicate BMP parser still linked in candidate: {sorted(dormant_bmp_decoder.intersection(current))}")
    dormant_utility_facades = {"SDL_SwapLE16", "SDL_SwapLE32", "SDL_RWwrite", "SDL_LoadBMP", "SDL_SetRenderDrawBlendMode", "SDL_RenderClear"}
    assert not dormant_utility_facades.intersection(current), (
        f"{args.profile}: SDL-B6 dormant facade linked in candidate: {sorted(dormant_utility_facades.intersection(current))}")
    assert not dormant_ui_stubs.intersection(current), (
        f"{args.profile}: SDL-B5 obsolete UI facade linked in candidate: "
        f"{sorted(dormant_ui_stubs.intersection(current))}")
    assert not dormant_memory_hooks.intersection(current), (
        f"{args.profile}: SDL-B4 hook is linked in candidate: {sorted(dormant_memory_hooks.intersection(current))}")
    assert not dormant_render_helpers.intersection(current), (
        f"{args.profile}: SDL-B3 removed symbol was linked: {sorted(dormant_render_helpers.intersection(current))}")
    # SDL-C1 has a real production migration: the native image owner replaces
    # the now-orphaned RWops/Surface/colorkey stages, not the shared blitter.
    c1_retired = {"SDL_RWFromMem", "SDL_RWread", "SDL_RWseek", "SDL_RWclose",
                  "SDL_CreateTextureFromSurface", "SDL_FreeSurface",
                  "SDL_MapRGB", "SDL_SetColorKey"}
    assert not c1_retired.intersection(current), (
        f"{args.profile}: old SDL image pipeline still linked "
        f"{sorted(c1_retired.intersection(current))}")
    d1_retired = {"SDL_RenderCopy", "SDL_DestroyTexture",
                  "SDL_SetTextureColorMod"}
    assert not d1_retired.intersection(current), (
        f"{args.profile}: SDL-D1 legacy texture ABI still linked "
        f"{sorted(d1_retired.intersection(current))}")
    # SDL-E1 migrates *production* rect/line/clip/color callers to the shared
    # EspNativeTexture pixel owner. Old shim definitions can remain for host
    # fixtures but must not occupy the CYD firmware's linked text.
    e1_retired = {"SDL_RenderSetClipRect", "SDL_SetRenderDrawColor",
                  "SDL_RenderFillRect", "SDL_RenderDrawLine"}
    assert not e1_retired.intersection(current), (
        f"{args.profile}: SDL-E1 legacy vector ABI still linked "
        f"{sorted(e1_retired.intersection(current))}")
    assert "SDL_RenderPresent" in current, (
        f"{args.profile}: SDL-F1 must keep live video presentation")
    f1_retired = {"SDL_GetTicks", "SDL_ShowMessageBox"}
    assert not f1_retired.intersection(current), (
        f"{args.profile}: SDL-F1 legacy process adapter still linked "
        f"{sorted(f1_retired.intersection(current))}")
    nm_tool = (shutil.which("xtensa-esp32-elf-nm") or
               str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm"))
    actual_names = set()
    for line in subprocess.check_output(
            [nm_tool, "--defined-only", str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 3:
            actual_names.add(parts[-1])
    c1_native = {"EspNativeImage_decode", "EspNativeImage_loadPack",
                 "EspNativeImage_dispose", "EspNativeTexture_adoptIndexed"}
    d1_native = {"EspNativeTexture_blit", "EspNativeTexture_destroy",
                 "EspNativeTexture_setColorMod", "EspNativeTexture_setClipRect"}
    e1_native = {"EspNativeTexture_setDrawColor", "EspNativeTexture_fillRect",
                 "EspNativeTexture_drawLine"}
    f1_native = {"EspNativeProcess_ticksMs", "EspNativeProcess_fatalMessage"}
    native_providers = c1_native | d1_native | e1_native | f1_native
    assert native_providers.issubset(actual_names), (
        f"{args.profile}: missing production image/raster owners "
        f"{sorted(native_providers - actual_names)}")
    assert "Esp32Bmp_LoadRW" not in actual_names, (
        f"{args.profile}: old RWops BMP decoder is unexpectedly still linked")
    report = {"profile": args.profile, "linked_sdl": current,
              "linked_sdl_count": len(current), "linked_sdl_body_bytes": sum(current.values()),
              "elf": allocated(elf), "source": "ESP32/src/esp32_sdl.cpp"}
    if args.baseline_dir is not None:
        baseline_elf = args.baseline_dir / "ESP32/.pio/build" / args.profile / "firmware.elf"
        assert baseline_elf.is_file(), baseline_elf
        before = linked_sdl(baseline_elf)
        assert not dormant_bmp_decoder.intersection(before), (
            f"{args.profile}: SDL-B7 duplicate BMP decoder already linked in SDL-A baseline: {sorted(dormant_bmp_decoder.intersection(before))}")
        assert not dormant_utility_facades.intersection(before), (
            f"{args.profile}: SDL-B6 facade ALREADY linked in historical SDL-A baseline: {sorted(dormant_utility_facades.intersection(before))}")
        assert not dormant_ui_stubs.intersection(before), (
            f"{args.profile}: SDL-B5 obsolete UI facade linked in SDL-A baseline: "
            f"{sorted(dormant_ui_stubs.intersection(before))}")
        assert not dormant_memory_hooks.intersection(before), (
            f"{args.profile}: SDL-B4 hook was linked in SDL-A baseline: {sorted(dormant_memory_hooks.intersection(before))}")
        assert not dormant_render_helpers.intersection(before), (
            f"{args.profile}: SDL-B3 symbol was ALREADY linked in SDL-A baseline: {sorted(dormant_render_helpers.intersection(before))}")
        assert set(current).issubset(before), (
            f"{args.profile}: unexpected SDL exports after native migration "
            f"added={sorted(set(current)-set(before))}")
        removed = set(before) - set(current)
        all_retired = c1_retired | d1_retired | e1_retired | f1_retired
        assert all_retired.issubset(removed), (
            f"{args.profile}: missing C1/D1/E1 linked SDL retirement "
            f"{sorted(all_retired - removed)}")
        report["c1_removed_sdl"] = sorted(c1_retired)
        report["d1_removed_sdl"] = sorted(d1_retired)
        report["e1_removed_sdl"] = sorted(e1_retired)
        report["f1_removed_sdl"] = sorted(f1_retired)
        report["f1_native_providers"] = sorted(f1_native)
        report["e1_native_providers"] = sorted(e1_native)
        report["c1_native_providers"] = sorted(c1_native)
        report["d1_native_providers"] = sorted(d1_native)
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
    print(f"SDL-F1 PASS {args.profile}: exactly one native shim TU, native image/raster/process providers, no generated source/obj, "
          f"{len(current)} linked SDL functions / {sum(current.values())} symbolic bytes")


if __name__ == "__main__":
    main()
