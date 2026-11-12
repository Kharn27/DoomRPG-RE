#!/usr/bin/env python3
"""Exercise production catalog preflight for drops absent from BSP sprite sets."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "ESP32/src/esp_native_sprite_renderer.c").read_text()

def body(signature):
    assert SRC.count(signature) == 1
    start = SRC.index(signature)
    depth = 0
    for i in range(SRC.index("{", start), len(SRC)):
        if SRC[i] == "{":
            depth += 1
        elif SRC[i] == "}":
            depth -= 1
            if not depth:
                return SRC[start:i + 1]
    raise AssertionError(signature)

prime = body("static int primePersistentDropSprites(")
cross = body("static int isLegacyCrossLogical(")
render = body("static int renderImpl(")
assert "primePersistentDropSprites(workspace->dropOrder, dropOrderCount)" in render
assert render.index("primePersistentDropSprites(workspace->dropOrder, dropOrderCount)") < render.index("EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)")
assert "EspNativeGraphicsCatalog_ensureSprite(resourceLogical)" in prime
assert "EspNativeGraphicsCatalog_findSprite(resourceLogical)" in prime

test_c = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS 8U
typedef struct { uint16_t defTile; uint8_t active, taken; } EspNativeGameplayMonsterDropRecord;
typedef struct { const EspNativeGameplayMonsterDropRecord *records; uint8_t active; } EspNativeGameplayMonsterDropView;
typedef struct { uint8_t slot, reserved[3]; int32_t sortZ; } PersistentDropOrder;
typedef enum {
    ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE = 2,
    ESP_NATIVE_GRAPHICS_CATALOG_PACK_BUSY = 3,
    ESP_NATIVE_GRAPHICS_CATALOG_OK = 10
} EspNativeGraphicsCatalogStatus;
static EspNativeGameplayMonsterDropRecord records[8];
static EspNativeGameplayMonsterDropView view = {records, 1};
static uint8_t present[256];
static int attempts, failId = -1, packOpen;
const EspNativeGameplayMonsterDropView* EspNativeGameplayMonsterDrop_view(void) {return &view;}
const void* EspNativeGraphicsCatalog_findSprite(uint16_t id) {return id < 256 && present[id] ? &present[id] : NULL;}
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_ensureSprite(uint16_t id) {
    assert(packOpen == 0);
    ++attempts;
    if ((int)id == failId) return ESP_NATIVE_GRAPHICS_CATALOG_PACK_BUSY;
    present[id] = 1; return ESP_NATIVE_GRAPHICS_CATALOG_OK;
}
__CROSS__
__PRIME__
int main(void) {
    PersistentDropOrder order[8] = {0};
    order[1].slot = 1;
    order[2].slot = 2;
    EspNativeGameplayMonsterDropRecord before[8];
    assert(primePersistentDropSprites(order, 0) == 1 && attempts == 0);
    records[0].active = 1; records[0].defTile = 100;
    records[1].active = 1; records[1].defTile = 84;
    records[2].active = 1; records[2].defTile = 100;
    memcpy(before, records, sizeof records);
    assert(primePersistentDropSprites(order, 3) == 1);
    assert(attempts == 2 && present[100] && present[83] && !present[84]);
    assert(primePersistentDropSprites(order, 3) == 1 && attempts == 2);
    assert(memcmp(before, records, sizeof records) == 0);
    records[0].defTile = 101; failId = 101;
    memcpy(before, records, sizeof records);
    assert(primePersistentDropSprites(order, 3) == 0);
    assert(attempts == 3 && !present[101] &&
           memcmp(before, records, sizeof records) == 0);
    failId = -1;
    assert(primePersistentDropSprites(order, 3) == 1 &&
           attempts == 4 && present[101]);
    view.active = 0;
    assert(primePersistentDropSprites(order, 3) == 0);
    view.active = 1; order[0].slot = 8;
    assert(primePersistentDropSprites(order, 3) == 0);
    order[0].slot = 0; records[0].taken = 1;
    assert(primePersistentDropSprites(order, 3) == 0);
    assert(primePersistentDropSprites(NULL, 1) == 0);
    assert(primePersistentDropSprites(order, 9) == 0);
    puts("MONSTERDROP-CATALOG PASS sparse, CROSS, retry, rollback and PAK lease");
    return 0;
}
"""
test_c = test_c.replace("__CROSS__", cross).replace("__PRIME__", prime)
with tempfile.TemporaryDirectory(prefix="monster-drop-") as tmp:
    file = Path(tmp) / "preflight.c"
    exe = Path(tmp) / "preflight"
    file.write_text(test_c)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(file), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
