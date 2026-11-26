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
    "bool isLargeResidentRange(", "uint32_t residentLargeRangeCount()",
    "uint32_t residentPayloadBytesUsed()", "void eraseResidentRange(",
    "uint32_t lowestLargeRangeOffset()", "bool evictLowestLargeRange()",
    "void clearLargeResidentRanges()", "void recycleResidentRangeWorkingSet()", "ResidentRangeRecord* findResidentRange(",
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
static void checkLayout() {
    uint32_t count=0,lowest=15360,small=0;
    bool occupied[7]={};
    for(uint32_t i=0;i<cache.rangeCount;++i) {
        const auto& r=cache.ranges[i];
        assert((uint32_t)r.dataOffset+r.length<=15360);
        assert((r.length==2048) == (i>=cache.rangeCount-cache.largeCount));
        if(r.length==2048) {
            ++count;
            assert(r.dataOffset==15360-count*2048);
            assert((15360-r.dataOffset)%2048==0);
            uint32_t slot=(15360-r.dataOffset)/2048;
            assert(slot>=1 && slot<=7 && !occupied[slot-1]);
            occupied[slot-1]=true;
            if(r.dataOffset<lowest)lowest=r.dataOffset;
        } else {
            assert(r.dataOffset==small);
            small+=r.length;
        }
    }
    for(uint32_t i=0;i<count;++i)assert(occupied[i]);
    assert(count==residentLargeRangeCount() && lowest==lowestLargeRangeOffset());
    assert(small==cache.bytesUsed && small<=lowest);
    assert(residentPayloadBytesUsed()==small+count*2048);
}
static void expect(const EspAssetPackEntry& e,uint32_t off,uint32_t n) {
    uint8_t out[2048];
    // Independent old full-scan lookup, including same offset/different size.
    ResidentRangeRecord* expected=nullptr;
    if(residentEnabled && (n<=1024 || n==2048)) {
        for(uint32_t i=0;i<cache.rangeCount;++i) {
            auto& r=cache.ranges[i];
            if(r.absoluteOffset==e.offset+off && r.length==n) {
                expected=&r;break;
            }
        }
    }
    assert(findResidentRange(e.offset+off,n)==expected);
    const uint32_t beforeReads=reads;
    const bool hit=!sourceProbeReady && expected &&
        (n<=1024 || residentLargeRangeEnabled);
    assert(n<=sizeof(out) && readRangeCore(&e,off,out,n));
    assert(reads==beforeReads+(hit?0U:1U));
    for(uint32_t i=0;i<n;++i)assert(out[i]==byteAt(e.offset+off+i));
    checkLayout();
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
    clearLargeResidentRanges();checkLayout();
    assert(residentLargeRangeCount()==0);
    // Explicit interleaved insertions, exact eviction victim and survivor
    // identity, with a small/large key sharing an absolute source offset.
    clearResidentCache();residentLargeRangeEnabled=true;
    expect(many,0,2048);expect(many,0,32);
    expect(many,4096,2048);expect(many,32,32);
    expect(many,8192,2048);expect(many,64,64);
    assert(cache.rangeCount==6 && cache.largeCount==3);
    assert(evictLowestLargeRange());checkLayout();
    assert(findResidentRange(many.offset+8192,2048)==nullptr);
    assert(findResidentRange(many.offset+4096,2048)!=nullptr);
    assert(findResidentRange(many.offset,2048)!=nullptr);
    assert(findResidentRange(many.offset,32)!=nullptr);
    clearLargeResidentRanges();checkLayout();
    assert(cache.rangeCount==3 && cache.largeCount==0);
    assert(findResidentRange(many.offset,2048)==nullptr);
    assert(findResidentRange(many.offset,32)!=nullptr);
    // Mixed inserts, disable, re-enable, evict and recycle repeatedly.
    uint32_t rng=0x12345678;
    for(uint32_t i=0;i<3000;++i) {
        rng=rng*1664525U+1013904223U;
        if(i%113==0){clearLargeResidentRanges();checkLayout();}
        if(i%197==0){recycleResidentRangeWorkingSet();checkLayout();}
        if(i%11==0)residentLargeRangeEnabled=!residentLargeRangeEnabled;
        uint32_t n=(rng&3)==0?2048:1+(rng>>16)%1024;
        const uint32_t off=(rng>>4)%(40000-n);
        expect(many,off,n);
        if(i%3==0)expect(many,off,n);
        if(i%19==0) {
            // Check every live descriptor after relocation, including
            // large-tail evictions and full-record recycling.
            for(uint32_t j=0;j<cache.rangeCount;++j) {
                auto& r=cache.ranges[j];
                assert(findResidentRange(r.absoluteOffset,r.length)==&r);
            }
        }
    }
    clearResidentCache();physicalUsesMapFlash=false;generation=3;
    expect(a,24,32);
    puts("PASS P3C real cache/read core: partitioned descriptors, exact lookup/eviction, owner=18088");
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
