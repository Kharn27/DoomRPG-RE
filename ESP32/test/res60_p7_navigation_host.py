#!/usr/bin/env python3
"""RES60-P7: real C11 navigation, 24px original-skin overlay, isolation.

The original P3 raycasting source must remain byte-identical; its spawn FNV
ed9b68c4 is tested on CYD, whereas four new perspective orientations await it.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(root/"ESP32/platformio.ini")
pilot=cfg["env:esp32-cyd-res60"]
assert "-D DOOMRPG_RES60_P7_NAV=1" in pilot["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P7_NAV" not in cfg[env]["build_flags"]
main=(root/"ESP32/src/main.cpp").read_text()
video=(root/"ESP32/src/platform_video.cpp").read_text()
probe=(root/"ESP32/src/esp_res60_world_probe.c").read_text()
ray=(root/"ESP32/src/esp_res60_world_preview.c").read_text()
assert "EspRes60Nav_hitPhysical(x,y)" in main
assert "PlatformVideo_showRes60WorldPreviewQuarterTurns(0U)" in main
assert "PlatformVideo_showRes60WorldPreviewQuarterTurns(next)" in main
assert "res60QuarterTurns = next;" in main
assert "EspRes60WorldProbe_renderIntroQuarterTurns" in video
assert "EspMapRuntime_reset();" in probe
assert "camera.angle = cameraAngle;" in probe
assert "cameraAngle=(uint8_t)(inv.spawnDirection + quarterTurns*64U);" in probe
assert "EspRes60Preview_render(framebuffer, framebufferBytes, &camera," in probe
assert "EspRes60Layout_fnv(framebuffer, framebufferBytes)" in ray
assert "ESP_RES60_WORLD_ROWS" in ray
assert "texel" not in (root/"ESP32/src/esp_res60_nav.c").read_text().lower()
assert 'render=FLAT_WALLS textureSamples=0 sprites=0' in video
src=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "platform_video_config.h"
#include "esp_res60_layout.h"
#include "esp_res60_nav.h"
static uint16_t arena[192U*144U+2U];
int main(void) {
    uint16_t* fb=arena+1U;uint8_t angle=99,turn=0;
    uint32_t hashes[4];
    arena[0]=0xaac1;arena[192U*144U+1U]=0xcaae;
    assert(!EspRes60Nav_resolveAngle(1,0,&angle)&&angle==99);
    assert(!EspRes60Nav_resolveAngle(64,4,&angle)&&angle==99);
    assert(!EspRes60Nav_resolveAngle(64,0,NULL));
    for(uint8_t i=0;i<4;++i) {
       assert(EspRes60Nav_resolveAngle(64,i,&angle));
       assert(angle==(uint8_t)(64+i*64));
    }
    assert(EspRes60Nav_applyTurn(0,ESP_RES60_NAV_LEFT,&turn)&&turn==1);
    assert(EspRes60Nav_applyTurn(turn,ESP_RES60_NAV_RIGHT,&turn)&&turn==0);
    assert(EspRes60Nav_applyTurn(turn,ESP_RES60_NAV_RIGHT,&turn)&&turn==3);
    assert(EspRes60Nav_applyTurn(3,ESP_RES60_NAV_MENU,&turn)&&turn==3);
    assert(!EspRes60Nav_applyTurn(4,ESP_RES60_NAV_LEFT,&turn));
    assert(!EspRes60Nav_applyTurn(0,ESP_RES60_NAV_NONE,&turn));
    assert(!EspRes60Nav_applyTurn(0,ESP_RES60_NAV_LEFT,NULL));
    assert(EspRes60Nav_hitPhysical(-1,220)==ESP_RES60_NAV_NONE);
    assert(EspRes60Nav_hitPhysical(320,220)==ESP_RES60_NAV_NONE);
    assert(EspRes60Nav_hitPhysical(50,190)==ESP_RES60_NAV_NONE);
    assert(EspRes60Nav_hitPhysical(50,218)==ESP_RES60_NAV_LEFT);
    assert(EspRes60Nav_hitPhysical(140,218)==ESP_RES60_NAV_MENU);
    assert(EspRes60Nav_hitPhysical(260,218)==ESP_RES60_NAV_RIGHT);
    assert(EspRes60Nav_hitPhysical(115,218)==ESP_RES60_NAV_NONE);
    assert(EspRes60Nav_hitPhysical(140,239)==ESP_RES60_NAV_NONE);
    for(unsigned i=0;i<192U*144U;++i)fb[i]=0x36b9U;
    assert(!EspRes60Nav_paint(NULL,ESP_RES60_FRAMEBUFFER_BYTES,64,0));
    assert(!EspRes60Nav_paint(fb,ESP_RES60_FRAMEBUFFER_BYTES-2,64,0));
    assert(!EspRes60Nav_paint(fb,ESP_RES60_FRAMEBUFFER_BYTES,65,0));
    assert(!EspRes60Nav_paint(fb,ESP_RES60_FRAMEBUFFER_BYTES,64,4));
    assert(fb[0]==0x36b9U);
    for(uint8_t i=0;i<4;++i) {
        assert(EspRes60Nav_paint(fb,ESP_RES60_FRAMEBUFFER_BYTES,64,i));
        hashes[i]=EspRes60Layout_fnv(fb,ESP_RES60_FRAMEBUFFER_BYTES);
        assert(hashes[i]!=0);
        assert(fb[0]==0x0800U);
        assert(fb[24U*192U]==0x36b9U);
        assert(fb[119U*192U+191U]==0x36b9U);
        assert(fb[120U*192U]==0x0800U);
        assert(arena[0]==0xaac1U);
        assert(arena[192U*144U+1U]==0xcaaeU);
        assert(EspRes60Nav_paint(fb,ESP_RES60_FRAMEBUFFER_BYTES,64,i));
        assert(EspRes60Layout_fnv(fb,ESP_RES60_FRAMEBUFFER_BYTES)==hashes[i]);
    }
    assert(hashes[0]!=hashes[1] && hashes[1]!=hashes[2] &&
           hashes[2]!=hashes[3] && hashes[3]!=hashes[0]);
    puts("RES60-P7 native cardinal direction controls, touch, canaries and viewport preservation PASS");
    return 0;
}
"""
flags=["-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
       "-DDOOMRPG_RES60_P2_LAYOUT=1",
       "-DDOOMRPG_RES60_P3_WORLD=1",
       "-DDOOMRPG_RES60_P5_TITLE=1",
       "-DDOOMRPG_RES60_P6_MENU=1",
       "-DDOOMRPG_RES60_P7_NAV=1"]
