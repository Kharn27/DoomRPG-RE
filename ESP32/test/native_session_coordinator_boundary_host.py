#!/usr/bin/env python3
"""ARCH-RC9: permanent session coordinator callsite and fail-closed ownership.

Source audit scans EVERY actual firmware C/C++ TU, not a curated caller list.
A separate RC8 oracle in the same workflow compiles the unchanged actual outer
C service/reset bodies and rejects 12 behavior-changing mutants.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
CONFIG = ROOT / "platformio.ini"
HEADER = ROOT / "include/esp_native_gameplay_session.h"

EXPECTED_COMPOSED = {
    "main.cpp": ("service",),
    "esp_native_gameplay_save.cpp": ("reset",),
    "esp_native_resident_gameplay.c": ("reset",),
    "esp_native_gameplay_transition_handoff.c": ("reset",),
    "esp_native_startup.c": ("reset", "reset"),
    "esp_native_gameplay_gib_fx.c": ("service", "reset"),
}
EXPECTED_RAW = {
    "esp_native_gameplay_session.c": ("service", "reset"),
    "esp_native_gameplay_player_resources.c": ("service", "reset"),
}
CALL = re.compile(r"\bEspNativeGameplaySession_(service|reset)\s*\(")
COMPOSED = re.compile(r"\bEspNativeGameplaySessionCoordinator_(service|reset)\s*\(")

def require(ok: bool, detail: str) -> None:
    if not ok:
        raise AssertionError(detail)

def source_code(value: str) -> str:
    # Exclude documentation and literal strings; never mistake comments for
    # real linker callsites, but keep the bodies of real C and C++ functions.
    value = re.sub(r"/\*[\s\S]*?\*/", "", value)
    value = re.sub(r"//[^\n]*", "", value)
    value = re.sub(r'"(?:\\.|[^"\\])*"', '""', value)
    return value

def scan(files: dict[str, str], ini: str, header: str) -> None:
    production = [name for name in files if name.endswith((".c", ".cpp"))]
    require(len(production) >= 150, "not scanning the complete firmware source tree")
    require(ini.count("-Wl,--wrap=EspNativeGameplaySession_service") == 0,
            "outer service linker interposition returned")
    require(ini.count("-Wl,--wrap=EspNativeGameplaySession_reset") == 0,
            "outer reset linker interposition returned")
    release_flags = re.findall(r"^\s+-Wl,--wrap=([^\n\r\s]+)", ini, re.M)
    require(len(release_flags) >= 15,
            "unexpectedly small linker inventory")
    for name in ("service", "reset"):
        declaration = f"void EspNativeGameplaySessionCoordinator_{name}("
        require(header.count(declaration) == 1,
                f"public coordinator declaration absent/duplicated: {name}")
        require(header.count(f"void EspNativeGameplaySession_{name}(") == 1,
                f"raw leaf signature drifted: {name}")

    observed_composed, observed_raw = {}, {}
    for name in production:
        code = source_code(files[name])
        # configure is still an independent, retained linker boundary:
        # retire ONLY the two session service/reset interpositions.
        require(not re.search(r"\b__wrap_EspNativeGameplaySession_(?:service|reset)\s*\(", code),
                "retired service/reset wrapper source left in " + name)
        require(not re.search(r"\b__real_EspNativeGameplaySession_(?:service|reset)\s*\(", code),
                "retired service/reset GNU ld alias source left in " + name)
        composed = tuple(sorted(COMPOSED.findall(code)))
        raw = tuple(sorted(CALL.findall(code)))
        if composed:
            observed_composed[name] = composed
        if raw:
            observed_raw[name] = raw
    need_composed = {p: tuple(sorted(v)) for p,v in EXPECTED_COMPOSED.items()}
    need_raw = {p: tuple(sorted(v)) for p,v in EXPECTED_RAW.items()}
    require(observed_composed == need_composed,
            f"coordinator bypass/additional callsite: got {observed_composed}, want {need_composed}")
    require(observed_raw == need_raw,
            f"raw leaf callers not exclusive: got {observed_raw}, want {need_raw}")

    fx = source_code(files["esp_native_gameplay_gib_fx.c"])
    for n in ("service", "reset"):
        require(fx.count(f"void EspNativeGameplaySessionCoordinator_{n}(") == 1,
                "coordinator missing C body " + n)
    require(files["esp_native_gameplay_player_resources.c"].count(
            "    EspNativeGameplaySession_service(doomRpg);") == 1,
            "resource service must call the raw leaf exactly once")
    require(files["esp_native_gameplay_player_resources.c"].count(
            "    EspNativeGameplaySession_reset();") == 1,
            "resource reset must call the raw leaf exactly once")
    require(files["main.cpp"].count(
            "        EspNativeGameplaySessionCoordinator_service(doomRpg);") == 1,
            "main loop must retain one guarded service")
    require("if (EspNativeGameplaySession_canService())" in files["main.cpp"],
            "main loop service admission gate removed")

def main() -> None:
    files = {p.name: p.read_text(encoding="utf-8") for p in SRC.iterdir()
             if p.suffix in (".c", ".cpp")}
    config = CONFIG.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    scan(files, config, header)
    print(f"PASS ARCH-RC9: {len(files)} real C/C++ sources scanned; "
          "six composed source owners / two raw leaf roles / 2 outer bodies")

    def clone(changes: dict[str, tuple[str,str]]) -> dict[str,str]:
        mutated = dict(files)
        for name,(old,new) in changes.items():
            require(mutated[name].count(old) == 1,
                    "mutant anchor count " + name + " " + old)
            mutated[name] = mutated[name].replace(old,new,1)
        return mutated

    service = "EspNativeGameplaySessionCoordinator_service(doomRpg);"
    reset = "EspNativeGameplaySessionCoordinator_reset();"
    raw_service = "EspNativeGameplaySession_service(doomRpg);"
    raw_reset = "EspNativeGameplaySession_reset();"
    mutants = [
        ("main bypass raw", clone({"main.cpp":(service,raw_service)}),config,header),
        ("main service missing", clone({"main.cpp":(service,";")}),config,header),
        ("main service twice", clone({"main.cpp":(service,service+service)}),config,header),
        ("save raw reset", clone({"esp_native_gameplay_save.cpp":(reset,raw_reset)}),config,header),
        ("save reset missing", clone({"esp_native_gameplay_save.cpp":(reset,";")}),config,header),
        ("exit raw reset", clone({"esp_native_resident_gameplay.c":(reset,raw_reset)}),config,header),
        ("exit reset missing", clone({"esp_native_resident_gameplay.c":(reset,";")}),config,header),
        ("handoff raw reset", clone({"esp_native_gameplay_transition_handoff.c":(reset,raw_reset)}),config,header),
        ("handoff reset missing", clone({"esp_native_gameplay_transition_handoff.c":(reset,";")}),config,header),
        ("startup initial reset bypass",
         clone({"esp_native_startup.c":(
            "    EspNativeGameplaySessionCoordinator_reset();\n    EspMapResidentLifecycle_resetAll();\n    resetSpawnOwners();\n    memset(&startupState",
            "    EspNativeGameplaySession_reset();\n    EspMapResidentLifecycle_resetAll();\n    resetSpawnOwners();\n    memset(&startupState")}),config,header),
        ("death route reset bypass",
         clone({"esp_native_startup.c":(
             "    PlatformInput_setTapCallback(NULL);\n    EspNativeGameplaySessionCoordinator_reset();",
             "    PlatformInput_setTapCallback(NULL);\n    EspNativeGameplaySession_reset();")}),config,header),
        ("resource recursive", clone({"esp_native_gameplay_player_resources.c":(raw_service,service)}),config,header),
        ("resource reset recursive", clone({"esp_native_gameplay_player_resources.c":(raw_reset,reset)}),config,header),
        ("decl drop", files,config,header.replace("void EspNativeGameplaySessionCoordinator_reset(void);","")),
        ("return GNU wrap", files,config+"\n-Wl,--wrap=EspNativeGameplaySession_service\n",header),
        ("return GNU reset", files,config+"\n-Wl,--wrap=EspNativeGameplaySession_reset\n",header),
    ]
    for name, candidate, ini, declarations in mutants:
        try:
            scan(candidate,ini,declarations)
        except AssertionError:
            pass
        else:
            raise AssertionError("RC9 adversarial mutant survived: " + name)
    print("PASS ARCH-RC9 16/16 adversarial route/alias mutations refused")

if __name__ == "__main__":
    main()
