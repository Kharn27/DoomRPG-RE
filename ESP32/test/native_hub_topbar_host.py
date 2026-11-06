#!/usr/bin/env python3
"""Native HUB top-bar painter/touch contract and full physical hitbox checks."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
inc=root/"ESP32/include"
ui=(root/"ESP32/src/esp_native_gameplay_hub_touch_ui.c").read_text()
hub=(root/"ESP32/src/esp_native_gameplay_hub.c").read_text()
inp=(root/"ESP32/src/esp_native_gameplay_input.c").read_text()
sys=(root/"ESP32/src/esp_native_gameplay_save_touch.c").read_text()

assert "EspNativeGameplayHubTouchUi_classify(logicalX, logicalY, outHit)" in inp
assert inp.index("EspNativeGameplayHubTouchUi_classify(logicalX, logicalY, outHit)") < inp.index("if (logicalY < GAMEPLAY_TOP_HUD_HEIGHT)")
assert "if (hubTouch < 0)" in inp
assert "if (logicalX >= GAMEPLAY_AUTOMAP_LEFT)" in inp # world unchanged
assert "EspNativeGameplayHubTouchUi_paintHeaderTabs(" in ui
assert "EspNativeGameplayHubTouchUi_paintHeaderTabs(" in hub
assert "headerFill(framebuffer, 0, 0, 159, 19" in hub
assert "menuFill(framebuffer, 0, 0, 31, 19" in hub
assert "menuOverlay.paintedZoneFNV = menuZoneFNV(framebuffer)" in hub
assert "menuOverlayRestore(framebuffer)" in hub
assert "drawText(font, framebuffer, pageTitle(hub.page)" not in hub
assert 'drawCrispText(framebuffer, labels[tab]' in ui
top=ui[ui.index("if (logicalY < HUB_UI_TOP) {"):ui.index("\n\n    if (view->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS) {",ui.index("if (logicalY < HUB_UI_TOP) {"))]
assert "if (logicalX <= 31) return 0;" in top  # only MENU hand fallback
assert "if (tab != 0) return tab;" in top
assert "return -1;" in top # absorb unused upper band incl AUTOMAP/PASS
assert "headerTabPages[k]" in top
assert "HUB_UI_INV_LEFT" not in ui
assert "HUB_UI_STATUS_LEFT" not in ui
assert "HUB_UI_SYSTEM_LEFT" not in ui
assert "#define HUB_UI_ROW0_TOP 23" in ui and "#define HUB_UI_ROW3_BOTTOM 115" in ui
assert "inventoryRowForY(logicalY)" in ui
assert "inventoryRowBounds(row, &top, &bottom)" in ui
assert "*outPage = headerTabPages[k];" in ui
assert "if (logicalY >= 21 && logicalY < 120) return -1;" in sys
assert "fillRect(framebuffer, left, 23, right, 51" in ui
assert "drawStatusProgress(framebuffer, 55, 153, 74" in ui

code=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_hub_touch_ui.h"
int main(void) {
    const int starts[3]={34,76,117},ends[3]={74,115,157};
    int claimed=0;
    for(int k=0;k<3;++k) {
        int l=-1,r=-1;
        assert(EspNativeGameplayHubTouchUi_headerTabBounds(k,&l,&r));
        assert(l==starts[k]&&r==ends[k]);
    }
    assert(!EspNativeGameplayHubTouchUi_headerTabBounds(-1,0,0));
    assert(!EspNativeGameplayHubTouchUi_headerTabBounds(3,0,0));
    assert(ESP_NATIVE_HUB_TOP_TAB_TOP==2);
    assert(ESP_NATIVE_HUB_TOP_TAB_BOTTOM==17);
    for(int py=0;py<40;++py)for(int px=0;px<320;++px) {
        int x=px/2,y=py/2,hit=-1;
        for(int k=0;k<3;++k) {
            int l,r;
            assert(EspNativeGameplayHubTouchUi_headerTabBounds(k,&l,&r));
            if(x>=l&&x<=r&&y>=2&&y<=17) {
                assert(hit==-1);
                hit=k;
                ++claimed;
            }
        }
        if(x<32) assert(hit==-1); /* 32x20 MENU underlay unaffected */
        if(hit>=0) assert(x>=34&&x<=157&&y>=2&&y<=17);
    }
    assert(claimed==(41+40+41)*16*4);
    puts("HUB-TOPBAR: PASS paint/touch geometry 160x20, 3 tabs, hand isolation");
}
"""
with tempfile.TemporaryDirectory(prefix="doom-hub-topbar-") as temp:
    c=Path(temp)/"main.c"
    exe=Path(temp)/"main"
    c.write_text(code)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(inc),str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("HUB-TOPBAR: PASS modal header owned; no world AUTOMAP/PASS on top band")
