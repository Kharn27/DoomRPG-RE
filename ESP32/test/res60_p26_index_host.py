#!/usr/bin/env python3
"""Execute the exact two SD validation loops extracted from esp_asset_pack.cpp."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/"src/esp_asset_pack.cpp").read_text()
start=src.index("#if DOOMRPG_RES60_P26_BATCH_INDEX",src.index("int EspAssetPack_open("))
end=src.index("#endif",start)+len("#endif")
loops=src[start:end]
prefix=r"""
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
constexpr size_t kEntryBytes=20;
struct EspAssetPackEntry { uint32_t nameHash; uint32_t offset; };
static uint8_t input[4096*20];
static uint32_t entryCount, pos, calls, validated, failAt;
static bool reset;
static void resetPhysicalState(){reset=true;}
static bool readSdExact(void* out,size_t n){
    ++calls;
    if (pos+n>entryCount*kEntryBytes || (failAt>=pos && failAt<pos+n)) return false;
    memcpy(out,input+pos,n);pos+=n;return true;
}
static void decodeEntry(const uint8_t* raw,EspAssetPackEntry* e){
    memcpy(&e->nameHash,raw,4);memcpy(&e->offset,raw+4,4);
}
static bool validateEntry(const EspAssetPackEntry& e){++validated;return e.offset==123;}
static int run(){
    uint32_t previousHash=0;
    bool havePreviousHash=false;
"""
suffix=r"""
    return 1;
}
static void set(uint32_t i,uint32_t h,uint32_t o){
    memcpy(input+i*20,&h,4);memcpy(input+i*20+4,&o,4);
}
static void prepare(uint32_t n){
    entryCount=n;pos=calls=validated=0;reset=false;failAt=UINT32_MAX;
    for(uint32_t i=0;i<n;++i)set(i,i+1,123);
}
int main(){
    for(uint32_t n: {1U,15U,16U,17U,31U,32U,33U,4096U}){
        prepare(n);assert(run()==1&&!reset&&validated==n&&pos==n*20);
#if DOOMRPG_RES60_P26_BATCH_INDEX
        assert(calls==(n+15)/16);
#else
        assert(calls==n);
#endif
        for(uint32_t i=0;i<n;i+=i<33?1:137){
            prepare(n);set(i,i,123);
            if(i>0)assert(!run()&&reset);
            prepare(n);set(i,i+1,124);assert(!run()&&reset);
            prepare(n);failAt=i*20+19;assert(!run()&&reset);
        }
    }
    puts("P26 exact index validation PASS");
}
"""
with tempfile.TemporaryDirectory() as d:
    p=Path(d)
    (p/"test.cpp").write_text("#include <initializer_list>\n"+prefix+loops+suffix)
    for flag in (0,1):
        subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror",f"-DDOOMRPG_RES60_P26_BATCH_INDEX={flag}",str(p/"test.cpp"),"-o",str(p/"test")],check=True)
        subprocess.run([str(p/"test")],check=True)
