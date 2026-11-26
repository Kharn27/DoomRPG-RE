#!/usr/bin/env python3
"""Production automap publisher: only successful negative queries are reusable."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/esp_native_bsp_visibility.c").read_text()
start = source.index("int EspNativeBspVisibility_publishAutomapWithRejected(")
end = source.index("\nint EspNativeBspVisibility_publishAutomap(", start)
body = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_bsp_visibility.h"
typedef struct { uint32_t lineCount,mapSpriteCount; } EspMapRuntimeView;
static EspMapRuntimeView map={8,278};
static uint8_t lines[8],sprites[1100],memo[128];
static uint32_t queries,activations,order[1100],bad=UINT32_MAX;
static const EspMapRuntimeView* EspMapRuntime_view(void){return &map;}
static int EspMapAutomapState_isReady(void){return 1;}
static int EspMapAutomapState_getLineRevealed(uint32_t i,uint8_t* v){*v=lines[i];return 1;}
static int EspMapAutomapState_setLineRevealed(uint32_t i,uint8_t v){lines[i]=v;return 1;}
static int EspMapAutomapState_getSpriteRevealed(uint32_t i,uint8_t* v){*v=sprites[i];return 1;}
static int EspMapAutomapState_setSpriteRevealed(uint32_t i,uint8_t v){sprites[i]=v;return 1;}
int EspNativeBspVisibility_mapSpriteVisible(const EspNativeBspVisibilityState* s,uint32_t i,uint32_t* leaf){
    (void)s; ++queries; *leaf=i==bad?UINT32_MAX:i%32U;
    if(i==bad || i%7U!=0U)return 0;
    order[activations++]=i;return 1;
}
'''
checks = r'''
int main(void){
    EspNativeBspVisibilityState state={0};
    uint8_t expected[1100],expectedLines[8];
    uint32_t expectedOrder[1100],a,q;
    uint16_t l,m,reused,l0,m0;
    state.visibleAutomapLines[0]=0xabU;
    for(uint32_t n=1;n<=1100;n+=17){
        map.mapSpriteCount=n;
        memset(memo,0,sizeof memo);
        memset(lines,0,sizeof lines);memset(sprites,0,sizeof sprites);
        queries=activations=0;
        assert(EspNativeBspVisibility_publishAutomapWithRejected(&state,NULL,&l0,&m0,&reused));
        assert(reused==0);
        memcpy(expected,sprites,sizeof sprites);memcpy(expectedLines,lines,sizeof lines);
        memcpy(expectedOrder,order,activations*sizeof(uint32_t));a=activations;q=queries;
        /* Hidden/unqueried indices (i%5==0) must still be checked.
         * Positive queries never enter the memo. */
        for(uint32_t i=0;i<n && i<1024U;++i)
            if(i%5U!=0U && i%7U!=0U)memo[i>>3]|=1U<<(i&7);
        memset(lines,0,sizeof lines);memset(sprites,0,sizeof sprites);
        queries=activations=0;
        assert(EspNativeBspVisibility_publishAutomapWithRejected(&state,memo,&l,&m,&reused));
        assert(l==l0&&m==m0&&activations==a&&queries+reused==q);
        assert(!memcmp(expected,sprites,sizeof sprites));
        assert(!memcmp(expectedLines,lines,sizeof lines));
        assert(!memcmp(expectedOrder,order,a*sizeof(uint32_t)));
        if(n>1024U)assert(reused==0);
    }
    map.mapSpriteCount=278;bad=5U;memset(memo,0,sizeof memo);
    assert(!EspNativeBspVisibility_publishAutomapWithRejected(&state,memo,&l,&m,&reused));
    assert(reused==0);
    puts("PASS P2l negative query reuse, exact reveals and activation order, invalid leaf and large-map fallback");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/"test.c";exe=Path(tmp)/"test"
    c.write_text(prefix+body+checks)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT/"include"),str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
