#!/usr/bin/env python3
"""FIRE-VIS-P2: compile the real C fire clock and redraw policy, no PAK fixture.

Tests temporal phase/raster/presentation contracts only. Real fire asset
distinction requires the user-supplied PAK; no synthetic payload is presented
as proof of original art.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "ESP32/src/esp_native_sprite_renderer.c").read_text()
HEAD = (ROOT / "ESP32/include/esp_native_ambient_sprite.h").read_text()
SH = (ROOT / "ESP32/include/esp_native_sprite_renderer.h").read_text()
GAME = (ROOT / "ESP32/src/esp_native_gameplay_frame.c").read_text()
RESIDENT = (ROOT / "ESP32/src/esp_native_resident_gameplay.c").read_text()
LEGACY = (ROOT / "src/Render.c").read_text(encoding="latin-1")

def body(signature: str, text: str = SRC) -> str:
    assert text.count(signature) == 1, (signature, text.count(signature))
    start = text.index(signature)
    brace = text.index("{", start + len(signature))
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError("Unclosed C body: " + signature)


# No existing hardcoded 375ms rule may change for non-fire entities.
assert "#define AMBIENT_FAST_MS 375U" in HEAD
assert "#define AMBIENT_RAPID_MS 250U" in HEAD
assert "return type == 10U" in HEAD
assert "(info & 0x04000000UL) == 0U" in HEAD
assert "EspNativeAmbientSprite_eligible(info,type)" in HEAD
assert "return (uint8_t)(((ms+phase)/250U)&3U);" in HEAD
assert "EspNativeAmbientSprite_fireChanged(ambientFireIndex," in SRC
assert "ambientFireVisible &&" in SRC
assert "else if (EspNativeAmbientSprite_fast(entityType))" in SRC
assert "++stats->fireRasterCount;" in SRC
assert "if (stats->fireRasterCount == 0U)" in SRC
assert "++stats->ambientCandidates;" in SRC
assert "ambientFireVisible=(uint8_t)(stats.fireRasterCount!=0U);" in SRC
assert "else EspNativeSpriteRenderer_suspendAmbient();" in SRC
assert "EspNativeAmbientSprite_rasterCandidate(" in SRC
assert "if (!glow && EspNativeAmbientSprite_fireEligible(" in SRC
assert "frame->actual" in SRC and "loadFrame(sources, logical, animation, glow," in SRC
assert "fireRasterCount" in SH and "fireSampleActual" in SH

# The LCD proof must follow successful physical presentation, exclude automap
# replacement, and be capped to avoid serial timing perturbation.
assert "if (!Esp32PlatformVideo_present())" in GAME
assert GAME.index("if (!Esp32PlatformVideo_present())") < GAME.index(
    'printf("[FIREVIS-P2] PRESENT')
assert "if (ok && publish && preview && !EspNativeResidentGameplay_isAutomapActive()" in GAME
assert "firePresentedLogCount < 48U" in GAME
assert "stats->finalPresented = 1U;" in GAME
assert "stats->spritePackReads" in GAME
assert "stats->presentMicros" in GAME and "stats->spriteMicros" in GAME
assert "!EspNativeGameplayPresentGate_isArmed()" in RESIDENT
assert "EspNativeSpriteRenderer_ambientDue(nowMs)" in RESIDENT
assert "v->viewX==v->destX && v->viewY==v->destY" in RESIDENT
assert "if (!painted || !frame.finalPresented || !rngExact)" in RESIDENT
assert "EspNativeSpriteRenderer_suspendAmbient();" in RESIDENT
assert "render->animFrameTime = DoomRPG_GetTimeMS();" in LEGACY
assert "render->animFrameTime + (8 * (byte)sprite->ent)" in LEGACY

# Compile UNALTERED scheduler functions from the production C file.
POLICY = "\n\n".join(body(s) for s in (
    "int EspNativeSpriteRenderer_ambientDue(uint32_t now)",
    "void EspNativeSpriteRenderer_suspendAmbient(void)",
    "void EspNativeSpriteRenderer_resetAmbient(void)",
))
STATES = (
    "static uint32_t ambientSampleMs, ambientPresentedMs;\n"
    "static uint8_t ambientCandidates, ambientFastCandidates, ambientRapidCandidates;\n"
    "static uint8_t ambientFireVisible, ambientFirePhase;\n"
    "static uint16_t ambientFireIndex;\n"
    "static uint8_t ambientSuspended;\n"
)
assert STATES.replace("\n","") .replace(" ","") in SRC.replace("\n","").replace(" ","")
C = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_ambient_sprite.h"
static int indicator_due;
int EspNativeFirstFrame_indicatorDue(uint32_t ms) { (void)ms; return indicator_due; }
''' + STATES + POLICY + r'''
static void arm(uint16_t index, uint32_t ms) {
    EspNativeSpriteRenderer_resetAmbient();
    ambientCandidates=1U;
    ambientFireVisible=1U;
    ambientFireIndex=index;
    ambientPresentedMs=ms;
    ambientFirePhase=EspNativeAmbientSprite_frame(0,10,0,index,ms);
}
int main(void) {
    uint32_t ms, n;
    const uint16_t index=7U;
    assert(EspNativeAmbientSprite_fireEligible(0,10));
    assert(!EspNativeAmbientSprite_fireEligible(0x80000000U,10));
    assert(!EspNativeAmbientSprite_fireEligible(0x04000000U,10));
    assert(!EspNativeAmbientSprite_fireEligible(0,9));
    assert(!EspNativeAmbientSprite_fireEligible(0,1));
    for (ms=0;ms<1000U;ms+=250U)
        assert(EspNativeAmbientSprite_frame(0,10,0,0,ms)==ms/250U);
    assert(EspNativeAmbientSprite_fireChanged(7,0,193U)==0);
    assert(EspNativeAmbientSprite_fireChanged(7,0,194U)!=0);
    assert(EspNativeAmbientSprite_fireChanged(0,0,249U)==0);
    assert(EspNativeAmbientSprite_fireChanged(0,0,250U)!=0);

    /* Four real original clock phases, all presented in order when a
       redraw is requested immediately after each boundary. */
    arm(index,0);
    assert(!EspNativeSpriteRenderer_ambientDue(193));
    assert(EspNativeSpriteRenderer_ambientDue(194));
    for(n=1;n<=12U;n++) {
        const uint32_t at=n*250U-56U;
        const uint8_t expected=(uint8_t)(n&3U);
        assert(EspNativeSpriteRenderer_ambientDue(at));
        ambientFirePhase=EspNativeAmbientSprite_frame(0,10,0,index,at);
        ambientPresentedMs=at;
        assert(ambientFirePhase==expected);
        assert(!EspNativeSpriteRenderer_ambientDue(at+1U));
    }
    /* Slow policy remains 375ms for non-fire fast sprites and 4s
       for slow monsters; subtype4 rapid remains 250ms. */
    EspNativeSpriteRenderer_resetAmbient();
    ambientCandidates=ambientFastCandidates=1U; ambientPresentedMs=10U;
    assert(!EspNativeSpriteRenderer_ambientDue(384));
    assert(EspNativeSpriteRenderer_ambientDue(385));
    EspNativeSpriteRenderer_resetAmbient();
    ambientCandidates=ambientRapidCandidates=1U; ambientPresentedMs=0U;
    assert(!EspNativeSpriteRenderer_ambientDue(249));
    assert(EspNativeSpriteRenderer_ambientDue(250));
    EspNativeSpriteRenderer_resetAmbient();
    ambientCandidates=1U; ambientPresentedMs=0U;
    assert(!EspNativeSpriteRenderer_ambientDue(3999));
    assert(EspNativeSpriteRenderer_ambientDue(4000));
    /* Zero pixel-visible candidate, or failed draw suspended: no idle. */
    EspNativeSpriteRenderer_resetAmbient();
    assert(!EspNativeSpriteRenderer_ambientDue(10000));
    arm(0,0);
    EspNativeSpriteRenderer_suspendAmbient();
    assert(!EspNativeSpriteRenderer_ambientDue(10000));
    /* Idle policy does not replace the independently scheduled wall LED. */
    EspNativeSpriteRenderer_resetAmbient();
    indicator_due=1;
    assert(EspNativeSpriteRenderer_ambientDue(1));
    indicator_due=0;
    /* A phase change before the coarse 375ms test is no longer skipped. */
    arm(0,0);
    assert(EspNativeSpriteRenderer_ambientDue(250));
    assert(EspNativeAmbientSprite_due(250,0,1,0,1)==0);
    puts("FIRE-VIS-P2: real-C fire phase clock + normal fast/rapid/slow and fail-closed PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="doom-fire-vis-p2-") as td:
    test = Path(td) / "fire_p2.c"
    exe = Path(td) / "fire_p2"
    test.write_text(C)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "ESP32/include"),
                    str(test), "-o", str(exe)], check=True, cwd=ROOT)
    subprocess.run([str(exe)], check=True, cwd=ROOT)
print("PASS FIRE-VIS-P2 source ownership, LCD witness cap, no PAK fixture")
