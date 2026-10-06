# ESP32 documentation map

## Legacy Hud object retired — REAL-CYD PASS (2026-10-06)

Hardware-tested code boundary:
`8d0cd7443d2524829128f6ee26b1c19244bd6dbd`.

Branch:
`agent/esp32-retire-legacy-combat`.

The remaining inherited `Hud_t` compatibility object is now retired from the
normal classic-CYD runtime. `doomRpg->hud` stays `NULL` by construction;
visible HUD composition, top-bar/status feedback, dialogs and view flashes are
owned by the native gameplay UI path. A final stale Hud dependency in the
native main-menu model was also removed before the tested boundary.

The real CYD validates MENU_MAIN -> START, full intro/disposal, exact Entrance
first frame `71ca7465`, resident gameplay, chained dialogs including opcode
19/26, automap open/close with HUD repaint, doors, fire-clear, note/dialog
resume, pickups and weapon-help, secret feedback, active monster turns,
committed monster retaliation, live monster movement, player kill/gib/drop and
dynamic drop pickup.

Representative combat witnesses:

```text
[MONSTERRETAL] COMMIT ... playerHP=30->27 armor=8->5 ... message="6 damage!" ...
[MONSTERRETAL] COMMIT ... playerHP=27->24 armor=5->2 ... message="6 damage!" ...
[MONSTERCOMBAT] COMMIT seq=102 ... hp=5->0 ... alive=1->0 ... xp=5-applied ...
[MONSTERDROP] COMMIT ... type=3 subtype=21 def=92 tile=658 ...
[PLAYERRES] COMMIT tile=658 ... armor=6/20 ... rollback=closed
```

Stable post-lazy-allocation ALIVE:

```text
heap=123612 heap8=57688 largest8=51188
```

The normal local PlatformIO build reports 45464 B static RAM and 781017 B
linked Flash. No completed GitHub Actions result is attached to this exact
hardware-tested head, so no CI result is claimed for it.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_HUD_OBJECT.md](MILESTONE_ESP32_RETIRE_LEGACY_HUD_OBJECT.md)

## Desktop Hud translation unit retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`9efa082d1aacbc137e1e7fcd726f857c0dbae2d6`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal `esp32-cyd` build no longer compiles `src/Hud.c`. A bounded
ESP32 compatibility shim temporarily preserves only the scalar/message ABI
still referenced by inherited helpers. All legacy HUD raster functions in that
shim are no-ops; visible HUD composition is owned by
`EspNativeGameplayHud` and its PAK-backed renderer.

Real-CYD validation covers cold boot, MENU_MAIN, START, the complete bounded
intro, resident MAP_INTRO load, hub inventory/weapons/status pages, PASS_TURN,
automap open/close, native dialogs, standalone weapon-help dialog, player
movement/pickups, monster activation, monster hit/miss retaliation, player
attack/kill, drop materialization and top-bar feedback.

Representative evidence:

```text
[HUDCOMPAT] INIT bytes=600 images=NULL renderer=native-pak-stream
[LAYOUT] HUD legacy bitmaps status=0x0 large=0x0 faces=0x0 icons=0x0 attack=0x0 arrow=0x0 owner=native-pak-stream
...
[GAMEPLAYHUD] REPAINT ... ownerMutation=no ...
...
[MONSTERRETAL] COMMIT ... message="5 damage!" ...
[MONSTERRETAL] MISS-COMMIT ... message="Dodged!" ...
[MONSTERCOMBAT] COMMIT ... alive=1->0 ...
```

The stable resident witness remains:

```text
heap=127880 heap8=61956 largest8=51188
```

Normal `esp32-cyd` CI #1584 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 781385 B
artifact id  = 11303743052
artifact sha256 = d90cb7dc7e4f18db92c0d3317be493ab2ea85ba6af6920d61da913693148886a
```

This is 1384 B less linked Flash than the preceding HUD-bitmap build.
No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_DESKTOP_HUD_TU.md](MILESTONE_ESP32_RETIRE_DESKTOP_HUD_TU.md)

## Legacy HUD bitmap residency retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`96813c8333ec07dcfc7343517e6c69764b70f082`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal ESP32 layout no longer calls legacy `Hud_startup()` to make six
desktop HUD bitmaps resident. The compact `Hud_t` compatibility object remains
temporarily for scalar geometry/message fields, while the production
`EspNativeGameplayHud` continues to read HUD assets directly from the native
PAK as bounded on-demand data.

Retired resident images:

```text
imgStatusBar
imgStatusBarLarge
imgHudFaces
imgIconSheet
imgAttArrow
imgStatusArrow
```

Real-CYD gameplay remains healthy through PASS_TURN, movement, automap-derived
redraws, repeated armor pickups, collision, regular door open/close and
top-bar/action feedback. A stable resident witness is:

```text
[ALIVE] ... heap=127880 heap8=61956 largest8=51188 ...
```

The previous legal-strip run at a comparable resident boundary reported
`heap=123632 / heap8=57708`. The observed +4248 B is consistent with removing
the six packed HUD image allocations plus allocator/image metadata overhead.

Normal `esp32-cyd` CI #1582 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782769 B
artifact id  = 11304071480
artifact sha256 = ca85db10f8a0595a743f68c27ea02f5ccc8b30bd165ea03ffbd58091a90b243e
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_HUD_BITMAPS.md](MILESTONE_ESP32_RETIRE_LEGACY_HUD_BITMAPS.md)

## Legacy legal-strip residency retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`35c310026484f095448ef926b2bba944c8dbb359`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal classic-CYD boot no longer allocates the inherited `g.bmp`
128x512 legal-screen strip. ESP32 production boot already bypasses
`ST_LEGALS` and paints the native opaque MENU_MAIN dashboard directly, so
retaining 32768 packed pixel bytes until START was dead residency.

Real-CYD evidence:

```text
[LAYOUT] heap8 used=21828 remaining=117932 largest=73716
...
[MAINOPAQUE] ... heap8=98772 largest8=73716
[ALIVE] ... heap=164696 heap8=98772 largest8=73716 ...
...
[MAINMENU] Runtime cleanup legals=already-free heap8=98772->107212 gained=8440 ...
```

The previous Sound-retirement menu witness was `heap8=65888`; the new menu
witness is `98772`, a +32884 B increase consistent with avoiding the 32768 B
packed legal strip plus allocator effects. START, intro disposal, MAP_INTRO
load, native HUD, resident gameplay, pickups and doors remain functional.

Normal `esp32-cyd` CI #1580 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782817 B
artifact id  = 11302983372
artifact sha256 = 5b53223c103def2db06d252853751e3d512f7e41c3fb00359eb245fee515d25e
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_LEGALS_STRIP.md](MILESTONE_ESP32_RETIRE_LEGACY_LEGALS_STRIP.md)

## Legacy Sound object retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`fffe6f6d780ae1d7444c49cb08df747fe5f4ca0c`.

Branch:
`agent/esp32-retire-legacy-combat`.

The classic-CYD runtime no longer constructs the inherited `Sound_t`.
Audio playback remains intentionally deferred; gameplay/menu code publishes
bounded `EspNativeAudioIntent` records while compatibility
`Sound_playSound/stopSounds/freeSounds` calls are NULL-safe no-ops.

The ESP32 generated sources also retire every remaining direct field dereference
that would require a resident Sound object:

```text
Game_loadConfig()      -> legacy volume field consumed but not stored in Sound_t
DoomCanvas_castState() -> legacy soundEnabled branch fixed to playback-disabled behavior
DoomCanvas_run()       -> legacy nextplay reset removed
DoomRPG_free()         -> Sound_free ownership removed
```

Real-CYD boot proves the object is absent:

```text
[CORE] Sound retired object=NULL owner=native-audio-intent playback=deferred
[CORE] READY objects=7 heap used=47460 remaining=139760 largest=73716 clip=160x120
```

START, the full bounded intro, resident MAP_INTRO construction, first frame,
HUD, caches and resident gameplay all complete successfully with
`doomRpg->sound == NULL`. The session reaches:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=123640 heap8=57716 largest8=51188 ... CORE=ready ...
```

The previous Combat-retirement witness at the same broad gameplay stage was
`heap=123436`; the observed +204 B is consistent with removing the ~220 B
Sound allocation, subject to normal allocator/layout variation.

Normal `esp32-cyd` CI #1578 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782837 B
artifact id  = 11302676326
artifact sha256 = 502243e5bdd8abc9c9b2764428939098440619e44e5f7b3871442925028501e9
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_SOUND.md](MILESTONE_ESP32_RETIRE_LEGACY_SOUND.md)

## Legacy Combat object retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`766d1e0cf2280d784f2c073f552e3f57889dc541`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal ESP32 runtime no longer constructs the inherited `Combat_t`.
`Combat.c` and the now-unreferenced desktop `Weapon.c` are excluded from
the `esp32-cyd` build, `doomRpg->combat` is required to remain `NULL`,
and the remaining production responsibilities are owned by the native combat
math, monster combat/retaliation, monster-turn sequencer and native first-person
weapon renderer.

The pre-retirement ELF retained only three desktop Combat roots:
`Combat_init`, `Combat_free` and `Combat_drawWeapon`. The native weapon
renderer already owns the legacy idle/attack offsets and bounded PAK-backed
decode/cache path, so the ESP32-generated `DoomCanvas.c` no longer calls the
legacy weapon draw. Generated `DoomRPG.c` likewise no longer retains
`Combat_free`.

The first hardware attempt exposed one real cleanup dependency rather than a
combat dependency. START crashed with `StoreProhibited`,
`EXCVADDR=0x00000004`; symbolization placed the fault in
`Game_unloadMapData()` during `DoomRPG_esp32ReleaseMainMenuMemory()`.
That cleanup still wrote `combat->curTarget/curAttacker = NULL`. The final
ESP32-generated `Game.c` retires only those obsolete writes while preserving
the rest of menu/map teardown. No `Combat_t` allocation or stub was restored.

Real-CYD acceptance on the final boundary proves START reaches resident gameplay
and a native player attack remains functional:

```text
[ACTIONENGINE] TRACE seq=3 weapon=2 distance=1 tile=873 target=sprite index=127 ... route=CRATE_SUBTYPE2
[CRATE] CONSEQUENCE seq=3 ... outcome=TRANSFORM ... attackDamage=6 attackArmorDamage=4 ...
[MONSTERTURN] ATTACK-REQUEST seq=3 source=explicit-native-player-attack ...
[CRATE] COMMIT seq=3 ... ammo=8->7 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[ACTIONENGINE] ATTACK seq=3 weapon=2 frame=1->0 generic=yes worldCommitted=yes
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=2 activeCount=0 ...
```

This covers the native weapon attack pose, ammo mutation, generic combat math,
crate consequence RNG/world mutation and semantic turn publication with
`doomRpg->combat == NULL`.

The same run remains stable in resident gameplay:

```text
[ALIVE] ... heap=123436 heap8=57512 largest8=51188 ... CORE=ready ... MENU=ready
```

A previous comparable early resident-gameplay witness before Combat retirement
was `heap=122376`; the observed +1060 B is consistent with removing the
1036-byte `Combat_t` allocation plus allocator overhead. Treat this as a
hardware consistency witness, not an exact allocator accounting proof.

Normal `esp32-cyd` CI #1575 is SUCCESS:

```text
static RAM   = 45480 B
linked Flash = 782773 B
artifact id  = 11299309172
artifact sha256 = f0d066fdfa7535ae875e1a9ff06269656103ff5b3952ae6a05ee85378a1d18ab
```

Versus merged main before the milestone: static RAM is 8 B lower and linked
Flash is 1800 B lower. More importantly, the 1036-byte desktop Combat object is
no longer allocated at runtime.

The final ELF contains no `Combat_*` or desktop `Weapon_*` symbols.
`CombatEntity_*` remains intentionally linked because `Player_t` still uses
that small stats container; retiring it belongs to a later Player cleanup.

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_COMBAT.md](MILESTONE_ESP32_RETIRE_LEGACY_COMBAT.md)

## Legacy EntityDef manager retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`9fa46cf86a1e8b4bbb25e20d80cfbf9664ef22db`.

Branch:
`agent/esp32-retire-legacy-entitydef`.

The ESP32 normal runtime no longer allocates or starts the inherited
`EntityDefManager_t`. `src/EntityDef.c` is excluded from the normal
`esp32-cyd` build, `doomRpg->entityDef` is required to remain `NULL`, and
resident maps resolve all entity metadata through the compact immutable
`EspEntityDefTypeCatalog` built from `/entities.db` in the native PAK.

This removes the duplicated desktop representation: the retired manager owned a
heap table of full 24-byte `EntityDef_t` records including persistent 16-byte
names, while the native catalog retains only compact
`{tileIndex,type,subtype,parm}` metadata and reads names from the PAK on
demand.

The final ELF/build graph contains no compiled `EntityDef.c` object and no
`EntityDef_init/startup/find/lookup/free` symbol. The native
`EspEntityDefTypeCatalog_*` owner remains linked.

The real CYD validates the consumers that matter rather than only startup:

```text
[PLAYERRES] PREPARE ... defTile=92 type=3 subtype=21 parm=4 action=armor ...
[PLAYERRES] FEEDBACK ... message="Got Armor Shard" sourceDefTile=92 ...
[PLAYERRES] PREPARE ... defTile=1 type=5 subtype=0 parm=0 action=weapon ...
[PLAYERRES] FEEDBACK ... message="Got Axe" sourceDefTile=1 ...
```

Those paths require catalog metadata and on-demand definition names. Combat then
materializes a dynamic monster drop by native type/subtype lookup, and the same
catalog validates and consumes it:

```text
[MONSTERDROP] COMMIT ... type=3 subtype=20 def=91 tile=658 ... persistence=map-session-live/save-deferred
[PLAYERRES] PREPARE tile=658 sprite=65535 defTile=91 type=3 subtype=20 parm=4 action=health ...
[PLAYERRES] FEEDBACK tile=658 message="Got Health Vial" sourceDefTile=91 ...
```

The same run also exercises doors, dialogs, secret activation, ordered monster
combat and post-kill movement without any legacy EntityDef owner.

Observed stable ALIVE samples include:

```text
heap=122376 heap8=56452 largest8=51188
heap=118916 heap8=52992 largest8=49140
```

The first figure is before later lazy gameplay allocations; the second is after
dialog/combat resources have become resident. They are runtime witnesses, not a
claim that every byte difference versus an older build belongs to EntityDef.

Normal `esp32-cyd` CI #1568 is SUCCESS:

```text
static RAM   = 45488 B
linked Flash = 784573 B
artifact id  = 11280285689
artifact sha256 = 3bff6f3b27453ac42a988f918eac3d0d83c1adc6813e091f54b131d855d38785
```

Static RAM is 8 B lower than the merged pre-milestone boundary. The larger
memory win is runtime heap: the desktop manager object and its separately
allocated full definition table are no longer constructed.

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_ENTITYDEF.md](MILESTONE_ESP32_RETIRE_LEGACY_ENTITYDEF.md)

## Native extinguisher turn parity — REAL-CYD PASS (2026-10-03)

Hardware-tested code boundary:
`a36edddd5b74b1737991f8fcfecb0ac43da727b8`.

Branch:
`agent/esp32-player-action-turn-parity`.

Legacy `Player_fireWeapon()` routes a successful player-owned utility attack
through `ST_COMBAT`; when that player combat finishes,
`DoomCanvas_combatState()` calls `Game_advanceTurn()` for
`curAttacker == NULL`. The native extinguisher path previously committed
`FIRE_CLEARED` with `turnAdvance=deferred`, allowing gameplay to continue
without explicitly publishing the player turn.

The native fire-clear commit now requests the already-proven
`PLAYER_ATTACK` monster-turn producer after the world/player transaction is
prepared. If the transactional world render fails, that pending turn is cancelled
before the fire removal and PlayerState/ammo rollback are restored. No new turn
owner, RNG path, heap allocation or legacy entity ownership is introduced.

The scope was deliberately reduced after real-CYD feedback: jammed/destructible
doors already behaved correctly on hardware, including a monster behind the
destroyed door taking its turn. That route was therefore left unchanged rather
than risking a duplicate semantic advance.

Hardware acceptance was initially functional gameplay acceptance. A later
real-CYD run on the merged code supplied a direct serial witness for the
fire-clear turn producer and dispatch:

