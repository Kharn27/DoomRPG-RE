# ESP32 MAP_INTRO native gameplay interaction expansion

Branch: `agent/esp32-native-gameplay-force-message`

Base merged `main`:

```text
PR   = #104 — native dialog font hotpath
main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
```

Hardware-accepted implementation:

```text
0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

The tester explicitly accepted the normal `esp32-cyd` result at this SHA. No unreported serial fingerprint is invented in this archive; the exact firmware SHA above is the hardware boundary.

## Objective

Move the resident gameplay engine from isolated dialog/door witnesses toward a broader, bounded interaction engine without opening legacy `Game_executeEvent` wholesale.

The accepted candidate adds four permanent semantic families plus one boot-memory guard:

```text
FORCE_MESSAGE production status bar
NOTE + dialog prefix ownership
bounded post-dialog command chains
post-move weapon pickup ownership
systematic interaction/pickup corpus reporting
ESP32 mappings reload peak guard
```

The immutable map/runtime architecture remains unchanged.

## Production FORCE_MESSAGE

`EV_FORCEMESSAGE` (`opcode 24`) now has a resident gameplay owner rather than only an earlier probe-level semantic owner.

The route keeps the legacy fallback meaning:

```text
non-empty map string -> active status-bar fallback
empty map string     -> clear fallback
```

The native owner stores the compact `EspMapStringRef`, not a persistent copied map string. Painting is restricted to the logical 160x20 top bar and presentation remains owned by the enclosing gameplay frame.

Important bounds:

```text
text scratch capacity = 384 B
visible top-bar chars = 21
font = a.bmp, 9x12 glyphs
bar = k.bmp
map/world mutation = none
legacy Hud mutation = none
```

Command removal remains transactional and rollback-capable.

## NOTE prefix + dialog

`EV_NOTE` (`opcode 40`) can execute as a bounded prefix immediately before an owned `EV_DIALOG` / `EV_DIALOGNOBACK` interaction.

The notebook owner is lazy-gameplay: only a pointer exists during boot/prologue; the bounded state/scratch owner is allocated on the first real NOTE+dialog pair and reused afterward.

The dialog is not opened until the continuation has already passed bounded preflight. No post-dialog command executes before the dialog closes.

## Bounded saved dialog continuation

The previous production dialog route accepted only zero/one state opcode after close. This candidate replaces that artificial preflight limit with a bounded saved continuation engine.

Maximum eligible commands per continuation:

```text
12
```

Supported post-dialog command families:

```text
7  EV_SHOW
18 EV_HIDE
13 EV_UNLOCK
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Filtering uses one fixed event-state/run-flags snapshot for the resumed run, matching the recovered legacy `Game_runEvent()` behavior even when later state opcodes mutate event state.

Transactions cover:

```text
script removed bits
state-op mutations
line locked/texture mutation for UNLOCK
SHOW/HIDE sprite-topology storage/view
```

The topology snapshot is allocated lazily only when a SHOW/HIDE continuation actually needs it. The main transaction journal was also moved from fixed BSS to lazy gameplay allocation after the final hardware boot regression exposed the value of keeping post-boot owners out of startup RAM.

Any eligible unsupported opcode or chain overflow still fails closed before opening the dialog.

## Real NPC continuation that motivated the boundary

Entrance event 60 contains a real mixed dialog continuation. The important recovered structure is:

```text
state 0:
  DIALOG
  SHOW
  CHANGESTATE
  NEXTSTATE

later state:
  DIALOGNOBACK
  HIDE
  CHANGESTATE
  UNLOCK
  NEXTSTATE
```

The old production preflight incorrectly rejected the event when it saw SHOW after DIALOG. The accepted engine now models the actual legacy pause boundary: dialog opens, script continuation is saved, then the bounded chain executes only after close.

## Generic weapon pickup boundary

Post-committed-move touch handling now owns the legacy `eType=5` weapon family with compact native state.

Permanent behavior:

```text
one consumed bit per resident map sprite
consumed sprite hidden through native map-sprite view
uint16 weapon ownership mask
new weapon auto-select overlay
HUD weapon view overlay
world rerender after pickup
exact rollback if rerender fails
owner scoped to runtime arena FNV/map
```

The owner is lazy and allocated after resident gameplay exists; it is not a boot-time map-wide entity structure.

The current boundary intentionally stops before full `Entity_touched()` parity:

```text
weapon world remove = owned
weapon ownership = owned
new weapon selection = owned
ammo increment from EntityDef.parm = DEFERRED
first-acquisition popup/dialog = DEFERRED
sound = DEFERRED
```

