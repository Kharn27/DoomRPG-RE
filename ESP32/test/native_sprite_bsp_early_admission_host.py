#!/usr/bin/env python3
"""Compile the real native sprite-order function and compare it with its
pre-admission reference. No ESP32 SDK or rendered-frame mock is required.
Includes hidden, visible, rejected, preview, malformed leaf and trace cases.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "ESP32/src/esp_native_sprite_renderer.c").read_text()
start = source.index("static int buildOrder(")
end = source.index("\nstatic int spans(", start)
optimized = source[start:end]
assert "Normal frames first ask the compact visual owner" in optimized
assert "The hidden bit (0x10000)" in optimized
assert "if (!EspMapRuntime_getMapSprite(i, &sprite) ||" in optimized
assert "if (leaf == UINT32_MAX) return 0;" in optimized

# Reconstruct the actual old call ordering from the tested function, not a
# separately reimplemented sorting algorithm. Every other line stays shared.
reference, n = re.subn(
    r"#if defined\(DOOMRPG_ESP32_TRACE_SPRITE\)\n(.*?)#else\n.*?#endif",
    lambda m: m.group(1).rstrip(),
    optimized, count=1, flags=re.S)
assert n == 1
reference, n = re.subn(
    r"#if defined\(DOOMRPG_ESP32_TRACE_SPRITE\)\n(.*?)#else\n.*?#endif",
    lambda m: m.group(1).rstrip(),
    reference, count=1, flags=re.S)
assert n == 1
reference, n = re.subn(
    r"#if !defined\(DOOMRPG_ESP32_TRACE_SPRITE\)\n.*?#endif\n",
    "", reference, count=1, flags=re.S)
assert n == 1
reference = reference.replace("static int buildOrder(", "static int buildOrderReference(", 1)

support = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MAP_SPRITES 344U
#define MAX_VISIBLE_SPRITES 64U
#define VISUAL_MASK 0x0001fe00UL
#define HIDDEN 0x00010000UL
#define TILE 0x00040000UL
#define CROSS 0x04000000UL
#define SKIP_RESOURCE 0x20000000UL
#define RENDER_MODE_ADD 7U
#define RENDER_MODE_NORMAL 0U
#define ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK 0x03ffU
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U
#define TRACE_SPRITE(index, ...) do { } while (0)

typedef struct {uint16_t x,y;uint32_t info;} EspMapSprite;
typedef struct {uint32_t mapSpriteCount;} EspMapRuntimeView;
typedef struct {uint8_t unused;} EspNativeBspVisibilityState;
typedef struct {uint16_t index,logical;uint32_t info;int32_t sortZ;} Order;
typedef struct {int viewX,viewY,viewCos_,viewSin_,viewTransX;} Render_t;
typedef struct {
    uint32_t objects, hidden, bspRejected, bspCandidates;
    uint32_t unsupported, mode7Objects, mode0Objects, orderFNV1a;
} EspNativeSpriteStats;
typedef struct {
    uint32_t recordUs, leafQueryUs, records, hidden, queried;
} SpriteOrderProbe;

static EspMapSprite mapSprites[MAP_SPRITES];
static uint8_t visualStates[MAP_SPRITES], entityTypes[MAP_SPRITES];
static uint32_t mapReads, topologyReads, visibilityReads, previewReads;
static uint32_t badLeaf = UINT32_MAX;
static uint32_t clockTicks;
static int64_t esp_timer_get_time(void) { return ++clockTicks; }

static int EspMapRuntime_getMapSprite(uint32_t i, EspMapSprite *out) {
    if (i >= MAP_SPRITES || !out) return 0;
    ++mapReads; *out=mapSprites[i]; return 1;
}
static int EspMapSpriteTopology_getVisualState(uint32_t i, uint8_t *out) {
    if (i >= MAP_SPRITES || !out) return 0;
    *out=visualStates[i]; return 1;
}
static int EspMapSpriteTopology_getEntity(uint32_t i, uint8_t *type,
                                          uint8_t *subtype, uint16_t *link,
                                          uint16_t *ordinal) {
    if (i >= MAP_SPRITES || !type || !subtype || !link || !ordinal) return 0;
    ++topologyReads;
    *type=entityTypes[i];*subtype=(uint8_t)(i%14);
    *link=(uint16_t)(ESP_MAP_SPRITE_TOPOLOGY_LINKED | (i&1023));
    *ordinal=(uint16_t)i;
    return 1;
}
static int visibleImpl(uint32_t i, uint32_t *leaf) {
    ++visibilityReads;
    if (i == badLeaf) { *leaf=UINT32_MAX; return 0; }
    *leaf=i%32U;
    return (i%11U==2U || i%17U==5U);
}
static int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState *v, uint32_t i, uint32_t *leaf) {
    (void)v;return visibleImpl(i,leaf);
}
static int EspNativeBspVisibility_mapSpriteVisibleReadOnly(
    const EspNativeBspVisibilityState *v, uint32_t i, uint32_t *leaf) {
    ++previewReads;return EspNativeBspVisibility_mapSpriteVisible(v,i,leaf);
}
static int isLegacyCrossLogical(uint16_t logical) {return logical==84U;}
static const int* EspNativeGraphicsCatalog_findSprite(uint16_t logical) {
    static const int found=1;
    return logical < 512U ? &found : NULL;
}
static int spriteRenderMode(uint16_t logical) {
    return logical==136U ? RENDER_MODE_ADD : RENDER_MODE_NORMAL;
}
static void EspNativeSpriteVisual_coordinates(uint16_t x, uint16_t y,
                                              uint32_t info, int *outX,int *outY) {
    *outX=x+(int)((info>>20)&3U); *outY=y;
}
static int32_t EspNativeSpriteVisual_sortDepth(
    int x,int y,uint32_t info,uint8_t type,int cos,int sin,int trans) {
    return (int32_t)(x*cos+y*sin+trans+(int)(info&255U)+(int)type);
}
static uint32_t fnv(uint32_t h,const void* data,uint32_t n) {
    const uint8_t* p=(const uint8_t*)data;
    uint32_t i;
    for(i=0;i<n;i++){h^=p[i];h*=16777619U;}
    return h;
}
'''
main = r'''
static void exercise(int preview, int malformed) {
    EspMapRuntimeView rt={MAP_SPRITES};
    EspNativeBspVisibilityState vis={0};
    Render_t render={0};
    Order oldOrder[MAX_VISIBLE_SPRITES]={0}, newOrder[MAX_VISIBLE_SPRITES]={0};
    EspNativeSpriteStats oldStats={0}, newStats={0};
    uint32_t oldCount=0,newCount=0,oldMap,oldTopo,newMap,newTopo;
    int oldOk,newOk;
    render.viewCos_=3;render.viewSin_=5;render.viewTransX=33;
    badLeaf=malformed ? 77U : UINT32_MAX;
    mapReads=topologyReads=visibilityReads=previewReads=clockTicks=0;
    oldOk=buildOrderReference(&render,&rt,&vis,oldOrder,&oldStats,
                              &oldCount,preview,NULL);
    oldMap=mapReads;oldTopo=topologyReads;
    mapReads=topologyReads=visibilityReads=previewReads=clockTicks=0;
    newOk=buildOrder(&render,&rt,&vis,newOrder,&newStats,
                     &newCount,preview,NULL);
    newMap=mapReads;newTopo=topologyReads;
    assert(oldOk==newOk);
    if (!malformed) {
        assert(oldOk);
        assert(oldCount==newCount);
        assert(memcmp(&oldStats,&newStats,sizeof oldStats)==0);
        assert(memcmp(oldOrder,newOrder,sizeof oldOrder)==0);
        assert(newStats.objects==MAP_SPRITES);
        assert(newStats.hidden>0 && newStats.bspRejected>0);
#if defined(DOOMRPG_ESP32_TRACE_SPRITE)
        assert(oldMap==newMap && oldTopo==newTopo);
#else
        assert(oldMap==MAP_SPRITES && oldTopo==MAP_SPRITES);
        assert(newMap==newCount && newTopo==newCount);
        assert(newMap<oldMap/4U);
#endif
    } else {
        assert(!oldOk && !newOk);
    }
    printf("PARITY preview=%d malformed=%d result=%d candidates=%u oldReads=%u/%u newReads=%u/%u\n",
           preview,malformed,oldOk,newCount,oldMap,oldTopo,newMap,newTopo);
}
int main(void){
    uint32_t i;
    for(i=0;i<MAP_SPRITES;i++) {
        mapSprites[i]=(EspMapSprite){(uint16_t)(i*19U),(uint16_t)(i*11U),
                                    (uint32_t)(i%200U)};
        visualStates[i]=(uint8_t)((i%19U==0U)?0x80U:(i%7U));
        entityTypes[i]=(uint8_t)(i%13U);
    }
    exercise(0,0);exercise(1,0);exercise(0,1);exercise(1,1);
    puts("native_sprite_bsp_early_admission_host: PASS");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="doomrpg-sprite-order-") as td:
    source_file=Path(td)/"admission.c"
    binary=Path(td)/"admission"
    source_file.write_text(support+"\n"+reference+"\n"+optimized+"\n"+main)
    for defines in ([], ["-DDOOMRPG_ESP32_TRACE_SPRITE=123"]):
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra",
                        "-Werror","-Wno-unused-parameter","-Wno-unused-function",
                        "-Wno-unused-variable","-Wno-unused-but-set-variable",
                        *defines,str(source_file),"-o",str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
print("PASS normal and trace, BSP admission parity and fail-closed")
