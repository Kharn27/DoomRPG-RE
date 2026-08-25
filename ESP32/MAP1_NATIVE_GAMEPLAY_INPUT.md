# MAP1 native gameplay touch input

## Status

```text
branch = agent/esp32-native-touch-input-intent
base   = 7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
implementation HEAD = 8c620093650ac4efd5d470343466ce9f5c441e4e
merged PR = #92
merged main = cdda239f1c884a7d6f6707ba1c30a0a0a3603923
status = MERGED
```

Historical note: the final input-only SHA was merged before its own final Serial block was archived. Do not retroactively label `8c620093...` as hardware-Serial-proven. The subsequent TURN milestone at tested SHA `66ba643e7650f51d0022cd56e007242902d76c77` re-exercises the evolved input path on the real CYD, including exact dynamic frame restore, all current TURN hitboxes, neon feedback, and stable memory.

## Objective

Add the first permanent native gameplay input ownership boundary without executing gameplay actions.

The CYD touch driver remains `SoftXpt2046`. A calibrated physical press is converted to logical 160x120 coordinates, classified into one compact pointer-free semantic intent, consumed by the temporary probe, and rendered as a short exact-restoring visual feedback overlay.

This milestone itself does **not** move the player, rotate the view, change weapons, advance the turn, open menus, open automap, activate entities, or invoke the broad legacy playing-event loop.

## Permanent semantic owner

```text
ESP32/include/esp_native_gameplay_input.h
ESP32/src/esp_native_gameplay_input.c
```

Compact ownership:

```text
EspNativeGameplayInputState = 12 B
EspNativeGameplayTouchHit   = 6 B
one pending intent maximum
no hidden queue
busy producer => fail closed
consumer explicitly clears pending
```

Recovered legacy action IDs:

```text
1  FORWARD
2  BACK
3  TURN_LEFT
4  TURN_RIGHT
5  MENU
6  SELECT
7  AUTOMAP
9  STRAFE_LEFT
10 STRAFE_RIGHT
11 PREV_WEAPON
12 NEXT_WEAPON
14 PASS_TURN
```

## Final CYD touch layout

Top HUD (`y=0..19`):

```text
x=0..31    MENU
x=32..127  PASS_TURN / message area
x=128..159 AUTOMAP
```

World viewport (`y=20..99`) is a full 3x3 keypad:

```text
row y=20..45
  x=0..52    STRAFE_LEFT
  x=53..105  FORWARD
  x=106..159 STRAFE_RIGHT

row y=46..72
  x=0..52    TURN_LEFT
  x=53..105  SELECT
  x=106..159 TURN_RIGHT

row y=73..99
  x=0..52    PREV_WEAPON
  x=53..105  BACK
  x=106..159 NEXT_WEAPON
```

Bottom HUD (`y=100..119`) remains unbound.

This preserves the original phone-keypad spirit while avoiding permanent on-screen controls.

## Transient visual feedback

The merged input branch introduced allocation-free renderer-independent feedback:

```text
style = neon double ring + vector glyph
max edits = 512
edit = framebuffer offset + saved RGB565 pixel = 4 B
static bounded edit storage ~= 2 KiB + metadata
runtime allocations = 0
restore = reverse-order exact pixel restore
```

Palette by family:

```text
top HUD controls = BLUE
movement row      = GREEN
turn/select row   = YELLOW
weapon/back row   = RED
```

Vector glyphs cover movement arrows, bent turn arrows, SELECT reticle, `<<`/`>>` weapon cycling, MENU, AUTOMAP and PASS_TURN.

The user visually confirmed the final colored 12-zone layout as perfect on the physical classic CYD.

The follow-on TURN milestone evolved the same feedback from a fixed initial-frame restore to a **dynamic current-frame restore** and shortened the hold to `120 ms`. That current behavior is documented in [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md).

## Hardware evidence history

An earlier real-CYD input run before the final 12-zone/color polish proved the core semantic and restore contract:

```text
baseline frame = ba3e5182
EspNativeGameplayInputState = 12 B
EspNativeGameplayTouchHit = 6 B
heap8 = 69828 -> 69828
largest8 = 34804 -> 34804
valid taps => semantic INTENT
feedback restore => ba3e5182 exact=yes
no gameplay dispatch
```

That run exercised movement, turn, select, weapon, menu and automap semantics and proved exact restore after transient overlays.

The final input-only implementation changed two hardware-visible details after that archived Serial run:

```text
11 active zones -> 12 active zones
single-color/inversion feedback -> row-coded neon double-ring + vector glyph
```

Therefore this archive intentionally does not invent exact runtime canons for `8c620093...`.

The later hardware-tested TURN SHA `66ba643e7650f51d0022cd56e007242902d76c77` proves the current evolved input path in real gameplay use:

```text
TURN hitbox recognition = yes
feedback hold = 120 ms
feedback dynamic baseline = yes
feedback restore = exact current-frame FNV
heap8 = 67284 -> 67284
largest8 = 34804 -> 34804
```

## Invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP graphics/map access forbidden
legacy Game.entities == 0
legacy Game.monsters == 0
legacy broad gameplay loop not called
input layer itself does not mutate player/view/world
```

## Follow-on milestone

The merged input owner is consumed by the first real native gameplay dispatcher archived in [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md). Only `TURN_LEFT` / `TURN_RIGHT` are executed there; all other recognized actions remain deferred.