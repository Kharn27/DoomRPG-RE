#!/usr/bin/env python3
"""Root-J4: native mappings startup receives an explicit process token.

Restore exact pre-J4 Git blobs for the only three production changes, then
extract, compile and execute the actual production C startup function under
4096 deterministic trials with two distinct roots and fail-closed paths.
"""
from pathlib import Path
import hashlib
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE_BLOBS = {
    "ESP32/src/esp_legacy_config_mappings_startup.c":
        "2339401de08c23c793e6d1205303d13b9aebf8cd",
    "ESP32/include/esp_legacy_config_mappings_startup.h":
        "e16818a6a755144ac29b6490b8e007ecbbe0b0ed",
    "ESP32/src/main.cpp":
        "fff22b2c36d37524841b4e2b57ac083e2a5d0619",
}
files = {p: (ROOT / p).read_text(encoding="utf-8") for p in BASE_BLOBS}
source = files["ESP32/src/esp_legacy_config_mappings_startup.c"]
header = files["ESP32/include/esp_legacy_config_mappings_startup.h"]
boot = files["ESP32/src/main.cpp"]
signature = "int EspLegacyConfigMappingsStartup_start(DoomRPG_t* root, int renderStartupReady) {"
assert signature in source
assert "extern DoomRPG_t* doomRpg;" not in source
assert "if (root == NULL || EspNativeRootOwners_live()->render == NULL)" in source
assert "struct DoomRPG_s;" in header
assert "int EspLegacyConfigMappingsStartup_start(struct DoomRPG_s* root, int renderStartupReady);" in header
assert "EspLegacyConfigMappingsStartup_start(doomRpg, engineRenderStartupReady ? 1 : 0) != 0;" in boot
assert source.count("EspLegacyConfigMappingsStartup_start(") == 1
assert boot.count("EspLegacyConfigMappingsStartup_start(") == 1

reversals = {
    "ESP32/src/esp_legacy_config_mappings_startup.c": [
        ("#include <esp_heap_caps.h>\n\n/* Desktop",
         "#include <esp_heap_caps.h>\n\nextern DoomRPG_t* doomRpg;\n\n/* Desktop"),
        (signature,
         "int EspLegacyConfigMappingsStartup_start(int renderStartupReady) {"),
        ("if (root == NULL || EspNativeRootOwners_live()->render == NULL)",
         "if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL)"),
    ],
    "ESP32/include/esp_legacy_config_mappings_startup.h": [
        ("struct DoomRPG_s;\n"
         "int EspLegacyConfigMappingsStartup_start(struct DoomRPG_s* root, int renderStartupReady);",
         "int EspLegacyConfigMappingsStartup_start(int renderStartupReady);"),
    ],
    "ESP32/src/main.cpp": [
        ("EspLegacyConfigMappingsStartup_start(doomRpg, engineRenderStartupReady ? 1 : 0) != 0;",
         "EspLegacyConfigMappingsStartup_start(engineRenderStartupReady ? 1 : 0) != 0;"),
    ],
}
for path, changes in reversals.items():
    restored = files[path]
    for new, old in changes:
        assert restored.count(new) == 1, (path, new)
        restored = restored.replace(new, old, 1)
    raw = restored.encode("utf-8")
    sha = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
    assert sha == BASE_BLOBS[path], (path, sha, BASE_BLOBS[path])

start = source.index(signature)
brace = source.index("{", start)
depth, end = 0, None
for pos in range(brace, len(source)):
    if source[pos] == "{":
        depth += 1
    elif source[pos] == "}":
        depth -= 1
        if depth == 0:
            end = pos + 1
            break
assert end is not None
production = source[start:end]
assert "configMappingsAttempted = 1;" in production
assert "configMappingsReady = 1;" in production
assert "EspLegacyAssetSource_stat(\"mappings.bin\", &mappingBytes)" in production
assert "mappingsResult = EspLegacyMappings_load(render)" in production
assert "render->mediaTexelOffsets == NULL" in production
assert "doomRpg" not in production

fixture = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct DoomRPG_s { unsigned char token; } DoomRPG_t;
typedef struct Render_s {
    int* mediaTexelOffsets;
    int* mediaBitShapeOffsets;
    short* mediaTexturesIds;
    short* mediaSpriteIds;
} Render_t;
typedef struct MappingPlan_s {
    int textureCnt;
    int spriteCnt;
    uint32_t persistentBytes;
} MappingPlan_t;
typedef struct EspNativeRootOwners_s {
    Render_t* render;
} EspNativeRootOwners_t;
typedef int boolean;
enum { false, true };
static int configMappingsAttempted, configMappingsReady;
static MappingPlan_t installedPlan;
static Render_t activeRender;
static EspNativeRootOwners_t owners = {&activeRender};
static DoomRPG_t rootA = {0x2d}, rootB = {0xd2};
static int allowStat, allowLoad, giveCompleteArrays;
static int statCalls, loadCalls;
static Render_t* passedRender;
static int mediaA, mediaB;
static short mediaC, mediaD;

