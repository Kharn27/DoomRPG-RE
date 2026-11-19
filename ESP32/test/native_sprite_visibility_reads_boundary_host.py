#!/usr/bin/env python3
"""ARCH-RC21: compile actual C bodies of raw/overlay map sprite and BSP activation.

Real bodies: packed BSP sprite decoder, projected sprite adapter, pure BSP
admission leaf, public visible activation and its little-endian helper.
No ESP-IDF SDK needed. Adversarial mutants must be killed. The hardware CYD
Serial remains the final behavioral and memory authority.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SOURCES = {
    "raw": (SRC / "esp_map_access.c").read_text(),
    "projection": (SRC / "esp_native_gameplay_sprite_overlay.c").read_text(),
    "bsp": (SRC / "esp_native_bsp_visibility.c").read_text(),
    "activation": (SRC / "esp_native_gameplay_monster_active_sequence.c").read_text(),
}
SIGNATURES = (
    ("activation", "static uint16_t read16le("),
    ("raw", "static uint16_t readLe16("),
    ("raw", "int EspMapRuntime_getMapSpriteBase("),
    ("projection", "int EspMapRuntime_getMapSprite("),
    ("bsp", "int EspNativeBspVisibility_mapSpriteVisibleBase("),
    ("activation", "int EspNativeBspVisibility_mapSpriteVisible("),
)


def require(ok, why):
    if not ok:
        raise AssertionError(why)


def extract(source, signature):
    require(source.count(signature) == 1, "missing/ambiguous actual C: " + signature)
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unclosed C body: " + signature)


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define ESP_MAP_SPRITE_RECORD_BYTES 5U
#define ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK 0x03ffU
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U
#define SPRITE_TYPE_ENEMY 1U
#define SPRITE_HIDDEN 0x00010000UL
#define SPRITE_SORT_BIAS 0x01000000UL
#define SPRITE_FIXED_ANIM 0x80000000UL
#define SPRITE_COMBAT_ATTACK_PRIMARY_VISUAL 1U
#define SPRITE_COMBAT_CORPSE_VISUAL 2U
#define SPRITE_COMBAT_DEATH_VISUAL 4U
#define SPRITE_COMBAT_ATTACK_ALTERNATE_VISUAL 5U
#define SPRITE_COMBAT_PAIN_VISUAL 6U
#define ACTIVESEQ_TYPE_ENEMY 1U
#define ACTIVESEQ_SUBTYPE_SPECIAL_AI 10U
#define ACTIVESEQ_SUBTYPE_LIMIT 14U
#define ACTIVESEQ_NO_SPRITE 0xffffU
#define ACTIVESEQ_SPRITE_NO_ACTIVATE 0x01000000UL
typedef struct {uint16_t x,y;uint32_t info;} EspMapSprite;
typedef struct {
    const uint8_t *mapSprites;
    uint32_t mapSpriteCount, mapSpriteBytes;
} EspMapRuntimeView;
typedef struct {
    const uint8_t* entityTypes, *entitySubTypes, *linkStatesLE;
    uint32_t spriteCount;
} EspMapSpriteTopologyView;
typedef struct {
    uint16_t spriteIndex,tileIndex,worldX,worldY;
} EspNativeGameplayMonsterPositionRecord;
typedef struct {uint32_t leaf;} EspNativeBspVisibilityState;

static uint8_t rawBytes[15],entityTypes[3],subtypes[3],linkBytes[6];
static EspMapRuntimeView mapView;
static EspMapSpriteTopologyView topology;
static EspNativeGameplayMonsterPositionRecord moved;
static uint8_t visualState, crateFail, crateChange, consumed, projected;
static uint8_t topReady, topViewReady, poseSprite, visibleLeaf, rawViewReady;
static int rawCalls, crateCalls, consumedCalls, topCalls, positionCalls;
static int leafCalls, activationCalls;
static uint16_t lastActivatedSprite, lastActivatedTile;
static uint8_t lastActivatedSubtype;
static void reset(void) {
    memset(rawBytes,0,sizeof(rawBytes));
    memset(entityTypes,0,sizeof(entityTypes));
    memset(subtypes,0,sizeof(subtypes));
    memset(linkBytes,0,sizeof(linkBytes));
    rawBytes[5]=10; rawBytes[6]=20; rawBytes[7]=5;
    mapView=(EspMapRuntimeView){rawBytes,3U,sizeof(rawBytes)};
    topology=(EspMapSpriteTopologyView){entityTypes,subtypes,linkBytes,3U};
    entityTypes[1]=1U; subtypes[1]=2U;
    linkBytes[2]=(uint8_t)(0x42U); linkBytes[3]=0x14U; /* tile 66, linked+alive */
    moved=(EspNativeGameplayMonsterPositionRecord){1U,66U,320U,384U};
    visualState=0U;crateFail=crateChange=consumed=projected=0U;
    topReady=topViewReady=poseSprite=visibleLeaf=rawViewReady=1U;
    rawCalls=crateCalls=consumedCalls=topCalls=positionCalls=0;
    leafCalls=activationCalls=0;
    lastActivatedSprite=lastActivatedTile=0xffffU;
    lastActivatedSubtype=0xffU;
}
static const EspMapRuntimeView* EspMapRuntime_view(void) {
    ++rawCalls;return rawViewReady?&mapView:NULL;
}
static int EspNativeGameplayCrateState_applyMapSprite(
    uint32_t idx,EspMapSprite* out) {
    assert(idx<3U&&out);++crateCalls;
    if (crateFail) return 0;
    if (crateChange) out->info|=0x00020000U;
    return 1;
}
static int EspNativeGameplayPlayerResources_isConsumed(uint32_t idx) {
    assert(idx<3U);++consumedCalls;return consumed;
}
static int EspMapSpriteTopology_isReady(void) {
    ++topCalls;return topReady;
}
static int EspMapSpriteTopology_getVisualState(uint32_t idx,uint8_t* out) {
    if (!topReady||idx>=3U||!out)return 0;
    *out=visualState;return 1;
}
static int EspMapSpriteTopology_getEntity(
    uint32_t idx,uint8_t* type,uint8_t* subtype,
    uint16_t* link,uint16_t* order) {
    if(!topReady||idx>=3U||!type||!subtype||!link||!order)return 0;
    *type=entityTypes[idx];*subtype=subtypes[idx];
    *link=(uint16_t)(linkBytes[idx*2U] |
                     ((uint16_t)linkBytes[idx*2U+1U]<<8));
    *order=9U;
    return 1;
}
static int EspNativeGameplayMonsterMovementPublish_isProjected(uint16_t idx) {
    return projected&&idx==1U;
}
static const EspNativeGameplayMonsterPositionRecord*
EspNativeGameplayMonsterPosition_find(uint16_t idx) {
    ++positionCalls;return idx==1U?&moved:NULL;
}
static int EspNativeGameplayMonsterAttackVisual_isPoseSprite(uint32_t idx) {
    assert(idx<3U);return poseSprite;
}
static int EspNativeBspVisibility_mapSpriteVisibleReadOnly(
    const EspNativeBspVisibilityState* state,uint32_t idx,uint32_t* leaf) {
    ++leafCalls;
    if(leaf)*leaf=UINT32_MAX;
    if(!state||idx>=mapView.mapSpriteCount||!rawViewReady)return 0;
    if(leaf)*leaf=state->leaf;
    return visibleLeaf;
}
static const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) {
    return topViewReady?&topology:NULL;
}
static int EspNativeGameplayMonsterActivation_observeVisible(
    uint16_t sprite,uint8_t subtype,uint16_t tile) {
    ++activationCalls;lastActivatedSprite=sprite;
    lastActivatedSubtype=subtype;lastActivatedTile=tile;return 1;
}
int EspMapRuntime_getMapSpriteBase(uint32_t,EspMapSprite*);
int EspMapRuntime_getMapSprite(uint32_t,EspMapSprite*);
int EspNativeBspVisibility_mapSpriteVisibleBase(
    const EspNativeBspVisibilityState*,uint32_t,uint32_t*);
int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState*,uint32_t,uint32_t*);
"""

