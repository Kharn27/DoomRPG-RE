# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative current tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone/source on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main = 6e07187f60a27e197189a47f2cbc7ff4e338cfec
branch = agent/esp32-native-full-gameplay
base main = 6e07187f60a27e197189a47f2cbc7ff4e338cfec
hardware-tested code boundary = feae39c768105b8851a77dab1afa4b52bec231dd
status = jammed-door destructible subtype-3 hardware complete
branch policy = LOCKED; docs-only tail only
```

Code after `feae39c...` must not be treated as hardware-tested unless the user
supplies new real-CYD logs. The current tail is documentation-only.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

## Hardware / permanent memory rules

```text
classic CYD ESP32-2432S028R
ESP32-D0WD-V3 dual core 240 MHz
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

Do not recreate map-wide texel ownership or migrate native runtime data back to
ZIP. `/DoomRPG.zip` remains transitional bootstrap/menu compatibility debt only.

## Selected resident-cache baseline

Hardware-selected implementation remains:

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The cache still exhibits isolated working-set recycle stalls. Preserve this
baseline while correctness milestones advance unless a cache milestone is
explicitly chosen.

## Source layout

```text
ESP32/src/esp_map_*                        map/runtime/event ownership
ESP32/src/esp_player_*                     player/view ownership
ESP32/src/esp_native_gameplay_*            live gameplay/action semantics
ESP32/src/esp_native_first_frame.c         BSP/wall compatibility renderer
ESP32/src/esp_native_plane_renderer.c      native floor/ceiling plane renderer
ESP32/src/esp_native_sprite_renderer.c     native sprite renderer
ESP32/src/esp_asset_pack.cpp               PAK backing + resident cache
ESP32/src/esp_native_dynamic_line_render.c mutable line presentation
ESP32/src/platform_*                       CYD input/video bridges
```

A new BSP must never create another map-specific engine.

## Renderer stack rules

Two permanent hardware lessons:

```text
1. BSP traversal must remain explicit, bounded and non-recursive.
2. Large renderer workspaces must not casually live on the 9 KiB loopTask stack.
```

`PlaneWork` is a temporary heap lease. Do not increase `loopTask` merely to hide
renderer stack pressure.

`PlatformVideo_present()` is consistently about 34-35 ms and is not the first
performance target. Cold sprite/asset working-set rebuilds remain the larger
stutter source.

## Linker/include hygiene

A linker wrapper must not call a higher-level API that can indirectly re-enter
its wrapped symbol. Prefer `__real_*`, already-materialized views, or explicit
non-reentrant helpers.

Never shadow Arduino/ESP-IDF framework headers under `ESP32/include`.

## Current native gameplay correctness frontier

Hardware-owned behavior includes:

```text
movement / turn / strafe
native collision
event-first SELECT routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
regular door animation
mutable line texture variants
weapon pickup and native weapon rendering
attack frame presentation
adjacent extinguisher fire clear
jammed-door subtype-3 axe destruction
```

The jammed-door milestone is documented in:

[`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)

Final hardware proof on line 201 showed:

```text
axe target type=12 subtype=3
EV_OPENLINE event 72 / line 201
RNG consumed
open 0->1 committed
no DOORANIM after legacy-correct snap fix
player traversed 654->686 then 686->718
next opcode 11 state event committed successfully
```

The compact EntityDef metadata catalog now exposes `{tile,type,subtype}` using a
512 B bounded owner instead of the older 817 B type-only tile cache.

## Current next witness — dog combat

Immediately behind destroyed line 201 the CYD reports:

```text
tile = 750
sprite = 179
EntityDef type = 1
EntityDef subtype = 1
distance = 1
weapon = 0 (axe)
route = ENEMY_COMBAT_DEFERRED
```

Current stop is intentional:

```text
reason=native-monster-hp+attack-state-not-owned
mutation=no
```

After this branch merges, the preferred next bounded milestone is the first
native monster combat transaction against this exact dog corpus. Recover the
legacy contract first; then add compact mutable monster HP/state ownership and
exact RNG/rollback without reviving desktop entity graphs.

Still deferred separately:

```text
generic monster combat beyond the first bounded dog case
monster AI / retaliation / turn advance
player HP/armor/stat pickups
ammo pickup/consumption
XP application / level progression
sound
inventory/key ownership and CHECK_KEY
GIVEMAP
PASSWORD
SAVEGAME persistence
live CHANGEMAP/stats handoff
```

## CHANGEMAP recovery point

Entrance event 1 / tile 69 is already recovered but intentionally not live yet:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force this transition before enough native gameplay exists to complete
the map normally.

## Development workflow

```text
recover true main + docs
 -> choose one bounded milestone
 -> recover exact legacy behavior
 -> design a small permanent native owner/API
 -> keep unsupported cases fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

After a merge announcement, re-read actual GitHub `main`, record its exact SHA,
and create the next `agent/*` branch from that SHA. Never merge `main` without an
explicit user request.