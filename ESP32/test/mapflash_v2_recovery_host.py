#!/usr/bin/env python3
"""P4b: execute real stage and prepare on fake NOR; inject interrupted writes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "ESP32/src/esp_asset_pack.cpp").read_text()
def body(sig):
    start = src.index(sig)
    depth = 0
    for i in range(src.index("{", start), len(src)):
        if src[i] == "{": depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0: return src[start:i+1]
    raise AssertionError(sig)

open_body = body("int EspAssetPack_open(")
assert open_body.index("if (!validateEntry(entry)") < open_body.index("mapFlashTryBootVerified(plan)") < open_body.index("physicalUsesMapFlash = true;", open_body.index("mapFlashTryBootVerified(plan)"))
assert "EspAssetPack_mapFlashStage(" not in body("bool mapFlashTryBootVerified(")

constants = src[src.index("constexpr uint32_t kMapFlashMagic"):src.index("constexpr uint8_t kMapFlashMaxMissLogs")]
layout = src[src.index("struct MapFlashHeader {"):src.index("File packFile;")]
harness = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_asset_pack.h"
constexpr uint32_t ESP_MAP_CATALOG_COUNT=13;
constexpr size_t kHeaderBytes=24,kEntryBytes=20;
struct esp_partition_t {uint32_t size;};
constexpr int ESP_OK=0, ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2;
""" + constants + layout + r"""
static esp_partition_t part{65536};
static uint8_t flash[65536], sd[9064], baseline[65536];
static MapFlashState mapFlash{};
static MapFlashPlan source{};
static uint32_t verifiedFlashEpoch=1, packSize=sizeof(sd), indexOffset=24,dataOffset=64,entryCount=2;
static bool openReady=false,residentEnabled=false,sourceProbeReady=false;
static unsigned mutations=0,cutAt=0,cutMode=0,readCalls=0,readFailAt=0;
static bool rejectSd=false;
static int64_t esp_timer_get_time(){return 100;}
static const esp_partition_t* esp_partition_find_first(int,int,const char*){return &part;}
static int esp_partition_read(const esp_partition_t* p,uint32_t off,void* dst,size_t n){
    assert(p==&part && off<=p->size && n<=p->size-off);
    if(++readCalls==readFailAt)return -1;
    memcpy(dst,flash+off,n);return ESP_OK;
}
static size_t allowed(size_t n) {
    ++mutations;
    if(mutations!=cutAt)return n;
    return cutMode==0?0:cutMode==1?n/2:n;
}
static int esp_partition_erase_range(const esp_partition_t* p,uint32_t off,size_t n){
    assert(p==&part && off%4096==0 && n%4096==0 && off<=p->size && n<=p->size-off);
    memset(flash+off,0xff,allowed(n));return mutations==cutAt?-1:ESP_OK;
}
static int esp_partition_write(const esp_partition_t* p,uint32_t off,const void* data,size_t n){
    assert(p==&part && off<=p->size && n<=p->size-off);
    size_t k=allowed(n);auto* b=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<k;++i){assert((flash[off+i]&b[i])==b[i]);flash[off+i]&=b[i];}
    return mutations==cutAt?-1:ESP_OK;
}
struct File {
    uint32_t pos=0;
    explicit operator bool()const{return openReady;}
    bool seek(uint32_t p){if(p>sizeof(sd))return false;pos=p;return true;}
    size_t read(uint8_t* d,size_t n){
        if(rejectSd || n>sizeof(sd)-pos)return 0;
        memcpy(d,sd+pos,n);pos+=(uint32_t)n;return n;
    }
};
static File packFile;
static void notifyMapFlashProgress(uint8_t,uint32_t,uint32_t){packFile.pos=1;}
static void resetPhysicalState(){openReady=false;}
static int EspMapCatalog_isValidId(uint8_t id){return id>=1 && id<=13;}
static const char* EspMapCatalog_nameForId(uint8_t){return "/map.bsp";}
uint32_t EspAssetPack_nameHash(const char*){return 1;}
int EspAssetPack_open(const char*){openReady=true;return 1;}
void EspAssetPack_close(){openReady=false;}
int EspAssetPack_isOpen(){return openReady;}
static bool mapFlashBuildSourcePlan(uint8_t,const esp_partition_t*,MapFlashPlan* out,const char**){*out=source;return true;}
static void printMapFlashV2Audit(const MapFlashPlan&,const esp_partition_t*){}
""" + "\n".join(body(s) for s in (
    "uint32_t readLe32(", "uint32_t fnv1aUpdate(", "void decodeEntry(",
    "uint32_t alignSector(", "void clearMapFlashRuntime(",
    "bool mapFlashHeaderLooksValid(", "bool mapFlashHeaderMatchesPlan(",
    "bool fnvFlashRange(", "bool fnvFlashRangeProgress(", "bool copySdRangeToFlash(",
    "uint32_t mapFlashCrc32Update(", "template <typename Read>\nbool mapFlashValidateEntryCrcs("
)) + "\n" + src[src.index("struct MapFlashCrcProof {"):src.index("bool mapFlashTranslate(")] + r"""
int EspAssetPack_findEntry(const char*,EspAssetPackEntry* e){decodeEntry(sd+24,e);return 1;}
""" + body("bool mapFlashTryBootVerified(") + "\n" + body("int EspAssetPack_mapFlashStage(") + "\n" + body("int EspAssetPack_mapFlashPrepare(") + r"""
static void put(uint32_t off,uint32_t x){for(unsigned i=0;i<4;++i)sd[off+i]=(uint8_t)(x>>(8*i));}
static void makeSource(){
    memset(sd,0,sizeof(sd));
    for(unsigned i=64;i<sizeof(sd);++i)sd[i]=(uint8_t)(i*7U);
    put(24,1);put(28,64);put(32,5000);put(36,mapFlashCrc32Update(UINT32_MAX,sd+64,5000)^UINT32_MAX);
    put(44,2);put(48,5064);put(52,4000);put(56,mapFlashCrc32Update(UINT32_MAX,sd+5064,4000)^UINT32_MAX);
    source={};source.sourcePackBytes=sizeof(sd);source.sourceIndexOffset=24;source.sourceDataOffset=64;
    source.sourceDataBytes=source.stagedBytes=9000;source.entryCount=2;source.indexBytes=40;
    source.payloadFlashOffset=8192;source.indexFNV1a=fnv1aUpdate(2166136261U,sd+24,40);source.currentMapHash=1;
}
static void reboot(){
    clearMapFlashRuntime(true);mapFlashCrcProof={};verifiedFlashEpoch=1;openReady=false;
    mutations=cutAt=cutMode=readCalls=readFailAt=0;rejectSd=false;
}
static void exact(){
    MapFlashHeader hdr{};memcpy(&hdr,flash,sizeof(hdr));
    assert(mapFlashHeaderLooksValid(hdr,&part));
    assert(mapFlash.active && mapFlash.stats.verified && !openReady);
    assert(memcmp(flash+4096,sd+24,40)==0);
    assert(memcmp(flash+8192,sd+64,9000)==0);
}
int main(){
    makeSource();reboot();memset(flash,0xff,sizeof(flash));
    assert(EspAssetPack_mapFlashStage(1));exact();
    const unsigned total=mutations, reads=readCalls;
    assert(total>=8 && reads>0);
    memcpy(baseline,flash,sizeof(flash));
    reboot();assert(EspAssetPack_mapFlashPrepare(1));exact();assert(mutations==0);
    // P5a first-open read-only proof: never writes or grants target handoff.
    reboot();assert(mapFlashTryBootVerified(source));exact();assert(!mapFlash.handoffEligible && mutations==0);
    reboot();source.indexFNV1a^=1;assert(!mapFlashTryBootVerified(source));assert(!mapFlash.active && mutations==0);source.indexFNV1a^=1;
    reboot();flash[8192]^=1;assert(!mapFlashTryBootVerified(source));assert(!mapFlash.active && mutations==0);flash[8192]^=1;
    reboot();readFailAt=1;assert(!mapFlashTryBootVerified(source));assert(!mapFlash.active && mutations==0);
    reboot();memset(flash,0xff,sizeof(flash));assert(!mapFlashTryBootVerified(source));assert(!mapFlash.active && mutations==0);
    memcpy(flash,baseline,sizeof(flash));
    // Bad authoritative source must not erase a known good committed image.
    reboot();sd[100]^=1;assert(!EspAssetPack_mapFlashStage(1));
    assert(mutations==0 && !mapFlash.active && !openReady && !memcmp(flash,baseline,sizeof(flash)));
    sd[100]^=1;
    reboot();rejectSd=true;assert(!EspAssetPack_mapFlashStage(1));
    assert(mutations==0 && !mapFlash.active && !openReady && !memcmp(flash,baseline,sizeof(flash)));
    // Every mutating call: no bytes, half bytes, all bytes then reported failure.
    // Start with committed old cache, observe on-flash marker across reboot.
    for(unsigned op=1;op<=total;++op)for(unsigned mode=0;mode<3;++mode){
        memcpy(flash,baseline,sizeof(flash));reboot();cutAt=op;cutMode=mode;
        assert(!EspAssetPack_mapFlashStage(1));
        assert(!mapFlash.active && !mapFlash.handoffEligible && !openReady);
        MapFlashHeader hdr{};memcpy(&hdr,flash,sizeof(hdr));
        const bool committed=mapFlashHeaderLooksValid(hdr,&part);
        assert(committed==((op==1 && mode==0)||(op==total && mode==2)));
        reboot();assert(EspAssetPack_mapFlashPrepare(1));exact();
        assert((mutations==0)==committed);
    }
    // Flash read failure at every stage readback operation, including header readback.
    for(unsigned op=1;op<=reads;++op){
        memcpy(flash,baseline,sizeof(flash));reboot();readFailAt=op;
        assert(!EspAssetPack_mapFlashStage(1));
        assert(!mapFlash.active && !openReady);
        reboot();assert(EspAssetPack_mapFlashPrepare(1));exact();
    }
    reboot();flash[8192]^=1;assert(EspAssetPack_mapFlashPrepare(1));exact();assert(mutations>0);
    reboot();flash[4096]^=1;assert(EspAssetPack_mapFlashPrepare(1));exact();assert(mutations>0);
    printf("PASS MAPFLASH P4b real stage/prepare NOR fault matrix: %u write/erase cuts, %u read failures; source preflight preserves old cache\n",total*3,reads);
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"test.cpp";p.write_text(harness)
    exe=Path(tmp)/"test"
    subprocess.run(["c++","-std=c++11","-O2","-Wall","-Wextra","-Werror","-I",str(root/"ESP32/include"),str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
