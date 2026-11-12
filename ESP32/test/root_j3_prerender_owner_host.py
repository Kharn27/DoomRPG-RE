#!/usr/bin/env python3
"""Root-J3: pre-render menu startup receives a process token from its caller.

Exact Git-blob inverse parity on all changed C/C++/header files, followed by
4096 executions of the extracted production C function with two distinct roots,
fail-closed PAK/menu checks and idempotent startup behavior.
"""
from pathlib import Path
import hashlib
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE_BLOBS = {
    "ESP32/src/esp_legacy_prerender_startup.c":
        "0b9cc3247a54690d179175db340f392eef9ee1a1",
    "ESP32/include/esp_legacy_prerender_startup.h":
        "1140adff11d98b1e9989ae755852c4485c89e54f",
    "ESP32/src/main.cpp":
        "bf9cd302e4d3d45203e7e602a2f9fdb206eb2180",
}
files = {path: (ROOT / path).read_text(encoding="utf-8") for path in BASE_BLOBS}
source = files["ESP32/src/esp_legacy_prerender_startup.c"]
header = files["ESP32/include/esp_legacy_prerender_startup.h"]
main = files["ESP32/src/main.cpp"]

assert "extern DoomRPG_t* doomRpg;" not in source
assert "int EspLegacyPrerenderStartup_start(DoomRPG_t* root, int layoutReady) {" in source
assert "if (root == NULL || EspNativeRootOwners_live()->menuSystem == NULL)" in source
assert "EspNativeMenuStorage_startup(EspNativeRootOwners_live()->menuSystem, root)" in source
assert "int EspLegacyPrerenderStartup_start(struct DoomRPG_s* root, int layoutReady);" in header
assert "EspLegacyPrerenderStartup_start(doomRpg, engineLayoutReady ? 1 : 0) != 0;" in main
assert main.count("EspLegacyPrerenderStartup_start(") == 1
assert source.count("EspLegacyPrerenderStartup_start(") == 1

# Restore exactly the pre-Root-J3 Git blobs, proving this is the ONLY change
# to the original production code, main boot and type ABI.
reversals = {
    "ESP32/src/esp_legacy_prerender_startup.c": [
        ("#include <esp_heap_caps.h>\n\nstatic int preRenderAttempted",
         "#include <esp_heap_caps.h>\n\nextern DoomRPG_t* doomRpg;\n\nstatic int preRenderAttempted"),
        ("int EspLegacyPrerenderStartup_start(DoomRPG_t* root, int layoutReady) {",
         "int EspLegacyPrerenderStartup_start(int layoutReady) {"),
        ("if (root == NULL || EspNativeRootOwners_live()->menuSystem == NULL)",
         "if (doomRpg == NULL || EspNativeRootOwners_live()->menuSystem == NULL)"),
        ("EspNativeMenuStorage_startup(EspNativeRootOwners_live()->menuSystem, root)",
         "EspNativeMenuStorage_startup(EspNativeRootOwners_live()->menuSystem, doomRpg)"),
    ],
    "ESP32/include/esp_legacy_prerender_startup.h": [
        (" * The caller supplies its process token and whether the preceding layout\n"
         " * stage succeeded. The bridge never reads the process-global doomRpg. The bridge",
         " * The caller supplies whether the preceding layout stage succeeded. The bridge"),
        ("struct DoomRPG_s;\n"
         "int EspLegacyPrerenderStartup_start(struct DoomRPG_s* root, int layoutReady);",
         "int EspLegacyPrerenderStartup_start(int layoutReady);"),
    ],
    "ESP32/src/main.cpp": [
        # Root-J5 layout token is an independent boot edit; reverse before old Git parity.
        ("DoomRPG_startEngineLayout(doomRpg, &layoutReport) != 0;",
         "DoomRPG_startEngineLayout(&layoutReport) != 0;"),
        # Normalize the separate Root-J4 mappings token call before restoring
        # the original pre-Root-J3 boot blob.
        ("EspLegacyConfigMappingsStartup_start(doomRpg, engineRenderStartupReady ? 1 : 0) != 0;",
         "EspLegacyConfigMappingsStartup_start(engineRenderStartupReady ? 1 : 0) != 0;"),
        ("EspLegacyPrerenderStartup_start(doomRpg, engineLayoutReady ? 1 : 0) != 0;",
         "EspLegacyPrerenderStartup_start(engineLayoutReady ? 1 : 0) != 0;"),
    ],
}
for path, reverse in reversals.items():
    restored = files[path]
    if path == "ESP32/src/esp_legacy_prerender_startup.c":
        # SDL-F20 keeps SDL ABI types for host/oracle compilation only. Undo
        # this later native-only include guard before Root-J3 historic parity.
        guard = ("#if !defined(DOOMRPG_ESP32) || "
                 "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
                 "#include <SDL.h>\n#endif\n")
        assert restored.startswith(guard), "native SDL-F20 include guard drift"
        restored = "#include <SDL.h>\n" + restored[len(guard):]
    for after, before in reverse:
        assert restored.count(after) == 1, (path, after)
        restored = restored.replace(after, before, 1)
    binary = restored.encode("utf-8")
    actual = hashlib.sha1(
        b"blob " + str(len(binary)).encode() + b"\0" + binary
    ).hexdigest()
    assert actual == BASE_BLOBS[path], (path, actual, BASE_BLOBS[path])

