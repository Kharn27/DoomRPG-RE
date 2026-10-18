#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
sprite=(root/"ESP32/src/esp_native_sprite_renderer.c").read_text()
resident=(root/"ESP32/src/esp_native_resident_gameplay.c").read_text()
frame=(root/"ESP32/src/esp_native_gameplay_frame.c").read_text()
wall=(root/"ESP32/src/esp_native_first_frame.c").read_text()
wall_policy=(root/"ESP32/include/esp_native_wall_indicator_animation.h").read_text()
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
assert "EspNativeFirstFrame_indicatorDue(now)" in sprite
assert "EspNativeAmbientSprite_due(now, ambientPresentedMs" in sprite
assert "ambientSuspended=1U" in sprite
assert "EspNativeWallIndicator_phase(" in wall
assert "work->indicatorSeen |= EspNativeWallIndicator_bit(mapLine.texture);" in wall
assert "indicatorVisible = work->indicatorSeen" in wall
assert "indicatorSampleMs = work->animationMs" in wall
assert "if (ok) {" in wall
assert "EspNativeFirstFrame_indicatorDue(uint32_t nowMs)" in wall
assert "EspNativeWallIndicator_changed(indicatorVisible," in wall
assert "logical == 9U || logical == 10U" in wall_policy
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
#include "esp_native_wall_indicator_animation.h"
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
    assert(EspNativeWallIndicator_isAnimated(9));
    assert(EspNativeWallIndicator_isAnimated(10));
    assert(!EspNativeWallIndicator_isAnimated(8));
    assert(!EspNativeWallIndicator_isAnimated(11));
    /* Absolute-clock red breathing: forward and back, 960ms/loop.
     * Green races through all four frames in only 576ms. */
    static const uint8_t red[6]={0,1,2,3,2,1};
    for (uint32_t i=0;i<30U;i++) {
        uint32_t ms=i*ESP_NATIVE_WALL_INDICATOR_RED_MS;
        assert(EspNativeWallIndicator_phase(9U,ms-27U)==red[i%6U]);
        assert(EspNativeWallIndicator_phase(10U,
            i*ESP_NATIVE_WALL_INDICATOR_GREEN_MS-30U)==i%4U);
    }
    assert(EspNativeWallIndicator_bit(9U)==ESP_NATIVE_WALL_INDICATOR_RED_BIT);
    assert(EspNativeWallIndicator_bit(10U)==ESP_NATIVE_WALL_INDICATOR_GREEN_BIT);
    assert(EspNativeWallIndicator_bit(15U)==0);
    /* Red's immutable texture-9 phase offset is 27ms: first transition
     * occurs at wall-clock 133ms. A MOVE repaint at 131ms must not delay it. */
    assert(!EspNativeWallIndicator_changed(1U,100U,130U));
    assert(!EspNativeWallIndicator_changed(1U,131U,132U));
    assert(EspNativeWallIndicator_changed(1U,131U,134U));
    assert(EspNativeWallIndicator_changed(1U,100U,134U));
    assert(!EspNativeWallIndicator_changed(2U,100U,110U));
    assert(EspNativeWallIndicator_changed(2U,100U,115U));
    assert(EspNativeWallIndicator_changed(3U,100U,115U));
    assert(!EspNativeWallIndicator_changed(0U,100U,300U));
    assert(!EspNativeWallIndicator_changed(1U,0U,960U));
    assert(EspNativeWallIndicator_changed(1U,0U,192U));
    assert(EspNativeWallIndicator_phase(9U,UINT32_MAX)==
           EspNativeWallIndicator_phase(9U,(uint32_t)UINT32_MAX));
    puts("PASS ambient sprites, 960ms red breathe, 576ms green cycle, absolute wall phase, MOVE parity");
}
"""
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/"t.c").write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-IESP32/include",str(p/"t.c"),"-o",str(p/"t")],
                   cwd=root,check=True)
    subprocess.run([str(p/"t")],cwd=root,check=True)
print("PASS native ambient ownership")
