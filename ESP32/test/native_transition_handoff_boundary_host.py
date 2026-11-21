#!/usr/bin/env python3
"""ARCH-RC18: compile the REAL two public transition facades and their raw leaves.

No firmware simulation: this tests exact extracted owner bodies with bounded
host dependency mocks and verifies linker/source ownership for the CYD build.
The original runtime/SD/controller/ISR must still be tested on physical CYD.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
INI = ROOT / "ESP32/platformio.ini"
PAK = SRC / "esp_asset_pack_perf_probe.cpp"
RAW_PAK = SRC / "esp_asset_pack.cpp"
MENU = SRC / "native_main_menu_touch.c"
INPUT = SRC / "platform_input.cpp"


def require(value, reason):
    if not value:
        raise AssertionError(reason)


def extract(source, signature):
    require(source.count(signature) == 1, "missing/ambiguous real body " + signature)
    begin = source.index(signature)
    depth = 0
    for pos in range(source.index("{", begin), len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:pos + 1]
    raise AssertionError("unbalanced real C/C++ body: " + signature)


pak_text = PAK.read_text()
raw_pak_text = RAW_PAK.read_text()
menu_text = MENU.read_text()
input_text = INPUT.read_text()
config = INI.read_text()
normal = config.split("[env:esp32-cyd]", 1)[1].split(
    "[env:esp-cyd-inverted]", 1)[0]

require(len(re.findall(r"^\s+-Wl,--wrap=", normal, re.M)) == 2,
        "ARCH-RC22 expects two genuine release GNU wrappers")
for name in ("EspAssetPack_residentBegin", "PlatformInput_setTapCallback"):
    require("-Wl,--wrap=" + name not in config,
            "old linker interposition resurrected: " + name)
    for file in (PAK, RAW_PAK, MENU, INPUT):
        source = file.read_text()
        require(not re.search(r"\b__(?:wrap|real)_" + name + r"\b", source),
                "retired alias in " + file.name + ": " + name)
require("extern \"C\" int EspAssetPack_residentBeginBase(void)" in raw_pak_text,
        "raw PAK leaf lost C linkage")
require("extern \"C\" void PlatformInput_setTapCallbackBase(" in input_text,
        "raw XPT2046 callback leaf lost C linkage")
require(raw_pak_text.count("int EspAssetPack_residentBeginBase(void)") == 1,
        "raw PAK begin body drift")
require(menu_text.count("void PlatformInput_setTapCallbackBase(") == 1,
        "duplicate low-level callback declaration in menu owner")

pak_body = extract(pak_text, "int EspAssetPack_residentBegin(void)")
menu_body = extract(menu_text, "void PlatformInput_setTapCallback(PlatformTapCallback callback)")
input_body = extract(
    input_text,
    'extern "C" void PlatformInput_setTapCallbackBase(PlatformTapCallback callback)'
)
require(pak_body.count("EspAssetPack_residentBeginBase()") == 1,
        "raw PAK leaf must execute once, after successful flash preflight")
require(menu_body.count("PlatformInput_setTapCallbackBase(") == 2,
        "public menu facade must route both gate and non-gate directly")
require("EspNativeGameplayTransitionHandoff_tryArmNullCallback()" in input_body,
        "raw input leaf has lost WAIT_STATS NULL callback guard")


PAK_HOST = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#define DRPG_LOGE(...) ((void)std::printf(__VA_ARGS__))
#define DRPG_LOGI(...) ((void)std::printf(__VA_ARGS__))
struct EspPlayerViewState { uint8_t active; uint8_t targetMapId; };
struct EspAssetPackMapFlashStats {
    uint32_t currentMapId, active, verified, reused;
    uint32_t stagedBytes, metadataBytes, buildMicros;
};
static EspPlayerViewState view{1,3};
static bool hasView = true, deferred = false, prepared = true, backend = true;
static int64_t now = 100;
static std::string trace;
static uint8_t preparedMap;
const EspPlayerViewState* EspPlayerView_view() {
    return hasView ? &view : nullptr;
}
int64_t esp_timer_get_time() { now += 20; return now; }
uint32_t elapsedMicros(int64_t start, int64_t end) {
    return (uint32_t)(end - start);
}
int EspNativeTransitionPresentation_isFlashDeferred() { return deferred ? 1 : 0; }
void EspNativeTransitionPresentation_progress(int) {}
void EspAssetPack_mapFlashSetProgressCallback(void (*p)(int)) {
    trace.push_back(p != nullptr ? 'C' : 'c');
}
int EspAssetPack_mapFlashPrepare(uint8_t map) {
    preparedMap = map; trace.push_back('P'); return prepared ? 1 : 0;
}
void EspNativeTransitionPresentation_flashReady() { trace.push_back('F'); }
int EspAssetPack_residentBeginBase() { trace.push_back('B'); return backend; }
void EspAssetPack_mapFlashDeactivate() { trace.push_back('D'); }
void EspAssetPack_mapFlashGetStats(EspAssetPackMapFlashStats* stats) {
    trace.push_back('S'); stats->currentMapId = preparedMap;
}
__PAK_BODY__
int main() {
    trace.clear(); hasView=true; view={1,3}; deferred=false; prepared=true; backend=true;
    assert(EspAssetPack_residentBegin()==1 && trace=="PBS" && preparedMap==3);
    trace.clear(); deferred=true; prepared=true; backend=true;
    assert(EspAssetPack_residentBegin()==1 && trace=="CPcFBS");
    trace.clear(); deferred=true; prepared=false;
    assert(EspAssetPack_residentBegin()==0 && trace=="CPc");
    trace.clear(); deferred=false; prepared=true; backend=false;
    assert(EspAssetPack_residentBegin()==0 && trace=="PBD");
    trace.clear(); deferred=true; prepared=true; backend=false;
    assert(EspAssetPack_residentBegin()==0 && trace=="CPcFBD");
    trace.clear(); hasView=false; deferred=true; prepared=true; backend=true;
    assert(EspAssetPack_residentBegin()==0 && trace=="Cc");
    trace.clear(); hasView=true; view.active=0;
    assert(EspAssetPack_residentBegin()==0 && trace=="Cc");
    puts("PASS RC18 real PAK C++: flash preflight/failure, progress handoff, raw-once, rollback");
    return 0;
}
""".replace("__PAK_BODY__", pak_body)

