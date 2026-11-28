#!/usr/bin/env python3
"""Compile the production HUB open/capture functions against a tiny fake CYD.

The hardware 261107 log exposed an unchanged-player close with displayed
bottom 478495be but canonical live HUD 7ac03eca. This runs the *actual*
openPage/menuOverlayCapture/hud hashing source with a deliberately stale bottom
and verifies the close can still demand strict baseline equality. No second
framebuffer, player mutation, heap owner or invented hardware PASS.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "ESP32/src/esp_native_gameplay_hub.c").read_text(encoding="utf-8")


def function(signature):
    begin = SOURCE.index(signature)
    left = SOURCE.index("{", begin)
    depth = 0
    for i in range(left, len(SOURCE)):
        if SOURCE[i] == "{":
            depth += 1
        elif SOURCE[i] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[begin:i + 1]
    raise AssertionError("unclosed production function: " + signature)


names = (
    "static uint32_t fnv1aUpdate(",
    "static uint32_t fnv1a32(",
    "static int framebufferReady(",
    "static uint32_t hudBandsFNV(",
    "static uint32_t hudProtectedFNV(",
    "static uint32_t menuZoneFNV(",
    "static int menuOverlayCapture(",
    "static int menuOverlayRestore(",
    "static EspNativeGameplayHubStatus openPage(",
)
production = "\n\n".join(function(name) for name in names)
opener = function(names[-1])
close = SOURCE.split("if (action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {", 1)[1].split(
    "if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT)", 1)[0]
assert opener.index("repaintGameplayHud()") < opener.index("menuOverlayCapture(framebuffer)")
assert "canonicalBottom = hudProtectedFNV();" in opener
assert "if (canonicalBottom == 0U)" in opener
assert "BASELINE-PRIME" in opener
assert "menuOverlay.baselineProtectedFNV = hudProtectedFNV();" in production
assert "restoredProtected == exactBottomTarget" in close
assert "return (playerExact && menuRestored && hudRepainted" in close
assert "HUBHUD] REBASE-FAILED" in close
assert "menuUnderlayRestore" in close

preamble = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define GAMEPLAY_BOTTOM_HUD_Y 100U
#define HUB_BAND_ROWS 20U
#define HUB_BAND_PIXELS (DOOMRPG_LOGICAL_WIDTH * HUB_BAND_ROWS)
#define HUB_MENU_LEFT 0U
#define HUB_MENU_WIDTH 32U
#define HUB_MENU_HEIGHT 20U
#define HUB_MENU_PIXELS (HUB_MENU_WIDTH * HUB_MENU_HEIGHT)
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT 12U
#define HUB_FACE_FRAME 0U

typedef enum {
    ESP_NATIVE_GAMEPLAY_HUB_NOT_READY=1,
    ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY=2,
    ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED=3,
    ESP_NATIVE_GAMEPLAY_HUB_IGNORED=4,
    ESP_NATIVE_GAMEPLAY_HUB_OK=7
} EspNativeGameplayHubStatus;
enum {
    ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY=0,
    ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS=1
};
typedef struct {
    uint32_t param1,param2,currentXP,nextLevelXP,keys,credits,xpGained;
    uint16_t weapons,disabledWeapons,berserkerTics;
    uint8_t ammo[6],inventory[5],level,weapon,active,reserved;
} EspNativeGameplayPlayerState;
typedef struct {
    uint32_t opens,closes,paints,playerFNVAtOpen,lastPlayerFNV,lastFrameFNV;
    uint8_t selectedRow,active,page,weaponAtOpen;
} EspNativeGameplayHubView;
typedef struct {
    uint16_t underlay[HUB_MENU_PIXELS];
    uint32_t baselineZoneFNV,paintedZoneFNV,baselineHudBandsFNV;
    uint32_t baselineProtectedFNV;
    uint8_t active,reserved[3];
} EspNativeGameplayHubMenuOverlay;

static uint16_t framebuffer[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static EspNativeGameplayHubMenuOverlay menuOverlay;
static EspNativeGameplayHubView hub;
static EspNativeGameplayPlayerState player;
static uint8_t notebookOpen,notebookPage;
static int busy, allowRepaint=1, allowPaint=1, repaintCalls, paintCalls;
static uint16_t canonical=0x5678U;

static void* Esp32PlatformVideo_framebuffer(void) { return framebuffer; }
static size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return sizeof(framebuffer);
}
static int EspAssetPack_isOpen(void) { return busy; }
static int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) {
    *out=player; return 1;
}
static uint32_t EspNativeGameplayPlayerState_fingerprint(void) {
    return 0xabcd1234U;
}
static int repaintGameplayHud(void) {
    ++repaintCalls;
    if (!allowRepaint) return 0;
    for (unsigned i=0;i<HUB_BAND_PIXELS;++i)
        framebuffer[GAMEPLAY_BOTTOM_HUD_Y*DOOMRPG_LOGICAL_WIDTH+i]=canonical;
    return 1;
}
static EspNativeGameplayHubStatus paintCurrentPage(void) {
    ++paintCalls;
    if (!allowPaint) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
    for (unsigned i=0;i<HUB_BAND_PIXELS;++i)
        framebuffer[GAMEPLAY_BOTTOM_HUD_Y*DOOMRPG_LOGICAL_WIDTH+i]=0xaaaa;
    return ESP_NATIVE_GAMEPLAY_HUB_OK;
}
static const char* pageName(uint8_t page) {
    return page == 1 ? "weapons" : "inventory";
}
"""
main = r"""
static void seed(uint16_t displayed) {
    memset(&menuOverlay, 0, sizeof(menuOverlay));
    memset(&hub, 0, sizeof(hub));
    memset(&player, 0, sizeof(player));
    player.active=1;
    player.weapon=3;
    player.weapons=0x000f;
    notebookOpen=0; notebookPage=0;
    busy=0; allowPaint=1; allowRepaint=1;
    paintCalls=0; repaintCalls=0;
    for (unsigned i=0;i<DOOMRPG_LOGICAL_WIDTH*DOOMRPG_LOGICAL_HEIGHT;++i)
        framebuffer[i]=0x1234U;
    for (unsigned i=0;i<HUB_BAND_PIXELS;++i)
        framebuffer[GAMEPLAY_BOTTOM_HUD_Y*DOOMRPG_LOGICAL_WIDTH+i]=displayed;
}
static void testOpen(uint16_t initial, uint8_t page) {
    seed(initial);
    const uint32_t before=hudProtectedFNV();
    assert(openPage(page)==ESP_NATIVE_GAMEPLAY_HUB_OK);
    assert(hub.active==1 && menuOverlay.active==1);
    assert(hub.page==page && hub.opens==1);
    assert(repaintCalls==1 && paintCalls==1);
    assert(menuOverlay.baselineProtectedFNV!=0);
    if (initial != canonical)
        assert(menuOverlay.baselineProtectedFNV != before);
    /* The HUB overwrote its bottom band. Restore must still be pixel-exact
     * to the primed pre-HUB reference, with NO tolerance or baseline rewrite. */
    assert(menuOverlayRestore(framebuffer));
    assert(repaintGameplayHud());
    assert(menuOverlay.baselineProtectedFNV==hudProtectedFNV());
    assert(repaintCalls==2);
    assert(!menuOverlay.active);
}
int main(void) {
    testOpen(0x1111U,ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY);
    testOpen(canonical,ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY);
    testOpen(0x2222U,ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS);
    seed(0x3333);
    allowRepaint=0;
    assert(openPage(ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY)==
           ESP_NATIVE_GAMEPLAY_HUB_NOT_READY);
    assert(!hub.active && !menuOverlay.active && !paintCalls);
    seed(0x4444);
    busy=1;
    assert(openPage(ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY)==
           ESP_NATIVE_GAMEPLAY_HUB_PACK_BUSY);
    assert(!hub.active && !repaintCalls);
    seed(0x5555);
    allowPaint=0;
    assert(openPage(ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY)==
           ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED);
    assert(!hub.active && !menuOverlay.active);
    puts("HUBHUD BASELINE PASS: stale/current, inventory/weapons, exact close, IO/busy fail-closed");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-hud-prime-") as td:
    src = Path(td) / "production_hub_prime.c"
    exe = Path(td) / "hub_prime"
    src.write_text(preamble + "\n" + production + "\n" + main, encoding="utf-8")
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        str(src), "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS actual production HUB open/capture source, strict unchanged-player close contract")


# STORE is a separate full-screen owner. Compile the actual resident close
# branch with its real canonical HUD adapter, not a rewritten algorithm.
resident_source = (ROOT / "ESP32/src/esp_native_resident_gameplay.c").read_text(encoding="utf-8")
begin = resident_source.index("    if (EspNativeGameplayStore_closePending()) {")
end = resident_source.index("    if (EspNativeGameplayStore_isActive()) return;", begin)
close_store = resident_source[begin:end]
begin = resident_source.index("static EspNativeGameplayHudStatus repaintLiveHudAtView(")
end = resident_source.index("\nstatic int renderAutomapCurrent(", begin)
hud_adapter = resident_source[begin:end]
store_preamble = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { int active, viewAngle, destAngle; } EspPlayerViewState;
typedef struct { int ammo, credits; } EspNativeGameplayPlayerState;
typedef struct { unsigned pixelsWritten; } EspNativeGameplayHudStats;
typedef enum {
    ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT=0,
    ESP_NATIVE_GAMEPLAY_HUD_OK=1
} EspNativeGameplayHudStatus;
typedef struct { void* render; } RootOwners;
static RootOwners root;
static EspPlayerViewState view;
static EspNativeGameplayPlayerState player;
static uint16_t fb[160*120];
static int pending, disabled, repaintOk, renderOk, paints, renders, finishes;
static const EspPlayerViewState* EspPlayerView_view(void) {return &view;}
static const void* EspNativeGameplayHud_view(void) {return &root;}
static const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {return &player;}
static RootOwners* EspNativeRootOwners_live(void) {return &root;}
static int EspNativeGameplayStore_closePending(void) {return pending;}
static void EspNativeGameplayStore_finishClose(void) {
    assert(pending && paints==1 && renders==1 && renderOk);
    ++finishes; pending=0;
}
static void disableGameplay(const char* reason) {assert(reason); ++disabled;}
static EspNativeGameplayHudStatus EspNativeGameplayHud_repaintLive(
    const void* retained, const EspNativeGameplayPlayerState* live,
    uint8_t angle, EspNativeGameplayHudStats* stats) {
    assert(retained==&root && live==&player && angle==64);
    ++paints;
    if (!repaintOk) return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;
    for(unsigned i=0;i<160*20;++i) {
        fb[i]=(uint16_t)live->credits;
        fb[160*100+i]=(uint16_t)live->ammo;
    }
    stats->pixelsWritten=6400U;
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}
static int renderCurrent(void* r,uint8_t angle,const char* reason) {
    (void)r;
    assert(angle==64 && strcmp(reason,"STORE-CLOSE")==0);
    assert(pending && paints==1 && repaintOk);
    for(unsigned i=0;i<160*20;++i) {
        assert(fb[i]==player.credits);
        assert(fb[160*100+i]==player.ammo);
    }
    ++renders;
    return renderOk;
}
"""
store_main = r"""
static void seed_store(int ammo,int credits) {
    memset(fb,0,sizeof(fb));
    view=(EspPlayerViewState){1,64,64};
    player=(EspNativeGameplayPlayerState){ammo,credits};
    pending=1; disabled=0; repaintOk=1; renderOk=1;
    paints=renders=finishes=0;
}
int main(void) {
    seed_store(17,12); closeService(); /* refusal, unchanged player */
    assert(!pending && !disabled && paints==1 && renders==1 && finishes==1);
    assert(player.ammo==17 && player.credits==12);
    seed_store(27,4); closeService(); /* purchase: use current values */
    assert(!pending && !disabled && paints==1 && renders==1 && finishes==1);
    assert(player.ammo==27 && player.credits==4);
    seed_store(17,12); view.destAngle=128; closeService();
    assert(pending && disabled==1 && !paints && !renders && !finishes);
    seed_store(17,12); repaintOk=0; closeService();
    assert(pending && disabled==1 && paints==1 && !renders && !finishes);
    seed_store(17,12); renderOk=0; closeService();
    assert(pending && disabled==1 && paints==1 && renders==1 && !finishes);
    puts("PASS STORE close: live HUD before world present; refusal/purchase and failure order");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-store-hud-") as td:
    src = Path(td) / "store_close.c"
    exe = Path(td) / "store_close"
    src.write_text(store_preamble + "\n" + hud_adapter +
                   "\nstatic void closeService(void) {\n" +
                   close_store + "\n}\n" + store_main, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