```text
[MONSTERTURN] ATTACK-REQUEST seq=64 source=explicit-native-player-attack rollback=available-until-cancel
[ACTIONENGINE] FIRE-COMMIT seq=64 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=19 activeCount=0 ...
[MONSTERACTIVESEQ] BEGIN turn=19 reason=3 activeCount=0 ...
```

A second fire in the same session repeats the exact sequence at seq=68 /
turnToken=23. The witness proves the successful extinguisher action now consumes
the semantic player turn and dispatches `PLAYER_ATTACK`. Both observed fires
had `activeCount=0`, so this still does not claim a captured “fire plus
already-active enemy attacks immediately” scenario.

Normal `esp32-cyd` CI #1563 is SUCCESS:

```text
static RAM   = 45496 B
linked Flash = 784877 B
artifact id  = 11268746232
artifact sha256 = a5ba9d656fcd0c468a7c4b489abdc4f6314b900301d41281b45f379fc3d53ed2
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_EXTINGUISHER_TURN_PARITY.md](MILESTONE_ESP32_NATIVE_EXTINGUISHER_TURN_PARITY.md)

## Native door / monster-turn parity — REAL-CYD PASS (2026-10-03)

Hardware-tested code boundary:
`69ee31a17f2e4900c825cc73ba1fee9400fc8ac5`.

Branch:
`agent/esp32-monster-drop-checkpoint-v11`.

This closes the remaining native door-turn parity gap against the legacy
`DoomCanvas SELECT -> Game_executeTile -> Game_advanceTurn` behavior.

Three bounded corrections now compose without reviving legacy world ownership:

- a monster standing on a door line that closes may leave its source tile through
  the recovered legacy type-0 source-axis exception; destination collision still
  sees the closed line;
- MOVE-triggered door closure commits `EspMapLineState=closed` immediately but
  holds the regular-door visual open through the monster turn, then animates the
  close after the ordered sequence completes;
- a successful ordinary SELECT door event explicitly schedules
  `SELECT_DOOR`, so monsters revealed by the opening render receive the same
  semantic turn as legacy. CHANGEMAP transition doors remain transition-owned.

The real CYD proves an ordinary opened door with no active monsters is harmless:

```text
[ACTION] DOOR-BATCH event=65 count=1 status=OK ... open=0->1 ...
[MONSTERTURN] DOOR-REQUEST seq=83 ... legacyAdvance=yes ...
[RESIDENTGAMEPLAY] SELECT ... turnAdvance=SELECT_DOOR-requested
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=25 activeCount=0 ...
```

It then opens a two-line secret door whose render activates two monsters. The
same SELECT immediately owns their ordered turn: sprite 220 attacks, the
sequencer waits for retaliation to resolve, then sprite 264 moves:

```text
[MONSTERACT] ACTIVE sprite=220 ... activeCount=1 activationOrder=0 ...
[MONSTERACT] ACTIVE sprite=264 ... activeCount=2 activationOrder=1 ...
[MONSTERTURN] DOOR-REQUEST seq=92 ...
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=31 activeCount=2 ...
[MONSTERTURN] MEMBER-ATTACK-PROBE reason=SELECT_DOOR sprite=220 ...
[MONSTERACTIVESEQ] PAUSE ... probe=1 reason=attack-in-flight ...
[MONSTERRETAL] COMMIT probe=1 ... playerHP=30->27 armor=8->6 ...
[MONSTERACTIVESEQ] RESUME ... nextOrdinal=2/2 ...
[MONSTERMOVELIVE] COMMIT ... sprite=264 tile=694->693 ...
[MONSTERACTIVESEQ] COMPLETE turn=31 reason=5 activeCount=2 delivered=2 ...
```

The same hardware session also proves the new close-presentation ordering keeps
gameplay collision closed while visual closure waits for the monster turn:

```text
[DOORANIM] HOLD line=234 logical=closed visual=open ... collision=closed-now
[DOORANIM] HELD-FRAME ... logical=closed visual=open ...
[MONSTERTURN] ORDERED-DISPATCH reason=MOVE turnToken=28 ...
[DOORANIM] RELEASE deferredClose=1 phase=after-monster-turn logical=closed ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[DOORANIM] POST-MONSTER-COMPLETE ... logical=closed visual=closed
```

A preceding real-CYD run had already confirmed the recovered source-line escape
prevents a monster caught by a closing door from remaining permanently trapped.
The visual hold was then added so that escape/movement occurs before the player
sees the door close.

Normal `esp32-cyd` CI #1556 is SUCCESS:

```text
static RAM   = 45496 B
linked Flash = 784621 B
artifact id  = 11266949372
artifact sha256 = 59d5e1ea7848495f6a936e8c03364f545b8129b116be696160d48fe48302c85c
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_DOOR_MONSTER_TURN_PARITY.md](MILESTONE_ESP32_NATIVE_DOOR_MONSTER_TURN_PARITY.md)

## Native monster-drop checkpoint persistence V11 — REAL-CYD PASS (2026-10-02)

Hardware-tested head:
`41665b6676f31372ef67a41257fd8baabc74d8ed`.

on `agent/esp32-monster-drop-checkpoint-v11`.

The compact 8-slot `EspNativeGameplayMonsterDrop` owner is now part of the
native checkpoint. Legacy Doom RPG is the behavioral reference: its World save
serialized the eight rotating drop entities plus `dropIndex`, and LOAD restored
that already-materialized state directly rather than calling
`Entity_spawnDropItem()` or consuming RNG again.

V11 is an append-only checkpoint extension:

```text
V10 bytes                    = 5460
monster-drop snapshot        = 144
V11 bytes                    = 5604
rotating slots               = 8
record size                  = 16 B
CRC coverage                 = full V11 record
legacy V1..V10 read support  = retained
```

The snapshot keeps the exact current pool rather than its history: eight compact
records plus arena identity, `spawnSerial`, `nextSlot` and a semantic FNV.
`visibleCount` is derived on restore. Records carry only native identity,
tile/world position and active/taken state; no `Entity_t`, mutable BSP sprite,
map-wide decompression or ZIP runtime state is introduced.

Session replacement now explicitly resets the drop owner before checkpoint
restore. This matters for LOAD on the same map, where the rebuilt immutable arena
has the same FNV and could otherwise retain post-SAVE live state accidentally.
V1..V10 checkpoints therefore restore an honestly empty dynamic-drop pool.

The real CYD first proves backward compatibility by loading the pre-existing V10
checkpoint:

```text
[NATIVESAVE] READABLE-SPATIAL ... bytes=5460 ... result=valid
[NATIVESAVE] LEGACY-MONSTER-DROP-GAP version=10 dynamicDrops=fresh-empty ... rng=untouched
[NATIVESAVE] LOAD ... version=10 bytes=5460 ... monsterDrops=legacy-empty/0/00000000/serial0/next0 ...
```

A lethal zombie attack then materialized Shell Clips in slot 0. The live drop was
left on the floor and saved:

```text
[MONSTERDROP] COMMIT roll=b61a3cc5 slot=0 ... type=16 subtype=2 def=86 tile=178 pos=1184,352 visible=1 next=1 ...
[MONSTERDROP] SAVE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 snapshotBytes=144 rng=untouched
[NATIVESAVE] SAVE ... version=11 bytes=5604 ... recordCrc=3a147996 ...
```

The player then picked that live drop up after the SAVE. A same-map LOAD of the
saved checkpoint restored the earlier pool exactly:

```text
[NATIVESAVE] READABLE-SPATIAL ... bytes=5604 ... result=valid
[MONSTERDROP] RESTORE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 rng=untouched materialize=replay-no
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... monsterDrops=restored/1/16550b12/serial1/next1 ... monster-drops-restored-exact
```

The user confirmed the restored presentation is visually correct. Entering the
saved drop tile then consumed the restored dynamic drop through the normal
transactional pickup path, together with the co-located static ammo pickup:

```text
[MONSTERDROP] RENDER-CULL slot=0 tile=178 reason=player-tile pickup=pending-after-commit
[PLAYERRES] PREPARE ... defTile=86 type=16 subtype=2 ... action=ammo value=25->35 ... worldRemove=dynamic-drop-slot rollback=armed
[PLAYERRES] PREPARE ... defTile=83 type=6 subtype=1 ... action=ammo value=6->10 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT tile=178 candidates=2 consumed=2 ...
```

This proves the live materialized state survives SAVE/LOAD without RNG replay or
legacy entity reconstruction, and that same-map session replacement does not leak
the post-SAVE consumed state into the restored checkpoint.

A second real-CYD run independently closes the opposite checkpoint state:
the drop was already consumed before SAVE. The saved PlayerResources overlay
contained 36 consumed resources, and V11 restored the same rotating slot as
taken:

```text
[PLAYERRES] RESTORE ... consumed=36 bytes=43 ...
[MONSTERDROP] RESTORE version=11 arena=c3882516 serial=1 next=1 visible=0 stateFNV=8d1747e5 rng=untouched materialize=replay-no
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... monsterDrops=restored/0/8d1747e5/serial1/next1 ... monster-drops-restored-exact
```

The drop did not reappear visually after LOAD. Hardware therefore proves both
V11 states for the same slot: an untaken saved drop restores visible and
pickable; a taken saved drop restores invisible and stays consumed.

Normal `esp32-cyd` CI #1540 is SUCCESS:

```text
static RAM   = 45392 B
linked Flash = 781433 B
artifact id  = 11248194036
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_MONSTER_DROP_CHECKPOINT_V11.md](MILESTONE_ESP32_NATIVE_MONSTER_DROP_CHECKPOINT_V11.md)

## Native LEVEL UP screen and checkpoint resume projection — REAL-CYD PASS (2026-10-02)

Hardware-tested head:
`ccce5be96690086d56babc477c88b41faf289c72`
on `agent/esp32-levelup-drop-consequences`.

Level-up consequence ownership is now fully native. Lethal combat applies XP and
the existing exact legacy stat rolls in `EspNativeGameplayPlayerState`, then
opens a dedicated 160x120 LEVEL UP owner instead of the standalone dialog
engine. It uses no timeout or typewriter state. A touch release barrier prevents
the attack press from dismissing the screen; the next fresh tap anywhere closes
it.

The screen owns physical presentation while active. Hidden gameplay compositors
may finish internal death/HITFX work, but attempted gameplay presents rebuild
and republish the LEVEL UP frame instead of leaking a world frame through.
Timed view/action feedback expiry is deferred while this owner is active.
Dismissal explicitly repaints the PlayerState-backed HUD and one complete world
frame before ownership is released.

The accepted CYD presentation shows the actual transition, e.g.
`LEVEL 1 -> 2`, and six compact cards. Each card now shows the current final
stat in ivory beside the gain in green (for example `35 +5`) for Max HP,
Max Armor, Defense, Strength, Agility and Accuracy. The shared 5x7 font gained
the missing `>` glyph; compact values use the shared 3x5 face.

Real-CYD closure:

```text
[LEVELUP] PRESENT ... level=1->2 ... input=fresh-release+fullscreen-tap stats=current+gain-mini owner=dedicated-fullscreen timer=none
[MONSTERTURN] SKIP reason=PLAYER_ATTACK levelup=active legacySkipTurn=yes mutation=no
[RESIDENTGAMEPLAY] LEVELUP-TAP ... source=fresh-fullscreen ...
[GAMEPLAYHUD] REPAINT health=35/35 armor=11/23 ...
[LEVELUP] CLOSE ... worldRedraw=complete turnAdvance=no owner=released
[RESIDENTGAMEPLAY] LEVELUP-CLOSE hudRepaint=yes ... worldRedraw=yes fullScreenOwner=released turnAdvance=no
```

Checkpoint resume also reconstructs the presentation-only monster movement
projection before cache-witness/world rendering. The checkpoint already owns
exact MonsterPosition/topology/collision state; the projected mask is derived
from restored position versus immutable BSP position and is therefore not
serialized:

```text
[MONSTERMOVELIVE] CHECKPOINT-PROJECTION arena=c3882516 monsters=30 projected=9 source=restored-position-v9 inference=raw-bsp-delta firstFrame=exact
```

The user confirmed the previously moved zombie is at its saved location
immediately on LOAD instead of appearing at BSP spawn until its first move.

CI #1532 succeeds with 45392 B static RAM and 778489 B linked flash.
Level-up sound remains deferred. Dynamic-drop SAVE/LOAD persistence was a
separate boundary here and is now hardware-closed by the V11 milestone above.

See
[MILESTONE_ESP32_NATIVE_LEVEL_UP_AND_RESUME_PROJECTION.md](MILESTONE_ESP32_NATIVE_LEVEL_UP_AND_RESUME_PROJECTION.md).

Recovery and development must start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime truth.

## Native monster dynamic drops — REAL-CYD PASS (2026-10-02)

Hardware-tested head:
`61b156415829a4f4bc70f76d1ba751effedc29eb`
on `agent/esp32-levelup-drop-consequences`.

Lethal native monster combat now materializes original Doom RPG drops into an
8-slot compact map-session owner instead of mutating immutable BSP sprites.
Drop selection uses the same already-consumed legacy RNG word and type/subtype
rules as `Entity_spawnDropItem()`. Live records carry only compact identity,
tile/world position and ownership state; rendering and pickup consume that owner
without a map-wide entity clone.

The hardware test closes the important renderer/pickup boundary. A pre-fix test
showed `fail=SPRITES -> MOVE ROLLBACK` when the camera entered a drop tile.
The tested fix culls that billboard for the committed player tile, allowing the
normal post-MOVE resource service to consume it transactionally.

Real-CYD witness:

```text
[MONSTERDROP] COMMIT roll=d8fb13bb ... type=3 subtype=21 def=92 tile=268 ... visible=1
[MONSTERCOMBAT] COMMIT ... dropMaterialize=live
[MONSTERDROP] RENDER-CULL slot=0 tile=268 reason=player-tile pickup=pending-after-commit
[RESIDENTGAMEPLAY] MOVE ... tile=267->268 ... committed=yes
[PLAYERRES] PREPARE ... type=3 subtype=21 ... action=armor value=1->5 ... worldRemove=dynamic-drop-slot
[PLAYERRES] PREPARE ... type=3 subtype=20 ... action=health value=26->30 ... worldRemove=hidden-overlay
[PLAYERRES] COMMIT tile=268 candidates=2 consumed=2 ... hp=30/30 armor=5/20
```

The same tile therefore proves mixed dynamic + static pickup ordering and
rollback ownership. This historical live-path milestone intentionally stopped at
`save-deferred`; checkpoint persistence is now hardware-closed separately by
the V11 milestone above.

CI #1524 succeeds at 45352 B static RAM and 776233 B linked flash.

The dedicated level-up presentation added later on this branch is now
hardware-valid; see the LEVEL UP/resume-projection section above.

See
[MILESTONE_ESP32_NATIVE_MONSTER_DYNAMIC_DROP.md](MILESTONE_ESP32_NATIVE_MONSTER_DYNAMIC_DROP.md).

## Mission report and V10 progress — development candidate (2026-10-02)

Changes on `fix/mainMenu`, rebased onto `origin/main` at `0f1cdb0` (native menu
state/header independence and Player divider retirement). The rebase has no
conflicts and preserves the mission-report implementation unchanged. The user
accepted the mission report on the rebased CYD branch at `e0ae824` (2026-10-02).
This is user-confirmed report acceptance, not a serial-log proof of every counter
or of the V10 checkpoint round-trip. Earlier hardware witnesses remain below.

The full-screen end-of-stage report now uses the HUB industrial palette and its
shared crisp 5x7 font: compact `MISSION COMPLETE` header, source sector name,
`SECRETS` and `MONSTERS` cards with proportional completion bars, then `TIME`,
`MOVES`, and `XP GAINED`. The existing single-tap continuation/handoff remains
unchanged. The report uses the existing framebuffer, no allocation and no asset
reads. Unusually long numbers fall back to unscaled 3x5 text to stay in their cards.

Counter ownership stays in `EspPlayerFreshMapState`, not another engine module:

- Duration uses the ESP32 monotonic timer. No RTC, date, network or NTP is needed.
  It starts when resident gameplay is actually armed, includes time in the HUB
  and dialogs, and excludes initial loading and time offline between SAVE/LOAD.
  Formatting is `mm:ss`, then `h:mm:ss`; time/Moves saturate instead of overflowing.
  A single continuous session must be shorter than the 32-bit millisecond wrap
  period (about 49.7 days).
- `MOVES` follows original `Game_advanceTurn -> Player_updateBerserkerTics`:
  one count at the native player-turn scheduling boundary, including attacks and
  PASS_TURN, even without enemies. Rotation and menu interaction do not count.
  Dialog-skipped turns and rolled-back movement do not count; a blocked Automap
  move that deliberately advances a turn does count.
- XP is the player's cumulative `xpGained` minus its baseline captured before the
  new map's initial tile events. A player level-up cannot erase earned XP.
- Entering another map resets these counters. This is a report for the current
  map visit, not a new campaign-total/completed-level owner.

New checkpoints are V10 (`DRPGSV10`, 5460 bytes), retaining the exact V9 mutable
world sections and appending a CRC-covered 16-byte suffix at offset 5444:
`moves`, elapsed milliseconds, cumulative XP baseline, map ID, complete-history
flag and two zero reserved bytes. Absolute boot timestamps are never serialized.
The original 132-byte core and 52-byte player layout/fingerprint are unchanged.
The streamed spatial workspace remains 3508 bytes and atomic temp/backup/rename
verification now includes the suffix. Earlier V1–V9 saves remain readable;
pre-load counters cannot be recovered from them, so the three new metrics start
at loading and the report shows `SINCE LOAD`. That flag survives re-saving as V10
until the next map entry. Older firmware cannot load new V10 saves.

Local validation: normal `pio run -e esp32-cyd` succeeds with 45208 B static RAM
and 771897 B linked flash after the rebase (respectively +32 B and +1704 B vs
the documented native-menu artifact). The host progress regression was rerun
successfully on the rebased tree. The included host regression
[`test/test_level_progress.c`](test/test_level_progress.c) covers fresh/reset,
XP baseline across a level-up, resume, legacy saves, timer wrap and saturation.
Additional temporary fixtures exercised production C report painting and the
C++ checkpoint writer/byte verification with an in-memory SD: normal/partial/max
values, header above y=20, framebuffer guards, present refusal, suffix layout,
V9/V10 CRC, truncation/corruption and checkpoint replacement.

CYD validation status:

The user has accepted the end-of-mission screen on the rebased branch. No serial
log or detailed per-scenario test results accompanied that acceptance; the
following checklist remains available for targeted regression testing. V10 reboot
resume and legacy-save migration are not marked hardware-proven.

1. Start a fresh game, perform movement/attacks/PASS_TURN, rotate and visit the HUB;
   finish a stage and check the report's layout and counters.
2. Tap once: verify the next map loads, gameplay HUD returns and its report starts
   with new counters.
3. SAVE/LOAD V10 mid-stage, including after reboot; verify duration/Moves/XP resume
   without including offline time. Look for `[LEVELPROGRESS]` and `[LEVELSTATS]`.
4. LOAD an existing V9 checkpoint, finish its stage and verify `SINCE LOAD`; save
   as V10 and load again to verify the partial-history flag remains honest.

## Previous main hardware/CI witness (before this branch)

```text
current main = afb7c7e8034ecb1084c0ed066bd1d5a03aa18ed1
branch = agent/esp32-native-menu-state-root
hardware-tested code boundary = ad8fe2f6e1ed8186a1cdd871d241b72623d905c6
CI = esp32-cyd #1509 SUCCESS
static RAM = 45176 B
linked Flash = 770193 B
artifact id = 11225823078
artifact digest = sha256:f37f9fc35cc06b9731ba50f4a41c6d27ccc20a00e61fb1c26743cd683c6cb6fd
hardware = targeted story teardown probe PASS + normal Start Game intro -> Entrance -> ENGINESESSION READY PASS
status = early engine teardown cannot retain native story hand owner; DoomCanvas_free release is hardware-proven with exact heap restoration
```

## Native divider formatter replaces Player MenuSystem buffer — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`ad8fe2f6e1ed8186a1cdd871d241b72623d905c6`.

ESP32 `Player.c` no longer depends on `MenuSystem_buildDivider()` or the
legacy `MenuSystem_t::stringBuffer` scratch. The replacement is the
caller-owned, allocation-free `EspNativeText_buildDivider()`.

The first probe exposed a latent legacy overflow pattern: the original
`strncpy(..., 32)` zero-padding could write beyond the 32-byte destination
when reproduced with a local buffer. The native formatter now copies only the
visible legacy text bytes and writes the suffix/NUL explicitly.

A guarded hardware probe validates all four Player divider strings byte-for-byte
while checking heap stability and post-buffer guard bytes:

```text
[DIVIDERPROBE] PASS cases=4 heap8=84048 exact=yes allocation=no owner=caller
```

The diagnostic firmware then continued normally through prerender, mappings and
native MAIN with no stack-protector failure.

Normal `esp32-cyd` CI #1509 is SUCCESS at the same code boundary.

## Native menu runtime decoupled from MenuSystem.h — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`35b6986686fa015a4c1ce554b45f1c6d1c865f7d`.

The permanent ESP32 menu runtime modules no longer include the legacy
`MenuSystem.h` header for state layout or capacity. They include
`esp_native_menu_state.h` directly and use
`ESP_NATIVE_MENU_MAX_ITEMS`.

The CI initially exposed two residual `MAX_MENUITEMS` uses in the native menu
model; these were replaced with the native capacity constant without restoring
the legacy include.

Real-CYD validation covered the full path:

```text
boot
[MENUSTORAGE] INIT bytes=496
OPTIONS -> Back
HELP page up/down -> Back
Start Game
INTRO1 FNV=ade0195d deltaHeap=0
intro disposal recovered=33944
Entrance load
ENGINESESSION READY
HUB open -> weapons -> status -> system -> close
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1489 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11225823078
digest       = sha256:f37f9fc35cc06b9731ba50f4a41c6d27ccc20a00e61fb1c26743cd683c6cb6fd
```

This establishes that the native menu runtime no longer depends on the legacy
menu header for its own state representation.

## Native menu state header extracted — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`d67caf8fe889f39802ca8a4a8499ecac53fb9e3d`.

The permanent ESP32 menu state definition now lives in
`ESP32/src/esp_native_menu_state.h`.
`src/MenuSystem.h` no longer owns the native struct layout; on ESP32 it is only
a compatibility facade that imports the native header and aliases the legacy
name.

The struct remains layout-identical:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=EspNativeMenuState_t
[CORE] MenuSystem used=512
```

