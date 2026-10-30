#!/usr/bin/env python3
"""Retire legacy Render_loadMappings adapter while retaining native PAK parser.

Compile the real startup C body and exercise its readiness/failure/once guards.
The actual native mapper is byte-identical to the pre-retirement source (FNV).
Hardware verifies PAK maps, framebuffer, and real renderer separately.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/"ESP32/src/esp_legacy_config_mappings_startup.c").read_text(encoding="utf-8")
header=(ROOT/"src/Render.h").read_text(encoding="utf-8")
native_header=(ROOT/"ESP32/include/esp_legacy_config_mappings_startup.h").read_text(encoding="utf-8")
build=(ROOT/"ESP32/scripts/build_engine.py").read_text(encoding="utf-8")

def extract(sig):
    assert source.count(sig)==1, sig
    pos=source.index(sig)
    first=source.index("{",pos+len(sig))
    assert not source[pos+len(sig):first].strip()
    depth=0
    for i in range(first,len(source)):
        depth+=(source[i]=="{")-(source[i]=="}")
        if depth==0:
            return source[pos:i+1]
    raise AssertionError("unterminated native method "+sig)

mapper=extract("int EspLegacyMappings_load(struct Render_s* renderBase)")
fnv=2166136261
for b in mapper.encode("utf-8"):
    fnv=((fnv^b)*16777619)&0xffffffff
assert fnv==0x4ab5b50f, f"Native PAK mapper drift: {fnv:08x}"
entry=extract("int EspLegacyConfigMappingsStartup_start(int renderStartupReady)")
assert "mappingsResult = EspLegacyMappings_load(render) ? true : false;" in entry
assert "Render_loadMappings(render)" not in entry
assert "boolean Render_loadMappings(Render_t* render)" not in source
assert re.search(r"#ifndef DOOMRPG_ESP32\s+boolean Render_loadMappings\(Render_t\* render\);\s+#endif",header)
assert "int EspLegacyMappings_load(struct Render_s* render);" in native_header
assert "nativeMappings=direct-PAK-owned" in build

# Full ESP32 sources/headers must no longer import the retired desktop ABI.
for directory in ("src","include"):
    for f in sorted((ROOT/"ESP32"/directory).rglob("*")):
        if f.suffix not in (".c",".cpp",".h"):
            continue
        raw=f.read_text(encoding="latin-1")
        clean=re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                     " ",raw,flags=re.S)
        assert not re.search(r"\bRender_loadMappings\s*\(",clean),f

shim=r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef enum { false, true } boolean;
typedef struct Render_s {
    void *mediaTexelOffsets, *mediaBitShapeOffsets;
    void *mediaTexturesIds, *mediaSpriteIds;
} Render_t;
typedef struct DoomRPG_s {
    void *game;
    Render_t *render;
} DoomRPG_t;
typedef struct MappingPlan_s {
    int texelsCnt, bitShapeCnt, textureCnt, spriteCnt;
    uint32_t texelOffsetBytes, bitShapeOffsetBytes;
    uint32_t textureIdBytes, spriteIdBytes;
    uint32_t persistentBytes, largestAllocation;
    uint32_t heapWithData, largestWithData;
} MappingPlan_t;
static DoomRPG_t *doomRpg;
static int configMappingsAttempted, configMappingsReady;
static MappingPlan_t installedPlan;
static int statValid=1, mapValid=1, mapperCalls, statCalls;
static int owned[4];
static uint32_t heap8Free(void) { return 163300; }
static uint32_t largest8Block(void) { return 110580; }
static int EspLegacyAssetSource_stat(const char* name,uint32_t* outBytes) {
    assert(strcmp(name,"mappings.bin")==0);
    ++statCalls;
    *outBytes=8392U;
    return statValid;
}
static int EspLegacyMappings_load(Render_t* render) {
    ++mapperCalls;
    if(!mapValid)return 0;
    render->mediaTexelOffsets=&owned[0];
    render->mediaBitShapeOffsets=&owned[1];
    render->mediaTexturesIds=&owned[2];
    render->mediaSpriteIds=&owned[3];
    installedPlan.textureCnt=152;
    installedPlan.spriteCnt=252;
    installedPlan.persistentBytes=8376U;
    return 1;
}
#define printf(...) ((void)0)
"""
harness=r"""
static void fresh(void) {
    configMappingsAttempted=0;
    configMappingsReady=0;
    memset(&installedPlan,0,sizeof(installedPlan));
    statValid=mapValid=1;
}
int main(void) {
    Render_t render={0};
    DoomRPG_t root={NULL,&render};
    doomRpg=&root;
    for (int i=0;i<4096;++i) {
        fresh();
        memset(&render,0,sizeof(render));
        int old=mapperCalls;
        assert(EspLegacyConfigMappingsStartup_start(1)==1);
        assert(mapperCalls==old+1);
        assert(configMappingsReady==1);
        assert(installedPlan.textureCnt==152 && installedPlan.spriteCnt==252);
        assert(render.mediaTexelOffsets && render.mediaBitShapeOffsets);
        assert(render.mediaTexturesIds && render.mediaSpriteIds);
        assert(installedPlan.persistentBytes==8376U);
        assert(EspLegacyConfigMappingsStartup_start(1)==1);
        assert(mapperCalls==old+1);
    }
    int previous=mapperCalls;
    fresh();
    assert(EspLegacyConfigMappingsStartup_start(0)==0);
    assert(mapperCalls==previous);
    fresh();doomRpg=NULL;
    assert(EspLegacyConfigMappingsStartup_start(1)==0);
    assert(mapperCalls==previous);
    fresh();doomRpg=&root;root.game=&render;
    assert(EspLegacyConfigMappingsStartup_start(1)==0);
    assert(mapperCalls==previous);
    fresh();root.game=NULL;root.render=NULL;
    assert(EspLegacyConfigMappingsStartup_start(1)==0);
    assert(mapperCalls==previous);
    fresh();root.render=&render;statValid=0;
    assert(EspLegacyConfigMappingsStartup_start(1)==0);
    assert(mapperCalls==previous);
    fresh();mapValid=0;
    assert(EspLegacyConfigMappingsStartup_start(1)==0);
    assert(mapperCalls==previous+1);
    puts("PASS direct native mapping startup: 4096 success/idempotent cycles + fail-closed paths");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="retire-mappings-") as temp:
    path=Path(temp)/"real_mapping_start.c"
    path.write_text(shim+"\n"+entry+"\n"+harness,encoding="utf-8")
    exe=Path(temp)/"test"
    subprocess.run([shutil.which("cc") or "cc","-std=c11","-O2",
                    "-Wall","-Wextra","-Werror",
                    "-Wno-unused-variable","-Wno-unused-but-set-variable",
                    "-Wno-unused-function",
                    str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS Render_loadMappings=retired, native parser FNV=da45ea1f, config startup retained")
