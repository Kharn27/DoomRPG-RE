#!/usr/bin/env python3
"""Exercise both real framebuffer FNV bodies, independent scalar oracle."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def extract(path, signature):
    s = (ROOT / "ESP32/src" / path).read_text()
    assert s.count(signature) == 1
    a = s.index(signature)
    return s[a:s.index("\n}", a)+2]

source = "\n".join([
    "#include <stdint.h>\n#include <stddef.h>\n#include <assert.h>\n#include <stdio.h>",
    extract("esp_native_gameplay_frame.c", "static uint32_t fnv1aUpdate("),
    extract("esp_native_first_frame.c", "static uint32_t fnv1a32("),
    r"""
static uint32_t scalar(uint32_t h, const uint8_t* p, uint32_t n) {
    if (!p && n) return 0;
    for (uint32_t i=0;i<n;++i) { h ^= p[i]; h *= 16777619U; }
    return h;
}
static uint8_t data[38408];
int main(void) {
    uint32_t state=0xa981e71fU;
    for (unsigned i=0;i<sizeof(data);++i) {
        state=state*1664525U+1013904223U;
        data[i]=(uint8_t)(state>>24);
    }
    const uint32_t seeds[]={0U,1U,2166136261U,0xffffffffU};
    assert(fnv1a32(NULL,0)==2166136261U && fnv1a32(NULL,1)==0U);
    for (unsigned j=0;j<4;++j) {
        assert(fnv1aUpdate(seeds[j],NULL,0)==seeds[j]);
        assert(fnv1aUpdate(seeds[j],NULL,8)==0);
        for (unsigned off=0;off<8;++off) {
            for (uint32_t n=0;n<512;++n) {
                assert(fnv1aUpdate(seeds[j],data+off,n)==scalar(seeds[j],data+off,n));
                assert(fnv1a32(data+off,n)==scalar(2166136261U,data+off,n));
            }
            const uint32_t sizes[]={6400,12800,25600,38400};
            for (unsigned k=0;k<4;++k) {
                uint32_t n=sizes[k];
                uint32_t expected=scalar(seeds[j],data+off,n);
                assert(fnv1aUpdate(seeds[j],data+off,n)==expected);
                assert(fnv1aUpdate(fnv1aUpdate(seeds[j],data+off,n/2+1),
                    data+off+n/2+1,n-n/2-1)==expected);
                assert(fnv1a32(data+off,n)==scalar(2166136261U,data+off,n));
            }
        }
    }
    puts("PASS P2X real framebuffer FNV scalar parity, tails/alignment/seeds/chunks");
    return 0;
}
"""
])
with tempfile.TemporaryDirectory(prefix="doomrpg-p2x-fnv-") as tmp:
    p=Path(tmp)/"test.c"
    b=Path(tmp)/"test"
    p.write_text(source)
    subprocess.run(shlex.split(os.environ.get("CC","cc"))+
        ["-std=c11","-Os","-Wall","-Wextra","-Werror",str(p),"-o",str(b)],check=True)
    subprocess.run([str(b)],check=True)
