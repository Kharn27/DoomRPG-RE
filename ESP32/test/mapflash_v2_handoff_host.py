#!/usr/bin/env python3
"""P2: execute the production prepare/header/FNV/invalidation code with fake flash."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "ESP32/src/esp_asset_pack.cpp").read_text()

def body(signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError(signature)

# The only production writer starts by invalidating the active proof.
stage = body("int EspAssetPack_mapFlashStage(")
assert stage.index("clearMapFlashRuntime(true);") < stage.index("esp_partition_erase_range(")
assert "mapFlash.handoffEligible = 1U;" in stage
assert "clearMapFlashRuntime(true);" in body("int EspAssetPack_residentEnd(")
assert "clearMapFlashRuntime(true);" in body("void EspAssetPack_mapFlashDeactivate(")

layout = src[src.index("struct MapFlashHeader {"):src.index("File packFile;")]
constants = src[src.index("constexpr uint32_t kMapFlashMagic"):src.index("constexpr uint32_t kMapFlashEraseChunkBytes")]
harness = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_asset_pack.h"
constexpr uint32_t ESP_MAP_CATALOG_COUNT=13;
struct esp_partition_t { uint32_t size; };
constexpr int ESP_OK=0, ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2;
constexpr size_t kHeaderBytes=24, kEntryBytes=20;
""" + constants + layout + r"""
static_assert(sizeof(MapFlashHeader)==60, "no cache format migration");
static esp_partition_t a{65536}, b{65536};
static const esp_partition_t* selected=&a;
static uint8_t flash[65536];
static MapFlashPlan source{};
static MapFlashState mapFlash{};
static uint32_t verifiedFlashEpoch=1, indexReads=0, payloadReads=0, plans=0, stages=0;
static bool openReady=false, residentEnabled=false, sourceProbeReady=false, planOk=true;
static bool rejectIndexRead=false, rejectPayloadRead=false;
static int64_t esp_timer_get_time() { return 100; }
static const esp_partition_t* esp_partition_find_first(int,int,const char*) { return selected; }
static int esp_partition_read(const esp_partition_t* p,uint32_t off,void* dst,size_t n) {
    assert(p==selected && off<=p->size && n<=p->size-off);
    if(off==kMapFlashIndexOffset) { ++indexReads; if(rejectIndexRead) return -1; }
    if(off>=source.payloadFlashOffset) { ++payloadReads; if(rejectPayloadRead) return -1; }
    memcpy(dst,flash+off,n); return ESP_OK;
}
static int EspMapCatalog_isValidId(uint8_t id) { return id>=1 && id<=13; }
static const char* EspMapCatalog_nameForId(uint8_t) { return "/map.bsp"; }
static bool mapFlashBuildSourcePlan(uint8_t id,const esp_partition_t*,MapFlashPlan* out,const char**) {
    ++plans;
    if(!planOk) return false;
    *out=source; out->currentMapHash=0x100U+id; return true;
}
static void printMapFlashV2Audit(const MapFlashPlan&,const esp_partition_t*) {}
static void resetPhysicalState() { openReady=false; }
""" + "\n".join(body(s) for s in (
    "uint32_t fnv1aUpdate(", "uint32_t alignSector(",
    "void clearMapFlashRuntime(", "bool mapFlashHeaderLooksValid(",
    "bool mapFlashHeaderMatchesPlan(", "bool fnvFlashRange("
)) + r"""
// P4a CRC resource scanner has its own production-body oracle.
static bool mapFlashEnsureEntryCrcs(const MapFlashPlan&, uint32_t, uint8_t*) { return true; }
int EspAssetPack_mapFlashStage(uint8_t) {
    ++stages;
    assert(!mapFlash.active && !mapFlash.handoffEligible);
    return 0; // fail-closed mock rebuild; actual stage covered by P1 tests
}
""" + body("int EspAssetPack_mapFlashPrepare(") + r"""
static MapFlashHeader original{};
static void setup() {
    selected=&a; openReady=residentEnabled=sourceProbeReady=false;
    rejectIndexRead=rejectPayloadRead=false; planOk=true;
    verifiedFlashEpoch=1; clearMapFlashRuntime(true);
    indexReads=payloadReads=plans=stages=0;
    memset(flash,0,sizeof(flash));
    source={}; source.sourceIndexOffset=24; source.entryCount=2;
    source.indexBytes=40; source.sourceDataOffset=64;
    source.stagedBytes=source.sourceDataBytes=9000;
    source.sourcePackBytes=9064; source.payloadFlashOffset=8192;
    for(uint32_t i=4096;i<4136;++i) flash[i]=(uint8_t)i;
    for(uint32_t i=8192;i<17192;++i) flash[i]=(uint8_t)(i*7U);
    source.indexFNV1a=fnv1aUpdate(2166136261U,flash+4096,40);
    original={}; original.magic=kMapFlashMagic; original.version=kMapFlashVersion;
    original.headerBytes=sizeof(original); original.committed=kMapFlashCommitted;
    original.sourcePackBytes=source.sourcePackBytes;
    original.sourceIndexOffset=source.sourceIndexOffset;
    original.sourceDataOffset=source.sourceDataOffset;
    original.entryCount=source.entryCount; original.indexFlashOffset=4096;
    original.indexBytes=source.indexBytes; original.payloadFlashOffset=8192;
    original.stagedBytes=9000; original.indexFNV1a=source.indexFNV1a;
    original.payloadFNV1a=fnv1aUpdate(2166136261U,flash+8192,9000);
    original.currentMapId=9; original.currentMapHash=0x109;
    memcpy(flash,&original,sizeof(original));
}
static void full(uint8_t id=2) {
    const uint32_t i=indexReads,p=payloadReads,n=plans;
    assert(EspAssetPack_mapFlashPrepare(id)==1);
    assert(indexReads==i+1 && payloadReads==p+3 && plans==n+1);
    assert(mapFlash.active && mapFlash.stats.verified && mapFlash.handoffEligible);
    assert(mapFlash.header.currentMapId==id);
}
static void miss(uint8_t id=2) {
    const uint32_t n=stages;
    assert(!EspAssetPack_mapFlashPrepare(id));
    assert(stages==n+1 && !mapFlash.active && !mapFlash.handoffEligible);
}
int main() {
    setup(); full();
    assert(EspAssetPack_mapFlashPrepare(2));
    assert(indexReads==2 && payloadReads==3 && plans==2);
    assert(mapFlash.active && mapFlash.stats.verified && !mapFlash.handoffEligible);
    full(); // cannot chain skipped proofs
    assert(!memcmp(flash,&original,sizeof(original))); // header never rewritten
    setup(); full(); full(9); // other map cannot adopt
    setup(); full(); clearMapFlashRuntime(true); full(); // reboot/deactivate
    setup(); full(); selected=&b; full(); // different partition identity
    setup(); full(); verifiedFlashEpoch=UINT32_MAX-1;
    assert(EspAssetPack_mapFlashPrepare(2) && payloadReads==6); // saturation
    setup(); full(); source.indexFNV1a^=1; miss(); // SD identity changed
    setup(); full(); flash[4096]^=1; miss(); // flash index is always checked
    setup(); full(); rejectIndexRead=true; miss();
    setup(); full(); original.payloadFNV1a^=1;
    memcpy(flash,&original,sizeof(original)); miss(); // changed header FNV -> full read
    assert(payloadReads==6);
    setup(); full(); original.committed=0; memcpy(flash,&original,sizeof(original)); miss();
    setup(); flash[8192]^=1; miss(); // cold payload corruption
    setup(); rejectPayloadRead=true; miss();
    setup(); full(); planOk=false; assert(!EspAssetPack_mapFlashPrepare(2));
    assert(!mapFlash.active && !mapFlash.handoffEligible && stages==0);
    setup(); full(); openReady=true; assert(!EspAssetPack_mapFlashPrepare(2));
    assert(!mapFlash.handoffEligible); openReady=false; full();
    setup(); full(); residentEnabled=true; assert(!EspAssetPack_mapFlashPrepare(2));
    assert(!mapFlash.handoffEligible); residentEnabled=false; full();
    setup(); full(); sourceProbeReady=true; assert(!EspAssetPack_mapFlashPrepare(2));
    assert(!mapFlash.handoffEligible); sourceProbeReady=false; full();
    setup(); full(); assert(!EspAssetPack_mapFlashPrepare(0));
    assert(!mapFlash.handoffEligible); full();
    puts("PASS MAPFLASH-V2 P2 real prepare: one-shot same-target proof, cold/full checks, index/header mismatch and boundary rejection");
}
"""
# No ESP-IDF runtime is linked here.
with tempfile.TemporaryDirectory(prefix="mapflash-p2-") as tmp:
    path = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    path.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) +
                   ["-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "ESP32/include"), str(path), "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True)
