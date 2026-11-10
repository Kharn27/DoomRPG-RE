#!/usr/bin/env python3
"""Run real production pain services against fake modal framebuffer ownership.

CYD 261108-022641: the HUB weapon grid opened successfully, then
PLAYERPAINFX EXPIRE performed a world render over the modal framebuffer.
This is a presentation-ownership failure, not a demonstrated CPU panic.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/esp_native_gameplay_gib_fx.c").read_text()
assert '#include "esp_native_gameplay_hub.h"' in source

def production_function(name: str) -> str:
    token = "static void " + name + "("
    start = source.index(token)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unbalanced source function " + name)

owners = ("serviceHitExpiry", "servicePlayerPainShake",
          "servicePlayerPainExpiry", "serviceGibAnimation", "serviceExpiry")
for name in owners:
    code = production_function(name)
    guard = code.index("EspNativeGameplayHub_isActive()")
    draw = code.index("EspNativeGameplayFrame_renderTurn(")
    assert guard < draw, ("HUB modal guard must precede world renderer", name)
    assert "EspNativeGameplayHub_isActive() ||" in code, name

body = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct Render_s { int dummy; } Render_t;
typedef struct DoomRPG_s { uint8_t processIdentity; } DoomRPG_t;
typedef struct EspNativeRootOwners_s { Render_t* render; } EspNativeRootOwners_t;
typedef struct EspPlayerViewState_s {
    int viewX, destX, viewY, destY, viewAngle, destAngle;
    unsigned char active;
} EspPlayerViewState;
typedef struct EspNativeGameplayFrameStats_s {
    uint32_t frameAfterFNV;
} EspNativeGameplayFrameStats;

static int hubActive, expiredDue, shakeDue, renders, acknowledges;
static int renderSucceeds = 1, worldOwner = 1;
static EspPlayerViewState pose = {1, 1, 2, 2, 64, 64, 1};
static Render_t renderer;
static DoomRPG_t doom = {1};
static EspNativeRootOwners_t nativeOwners = {&renderer};
static EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &nativeOwners; }

uint32_t DoomRPG_GetUpTimeMS(void) { return 550; }
int EspNativePlayerPainVisual_expiryDue(uint32_t now) {
    (void)now; return expiredDue;
}
int EspNativePlayerPainVisual_ackExpiry(uint32_t now) {
    (void)now; ++acknowledges; expiredDue = 0; return 1;
}
int EspNativePlayerPainVisual_shakeRefreshDue(uint32_t now) {
    (void)now; return shakeDue;
}
int EspNativeGameplayHub_isActive(void) { return hubActive; }
int EspNativeResidentGameplay_ownsWorldPresent(void) { return worldOwner; }
int EspNativeGameplayControls_isActive(void) { return 0; }
int EspAssetPack_isOpen(void) { return 0; }
int EspNativeGameplayEventChain_isWaiting(void) { return 0; }
int EspNativeResidentGameplay_isAutomapActive(void) { return 0; }
const EspPlayerViewState* EspPlayerView_view(void) { return &pose; }
int EspNativeGameplayFrame_renderTurn(Render_t* render,
                                     uint8_t angle,
                                     EspNativeGameplayFrameStats* frame) {
    assert(render == &renderer && angle == 64);
    ++renders;
    if (frame) frame->frameAfterFNV = 0x71ca7465;
    return renderSucceeds;
}
'''
body += "\n" + "\n\n".join(production_function(n) for n in (
    "servicePlayerPainShake", "servicePlayerPainExpiry"))
body += r'''
int main(void) {
    hubActive = 1;
    expiredDue = shakeDue = 1;
    servicePlayerPainShake(&doom);
    servicePlayerPainExpiry(&doom);
    assert(renders == 0 && acknowledges == 0 && expiredDue == 1);

    // A HUB close exposes world presentation again: one ordinary redraw
    // clears the pain overlay and only a successful redraw acknowledges it.
    hubActive = 0;
    shakeDue = 0;
    renderSucceeds = 0;
    servicePlayerPainExpiry(&doom);
    assert(renders == 1 && acknowledges == 0 && expiredDue == 1);
    renderSucceeds = 1;
    servicePlayerPainExpiry(&doom);
    assert(renders == 2 && acknowledges == 1 && expiredDue == 0);

    shakeDue = 1;
    servicePlayerPainShake(&doom);
    assert(renders == 3);
    hubActive = 1;
    servicePlayerPainShake(&doom);
    assert(renders == 3);
    puts("PASS real pain expiry and shake: HUB owns framebuffer");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    c = path / "hub_pain_owner.c"
    executable = path / "hub_pain_owner"
    c.write_text(body)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", str(c), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

print("PASS 5 asynchronous world services fail closed during HUB weapon/modal owner")


# The former crash is cured but the following hardware session proves a
# second, independent modal leak: [HUBWGRID] FRAME -> [PLAYERIMPACT] PAINT ->
# [HUB] FRAME. The presentation wrapper must return to direct physical LCD
# publishing before any live impact or action feedback can touch the grid.
def production_signature(signature: str) -> str:
    begin = source.index(signature)
    opening = source.index("{", begin)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:i + 1]
    raise AssertionError("unbalanced production function " + signature)


present = production_signature("int EspNativeGameplayActionEngine_present(void)")
assert present.index("if (EspNativeGameplayHub_isActive())") < present.index(
    "decorateActiveHit();"
)
assert present.index("return __real_Esp32PlatformVideo_present();") < present.index(
    "decoratePlayerImpact();"
)
assert 'extern int __real_Esp32PlatformVideo_present(void);' in source
impact = production_function("decoratePlayerImpact")
assert "!EspNativeGameplayHub_isActive()" in impact
assert impact.index("!EspNativeGameplayHub_isActive()") < impact.index(
    "EspNativeSpriteRenderer_paintPlayerImpact("
)
dog = present.split("if (EspNativeResidentGameplay_ownsWorldPresent()", 1)[1]
assert "!EspNativeGameplayHub_isActive()" in dog

presentation_body = r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
typedef struct EspNativeGameplayPlayerState_s {
    uint8_t active;
    uint32_t berserkerTics;
} EspNativeGameplayPlayerState;
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
static int modal = 1, directPresents, basePresents, decorators;
static uint16_t pixels[160 * 120];
int EspNativeGameplayHub_isActive(void) { return modal; }
void decorateActiveHit(void) { ++decorators; }
void decorateActiveGib(void) { ++decorators; }
void decorateNewGibs(void) { ++decorators; }
void decoratePlayerImpact(void) { ++decorators; }
int __real_Esp32PlatformVideo_present(void) {
    ++directPresents;
    return 1;
}
int EspNativeGameplayActionEngine_presentBase(void) {
    ++basePresents;
    return 1;
}
int EspNativeResidentGameplay_ownsWorldPresent(void) { return 1; }
int EspNativeResidentGameplay_isAutomapActive(void) { return 0; }
int EspNativeGameplayControls_isActive(void) { return 0; }
int EspAssetPack_isOpen(void) { return 0; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return sizeof(pixels); }
void* Esp32PlatformVideo_framebuffer(void) { return pixels; }
uint32_t DoomRPG_GetUpTimeMS(void) { return 200; }
int EspNativePlayerPainVisual_dogInfo(uint32_t ms, uint32_t* seed,
                                     uint32_t* age) {
    (void)ms; (void)seed; (void)age; return 0;
}
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    static EspNativeGameplayPlayerState player = {1, 0};
    return &player;
}
int EspNativeDogBlood_paint(uint16_t* fb, int width, int height,
                            uint32_t seed, uint32_t age, uint8_t berserker) {
    (void)fb; (void)width; (void)height; (void)seed;
    (void)age; (void)berserker; assert(0); return 0;
}
'''
presentation_body += "\n" + present + "\n"
presentation_body += r'''
int main(void) {
    // The real resident world remains active during an open HUB. A live
    // impact callback is registered but must not decorate those pixels.
    assert(EspNativeGameplayActionEngine_present());
    assert(directPresents == 1 && basePresents == 0 && decorators == 0);
    modal = 0;
    assert(EspNativeGameplayActionEngine_present());
    assert(directPresents == 1 && basePresents == 1 && decorators == 4);
    modal = 1;
    assert(EspNativeGameplayActionEngine_present());
    assert(directPresents == 2 && basePresents == 1 && decorators == 4);
    puts("PASS real compositor: modal directly presented, world decorations deferred");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    c = path / "hub_present.c"
    executable = path / "hub_present"
    c.write_text(presentation_body)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", str(c), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

print("PASS source impact/owner guard and real compositor own direct-modal-present path")
