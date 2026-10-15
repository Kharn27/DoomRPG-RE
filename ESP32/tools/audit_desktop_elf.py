#!/usr/bin/env python3
"""Attribute desktop-copy input sections to loaded ELF ranges, not discarded map rows.

This is build/link evidence, never hardware execution evidence. Anonymous input
rodata/literals are included in section totals; named symbol sizes are separate.
Direct-call evidence omits indirect calls and pointers. COMMON may group symbols.
"""
import argparse
import csv
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

TARGETS = ("DoomRPG", "Render")
NM = Path(shutil.which("xtensa-esp32-elf-nm") or
          Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm")
ROW = re.compile(r"^\s+(?:(\.\S+|COMMON)\s+)?0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S+\.o(?:\))?)\s*$")
SECTION = re.compile(r"^ (\.\S+|COMMON)\s*$")


def map_sections(text, allocated):
    result = {name: [] for name in TARGETS}
    pending = None
    in_map = False
    for line in text.splitlines():
        if line.strip() == "Linker script and memory map":
            in_map = True
        section = SECTION.match(line)
        if section:
            pending = section[1]
            continue
        row = ROW.match(line)
        if not row:
            continue
        name, address, size, obj = row.groups()
        name = name or pending
        pending = None
        stem = next((s for s in TARGETS
                     if obj.endswith("/doomrpg_engine_patched/" + s + ".o")), None)
        if stem is None or name is None:
            continue
        address, size = int(address, 16), int(size, 16)
        # Membership in an actual SHF_ALLOC range, not merely a nonzero address.
        output = next((n for n, (_, _, start, length, _, _) in allocated.items()
                       if size and start <= address and address + size <= start + length), None)
        result[stem].append({"section": name, "address": address, "size": size,
                             "loaded": in_map and output is not None,
                             "output": output if in_map else None})
    return result


def symbols(nm, obj):
    text = subprocess.check_output([str(nm), "-S", "--defined-only", str(obj)], text=True)
    result = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 4:
            address, size, kind, name = parts
            result.setdefault(name, []).append((int(address, 16), int(size, 16), kind))
    return result


def owned_symbol(candidates, loaded):
    matches = [symbol for symbol in candidates
               if any(s["address"] <= symbol[0] < s["address"] + s["size"] for s in loaded)]
    if len(matches) > 1:
        raise ValueError("ambiguous named symbol in attributed input ranges")
    return matches[0] if matches else None


def audit(build, root, nm, db=None):
    sys.path.insert(0, str(root / "ESP32/tools"))
    from compare_elf_alloc import allocated_sections
    _, _, allocated = allocated_sections((build / "firmware.elf").read_bytes())
    sections = map_sections((build / "firmware.map").read_text(), allocated)
    final = symbols(nm, build / "firmware.elf")
    entries = json.loads(db.read_text()) if db else None
    if entries is not None and not entries:
        raise ValueError("empty compilation database cannot prove source admission")
    admitted = None
    originals = []
    if entries is not None:
        admitted = set()
        for entry in entries:
            source = (Path(entry["directory"]) / entry["file"]).resolve()
            if source.is_relative_to(root / "ESP32/src") or source.is_relative_to(root / "ESP32/.pio/build"):
                admitted.add(str(source.relative_to(root)))
            if source.is_relative_to(root / "src"):
                originals.append(str(source.relative_to(root)))
        admitted = sorted(admitted)
    reports = {}
    for stem in TARGETS:
        obj = build / "doomrpg_engine_patched" / (stem + ".o")
        if not obj.exists():  # Snapshot layout before a consolidation.
            obj = build / (stem + ".o")
        unit_admitted = any(s.endswith("/doomrpg_patched_sources/" + stem + ".c")
                            for s in admitted) if admitted is not None else None
        # A physical object may be stale after BuildSources registration removal.
        defined = symbols(nm, obj) if obj.exists() and unit_admitted is not False else {}
        loaded = [s for s in sections[stem] if s["loaded"]]
        rows = []
        for name, definitions in sorted(defined.items()):
            if len(definitions) != 1:
                raise ValueError("ambiguous object symbol: " + name)
            _, size, kind = definitions[0]
            linked = False
            address = 0
            final_size = 0
            match = owned_symbol(final.get(name, []), loaded)
            if match:
                address, final_size, _ = match
                linked = True
            rows.append({"symbol": name, "kind": kind, "object_bytes": size,
                         "linked": linked, "address": address if linked else 0,
                         "final_bytes": final_size if linked else 0})
        totals = {}
        for s in loaded:
            totals[s["output"]] = totals.get(s["output"], 0) + s["size"]
        reports[stem] = {
            "admitted": unit_admitted, "object_present": obj.exists(),
            "defined_symbols": len(defined),
            "linked_functions": sum(r["linked"] and r["kind"].upper() == "T" for r in rows),
            "linked_named_data": sum(r["linked"] and r["kind"].upper() != "T" for r in rows),
            "input_loaded_bytes": totals, "loaded_sections": len(loaded),
            "discarded_sections": sum(not s["loaded"] and s["size"] > 0 for s in sections[stem]),
            "symbols": rows}
    if entries is not None:
        reports["admitted_engine_units"] = admitted
        reports["desktop_original_units"] = sorted(set(originals))
        reports["compile_commands"] = len(entries)
    return reports


def direct_callers(nm, elf, reports):
    objdump = Path(nm).with_name(Path(nm).name.replace("-nm", "-objdump"))
    text = subprocess.check_output([str(objdump), "-d", str(elf)], text=True)
    wanted = {r["symbol"] for stem in TARGETS for r in reports[stem]["symbols"]
              if r["linked"] and r["kind"].upper() == "T"}
    callers = {name: set() for name in wanted}
    caller = None
    for line in text.splitlines():
        function = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
        if function:
            caller = function[1]
        call = re.search(r"\b(?:call[048]|call12|j)\s+[0-9a-f]+ <([^>+]+)(?:\+[^>]*)?>", line)
        if call and call[1] in callers and caller != call[1]:
            callers[call[1]].add(caller)
    return {name: sorted(c) for name, c in sorted(callers.items())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--nm", type=Path, default=NM)
    parser.add_argument("--compiledb", type=Path)
    parser.add_argument("--csv", action="store_true")
    parser.add_argument("--calls", action="store_true")
    parser.add_argument("--require-retired-render", action="store_true")
    parser.add_argument("--require-native-doomrpg-presentation", action="store_true")
    parser.add_argument("--require-native-doomrpg-clock", action="store_true")
    parser.add_argument("--require-native-doomrpg-rng", action="store_true")
    parser.add_argument("--require-native-doomrpg-config-read", action="store_true")
    args = parser.parse_args()
    reports = audit(args.build, args.repo.resolve(), args.nm, args.compiledb)
    if args.require_native_doomrpg_config_read:
        if args.compiledb is None:
            parser.error("--require-native-doomrpg-config-read requires compiledb")
        names = {"File_readByte", "File_readInt"}
        generated = {row["symbol"] for row in reports["DoomRPG"]["symbols"]}
        if generated & names or reports["desktop_original_units"]:
            parser.exit(1, "FAIL: legacy config reader definition/source leaked\n")
        owner = args.build / "src" / "esp_doomrpg_config_read.c.o"
        if not owner.is_file():
            parser.exit(1, "FAIL: native config reader owner not built\n")
        owner_nm = subprocess.check_output(
            [str(args.nm), "--defined-only", str(owner)], text=True)
        own = {}
        for line in owner_nm.splitlines():
            fields = line.split()
            if len(fields) >= 3:
                own.setdefault(fields[-1], []).append(fields[-2])
        if any(own.get(name) != ["T"] for name in names):
            parser.exit(1, "FAIL: native config reader owner symbols: " +
                        str(own) + "\n")
        elf_nm = subprocess.check_output(
            [str(args.nm), "-g", "--defined-only",
             str(args.build / "firmware.elf")], text=True)
        found = {name: 0 for name in names}
        for line in elf_nm.splitlines():
            fields = line.split()
            if len(fields) >= 3 and fields[-2].upper() == "T" and fields[-1] in found:
                found[fields[-1]] += 1
        if any(n != 1 for n in found.values()):
            parser.exit(1, "FAIL: config readers linked count: " +
                        str(found) + "\n")
        print("PASS native config readers linked=2 generated=0 "
              "signedByte=preserved LE32=preserved")
        return
    if args.require_native_doomrpg_rng:
        if args.compiledb is None:
            parser.error("--require-native-doomrpg-rng requires current compiledb")
        linked = {"Random_Get", "Random_getRand", "DoomRPG_setRand"}
        definitions = linked | {"Random_SetSeed"}
        generated = {row["symbol"] for row in reports["DoomRPG"]["symbols"]}
        leaked = sorted((definitions | {"_seed", "resetRand"}) & generated)
        if leaked:
            parser.exit(1, "FAIL: generated DoomRPG still owns RNG: " +
                        ", ".join(leaked) + "\n")
        if reports["desktop_original_units"]:
            parser.exit(1, "FAIL: desktop original unit admitted\n")
        owner = args.build / "src" / "esp_doomrpg_process_rng.c.o"
        if not owner.is_file():
            parser.exit(1, "FAIL: permanent RNG owner not built\n")
        native_nm = subprocess.check_output(
            [str(args.nm), "--defined-only", str(owner)], text=True)
        owner_symbols = {}
        for line in native_nm.splitlines():
            fields = line.split()
            if len(fields) >= 3:
                owner_symbols.setdefault(fields[-1], []).append(fields[-2])
        if any(owner_symbols.get(x) != ["T"] for x in definitions):
            parser.exit(1, "FAIL: RNG owner method definitions: " +
                        str({x: owner_symbols.get(x) for x in definitions}) + "\n")
        for name in ("_seed", "resetRand"):
            matches = [x for x in owner_symbols
                       if x == name or x.startswith(name + ".")]
            if len(matches) != 1 or owner_symbols[matches[0]] != ["b"]:
                parser.exit(1, "FAIL: hidden RNG BSS owner: " +
                            name + " " + str(matches) + "\n")
        output = subprocess.check_output(
            [str(args.nm), "-g", "--defined-only",
             str(args.build / "firmware.elf")], text=True)
        counts = {name: 0 for name in linked}
        for line in output.splitlines():
            fields = line.split()
            if len(fields) >= 3 and fields[-1] in counts and fields[-2].upper() == "T":
                counts[fields[-1]] += 1
        if any(v != 1 for v in counts.values()):
            parser.exit(1, "FAIL: linked native RNG methods: " +
                        str(counts) + "\n")
        print("PASS process RNG provider linked=3 ownerDefinitions=4 "
              "hiddenGlobals=2 generated=0 originalDesktop=0")
        return
    if args.require_native_doomrpg_clock:
        if args.compiledb is None:
            parser.error("--require-native-doomrpg-clock requires current compiledb")
        wanted = {"DoomRPG_GetTimeMS", "DoomRPG_GetUpTimeMS"}
        generated = {row["symbol"] for row in reports["DoomRPG"]["symbols"]}
        leaked = sorted((wanted | {"basetime"}) & generated)
        if leaked:
            parser.exit(1, "FAIL: generated DoomRPG clock definitions: " +
                        ", ".join(leaked) + "\n")
        if reports["desktop_original_units"]:
            parser.exit(1, "FAIL: desktop original unit admitted\n")
        owner = args.build / "src" / "esp_doomrpg_process_clock.c.o"
        if not owner.is_file():
            parser.exit(1, "FAIL: native clock owner object not built\n")
        native_nm = subprocess.check_output(
            [str(args.nm), "--defined-only", str(owner)], text=True)
        symbols = {}
        for line in native_nm.splitlines():
            parts = line.split()
            if len(parts) >= 3:
                symbols.setdefault(parts[-1], []).append(parts[-2].upper())
        if any(symbols.get(name) != ["T"] for name in wanted):
            parser.exit(1, "FAIL: native clock ABI missing/duplicate: " +
                        str({name: symbols.get(name) for name in wanted}) + "\n")
        # GCC can suffix the static basetime symbol but the ownership must
        # remain in this object, not disappear into the generated root.
        state = [name for name in symbols if name == "basetime" or
                 name.startswith("basetime.")]
        if len(state) != 1 or symbols[state[0]] != ["B"]:
            parser.exit(1, "FAIL: native clock basetime must be one BSS owner: " +
                        str(state) + "\n")
        final = subprocess.check_output(
            [str(args.nm), "-g", "--defined-only",
             str(args.build / "firmware.elf")], text=True)
        counts = {name: 0 for name in wanted}
        for line in final.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-1] in counts and parts[-2].upper() == "T":
                counts[parts[-1]] += 1
        if any(count != 1 for count in counts.values()):
            parser.exit(1, "FAIL: linked native clock methods missing/duplicate: " +
                        str(counts) + "\n")
        print("PASS native clock ABI=2 generated clock definitions/state=0 "
              "process basetime=BSS original desktop TUs=0")
        return
    if args.require_native_doomrpg_presentation:
        if args.compiledb is None:
            parser.error("--require-native-doomrpg-presentation requires current compiledb")
        wanted = {
            "DoomRPG_setClipFalse", "DoomRPG_setClipTrue",
            "DoomRPG_setColor", "DoomRPG_setFontColor",
            "DoomRPG_fillRect", "DoomRPG_drawLine",
            "DoomRPG_flushGraphics",
        }
        generated = {row["symbol"] for row in reports["DoomRPG"]["symbols"]}
        leaked = sorted(wanted & generated)
        if leaked:
            parser.exit(1, "FAIL: DoomRPG.o still defines presentation methods: " +
                        ", ".join(leaked) + "\\n")
        if reports["desktop_original_units"]:
            parser.exit(1, "FAIL: desktop original unit admitted\\n")
        # Verify the permanent owner itself defines the ABI, rather than
        # accepting arbitrary providers merely because final symbols exist.
        owner_obj = args.build / "src" / "esp_doomrpg_presentation.c.o"
        if not owner_obj.is_file():
            parser.exit(1, "FAIL: native presentation owner object not built\\n")
        owner_symbols = subprocess.check_output(
            [str(args.nm), "-g", "--defined-only", str(owner_obj)], text=True)
        owner_counts = {name: 0 for name in wanted}
        for line in owner_symbols.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-1] in owner_counts and parts[-2].upper() == "T":
                owner_counts[parts[-1]] += 1
        if any(count != 1 for count in owner_counts.values()):
            parser.exit(1, "FAIL: presentation owner object symbols: " +
                        str(owner_counts) + "\\n")
        # Require each global definition to survive exactly once in the ELF,
        # independent of old generator object or stale build-directory files.
        result = subprocess.check_output([str(args.nm), "-g", "--defined-only",
                                          str(args.build / "firmware.elf")], text=True)
        counts = {name: 0 for name in wanted}
        for line in result.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-1] in counts and parts[-2].upper() == "T":
                counts[parts[-1]] += 1
        bad = {name: count for name, count in counts.items() if count != 1}
        if bad:
            parser.exit(1, "FAIL: native presentation symbols missing/duplicate: " +
                        str(bad) + "\\n")
        # A call/jump graph is evidence of direct static consumers only;
        # indirect calls, inlining and hardware execution remain unproven.
        objdump = Path(args.nm).with_name(
            Path(args.nm).name.replace("-nm", "-objdump"))
        disasm = subprocess.check_output(
            [str(objdump), "-d", str(args.build / "firmware.elf")], text=True)
        direct = {name: set() for name in wanted}
        current = None
        for line in disasm.splitlines():
            function = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
            if function:
                current = function[1]
            call = re.search(
                r"\b(?:call[048]|call12|j)\s+[0-9a-f]+ <([^>+]+)(?:\+[^>]*)?>",
                line)
            if call and call[1] in direct and current != call[1]:
                direct[call[1]].add(current)
        print("Native DoomRPG presentation direct callers (no indirect proof):")
        for name, callers in sorted(direct.items()):
            print("  " + name + " <- " +
                  (", ".join(sorted(callers)) if callers else "(none direct)"))
        print("PASS presentation ABI linked=7, generated DoomRPG definitions=0, "
              "original desktop TUs=0")
        return
    if args.require_retired_render:
        if args.compiledb is None:
            parser.error("--require-retired-render needs --compiledb, not stale object presence")
        if (reports["Render"]["admitted"] is not False or reports["Render"]["loaded_sections"] or
                reports["desktop_original_units"] or
                any(s.endswith("/Render.c") for s in reports["admitted_engine_units"])):
            parser.exit(1, "FAIL: desktop Render/original source admitted or linked\n")
        print("PASS Render.c not admitted, no loaded generated Render section; original desktop units=0")
        return
    if args.csv:
        output = io.StringIO()
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(("object", "symbol", "kind", "object_bytes", "linked", "address", "final_bytes"))
        for stem in TARGETS:
            for r in reports[stem]["symbols"]:
                writer.writerow((stem, r["symbol"], r["kind"], r["object_bytes"],
                                 int(r["linked"]), "%08x" % r["address"], r["final_bytes"]))
        print(output.getvalue(), end="")
    else:
        for stem in TARGETS:
            reports[stem]["linked_symbols"] = [r["symbol"] for r in reports[stem].pop("symbols") if r["linked"]]
        if args.calls:
            full = audit(args.build, args.repo.resolve(), args.nm)
            reports["direct_callers"] = direct_callers(args.nm, args.build / "firmware.elf", full)
        print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    main()
