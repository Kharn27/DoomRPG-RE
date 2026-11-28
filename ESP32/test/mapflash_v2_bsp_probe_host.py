#!/usr/bin/env python3
"""P3: exercise actual exact-entry BSP probe and payload routing against fake storage."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "ESP32/src/esp_asset_pack.cpp").read_text()

def body(sig):
    start = src.index(sig)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError(sig)

core = body("int readRangeCore(")
assert "if (!readSourceProbePayload(absoluteOffset, destination, length)) return 0;" in core
assert "cacheableSmall = !sourceProbeExact" in core and "cacheableLarge = !sourceProbeExact" in core
assert "sourceProbeHash && !sourceProbeExact" in core

layout = src[src.index("struct MapFlashHeader {"):src.index("File packFile;")]
constants = src[src.index("constexpr uint32_t kMapFlashMagic"):src.index("constexpr uint32_t kMapFlashEraseChunkBytes")]
harness = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "esp_asset_pack.h"
constexpr uint32_t ESP_MAP_CATALOG_COUNT=13;
constexpr size_t kHeaderBytes=24, kEntryBytes=20;
constexpr uint32_t kVersion=2;
constexpr uint8_t kMagic[8]={'D','R','P','G','E','S','P','2'};
struct esp_partition_t { uint32_t size; };
""" + constants + layout + r"""
static esp_partition_t part{65536};
static uint8_t sd[128], flash[65536];
static uint32_t sdPayloadReads=0, flashPayloadReads=0, verifiedFlashEpoch=1;
static bool flashReadFails=false, openReady=false, residentEnabled=true;
constexpr int FILE_READ=0;
struct File {
    bool live=false;
    size_t pos=0;
    explicit operator bool() const {return live;}
    void close(){live=false;}
    size_t size(){return sizeof(sd);}
    bool seek(size_t at){pos=at;return live && at<=sizeof(sd);}
    size_t read(uint8_t* dst,size_t len){
        if(!live || pos>sizeof(sd) || len>sizeof(sd)-pos) return 0;
        if(pos>=44) ++sdPayloadReads;
        memcpy(dst,sd+pos,len);pos+=len;return len;
    }
};
struct SDType { File open(const char*,int){File f;f.live=true;return f;} } SD;
static File sourceProbeFile;
static EspAssetPackEntry sourceProbeEntry{};
static bool sourceProbeReady=false;
static uint32_t sourceProbeFlashEpoch=0;
static uint8_t sourceProbeReadLogged=0;
static MapFlashState mapFlash{};
static void suspendFontSdBeforePackOpen(){}
static uint32_t EspAssetPack_nameHash(const char* name){return name && *name ? 0x1234U:0U;}
static bool EspMapCatalog_idForName(const char* name,uint8_t* id){*id=2;return !strcmp(name,"/level01.bsp");}
int EspAssetPack_isMapFlashActive(){return mapFlash.active;}
int EspAssetPack_isResident(){return residentEnabled;}
static bool mapFlashRead(uint32_t off,void* dst,size_t len,bool runtime){
    assert(!runtime); // bypass L1 and gameplay counters
    if(flashReadFails || !mapFlash.active || off>part.size || len>part.size-off)return false;
    if(off>=8192) ++flashPayloadReads;
    memcpy(dst,flash+off,len);return true;
}
""" + "\n".join(body(x) for x in (
    "uint32_t readLe32(", "uint32_t fnv1aUpdate(", "uint32_t alignSector(",
    "void clearSourceProbe(", "void decodeEntry(", "bool mapFlashHeaderLooksValid(",
    "bool mapFlashTranslate(", "bool readSourceProbePayload(",
    "int EspAssetPack_sourceProbeBegin(", "void EspAssetPack_sourceProbeEnd(",
    "int EspAssetPack_isSourceProbeActive("
)) + r"""
static void put(uint8_t* p,uint32_t v){for(int i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void setup(){
    clearSourceProbe();openReady=false;flashReadFails=false;
    verifiedFlashEpoch=1;sdPayloadReads=flashPayloadReads=0;
    memset(sd,0,sizeof(sd));memset(flash,0,sizeof(flash));
    memcpy(sd,kMagic,8);put(sd+8,2);put(sd+12,1);put(sd+16,24);put(sd+20,44);
    put(sd+24,0x1234);put(sd+28,44);put(sd+32,84);put(sd+36,0xabcdef);put(sd+40,0);
    for(size_t i=44;i<sizeof(sd);++i) sd[i]=(uint8_t)i;
    memcpy(flash+4096,sd+24,20);memcpy(flash+8192,sd+44,84);
    mapFlash={};mapFlash.partition=&part;mapFlash.active=1;mapFlash.stats.verified=1;
    auto& h=mapFlash.header;
    h.magic=kMapFlashMagic;h.version=kMapFlashVersion;h.headerBytes=sizeof(h);
    h.committed=kMapFlashCommitted;h.sourcePackBytes=128;h.sourceIndexOffset=24;
    h.sourceDataOffset=44;h.entryCount=1;h.indexFlashOffset=4096;h.indexBytes=20;
    h.payloadFlashOffset=8192;h.stagedBytes=84;h.indexFNV1a=fnv1aUpdate(2166136261U,sd+24,20);
}
int main(){
    uint8_t dst[32]{};
    setup();assert(EspAssetPack_sourceProbeBegin("/level01.bsp"));
    assert(EspAssetPack_isSourceProbeActive() && sourceProbeFlashEpoch==1);
    assert(readSourceProbePayload(44,dst,32) && !memcmp(dst,sd+44,32));
    assert(sdPayloadReads==0 && flashPayloadReads==1);
    flashReadFails=true;assert(!readSourceProbePayload(44,dst,32) && sdPayloadReads==0);
    flashReadFails=false;++verifiedFlashEpoch;
    assert(!readSourceProbePayload(44,dst,32) && sdPayloadReads==0);
    EspAssetPack_sourceProbeEnd();assert(!sourceProbeFlashEpoch && !EspAssetPack_isSourceProbeActive());
    setup();mapFlash.active=0;assert(EspAssetPack_sourceProbeBegin("/level01.bsp"));
    assert(!sourceProbeFlashEpoch && readSourceProbePayload(44,dst,32) && sdPayloadReads==1);
    setup();sd[36]^=1;assert(EspAssetPack_sourceProbeBegin("/level01.bsp"));
    assert(!sourceProbeFlashEpoch && readSourceProbePayload(44,dst,32) && sdPayloadReads==1);
    setup();flash[4096]^=1;assert(!EspAssetPack_sourceProbeBegin("/level01.bsp"));
    assert(!EspAssetPack_isSourceProbeActive() && sdPayloadReads==0);
    setup();flashReadFails=true;assert(!EspAssetPack_sourceProbeBegin("/level01.bsp"));
    setup();assert(EspAssetPack_sourceProbeBegin("/not-bsp.bin"));
    assert(!sourceProbeFlashEpoch && readSourceProbePayload(44,dst,32) && sdPayloadReads==1);
    setup();openReady=true;assert(!EspAssetPack_sourceProbeBegin("/level01.bsp"));
    puts("PASS MAPFLASH P3 actual probe routing, identity mismatch, epoch invalidation and no SD fallback");
}
"""
with tempfile.TemporaryDirectory(prefix="mapflash-p3-") as tmp:
    path = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    path.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CXX","c++")) +
                   ["-std=c++11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(root/"ESP32/include"),str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