CHECKS = r"""
int main(void) {
    EspMapSprite sprite;
    EspNativeBspVisibilityState state={3U};
    uint32_t leaf;
    reset();
    assert(EspMapRuntime_getMapSpriteBase(1U,&sprite));
    assert(sprite.x==80U&&sprite.y==160U&&sprite.info==5U);
    assert(rawCalls==1&&crateCalls==0&&topCalls==0);
    assert(!EspMapRuntime_getMapSpriteBase(3U,&sprite));
    assert(!EspMapRuntime_getMapSpriteBase(1U,NULL));
    mapView.mapSpriteBytes=14U;
    assert(!EspMapRuntime_getMapSpriteBase(1U,&sprite));
    mapView.mapSpriteBytes=15U;

    reset();
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert(sprite.x==80U&&sprite.y==160U&&sprite.info==5U);
    assert(rawCalls==1&&crateCalls==1&&consumedCalls==1&&topCalls==1);
    reset();crateChange=1U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&0x00020000U)!=0U&&crateCalls==1);
    reset();consumed=1U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&SPRITE_HIDDEN)!=0U);
    reset();crateFail=1U;
    assert(!EspMapRuntime_getMapSprite(1U,&sprite));
    assert(crateCalls==1&&consumedCalls==0);
    reset();topReady=0U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert(sprite.x==80U&&sprite.info==5U);

    reset();projected=1U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert(sprite.x==320U&&sprite.y==384U&&positionCalls==1);
    reset();projected=1U;moved.tileIndex=67U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert(sprite.x==80U&&sprite.y==160U);
    reset();projected=1U;linkBytes[3]=0x00U; /* moved corpse still uses position */
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert(sprite.x==320U&&sprite.y==384U);

    reset();visualState=SPRITE_COMBAT_ATTACK_PRIMARY_VISUAL;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&SPRITE_FIXED_ANIM)!=0U);
    reset();visualState=SPRITE_COMBAT_ATTACK_PRIMARY_VISUAL;poseSprite=0U;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&SPRITE_FIXED_ANIM)==0U);
    reset();visualState=SPRITE_COMBAT_ATTACK_ALTERNATE_VISUAL;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&SPRITE_FIXED_ANIM)!=0U);
    reset();visualState=SPRITE_COMBAT_PAIN_VISUAL;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&SPRITE_FIXED_ANIM)!=0U);
    reset();visualState=SPRITE_COMBAT_CORPSE_VISUAL;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&(SPRITE_FIXED_ANIM|SPRITE_SORT_BIAS))==
          (SPRITE_FIXED_ANIM|SPRITE_SORT_BIAS));
    reset();visualState=SPRITE_COMBAT_DEATH_VISUAL;
    assert(EspMapRuntime_getMapSprite(1U,&sprite));
    assert((sprite.info&(SPRITE_FIXED_ANIM|SPRITE_SORT_BIAS))==
          (SPRITE_FIXED_ANIM|SPRITE_SORT_BIAS));

    reset();leaf=88U;
    assert(EspNativeBspVisibility_mapSpriteVisibleBase(&state,1U,&leaf));
    assert(leaf==3U&&activationCalls==0&&leafCalls==1);
    reset();leaf=88U;
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&state,1U,&leaf));
    assert(leaf==3U&&activationCalls==0); /* preview must stay pure */
    reset();leaf=88U;
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(leaf==3U&&activationCalls==1);
    assert(lastActivatedSprite==1U&&lastActivatedSubtype==2U&&
           lastActivatedTile==66U);
    assert(crateCalls==0&&rawCalls==1); /* admission reads immutable BSP */
    reset();visibleLeaf=0U;leaf=88U;
    assert(!EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(leaf==3U&&activationCalls==0&&rawCalls==0);
    reset();topViewReady=0U;
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(activationCalls==0);
    reset();entityTypes[1]=12U;
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(activationCalls==0);
    reset();linkBytes[3]=0x04U; /* linked but not alive */
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(activationCalls==0);
    reset();linkBytes[3]=0x10U; /* alive but not linked */
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(activationCalls==0);
    reset();rawBytes[9]=0x01U; /* no-activate flag is high byte of info */
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf));
    assert(activationCalls==0);
    reset();rawViewReady=0U;
    assert(EspNativeBspVisibility_mapSpriteVisible(&state,1U,&leaf)==0);
    assert(activationCalls==0);
    reset();rawViewReady=0U;
    /* even if visibility passed via a synthetic leaf, raw canonical error stops */
    assert(!EspMapRuntime_getMapSpriteBase(1U,&sprite));
    reset();leaf=88U;
    assert(!EspNativeBspVisibility_mapSpriteVisible(&state,3U,&leaf));
    assert(leaf==UINT32_MAX&&activationCalls==0);

    puts("PASS RC21 six real C bodies: raw sprite, rendered sprite, "
         "BSP pure admission and single post-visible monster activation");
    return 0;
}
"""


