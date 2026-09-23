# Milestone — Native facing-entity top-bar label

## Scope

This milestone restores the original Doom RPG behavior where the top HUD shows
the name of the nearby entity the player is currently facing.

The implementation remains ESP32-native and pointer-free. It does not retain
legacy `Entity_t` ownership and does not add a resident table of entity names.

Hardware-tested code head:

```text
ec5207f3b18d7d2d89d6f569cf7aeb25da35270f
```

Base `main`:

```text
120449ff3aa02b055d0ead75f9c2c55e799a5851
```

Normal `esp32-cyd` CI run #617 passed with:

```text
RAM:   44984 B / 327680 B
Flash: 748213 B / 1310720 B
```

Compared with the previous 44,944-byte static-RAM boundary, the permanent cost
is only 40 bytes.

## Recovered legacy behavior

Legacy `DoomCanvas_checkFacingEntity()` performs a short forward trace after a
settled movement/rotation and updates `player->facingEntity`.

`Hud_drawTopBar()` uses that entity name only as a low-priority fallback:

```text
timed HUD message
 > statBarMessage
 > logMessage
 > facingEntity->def->name (while playing, eType != 9)
 > empty
```

The forward trace is intentionally short rather than a room-wide targeting
system. Native recovery preserves the original geometry:

```text
trace origin = player center shifted 31 units forward
trace reach  = 3 tile steps
entity order = current linked order
line entity  = wins shared-tile head ordering like legacy Game_linkEntity()
type 9       = may stop the trace but is not shown as a HUD label
```

This gives the original near-field feel: labels appear only when an eligible
object is sufficiently close and in the current facing trace.

## Native implementation

New compact owner:

```text
EspNativeGameplayFacingLabelView = 34 B
```

It retains only the currently derived target:

```text
name[17]
spriteIndex / lineIndex
definition tile
map tile
type / subtype
trace distance
sprite-vs-line identity
active/displayable/dirty flags
```

No `Entity_t*`, no desktop object graph and no map-wide name cache are
introduced.

Names are resolved on demand through the existing compact entity-definition
catalog:

```text
EspEntityDefTypeCatalog_readName()
 -> /entities.db
 -> read only the historical 16-byte EntityDef name for the current target
 -> close PAK
```

Crate transforms reuse the effective native definition tile so transformed
world objects continue to resolve the correct semantic definition.

The facing owner refreshes before settled world rendering, including:

- session arm;
- MOVE;
- TURN;
- HUB close world restoration;
- world-changing action commit/rollback.

A failed facing-label refresh is presentation-only and does not invalidate an
otherwise healthy gameplay frame.

## Top-bar composition

The existing action-feedback presentation leaf remains the physical top-bar
compositor.

When no timed action feedback owns the top bar, it now restores:

```text
active statBarMessage
 -> else current facing label
 -> else empty topbar
```

This preserves the recovered priority instead of implementing facing names as
another timer or queued feedback message.

Top touch locator notches are repainted by the same top-bar paths and remain
preserved.

## REAL-CYD PASS

The hardware run validated sprite, line and empty-target transitions.

### Sprite target

After closing the HUB, the current target remained stable and was repainted:

```text
[FACINGLABEL] REFRESH reason=HUB-CLOSE active=1 display=1 source=sprite
              index=19 tile=801 distance=1 type=2 subtype=0 def=150
              name="Civilian" dirty=0 ownerBytes=34 traceSteps=3
[FACINGLABEL] PAINT name="Civilian" chars=8 source=sprite index=19
              priority=fallback ...
```

This also proves HUB ownership does not destroy the derived world-facing label.

### Line entity

A strafe resolved a native line entity and displayed its definition name:

```text
[FACINGLABEL] REFRESH reason=MOVE active=1 display=1 source=line
              index=279 tile=832 distance=2 type=7 subtype=5 def=359
              name="Computer" dirty=1 ownerBytes=34 traceSteps=3
[FACINGLABEL] PAINT name="Computer" chars=8 source=line index=279
              priority=fallback ...
```

The next forward movement kept the same line target at a closer trace distance:

```text
name="Computer" distance=1
```

### Rotation and target change

A pure rotation changed the target from the computer to a sprite without
advancing the monster turn:

```text
[FACINGLABEL] REFRESH reason=TURN ... source=sprite index=19
              distance=1 type=2 subtype=0 def=150 name="Civilian" dirty=1
...
[RESIDENTGAMEPLAY] TURN ... angle=128->64 committed=yes
[MONSTERTURN] ROTATE-NO-TURN ... legacyAdvance=no
```

### Empty trace

Turning away cleared the label exactly:

```text
[FACINGLABEL] REFRESH reason=TURN active=0 display=0 source=none
              index=65535 tile=65535 ... name="" dirty=1
[FACINGLABEL] CLEAR source=none priority=fallback ...
```

### Door line

The same hardware run also resolved a door definition through the line path:

```text
[FACINGLABEL] REFRESH reason=MOVE active=1 display=1 source=line
              index=275 tile=837 distance=3 type=0 subtype=0 def=305
              name="Door" dirty=1 ownerBytes=34 traceSteps=3
[FACINGLABEL] PAINT name="Door" chars=4 source=line index=275
              priority=fallback ...
```

This proves the bounded trace is not limited to sprite entities.

## Runtime invariants retained

The tested gameplay run continued normally across HUB open/close, strafing,
forward movement and rotations. The supplied serial witness remained alive with:

```text
heap=81936
heap8=16384
largest8=11764
```

The permanent architecture is unchanged:

```text
shapeData == NULL
mediaTexels == NULL
/DoomRPG-ESP32.pak remains the native backing store
no runtime ZIP fallback introduced by this milestone
no legacy Entity_t ownership introduced
```

A legacy compact-render guard was also encountered later in the run and
recovered through the already-existing renderer recovery path; it is unrelated
to the facing-label owner.

## Result

**Native facing-entity top-bar label: REAL-CYD PASS.**

The recovered behavior now covers nearby sprite entities, line entities,
rotation-only target changes, empty-trace clearing and HUB-close restoration,
with only a 34-byte semantic owner and a 40-byte increase in static RAM at the
firmware boundary.
