#!/usr/bin/env python3
"""P4a: real CRC scanner and boot-local proof with fake raw partition."""
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

stage = body("int EspAssetPack_mapFlashStage(")
assert stage.index("mapFlashValidateEntryCrcs(plan, buffer, readSource") < stage.index("esp_partition_erase_range(")
assert stage.index("mapFlashCrcProof = {};") < stage.index("esp_partition_erase_range(")
assert stage.index("mapFlashEnsureEntryCrcs(plan, verifyPayloadFNV") < stage.index("offsetof(MapFlashHeader, committed)")
prepare = body("int EspAssetPack_mapFlashPrepare(")
assert prepare.index("flashPayloadFNV != cached.payloadFNV1a") < prepare.index("mapFlashEnsureEntryCrcs(") < prepare.index("mapFlash.active = 1U;")
code = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "esp_asset_pack.h"
constexpr size_t kEntryBytes=20;
constexpr uint32_t kMapFlashCopyBufferBytes=4096,kMapFlashIndexOffset=4096;
struct esp_partition_t {uint32_t size;};
struct MapFlashPlan {uint32_t entryCount,sourceIndexOffset,sourceDataOffset,sourcePackBytes,indexBytes,indexFNV1a,payloadFlashOffset;};
esp_partition_t partition{32768};
struct {const esp_partition_t* partition;} mapFlash{&partition};
uint8_t flash[32768]={};
unsigned reads=0;
bool reject=false;
constexpr int ESP_OK=0;
int64_t esp_timer_get_time(){return 100;}
int esp_partition_read(const esp_partition_t*,uint32_t offset,void* dst,size_t len){
    ++reads;
    if(reject || offset>sizeof(flash) || len>sizeof(flash)-offset) return -1;
    memcpy(dst,flash+offset,len); return ESP_OK;
}
""" + "\n".join(body(x) for x in ("uint32_t readLe32(", "uint32_t fnv1aUpdate(", "void decodeEntry(", "uint32_t mapFlashCrc32Update(", "template <typename Read>\nbool mapFlashValidateEntryCrcs(")) + "\n" + src[src.index("struct MapFlashCrcProof {"):src.index("bool mapFlashTranslate(")] + r"""
void put(uint32_t off,uint32_t val){for(unsigned i=0;i<4;++i)flash[off+i]=(uint8_t)(val>>(i*8));}
int main(){
    uint8_t scratch[4096];
    const uint8_t known[]="123456789";
    assert((mapFlashCrc32Update(UINT32_MAX,known,9)^UINT32_MAX)==0xcbf43926U);
    uint32_t partial=mapFlashCrc32Update(UINT32_MAX,known,4);
    assert((mapFlashCrc32Update(partial,known+4,5)^UINT32_MAX)==0xcbf43926U);
    MapFlashPlan p{3,24,84,84+5009,60,0,8192};
    // sorted hashes, two files (one crosses buffer boundary), one empty directory.
    put(4096,1);put(4100,84);put(4104,9);put(4108,0xcbf43926U);
    memcpy(flash+8192,known,9);
    put(4116,2);put(4120,93);put(4124,5000);
    for(unsigned i=0;i<5000;++i)flash[8201+i]=(uint8_t)i;
    put(4128,mapFlashCrc32Update(UINT32_MAX,flash+8201,5000)^UINT32_MAX);
    put(4136,3);put(4140,p.sourcePackBytes);put(4152,ESP_ASSET_PACK_FLAG_DIRECTORY);
    p.indexFNV1a=fnv1aUpdate(2166136261U,flash+4096,60);
    assert(mapFlashEnsureEntryCrcs(p,77,scratch));
    unsigned n=reads;
    assert(mapFlashEnsureEntryCrcs(p,77,scratch)&&reads==n);
    flash[8205]^=1;
    assert(!mapFlashEnsureEntryCrcs(p,78,scratch)); // different full FNV, stale entry CRC
    flash[8205]^=1;
    assert(mapFlashEnsureEntryCrcs(p,78,scratch));
    mapFlashCrcProof={}; reject=true;
    assert(!mapFlashEnsureEntryCrcs(p,78,scratch));
    reject=false;
    put(4104,UINT32_MAX);
    assert(!mapFlashEnsureEntryCrcs(p,78,scratch)); // bounds before read
    puts("PASS MAPFLASH P4a real CRC, chunking, directory, proof reuse, corrupt file, read failure, bounds");
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"test.cpp"; p.write_text(code)
    exe=Path(tmp)/"test"
    subprocess.run(["c++","-std=c++11","-O2","-Wall","-Wextra","-Werror","-I",str(root/"ESP32/include"),str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