Other pickup entity families remain reported but not executed:

```text
eType 3  world/player-stat item = DEFERRED
eType 4  inventory item         = DEFERRED
eType 6  ammo                   = DEFERRED
eType 16 alternate ammo         = DEFERRED
```

## First-person weapon rendering

The candidate also contains the native idle first-person weapon renderer used by the resident gameplay frame.

It preserves the recovered logical sprite rule:

```text
logical weapon sprite = 240 + weapon
```

and uses bounded PAK reads plus a fixed/lazy workspace rather than `shapeData` or map-wide `mediaTexels`.

## Systematic interaction inventory

The resident session emits read-only one-shot inventories so unsupported gameplay is visible before the tester physically reaches each object/event.

The inventory layer reports:

```text
resident opcode corpus
per-event unsupported opcode IDs/reasons
pickup entity-family counts
owned versus deferred routes
```

This is diagnostics only: it allocates no semantic authority and does not broaden execution.

For Entrance, the known opcode ID set remains:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Still production-deferred/fail-closed in this candidate:

```text
2  EV_CHANGEMAP  -> transition consumer not yet promoted here
9  EV_GIVEMAP    -> automap production route not yet promoted here
10 EV_PASSWORD   -> password input UI missing
27 EV_SAVEGAME   -> save-route consumer missing
41 EV_CHECK_KEY  -> native player key owner missing
```

Existing probe/intent APIs for these commands are not permission to execute them in live gameplay.

## ESP32 mappings reload peak guard

The final hardware test initially exposed a startup regression at `Render_beginLoadMap(MAP_MENU)`:

```text
[MAPPINGS] Render_loadMappings result=1
...
[MAPSTRUCT] -> Render_beginLoadMap(MAP_MENU)
---Render_loadMappings---
out of memory allocating inflate state for mappings.bin
```

The important measured pre-failure boundary supplied by hardware was:

```text
heap8    = 34036 B
largest8 = 9716 B
mapping payload = 8376 B
```

Legacy `Render_beginLoadMap()` always calls `Render_loadMappings()`. That loader opens/inflates `mappings.bin` before freeing the previous four mapping arrays, creating avoidable double residency on the no-PSRAM CYD.

The ESP32-only guard now releases exactly those four immutable mapping arrays immediately before the real `Render_beginLoadMap()` call:

```text
mediaTexelOffsets
mediaBitShapeOffsets
mediaTexturesIds
mediaSpriteIds
```

The real legacy `Render_loadMappings()` still performs the parse/rebuild and remains sole owner of the rebuilt data. No file format or map parser semantics were changed.

The same closeout also moved the dialog-chain transaction journal to lazy-gameplay allocation, further enforcing the rule that post-playing semantic owners must not consume boot/menu margin.

The tester accepted the corrected normal firmware at SHA `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc`.

## Permanent invariants preserved

```text
board = ESP32-2432S028R classic CYD
PSRAM = none
framebuffer = 160x120 RGB565 = 38400 B
/DoomRPG-ESP32.pak = native backing store
shapeData == NULL
mediaTexels == NULL
legacy Game.entities = 0
legacy Game.monsters = 0
unsupported semantic families fail closed
```

This milestone does not authorize runtime ZIP fallback or map-wide decompression/decoded graphics ownership.

## Accepted gameplay boundary after this milestone

Production now includes, in addition to the previously merged resident movement/doors/dialog UI:

```text
EV_FORCEMESSAGE top-bar fallback
EV_NOTE bounded prefix before dialog
EV_DIALOG / EV_DIALOGNOBACK with bounded saved continuation
post-dialog SHOW/HIDE/UNLOCK + 11/19/20 state ops
native idle first-person weapon painting
eType=5 weapon remove/ownership/auto-select overlay
interaction and pickup corpus diagnostics
bounded ESP32 mappings reload peak recovery
```

Still not equivalent to full Doom RPG gameplay:

```text
ammo/inventory/player-stat pickup families
weapon acquisition popup and sound
combat/monsters/turn advance
password input
player key owner / CHECK_KEY
save consumer
map transition consumer
broad arbitrary event execution
```

## Merge boundary

```text
base main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
hardware-tested implementation SHA = 0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
REAL-CYD HARDWARE PASS = YES
normal env = esp32-cyd
shapeData/mediaTexels = NULL
legacy entities/monsters = 0
unsupported routes fail closed = YES
MERGE-READY = YES
```

Every commit after `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc` must remain documentation-only for this closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
