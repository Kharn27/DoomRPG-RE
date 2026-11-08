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
typedef struct DoomRPG_s { Render_t* render; } DoomRPG_t;
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
static DoomRPG_t doom = {&renderer};

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
