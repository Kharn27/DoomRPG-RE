#!/usr/bin/env python3
"""P2Z: extract real cache and read core, test immutable absolute-byte identity."""
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

# The production physical owner must clear ranges when backing changes;
# logical opens on the same physical owner do not clear them.
assert "clearResidentCache();" in body("void resetPhysicalState()")
assert "resetPhysicalState();" in body("int EspAssetPack_open(")
assert "resetPhysicalState();" in body("int EspAssetPack_mapFlashPrepare(")
assert "resetPhysicalState();" in body("int EspAssetPack_residentEnd(")
layout = src[src.index("constexpr uint32_t kResidentRangeCapacityBytes"):src.index("constexpr uint32_t kMapFlashMagic")]
layout += src[src.index("struct ResidentRangeRecord {"):src.index("struct MapFlashExcludedSpan {")]
functions = "\n".join(body(s) for s in (
    "void clearResidentCache()", "bool validateEntry(",
    "bool isLargeResidentRange(", "void eraseResidentRange(",
    "uint32_t lowestLargeRangeOffset()", "bool evictLowestLargeRange()",
    "void recycleResidentRangeWorkingSet()", "ResidentRangeRecord* findResidentRange(",
    "void storeResidentSmallRange(", "void storeResidentLargeRange(",
    "static int readRangeCore("))
harness = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "esp_asset_pack.h"
""" + layout + r"""
static_assert(sizeof(ResidentCache)==18088U,"1024 bytes reclaimed");
static ResidentCache cache{};
static ResidentCache* residentCache=&cache;
static EspAssetPackResidentStats residentStats{};
static bool residentEnabled=true, residentLargeRangeEnabled=true;
static bool openReady=true, physicalUsesMapFlash=true, sourceProbeReady=false;
static uint8_t sourceProbeReadLogged=0;
static EspAssetPackEntry sourceProbeEntry{};
static uint32_t packSize=65536,dataOffset=32;
static uint32_t reads=0, generation=0, strictMisses=0;
static uint8_t byteAt(uint32_t n) { return (uint8_t)((n^(n>>8)^generation)&255); }
struct File {
    uint32_t pos=0;
    bool seek(uint32_t p) {pos=p;return true;}
    size_t read(uint8_t* dst,size_t n) {
        ++reads;
        for(size_t i=0;i<n;++i)dst[i]=byteAt(pos+i);
        pos+=(uint32_t)n;return n;
    }
};
static File packFile,sourceProbeFile;
static bool sourceProbeEntryExact(const EspAssetPackEntry& e) {
    return sourceProbeReady && memcmp(&e,&sourceProbeEntry,sizeof(e))==0;
}
static bool mapFlashTranslate(uint32_t a,uint32_t n,uint32_t* out) {
    if(a>=60000 || n>60000-a)return false;
    *out=a;return true;
}
static bool mapFlashRead(uint32_t a,void* d,size_t n,bool) {
    packFile.seek(a);return packFile.read((uint8_t*)d,n)==n;
}
static bool readSdExact(void* d,size_t n){return packFile.read((uint8_t*)d,n)==n;}
static void logMapFlashStrictMiss(const EspAssetPackEntry*,uint32_t,size_t){++strictMisses;}
""" + functions + r"""
static void expect(const EspAssetPackEntry& e,uint32_t off,uint32_t n) {
    uint8_t out[2048];
    assert(n<=sizeof(out) && readRangeCore(&e,off,out,n));
    for(uint32_t i=0;i<n;++i)assert(out[i]==byteAt(e.offset+off+i));
}
int main() {
    EspAssetPackEntry a={11,1000,4000,0,0}, b={11,8000,4000,0,0};
    EspAssetPackEntry alias={22,900,4100,0,0};
    expect(a,24,32);assert(reads==1);
    expect(a,24,32);assert(reads==1);
    expect(b,24,32);assert(reads==2); // equal hash, different absolute offset
    expect(alias,124,32);assert(reads==2); // same immutable bytes
    expect(a,24,31);assert(reads==3); // length is still exact
    expect(a,64,2048);expect(a,64,2048);assert(reads==4);
    uint8_t out[2048];
    assert(!readRangeCore(&a,3999,out,2));
    assert(!readRangeCore(&a,UINT32_MAX,out,2));
    assert(!readRangeCore(&a,24,nullptr,32));
    EspAssetPackEntry excluded={33,60000,100,0,0};
    assert(!readRangeCore(&excluded,0,out,4) && strictMisses==1);
    // Independent SD probe neither consumes nor contaminates the cached range.
    sourceProbeReady=true;sourceProbeEntry=a;generation=1;
    expect(a,24,32);assert(reads==5);
    assert(!readRangeCore(&b,24,out,32));
    sourceProbeReady=false;generation=0;
    expect(a,24,32);assert(reads==5);
    clearResidentCache();generation=2;residentLargeRangeEnabled=true;
    expect(a,24,32);assert(reads==6);
    // Exercise original mixed small/large eviction and record-full recycling.
    EspAssetPackEntry many={44,10000,40000,0,0};
    for(uint32_t i=0;i<7;++i)expect(many,i*2048,2048);
    for(uint32_t i=0;i<350;++i)expect(many,i*64,64);
    assert(cache.rangeCount<=256 && cache.bytesUsed<=15360);
    assert(cache.bytesUsed<=lowestLargeRangeOffset());
    for(uint32_t i=0;i<cache.rangeCount;++i)
        assert((uint32_t)cache.ranges[i].dataOffset+cache.ranges[i].length<=15360);
    clearResidentCache();physicalUsesMapFlash=false;generation=3;
    expect(a,24,32);
    puts("PASS P2Z real cache/read core: exact offset+length, SD/flash/probe/eviction; owner=18088");
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-p2z-") as tmp:
    p=Path(tmp)/"test.cpp"
    binary=Path(tmp)/"test"
    p.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CXX","c++"))+
        ["-std=c++11","-O2","-Wall","-Wextra","-Werror","-I",
         str(root/"ESP32/include"),str(p),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
