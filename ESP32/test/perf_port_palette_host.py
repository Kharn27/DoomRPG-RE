#!/usr/bin/env python3
"""PERF-PORT-P1: compile the real C buildResolvedTextures body twice (ON/OFF).

The host PAK double supplies immutable mappings/palettes, phase changes, a
separate map generation, high-offset alias attacks, and corrupt/failed reads.
This does not claim hardware latency or visual fidelity; CYD Serial is final.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_native_first_frame.c"
CATALOG = ROOT / "ESP32/src/esp_native_graphics_catalog.c"
CI = ROOT / ".github/workflows/esp32-cyd.yml"


def extract(text, signature):
    assert text.count(signature) == 1, "not exactly one actual body: " + signature
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise AssertionError("unterminated actual C body: " + signature)


HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ESP_NATIVE_GRAPHICS_PALETTE_COLORS 16U
#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define MAX_RESOLVED_TEXTURES 64U
#define FIRST_FRAME_ANIMATION_TIME 0U
typedef struct {
    uint32_t nameHash;
    uint32_t size;
} EspAssetPackEntry;
typedef struct {
    uint16_t resourceId;
    uint16_t paletteSourceOffset;
    uint32_t sourceOffset;
    uint16_t paletteRgb565[ESP_NATIVE_GRAPHICS_PALETTE_COLORS];
} EspNativeGraphicsCatalogRecord;
typedef struct {
    const EspNativeGraphicsCatalogRecord *textures;
    uint16_t textureCount;
} EspNativeGraphicsCatalogView;
typedef struct ResolvedWallTexture_s {
    uint16_t logicalId;
    uint16_t actualId;
    uint16_t paletteSourceOffset;
    uint16_t reserved;
    uint32_t sourceTexelOffset;
    uint16_t paletteRgb565[ESP_NATIVE_GRAPHICS_PALETTE_COLORS];
} ResolvedWallTexture;
typedef struct FirstFrameWork_s {
    ResolvedWallTexture resolved[MAX_RESOLVED_TEXTURES];
    uint16_t resolvedCount;
    uint32_t animationMs;
    uint16_t paletteCatalogHits;
    uint16_t palettePakReads;
} FirstFrameWork;
static EspNativeGraphicsCatalogRecord rec[4];
static EspNativeGraphicsCatalogView catalog = {rec, 4};
static uint8_t mappingsData[128];
static uint8_t palettesData[140000];
static EspAssetPackEntry mappings = {1U, 106U};
static EspAssetPackEntry palettes = {2U, 100U};
static uint32_t paletteReads;
static uint32_t failPaletteOffset;
static uint32_t mappingFails;
static int ready = 1;
static uint32_t animatedPhase = 0U;
static void put16(uint8_t* b, uint32_t pos, uint16_t v) {
    b[pos] = (uint8_t)v; b[pos + 1U] = (uint8_t)(v >> 8);
}
static void put32(uint8_t* b, uint32_t pos, uint32_t v) {
    put16(b,pos,(uint16_t)v); put16(b,pos+2U,(uint16_t)(v >> 16));
}
static int EspNativeGraphicsCatalog_isReady(void) {return ready;}
static const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {return &catalog;}
static int EspNativeWallIndicator_isAnimated(uint16_t id) {return id == 4U;}
static uint32_t EspNativeWallIndicator_phase(uint16_t id, uint32_t ms) {
    (void)id; (void)ms; return animatedPhase;
}
static int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                   uint32_t off, void* dst, size_t len) {
    const uint8_t *data = entry->nameHash == 1U ? mappingsData : palettesData;
    if (off > entry->size || len > entry->size - off) return 0;
    if (entry->nameHash == 1U && mappingFails) return 0;
    if (entry->nameHash == 2U && off >= PALETTES_HEADER_BYTES) {
        ++paletteReads;
        if (off == failPaletteOffset) return 0;
    }
    memcpy(dst, data + off, len);
    return 1;
}
/* ACTUAL_FUNCTIONS */
static void buildFixture(uint16_t seed) {
    const uint16_t offsets[8] = {0,0,16,32,16,0,32,16};
    unsigned int i, p;
    memset(mappingsData, 0, sizeof(mappingsData));
    memset(palettesData, 0, sizeof(palettesData));
    mappings.size = 106U;
    palettes.size = 100U;
    put32(mappingsData,0U,8U);
    put32(mappingsData,4U,1U);
    put32(mappingsData,8U,8U);
    put32(mappingsData,12U,1U);
    for (i = 0U; i < 8U; ++i) {
        put32(mappingsData, 16U+i*8U, i*256U);
        put32(mappingsData, 20U+i*8U, offsets[i]);
    }
    for (i = 0U; i < 8U; ++i) put16(mappingsData, 88U+i*2U, (uint16_t)i);
    put16(mappingsData, 90U, 3U); /* logical 1 -> actual 3, palette mismatch */
    put16(mappingsData, 92U, 4U); /* logical 2 -> actual 4, palette matches */
    put16(mappingsData, 94U, 5U); /* logical 3 -> actual 5, palette mismatch */
    put16(mappingsData, 96U, 6U); /* logical 4 -> actual 6/7 (animated) */
    put32(palettesData, 0U, 96U);
    for (i = 0U; i < 48U; ++i) {
        uint16_t rgb = (uint16_t)((((i+seed)&31U)<<11) | (((i*3U)&63U)<<5) |
                                  ((i+seed*3U)&31U));
        put16(palettesData,4U+i*2U,rgb);
    }
    for (i = 0U; i < 4U; ++i) {
        const uint32_t id = i+1U;
        const uint32_t off = offsets[id];
        rec[i].resourceId = (uint16_t)id;
        rec[i].sourceOffset = id * 256U;
        rec[i].paletteSourceOffset = (uint16_t)off;
        for (p = 0U; p < ESP_NATIVE_GRAPHICS_PALETTE_COLORS; ++p)
            rec[i].paletteRgb565[p] = readLe16(palettesData+4U+(off+p)*2U);
    }
    animatedPhase = 0U;
    paletteReads = 0U;
    failPaletteOffset = UINT32_MAX;
    mappingFails = 0U;
    ready = 1;
}
static uint32_t fnv(const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t h = 2166136261U;
    while (n--) {h ^= *p++; h *= 16777619U;}
    return h;
}
static void runCase(const char* name, int expectOK) {
    FirstFrameWork work;
    int ok;
    memset(&work,0,sizeof(work));
    paletteReads = 0U;
    ok = buildResolvedTextures(&work, &mappings, &palettes);
    if (ok != expectOK) {fprintf(stderr, "bad status %s\n",name);exit(2);}
    printf("CASE %s status=%d fnv=%08x reads=%u hits=%u pak=%u\n",
           name,ok,fnv(work.resolved,sizeof(work.resolved)),
           paletteReads,work.paletteCatalogHits,work.palettePakReads);
    if (ok) {
        if (work.resolvedCount != 4U ||
            work.paletteCatalogHits + work.palettePakReads != 4U ||
            work.palettePakReads != paletteReads) exit(3);
    }
}
int main(void) {
    buildFixture(1U);
    runCase("cold", 1);
    animatedPhase = 1U;
    runCase("animated", 1);
    buildFixture(9U); /* map session generation: new palette contents */
    runCase("newmap", 1);
    /* Valid offset > UINT16_MAX with the same low 16 bits as logical 2's
     * catalog offset: must NOT be accepted as a catalog hit. */
    palettes.size = 4U + 65584U*2U;
    put32(palettesData,0U,palettes.size-4U);
    put32(mappingsData,16U+4U*8U+4U, 65552U);
    { unsigned int p;
      for (p=0U;p<16U;++p) put16(palettesData,4U+(65552U+p)*2U,
                                  (uint16_t)(0xf000U+p)); }
    runCase("high-offset", 1);
    buildFixture(1U);
    failPaletteOffset = 4U + 32U*2U; /* must fail on uncached actual 3 */
    runCase("failed-palette", 0);
    buildFixture(1U);
    mappingFails = 1U;
    runCase("failed-mapping", 0);
    buildFixture(1U);
    ready = 0;
    runCase("not-ready", 0);
    buildFixture(1U);
    put32(mappingsData,0U,0U);
    runCase("invalid-header", 0);
    return 0;
}
"""


