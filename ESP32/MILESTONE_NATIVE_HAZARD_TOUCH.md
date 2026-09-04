# Milestone — Native movement hazard touch

## Git / hardware boundary

```text
base main = a74a6f067cc7a5304a1f940f78317804659c37e8
branch = agent/esp32-native-hazard-touch
hardware-tested code boundary = bae9d1a78f40db31c35a8a0aa9a1875692cf5c9e
status = REAL-CYD PASS
branch policy = LOCKED; docs-only tail only
```

`bae9d1a7...` is the exact code boundary exercised on the real classic CYD.
Commits after this SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33845214033` also completed successfully
for this exact SHA. CI is only the compile/link gate; the real-CYD serial witness
below is the runtime authority.

## Goal

Recover the movement-side `Game_touchTile(..., true)` hazard subset without
reintroducing legacy `Entity_t` ownership. The permanent path remains compact and
reuses the existing shared player and feedback owners:

```text
committed player move
 -> scan linked entities on destination tile
 -> type 10 / type 11 hazard classifier
 -> shared 52 B PlayerState pain transaction
 -> bounded dynamic damage text
 -> bounded viewport-border flash
 -> full native redraw/present
 -> existing monster-turn continuation
```

This milestone adds no new heap/BSS gameplay owner for hazard state.

## Owned hazard behavior — hardware PASS

The native executor currently owns linked movement hazards with the recovered
legacy pain amounts:

```text
type 10: pain(1, 2)
type 11: pain(10, 10)
```

The shared pain rule first spends the requested armor damage, then adds any armor
shortfall to HP damage. PlayerState mutation is transactional and redraw failure
rolls the player snapshot back.

The real CYD crossed two separate type-10 flames in Entrance. First witness:

```text
[HAZARD] COMMIT tile=613 sprite=74 type=10 hazards=1
  rawDamage=1+2
  hp=30->29 armor=8->6
  playerFNV=a6e115a7->5055206e
  message="3 damage!"
  flash=red-bb0000/500ms
  rollback=closed
```

Second independent witness:

```text
[HAZARD] COMMIT tile=616 sprite=110 type=10 hazards=1
  rawDamage=1+2
  hp=30->29 armor=6->4
  playerFNV=9bac3791->61238584
  message="3 damage!"
  flash=red-bb0000/500ms
  rollback=closed
```

The user physically observed both the red border and the damage result.

## Damage presentation — hardware PASS

Hazard feedback reuses the existing bounded action-feedback owner. No separate
hazard UI owner was introduced.

```text
top-bar text = dynamic, bounded to existing 24 B buffer
text example = "3 damage!"
text duration = 1200 ms
flash color = recovered legacy red 0xBB0000 -> RGB565 b800
flash viewport = 0,20,160,80
flash thickness = 2 px
flash pixels = 944
flash duration = 500 ms
snapshot = bounded static 1888 B border storage
```

Representative real-CYD sequence:

```text
[ACTIONFEEDBACK] PAINT kind=6 text="3 damage!" ... durationMs=1200
[VIEWFLASH] PAINT color565=b800 viewport=0,20,160,80 thickness=2 pixels=944 durationMs=500 ...
[HAZARD] FRAME reason=HAZARD-TOUCH ... presented=1
[HAZARD] COMMIT ... rollback=closed
[VIEWFLASH] EXPIRE elapsedMs=513 targetMs=500 color565=b800 restored=viewport-border-only
[ACTIONFEEDBACK] EXPIRE kind=6 elapsedMs=1205 targetMs=1200 restored=topbar-only
```

A second flame independently expired the red flash at 523 ms and the damage text
at 1214 ms, confirming the bounded lease repeatedly recovers cleanly.

## Turn ordering and regression chain

Hazard processing occurs after the committed movement frame and before the
existing monster-turn schedule. The real-CYD witness proves the movement turn was
not swallowed by the new touch family:

```text
[RESIDENTGAMEPLAY] MOVE ... tile=612->613 ... committed=yes
[HAZARD] COMMIT tile=613 ...
[MONSTERTURN] SCHEDULE n=20 reason=MOVE ... playerFNV=5055206e ...
```

The same session then continued through door animation, a Health Vial pickup and
a second flame. The Health Vial raised HP `29->30`, used the existing pickup
message/white-flash route, and the following flame again applied the expected
hazard damage. This is a useful cross-family regression witness for move events,
hazard touch, PlayerState, resources, feedback and monster-turn scheduling.

## RAM witness

Representative real-CYD `ALIVE` lines during and after the hazard tests:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

No RAM regression was observed across repeated flame feedback and later pickup
feedback. The permanent project invariants remain `shapeData == NULL`,
`mediaTexels == NULL` and no PSRAM; this excerpt does not independently reprint
those pointer values.

## Explicit fail-closed / deferred boundaries

This milestone intentionally does not broaden into the complete legacy
`Entity_touched()` family:

```text
mixed resource + hazard tile ordering = fail closed
familiar weapon redirection for slots 9..11 with dog ammo = fail closed
lethal player transition = fail closed
PASS_TURN current-tile type10/11 touch = still fail closed
secondary burn text "It burns!" / "It really burns!!" = deferred
pain face / shake / sound = deferred
```

Monster retaliation damage presentation is also still a separate family. A
real-CYD Hellhound PASS_TURN witness on this exact firmware commits player damage
but logs:

```text
[MONSTERRETAL] COMMIT ... painFX=deferred damageText=deferred sound=deferred ...
```

The player therefore correctly observed no red border and no HP-loss text for a
monster hit. That is not claimed by this hazard milestone and should be recovered
as its own bounded monster-attack/player-pain presentation milestone.

## Merge rule

The code boundary is locked at
`bae9d1a78f40db31c35a8a0aa9a1875692cf5c9e`. Only documentation may follow on
this branch. After merge, recover the exact new GitHub `main` SHA before creating
the next `agent/*` branch.