def scan():
    config=(ROOT/"platformio.ini").read_text()
    base=config.split("[env:esp32-cyd]",1)[1].split(
        "[env:esp-cyd-inverted]",1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=",base,re.M))==4,
            "RC21 requires exactly 4 production GNU wrappers")
    old_aliases = []
    for symbol in ("EspMapRuntime_getMapSprite",
                   "EspNativeBspVisibility_mapSpriteVisible"):
        require("-Wl,--wrap="+symbol not in config,
                "retired GNU interposition returned: "+symbol)
        for file in SRC.iterdir():
            if file.suffix not in (".c",".cpp"):
                continue
            if re.search(r"\\b__(?:real|wrap)_"+symbol+r"\\b",
                         file.read_text()):
                old_aliases.append(file.name+":"+symbol)
    require(not old_aliases,
            "obsolete linker aliases: "+repr(sorted(old_aliases)))
    require("EspMapRuntime_getMapSpriteBase(" in SOURCES["activation"],
            "activation must inspect immutable source sprite (no recursion)")
    require("EspNativeBspVisibility_mapSpriteVisibleBase(" in SOURCES["activation"],
            "public activation must first pass pure BSP admission")
    require("EspMapRuntime_getMapSpriteBase(" in SOURCES["projection"],
            "rendered map sprite must start at immutable canonical source")
    require("EspNativeBspVisibility_mapSpriteVisibleReadOnly(" in SOURCES["bsp"],
            "BSP raw admission must reuse pure read-only leaf")
    print("PASS RC21 static owner inventory: 4+3 wrappers; no source aliases")


