#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
sprite=(root/"ESP32/src/esp_native_sprite_renderer.c").read_text()
resident=(root/"ESP32/src/esp_native_resident_gameplay.c").read_text()
frame=(root/"ESP32/src/esp_native_gameplay_frame.c").read_text()
dynamic=(root/"ESP32/src/esp_native_dynamic_line_render.c").read_text()
platformio=(root/"ESP32/platformio.ini").read_text()
legacy=(root/"src/Render.c").read_text(encoding="latin-1")
assert "render->animFrameTime" in legacy
assert "EspNativeAmbientSprite_frame(" in sprite
assert "EspNativeAmbientSprite_eligible(" in sprite
assert "EspNativeSpriteRenderer_ambientDue(nowMs)" in resident
assert "EspNativeGameplayFrame_renderAmbient(" in resident
assert "EspNativeSpriteRenderer_resetAmbient()" in resident
assert "EspNativeGameplayPresentGate_isArmed()" in resident
assert "renderComposed(render,v,(uint8_t)v->viewAngle,1,1,stats)" in frame
assert "if (!preview && EspMapAutomapState_isReady())" in sprite
assert "-Wl,--wrap=EspNativeGameplayFrame_renderAmbient" in platformio
assert "int __wrap_EspNativeGameplayFrame_renderAmbient(" in dynamic
assert "beginDynamicPreview()" in dynamic
assert 'endDynamicPreview(ok, "ambient")' in dynamic
assert "__real_EspNativeGameplayFrame_renderAmbient(render, outStats)" in dynamic
assert "if (open == 0U) return 1;" in dynamic
assert "resolveDynamicTexture(index, outLine->texture" in dynamic
code=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_ambient_sprite.h"
#define INFO(v,f) ((uint32_t)(v)*512U|((f)?0x80000000UL:0U))
int main(void) {
    for (uint32_t t=0;t<10000;t+=250)
      assert(EspNativeAmbientSprite_frame(INFO(0,0),0,0,t)==(t/250)%4);
    assert(EspNativeAmbientSprite_frame(INFO(0,0),1,0,3999)==0);
    assert(EspNativeAmbientSprite_frame(INFO(0,0),1,0,4000)==3);
    assert(EspNativeAmbientSprite_frame(INFO(0,0),1,0,8000)==0);
    assert(EspNativeAmbientSprite_frame(INFO(5,1),1,0,4000)==5);
    assert(EspNativeAmbientSprite_frame(INFO(1,0),1,0,4000)==1);
    assert(EspNativeAmbientSprite_frame(INFO(0,0),2,0,6000)==1);
    assert(EspNativeAmbientSprite_frame(INFO(0,0),2,0,12000)==0);
    assert(!EspNativeAmbientSprite_due(374,0,1,1));
    assert(EspNativeAmbientSprite_due(375,0,1,1));
    assert(EspNativeAmbientSprite_due(4000,0,0,1));
    assert(!EspNativeAmbientSprite_due(4000,0,0,0));
    assert(!EspNativeAmbientSprite_due(10,0xfffffff0UL,1,1));
    assert(EspNativeAmbientSprite_due(400,0xffffff00UL,1,1));
    puts("PASS ambient pure frames/timers/fixed");
}
"""
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/"t.c").write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-IESP32/include",str(p/"t.c"),"-o",str(p/"t")],
                   cwd=root,check=True)
    subprocess.run([str(p/"t")],cwd=root,check=True)
print("PASS native ambient ownership")
