# MAP1 native gameplay touch input

## Status

```text
branch = agent/esp32-native-touch-input-intent
base   = 7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
implementation HEAD = 8c620093650ac4efd5d470343466ce9f5c441e4e
status = REAL-CYD VISUAL PASS / FINAL SERIAL ARCHIVE PENDING
merge-ready = no (until final Serial block from implementation HEAD is archived)
```

Base `7686f7fb5c93d375f51a34ec0dd0b5cb127017e3` is merged PR #91 (`agent/esp32-native-gameplay-hud`).

## Objective

Add the first permanent native gameplay input ownership boundary without executing gameplay actions.

The CYD touch driver remains `SoftXpt2046`. A calibrated physical press is converted to logical 160x120 coordinates, classified into one compact pointer-free semantic intent, consumed immediately by the temporary probe, and rendered as a short exact-restoring visual feedback overlay.

This milestone deliberately does **not** move the player, rotate the view, change weapons, advance the turn, open menus, open automap, activate entities, or invoke the broad legacy playing-event loop.

## Permanent semantic owner

Permanent modules:

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

Recovered legacy action IDs are retained:

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

World viewport (`y=20..99`) is a 3x3 keypad:

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

The current diagnostic/polish feedback is intentionally allocation-free and renderer-independent:

```text
hold = 250 ms
style = neon double ring + vector glyph
max edits = 512
edit = framebuffer offset + saved RGB565 pixel = 4 B
static bounded edit storage ~= 2048 B + metadata
runtime allocations = 0
restore = reverse-order exact pixel restore
```

Palette by row/family:

```text
top HUD controls = BLUE neon
movement row      = GREEN neon
turn/select row   = YELLOW neon
weapon/back row   = RED neon
```

Vector glyphs are generated procedurally and require no asset/PAK access:

```text
movement     arrows
turn         bent arrows
SELECT       reticle
PREV_WEAPON  <<
NEXT_WEAPON  >>
MENU         three-line icon
AUTOMAP      compact map icon
PASS_TURN    arrow-to-stop glyph
```

The user explicitly reported the final row-coded neon layout as visually perfect on the physical classic CYD.

## Hardware evidence already archived before final polish

An earlier hardware run of the same semantic owner and touch path (before the final 12-zone/layout + row-color polish) proved the core input/restore contract on real hardware:

```text
baseline frame = ba3e5182
EspNativeGameplayInputState = 12 B
EspNativeGameplayTouchHit = 6 B
feedback predecessor storage = 332 B
heap8 = 69828 -> 69828
largest8 = 34804 -> 34804
heapDelta = 0
largestDelta = 0
all exercised valid taps produced INTENT
all exercised feedback restores returned exactly to frame ba3e5182
no gameplay dispatch
```

The archived run exercised movement, turn, select, weapon, menu and automap semantics and showed exact restore after each transient overlay. It also proved an intentionally unbound region returns `NO_HIT` without mutating gameplay.

## Final-HEAD evidence still required before merge-ready

The final implementation HEAD changed two hardware-visible details after the archived Serial run:

```text
11 active zones -> 12 active zones
single-color/inversion feedback -> row-coded neon double-ring + vector glyph feedback
```

Therefore the repo must not promote `8c620093650ac4efd5d470343466ce9f5c441e4e` to hardware-tested/merge-ready until a Serial block from that exact firmware is archived.

Required final proof is intentionally small:

```text
[NATIVEINPUTPROBE] READY ... zones=12 ... baseline=ba3e5182 ...
representative BLUE/GREEN/YELLOW/RED taps
PREV_WEAPON and NEXT_WEAPON recognized
PASS_TURN recognized in top message area
FEEDBACK RESTORE ... frame=ba3e5182 exact=yes
heap/largest unchanged
no FAILED/ERROR
```

No unobserved final-HEAD framebuffer hashes, heap values or per-action overlay FNVs are canonicalized here.

## Invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP graphics/map access forbidden
legacy Game.entities == 0
legacy Game.monsters == 0
legacy broad gameplay loop not called
turn advancement = none
player/view mutation = none
world mutation = none
entity activation = none
```

The touch intent owner is the first native gameplay-input boundary; action execution remains outside this milestone.

## Next bounded milestone after merge

After the final Serial proof and merge, recover the exact new `main` SHA before branching again.

The first real gameplay dispatcher milestone should enable only one small action family (preferably `TURN_LEFT` / `TURN_RIGHT`) while all other actions remain recognized but fail closed/deferred. Do not combine first dispatch with movement collision, turn advancement, monsters or general entity gameplay.