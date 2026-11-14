#!/usr/bin/env python3
"""SDL-F30: verify the *actual compiler-produced* ESP32 header closure.

F29 handled direct SDL.h imports. F30 refuses transitive SDL.h / SDL_Video.h
imports in any ESP32/src object across the three real PlatformIO profiles.
Use each object's GCC .d dependency file rather than grep source text or
infer from the absence of linked SDL functions. No firmware source is changed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "ESP32"
OWNERS = PROJECT / "src"
PROFILES = ("esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace")
EXCLUDED = {"esp32_sdl.cpp", "esp32_bmp.cpp"}
FORBIDDEN_HEADERS = {"SDL.h", "SDL_Video.h"}


def object_for(entry):
    """Resolve the real object from a compiledb entry without a build guess."""
    command = entry.get("arguments")
    if command is None:
        command = shlex.split(entry["command"])
    for i, part in enumerate(command):
        if part == "-o" and i + 1 < len(command):
            dest = Path(command[i + 1])
            return (Path(entry["directory"]) / dest).resolve()
        if part.startswith("-o") and len(part) > 2:
            return (Path(entry["directory"]) / part[2:]).resolve()
    raise AssertionError("compiledb entry without output -o: " + entry["file"])


def dependency_for(obj):
    # GCC/SCons normally emits foo.c.o.d; support either spelling for hosts.
    candidates = (Path(str(obj) + ".d"), obj.with_suffix(".d"))
    present = [path for path in candidates if path.is_file()]
    assert present, f"missing GCC dependency evidence for {obj}"
    return present[0]


def dependencies(depfile):
    """Read GCC makefile .d dependency paths, including line continuations.

    The generated dependency list has one first target, then header paths;
    GCC -MP may also add empty header targets. A forbidden filename in any
    target is still a positive witness of an included header.
    """
    raw = depfile.read_text(encoding="utf-8")
    raw = raw.replace("\\\n", " ")
    # Escaped spaces in paths aren't normal on CI, but don't split them.
    return [token.replace("\\ ", " ") for token in
            re.findall(r"(?:\\ |[^\s])+", raw)]


def forbidden(deps):
    hits = []
    for item in deps:
        name = Path(item.rstrip(":")).name
        if name in FORBIDDEN_HEADERS:
            hits.append(item.rstrip(":"))
    return hits


def owned_entries(database):
    entries = []
    for item in database:
        source = (Path(item["directory"]) / item["file"]).resolve()
        if OWNERS not in source.parents:
            continue
        assert source.suffix in (".c", ".cpp", ".cc"), source
        entries.append((source, object_for(item)))
    return entries


def audit_profile(profile):
    assert profile in PROFILES
    database_path = PROJECT / "compile_commands.json"
    build = (PROJECT / ".pio/build" / profile).resolve()
    assert build.joinpath("firmware.elf").is_file(), profile
    database = json.loads(database_path.read_text(encoding="utf-8"))
    sources = owned_entries(database)
    assert len(sources) >= 100, f"{profile}: vacuous native source coverage {len(sources)}"
    names = {source.name for source, _ in sources}
    assert not (names & EXCLUDED), f"{profile}: retired shim/BMP compiled: {names & EXCLUDED}"
    assert {"main.cpp", "native_main_menu_actions.c", "esp_native_first_frame.c"} <= names
    objects = set()
    witnesses = 0
    for source, obj in sources:
        assert obj not in objects, f"duplicate compile object: {obj}"
        objects.add(obj)
        assert build == obj or build in obj.parents, (
            f"{profile}: compiledb object not in profile: {obj}")
        dep = dependency_for(obj)
        tokens = dependencies(dep)
        assert tokens and source.name in dep.read_text(encoding="utf-8"), (
            f"{profile}: dependency list not tied to {source.name}: {dep}")
        hits = forbidden(tokens)
        assert not hits, (
            f"{profile}: transitive SDL header in real compiler deps of "
            f"{source.name}: {hits} ({dep})")
        witnesses += 1
    print(f"SDL-F30 PASS {profile}: {len(sources)} actual ESP32/src TUs, "
          f"{witnesses} compiler .d closures, 0 SDL.h/SDL_Video.h; "
          "legacy shim/BMP excluded, three-profile ELF audit remains")


def self_test():
    with tempfile.TemporaryDirectory(prefix="sdl-f30-dep-") as tmp:
        root = Path(tmp)
        sample = root / "main.cpp.o.d"
        sample.write_text("obj.o: src/main.cpp \\\n /tmp/ESP32/include/Sound.h \\\n /tmp/ESP32/include/SDL_mixer.h\n")
        assert not forbidden(dependencies(sample))
        sample.write_text("obj.o: src/main.cpp \\\n /tmp/ESP32/include/SDL.h \\\n /tmp/ESP32/include/SDL_Video.h\n")
        assert len(forbidden(dependencies(sample))) == 2
        sample.write_text("obj.o: src/main.cpp /tmp/with\\ space/SDL.h\n")
        assert len(forbidden(dependencies(sample))) == 1
        sample.write_text("obj.o: src/main.cpp /tmp/ESP32/include/SDL.h\n"
                          "/tmp/ESP32/include/SDL.h:\n")
        assert len(forbidden(dependencies(sample))) >= 1
        entry = {"directory": str(root), "file": "src/main.cpp",
                 "command": "cc -c src/main.cpp -o main.cpp.o"}
        assert object_for(entry) == root / "main.cpp.o"
        assert dependency_for(object_for(entry)) == sample
        entry["arguments"] = ["cc", "-c", "src/main.cpp", "-omain.cpp.o"]
        assert object_for(entry) == root / "main.cpp.o"
    print("SDL-F30 PASS adversarial .d parsing: clean opaque mixer, nested "
          "SDL.h, SDL_Video.h, escaped paths, -MP and compile object mapping")



def zip_header_boundary():
    """Native root must not import the dead ZIP header and its SDL.h edge.

    Inverting the one site-local guard must recover the exact pre-F30
    engine_stubs.c Git blob; host and geometry still include the same ZIP ABI.
    """
    source = (PROJECT / "src/engine_stubs.c").read_bytes()
    original_include = b'#include "Z_Zip.h"\n'
    guard = (b"#if !defined(DOOMRPG_ESP32) || "
             b"defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
             + original_include + b"#endif\n")
    assert source.count(guard) == 1, "native ZIP import not guarded exactly once"
    restored = source.replace(guard, original_include)
    blob = hashlib.sha1(
        b"blob " + str(len(restored)).encode() + b"\0" + restored
    ).hexdigest()
    assert blob == "fbec028fc332d4b1bd8bf516f1676757a74dd5ad", (
        "engine_stubs.c changed outside the ZIP include guard")
    assert source.count(original_include) == 1
    assert '#include <SDL.h>' in (ROOT / "src/Z_Zip.h").read_text()
    print("SDL-F30 PASS engine_stubs.c: exact pre-F30 Git body, "
          "native ZIP/SDL import isolated; desktop/geometry preserved")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=PROFILES)
    args = parser.parse_args()
    zip_header_boundary()
    if args.profile:
        audit_profile(args.profile)
    else:
        self_test()


if __name__ == "__main__":
    main()
