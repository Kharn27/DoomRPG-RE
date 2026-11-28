#!/usr/bin/env python3
"""P3m: real sprite bounds/payload decoder, scalar header vs full clear."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/esp_native_sprite_renderer.c").read_text()
def extract(sig):
    assert SOURCE.count(sig) == 1, sig
    start = SOURCE.index(sig)
    depth = 0
    for i in range(SOURCE.index("{",start),len(SOURCE)):
        if SOURCE[i]=="{":
            depth+=1
        elif SOURCE[i]=="}":
            depth-=1
            if depth==0:
                return SOURCE[start:i+1]
    raise AssertionError(sig)
def struct(name):
    start=SOURCE.index("typedef struct "+name+"_s {")
    end=SOURCE.index("} "+name+";",start)+len("} "+name+";")
    return SOURCE[start:end]

PREFIX=r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_sprite_mask.h"
#include "esp_native_sprite_renderer.h"
#define MAX_DIM 64
#define MAX_MASK 512U
#define MAX_TEXELS 2048U
#define PAIR_BYTES 8U
#define SHAPE_HEADER 12U
#define TEXEL_HEADER 4U
#define PALETTE_HEADER 4U
typedef struct { uint32_t size, kind; } EspAssetPackEntry;
'''
SHIM=r'''
static uint8_t mapping[4096], shapes[1024], texels[4096], palettes[64];
static unsigned calls, failAt;
static uint32_t rng=0xeab09127U;
static uint32_t next(void) {
    rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng;
}
static const void* EspNativeGraphicsCatalog_findSprite(uint16_t id) {
    return id<252U ? mapping : NULL;
}
static int readRange(const EspAssetPackEntry* e,uint32_t o,void* d,
                     uint32_t n,EspNativeSpriteStats* stats) {
    const uint8_t* data;
    ++calls;
    if(calls==failAt || o>e->size || n>e->size-o) return 0;
    switch(e->kind) {
        case 0: data=mapping; break;
        case 1: data=shapes; break;
        case 2: data=texels; break;
        default: data=palettes; break;
    }
    memcpy(d,data+o,n); ++stats->packReads; return 1;
}
'''
CHECKS=r'''
int main(void) {
    Sources s={0};
    Frame a,b;
    FrameLocation la,lb;
    EspNativeSpriteStats sa,sb;
    uint32_t seenA[8],seenB[8];
    unsigned trial,i,fault,refCalls;
    s.mappings=(EspAssetPackEntry){sizeof(mapping),0};
    s.bitshapes=(EspAssetPackEntry){sizeof(shapes),1};
    s.stexels=(EspAssetPackEntry){sizeof(texels),2};
    s.palettes=(EspAssetPackEntry){sizeof(palettes),3};
    s.spriteIds=252; s.bitShapePairs=256;
    s.spriteIdBase=2048; s.spritePairBase=0; s.paletteEntries=30;
    /* Logical 5 resolves to actual 2; pair/source/palette offsets zero.
     * Header at byte 4; payload mask at byte 16. */
    mapping[2048+10]=2;
    for(trial=0;trial<4096U;++trial) {
        unsigned width=1U+trial%64U, height=1U+(trial/64U)%64U;
        memset(shapes,0,sizeof(shapes));
        shapes[12]=0; shapes[13]=(uint8_t)(width-1);
        shapes[14]=0; shapes[15]=(uint8_t)(height-1);
        for(i=16;i<sizeof(shapes);++i) shapes[i]=(uint8_t)next();
        shapes[16]|=1U; /* at least one live texel */
        for(i=0;i<sizeof(texels);++i) texels[i]=(uint8_t)next();
        for(i=0;i<sizeof(palettes);++i) palettes[i]=(uint8_t)next();
        /* Inject a bad dimension periodically; the reference must agree. */
        if(trial%97U==0U) shapes[13]=64;
        for(fault=0;fault<=6U;++fault) {
            int ra,rb;
            memset(&a,0xa5,sizeof(a)); memset(&b,0x5a,sizeof(b));
            memset(&sa,0,sizeof(sa)); memset(&sb,0,sizeof(sb));
            memset(seenA,0,sizeof(seenA)); memset(seenB,0,sizeof(seenB));
            memset(&la,0,sizeof(la)); memset(&lb,0,sizeof(lb));
            failAt=fault; calls=0;
            ra=loadFrameBounds(&s,5,0,&a,&la,&sa);
            refCalls=calls; calls=0;
            rb=referenceBounds(&s,5,0,&b,&lb,&sb);
            assert(ra==rb && calls==refCalls);
            assert(memcmp(&a,&b,offsetof(Frame,palette))==0);
            assert(memcmp(&la,&lb,sizeof(la))==0);
            assert(memcmp(&sa,&sb,sizeof(sa))==0);
            /* Bounds-only path must not touch payload at all. */
            for(i=offsetof(Frame,palette);i<sizeof(a);++i)
                assert(((const uint8_t*)&a)[i]==0xa5);
            if(!ra) continue;
            calls=refCalls;
            ra=loadFramePayload(&s,trial&1U,&a,&la,seenA,&sa);
            i=calls; calls=refCalls;
            rb=loadFramePayload(&s,trial&1U,&b,&lb,seenB,&sb);
            assert(ra==rb && calls==i);
            assert(memcmp(&sa,&sb,sizeof(sa))==0);
            assert(memcmp(seenA,seenB,sizeof(seenA))==0);
            if(!ra) continue;
            assert(memcmp(&a,&b,offsetof(Frame,palette))==0);
            assert(memcmp(a.palette,b.palette,sizeof(a.palette))==0);
            assert(memcmp(a.prefix,b.prefix,(a.width+1U)*sizeof(a.prefix[0]))==0);
            assert(memcmp(a.mask,b.mask,a.maskBytes)==0);
            assert(memcmp(a.texels,b.texels,a.packedBytes)==0);
        }
    }
    puts("PASS P3m real sprite decoder: 4096 dimensions, poisoned tails, bounds/payload/read failures and live bytes exact");
    return 0;
}
'''
bounds=extract("static int loadFrameBounds(")
anchor="    memset(frame, 0, offsetof(Frame, palette));"
assert bounds.count(anchor)==1
reference=bounds.replace("static int loadFrameBounds(","static int referenceBounds(",1).replace(anchor,"    memset(frame, 0, sizeof(*frame));",1)
body=PREFIX+"\n".join(struct(n) for n in ("Sources","Frame","FrameLocation"))+"\n"+SHIM+"\n".join(extract(n) for n in (
    "static uint16_t le16(","static uint32_t le32(","static uint16_t source565("))+"\n"+bounds+"\n"+reference+"\n"+extract("static int loadFramePayload(")+"\n"+CHECKS
with tempfile.TemporaryDirectory(prefix="p3m-decode-") as tmp:
    path=Path(tmp)/"test.c"; exe=Path(tmp)/"test"
    path.write_text(body)
    subprocess.run(shlex.split(os.environ.get("CC","cc"))+
        ["-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT/"include"),
         str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


# Exercise the actual P4l slot opener against the old scalar insertion.
ORDER_CHECK = r'''
static uint32_t rng=0x859a2f31U;
static uint32_t next(void) {
    rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng;
}
int main(void) {
    Order a[256], b[256];
    for (unsigned trial=0; trial<4096U; ++trial) {
        memset(a,0xa5,sizeof(a)); memset(b,0xa5,sizeof(b));
        unsigned count=trial%256U;
        for (unsigned i=0;i<count;++i) {
            int32_t z;
            switch(trial%6U) {
                case 0: z=(int32_t)i; break;
                case 1: z=-(int32_t)i; break;
                case 2: z=0; break;
                case 3: z=(int32_t)(next()%9U)-4; break;
                case 4: z=(i&1U)?INT32_MIN:INT32_MAX; break;
                default: z=(int32_t)(next()&0x7fffffffU); break;
            }
            Order incoming={(uint16_t)i,(uint16_t)next(),next(),z};
            uint32_t ref=i;
            while(ref>0U && z>=b[ref-1U].sortZ) {
                b[ref]=b[ref-1U]; --ref;
            }
            uint32_t got=openOrderSlot(a,i,z);
            assert(got==ref);
            a[got]=incoming; b[ref]=incoming;
            assert(memcmp(a,b,sizeof(a))==0);
        }
    }
    puts("PASS P4l order exact bytes, newest-first ties, dense/ascending/descending/extreme depths");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="p4l-order-") as tmp:
    path=Path(tmp)/"order.c"; exe=Path(tmp)/"order"
    path.write_text("#include <stdint.h>\n#include <string.h>\n#include <assert.h>\n#include <stdio.h>\n"+
                    struct("Order")+"\n"+extract("static uint32_t openOrderSlot(")+"\n"+ORDER_CHECK)
    subprocess.run(shlex.split(os.environ.get("CC","cc"))+
        ["-std=c11","-O2","-Wall","-Wextra","-Werror",str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
