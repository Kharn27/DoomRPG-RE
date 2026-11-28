#!/usr/bin/env python3
"""ARCH-RC4: compile the REAL PAK readRange core, public boundary and probe hooks.

This checks the original open/validate/cache/SD/flash/source-probe early returns
under native INFO and TRACE semantics. The TRACE observer must record every
public call, including errors and zero-byte reads; INFO must add no timers.
Negative fixtures alter actual extracted C++ bodies, not substitute pseudocode.
No assets, hardware measurements, or claim of real-CYD execution.
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


def require(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def extract(text: str, signature: str) -> str:
    require(text.count(signature) == 1, "missing/duplicate function " + signature)
    first = text.index(signature)
    left = text.find("{", first)
    require(left >= 0, "missing opening brace: " + signature)
    depth = 0
    for pos in range(left, len(text)):
        if text[pos] == "{":
            depth += 1
        if text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[first:pos + 1]
    raise AssertionError("unclosed function " + signature)


PREFIX = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>
#include "esp_asset_pack.h"
#include "esp_asset_pack_io_probe.h"

constexpr uint32_t kResidentMaxCachedRangeBytes = 1024U;
constexpr uint32_t kResidentLargeRangeBytes = 2048U;
constexpr uint32_t kReportLogicalCallThreshold = 64U;
struct ResidentRangeRecord { uint32_t absoluteOffset; uint16_t length, dataOffset; };
struct ResidentCache { uint8_t bytes[2048]; };
struct PakIoSample { uint32_t logicalCalls; uint64_t logicalMicros; uint32_t maxLogicalMicros; };
PakIoSample ioSample = {};
bool physicalBaselineValid = false;
uint32_t baselineCaptures = 0U, reportCount = 0U, clockReads = 0U;
int64_t esp_timer_get_time() { ++clockReads; return (int64_t)clockReads * 10LL; }
bool observerResident = false;
int EspAssetPack_isResident() { return observerResident ? 1 : 0; }
void capturePhysicalBaseline() { ++baselineCaptures; physicalBaselineValid = true; }
void reportIoSample(const char* reason) {
    assert(strcmp(reason, "threshold") == 0);
    assert(ioSample.logicalCalls == 64U);
    assert(ioSample.logicalMicros == 640U);
    assert(ioSample.maxLogicalMicros == 10U);
    ++reportCount;
    ioSample = {};
}

bool openReady = true, sourceProbeReady = false, sourceProbeReadLogged = false;
uint32_t sourceProbeFlashEpoch = 0U, verifiedFlashEpoch = 1U;
struct { struct { bool verified; } stats; } mapFlash = {{true}};
bool residentEnabled = false, residentLargeRangeEnabled = false;
bool physicalUsesMapFlash = false, haveCache = false, flashOk = true, sdOk = true;
ResidentCache cache = {};
ResidentCache* residentCache = nullptr;
ResidentRangeRecord hit = {20U, 8U, 0U};
EspAssetPackEntry sourceProbeEntry = {};
EspAssetPackResidentStats residentStats = {};
unsigned int flashStrictMisses = 0, storeCount = 0;
bool validateEntry(const EspAssetPackEntry& e) { return e.flags != 7U; }
bool sourceProbeEntryExact(const EspAssetPackEntry& e) {
    return e.nameHash == sourceProbeEntry.nameHash &&
           e.offset == sourceProbeEntry.offset &&
           e.size == sourceProbeEntry.size &&
           e.crc32 == sourceProbeEntry.crc32 &&
           e.flags == sourceProbeEntry.flags;
}
ResidentRangeRecord* findResidentRange(uint32_t absoluteOffset, uint32_t length) {
    return haveCache && absoluteOffset == hit.absoluteOffset && length == hit.length ? &hit : nullptr;
}
void storeResidentSmallRange(uint32_t, const void*, uint32_t) { ++storeCount; }
void storeResidentLargeRange(uint32_t, const void*, uint32_t) { ++storeCount; }
struct FakeFile {
    bool failSeek = false, failRead = false;
    unsigned int seeks = 0, reads = 0;
    bool seek(uint32_t) { ++seeks; return !failSeek; }
    size_t read(uint8_t* destination, size_t length) {
        ++reads;
        if (failRead) return 0U;
        memset(destination, 0x5a, length);
        return length;
    }
} sourceProbeFile, packFile;
bool readSdExact(void* destination, size_t length) {
    if (!sdOk) return false;
    memset(destination, 0x5b, length);
    return true;
}
bool mapFlashTranslate(uint32_t offset, uint32_t, uint32_t* out) {
    *out = offset;
    return true;
}
bool mapFlashRead(uint32_t, void* destination, size_t length, bool) {
    if (!flashOk) return false;
    memset(destination, 0x5c, length);
    return true;
}
void logMapFlashStrictMiss(const EspAssetPackEntry*, uint32_t, size_t) {
    ++flashStrictMisses;
}

void check(int expected, const EspAssetPackEntry* entry, uint32_t offset,
           void* destination, size_t length) {
    const uint32_t clocks = clockReads, calls = ioSample.logicalCalls;
    const uint32_t reports = reportCount;
    const int actual = EspAssetPack_readRange(entry, offset, destination, length);
    assert(actual == expected);
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    assert(clockReads == clocks + 2U);
    if (calls + 1U == 64U) {
        assert(reportCount == reports + 1U);
        assert(ioSample.logicalCalls == 0U);
    } else {
        assert(reportCount == reports);
        assert(ioSample.logicalCalls == calls + 1U);
        assert(ioSample.maxLogicalMicros == 10U);
    }
#else
    assert(clockReads == clocks);
    assert(reportCount == reports);
    assert(ioSample.logicalCalls == calls);
#endif
}

int main() {
    uint8_t bytes[16] = {};
    EspAssetPackEntry entry = {0x1234U, 20U, 16U, 0xaabbccddU, 0U};
    openReady = false;
    check(0, &entry, 0U, bytes, 1U);  // failed lease still metered
    openReady = true;
    check(0, nullptr, 0U, bytes, 1U);
    check(0, &entry, 0U, nullptr, 1U);
    check(0, &entry, 17U, bytes, 1U);
    check(0, &entry, 15U, bytes, 2U);
    entry.flags = 7U;
    check(0, &entry, 0U, bytes, 1U);  // failed validation
    entry.flags = 0U;
    sdOk = false;  // zero-length success must bypass physical SD failure
    check(1, &entry, 0U, bytes, 0U);  // zero length still metered
    sdOk = true;

    // The immutable cache hit is never read from raw flash or SD.
    residentEnabled = true;
    residentCache = &cache;
    haveCache = true;
    observerResident = true;
    memset(cache.bytes, 0x44, sizeof(cache.bytes));
    check(1, &entry, 0U, bytes, 8U);
    assert(residentStats.rangeCacheHits == 1U && bytes[0] == 0x44);
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    assert(baselineCaptures == 1U);
#else
    assert(baselineCaptures == 0U);
#endif

    haveCache = false;
    physicalUsesMapFlash = true;
    flashOk = false;
    check(0, &entry, 0U, bytes, 8U);  // strict raw-flash fail, no SD fallback
    assert(flashStrictMisses == 1U);
    flashOk = true;
    check(1, &entry, 0U, bytes, 8U);
    assert(bytes[0] == 0x5c);

    // Unselected source-probe reads use authoritative SD; P3 may select flash.
    sourceProbeReady = true;
    sourceProbeEntry = entry;
    sourceProbeEntry.offset = 88U;
    check(0, &entry, 0U, bytes, 1U);  // mismatched same-hash entry refused
    sourceProbeEntry = entry;
    sourceProbeFile.failSeek = true;
    check(0, &entry, 0U, bytes, 1U);
    sourceProbeFile.failSeek = false;
    check(1, &entry, 0U, bytes, 1U);
    assert(bytes[0] == 0x5a && sourceProbeFile.reads == 1U);

    sourceProbeFlashEpoch = verifiedFlashEpoch;
    check(1, &entry, 0U, bytes, 1U);
    assert(bytes[0] == 0x5c && sourceProbeFile.reads == 1U);
    flashOk = false;
    check(0, &entry, 0U, bytes, 1U);
    assert(sourceProbeFile.reads == 1U);
    flashOk = true;
    sourceProbeFlashEpoch = 0U;

    // Normal SD failure/success after the source-probe scope is released.
    sourceProbeReady = false;
    physicalUsesMapFlash = false;
    residentEnabled = false;
    residentCache = nullptr;
    observerResident = false;
    sdOk = false;
    check(0, &entry, 0U, bytes, 1U);
    sdOk = true;
    check(1, &entry, 0U, bytes, 1U);
    assert(bytes[0] == 0x5b);

    // Complete the first 64 logical calls with empty successful reads.
    // The threshold must flush once, then next call starts a new sample.
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    while (ioSample.logicalCalls < 63U) {
        check(1, &entry, 0U, bytes, 0U);
    }
    check(1, &entry, 0U, bytes, 0U);
    assert(reportCount == 1U);
    check(1, &entry, 0U, bytes, 0U);
    assert(ioSample.logicalCalls == 1U && reportCount == 1U);
#else
    check(1, &entry, 0U, bytes, 0U);
    assert(reportCount == 0U && clockReads == 0U);
#endif
    return 0;
}
'''


def harness(core: str, public: str, before: str, after: str,
            elapsed: str) -> str:
    # Forward-declare the public C entry before check() in PREFIX.
    anchor = 'void check(int expected, const EspAssetPackEntry* entry'
    header = ('int EspAssetPack_readRange(const EspAssetPackEntry*, '
              'uint32_t, void*, size_t);\n')
    stub = PREFIX.replace(anchor, header + anchor)
    # Actual probe uses the real elapsed helper from perf_probe, not a mock.
    # The two source-defined probe functions need C ABI (header declares it).
    return "\n".join((stub.split('void check(', 1)[0],
                      elapsed, before, after,
                      extract(OWNER.read_text(), "bool readSourceProbePayload("), core, public,
                      'void check(' + stub.split('void check(', 1)[1]))


def run(source: str, level: int, expect_ok: bool) -> None:
    cc = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    require(cc is not None, "host C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="arch-rc4-") as tmp:
        src = Path(tmp) / "real.cpp"
        exe = Path(tmp) / "real"
        src.write_text(source)
        build = subprocess.run(
            [cc, "-std=c++11", "-Wall", "-Wextra", "-Werror",
             "-I", str(ESP / "include"), "-DDOOMRPG_LOG_TRACE=4",
             f"-DDOOMRPG_LOG_LEVEL={level}", "-DDOOMRPG_RA2B_CAPTURE=0",
             str(src), "-o", str(exe)],
            capture_output=True, text=True)
        if build.returncode != 0:
            require(not expect_ok, "real C++ compile failure: " + build.stderr)
            return
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        require((result.returncode == 0) == expect_ok,
                f"parity or negative case mismatch level={level} rc="
                f"{result.returncode} stderr={result.stderr}")


def main() -> None:
    owner = OWNER.read_text()
    probe = PROBE.read_text()
    header = HEADER.read_text()
    ini = INI.read_text()
    core = extract(owner, "static int readRangeCore(")
    public = extract(owner, "int EspAssetPack_readRange(")
    before = extract(probe, "int64_t EspAssetPackIoProbe_beforeReadRange(void)")
    after = extract(probe, "void EspAssetPackIoProbe_afterReadRange(int64_t start)")
    elapsed = extract(probe, "uint32_t elapsedMicros(int64_t start, int64_t end)")

    require('extern "C"' in header and
            "int64_t EspAssetPackIoProbe_beforeReadRange(void);" in header and
            "void EspAssetPackIoProbe_afterReadRange(int64_t start);" in header,
            "lost permanent C ABI")
    require('#include "doomrpg_log.h"' in owner and
            '#include "esp_asset_pack_io_probe.h"' in owner,
            "public owner missing native observer/log-level header")
    require(public.index("EspAssetPackIoProbe_beforeReadRange()") <
            public.index("readRangeCore(") <
            public.index("EspAssetPackIoProbe_afterReadRange(ioStart)") <
            public.index("return result;"), "read instrumentation not paired")
    require(public.count("readRangeCore(") == 1 and
            "#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE" in public,
            "public read wrapper must call one core and compile INFO cleanly")
    for token in ("if (!openReady", "if (!validateEntry(", "length == 0U",
                  "sourceProbeEntryExact", "findResidentRange(",
                  "readSourceProbePayload(", "mapFlashTranslate(",
                  "mapFlashRead(", "readSdExact(", "storeResidentSmallRange(",
                  "storeResidentLargeRange("):
        require(token in core, "PAK core contract erased: " + token)
    require(re.search(r"(?m)^\s*-Wl,--wrap=EspAssetPack_readRange\s*$",
                      ini) is None, "retired --wrap flag returned")
    require("__wrap_EspAssetPack_readRange" not in probe and
            "__real_EspAssetPack_readRange" not in probe,
            "GNU ld alias still survives")
    require(before.index("capturePhysicalBaseline()") <
            before.index("return esp_timer_get_time()"), "baseline too late")
    require(after.index("++ioSample.logicalCalls") <
            after.index("reportIoSample(\"threshold\")"), "threshold too early")
    golden = harness(core, public, before, after, elapsed)
    for level in (2, 4):
        run(golden, level, True)
    bad = [
        (core, public.replace("EspAssetPackIoProbe_beforeReadRange();", "0LL;"),
         before, after),
        (core, public.replace("EspAssetPackIoProbe_afterReadRange(ioStart);", ""),
         before, after),
        (core, public.replace("readRangeCore(entry, relativeOffset, destination, length)",
                              "1"), before, after),
        (core.replace("!openReady", "false", 1), public, before, after),
        (core.replace("length == 0U", "false", 1), public, before, after),
        (core.replace("!sourceProbeExact", "false", 1), public, before, after),
        (core, public, before.replace("capturePhysicalBaseline();", ""),
         after),
        (core, public, before, after.replace("++ioSample.logicalCalls;", "")),
        (core, public, before,
         after.replace("ioSample.logicalMicros += duration;", "")),
        (core, public, before,
         after.replace("if (duration > ioSample.maxLogicalMicros)",
                       "if (false)")),
        (core, public, before,
         after.replace("ioSample.logicalCalls >= kReportLogicalCallThreshold",
                       "ioSample.logicalCalls > kReportLogicalCallThreshold")),
    ]
    for negative_index, (c, p, b, a) in enumerate(bad, 1):
        require((c, p, b, a) != (core, public, before, after),
                "no-op adversarial mutation")
        try:
            run(harness(c, p, b, a, elapsed), 4, False)
        except AssertionError as error:
            raise AssertionError(f"negative fixture {negative_index}: {error}") from error
    print("PASS ARCH-RC4 real-source C++: INFO/TRACE 17 read paths + 64-call "
          "threshold/reset; 11 adversarial omissions/bypasses, PAK core retained")


if __name__ == "__main__":
    main()
