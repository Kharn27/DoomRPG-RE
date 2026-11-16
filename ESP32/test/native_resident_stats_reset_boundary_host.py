#!/usr/bin/env python3
"""ARCH-RC2: compile the real PAK stats-reset owner and explicit I/O hooks on host.

The fixtures prove the exact operation order in INFO and TRACE modes with and
without RA2b capture, compare resulting counters with the former wrapper
contract, and reject reordered/missing hooks or counter resets. This is NOT a
firmware build, full PAK integration, or a substitute for the real CYD log.
"""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
OWNER = ESP / "src/esp_asset_pack.cpp"
PROBE = ESP / "src/esp_asset_pack_perf_probe.cpp"
HEADER = ESP / "include/esp_asset_pack_io_probe.h"
INI = ESP / "platformio.ini"

def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)

def extract(code: str, signature: str) -> str:
    require(code.count(signature) == 1, f"missing/duplicate C ABI: {signature}")
    first = code.index(signature)
    left = code.find("{", first)
    require(left != -1, f"missing function body: {signature}")
    depth = 0
    for index in range(left, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[first:index + 1]
    raise AssertionError(f"unclosed function: {signature}")

def harness(owner_body: str, before_body: str, after_body: str) -> str:
    return "\n".join((
        '#include <assert.h>',
        '#include <stdint.h>',
        '#include <string.h>',
        '#include "esp_asset_pack_io_probe.h"',
        'struct ResidentStats { uint32_t logicalOpens; uint32_t physicalReads; };',
        'ResidentStats residentStats = {};',
        'uint32_t mapFlashRuntimeReadMicros = 0U;',
        'uint32_t ra2bSdRangeMicros = 0U;',
        'int eventCount = 0;',
        'char eventOrder[4] = {};',
        'void reportIoSample(const char* reason) {',
        '  assert(strcmp(reason, "resident-reset") == 0);',
        '  assert(residentStats.logicalOpens == 17U);',
        '  assert(residentStats.physicalReads == 19U);',
        '  assert(mapFlashRuntimeReadMicros == 23U);',
        '  eventOrder[eventCount++] = \'B\';',
        '}',
        'void invalidatePhysicalBaseline() {',
        '  assert(residentStats.logicalOpens == 0U);',
        '  assert(residentStats.physicalReads == 0U);',
        '  assert(mapFlashRuntimeReadMicros == 0U);',
        '  eventOrder[eventCount++] = \'A\';',
        '}',
        before_body,
        after_body,
        owner_body,
        'int main() {',
        '  for (int iteration = 0; iteration < 2; ++iteration) {',
        '    residentStats.logicalOpens = 17U;',
        '    residentStats.physicalReads = 19U;',
        '    mapFlashRuntimeReadMicros = 23U;',
        '    ra2bSdRangeMicros = 29U;',
        '    eventCount = 0;',
        '    memset(eventOrder, 0, sizeof(eventOrder));',
        '    EspAssetPack_residentResetStats();',
        '    assert(residentStats.logicalOpens == 0U);',
        '    assert(residentStats.physicalReads == 0U);',
        '    assert(mapFlashRuntimeReadMicros == 0U);',
        '#if DOOMRPG_RA2B_CAPTURE',
        '    assert(ra2bSdRangeMicros == 0U);',
        '#else',
        '    assert(ra2bSdRangeMicros == 29U);',
        '#endif',
        '#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE',
        '    assert(eventCount == 2);',
        '    assert(strcmp(eventOrder, "BA") == 0);',
        '#else',
        '    assert(eventCount == 0);',
        '#endif',
        '  }',
        '  return 0;',
        '}',
        '',
    ))

def run(source: str, level: int, ra2b: int, expect_ok: bool) -> None:
    cc = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    require(cc is not None, "host C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="arch-rc2-") as folder:
        src = Path(folder) / "probe.cpp"
        exe = Path(folder) / "probe"
        src.write_text(source)
        build = subprocess.run(
            [cc, "-std=c++11", "-Wall", "-Wextra", "-Werror",
             "-I", str(ESP / "include"),
             f"-DDOOMRPG_LOG_TRACE=4", f"-DDOOMRPG_LOG_LEVEL={level}",
             f"-DDOOMRPG_RA2B_CAPTURE={ra2b}",
             str(src), "-o", str(exe)],
            capture_output=True, text=True)
        require(build.returncode == 0, "host compile failed: " + build.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        require((result.returncode == 0) == expect_ok,
                f"wrong host fixture outcome: level={level} ra2b={ra2b} "
                f"expected={expect_ok} return={result.returncode} "
                f"stderr={result.stderr}")

def main() -> None:
    owner = OWNER.read_text()
    probe = PROBE.read_text()
    ini = INI.read_text()
    header = HEADER.read_text()
    reset = extract(owner, "void EspAssetPack_residentResetStats(void)")
    before = extract(probe, "void EspAssetPackIoProbe_beforeResidentStatsReset(void)")
    after = extract(probe, "void EspAssetPackIoProbe_afterResidentStatsReset(void)")
    require("#include \"esp_asset_pack_io_probe.h\"" in owner and
            "#include \"esp_asset_pack_io_probe.h\"" in probe,
            "native observer ABI must be shared")
    require("extern \"C\"" in header and
            "EspAssetPackIoProbe_beforeResidentStatsReset" in header and
            "EspAssetPackIoProbe_afterResidentStatsReset" in header,
            "native C linkage/declarations missing")
    require("EspAssetPackIoProbe_beforeResidentStatsReset();" in reset and
            "EspAssetPackIoProbe_afterResidentStatsReset();" in reset,
            "native owner does not call both hooks")
    require(reset.index("EspAssetPackIoProbe_beforeResidentStatsReset();") <
            reset.index("memset(&residentStats") <
            reset.index("mapFlashRuntimeReadMicros = 0U;") <
            reset.index("EspAssetPackIoProbe_afterResidentStatsReset();"),
            "resident stats reset order changed")
    require('reportIoSample("resident-reset");' in before and
            'invalidatePhysicalBaseline();' in after,
            "TRACE sample/baseline effects changed")
    require(re.search(r"(?m)^\s*-Wl,--wrap=EspAssetPack_residentResetStats\s*$",
                      ini) is None, "retired reset --wrap flag resurrected")
    require("__wrap_EspAssetPack_residentResetStats" not in probe and
            "__real_EspAssetPack_residentResetStats" not in probe,
            "retired linker wrapper/real alias survived")
    golden = harness(reset, before, after)
    for level in (2, 4):
        for ra2b in (0, 1):
            run(golden, level, ra2b, True)
    replacements = (
        ('EspAssetPackIoProbe_beforeResidentStatsReset();', ''),
        ('EspAssetPackIoProbe_afterResidentStatsReset();', ''),
        ('memset(&residentStats, 0, sizeof(residentStats));', ''),
        ('mapFlashRuntimeReadMicros = 0U;', ''),
        ('ra2bSdRangeMicros = 0U;', ''),
    )
    for old, new in replacements:
        require(old in reset, f"fixture source missing: {old}")
        bad = harness(reset.replace(old, new), before, after)
        run(bad, 4, 1, False)
    bad = harness(reset.replace(
        'EspAssetPackIoProbe_beforeResidentStatsReset();',
        'EspAssetPackIoProbe_afterResidentStatsReset();').replace(
        '    EspAssetPackIoProbe_afterResidentStatsReset();',
        '    EspAssetPackIoProbe_beforeResidentStatsReset();', 1),
        before, after)
    run(bad, 4, 1, False)
    print("PASS ARCH-RC2 real-source C++ host: 4 parity modes (INFO/TRACE x "
          "RA2b off/on), 6 adverse reorder/omission fixtures, no link wrapper")

if __name__ == "__main__":
    main()