Real-CYD validation covered:

```text
boot
OPTIONS -> Back
HELP page up/down -> Back
MAIN FNV=522dc605
Start Game
INTRO1 FNV=ade0195d deltaHeap=0
intro disposal recovered=33944
Entrance load
ENGINESESSION READY
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1466 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11223917653
digest       = sha256:ecd9433708f4f6c3b7dbeb25770faa0f4b87cdd2b7c8733b84c21fe6ddb44021
```

This proves the native type can live independently of the legacy header before
native modules drop their own `MenuSystem.h` includes.

## Native menu runtime APIs typed directly — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`7a14a0c8c092f7a24195251e669f020982206755`.

All permanent ESP32 native menu runtime modules now use
`EspNativeMenuState_t*` directly rather than the compatibility
`MenuSystem_t*` alias, including storage, MAIN model/touch/actions,
OPTIONS/Back, Start action and the core-size accounting probe.

Real-CYD proof:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=EspNativeMenuState_t
MAIN FNV=522dc605
HELP page-up 24->16->8->0 exact FNVs=9213df95/d0788359/5f22cf6b
HELP -> MAIN FNV=522dc605
heap8=62112
largest8=32756
p.bmp preflight=present
```

Normal `esp32-cyd` CI #1458 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11224131637
digest       = sha256:77ad414bf3e5f0487444260776c527cc239fbaaf1a99f4af4f583c2deb4466bb
```

This validates direct native typing before moving the native state definition
out of the legacy `MenuSystem.h` header.

## Shared p.bmp preflight restored — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`862d2f7ca482d74c1fbbd2f7730d0e3479d3c22c`.

A review correctly identified that `p.bmp` remained a mandatory shared runtime
dependency after ownership moved out of the legacy menu shell: native story
presentation and the gameplay HUB both still require it. Removing it from the
startup preflight allowed a malformed PAK to pass startup and fail later during
Start.

The resource remains **not owned by MenuSystem**. Only its presence validation
is restored:

```text
[PRERENDER] Resource preflight (3 files)
[PRERENDER] j.bmp          bytes=4264 backing=pak
[PRERENDER] p.bmp          bytes=156 backing=pak
[PRERENDER] entities.db    bytes=2762 backing=pak
[PRERENDER] Resource preflight OK
```

The same real-CYD boot reaches the unchanged native MAIN:

```text
[MENUSTORAGE] INIT bytes=496
MAIN FNV=522dc605
heap8=62112
largest8=32756
```

Normal `esp32-cyd` CI #1448 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11223741289
digest       = sha256:09daf578d0c2f854ee27fdfefbf2a7d49be0a45527e976ba03293bb247b11971
```

## Native menu state root identity — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`f76e3ed415c5d2012fe61cc38b7cca11631ee796`.

This first root-migration step introduces `EspNativeMenuState_t` as the
canonical ESP32 menu-state identity while preserving the existing 496-byte
layout exactly.

On ESP32:
- `DoomRPG_t::menuSystem` now points to `EspNativeMenuState_s*`;
- `DoomCanvas_t::menuSystem` now points to `EspNativeMenuState_s*`;
- `MenuSystem_t` remains only as a compatibility alias for surviving legacy
  signatures;
- desktop/J2ME layout remains unchanged.

Real-CYD regression:

```text
[MENUSTORAGE] INIT bytes=496
MAIN FNV=522dc605
HELP0 FNV=5f22cf6b
HELP8 FNV=d0788359
HELP16 FNV=9213df95
HELP24 FNV=b0191189
HELP32 FNV=4c944ee5
HELP40 FNV=943f0b77
OPTIONS FNV=162d3999
MAIN heap8=62112
largest8=32756
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1434 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770197 B
artifact id  = 11222798316
digest       = sha256:6b61f9188b2a079e11d527d0cfd357e9062e67927826b500e35e1d597bc4e503
```

No runtime behavior or layout change was observed. This validates the native
type identity before removing the compatibility alias from native APIs.

## Story-hand teardown safety — REAL-CYD PASS (2026-10-02)

Production fix boundary:
`f398807df63c88d3d453a71eaf675cd1be2c1dbd`.

A reviewer correctly identified that the native story-hand owner was only
released on normal intro disposal. If the engine tore down first, the
module-global `storyHandOwner` could outlive the `DoomRPG_t` it referenced.

The permanent fix adds an unconditional ESP32 release in
`DoomCanvas_free()`:

```text
DoomCanvas_free()
 -> Esp32StoryFit_release(doomCanvas)
 -> clear storyHand + storyHandOwner
 -> continue ordinary canvas teardown
```

The normal `esp32-cyd` build at this boundary is CI #1420 SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770197 B
artifact id  = 11222143129
digest       = sha256:2f99a975d67cf895c54a5d50fbc0714b6829fc91c7ee65215ddb5fe041683911
```

A dedicated diagnostic env,
`esp32-cyd-story-teardown-probe`, was then hardware-tested on the real CYD.
It creates a synthetic canvas, prepares the native story hand, invokes the real
`DoomCanvas_free(..., false)`, and requires both owner release and exact heap
restoration.

Real-CYD proof:

```text
[STORYTEARDOWN] BEGIN heap8=84048 owner=0
[INTROFIT] HAND-READY asset=p.bmp bytes=bounded owner=native-story
[INTROFIT] PREPARE hand=13x10 asset=p.bmp owner=native-story ...
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[STORYTEARDOWN] PASS prepare->DoomCanvas_free->released heap8=84048 exact=yes
```

The same diagnostic firmware then continued through the normal Start Game intro
and reached native gameplay, confirming that the targeted teardown probe does not
damage the ordinary path.

## Final ESP32 MenuSystem compatibility shell — REAL-CYD PASS (2026-10-02)

Hardware-tested production boundary:
`d8d0622eb92d7f32b2e37033cb557b6ba35deb5e`.

The final simple trim for this milestone removes the ESP32-only
`MenuSystem_t::doomRpg` backpointer and the write-only `paintMenu` flag.
`EspNativeMenuStorage_startup/free` now receive the `DoomRPG_t*` owner
explicitly.

The real CYD reports the final compact shell as:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=MenuSystem_t desktopTU=no
[CORE] MenuSystem used=512
```

The measured struct size is therefore **496 B**, not the conservative 504 B
estimate. Relative to the original 604 B compact compatibility shell, this
milestone removes 108 B from the retained ESP32 layout.

Full real-hardware regression on the exact code boundary covered:

```text
cold boot
MAIN exact FNV=522dc605
OPTIONS -> Back
HELP page 0 -> 8 -> 16 -> 8 -> Back
Start Game
full semantic intro
bounded first ST_INTRO draw with deltaHeap=0
native story-hand release gain=176 B
bounded intro disposal heap8=69492 -> 103436 recovered=33944
/intro.bsp Entrance
ENGINESESSION READY
one committed FORWARD move
native HUB open
```

The menu presentation fingerprints remain exact:
`OPTIONS=162d3999`, `HELP0=5f22cf6b`,
`HELP8=d0788359`, `HELP16=9213df95`.

The story-owner contract remains symmetric and allocation-safe:

```text
[INTROFIT] PREPARE ... drawAllocation=no
[INTRO1] Drawn ... deltaHeap=0
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand ... gain=176
```

The same run reaches native gameplay with:

```text
shapeData   = NULL
mediaTexels = NULL
```

Normal `esp32-cyd` CI #1413 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770189 B
artifact id  = 11221787233
digest       = sha256:aabcf3c22267d1b0866f4b642eee70fa2d81b14236cfed1a7b891bfed4dc0603
```

This closes the bounded field-pruning phase. The remaining ESP32
`MenuSystem_t` fields are live native-menu state; replacing the type/root is
a separate architectural milestone rather than another dead-field cleanup.

## 512-byte MenuSystem shell + native story hand owner — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`0a2bcc39656d8b855d777edae4273a368f44307c`.

The ESP32-only compatibility shell now removes an additional 40 bytes of legacy
layout residue:

```text
memory
imgHand
imgArrowUpDown
field_0xc58
f749g
setBind
```

This brings `sizeof(MenuSystem_t)` from 552 B to 512 B on ESP32.

A real remaining consumer of the old `imgHand` was found in
`native_story_fit.c`. That dependency is now owned natively:
`p.bmp` is acquired before the bounded first ST_INTRO frame, the draw path is
allocation-free, and the same owner is released by the bounded native intro
disposer.

Real-CYD proof:

```text
[INTROFIT] HAND-READY asset=p.bmp owner=native-story
[INTROFIT] PREPARE hand=13x10 asset=p.bmp owner=native-story ... drawAllocation=no
[INTRO1] Drawn ... heap8=69484->69484 deltaHeap=0
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand heap8=69484->69660 gain=176
[INTRODISP] READY ... heap8=69484->103428 recovered=33944
```

The final disposal heap exactly returns to the pre-intro allocation level,
proving no retained story-hand leak.

The same hardware run continues through:
- full semantic intro navigation;
- bounded intro disposal;
- native transition loading;
- `/intro.bsp` Entrance runtime creation;
- fresh generic gameplay session;
- `ENGINESESSION READY`.

Critical invariants remain:

```text
shapeData   = NULL
mediaTexels = NULL
```