def main():
    renderer = SOURCE.read_text()
    assert "DOOMRPG_PERF_PORT_PALETTE_REUSE" in renderer
    assert "logicalRecord->paletteSourceOffset == (uint16_t)paletteOffset" in renderer
    assert "(uint32_t)paletteOffset <= UINT16_MAX" in renderer
    assert "sourceBgr565ToRgb565(logicalRecord->paletteRgb565[p])" in renderer
    assert "work->paletteCatalogHits" in renderer
    assert "work->palettePakReads" in renderer
    assert "shapeData" in renderer and "mediaTexels" in renderer
    catalog = CATALOG.read_text()
    assert "catalogArena[textureIndex]" in catalog
    assert "readRecord(&mappings, &palettes" in catalog
    assert "EspNativeGraphicsCatalog_reset" in catalog
    assert "palettes,\n" in catalog
    assert "perf_port_palette_host.py" in CI.read_text()
    actual = "\n\n".join(
        extract(renderer, signature) for signature in (
            "static uint16_t readLe16(",
            "static uint32_t readLe32(",
            "static uint16_t sourceBgr565ToRgb565(",
            "static int buildResolvedTextures(",
        )
    )
    code = HARNESS.replace("/* ACTUAL_FUNCTIONS */", actual)
    results = {}
    with tempfile.TemporaryDirectory(prefix="doom-perf-port-") as tmp:
        path = Path(tmp) / "palette.c"
        path.write_text(code)
        for enabled in (0, 1):
            binary = Path(tmp) / ("palette_" + str(enabled))
            cc = shlex.split(os.environ.get("CC","cc"))
            subprocess.run(cc + ["-std=c11","-O2","-Wall","-Wextra","-Werror",
                "-DDOOMRPG_PERF_PORT_PROFILE=1",
                "-DDOOMRPG_PERF_PORT_PALETTE_REUSE=" + str(enabled),
                str(path),"-o",str(binary)],check=True,cwd=ROOT)
            out = subprocess.check_output([str(binary)],text=True,cwd=ROOT)
            data = {}
            for line in out.splitlines():
                m = re.fullmatch(r"CASE ([\w-]+) status=(\d+) fnv=([0-9a-f]{8}) reads=(\d+) hits=(\d+) pak=(\d+)",line)
                assert m, line
                data[m.group(1)] = tuple(int(m.group(i), 16 if i==3 else 10)
                                           for i in range(2,7))
            results[enabled] = data
    off, on = results[0], results[1]
    assert set(off) == set(on)
    for name in off:
        assert off[name][:2] == on[name][:2], (name,off[name],on[name])
    for name, hits in (("cold",1),("animated",2),("newmap",1),("high-offset",0)):
        assert off[name][2:] == (4,0,4), (name,off[name])
        assert on[name][2:] == (4-hits,hits,4-hits), (name,on[name])
    assert off["cold"][1] != off["newmap"][1], "map change must change resolved FNV"
    assert off["cold"][1] != off["animated"][1], "animation must change resolved FNV"
    for name in ("failed-palette","failed-mapping","not-ready","invalid-header"):
        assert off[name][0] == on[name][0] == 0
    print("PERF-PORT-P1 PASS: actual C body ON/OFF FNV exact, palette reads 4->3/2, "
          "animated/new-map/high-offset, failed reads and invalid headers closed")


if __name__ == "__main__":
    main()