def run(bodies,good):
    full="\n\n".join((PREFIX,*bodies,CHECKS))
    with tempfile.TemporaryDirectory(prefix="arch-rc21-") as d:
        src=Path(d)/"real.c"
        exe=Path(d)/"real"
        src.write_text(full)
        p=subprocess.run(["cc","-std=c11","-O0","-Wall","-Wextra",
                          "-Werror",str(src),"-o",str(exe)],
                         capture_output=True,text=True)
        require(p.returncode==0,"host C fixture failed to compile: "+p.stderr)
        q=subprocess.run([str(exe)],capture_output=True,text=True)
        require((q.returncode==0)==good,
                ("real C failed: " if good else "mutant survived: ")+
                q.stdout+q.stderr)


def main():
    scan()
    bodies=[extract(SOURCES[k],sig) for k,sig in SIGNATURES]
    run(bodies,True)
    mutations=(
        (2,"(uint16_t)raw[0] << 3","(uint16_t)raw[0] << 2"),
        (2,"(uint16_t)raw[1] << 3","(uint16_t)raw[1] << 2"),
        (2,"(uint32_t)raw[2]","(uint32_t)raw[3]"),
        (3,"!EspNativeGameplayCrateState_applyMapSprite(index, outSprite)",
             "0"),
        (3,"outSprite->info |= SPRITE_HIDDEN;",
             "outSprite->info &= ~SPRITE_HIDDEN;"),
        (3,"EspNativeGameplayMonsterMovementPublish_isProjected((uint16_t)index)",
             "0"),
        (3,"monsterPosition->tileIndex ==",
             "monsterPosition->tileIndex !="),
        (3,"EspNativeGameplayMonsterAttackVisual_isPoseSprite(index)",
             "0"),
        (3,"outSprite->info |= SPRITE_SORT_BIAS;",
             "outSprite->info &= ~SPRITE_SORT_BIAS;"),
        (4,"EspNativeBspVisibility_mapSpriteVisibleReadOnly(",
             "EspNativeBspVisibility_mapSpriteVisible("),
        (5,"if (!visible || mapSpriteIndex > 0xffffU)",
             "if (visible || mapSpriteIndex > 0xffffU)"),
        (5,"type != ACTIVESEQ_TYPE_ENEMY","type == ACTIVESEQ_TYPE_ENEMY"),
        (5,"(sprite.info & ACTIVESEQ_SPRITE_NO_ACTIVATE) != 0U",
             "(sprite.info & ACTIVESEQ_SPRITE_NO_ACTIVATE) == 0U"),
        (5,"tile = (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);",
             "tile = 0U;"),
    )
    for which,original,changed in mutations:
        text=bodies[which]
        require(text.count(original)==1,"mutant anchor drift: "+original)
        variant=list(bodies)
        variant[which]=text.replace(original,changed,1)
        run(variant,False)
    print("PASS RC21: six real C bodies, 14 adversarial mutants rejected")


if __name__=="__main__":
    main()