Normal `esp32-cyd` CI #1389 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770205 B
artifact id  = 11221054541
digest       = sha256:1a8e220c45c25194375ad2d8a9bfbb8d02c3e5dc8adc6097c9cb08c4b8e025be
```

## MenuSystem compatibility-shell trim — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`9d42d669d274c14bf39fde5a4d8af75df3cbe04b`.

The ESP32-only `MenuSystem_t` layout no longer carries six fields that had no
surviving reader in either compiled desktop translation units or native ESP32
code:

```text
stringBuffer[32]
bindIndx
nextMsgTime
nextMsg
cheatCombo
digitCount
```

The two native writes to `cheatCombo` / `digitCount` were dead resets and
were removed with the fields. Desktop/J2ME layout remains unchanged outside the
ESP32 build.

Real-CYD structural witness:

```text
[MENUSTORAGE] INIT bytes=552 items=8 ...
[CORE] MenuSystem used=568
```

Previous hardware-proven values were 604 B / 620 B, so the shell shrank by
exactly 52 bytes.

Runtime memory on the same hardware:

```text
MAIN heap8:     62016 -> 62060  (+44 B)
gameplay heap8: 55416 -> 55456  (+40 B)
largest8:       38900 -> 38900  (unchanged)
```

Regression coverage:
- cold boot and native MAIN exact;
- OPTIONS -> Back;
- HELP page down/up -> Back;
- V9 LOAD -> Sector 1 -> ENGINESESSION READY;
- native gameplay HUB opened and all four pages rendered.

All relevant menu fingerprints remain exact and
`shapeData == NULL` / `mediaTexels == NULL` remain true.

Important ownership clarification discovered during the hardware run:
the native gameplay HUB still intentionally uses `p.bmp` through its own
`HUB_FACE_NAME` path in `esp_native_gameplay_hub.c`. The earlier
`p.bmp/q.bmp` retirement applies only to `EspNativeMenuStorage_startup()`
and the old `MenuSystem_t::imgHand/imgArrowUpDown` ownership; it does not mean
that `p.bmp` is globally unused.

Normal `esp32-cyd` CI #1360 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769573 B
artifact id  = 11219735467
digest       = sha256:ec07fc36ff3aff92080e34444a2066fbb5a221924df0536c5a3eb78c8314f407
```

## Legacy menu accessory assets retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`31caa46af67a88550adcf2cd01ccbf4124bcf40b`.

The ESP32 runtime no longer preflights or loads the legacy menu accessory BMPs
`p.bmp` / `imgHand` and `q.bmp` / `imgArrowUpDown`. Direct final-ELF
inspection of the previous hardware-proven image showed that their remaining
source references lived only in DoomCanvas Story/Epilogue/scrollbar functions
that did not survive the link. The native menu keeps only `j.bmp`, which is
actually presented as the MAIN/OPTIONS logo.

Real-CYD cold boot proves the reduced startup contract:

```text
[PRERENDER] Resource preflight (2 files)
[PRERENDER] j.bmp          bytes=4264 backing=pak
[PRERENDER] entities.db    bytes=2762 backing=pak
[MENUSTORAGE] STARTUP READY assets=j owner=esp-native
              legacyAccessoryAssets=p/q-retired
```

Measured runtime improvement versus the previous hardware-proven boundary:

```text
EspNativeMenuStorage_startup: 4500 -> 4156 B  (-344 B)
MAIN heap8:                   61672 -> 62016   (+344 B)
gameplay heap8:               55024 -> 55416   (+392 B)
gameplay largest8:            38900 -> 38900   (unchanged)
```

MENU regression coverage on the real CYD:

```text
MAIN -> OPTIONS -> Back
MAIN -> HELP
HELP page 0 -> 8 -> 16 -> 24 -> 32 -> 24 -> 16 -> 8 -> 0
HELP -> Back -> MAIN
MAIN -> LOAD V9 -> Sector 1 -> ENGINESESSION READY
```

All relevant presentation fingerprints remain exact:

```text
MAIN        = 522dc605
OPTIONS     = 162d3999
HELP page0  = 5f22cf6b
HELP page8  = d0788359
HELP page16 = 9213df95
HELP page24 = b0191189
HELP page32 = 4c944ee5
```

Critical invariants remain `shapeData == NULL` and `mediaTexels == NULL`.

Normal `esp32-cyd` CI #1354 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769585 B
artifact id  = 11218479535
digest       = sha256:8997a4b99c89bc57e149d7dcf8d51e56c88ef9eb6d0f6765829d6659a13bee0b
```

The retained `imgHand` / `imgArrowUpDown` struct fields are now layout-only
compatibility residue; no runtime asset is owned behind them.

## Desktop MenuItem helper translation unit retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`4c4a48230303cef7eeedc9722198fbe2c7506517`.

The remaining ESP32 callers of desktop `MenuItem_Set()` / `MenuItem_Set2()`
were only the already-native fixed main-menu builders. Those trivial writes are
now local native bounded copies, and `src/MenuItem.c` is excluded from the
ESP32 compile graph.

Real-CYD validation covers the exact affected surface:

```text
MAIN stable for >95 s
 -> OPTIONS -> Back
 -> HELP page up/down through multiple ranges -> Back
 -> OPTIONS -> Back again
```

All previously validated framebuffer/model fingerprints remain exact:

```text
MAIN        = 522dc605
OPTIONS     = 162d3999
HELP page0  = 5f22cf6b
HELP page8  = d0788359
HELP page16 = 9213df95
HELP page24 = b0191189
```

The menu remains allocation-stable outside the intentionally temporary HELP
buffer:

```text
MAIN/OPTIONS heap8=61672 largest8=32756
HELP active  heap8=60248 largest8=32756
HELP Back    heap8=61672 largest8=32756
```

Normal `esp32-cyd` CI #1341 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769613 B
artifact id  = 11218132618
digest       = sha256:ecf7fb1385a00d7577c85624805d658dadede308802ae0ce7894c9e8fb269448
```

Direct final-ELF inspection confirms:

```text
MenuItem_*   = 0
MenuSystem_* = 0
```

The ESP32 compile graph now excludes `Menu.c`, `MenuItem.c`,
`MenuSystem.c`, and `ParticleSystem.c`.

Remaining menu-related cleanup is no longer equivalent dead-helper retirement:
`imgHand` and `imgArrowUpDown` are still referenced by linked DoomCanvas
Story/Epilogue/scrollbar paths, while full `MenuSystem_t` root replacement
crosses multiple compatibility contracts. Those belong to later bounded
milestones rather than this retirement step.

## Compact ESP32 menu storage — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`bb04faa839169c559e161ff0dea59b5e8f1e6dbc`.

The retained compatibility `MenuSystem_t` no longer reserves the desktop
`MenuItem_t items[96]` array on ESP32. Its ESP32 item capacity is bounded to 8,
which is sufficient for the fixed native MAIN/OPTIONS/CONTINUE models (4/4/3).
HELP keeps the original `help.txt` payload compact and uses a bounded native
line-offset table instead of inflating 83 lines into desktop `MenuItem_t`
records.

Real-CYD HELP witness:

```text
[MAINMODEL] HELP-PARSE bytes=1405 ... declared=83 parsed=83
            compactBytes=1116 menuItemSlots=8 lineChars<=31 result=valid
[MAINHELP] PAGE-DOWN 0->8 ->16 ->24
[MAINHELP] PAGE-UP   24->16 ->8 ->0
[MAINBACK] READY source=help ... frame=522dc605
```

The fixed OPTIONS path also remains exact and allocation-free:

```text
MAIN heap8=61624 largest8=32756
OPTIONS heap8=61624 largest8=32756
Back -> MAIN frame=522dc605
```

The V9 LOAD path then restores Sector 1, reaches resident gameplay, executes
player movement and ordered four-monster movement/three-goal behavior, including
publication of a three-loop monster attack probe.

Hardware memory improvement versus the previous MenuSystem-retirement boundary:

```text
MAIN menu: 56908 -> 61624 heap8  (+4716 B)
gameplay:  50300 -> 55024 heap8  (+4724 B)
largest8:  38900 -> 38900        (unchanged in gameplay)
```

HELP cleanup returns exactly to the pre-HELP MAIN value `heap8=61624`, proving
the compact HELP allocation is not leaked.

Critical invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
```

Normal `esp32-cyd` CI #1333 is SUCCESS:

```text
static RAM   = 45208 B
linked Flash = 769665 B
artifact id  = 11202101528
digest       = sha256:308632475a69248c35d2bd57ceac73e12c7cd432ebbf77884cf83117e9b9cf87
```

The remaining `MenuSystem_t` is now a small compatibility shell rather than a
5.5 KiB desktop menu container. Further retirement must audit each remaining
field and linked consumer before replacing the root type/pointer entirely.

## Desktop MenuSystem translation unit retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`2e0193b4e4f82d78f0b361c84e0d2bcd4f8f1cba`.

The ESP32 build no longer compiles `src/MenuSystem.c`. The final ELF contains zero
`MenuSystem_*` symbols. The still-transitional `MenuSystem_t` allocation is now
owned by `EspNativeMenuStorage`, which performs only the bounded storage/image
responsibilities still consumed by the native MAIN/OPTIONS/HELP models.

The former menu audio calls are preserved as semantic native intents rather than
desktop Sound/MenuSystem behavior:

```text
5046 = select/enter
5042 = back
5067 = in-game menu entry companion cue
```

The backend remains deliberately silent. The real CYD proves call-order publication
through:

```text
[AUDIOINTENT] seq=1 resource=5046
[AUDIOINTENT] seq=2 resource=5042
[AUDIOINTENT] seq=3 resource=5046
[AUDIOINTENT] seq=4 resource=5042
```

Hardware path:

```text
cold boot
 -> EspNativeMenuStorage INIT 5532 B / 96 items
 -> p/q/j asset startup
 -> MAIN
 -> OPTIONS -> Back
 -> HELP -> page down/up -> Back
 -> V9 LOAD Sector 1
 -> resident gameplay
 -> player MOVE
 -> ordered four-monster movement
