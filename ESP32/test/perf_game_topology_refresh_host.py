#!/usr/bin/env python3
"""P3l: compare actual production topology refresh/FNV with frozen scalar bodies."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/esp_map_sprite_topology.c").read_text()

def extract(signature):
    assert SOURCE.count(signature) == 1, signature
    start = SOURCE.index(signature)
    depth = 0
    for i in range(SOURCE.index("{", start), len(SOURCE)):
        if SOURCE[i] == "{":
            depth += 1
        elif SOURCE[i] == "}":
            depth -= 1
            if not depth:
                return SOURCE[start:i+1]
    raise AssertionError(signature)

PREFIX = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_map_sprite_topology.h"
#define VISUAL_HIDDEN 0x80U
static uint8_t storage[7U*1024U+9U];
static uint8_t *topologyStorage, *entityTypes, *visualStates, *linkStatesLE;
static EspMapSpriteTopologyView topologyView;
static uint32_t rng=0x19abce43U;
static uint32_t next(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}
'''
REFERENCE = r'''
static uint32_t scalarHash(void) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (topologyStorage == NULL) return 0U;
    for (i = 0U; i < topologyView.storageBytes; ++i) {
        hash = hashByte(hash, topologyStorage[i]);
    }
    hash = hashByte(hash, (uint8_t)(topologyView.nextLinkOrder & 0xffU));
    return hashByte(hash,
                    (uint8_t)((topologyView.nextLinkOrder >> 8) & 0xffU));
}
static void scalarRefresh(void) {
    uint32_t i;
    uint16_t state;

    topologyView.entityCount = 0U;
    topologyView.linkedCount = 0U;
    topologyView.hiddenCount = 0U;
    topologyView.enemyCount = 0U;
    topologyView.humanCount = 0U;
    topologyView.destructibleCount = 0U;

    for (i = 0U; i < topologyView.spriteCount; ++i) {
        state = linkStateAt(i);
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U) {
            ++topologyView.entityCount;
            if ((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U) {
                ++topologyView.linkedCount;
            }
        }
        if ((visualStates[i] & VISUAL_HIDDEN) != 0U) {
            ++topologyView.hiddenCount;
        }
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_ENEMY) {
            ++topologyView.enemyCount;
        }
        else if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_HUMAN) {
            ++topologyView.humanCount;
        }
        else if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) {
            ++topologyView.destructibleCount;
        }
    }
    topologyView.stateFNV1a = scalarHash();
}
'''
CHECKS = r'''
int main(void) {
    unsigned n, trial, i;
    /* Test all tail lengths, including unaligned input and empty buffers. */
    for (n=0; n<=7U*1024U; ++n) {
        topologyStorage=storage+1U;
        topologyView.storageBytes=n;
        topologyView.nextLinkOrder=(uint16_t)next();
        for (i=0;i<n;++i) topologyStorage[i]=(uint8_t)next();
        assert(stateHash()==scalarHash());
    }
    topologyStorage=NULL;
    assert(stateHash()==scalarHash());
    for (trial=0;trial<1025U;++trial) {
        n=trial;
        topologyStorage=storage+(trial&1U);
        entityTypes=topologyStorage;
        visualStates=topologyStorage+2U*n;
        linkStatesLE=topologyStorage+3U*n;
        memset(&topologyView,0,sizeof(topologyView));
        topologyView.spriteCount=n;
        topologyView.storageBytes=n*7U;
        topologyView.nextLinkOrder=(uint16_t)next();
        for (i=0;i<7U*n;++i) topologyStorage[i]=(uint8_t)next();
        for (i=0;i<n;++i) entityTypes[i]=(uint8_t)(next()%17U);
        /* Repeated changes to link, visual and type bytes model relink,
         * script mutations and their restoration; other view fields survive. */
        for (i=0;i<9U;++i) {
            EspMapSpriteTopologyView expected;
            if(n) topologyStorage[next()%(7U*n)]^=(uint8_t)next();
            topologyView.entityDefCount=next();
            scalarRefresh(); expected=topologyView;
            topologyView.entityCount=topologyView.linkedCount=UINT32_MAX;
            topologyView.hiddenCount=topologyView.enemyCount=UINT32_MAX;
            topologyView.humanCount=topologyView.destructibleCount=UINT32_MAX;
            topologyView.stateFNV1a=0U;
            refreshView();
            assert(memcmp(&expected,&topologyView,sizeof(expected))==0);
        }
    }
    puts("PASS P3l topology exact FNV all lengths and refresh counters 0..1024 sprites");
    return 0;
}
'''
body = PREFIX + "\n".join(extract(s) for s in (
    "static uint16_t readLe16(", "static uint32_t hashByte(",
    "static uint32_t stateHash(", "static uint16_t linkStateAt(",
    "static void refreshView(")) + REFERENCE + CHECKS
with tempfile.TemporaryDirectory(prefix="p3l-topology-") as tmp:
    path=Path(tmp)/"test.c"
    exe=Path(tmp)/"test"
    path.write_text(body)
    subprocess.run(shlex.split(os.environ.get("CC","cc"))+
        ["-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT/"include"),
         str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