MENU_HOST = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
enum { ST_MENU = 2 };
using PlatformTapCallback = void (*)(int16_t,int16_t,uint16_t,uint16_t,uint16_t);
struct Canvas { int state; };
struct Owners { Canvas* doomCanvas; };
static Canvas canvas{9};
static Owners owners{&canvas};
static void* touchDoomRpg = (void*)0x1;
static int touchActive = 1, syncCalls = 0;
static int gateSelectedItem = 7, lastTappedItem = 7;
static unsigned gateTapCount = 99;
static PlatformTapCallback downstreamTapCallback = nullptr;
static PlatformTapCallback gTapCallback = nullptr;
static bool busy = false;
static int armAttempts = 0;
static Owners* EspNativeRootOwners_live() { return &owners; }
static void EspNativeMainMenu_transitionCanvasState(Canvas* canvas, void* root, int s) {
    assert(root == touchDoomRpg && canvas != nullptr);
    ++syncCalls; canvas->state = s;
}
static void DoomRPG_esp32MainMenuTouchOnTap(int16_t,int16_t,uint16_t,uint16_t,uint16_t) {}
static void gatedTap(int16_t,int16_t,uint16_t,uint16_t,uint16_t) {}
static void otherTap(int16_t,int16_t,uint16_t,uint16_t,uint16_t) {}
static int uacResets = 0;
static void EspUacMenuPreview_reset() { ++uacResets; }
static int EspNativeGameplayTransitionHandoff_tryArmNullCallback() {
    ++armAttempts; return busy ? 1 : 0;
}
__INPUT_BODY__
__MENU_BODY__
int main() {
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);
    assert(syncCalls==1 && canvas.state==ST_MENU);
    assert(gTapCallback==gatedTap);
    assert(downstreamTapCallback==DoomRPG_esp32MainMenuTouchOnTap);
    assert(gateSelectedItem==0 && lastTappedItem==-1 && gateTapCount==0);
    PlatformInput_setTapCallback(otherTap);
    assert(gTapCallback==otherTap && downstreamTapCallback==otherTap && syncCalls==1);
    busy=true; PlatformInput_setTapCallback(nullptr);
    assert(gTapCallback==otherTap && downstreamTapCallback==nullptr && armAttempts==1);
    busy=false; PlatformInput_setTapCallback(nullptr);
    assert(gTapCallback==nullptr && armAttempts==2);
    touchActive=0; canvas.state=9;
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);
    assert(syncCalls==1 && canvas.state==9 && gTapCallback==gatedTap);
    touchActive=1; touchDoomRpg=nullptr;
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);
    assert(syncCalls==1 && canvas.state==9);
    touchDoomRpg=(void*)0x1; owners.doomCanvas=nullptr;
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);
    assert(syncCalls==1 && gTapCallback==gatedTap);
    puts("PASS RC18 real input C++: menu gate, callback passthrough, WAIT_STATS NULL refusal");
    return 0;
}
""".replace("__INPUT_BODY__", input_body).replace("__MENU_BODY__", menu_body)

with tempfile.TemporaryDirectory(prefix="rc18-native-handoff-") as dirname:
    root = Path(dirname)
    for name, source, witness in (
        ("pak", PAK_HOST, "PASS RC18 real PAK C++"),
        ("input", MENU_HOST, "PASS RC18 real input C++"),
    ):
        cpp = root / (name + ".cpp")
        exe = root / name
        cpp.write_text(source)
        cmd = ["c++", "-std=c++17", "-O2", "-Wall", "-Wextra",
               "-Werror", str(cpp), "-o", str(exe)]
        subprocess.run(cmd, check=True)
        result = subprocess.run([str(exe)], text=True,
                                capture_output=True, check=True)
        require(witness in result.stdout, "missing real-body witness " + name)
        print(result.stdout.strip())

print("PASS ARCH-RC18: two real production transaction facades + raw callback leaf, 2+3 GNU wraps")
