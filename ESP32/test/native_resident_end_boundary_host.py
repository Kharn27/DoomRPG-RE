#!/usr/bin/env python3
"""ARCH-RC3: real-source C++ parity for the PAK residentEnd pre-close observer.

The former GNU ld --wrap called TRACE reportIoSample("resident-end") and
invalidatePhysicalBaseline() BEFORE __real_EspAssetPack_residentEnd(), even
when an open PAK lease made the real owner return 0. Extract real C++ function
bodies rather than restating their implementation in the test. No CYD emulator,
RAM measurements, full PAK integration or substitute for hardware Serial.
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
    require(left >= 0, f"missing function body: {signature}")
    depth = 0
    for index in range(left, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[first:index + 1]
    raise AssertionError(f"unclosed C++ function: {signature}")


def harness(owner: str, before: str) -> str:
    return "\n".join((
        '#include <assert.h>',
        '#include <stdint.h>',
        '#include <string.h>',
        '#include "esp_asset_pack_io_probe.h"',
        'struct ResidentCache { uint8_t witness; };',
        'ResidentCache fakeCache = {};',
        'ResidentCache* residentCache = nullptr;',
        'bool openReady = false;',
        'bool residentLargeRangeEnabled = true;',
        'bool residentEnabled = true;',
        'bool physicalBaselineValid = true;',
        'int reports = 0, invalidates = 0, resets = 0, frees = 0, clears = 0;',
        'char order[16] = {};',
        'int orderSize = 0;',
        'void append(char c) {',
        '  assert(orderSize < 15);',
        '  order[orderSize++] = c;',
        '  order[orderSize] = 0;',
        '}',
        'void reportIoSample(const char* why) {',
        '  assert(strcmp(why, "resident-end") == 0);',
        '  assert(physicalBaselineValid && residentEnabled);',
        '  assert(resets == 0 && frees == 0 && clears == 0);',
        '  append(\'S\');',
        '  ++reports;',
        '}',
        'void invalidatePhysicalBaseline() {',
        '  assert(reports == 1 && physicalBaselineValid);',
        '  assert(resets == 0 && frees == 0 && clears == 0);',
        '  physicalBaselineValid = false;',
        '  append(\'I\');',
        '  ++invalidates;',
        '}',
        'void resetPhysicalState() {',
        '  assert(!residentLargeRangeEnabled && !residentEnabled);',
        '#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE',
        '  assert(!physicalBaselineValid && reports == 1 && invalidates == 1);',
        '#else',
        '  assert(physicalBaselineValid && reports == 0 && invalidates == 0);',
        '#endif',
        '  append(\'R\');',
        '  ++resets;',
        '}',
        'void trackedFree(void* ptr) {',
        '  assert(ptr == (void*)&fakeCache && resets == 1 && frees == 0);',
        '  append(\'F\');',
        '  ++frees;',
        '}',
        'void clearMapFlashRuntime(bool clearStats) {',
        '  assert(clearStats && resets == 1 && residentCache == nullptr);',
        '  append(\'C\');',
        '  ++clears;',
        '}',
        before,
        '#define free trackedFree',
        owner,
        '#undef free',
        'int main() {',
        '  for (int reject = 0; reject < 2; ++reject) {',
        '    for (int withCache = 0; withCache < 2; ++withCache) {',
        '      openReady = reject != 0;',
        '      residentLargeRangeEnabled = true;',
        '      residentEnabled = true;',
        '      physicalBaselineValid = true;',
        '      residentCache = withCache ? &fakeCache : nullptr;',
        '      reports = invalidates = resets = frees = clears = orderSize = 0;',
        '      memset(order, 0, sizeof(order));',
        '      const int result = EspAssetPack_residentEnd();',
        '      assert(result == (reject ? 0 : 1));',
        '#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE',
        '      assert(reports == 1 && invalidates == 1 && !physicalBaselineValid);',
        '#else',
        '      assert(reports == 0 && invalidates == 0 && physicalBaselineValid);',
        '#endif',
        '      if (reject) {',
        '        assert(openReady && residentLargeRangeEnabled && residentEnabled);',
        '        assert(resets == 0 && frees == 0 && clears == 0);',
        '        assert(residentCache == (withCache ? &fakeCache : nullptr));',
        '#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE',
        '        assert(strcmp(order, "SI") == 0);',
        '#else',
        '        assert(strcmp(order, "") == 0);',
        '#endif',
        '      } else {',
        '        assert(!residentLargeRangeEnabled && !residentEnabled);',
        '        assert(residentCache == nullptr);',
        '        assert(resets == 1 && clears == 1 && frees == withCache);',
        '#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE',
        '        assert(strcmp(order, withCache ? "SIRFC" : "SIRC") == 0);',
        '#else',
        '        assert(strcmp(order, withCache ? "RFC" : "RC") == 0);',
        '#endif',
        '      }',
        '    }',
        '  }',
        '  return 0;',
        '}',
        '',
    ))


def run(source: str, level: int, expect_ok: bool) -> None:
    cc = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    require(cc is not None, "host C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="arch-rc3-") as folder:
        src = Path(folder) / "probe.cpp"
        exe = Path(folder) / "probe"
        src.write_text(source)
        build = subprocess.run(
            [cc, "-std=c++11", "-Wall", "-Wextra", "-Werror",
             "-I", str(ESP / "include"), "-DDOOMRPG_LOG_TRACE=4",
             f"-DDOOMRPG_LOG_LEVEL={level}", str(src), "-o", str(exe)],
            capture_output=True, text=True)
        require(build.returncode == 0, "host compile failed: " + build.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        require((result.returncode == 0) == expect_ok,
                f"wrong fixture outcome: level={level}, expected={expect_ok}, "
                f"rc={result.returncode}, stderr={result.stderr}")


def main() -> None:
    owner = OWNER.read_text()
    probe = PROBE.read_text()
    header = HEADER.read_text()
    ini = INI.read_text()
    end = extract(owner, "int EspAssetPack_residentEnd(void)")
    hook = extract(probe, "void EspAssetPackIoProbe_beforeResidentEnd(void)")
    call = "    EspAssetPackIoProbe_beforeResidentEnd();"
    require("#include \"esp_asset_pack_io_probe.h\"" in owner and
            "#include \"esp_asset_pack_io_probe.h\"" in probe,
            "owner/observer are not using shared permanent native ABI")
    require("extern \"C\"" in header and
            "void EspAssetPackIoProbe_beforeResidentEnd(void);" in header,
            "permanent close observer lacks C linkage")
    require(end.count(call) == 1 and
            end.index(call) < end.index("if (openReady)") and
            end.index("if (openReady)") < end.index("resetPhysicalState();"),
            "beforeResidentEnd must run ahead of the openReady failure gate")
    require('reportIoSample("resident-end");' in hook and
            'invalidatePhysicalBaseline();' in hook and
            hook.index('reportIoSample("resident-end");') <
            hook.index('invalidatePhysicalBaseline();'),
            "TRACE flush/baseline order changed")
    require(re.search(r"(?m)^\s*-Wl,--wrap=EspAssetPack_residentEnd\s*$",
                      ini) is None, "retired residentEnd linker flag returned")
    require("__wrap_EspAssetPack_residentEnd" not in probe and
            "__real_EspAssetPack_residentEnd" not in probe,
            "retired close wrapper or __real_ alias survives")

    for level in (2, 4):
        run(harness(end, hook), level, True)
    bad_owner = [
        end.replace(call, ""),
        end.replace(call, "").replace(
            "    residentLargeRangeEnabled = false;",
            call + "\n    residentLargeRangeEnabled = false;"),
        end.replace(call, "").replace(
            "    resetPhysicalState();",
            "    resetPhysicalState();\n" + call),
    ]
    for token in ("residentLargeRangeEnabled = false;",
                  "residentEnabled = false;", "resetPhysicalState();",
                  "free(residentCache);", "residentCache = nullptr;",
                  "clearMapFlashRuntime(true);"):
        require(token in end, f"negative fixture missing: {token}")
        bad_owner.append(end.replace(token, "", 1))
    bad_hooks = [
        hook.replace('reportIoSample("resident-end");', ""),
        hook.replace("invalidatePhysicalBaseline();", ""),
        hook.replace('    reportIoSample("resident-end");',
                     '    __RC3_SWAP__').replace(
            '    invalidatePhysicalBaseline();',
            '    reportIoSample("resident-end");').replace(
            '    __RC3_SWAP__', '    invalidatePhysicalBaseline();'),
    ]
    for bad in bad_owner:
        run(harness(bad, hook), 4, False)
    for bad in bad_hooks:
        run(harness(end, bad), 4, False)
    print("PASS ARCH-RC3 real-source C++ host: 8 close-state parity cases "
          "(INFO/TRACE x openReady x cache), 12 adverse omission/order "
          "fixtures, no residentEnd linker wrapper")


if __name__ == "__main__":
    main()