```

CI #1324 succeeds in normal `esp32-cyd`:

```text
static RAM   = 45000 B
linked Flash = 769861 B
artifact id  = 11200848779
digest       = sha256:82fdd69dd60c3a8fa22fe15ce82c296bc291a854495807d7ec7d76c8682961dd
```

The +8 B static RAM is the native audio-intent state. Hardware memory moves by the
same exact amount relative to the previous milestone and fragmentation is unchanged:

```text
MAIN:     heap=122832 heap8=56908 largest8=32756
gameplay: heap=116224 heap8=50300 largest8=38900
```

Critical invariants remain `shapeData == NULL` and `mediaTexels == NULL`.

The next structural question is not `MenuSystem.c` anymore; it is the 5532-byte
compatibility layout itself. Any compaction must first prove that no linked
DoomCanvas/DoomRPG path still dereferences fields outside the native model contract.

## Dead Menu / ParticleSystem translation-unit retirement — REAL-CYD PASS (2026-10-02)

Commit `bc65cc337000d8c7ef54b5f0451e51d958cf7cef` removes the already-retired
desktop `Menu.c` and `ParticleSystem.c` translation units from the ESP32 compile
graph. Previous milestones had already proven zero final `Menu_*` and
`ParticleSystem_*` symbols; this milestone stops spending compiler/build surface on
them at all.

CI #1298 succeeds with exactly the same 44992 B static RAM and 769357 B linked Flash,
confirming that no linked runtime code changed. The real CYD then loads a V9 Sector 1
checkpoint, restores the native session, moves the player, services four monsters in
order, runs subtype-4 continuation and resolves a three-loop attack. Gameplay remains
stable at `heap=116232 heap8=50308 largest8=38900`, with
`shapeData == NULL` and `mediaTexels == NULL`.

The known lack of smooth player MOVE/TURN interpolation is recorded separately as a
native presentation gap. It is not part of this retirement and must not justify
reintroducing desktop ownership.

See
[MILESTONE_ESP32_RETIRE_DEAD_MENU_PARTICLE_TUS.md](MILESTONE_ESP32_RETIRE_DEAD_MENU_PARTICLE_TUS.md).

## Ordered monster turn + lethal monster death — REAL-CYD PASS (final review closure 2026-10-02)

Hardware-tested code boundary:
`717e7bd980ff7110c227055d940d01c980a5683d`.

This closes two intentionally deferred boundaries from the earlier active-sequence
and player-death milestones.

The monster turn is now serialized in first-activation order instead of choosing
one global immediate attacker. Each active member executes its own legacy-style
`Entity_aiThink` slice. If that member publishes an attack probe, the sequence
pauses, lets AttackVisual + Retaliation resolve it, then resumes at the next
ordinal in the same semantic monster turn.

Real-CYD witness:

```text
[MONSTERACTIVESEQ] BEGIN turn=3 ... activeCount=4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=2/4 ... attackProbe=1->2 ...
[MONSTERACTIVESEQ] PAUSE ... probe=2 ... nextOrdinal=3 ...
[MONSTERRETAL] MISS-COMMIT probe=2 ... gameplayRngCommitted=yes ...
[MONSTERACTIVESEQ] RESUME ... resolvedProbe=2 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=3/4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=4/4 ... attackProbe=2->3 ...
[MONSTERACTIVESEQ] COMPLETE ... ordered=yes publication=serialized-per-member multiAttack=one-probe-at-a-time
```

The same session proves repeated pause/resume across several attackers and keeps
three-goal subtype-4 movement/shortcut behavior live. A dead active monster is
skipped on later turns without disturbing active-list order.

Monster retaliation now owns lethal player damage. On probe 10, the real CYD
commits the attack RNG and authoritative HP=0, arms the already-native
PlayerDeath owner, consumes exactly one additional legacy death RNG byte, and
terminates the remaining active-list suffix:

```text
[MONSTERTURN] MEMBER-ATTACK-PROBE ... probe=10 ... playerHP=4->0 ... lethal=deferred-player-death ...
[PLAYERDEATH] ARM seq=10 tile=506 ... hp=0 ... rngByte=87 deathSound=5058-deferred ...
[MONSTERRETAL] LETHAL-COMMIT probe=10 ... playerHP=4->0 ... rng=41a9a848->f4415a47 attackRngCommitted=yes deathRngCommitted=yes ... deathOwner=armed ... turn=terminal
[MONSTERACTIVESEQ] TERMINAL turn=7 ordinal=2/4 probe=10 cause=player-death remaining=discarded ...
[PLAYERDEATH] PHASE ... fall=complete ...
[PLAYERDEATH] READY ... elapsedMs=3012 ... input=death-menu load=available ...
```

Death-menu touch routing remains bounded and explicit. LOAD is the only live
session-replacement route. JUNCTION, RETRY and MAIN classify correctly and
remain fail-closed with no session mutation. The active LOAD row is visually
distinguished from the deliberately disabled routes. Earlier hardware on the
same branch already proved LOAD replaces the dead session with the V9
checkpoint; this final hardware run revalidates the retained menu routing and
presentation.

The final code-review P1 closes a one-session-tick input race after a
post-move attack publication. Session composition services
`MonsterActivation_serviceTurn()` before `MovementProbe/ActiveSequence`, so a
post-move member can increment the raw `MonsterTurn.attackProbes` after the
filtered activation view for that tick has already been copied. Previously,
`AttackVisual_isBusy()` observed only the filtered view; a very fast world tap
could therefore enter before the next activation delivery and a new
`runProbe()` could clear `lastAttackerSpriteIndex`.

Commit `717e7bd980ff7110c227055d940d01c980a5683d` makes the input gate also
observe the raw MonsterTurn producer. Any producer
`attackProbes > AttackVisual.observedAttackProbes` is combat-owned
immediately, while the activation-filtered view remains the normal delivery
contract on the following service tick. This is intentionally fail-closed for
an unexpected probe gap and changes no movement, RNG, retaliation or animation
transaction.

The real CYD regression run kept the four-active-monster ordered movement path
stable and the user explicitly confirmed rapid taps no longer stack through the
combat boundary. Representative stable sample remains
`heap=116232 heap8=50308 largest8=38900`.

CI #1290 succeeds in normal `esp32-cyd` at 44992 B static RAM and 769357 B
linked Flash. Artifact 11196908002 has digest
`sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5`.

Repeated live samples remain stable:

```text
heap=116232
heap8=50308
largest8=38900
```

CI #1290 succeeds in the normal `esp32-cyd` environment at 44992 B static RAM
and 769357 B linked Flash. Artifact 11196908002 has digest
`sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5`.

The old "simultaneous attack-ready = fail-closed" and "monster lethal =
fail-closed" statements remain historically true for their earlier milestones,
but are superseded by this boundary.

Detailed closure:
[MILESTONE_NATIVE_MONSTER_ORDER_AND_PLAYER_DEATH.md](MILESTONE_NATIVE_MONSTER_ORDER_AND_PLAYER_DEATH.md)


## Native player death core — REAL-CYD PASS (2026-10-01)

Hardware-tested head `d6811e23db4580795887c97c3bdf5e6493224268`
connects lethal current-tile type-10/11 `PASS_TURN` damage to the first
permanent native `ST_DYING` owner.

The real CYD proves the terminal sequence
`HP 1 -> 0 -> PlayerDeath ARM -> camera fall -> viewport fade -> death-menu-ready`.
The lethal route consumes one legacy death RNG byte, hides the first-person
weapon by clearing the death-state weapon fields, blocks all resident gameplay
input and schedules no MonsterTurn. Camera state remains in `EspPlayerView`;
the fade mutates only the shared 160x80 RGB565 viewport and allocates no second
framebuffer. `shapeData` and `mediaTexels` remain NULL.

Observed terminal state is `viewZ=6` at the 750 ms handoff and full black at
about 3004 ms. Live memory stays
`heap=116264 heap8=50340 largest8=38900`.

The inert HUD after black is intentional for this boundary: death-menu routing
is still unowned and all world input remains blocked. MOVE-hazard lethal,
monster-retaliation lethal and explosion/radius lethal producers also remain
fail-closed until their own wiring milestones.

CI #1274: 44960 B static RAM / 765497 B linked Flash, artifact 11193296410.

See
[MILESTONE_NATIVE_PLAYER_DEATH_CORE.md](MILESTONE_NATIVE_PLAYER_DEATH_CORE.md).





## Hazard PASS_TURN HUD refresh — REAL-CYD PASS (2026-10-01)

Hardware-tested head `13bb05ed09aa217a2263a4f3fb8a521348c96258` keeps the existing
hazard damage transaction unchanged and fixes only retained HUD publication.
Each committed PASS_TURN damage now repaints both HUD bands from the current
PlayerState-backed HUD overlay before the immediate damage-feedback present.

Real hardware proves consecutive transitions
`17/6 -> 16/4 -> 15/2` are visible immediately, with
`[GAMEPLAYHUD] REPAINT` and `[PASSTURN] HUD-REPAINT` matching the committed
values. Live memory stays `heap8=50364 largest8=38900`.

CI #1257: 44936 B static RAM / 762861 B linked Flash, artifact 11191801698.

The same session naturally reaches the still-fail-closed native player-death
boundary from a barrel-radius hit, with exact transaction rollback.

See
[MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md).

## Three-goal subtype 4/13 multi-loop attack — REAL-CYD PASS (2026-10-01)

Hardware-tested head `5943974dcf1b5bd1c142e4665340f51fb6fbb19b`
connects the native three-goal movement chain to the existing native monster
attack pipeline without creating a parallel combat owner.

A real subtype-4 monster commits goal 2/3 onto the adjacent tile, takes the
legacy early-shortcut to frameTime 3, publishes one `loops=3` MonsterTurn
probe, renders all three attack/idle phases, then commits the exact prospective
retaliation result. Player HP/armor changes `33/23 -> 31/21`; the six combat
RNG calls advance only at final resolution. Live memory remains
`heap8=50364 largest8=38900`.

Simultaneous attack-ready publication is intentionally still fail-closed: only
one undelivered post-move attack probe may exist at a time.

CI #1253: 44936 B static RAM / 761949 B linked Flash / 762320 B firmware.bin,
artifact 11191212285.

The same test session found a separate PASS_TURN-on-hazard HUD refresh defect:
gameplay damage commits correctly but the bottom HUD presentation is stale until
the next movement redraw. That is the next bounded presentation fix.

See
[MILESTONE_ESP32_NATIVE_MONSTER_THREE_GOAL_MULTI_LOOP_ATTACK.md](MILESTONE_ESP32_NATIVE_MONSTER_THREE_GOAL_MULTI_LOOP_ATTACK.md).

## Fire Ext monster combat semantics — REAL-CYD PASS (2026-10-01)

Hardware-tested head `ef8dc9b5f06dd93c34c5179f6b95935dd0af13d2`
restores the exact legacy weapon-1 split: Fire Ext attacks `eType == 1`
monsters through the normal generic combat math, while its impact presentation
is special-cased. Phantom subtype 4 gets a bounded gray `ce79` 15-particle
HITFX; other monster subtypes take normal damage without blood spray. A real
miss remains `No effect!`.

The real CYD kills two subtype-4 Phantoms with Fire Ext and then damages a
subtype-5 monster with the same weapon. The subtype-4 path reports
`impact=extinguisher-gray-armed`; the subtype-5 path reports
`impact=none-extinguisher`. Ammo, monster state and GIBFX commit normally,
visual RNG remains decoupled, and live memory is stable at
`heap8=50364 largest8=38900`.

CI #1247: 44936 B static RAM / 757869 B linked Flash / 758240 B firmware.bin,
artifact 11189341147.

See
[MILESTONE_ESP32_NATIVE_FIRE_EXT_MONSTER_COMBAT.md](MILESTONE_ESP32_NATIVE_FIRE_EXT_MONSTER_COMBAT.md).

## Legacy Menu root retired — REAL-CYD PASS (2026-10-01)

V24 removes the desktop `Menu_t` object from the ESP32 core graph and keeps
the inherited `doomRpg->menu` field NULL. A first real-CYD attempt exposed one
stale graphics-boundary check that still required the retired pointer; the
fail-closed dashboard contract stopped cleanly before presentation. The
follow-up commit removes only that obsolete requirement.

CI #1239 is 44936 B static RAM / 757585 B linked Flash / 757952 B firmware.bin,
with 49 active linker wraps. The final ELF contains no `Menu_*` symbols.
`MenuSystem_init/startup/playSound/free` remain intentionally linked because
the bounded native menu models still consume that storage/resource owner.

The corrected real-CYD run reaches native MAIN and exercises OPTIONS plus HELP
entry/paging/back without allocations. Core usage is 53804 B, exactly 76 B less
than V23, and steady MAIN is `heap8=56972 largest8=32756`.

See
[MILESTONE_ESP32_RETIRE_LEGACY_MENU_ROOT_V24.md](MILESTONE_ESP32_RETIRE_LEGACY_MENU_ROOT_V24.md).

## Legacy ParticleSystem core object retired — REAL-CYD PASS (2026-10-01)

V23 removes the inherited `ParticleSystem_t` object itself from the ESP32 core
graph and severs the final desktop cleanup reference. The final ELF contains
zero `ParticleSystem_*` symbols.

CI #1236: 44944 B static RAM / 757525 B linked Flash / 757888 B firmware.bin,
49 active linker wraps. The real CYD confirms the core graph shrinks by exactly
2280 B and native MAIN gains 2304 B free heap8 over V22.

The decisive hardware witness is a real monster death: the native GIBFX owner
paints, repaints and expires the gib overlay with
`legacyParticleSystem=no`, while gameplay RNG remains untouched. Thus the
legacy particle subsystem is fully absent from the production image and its
visible death-effect responsibility is hardware-proven native.

See the V23 addendum in
[MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md](MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md).

## Legacy ParticleSystem startup retired — REAL-CYD PASS (2026-10-01)

V22 removes `ParticleSystem_startup()` from the production ESP32 prerender
chain and drops `gibs_24.bmp` from the PAK preflight. The final ELF retains
only the legacy object constructor/destructor pair; no particle startup,
unlink, render, spawn or calculation code is linked.

CI #1233 is 44952 B static RAM / 757505 B linked Flash / 757872 B firmware.bin,
with 49 active linker wraps. The real CYD boots with four prerender files,
holds native MAIN stable at `heap8=54548 largest8=32756`, completes the full
intro and exact Entrance first frame `71ca7465`, then validates resident
MOVE/TURN, pickups, door, opcode-26 dialog and renderer recovery. The core graph
still allocates the dead `ParticleSystem_t` object at 2280 B; this is the next
retirement target on the same active branch.

See
[MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md](MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md).

### Runtime ZIP asset source retirement — REAL-CYD PASS (2026-09-30)

The ESP32 runtime now uses only `/DoomRPG-ESP32.pak` as its asset source.
`src/Z_Zip.c` is excluded from the ESP32 build and the final ELF contains no
ZIP parser or miniz decompression symbols.

```text
legacy desktop src/*.c units: 17 -> 16
active linker --wraps:        57 -> 57
static RAM:                   45776 B -> 45760 B
flash:                        815677 B -> 807377 B
runtime ZIP parser:           present -> absent
```

The real classic CYD validates cold boot, HUD/pre-render/Render/mappings from
PAK, Start Game intro assets from PAK, Sector1 -> Entrance SD-to-raw-flash
restaging, inverse V9 Load back to Sector 1, then live movement/monster combat.
Both gameplay sessions keep `shapeData == NULL` and `mediaTexels == NULL`.
Post-load ALIVE is stable at
`heap=94016 heap8=28400 largest8=16372`.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md](MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md)


Current consolidation result from merged main
`c37c66ad800603ea7d0622681a3bfaf5bab0b41d`:

```text
ESP32 translation units:        173 -> 173
active linker --wraps:          58 -> 57
EspNativeGameplayMonster wraps:  1 -> 0
static RAM:                     45776 B -> 45776 B
flash:                          815677 B -> 815677 B
```

The dead historical Retaliation compatibility translation unit remains gone.
Five formerly active native-to-native linker seams have now been retired or
made explicit:

1. `EspNativeGameplayMonsterPosition_prepareCardinalMove`:
   activation gating + publication capture now pass through
   `EspNativeGameplayMonsterMovementActivation_prepareCardinalMove`.
2. `EspNativeGameplayMonsterMovementProbe_reset`:
   reset ownership is explicit in MovementProbe with exact
   ThreeGoal -> Publish -> Movement -> Position ordering.
3. `EspNativeGameplayMonsterMovement_view`:
   three-goal continuations pass their synthetic movement view explicitly to
   `EspNativeGameplayMonsterMovementPublish_afterProbeWithView`.
4. `EspNativeGameplayMonsterTurn_postMoveGoal`:
   `MonsterTurn` owns ordinary-vs-three-goal dispatch explicitly and calls
   `EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal` only for subtype 4/13.
5. `EspNativeGameplayMonsterState_view`:
   the wrapper was pure one-shot `WITNESS/CENSUS` instrumentation, so the
   wrapper and its 119-line witness translation unit are deleted with no
   replacement API.
6. `EspNativeGameplayMonsterState_actionService`:
   HUB/automap framebuffer ownership now composes explicitly through
   `EspNativeGameplayHubActionGate_service`, which calls the unchanged
   MonsterState action-service chain only while world presentation is active.
7. `EspNativeGameplayMonsterTurn_view`:
   `MonsterTurn` remains the raw producer, while
   `EspNativeGameplayMonsterActivation_serviceTurn` explicitly flushes the
   deferred destructible-turn intent and builds the filtered cached view.
   AttackVisual, Retaliation and ordinary movement read the side-effect-free
   `EspNativeGameplayMonsterActivation_turnView`; ActiveSequence reads the raw
   producer directly.
8. `EspNativeGameplayMonsterMovement_service`:
   `MonsterMovementProbe_service` now calls
   `EspNativeGameplayMonsterActiveSequence_service` explicitly. The sequencer
   selects one active member at a time; `serviceMember` then invokes the normal
   Movement planner leaf and closes publication/post-move ownership before the
   next member is selected.

The latest real-CYD witness executes one four-member MOVE in activation order.
Sprites 218 and 237 commit ordinary movement; subtype-4 sprites 0 and 1 complete
their three-goal chains; the sequence closes with
`activeCount=4 delivered=4 ordered=yes publication=per-member`. Two following
ALIVE samples are stable at `heap=82704 heap8=17152 largest8=10228`.

There are now no active linker wraps whose target symbol begins with
`EspNativeGameplayMonster`.

A later real-CYD PASS_TURN also proves a genuine ranged monster attack after
the refactor: sprite 218 / subtype 3 / weapon 15 follows
`ATTACK-PROBE -> MONSTERACT -> MONSTERATKVIS -> MONSTERRETAL`, then commits
`playerHP=22->20 armor=12->10` only after the visual completes. Following
ALIVE samples remain stable at `heap=82704 heap8=17152 largest8=10228`.

That ranged attack emits no `RANGED-MEMBER`, confirming that
`RANGED-MEMBER` denotes exact-source ranged-AI movement/repositioning rather
than the actual attack presentation/resolution path.

See [MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md](MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md)
for the final monster-wrapper closure.

See [MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md](MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md)
for the exact regression history, CI artifacts and hardware witnesses.

The merged barrel milestone remains hardware-valid on the real CYD. A distant shot
proved a complete three-barrel causal chain: the root runs its 3-frame logical
180 explosion, discovers both cardinal neighbors, then both neighbors animate
together in one bounded second wave before their radius callbacks execute.
The user explicitly accepted the visual result.

A close-range run additionally reached native player radius damage twice and
then the intentionally unsupported lethal/death boundary. The final lethal
component produced `PLAYER-DEFER`, cancelled the requested monster turn and
rolled back player/RNG/world ownership exactly. The multi-blast aggregate damage
message did not get a successful commit in that run and is explicitly deferred
until healing/medkits make a clean nonlethal retest practical.

See `MILESTONE_NATIVE_BARREL_SUBTYPE1.md` for the exact hardware boundary.

Current rebased integration:

```text
main HUB = INV | WPN | STAT | SYS
WPN = complete 3x3 normal arsenal
SYS = two-step SAVE/LOAD/NO SAVE
touch feedback = 640 compact 4-byte edits + per-pixel ownership restore
agent checkpoint = V8 monster-state extension retained
event43 = bounded EV_SHOW x4 MOVE chain retained
CHECK_KEY = native PlayerState key gate retained
Codex fix = SHOW rollback lease released after pending dialog finalizes
boot fix = compact feedback owner restores menu.bsp contiguous heap
LOAD fix = session replacement bypasses ordinary HUB-close HUD exactness gate
SAVE fix = successful confirmation closes HUB and queues 1200 ms Game saved
SAVE fallback = permanent status, then current facing label, then empty
HUB close integrity = bottom HUD exact after live compass repaint; top bar recomposed by world redraw
HUB active surface = 160x100 at y=20..119; gameplay portrait strip hidden until close
RNG fix = core creation seeds the inherited 128-byte Random_t table once
```

Current continuation also has a real-CYD pass for native EV_CHECK_KEY on the Entrance Yellow Door. Opcode 41 uses the shared PlayerState key bitmask, reports Need Yellow Key for selector 1 / mask 0x02, queues bounded top-bar feedback, and pauses before the following OPENLINE with zero world/script mutation when the key is absent.

Entrance tile 377 / event 43 is no longer a progression blocker. Hardware proved the exact bounded MOVE chain EV_SHOW x4 on ENTER followed later by EV_CLOSELINE 102 on EXIT. The SHOW journal is a single 192-byte static owner; the public MOVE result is 76 bytes and carries no batch-sized stack payload. Preflight applies+reverse-rolls-back the four SHOWs before MOVE commit, and the rendered destination frame closes the rollback lease only after success. Exact serial witnesses are in PORTING_STATUS.md and MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md.

Current hardware-proven addition on this branch:

```text
native facing-entity top-bar label
 -> compact 34-byte current-target owner
 -> no legacy Entity_t pointer
 -> no resident table of entity names
 -> on-demand /entities.db name read through DoomRPG-ESP32.pak
 -> recovered short forward trace: +31-unit origin, 3 tile steps
 -> sprite + line-entity targets
 -> type-9 blocker remains label-hidden
 -> topbar fallback below timed feedback and statBarMessage
```

Real-CYD witnesses:

```text
Civilian : sprite target, distance=1
Computer : line target, distance=2 then 1
Door     : line target, distance=3
none     : exact label clear after rotation
HUB close: current Civilian target retained and repainted
```

Pure TURN retargeting remains non-turn gameplay:

```text
[MONSTERTURN] ROTATE-NO-TURN ... legacyAdvance=no
```

The supplied hardware run remained alive with:

```text
heap=81936
heap8=16384
largest8=11764
```

Latest relevant milestones:

- [`MILESTONE_NATIVE_BARREL_SUBTYPE1.md`](MILESTONE_NATIVE_BARREL_SUBTYPE1.md)
- [`MILESTONE_MAIN_MENU_FINGER_FIRST.md`](MILESTONE_MAIN_MENU_FINGER_FIRST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md)
- [MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md](MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md)
- [MILESTONE_NATIVE_CHECK_KEY_YELLOW_DOOR.md](MILESTONE_NATIVE_CHECK_KEY_YELLOW_DOOR.md)
- [MILESTONE_NATIVE_FACING_LABEL.md](MILESTONE_NATIVE_FACING_LABEL.md)

Previously merged relevant milestones remain:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md)
- [`MILESTONE_NATIVE_AUTOMAP.md`](MILESTONE_NATIVE_AUTOMAP.md)
- [`MILESTONE_NATIVE_PICKUP_FEEDBACK.md`](MILESTONE_NATIVE_PICKUP_FEEDBACK.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)
- [`MILESTONE_NATIVE_RESIDENT_GAMEPLAY_POLISH.md`](MILESTONE_NATIVE_RESIDENT_GAMEPLAY_POLISH.md)

The final SYS SAVE close regression is also hardware-closed at
`a5b30a12b4bb51cd4f016d53212b74e967c19d6d`: when the player's live
orientation differs from the retained base-HUD angle, HUB close repaints the
base HUD and then reapplies only the bounded compass dirty rectangle from the
settled `EspPlayerViewState`. The real CYD produced `exactBottom=yes`,
`SAVE-CLOSE`, `Game saved` for 1200 ms and then the current `Door` facing
label. CI #767 reports 45096 B static RAM and 782141 B flash.

### Current progression boundary — Junction -> Sector 1 REAL-CYD PASS

The current branch did not complete its original large structural-consolidation
goal. Its durable result is a sequence of generic correctness fixes discovered
while extending real progression.

Current real-CYD boundary:

```text
/junction.bsp  = Junction, map 9, gameplayLoadMapId 2
/level01.bsp   = Sector 1, map 2, gameplayLoadMapId 3
Sector 1 spawn = tile 477, dir 192
```

The user has validated zero-enemy PASS_TURN, V9 SAVE and V9 LOAD in Junction,
the direct `showStats=0` transition into Level01, first movement away from the
Sector-1 spawn, a second movement after rotation, monster activation, and
ordered live monster movement.

Generic coverage added on the way:

```text
SAVEGAME route != CHANGEMAP target is valid
transition door -> READY direct handoff for showStats=0
fresh-map spawn owns all four cardinal directions
spawn tile may execute EV_FORCEMESSAGE transactionally
mixed MOVEEVENT may include EV_FORCEMESSAGE with state/show/lock/door families
```

Post-review, the independent SAVEGAME route is copied into a SAVE-owned bounded
state before the transition session reset. Real-CYD Sector 1 SAVE proves that
`/junction.bsp / 416,1824 / 192` survives the Junction -> Sector 1 handoff,
and the following V9 LOAD returns to a ready Sector 1 gameplay session. Current
V9 bytes are unchanged; LOAD clears the non-serialized live route to avoid
cross-session leakage.

Fresh-spawn facing also mirrors legacy `Game_trace()` map-edge behavior by
clamping source/destination tile components to `[0,31]`. The exact near-edge
spawn case is build/CI-valid but was not exercised in the supplied real-CYD
trace, so that narrow edge behavior is not yet a hardware claim.

Current post-review hardware-tested code boundary:
`7d64839a31376c4ca0a3ec4f0f5ae10395b04635`, esp32-cyd CI #1009,
45784 B static RAM / 816517 B flash.

See [MILESTONE_NATIVE_JUNCTION_SECTOR1.md](MILESTONE_NATIVE_JUNCTION_SECTOR1.md).

### Current transition milestone

The native Entrance -> Junction level exit is real-CYD validated on final code
head `e4acb92403dd48810f3c4e989d4dee16605125e3`, rebased directly on
`main@6cd8b6804cbec75538becab0d6cbe66e3c79d238`. CI #829 succeeds in the
normal `esp32-cyd` environment with 45200 B static RAM and 789345 B flash.

The tested route owns the exact Entrance event-1
SAVEGAME/CHANGEMAP/OPENLINE sequence, the WAIT_STATS one-tap bridge,
requested-map raw-flash rebuild, Junction compact-runtime reconstruction,
spawn/session re-arm, and the first committed Junction step with ENTER
`EV_MESSAGE "Junction"`.

The same final hardware trace continues into ordinary Junction gameplay and
proves one complete native NPC dialog continuation: Scientist tile 878 /
event 56 opens opcode-8 DIALOG, supports fast-forward/page advance, closes with
the pack released, then resumes at command offset 1 and commits opcode 11
`CHANGESTATE` with `stateMutation=1` before a successful world redraw.
Dialog/NOTE/continuation filtering on this code head now carries the live shared
PlayerState key context instead of silently revalidating with `keys=0`. The
earlier Marine event 45 failure that exposed this mismatch was not replayed in
the final supplied trace, so only the event-56 hardware witness is claimed.

Post-PASS review also closed a render-failure-only transaction leak: after a
successful SELECT-door world/script rollback, a staged CHANGEMAP door owner is
now aborted before the rollback frame is presented. This does not alter the
hardware-proven happy path and that failure-only cleanup path was not
hardware-triggered.

Detailed record:

- [`MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md`](MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md)

The barrel aggregate-damage-message retest remains a separate UI frontier; its
historical code boundary stays `1b93651699d981e34b2a10318936ddfa0cf7b2e8`.

## Current continuation — real-CYD validated

The branch is rebased directly on `b77513309a38a970a5d59195ce76424a3f44a7cb` and the code head
`aa32270adbb22de6666c3ad45c5d63c88fc34db4` is the real-CYD validation boundary.

Two gameplay/progression fixes are now hardware-proven:

```text
ordinary monster attack
 -> animation lease first
 -> world input blocked during the lease
 -> retaliation HP/armor mutation only after animation COMPLETE

Entrance Yellow Key trap event 74
 -> mixed state + SHOW + line lock/open batch
 -> later fully-satisfied traversal may be mutation=no / rollback=0
 -> no rollback owner retained for that no-op
 -> later movement/combat remains enabled
```

The user still perceives a broader slowdown. Do not compensate by shortening
individual dirty-animation timings in isolation. Current logs show many complete
world frames around 190-240 ms, occasional heavier recovered-render frames above
300 ms, and physical 160x120 -> 320x240 presentation around 34.5 ms. Treat
performance as a separate system-level profiling milestone covering render
cadence, PAK I/O/diagnostics, repeated redraws and presentation.

Latest relevant records:

- [MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md](MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md)
- [MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md](MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md)

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`. Bring-up diagnostics perturb RAM and are not the production memory canon. Never claim a local build or hardware pass that did not occur.

The production environment uses:

```text
board_build.partitions = partitions_cyd_raw_pak.csv
```

## Hardware / permanent memory rules

```text
classic CYD = ESP32-2432S028R
MCU = ESP32-D0WD-V3 dual core 240 MHz
flash = 4 MB
PSRAM = none
logical framebuffer = 160x120 RGB565 = 38400 B
shapeData == NULL
mediaTexels == NULL
```

Do not recreate map-wide texel ownership, pointer-heavy desktop world graphs or native runtime ZIP dependence. `/DoomRPG-ESP32.pak` remains the native asset source/backing store.

## Current native asset backing

Hardware-proven active gameplay path:

```text
/DoomRPG-ESP32.pak on SD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native renderer/gameplay
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Entrance storage witness:

```text
pack=2457398 B
entries=241
index=4820 B
metadata=12288 B
excluded other BSPs=12 / 203811 B
staged payload=2248743 B
partition=2752512 B
headroom=491481 B
indexFNV=3a51cc4d
payloadFNV=9ec04e22
```

## Entrance canonical format witness

```text
map=1
resource=/intro.bsp
sourceBytes=21823
sourceCRC32=623f34e4
sourceFNV=d5cc751f
runtime arena=14095 B
runtimeFNV=c3882516
resident payload=17891 B
spawn tile=904
spawn position=544,1824
spawn direction=64
nodes=223
lines=480
sprites=344
events=93
byteCodes=265
strings=94
native topology entities=220
enemies=30
destructibles=13
```

Canonical fresh-map fingerprints:

```text
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
```

The V3 hardware mirror checkpoint deliberately captured a mutated script owner at `scriptFNV=f9e59e9f`; that checkpoint fingerprint must not replace the fresh-map canonical `f9e3d9df` above.

Resident cache baseline:

```text
owner=23592 B
payload=19456 B
range records=288 x 12 B
resident entry slots=24
large exact range=2048 B
```

## Current native gameplay frontier

The real-CYD-owned engine includes native movement/collision, rotation-in-place without gameplay/monster turn advancement, event-first SELECT, bounded event/script execution, dialog, dynamic doors/lines including pure multi-line SELECT door batches, mutable line textures, shared PlayerState, pickups/resources, hazards, native weapon rendering/control/combat, type-12/subtype-2 crate combat with exact transform RNG and transformed-pickup projection, monster state/position/activation/movement/attack families, raw-flash requested-map backing, the four-page HUB `INV/WPN/STAT/SYS`, bounded checkpoint save/load from both HUB and the main menu, resource consumed-overlay persistence, script/event-state persistence, line open/locked + texture-variant persistence, V5 action-owned removed-sprite persistence, hardware-proven checkpoint-resume HUD/cache/input rearm, and HUB/world feedback framebuffer ownership gating.

Player/HUB compact roots:

```text
EspNativeGameplayPlayerState = 52 B
EspNativeGameplayHubView = 28 B
```

### HUB

```text
pages = INV | WPN | STAT | SYS
tab labels = native 5x7; touch geometry remains 38x13 logical
active surface = 160x100 logical at y=20..119; lower gameplay HUD hidden
INV = four-row direct-touch window; Notebook + carried items + owned keys
WPN = complete 3x3 normal arsenal
STAT = compact read-only dashboard; only its tab is touch-active
SYS = two-step SAVE/LOAD checkpoint page; SAVE success returns to gameplay
MENU underlay = 32x20 RGB565 = 1280 B
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

INV projects Notebook, the five carried-item slots and owned keys in a derived four-row
scrolling window. A tap on any populated row moves the selection directly; the
selected row keeps the amber rail and item counts use compact `Xn` labels. No
scroll owner or duplicated list is retained. Weapons remain in WPN and Credits
remain in STAT; keys intentionally appear both as inventory objects and as the
compact STAT summary. Notebook opening and item use are still
intentionally read-only until their native modal and exact turn/effect
transactions are implemented.

WPN is the 3x3 direct-touch normal arsenal grid; familiar IDs 9..11 remain
excluded. STAT is read-only: its content uses compact 3x5 labels with
intermediate 5x7 HP/Armor values, proportional green/red health and blue armor
rails, level/XP progress, an aligned attribute grid, and owned-key mini-cards in
green/true-yellow/blue/red instead of the internal hexadecimal bitmask; no
content hitboxes are introduced. SYS owns the bounded in-game SAVE/LOAD
controls. All four pages use the reclaimed logical rows 100..119 while the HUB
is active. On ordinary close, the full retained gameplay HUD plus live compass
are rebuilt before the existing exact lower-band integrity check; LOAD remains
a deliberate whole-session replacement. After the second SAVE selection
succeeds, the HUB closes immediately and gameplay shows `Game saved` for about
1200 ms. Expiry recomposes the permanent status-message fallback, then the
current facing-entity label, then an empty bar. The main menu exposes a separate
LOAD-only entry through the same checkpoint service. The four-row INV layout
builds at 45224 B static RAM and 796045 B flash. The full-height HUB, direct
four-row touch selection, owned-key projection (including the yellow card),
compact STAT presentation and ordinary HUD restoration have passed a focused
real-CYD visual/touch check.

## Main-menu Load Game — REAL-CYD PASS

```text
Start Game
Load Game
Options
Help/About
```

The ESP32 presentation replaces the obsolete J2ME Exit row and keeps the existing double-tap confirmation contract. The original main-menu LOAD milestone was hardware-proven at 18c1cfb for the then-current checkpoint formats: a successful read released menu-only runtime, restored the native world and entered ST_PLAYING with intro replay disabled; missing/invalid saves remained in MENU_MAIN with red No Save.

The `Options` child now shares the main menu's 2x2 finger-first card renderer
(`BACK | VIDEO` / `INPUT | SOUND`) instead of reverting to legacy text rows.
Only `BACK` is enabled in the current model; the deferred cards are visibly
subdued. Back keeps the two-tap contract and uses the runtime framebuffer hash
returned by the painter, so presentation changes no longer require a duplicated
hard-coded framebuffer constant. The shared dashboard, deferred-card styling
and two-tap Back route have passed focused visual/touch testing on the real CYD.

The later V8 cold-load regression is no longer current. Catalog-independent
header validation fixed the false `No Save`, and both valid cold resume and the
genuine no-save response were revalidated on the real CYD. The exact regression,
fix and witnesses remain recorded in `MILESTONE_MAIN_MENU_LOAD.md`.

## Native checkpoint save/load

### V1 core — hardware proven

```text
/sd/DoomRPG-ESP32.sav
DRPGSAV1
version=1
recordBytes=132
```

The v1 semantic core persists:

```text
saved BSP sourceBytes + sourceCRC32
immutable runtime FNV
map/load identity
full settled EspPlayerViewState
full EspNativeGameplayPlayerState
player FNV
record CRC32
```

Atomic write contract:

```text
temp write -> reread/validate -> old primary to backup -> temp to primary -> reread/validate
```

The final LOAD path rebuilds the BSP from native PAK data, verifies immutable identity, restores player/pose, then recreates the two semantic HUD reprime owners before configuring the gameplay session.

### V2 resource section — REAL-CYD PASS

V2 adds one bounded pointer-free resource snapshot to the proven v1 core:

```text
DRPGSAV2
version=2
recordBytes=276
v1 read compatibility=retained
max consumed payload=128 B
max represented sprites=1024
Entrance sprites=344
Entrance used consumed bytes=43
```

The section stores runtime/map identity, sprite count, consumed count/byte count, and the consumed bitset. It never persists the heap-owned `ResourceOwner` itself.

The real-CYD behavior proved both directions:

```text
pickup consumed before SAVE -> remains absent after LOAD
pickup consumed after SAVE -> reappears after LOAD
```

Armor Shards and a Small Medkit exercised that path through normal native topology/rendering.

### V3 script section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV3
version=3
recordBytes=808
v1/v2 read compatibility=retained
```

V3 keeps the v1 core plus v2 resource section and appends exactly one `EspMapScriptStateSnapshot`. The section is pointer-free and bounded:

```text
ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES=512
Entrance events=93
Entrance byteCodes=265
Entrance event-state bytes=47
Entrance removed-command bytes=34
Entrance script storage=81 B
```

The payload stores:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
packed event states + removed-command bits
```

Restore is performed only after fresh immutable runtime reconstruction. It validates the runtime FNV, event/bytecode counts, exact packed sizes and zero tail, copies into the existing compact owner without allocation, and checks the post-restore semantic fingerprint before session configure.

Final mirror SAVE witness after two Armor Shards, the Small Medkit and relevant event/script mutations:

```text
[NATIVESAVE] SAVE ... version=3 bytes=808
             map=1 gameplayLoadMapId=1
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
```

Gameplay then diverged by taking a Bullet Clip and Fire Ext, reaching:

```text
playerFNV=a6e115a7
weapon=1
weapons=0006
ammo0=10
ammo1=12
consumed resources=5
```

LOAD restored exactly the checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[NATIVESAVE] LOAD ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

The semantic mirror proof is stronger than the checksum alone. Event 79 had already advanced before SAVE, and after LOAD it remained ineligible:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

Combined with the earlier opposite-direction test where post-SAVE script mutations were rolled back and became executable again:

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Detailed records:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

### Historical V3 save-world boundary (superseded by V4)

Persisted now:

```text
settled player pose
EspNativeGameplayPlayerState
player-resource consumed overlay
EspMapScriptState event states + removed-command bits
```

Still fresh after LOAD:

```text
line open/locked state and texture variants
automap reveal state
monster state/position/activation/combat consequences
destructibles
gameplay RNG
```

Continue one owner at a time; never dump a raw runtime/legacy object graph.

The long-session V3 hardware run stabilized at:

```text
heap8=17724
largest8=8692
```

That remained stable across the final LOAD/replay segment. Fragmentation/headroom is still below the advisory target and remains a review item, but the run does not show a new per-LOAD leak.

### V4 line section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV4
version=4
recordBytes=1212
v1/v2/v3 read compatibility=retained
```

V4 appends one bounded `EspMapLineCheckpointSnapshot` to the proven v1/v2/v3 sections. Entrance uses 60 bytes per line bitset for 480 lines. The persisted semantics are:

```text
open bits
locked bits
mutable locked/unlocked texture-10 variant bits
line/runtime identity + semantic fingerprints
```

The real-CYD load rebuilt canonical Entrance first, then restored:

```text
[MAPLINECHECKPOINT] RESTORE ... open=0 locked=6 texture10=1
                    lineFNV=69334d90 textureFNV=bda09634
[NATIVESAVE] LOAD ... version=4 bytes=1212
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
```

The soldier-controlled door at line 352 then opened with `locked=0` without replaying the soldier unlock script, proving the saved unlock and matching texture state survived LOAD.

The first V4 hardware attempt triggered a `loopTask` stack canary while entering STAT. The final fix moved the large checkpoint read workspace to bounded static storage and removed unnecessary full-record stack copies. Hardware now reaches the STATUS SAVE/LOAD UI, performs V4 LOAD, reprimes the session and continues gameplay without reset.

Post-LOAD invariants:

```text
shapeData == NULL
mediaTexels == NULL
heap8=14076
largest8=6644
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)

### V5 action-removal section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV5
version=5
recordBytes=1356
v1/v2/v3/v4 read compatibility=retained
max removed payload=128 B / 1024 sprites
Entrance removed bytes=43 / 344 sprites
```

V5 appends one pointer-free `EspNativeGameplayActionRemovedSnapshot` containing
runtime/map identity, sprite count, removed count/bytes, semantic FNV and the
removed-sprite bitset. Restore validates all identity/count/FNV/tail invariants
and mutates only the compact action-engine removal owner.

Hardware SAVE witness after clearing fire sprite 74:

```text
[ACTIONENGINE] FIRE-COMMIT ... sprite=74 ammo=10->9 ...
[NATIVESAVE] SAVE ... version=5 bytes=1356
             pos=288,1248 angle=0
             playerFNV=15cb16e4
             resources=5/43B
             script=93/265/81B/26f291e3
             lines=480/60B/open1/locked6/tex101/c50b0721/bda09634
             actionRemoved=1/43B/a54be373
```

Hardware LOAD witness:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[NATIVESAVE] LOAD ... version=5 bytes=1356
             pos=288,1248 angle=0
             actionRemoved=restored/1/43B/a54be373
[ENGINESESSION] RESUME checkpoint=restored freshFirstFrame=skipped dynamicLines=gameplay-wrapper
[DYNAMICLINES] FRAME angle=0 open=1 ... render=ok immutableRuntime=yes
[RESIDENTGAMEPLAY] READY map=current entry=checkpoint-resume ...
[ENGINESESSION] READY map=1 angle=0 ... TURN+MOVE=armed
```

The real-CYD visual result matched the semantic snapshot: the fire cleared before
SAVE remained absent after LOAD, while another fire that was never cleared
remained lit.

The hardware investigation also established the permanent checkpoint-resume
ordering. A restored checkpoint does not replay the historical fresh-map first
frame. It restores settled HUD owners, uses the production gameplay renderer to
prime caches (and therefore dynamic lines), then one-shot admits resident
gameplay after HUD + resident cache + large cache are ready. Fresh startup still
requires the normal first-frame witness.

The V5 memory correction shares one static read/write save workspace and streams
V5 write verification in a 64-byte compare buffer. This recovered startup DRAM
after the first V5 candidate caused an OOM/reset while loading mappings.

Exact final code/CI boundary:

```text
d65e5b9be9947e92c700b2296790b003ff7b7df0
esp32-cyd #387 / 35704985512 = SUCCESS
REAL-CYD = PASS
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

### Current save-world boundary

Persisted now:

```text
settled player pose
EspNativeGameplayPlayerState
player-resource consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
EspNativeGameplayActionEngine action-owned removed-sprite overlay
```

Still fresh after LOAD:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
full entity/sprite dynamic state and transformed definitions
destructible transformed state such as crate -> pickup
power-coupling health/death globals
persistent GSprites
ceiling/floor color
other legacy player metadata not yet owned natively
```

Gameplay RNG is rebuilt fresh, but the recovered original save format does not
serialize RNG state, so this is not a missing-vs-original save field.

Continue one owner at a time; never dump a raw runtime/legacy object graph.

## HUB/action-feedback visual ownership fix — REAL-CYD PASS

During the save-v2 hardware test, a pickup message could expire while HUB owned the framebuffer. The original bug left a stale `Got ...` fragment over the MENU area and could produce:

```text
[HUB] CLOSE ... menuUnderlayRestore=FAILED ... exactHud=NO
[RESIDENTGAMEPLAY] HUB-RECOVER ...
```

The branch adds a bounded ownership gate so action-feedback / viewport-flash expiry does not restore world pixels while HUB owns the framebuffer:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
CI #267 SUCCESS
```

The timer still uses real elapsed time and resumes after HUB closes. The user reproduced the original pickup-message/HUB scenario on the real CYD and confirmed the stale fragment has disappeared. This fix is hardware-valid at `5a1020fd5d160c111ff09ecb8a480f37ea8d0578`.

## Entrance -> Junction CHANGEMAP — REAL-CYD PASS

Hardware-tested happy-path boundary:

```text
code SHA = 2b7c4dcf6d0d00abf176b797b8fb335c3f332a61
CI #807 = SUCCESS
RAM static = 45200 B
Flash = 787865 B
artifact id = 10869413761
```

Current rebased code boundary:

```text
main = 6cd8b6804cbec75538becab0d6cbe66e3c79d238
code head = 455e1da6032d9b9086a00e39d338d3218f8b58f4
```

Recovered Entrance event:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> line 459, regular four-frame door
```

Target BSP inventory is read through one exact authoritative-SD source-probe
lease while the active Entrance raw-flash gameplay backing remains untouched.
After the explicit WAIT_STATS acknowledgement, map-flash preparation correctly
MISSes world identity from map 1 to map 9, rebuilds the Junction slot, and only
then releases the source runtime. Junction rebuilds from raw internal flash,
spawns at tile 943 / position 992,1888 / angle 64, primes the resident cache and
reaches the generic gameplay service.

The first Junction movement is also hardware-proven. The locked EXIT
`CLOSELINE` on tile 943 is a non-blocking legacy no-op; the move commits to tile
911 / position 992,1824, the destination frame renders, and only then opcode-4
`MESSAGE` publishes `"Junction"`.

Post-PASS code review closed a failure-only rollback leak: if the shared
`SELECT-DOOR` render fails for the staged transition door, the native code now
rolls back the line/script mutation, aborts the `waitingDoor` transition owner,
then renders the restored frame. This keeps the exit retryable. The successful
hardware path is unchanged; that failure path was not hardware-triggered.

The real statistics screen is still deferred: the current proof includes the
one-tap WAIT_STATS bridge, not final stats presentation. Generic Junction ->
Level01..Level07 transitions and unrelated MESSAGE routes are not implied by
this PASS.

Detailed record:

- [`MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md`](MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md)

## Rotation no-turn parity — REAL-CYD PASS

Legacy/J2ME parity is restored at code boundary:

```text
b548321f477626777800371f0f82a9f3c2375bd9
```

`DoomCanvas_finishMovement()` advances the gameplay turn; `DoomCanvas_finishRotation()` does not. The native observer now mirrors that split: angle-only changes update the settled-view baseline but do not schedule monster AI.

The user confirmed the corrected behavior on the real classic CYD. The supplied hardware run also reconfirmed normal turn scheduling for actual gameplay actions:

```text
reason=PLAYER_ATTACK -> scheduled
reason=MOVE          -> scheduled
```

Detailed record:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)

## Preferred next milestone

After this branch merges, recover the exact new GitHub `main` SHA before any
new branch is created.

Resume the structural consolidation objective: reduce ESP32 translation-unit
sprawl and native->native linker wrappers by permanent domain, then reduce
legacy implementation dependencies family by family. Preserve the now
hardware-proven Entrance -> Junction -> Sector 1 progression, Junction
zero-enemy checkpoint semantics and Level01 first movement as regression guards.

## Secret-door multi-line SELECT transaction

The hidden door on `/intro.bsp` is a useful legacy case because one SELECT event owns two separate line mutations rather than one. Hardware recovered the exact event as tile `195`, event `10`, with two eligible opcode-15 (`EV_OPENLINE`) commands targeting lines `471` and `470`.

The permanent native SELECT rule is therefore not “exactly one door command”. It is:

- the eligible event must remain a pure line-command family;
- the complete batch is preflighted before the first mutation;
- at most eight lines are allowed, matching `openDoors[8]`;
- commit occurs in event order with exact line/script removed-bit rollback available;
- mixed opcode families and unsupported sequential duplicate-line semantics remain fail-closed.

The real CYD produced:

```text
[ACTION] DOOR-BATCH event=10 count=2 status=OK [0]line=471/op=15/open=0->1/removed=0->1 [1]line=470/op=15/open=0->1/removed=0->1
[DYNAMICLINES] FRAME angle=64 open=4 ... render=ok immutableRuntime=yes
```

Both lines reported `DOORANIM SNAP` because their immutable flags do not mark regular animated doors. The visual/world result was correct: the secret door opened, the player crossed it, and later movement over tile `195` reported `NO_ELIGIBLE`, proving the remove-if-handled state was committed for both commands.

See `MILESTONE_NATIVE_SECRET_DOOR_BATCH.md` for the exact hardware evidence and transaction boundary.

## Intro display-fit contract

The classic CYD uses a 160x120 logical framebuffer while the original intro composition assumes a 128x128 story space. Hardware review established that one fit rule does not look correct for every intro element.

The permanent split is:

- ordinary story-page image geometry: centered 120x120 aspect-preserving viewport;
- scrolling starfield: full 160x120;
- dedicated animated story scene: full 160x120;
- narrative glyphs: centered 156x120 soft-wide mapping;
- no intermediate framebuffer.

This keeps the animation visually full on the 4:3 CYD while avoiding the slightly over-stretched text produced by a full 160-pixel text mapping. The real-CYD visual result on `0d21332bd2524bca73d5284f70e053ca8ba6430d` was explicitly accepted as correct.

See `MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md` for the exact geometry and hardware boundary.

## Native transition presentation component

`EspNativeTransitionPresentation` is the shared full-screen owner for native
CHANGEMAP/fresh-start loading and checkpoint LOAD. Checkpoint restore itself has
one implementation: MENU_MAIN and the in-game SYS HUB both enter the same
`EspNativeGameplaySave_loadCheckpoint()` / restore/session-prime pipeline.

The logical 160x120 framebuffer is shared. Suppressing gameplay presents is not
enough to preserve a retained loading image because gameplay painters may still
write into that framebuffer. Therefore every visible progress update rebuilds
the complete `c.bmp` starfield + loading card immediately before the real
physical present. No second framebuffer or PSRAM is used.

Checkpoint progress is currently paced through:

```text
10 CHECKPOINT
30 BSP
60 RUNTIME
75 STATE
85 RESTORE
90 CACHE-COLD
93 CACHE-WARM
96 CACHE-LEARN
100 READY
```

The final handoff is intentionally asymmetric: `READY 100%` owns the LCD,
`FINAL-READY` reconstructs the gameplay world in the logical framebuffer,
loading is released without a present, the retained gameplay HUD repaints both
top and bottom bands, and only then is the complete gameplay frame published.
This prevents both stale loading pixels in the lower HUD and gameplay/HUB
contamination during loading.

For in-game LOAD, the old gameplay/HUB session is torn down before
`beginLoading()`. The cache witnesses also accept an already-warm resume; cache
replacement details such as zero new LARGE-LEARN stores or eviction of learned
large entries are diagnostics, not functional load failures.

Both checkpoint entry contexts are explicitly hardware-validated on the real
classic CYD at code boundary
`6903431a60700960127be95d95584728698a36c8`.

See [MILESTONE_NATIVE_TRANSITION_PRESENTATION.md](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md).

## Recent milestone index

- [`MILESTONE_NATIVE_TRANSITION_PRESENTATION.md`](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md)
- [`MILESTONE_NATIVE_BARREL_SUBTYPE1.md`](MILESTONE_NATIVE_BARREL_SUBTYPE1.md)
- [`MILESTONE_NATIVE_AUTOMAP.md`](MILESTONE_NATIVE_AUTOMAP.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)
- [`MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md`](MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md)
- [`MILESTONE_NATIVE_SECRET_DOOR_BATCH.md`](MILESTONE_NATIVE_SECRET_DOOR_BATCH.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V6_CRATE_TRANSFORMS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V6_CRATE_TRANSFORMS.md)

- [`MILESTONE_NATIVE_CRATE_SUBTYPE2.md`](MILESTONE_NATIVE_CRATE_SUBTYPE2.md)

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

- [`MILESTONE_NATIVE_PLAYER_HIT_FEEDBACK.md`](MILESTONE_NATIVE_PLAYER_HIT_FEEDBACK.md)
- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)
- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)

## Current intentionally incomplete families

See `PORTING_STATUS.md` for the authoritative list. Important current boundaries include:

```text
save-v6 mutable-world sections beyond each validated owner
CHANGEMAP/script shapes beyond hardware-proven Entrance -> Junction -> Sector 1
native player lethal/death transition
barrel aggregate multi-blast damage-message hardware retest / proper HUD queue
remaining barrel radius-hurtable families beyond barrel + player
audio
password late presentation cleanup replay
GIVEMAP hardware execution + remaining Automap action parity
Automap reveal-state checkpoint persistence
CHECK_KEY production route
HUB Notebook activation / consumable use / Options / store
remaining advanced combat/monster/special-death families
```

## Development workflow

```text
recover true main + docs
 -> choose one bounded native owner/family
 -> recover exact legacy behavior where relevant
 -> design a permanent compact API
 -> keep unrelated families fail-closed
 -> commit + push agent/*
 -> build esp32-cyd in CI
 -> test on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

Never merge into `main` without explicit user request.


## Checkpoint monster spatial state: V8 compatibility and V9 owner

Hardware-tested boundary:

```text
7b3efeb8d590c027b94f08ac7c31c886938709d4
esp32-cyd CI #980/#981 = SUCCESS
```

V8 checkpoints contain logical monster state and script state, but not the
mutable sprite topology/position state created by one-shot `SHOW/HIDE`
commands. On LOAD, the native compatibility path therefore reconstructs only
those topology effects that the V8 script snapshot proves were already
successfully consumed:

```text
removed-command bit == 1
+ REMOVE-if-handled
+ opcode SHOW/HIDE
```

This is intentionally narrower than replaying arbitrary scripts and does not
guess historical monster movement. The real classic CYD confirmed that monsters
revealed by the yellow-card trigger are present again after loading the existing
V8 save.

The final V9 record owns compact monster state/position/activation plus topology
for both enemies and destructibles. This matters because EV_SHOW can remove
either an enemy blocker or a deterministic destructible before linking its
target. Enemy blocker death is reconciled only in the checkpoint copy of
MonsterState, without synthesizing gameplay side effects; destructible topology
is serialized directly.

Real classic CYD proof at
`7b3efeb8d590c027b94f08ac7c31c886938709d4`:

```text
SAVE: topology=43, monsters=30, positions=30
LOAD: tracked=43, enemies=30, destructibles=13,
      scope=enemy+destructible-v9, exact=yes
READY: shapeData=0x0, mediaTexels=0x0
```

The exact V9 SAVE/LOAD spatial round-trip is therefore hardware-validated for
this bounded owner set.

See
[`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md).

## Main-menu semantic dispatcher

The classic-CYD main menu now owns selection semantically rather than routing
through the desktop/J2ME generic SELECT chain.

```text
2x2 touch gate
 -> native semantic dispatcher
    START   -> always new game
    LOAD    -> native checkpoint resume
    OPTIONS -> bounded options dashboard
    HELP    -> bounded paged help
```

The produced ESP32 ELF no longer contains `Menu_select`,
`MenuSystem_select`, `Menu_initMenu` or `Menu_LoadHelpResource`.
`MenuSystem_t` remains a temporary bounded storage container, but construction
of the retained pre-game MAIN/CONTINUE/OPTIONS models is now owned explicitly by
`native_main_menu_model.c`; Help continues to use its PAK-backed
size/item/line-bounded native parser. No generic native menu factory replaces
the retired legacy switch.

Help is now a native opaque list viewer with eight visible lines and a permanent
BACK / UP / DOWN footer. The current styling is functional rather than final;
later visual polish does not require restoring generic legacy menu rendering.

A source audit removed the obsolete START "Continue" branch: it depended on
`Game_checkConfigVersion()`, which checks the legacy desktop files
`Config/Player/Player2/World` and cannot identify the native V9 checkpoint.
START is new-game-only; LOAD exclusively owns resume.

A shared `native_main_menu_present` owner now centralizes framebuffer hashing,
graphics-boundary validation and MENU_MAIN failure recovery. Recovery rebuilds
and repaints the main dashboard and re-arms touch only while ST_MENU still owns
the UI. LOAD reports typed NO_SAVE / RECOVERED / TRANSITIONED / FATAL outcomes
instead of treating every failure as a normal stay-main condition.

Hardware-tested boundary:
`cd5f24dd0ff538a85cebc02025de48cb5998401e` (CI #1135/#1136).

See
[`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md).

## Main-menu native Back action

The bounded pre-game main-menu children now return to MENU_MAIN without the
desktop/J2ME `MenuSystem_back()` hierarchy router.

```text
HELP / OPTIONS child
 -> validate expected child + ST_MENU
 -> direct legacy-equivalent Back cue 5042
 -> native MENU_MAIN model
 -> opaque main repaint
 -> main touch re-arm
 -> exact FNV/model/graphics invariants
```

Both HELP and OPTIONS are hardware-proven on the real classic CYD at
`8d9b6d25f481d76fe4c79bc1af99d59bc314328f`. Each restored the exact main
framebuffer FNV `522dc605`, rearmed touch, retained
`shapeData == NULL` / `mediaTexels == NULL`, and kept heap stable.

The linked ESP32 image no longer contains `MenuSystem_back`.
`MenuSystem_setMenu` remains linked because other retained legacy flows still
own broader menu/state transitions; it is deliberately not hidden behind a new
generic native wrapper.

See
[`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md).



## Legacy monolithic init anchor retired

The ESP32 build no longer keeps the inherited `DoomRPG_Init()` call graph alive
just so a startup diagnostic can print its address.

The old diagnostic closure was:

```text
DoomRPG_engineLinkAnchor()
 -> DoomRPG_Init
 -> DoomCanvas_setupmenu
 -> MenuSystem_setMenu
```

The real runtime already uses the staged ESP32 owners
`DoomRPG_initEngineCore()`, `DoomRPG_startEngineLayout()`, explicit startup
bridges, the native main-menu dispatcher and the native intro/gameplay session.
No generic replacement router was added.

At hardware-tested head
`f9ab3bda2cd9aadee2d1be0fc08d49600b7a9141` / CI #1159, the final ELF contains
none of `DoomRPG_Init`, `DoomRPG_engineLinkAnchor`,
`DoomCanvas_setupmenu` or `MenuSystem_setMenu`. Static RAM remains 45768 B;
linked flash and firmware both shrink by 3728 B from the V9 artifact.

The real classic CYD revalidated HELP/OPTIONS Back and a complete START path
through intro disposal, Entrance resident loading/cache priming and
`ENGINESESSION READY`. Final ALIVE samples are stable at
`heap=92492 heap8=26824 largest8=18420`.

See
[`MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md`](MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md).


## Main-menu source/model consolidation

Hardware-tested head `8cd0b019ebcb80491a27c2ed0bad3cf57f9ad47e`
completes two related cleanup steps:

1. `native_main_menu_*.c` is consolidated from 13 source files to 7 without
   adding replacement wrappers.
2. fixed pre-game model construction moves into the existing
   `native_main_menu_model.c`, allowing the final linker to remove the broad
   desktop `Menu_initMenu()` factory and `Menu_LoadHelpResource()`.

CI #1175 reports 45224 B static RAM and 782265 B Flash. Direct `nm` on the
artifact ELF confirms `Menu_initMenu` and `Menu_LoadHelpResource` are absent
while `Menu_startGame` remains present.

The real classic CYD confirms exact menu fingerprints, native-fixed MAIN/OPTIONS
builders, full START through Entrance `ENGINESESSION READY`, and stable final
`heap8=27368 largest8=18420`. The +544 B free heap versus the preceding
hardware-tested baseline exactly matches the 544 B static-RAM reduction.

See
[`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md).


## Native START / intro escape closure

V12 hardware head `6b565cd46172e384209a1d93e355c951f4d6c4fa`
finishes ownership of the pre-game START-to-intro transition.

The ESP32 START action directly performs the exact new-game state change
(`Player_reset`, `totalDeaths=0`, `ST_INTRO`) and no longer calls
`Menu_startGame()`. The native story renderer owns only the bounded timed
`storyPage 1 -> 2` transition and no longer calls the legacy
`DoomCanvas_changeStoryPage()` escape.

The final ELF therefore contains none of:

```text
Menu_startGame
DoomCanvas_loadState
DoomCanvas_changeStoryPage
DoomCanvas_disposeIntro
DoomCanvas_loadMap
```

CI #1185 is 45224 B RAM / 782009 B Flash, a 256 B Flash reduction from merged
main with unchanged static RAM.

The real CYD proves the timed automatic page transition, final native
Continue/PARK, bounded resource-only disposal, Entrance native bootstrap and
resident gameplay. Final ALIVE is stable at
`heap=93076 heap8=27368 largest8=18420`.

Detailed milestone:
[`MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md`](MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md).


## Explicit intro/startup composition — REAL-CYD PASS (2026-09-30)

V13 hardware head `0f733d1a6ac680b0ff3f7954f4e40bcf44f942f8`
retires the two `Esp32IntroDispose_*` linker wrappers. The intro clock now
composes the resource-only disposer and the generic native startup owner
directly, preserving reset and service order without hidden linker routing.

CI #1191 remains byte-for-byte at 45224 B static RAM / 782009 B Flash /
782368 B firmware.bin. Final-ELF wrap count drops 57 -> 55. The real CYD proves
START, timed page 1 -> 2, final PARK, 33772 B bounded intro resource recovery,
Entrance native bootstrap and stable resident gameplay at
`heap=93076 heap8=27368 largest8=18420`.

Detailed milestone:
[`MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md`](MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md).


## Production menu BSP runtime retired — REAL-CYD PASS (2026-09-30)

V14 hardware head `dd4161a40b28d2ed9c88370b2f8281b9045f2980`
removes the old `menu.bsp` map-runtime construction from normal `esp32-cyd`
startup. Production now enters the fixed native MENU_MAIN model and opaque
dashboard directly after config/mappings startup.

The old menu BSP structure/render regression chain is not deleted as a
diagnostic asset; it is scoped to `esp32-cyd-bringup` only. Normal firmware
therefore no longer retains `Render_beginLoadMap/Data`,
`DoomCanvas_updateLoadingBar`, the menu BSP probes or their `longjmp`
escape closure.

CI #1197 reports 45128 B static RAM / 773089 B Flash / 773456 B firmware.bin,
a reduction of 96 B RAM and 8920 B linked Flash versus merged main. Final ELF
wrap count drops 55 -> 52.

The real classic CYD boots directly to framebuffer FNV `522dc605` at
`heap8=35656 largest8=23540`, with no `MENUBSP/MAPSTRUCT` trace. OPTIONS
and HELP Back both restore the same frame without allocations. START begins
with `nodes/lines/mapSprites == NULL`, completes intro disposal, reaches
Entrance `ENGINESESSION READY`, then commits a real FORWARD move. Stable
gameplay ALIVE is `heap=93388 heap8=27464 largest8=18420`.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md](MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md).


## First-frame fidelity wrapper scoped to bringup — REAL-CYD PASS (2026-10-01)

V15 hardware head `5460c689b708468e3bdd618d0000753159a24109`
removes the production linker interception around
`EspNativeFirstFrame_route()`. The real native first-frame renderer, FNV
publication, present gate and session checks remain unchanged.

The historical read-only `[JUNCTIONFRAME] COLORSTATS` / optional BMP capture
continues to exist only in `esp32-cyd-bringup`.

CI #1202 reports 45120 B static RAM / 772161 B Flash / 772528 B firmware.bin,
saving 8 B RAM and 928 B Flash from merged V14 main. Final wrap count is 51.

Real-CYD START reaches Entrance with exact first-frame FNV `71ca7465`,
no COLORSTATS line, then `ENGINESESSION READY` with
`shapeData=0x0 mediaTexels=0x0`. Two ALIVE samples are stable at
`heap=93396 heap8=27472 largest8=18420`.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md](MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md).


## Compile-time logging levels — REAL-CYD PASS (2026-10-01)

V16 establishes `ESP32/include/doomrpg_log.h` as the common C/C++ logging
policy with `DRPG_LOGE/I/D/T`. Normal `esp32-cyd` defaults to INFO;
`esp32-cyd-bringup` defines TRACE. The policy is compile-time and has no
runtime owner, allocator, parser or persistence.

The first migration removes always-on hot VIDEO/PAKIO/plane/sprite/RNG
profiling from the INFO image while preserving errors, operational transitions
and ALIVE. Crate/interact census detail is DEBUG.

Hardware-tested code: `5a320d50a5f88646382db1211114a31b671b34b2`.
CI #1209: 45080 B RAM, 767581 B linked Flash, 767952 B firmware.bin.
All 51 linker wrappers remain the identical set from merged V15 main.

The real CYD reaches FIRST_FRAME and ENGINESESSION READY with
`shapeData=0x0 mediaTexels=0x0`, then commits multiple gameplay actions with
the migrated TRACE spam absent from the runtime log.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md](MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md).

## Interaction / CHANGEMAP recovery diagnostics scoped to bringup — REAL-CYD PASS (2026-10-01)

V17 hardware head `53b548b5adf0d09c2d1e1ed4b673a3ae8054cac2` removes the temporary
`EspNativeGameplayInteractionInventory_log` wrapper from normal
`esp32-cyd` and compile-time gates both the session census call and the
historical CHANGEMAP corpus wrapper behind DEBUG/TRACE.

The real transition engine is unchanged. Normal INFO firmware retains
`EspNativeGameplayTransitionHandoff_service` and the complete native
SAVEGAME/CHANGEMAP production route, but no longer executes or links the
read-only recovery census.

CI #1214 reports 45072 B static RAM / 764757 B linked Flash /
765120 B firmware.bin. Final wrap count drops 51 -> 50. The reduction versus
merged V16 main is 8 B RAM and 2824 B linked Flash.

The real CYD reaches exact Entrance first-frame FNV `71ca7465`, then
`ENGINESESSION READY` with `shapeData=0x0 mediaTexels=0x0`, commits
MOVE/TURN, crate transform, dialog resume and resource pickup, with stable
ALIVE witnesses and no `CHANGEMAPPROBE` or `INTERACTMAP` output.

`INTERACTCORPUS` and `JUNCTIONEXITCENSUS` remain separate production
diagnostic candidates and were not changed in this milestone.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md).

## Junction exit census scoped to DEBUG/TRACE — REAL-CYD PASS (2026-10-01)

V18 hardware head `6733395845c289fa9f69cc6c61001c3a68f2d72d`
compile-time gates the historical Junction exit census out of normal INFO
firmware. The real transition and handoff owners are unchanged.

CI #1216 reports 45072 B static RAM / 762617 B linked Flash /
762976 B firmware.bin. The wrapper count remains 50. The normal ELF contains no
`EspNativeGameplayTransition_probeJunctionExitCensus`, no
`junctionExitCensusDone` and no `[JUNCTIONEXITCENSUS]` strings.

The real CYD preserves exact Entrance first-frame FNV `71ca7465`,
`shapeData=0x0 mediaTexels=0x0`, committed MOVE/TURN, crate/resource gameplay,
regular-door open/close animation and stable ALIVE
`heap=93444 heap8=27520 largest8=18420`.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md](MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md).

## Interaction-chain corpus wrapper scoped to bringup — REAL-CYD PASS (2026-10-01)

V19 hardware head `a2dffc4243701a5f78fa02abf39a81da68b5c128`
removes the production wrapper around `EspNativeResidentGameplay_service`.
That wrapper performed only the one-shot `INTERACTCORPUS` recovery scan;
the normal product now calls the real resident gameplay service directly.

CI #1218 reports 45064 B static RAM / 762021 B linked Flash /
762384 B firmware.bin. Active wrappers drop 50 -> 49. The final normal ELF has
no corpus function/state/strings or resident-gameplay diagnostic wrapper.

The real CYD preserves exact Entrance first-frame FNV `71ca7465`,
`shapeData=0x0 mediaTexels=0x0`, committed MOVE/TURN, crate transform,
scientist dialog open/close and opcode-19 resume, with stable ALIVE. The
pre-dialog gameplay heap increases by the exact expected 8 B versus V18.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md).

## Hot redraw success telemetry moved to TRACE — REAL-CYD PASS (2026-10-01)

V20 hardware head `552ec9c529d1bd31a656c9847b0b267bacd713fa`
moves repeated success-only redraw summaries to TRACE while keeping renderer,
gameplay and recovery logic unchanged.

CI #1220 reports 45064 B static RAM / 759197 B linked Flash /
759568 B firmware.bin. Active wrappers remain 49.

The real CYD preserves exact Entrance first-frame FNV `71ca7465`,
`shapeData=0x0 mediaTexels=0x0`, committed MOVE/TURN, resource pickups and
regular-door operation. Repetitive `NATIVEFRAME`, weapon, facing-label,
resident-frame, action-frame and dynamic-line success summaries no longer
flood INFO, while `LEGACY_GUARD -> RETRY -> RECOVERED` remains visible and
hardware-proven.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md](MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md).


## Hot input / move / idle-turn telemetry moved to TRACE — REAL-CYD PASS (2026-10-01)

V21 hardware head `ac5e11127f294a5e2d7d1127febb21214be94458` removes
success-only serial traffic from the hot input/movement path while preserving
all runtime behavior and the diagnostic branches that matter.

Normal INFO no longer prints generic move-event phase summaries,
touch-feedback FLASH/RESTORE, per-MOVE automap-uncover success, turn scheduling,
rotation-no-turn, or strictly idle zero-monster completion summaries.
Conditional monster summaries remain INFO whenever they carry nontrivial state.

CI #1225 reports 45064 B static RAM / 757789 B linked Flash /
758160 B firmware.bin; active wrappers remain 49. Versus V20 this is
0 B static RAM and -1408 B linked Flash.

The real classic CYD preserves exact Entrance first-frame FNV `71ca7465`,
`shapeData=0x0 mediaTexels=0x0`, banal committed MOVE/TURN, pickups,
dialog/resume, blocked movement and regular-door operation. Meaningful
`MOVEEVENT WORLD-READY/COMMIT` remains visible, and two real
`LEGACY_GUARD -> RETRY -> RECOVERED` renderer recoveries complete normally.

A V9 Sector 1 resume also preserves the live four-member monster sequence,
movement commits, subtype-4 three-goal chains and the non-idle
`MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4` witness.

Repeated ALIVE is stable at `heap=93448 heap8=27524 largest8=18420` before
lazy dialog allocation and `heap=92412 heap8=26488 largest8=18420` afterward.

Detailed milestone:
[MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md](MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md).


## V21 post-review MOVE-event diagnostic visibility fix — REAL-CYD regression PASS (2026-10-01)

Post-merge review found that V21's generic `logPhase(...)` demotion also
removed detailed INFO context for fail-closed MOVE-event statuses. Commit
`2c855bd217999453ec21246937ef6730e1697f3c` keeps ordinary
`NO_EVENT/NO_ELIGIBLE` and supported success phases at TRACE, but restores
`INVALID/NOT_READY/UNSUPPORTED/COMPLEX`, unknown statuses, and unexpected
post-preflight EXIT dialog/message divergence to INFO.

CI #1230: 45064 B static RAM / 758389 B linked Flash / 758752 B firmware.bin,
49 active linker wraps. The +600 B linked Flash versus V21 is the retained INFO
diagnostic format; static RAM is unchanged.

The real CYD regression path remains compact across more than twenty MOVE
commits, turns, pickups, multiple regular doors, opcode-26 and SELECT dialogs,
fire actions and three genuine renderer compact-guard recoveries. Routine
`MOVEEVENT EXIT-PREFLIGHT/ENTER-PREFLIGHT/EXIT/ENTER` output remains absent.
Meaningful `MOVEEVENT COMMIT/WORLD-READY` remains visible.

The hardware run does not naturally trigger an unsafe MOVE-event phase, so the
new unsafe-status INFO record is source/CI verified, not claimed as a
hardware-triggered witness.

See the post-review addendum in
[MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md](MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md).