# Extract the actual production function from the native TU. The body is
# compiled and executed, not recreated in the host fixture.
signature = "int EspLegacyPrerenderStartup_start(DoomRPG_t* root, int layoutReady) {"
begin = source.index(signature)
brace = source.index("{", begin)
end = None
level = 0
for index in range(brace, len(source)):
    if source[index] == "{":
        level += 1
    elif source[index] == "}":
        level -= 1
        if level == 0:
            end = index + 1
            break
assert end is not None
body = source[begin:end]
assert "preRenderAttempted = 1;" in body
assert "if (!layoutReady)" in body
assert "if (!preflightResources())" in body
assert "preRenderReady = 1;" in body
assert "EspNativeMenuStorage_startup(EspNativeRootOwners_live()->menuSystem, root)" in body
assert "doomRpg" not in body

fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct DoomRPG_s {
    uint8_t nativeProcessIdentity;
} DoomRPG_t;
typedef struct {
    void* menuSystem;
} EspNativeRootOwners_t;

static int menuStorage;
static EspNativeRootOwners_t owners = {&menuStorage};
static DoomRPG_t rootA = {0x5a}, rootB = {0xa5};
static int preRenderAttempted = 0;
static int preRenderReady = 0;
static int allowPreflight = 1, allowMenuStartup = 1;
static int preflightCalls = 0, startupCalls = 0;
static void* startupMenu = NULL;
static DoomRPG_t* startupRoot = NULL;

static EspNativeRootOwners_t* EspNativeRootOwners_live(void) {
    return &owners;
}
static uint32_t heap8Free(void) {
    return 100000U;
}
static uint32_t largest8Block(void) {
    return 61428U;
}
static int preflightResources(void) {
    ++preflightCalls;
    return allowPreflight;
}
static int EspNativeMenuStorage_startup(void* menu, DoomRPG_t* root) {
    ++startupCalls;
    startupMenu = menu;
    startupRoot = root;
    return allowMenuStartup;
}
static void printStageResult(const char* name, uint32_t before, uint32_t after) {
    (void)name;
    (void)before;
    (void)after;
}
__PRODUCTION_FUNCTION__

int main(void) {
    for (int i = 0; i < 4096; ++i) {
        const int mode = i % 8;
        DoomRPG_t* root = (i & 1) ? &rootA : &rootB;
        int layoutReady = 1;
        preRenderAttempted = 0;
        preRenderReady = 0;
        owners.menuSystem = &menuStorage;
        allowPreflight = allowMenuStartup = 1;
        preflightCalls = startupCalls = 0;
        startupMenu = NULL;
        startupRoot = NULL;
        switch (mode) {
          case 2: root = NULL; break;
          case 3: layoutReady = 0; break;
          case 4: owners.menuSystem = NULL; break;
          case 5: allowPreflight = 0; break;
          case 6: allowMenuStartup = 0; break;
        }
        const int expected = (mode == 0 || mode == 1 || mode == 7);
        const int preflightExpected = (mode != 2 && mode != 3 && mode != 4);
        const int startupExpected = (mode == 0 || mode == 1 ||
                                     mode == 6 || mode == 7);
        const int first = EspLegacyPrerenderStartup_start(root, layoutReady);
        assert(first == expected);
        assert(preRenderAttempted == 1);
        assert(preRenderReady == expected);
        assert(preflightCalls == preflightExpected);
        assert(startupCalls == startupExpected);
        if (startupExpected) {
            assert(startupMenu == &menuStorage);
            assert(startupRoot == root);
            assert(startupRoot != NULL);
        }
        // One-shot gate must never re-read SD or allocate menu storage even
        // when caller passes NULL, opposite identity, or false layout later.
        assert(EspLegacyPrerenderStartup_start(NULL, 0) == expected);
        assert(EspLegacyPrerenderStartup_start(root == &rootA ? &rootB : &rootA,
                                               1) == expected);
        assert(preflightCalls == preflightExpected);
        assert(startupCalls == startupExpected);
    }
    puts("PASS Root-J3 real pre-render startup 4096 cycles, two tokens,");
    puts("     null/layout/menu/PAK/startup refusals, idempotent single attempt");
    return 0;
}
'''.replace("__PRODUCTION_FUNCTION__", body)

with tempfile.TemporaryDirectory(prefix="root-j3-prerender-") as tmp:
    src = Path(tmp) / "root_j3_prerender.c"
    executable = Path(tmp) / "root_j3_prerender"
    src.write_text(fixture, encoding="utf-8")
    compilation = subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         "-Wno-unused-but-set-variable", str(src), "-o", str(executable)],
        capture_output=True, text=True,
    )
    assert compilation.returncode == 0, compilation.stderr
    run = subprocess.run([str(executable)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "PASS Root-J3 real pre-render startup 4096 cycles" in run.stdout

print("Root-J3 PASS: exact reverse blob parity of 3 changed source files;")
print("  4096 real C pre-render startups, 2 process tokens, fail-closed/no retry")