static EspNativeRootOwners_t* EspNativeRootOwners_live(void) {
    return &owners;
}
static uint32_t heap8Free(void) { return 125860U; }
static uint32_t largest8Block(void) { return 86004U; }
static int EspLegacyAssetSource_stat(const char* name, uint32_t* bytes) {
    ++statCalls;
    assert(strcmp(name, "mappings.bin") == 0);
    if (!allowStat) return 0;
    *bytes = 8392U;
    return 1;
}
static int EspLegacyMappings_load(Render_t* render) {
    ++loadCalls;
    passedRender = render;
    if (!allowLoad) return 0;
    render->mediaTexelOffsets = &mediaA;
    render->mediaBitShapeOffsets = &mediaB;
    render->mediaTexturesIds = &mediaC;
    render->mediaSpriteIds = giveCompleteArrays ? &mediaD : NULL;
    installedPlan.textureCnt = 152;
    installedPlan.spriteCnt = 252;
    installedPlan.persistentBytes = 8376U;
    return 1;
}
__PRODUCTION_FUNCTION__
int main(void) {
    for (int i = 0; i < 4096; ++i) {
        const int mode = i % 10;
        DoomRPG_t* token = (i & 1) ? &rootA : &rootB;
        int renderReady = 1;
        configMappingsAttempted = configMappingsReady = 0;
        memset(&activeRender, 0, sizeof(activeRender));
        owners.render = &activeRender;
        installedPlan.persistentBytes = 0;
        allowStat = allowLoad = giveCompleteArrays = 1;
        statCalls = loadCalls = 0;
        passedRender = NULL;
        switch (mode) {
            case 2: renderReady = 0; break;
            case 3: token = NULL; break;
            case 4: owners.render = NULL; break;
            case 5: allowStat = 0; break;
            case 6: allowLoad = 0; break;
            case 7: giveCompleteArrays = 0; break;
        }
        const int success = (mode == 0 || mode == 1 || mode == 8 || mode == 9);
        const int statExpected = (mode != 2 && mode != 3 && mode != 4);
        const int loadExpected = (mode == 0 || mode == 1 || mode == 6 ||
                                  mode == 7 || mode == 8 || mode == 9);
        const int result = EspLegacyConfigMappingsStartup_start(token, renderReady);
        assert(result == success);
        assert(configMappingsAttempted == 1);
        assert(configMappingsReady == success);
        assert(statCalls == statExpected);
        assert(loadCalls == loadExpected);
        if (loadExpected) {
            assert(passedRender == &activeRender);
            if (allowLoad) {
                assert(activeRender.mediaTexelOffsets == &mediaA);
                assert(activeRender.mediaBitShapeOffsets == &mediaB);
                assert(activeRender.mediaTexturesIds == &mediaC);
                assert(activeRender.mediaSpriteIds ==
                       (giveCompleteArrays ? &mediaD : NULL));
            }
        }
        // Single-attempt cache: no reads or loader calls on any later request.
        assert(EspLegacyConfigMappingsStartup_start(NULL, 0) == success);
        assert(EspLegacyConfigMappingsStartup_start(
                   token == &rootA ? &rootB : &rootA, 1) == success);
        assert(statCalls == statExpected);
        assert(loadCalls == loadExpected);
    }
    puts("PASS Root-J4 4096 real mappings-startup C cases, two roots,");
    puts("     fail-closed no-token/no-Render/no-PAK/load/incomplete and idempotence");
    return 0;
}
""".replace("__PRODUCTION_FUNCTION__", production)

with tempfile.TemporaryDirectory(prefix="root-j4-mappings-") as tmp:
    program = Path(tmp) / "root_j4_mappings.c"
    executable = Path(tmp) / "root_j4_mappings"
    program.write_text(fixture, encoding="utf-8")
    build = subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         str(program), "-o", str(executable)],
        capture_output=True, text=True)
    assert build.returncode == 0, build.stderr
    run = subprocess.run([str(executable)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "PASS Root-J4 4096 real mappings-startup C cases" in run.stdout
print("Root-J4 PASS: exact Git blob parity and 4096 production C cycles")
