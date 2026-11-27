#!/usr/bin/env python3
"""Execute the exact two SD validation loops extracted from esp_asset_pack.cpp."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/"src/esp_asset_pack.cpp").read_text()
start=src.index("#if DOOMRPG_RES60_P26_BATCH_INDEX",src.index("int EspAssetPack_open("))
end=src.index("\n#endif\n\n    physicalReady = true;",start)+len("\n#endif")
loops=src[start:end]
helper_start=src.index("#if DOOMRPG_RES60_P32_VALIDATED_INDEX")
helper_end=src.index("#endif /* P32 validated index */",helper_start)+len("#endif /* P32 validated index */")
helper=src[helper_start:helper_end]
prefix=r"""
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
constexpr size_t kEntryBytes=20;
struct EspAssetPackEntry { uint32_t nameHash,offset,size,crc32,flags; };
"""+helper+r"""
static uint8_t input[4096*20];
static uint32_t entryCount, pos, calls, validated, failAt;
static bool reset;
static void resetPhysicalState(){reset=true;
#if DOOMRPG_RES60_P32_VALIDATED_INDEX
    clearRes60Index();
#endif
}
static bool readSdExact(void* out,size_t n){
    ++calls;
    if (pos+n>entryCount*kEntryBytes || (failAt>=pos && failAt<pos+n)) return false;
    memcpy(out,input+pos,n);pos+=n;return true;
}
static void decodeEntry(const uint8_t* raw,EspAssetPackEntry* e){
    memcpy(e,raw,20);
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
#if DOOMRPG_RES60_P32_VALIDATED_INDEX
    clearRes60Index();
#endif
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
#if DOOMRPG_RES60_P32_VALIDATED_INDEX
    /* Actual P32 helpers, exact five-field records captured during the actual
     * P26 validation loop. Unknown/missing names never produce a HIT. */
    uint32_t hashes[4]={res60IndexHash("intro.bsp"),res60IndexHash("mappings.bin"),
        res60IndexHash("palettes.bin"),res60IndexHash("wtexels.bin")};
    for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)
        if(hashes[j]<hashes[i]){uint32_t tmp=hashes[i];hashes[i]=hashes[j];hashes[j]=tmp;}
    prepare(4);
    for(unsigned i=0;i<4;++i){
        EspAssetPackEntry e={hashes[i],123U,100U+i,200U+i,300U+i};
        memcpy(input+i*20,&e,20);
    }
    assert(run()==1 && calls==1 && validated==4);
    EspAssetPackEntry got={};
    for(unsigned i=0;i<4;++i){
        assert(findRes60Index(hashes[i],&got));
        assert(got.nameHash==hashes[i] && got.offset==123U &&
               got.size==100U+i && got.crc32==200U+i && got.flags==300U+i);
    }
    assert(res60IndexHitMask==15U && !findRes60Index(0,&got) &&
           !findRes60Index(123,&got) && !findRes60Index(hashes[0],nullptr));
    resetPhysicalState();
    for(unsigned i=0;i<4;++i)assert(!findRes60Index(hashes[i],&got));
    /* Partial validation followed by bad record must clear captured metadata. */
    prepare(4);
    for(unsigned i=0;i<4;++i)set(i,hashes[i],123);
    set(3,hashes[3],124);
    assert(!run() && reset);
    for(unsigned i=0;i<4;++i)assert(!findRes60Index(hashes[i],&got));
    puts("P32 exact validated entry capture and cleanup PASS");
#endif
    puts("P26 exact index validation PASS");
}
"""
with tempfile.TemporaryDirectory() as d:
    p=Path(d)
    (p/"test.cpp").write_text("#include <initializer_list>\n"+prefix+loops+suffix)
    for flag,p32 in ((0,0),(1,0),(1,1)):
        subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror",f"-DDOOMRPG_RES60_P26_BATCH_INDEX={flag}",f"-DDOOMRPG_RES60_P32_VALIDATED_INDEX={p32}",str(p/"test.cpp"),"-o",str(p/"test")],check=True)
        subprocess.run([str(p/"test")],check=True)
