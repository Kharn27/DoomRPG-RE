#!/usr/bin/env python3
"""RES60-P18: real C P15 BSS vs P18 heap alias, 261 full-frame + P10 UV parity, fail-closed owner."""
from pathlib import Path
import configparser, subprocess, tempfile, re, sys
P21_TRIAL = '--p21' in sys.argv

ROOT=Path(__file__).resolve().parents[2]
ini=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert ini.read(ROOT/"ESP32/platformio.ini")
flags=ini["env:esp32-cyd-res60"]["build_flags"]
assert "-D DOOMRPG_RES60_P17_HEAP_SNAPSHOT=1" in flags
assert "-D DOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1" in flags
assert ini["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P17_HEAP_SNAPSHOT" not in ini[env]["build_flags"]
    assert "DOOMRPG_RES60_P18_LOCAL_SNAPSHOT" not in ini[env]["build_flags"]
config=(ROOT/"ESP32/include/platform_video_config.h").read_text()
source=(ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
video=(ROOT/"ESP32/src/platform_video.cpp").read_text()
assert "RES60-P18 requires isolated P17/P16/P15/P13 preview" in config
assert "static EspRes60P15SnapshotLine* candidateLines=NULL;" in source
assert "static EspRes60P15SnapshotLine candidateLines[RES60_P9_MAX_LINES];" in source
assert "if (candidateLines == NULL) return 0;" in source
assert "EspRes60P15SnapshotLine* const frameSnapshot=candidateLines;" in source
assert "candidateLines != frameSnapshot" in source
assert "frameSnapshot[candidateCount]" in source and "&frameSnapshot[index]" in source
assert source.count("malloc(")==1 and source.count("free(")==1
assert "EspRes60Preview_snapshotBegin()" in video and "free(framebuffer);" in video
assert "lifecycle=once-after-framebuffer" in video
assert "[RES60P18] ALIAS" in video and "localPerFrame=1" in video
assert "[RES60P17] COMPACT" in video and "snapshotBss=0 snapshotHeap=6144" in video
assert video.index("heap_caps_get_info(&p16AfterFramebuffer") < video.index("EspRes60Preview_snapshotBegin()")
assert video.index("const uint32_t preparedUs=micros()-started;") < video.index('res60P16_printHeapTopology("first-map-hit")')
assert "DOOMRPG_RES60_P17_HEAP_SNAPSHOT" not in (ROOT/"ESP32/src/esp_res60_wall_material.c").read_text()

# Reuse the P15 full pixel+UV+tie+canary host oracle, not reimplemented
# projection logic. Only the dynamic owner setup and fault tests differ.
p15=(ROOT/"ESP32/test/res60_p15_compact_snapshot_host.py").read_text()
assert p15.count('harness=r"""')==1
harness=p15.split('harness=r"""',1)[1].split('"""',1)[0]
begin='int main(void){\n    EspRes60PreviewCamera cam'
assert harness.count(begin)==1
setup=r"""
#include <stdlib.h>
static unsigned allocCalls=0U, freeCalls=0U;
static int failNextAlloc=1;
void* TestAlloc(size_t n){
    assert(n==6144U);
    ++allocCalls;
    if(failNextAlloc){failNextAlloc=0;return NULL;}
    return malloc(n);
}
void TestFree(void* p){++freeCalls;free(p);}
int main(void){
    static uint16_t guard[192U*144U+2U];
    EspRes60PreviewStats absent;
    EspRes60PreviewCamera failCam={1024,1024,64,36,0x1111,0x2222};
    fixture(24U);
    for(unsigned i=0U;i<192U*144U+2U;++i)guard[i]=0x7788U;
    assert(!EspRes60Preview_snapshotBegin()); /* malloc failure injected */
    assert(allocCalls==1U && EspRes60Preview_snapshotAddress()==NULL);
    assert(!EspRes60Preview_render(guard+1,55296U,&failCam,24U,readLine,NULL,&absent));
    for(unsigned i=0U;i<192U*144U+2U;++i)assert(guard[i]==0x7788U);
    assert(EspRes60Preview_snapshotBegin());
    const void* one=EspRes60Preview_snapshotAddress();
    assert(one && EspRes60Preview_snapshotBegin());
    assert(allocCalls==2U && one==EspRes60Preview_snapshotAddress());
    EspRes60PreviewCamera cam"""
harness=harness.replace(begin,setup)
finish=r"""
    EspRes60Preview_snapshotEnd();
    assert(EspRes60Preview_snapshotAddress()==NULL && freeCalls==1U);
    for(unsigned i=0U;i<192U*144U+2U;++i)guard[i]=0x7788U;
    assert(!EspRes60Preview_render(guard+1,55296U,&failCam,24U,readLine,NULL,&absent));
    for(unsigned i=0U;i<192U*144U+2U;++i)assert(guard[i]==0x7788U);
    assert(EspRes60Preview_snapshotBegin() && allocCalls==3U);
    EspRes60Preview_snapshotEnd();
    EspRes60Preview_snapshotEnd();
    assert(freeCalls==2U && EspRes60Preview_snapshotAddress()==NULL);
    printf("RES60-P18 real-C 261 frames, full 55296B, UV/ties/canaries, single bounded owner + rollback PASS\\n");
    return 0;
}
"""
assert harness.count('    return 0;\n}\n')==1
harness=harness.replace('    return 0;\n}\n',finish)
cc=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(ROOT/"ESP32/include")]
f=[
    "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
    "-DDOOMRPG_RES60_P2_LAYOUT=1",
    "-DDOOMRPG_RES60_P3_WORLD=1",
    "-DDOOMRPG_RES60_P5_TITLE=1",
    "-DDOOMRPG_RES60_P6_MENU=1",
    "-DDOOMRPG_RES60_P7_NAV=1",
    "-DDOOMRPG_RES60_P8_TEXEL=1",
    "-DDOOMRPG_RES60_P9_WEDGE=1",
    "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1",
    "-DDOOMRPG_RES60_P11_PROFILE=1",
    "-DDOOMRPG_RES60_P12_INVENTORY_CACHE=1",
    "-DDOOMRPG_RES60_P13_MAP_LEASE=1",
    "-DDOOMRPG_RES60_P14_LINE_SNAPSHOT=1",
    "-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
    "-DDOOMRPG_RES60_P16_HEAP_TOPOLOGY=1"
]
with tempfile.TemporaryDirectory(prefix="res60-p18-") as tmp:
    d=Path(tmp);(d/"test.c").write_text(harness)
    projector=str(ROOT/"ESP32/src/esp_res60_world_preview.c")
    material=str(ROOT/"ESP32/src/esp_res60_wall_material.c")
    old=d/"old.o";new=d/"new.o";mat=d/"mat.o";exe=d/"test"
    subprocess.run(cc+f+["-DDOOMRPG_RES60_P17_HEAP_SNAPSHOT=0",
        "-DEspRes60Preview_render=EspRes60Preview_renderBaseline",
        "-c",projector,"-o",str(old)],check=True)
    trialFlags=["-DDOOMRPG_RES60_P20_COMPACT_SERIAL=1",
                "-DDOOMRPG_RES60_P21_INTERSECTION32=1"] if P21_TRIAL else []
    subprocess.run(cc+f+["-DDOOMRPG_RES60_P17_HEAP_SNAPSHOT=1",
        "-DDOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1"]+trialFlags+[
        "-Dmalloc=TestAlloc","-Dfree=TestFree",
        "-c",projector,"-o",str(new)],check=True)
    subprocess.run(cc+f+["-DDOOMRPG_RES60_P17_HEAP_SNAPSHOT=1",
        "-DDOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1",
        "-c",material,"-o",str(mat)],check=True)
    subprocess.run(cc+f+["-DDOOMRPG_RES60_P17_HEAP_SNAPSHOT=1",
        "-DDOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1",
        str(d/"test.c"),str(old),str(new),str(mat),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    def symbol_size(obj,name):
        nm=subprocess.check_output(["nm","-S",str(obj)],text=True)
        a=[]
        for line in nm.splitlines():
            m=re.search(r"\b[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[bBdD]\s+"+name+r"$",line)
            if m:a.append(int(m.group(1),16))
        assert len(a)==1,(name,a,nm[-900:])
        return a[0]
    assert symbol_size(old,"candidateLines")==6144
    assert symbol_size(new,"candidateLines") in (4,8)
    raw=subprocess.check_output(["nm","-g",str(new)],text=True)
    assert "EspRes60Preview_snapshotBegin" in raw
    assert "EspRes60Preview_snapshotEnd" in raw
    assert "EspRes60Preview_snapshotAddress" in raw
    bad=subprocess.run(cc+["-DDOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1",
                           "-fsyntax-only",projector],capture_output=True,text=True)
    assert bad.returncode and "requires isolated P17/P16/P15/P13" in bad.stderr
    prod=d/"prod.o"
    subprocess.run(cc+["-DDOOMRPG_RES60_P3_WORLD=0",
                        "-c",projector,"-o",str(prod)],check=True)
    symbols=subprocess.check_output(["nm","-g",str(prod)],text=True)
    assert "EspRes60Preview_snapshotBegin" not in symbols
print(("RES60-P21 INTERSECTION32 HOST PASS" if P21_TRIAL else
       "RES60-P18 HOST PASS") +
      ": real P15 static 64-bit vs P17 heap render, 261 full-frame UV parity, bounded one-shot fail closed, BSS and production isolation")
