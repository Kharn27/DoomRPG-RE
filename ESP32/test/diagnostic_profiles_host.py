#!/usr/bin/env python3
"""Check profile separation and real renderer diagnostic admission, not HW parity."""
import configparser
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
         "-DDOOMRPG_ESP32=1",
         "-Wno-unused-parameter", "-ffunction-sections", "-fdata-sections",
         "-IESP32/test/host_include", "-IESP32/include", "-IESP32/src", "-Isrc"]


def run(args):
    return subprocess.check_output(args, cwd=ROOT, text=True)


def main():
    config = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
    config.read(ROOT / "ESP32/platformio.ini")
    assert {s for s in config if s.startswith("env:")} == {
        "env:esp32-cyd", "env:esp32-cyd-trace", "env:esp32-cyd-bringup"}
    assert config["platformio"]["default_envs"] == "esp32-cyd"
    base = config["env:esp32-cyd"]
    assert base["build_type"] == "release"
    assert base["upload_port"] == base["monitor_port"] == "/dev/ttyUSB0"
    trace = config["env:esp32-cyd-trace"]
    assert trace["extends"] == "env:esp32-cyd"
    trace_flags = trace["build_flags"].split()
    assert trace_flags[:-1] == [
        "${env:esp32-cyd.build_flags}", "-D", "DOOMRPG_LOG_LEVEL=4",
        "-D"]
    selected = re.fullmatch(r"DOOMRPG_ESP32_TRACE_SPRITE=(\d+)", trace_flags[-1])
    assert selected and 0 <= int(selected[1]) <= 65535
    startup = (ROOT / "ESP32/src/esp_legacy_prerender_startup.c").read_text()
    assert "STORY_TEARDOWN_PROBE" not in startup and "DIVIDER_PROBE" not in startup
    renderer = "ESP32/src/esp_native_sprite_renderer.c"
    with tempfile.TemporaryDirectory(prefix="doomrpg-profiles-host-") as temp:
        for name, defines in (("normal", []), ("bringup", ["-DDOOMRPG_LOG_LEVEL=4", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"]),
                              ("trace", ["-DDOOMRPG_LOG_LEVEL=4", "-DDOOMRPG_ESP32_TRACE_SPRITE=0"])):
            run(CC + FLAGS + defines + ["-fsyntax-only", renderer])
            text = run(CC + FLAGS + defines + ["-E", renderer])
            assert ("[SPRITETRACE]" in text) == (name == "trace")
            binary = str(Path(temp) / name)
            run(CC + FLAGS + defines + ["ESP32/test/sprite_trace_host.c",
                                       "-Wl,--gc-sections", "-o", binary])
            output = run([binary])
            assert "PASS" in output
            lines = [line for line in output.splitlines() if line.startswith("[SPRITETRACE]")]
            assert len(lines) == (12 if name == "trace" else 0)
            assert all("sprite=0 " in line for line in lines), lines
        binary = str(Path(temp) / "leaf-cache")
        run(CC + FLAGS + ["ESP32/test/native_bsp_sprite_leaf_cache_host.c",
                          "-Wl,--gc-sections", "-o", binary])
        assert "PASS" in run([binary])
        binary = str(Path(temp) / "startup")
        run(CC + FLAGS + ["-DDOOMRPG_ESP32=1", "ESP32/test/startup_regressions_host.c",
                         "ESP32/src/native_story_fit.c", "ESP32/src/esp_native_text_format.c",
                         "-Wl,--gc-sections", "-o", binary])
        assert "PASS" in run([binary])
    print("diagnostic_profiles_host: PASS profile separation, selected-sprite-only admission, no extra reads, retired startup regressions")


if __name__ == "__main__":
    main()
