#!/usr/bin/env python3
"""Compare the actual P2k sorted lookups to a linear oracle, all uint16 keys."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def extract(path, signature):
    data = (ROOT / "src" / path).read_text()
    assert data.count(signature) == 1
    start = data.index(signature)
    depth = 0
    for i in range(data.index("{", start), len(data)):
        if data[i] == "{":
            depth += 1
        elif data[i] == "}":
            depth -= 1
            if not depth:
                return data[start:i+1]
    raise AssertionError(signature)

prefix = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_position.h"

static EspNativeGameplayMonsterRecord records[100];
static EspNativeGameplayMonsterPositionRecord positions[100];
static EspNativeGameplayMonsterView monsters;
static EspNativeGameplayMonsterPositionView positionView;
static EspNativeGameplayMonsterPositionRecord* positionRecords = positions;
static const EspNativeGameplayMonsterView* published = &monsters;
const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void) {
    return published;
}
'''
checks = r'''
int main(void) {
    uint32_t n, key, j, index;
    monsters.records = records;
    for (n=0;n<=100U;++n) {
        monsters.count = positionView.count = n;
        for (j=0;j<n;++j) {
            records[j].spriteIndex = positions[j].spriteIndex =
                (uint16_t)(j * 641U + (j % 7U));
        }
        for (key=0;key<=UINT16_MAX;++key) {
            uint32_t expected=n;
            for (j=0;j<n;++j)
                if (records[j].spriteIndex == key) { expected=j; break; }
            index=UINT32_MAX;
            assert(projectionRecordIndex((uint16_t)key,&index)==(expected<n));
            assert(index==(expected<n ? expected : 0U));
            assert(findMutable((uint16_t)key)==
                   (expected<n ? &positions[expected] : NULL));
        }
    }
    /* Guards and upper boundary; never dereference invalid view. */
    monsters.count=101U;
    assert(!projectionRecordIndex(0U,&index) && index==0U);
    monsters.count=1U;
    records[0].spriteIndex=positions[0].spriteIndex=UINT16_MAX;
    positionView.count=1U;
    assert(projectionRecordIndex(UINT16_MAX,&index) && index==0U);
    assert(findMutable(UINT16_MAX)==positions);
    assert(!projectionRecordIndex(0U,NULL));
    published=NULL;
    assert(!projectionRecordIndex(0U,&index) && index==0U);
    published=&monsters; monsters.records=NULL;
    assert(!projectionRecordIndex(0U,&index) && index==0U);
    positionRecords=NULL;
    assert(findMutable(UINT16_MAX)==NULL);
    puts("PASS P2k exact ordinal and position pointer, all 65536 keys x 101 sizes");
    return 0;
}
'''
body = prefix + extract("esp_native_gameplay_monster_movement_publish.c",
                       "static int projectionRecordIndex(") + "\n" + extract(
    "esp_native_gameplay_monster_position.c",
    "static EspNativeGameplayMonsterPositionRecord* findMutable(") + checks
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / "lookup.c"
    exe = Path(tmp) / "lookup"
    c.write_text(body)
    subprocess.run(shlex.split(os.environ.get("CC","cc")) +
                   ["-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT/"include"),str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
