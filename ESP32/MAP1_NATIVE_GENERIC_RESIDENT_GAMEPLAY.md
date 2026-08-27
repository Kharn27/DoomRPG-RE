# MAP1 native generic resident gameplay — real-CYD hardware PASS

## Boundary

This archive closes the long Entrance startup branch by proving that the previously validated Junction gameplay/render stack is a generic resident-map engine, not a Junction-specific engine.

```text
branch = agent/esp32-native-entrance-startup-route
base = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
status = REAL-CYD HARDWARE PASS
```

Permanent architecture is summarized in [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md).

## Hardware route

Real classic CYD completed:

```text
intro cinematic
 -> Entrance resident /intro.bsp
 -> spawn tile 904, direction 64
 -> generic gameplay session
 -> first world frame
 -> native HUD
 -> sprite dependency closure
 -> small resident cache cold/warm
 -> exact 2048-B cache learn/warm
 -> collision catalog
 -> native touch gameplay
```

Final session witness:

```text
[ENGINESESSION] READY map=1 angle=64
residentCache=yes
largeCache=yes
touch=invisible-120ms
TURN+MOVE=armed
shapeData=0x0
mediaTexels=0x0
```

## Engine/cache proof

```text
catalog textures=33 sprites=45 storage=3120 fnv=29ffc14a
sprite dependency closure -> sprites=46

resident cache owner=21160 B
payload=16384 B
entry capacity=256
heap8=57496->31956
largest8=29684->8692

SMALL-COLD  totalUs=2119886
SMALL-WARM  totalUs=256807
LARGE-LEARN totalUs=247770
LARGE-WARM  totalUs=229719
large entries=2
payload used=14645/16384 B
```

The plane renderer successfully reconstructs 12800 floor/ceiling pixels after the resident cache is active. Its six 2048-B texture slots no longer require one contiguous 12288-B allocation.

## Gameplay proof

The tester walked and turned repeatedly in Entrance. Representative committed actions:

```text
MOVE 904 -> 872
MOVE 872 -> 840
TURN 64 -> 0
TURN 0 -> 192
MOVE 840 -> 872
MOVE 872 -> 904
TURN 192 -> 128
TURN 128 -> 64
```

Frames recomposed with walls, planes, sprites/glows and HUD. Representative sprite outputs include:

```text
8 draws / 3049 pixels
4 draws / 1754 pixels
6 draws / 6776 pixels
3 draws / 1485 pixels
10 draws / 550 pixels
13 draws / 3789 pixels
```

Zero visible sprite draws are also a valid view and did not fail the compositor.

## Collision proof

At the Entrance starting area, forward movement into the closed/locked line is rejected:

```text
source tile=904
dest tile=936
line=258
texture=7
flags=00000505
type=0
defTile=312
result=BLOCKED
```

This proves generic line-state collision on Entrance.

## SELECT observer proof

The Action/SELECT zone is classified and routed through the existing map-generic resolver, but semantic execution remains intentionally disabled.

Observed Entrance interaction 1:

```text
front tile=841
event=88
commands=4
eligible EV_DIALOG
eligible EV_NEXTSTATE
remaining commands state-mismatched
```

Observed Entrance interaction 2:

```text
front tile=936
event=91
line=258
locked=1
eligible EV_OPENLINE(258)
```

The observer preserved heap/frame/resident/legacy state and performed no bytecode execution or door mutation. Gameplay then correctly logged `SELECT semantic-not-enabled`.

## Stability

Stable post-prime heartbeat during the supplied run:

```text
heap=97664
heap8=31956
largest8=8692
shapeData=NULL
mediaTexels=NULL
```

No Guru Meditation, reboot or engine failure was reported while walking/turning.

## Architectural conclusion

The correct production abstraction is:

```text
resident BSP
 -> common runtime owners
 -> common player/view
 -> common EspNativeGameplaySession
```

Junction and Entrance are hardware corpora for the same engine. Historical level-named probes must not become production prerequisites.

Future maps use the same pipeline. If a BSP exposes unsupported data/behavior, extend that family generically rather than adding a level-specific engine path.

## Next milestone

Bounded native Action/SELECT execution on Entrance:

```text
SELECT intent
 -> current front-tile resolver
 -> lock/key guard
 -> only explicitly supported native opcode/UI families
 -> correct mutation/UI intent
 -> redraw/turn semantics as recovered
 -> unsupported command => fail closed
```

Do not broad-enable legacy `Game_executeEvent`.

## Merge boundary

```text
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
REAL-CYD HARDWARE PASS
MERGE-READY = YES
```

All commits after the implementation SHA are documentation-only closeout.