with tempfile.TemporaryDirectory(prefix="res60-p7-") as path:
    temp=Path(path); source=temp/"nav.c"; exe=temp/"test"
    source.write_text(src)
    cmd=["cc","-O2","-std=c11","-Wall","-Wextra","-Werror",*flags,
         "-I",str(root/"ESP32/include"),
         str(root/"ESP32/src/esp_res60_nav.c"),
         str(root/"ESP32/src/esp_main_menu_skin.c"),
         str(source),"-o",str(exe)]
    subprocess.run(cmd,check=True)
    subprocess.run([str(exe)],check=True)
    blank=temp/"blank.o"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
         "-DDOOMRPG_RES60_P7_NAV=0","-I",str(root/"ESP32/include"),
         "-c",str(root/"ESP32/src/esp_res60_nav.c"),"-o",str(blank)],check=True)
    assert "EspRes60Nav_paint" not in subprocess.check_output(["nm","-g",str(blank)],text=True)
    missing=subprocess.run(["cc","-std=c11","-fsyntax-only",
          "-DDOOMRPG_RES60_P7_NAV=1","-I",str(root/"ESP32/include"),
          str(root/"ESP32/src/esp_res60_nav.c")],capture_output=True,text=True)
    assert missing.returncode!=0 and "requires P6 and isolated P3" in missing.stderr
print("RES60-P7 HOST PASS: 4 cardinal views + native touch overlays, no world/gameplay mutation")
