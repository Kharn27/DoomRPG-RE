# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Current Git boundary

```text
current main = 67950d6310ddb2fe4b48342200603674f6e71815
branch = agent/esp32-consolidation-interaction-diagnostics-v17
hardware-tested code boundary = 552ec9c529d1bd31a656c9847b0b267bacd713fa
CI = esp32-cyd #1220 SUCCESS
static RAM = 45064 B
linked Flash = 759197 B
firmware.bin = 759568 B
artifact id = 11157510597
artifact digest = sha256:f891d1f121611cade66ed43eb9064919fa2da5336d39ac27d8a97f924802aded
firmware sha256 = 956d0593fa16366448736ee602eba321f217a94b106c4dbcf7286503afe976e8
ELF sha256 = 559f03b35ab5950ce4e9ee51c4fe4c128e81975c708001b6c7aa52952a3b4c10
hardware = cold boot -> native MENU_MAIN -> START -> full intro -> Entrance -> FIRST_FRAME/READY -> repeated MOVE/TURN/pickup/door gameplay PASS
status = HARDWARE PASS; hot redraw success telemetry is TRACE-only while failure/recovery witnesses remain INFO/ERROR; active linker wraps = 49
```

### Runtime ZIP asset source retirement — REAL-CYD PASS (2026-09-30)

Commit `eb18ea2c5fc090161cee148b1f9aea52c7dc91d9` removes the ESP32 runtime
dependency on `DoomRPG.zip`. `src/Z_Zip.c` is excluded from the build and
the remaining desktop-derived startup/menu consumers use the bounded
`EspLegacyAssetSource` bridge over the authoritative
`/DoomRPG-ESP32.pak`.

CI #1083 succeeds at 45760 B static RAM and 807377 B flash, a reduction of
16 B RAM and 8300 B flash relative to merged main. The produced ELF contains
none of `zipFile`, `openZipFile`, `closeZipFile`,
`readZipFileEntry`, `readZipFileEntryInto`, `findAndReadZipDir` or
`tinfl_decompress`; the ZIP parser and miniz decompressor are genuinely absent.

The real classic CYD validates cold boot entirely from the native PAK:
HUD/pre-render/Render startup/mappings all report `backing=pak`, the normal
menu reaches `shapeData=0x0 mediaTexels=0x0`, and ALIVE reports
`SD=ready PAK=ready`.

Start Game then exercises the four intro BMPs from the PAK and forces a real
Sector1 -> Entrance raw-flash rebuild:

```text
[MAPFLASH] REUSE MISS requestedMap=1 ... cachedMap=2 reason=world-identity
[MAPFLASH] READY map=1 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Loading the existing V9 Sector 1 save proves the inverse direction:

```text
[NATIVESAVE] LOAD ... version=9 ... restored-exact
[MAPFLASH] REUSE MISS requestedMap=2 ... cachedMap=1 reason=world-identity
[MAPFLASH] READY map=2 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=2 ... shapeData=0x0 mediaTexels=0x0
```

Post-load gameplay then commits player movement, services the ordered four-member
monster sequence including both subtype-4 three-goal chains, and resolves a real
Bull Demon attack. ALIVE remains stable at
`heap=94016 heap8=28400 largest8=16372`.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md](MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md)

### Explicit MonsterMovement ActiveSequence composition — REAL-CYD PASS (2026-09-30)

Commit `c27b94e263f4ea0d445e7debeea4d836dbd59e4f` removes the final active
monster-domain linker interception,
`--wrap=EspNativeGameplayMonsterMovement_service`.

Permanent ownership is now explicit:

```text
GameplaySession_service
 -> MonsterMovementProbe_service
 -> MonsterActiveSequence_service
 -> MonsterMovementProbe_serviceMember
 -> MonsterMovement_service
 -> MovementPublish
 -> postMoveGoal / three-goal continuation
```

The real classic CYD validates one ordered four-member MOVE: sprites 218 and 237
commit ordinary movement; subtype-4 sprites 0 and 1 each complete their
three-goal chains. The turn closes with
`activeCount=4 delivered=4 sameMonsterTurn=yes ordered=yes publication=per-member`.
Two following ALIVE samples are identical at
`heap=82704 heap8=17152 largest8=10228`.

CI #1069 succeeds with unchanged 45776 B static RAM and 815677 B flash.
Translation units remain 173, active linker wraps drop 58 -> 57, and active
`EspNativeGameplayMonster*` linker wraps drop 1 -> 0.

A later real-CYD PASS_TURN provides the complementary ranged-attack witness:
sprite 218 / subtype 3 / weapon 15 runs through
`ATTACK-PROBE -> MONSTERACT -> MONSTERATKVIS -> MONSTERRETAL`, with
`playerHP=22->20 armor=12->10` committed only after the visual completes and
rollback closes. Following ALIVE samples remain stable at
`heap=82704 heap8=17152 largest8=10228`.

That genuine ranged attack emits no `RANGED-MEMBER`, confirming the boundary:
`RANGED-MEMBER` denotes the unchanged exact-source ranged-AI
movement/repositioning branch, not the attack presentation/resolution path.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md](MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md)

### Explicit MonsterTurn activation-filter composition — REAL-CYD PASS (2026-09-30)

Commit `517c37855e18894c4d2292dab20ec7852107549b` removes
`--wrap=EspNativeGameplayMonsterTurn_view` and replaces the hidden getter
interception with an explicit producer/filter service boundary.

The permanent service order is now:

```text
PlayerResources session service
 -> MonsterTurn observe/probe producer
 -> MonsterActivation_serviceTurn
    -> deferred DestructibleTurn flush
    -> producer snapshot + attack-probe filtering
 -> AttackVisual / Retaliation / Movement read MonsterActivation_turnView
```

`EspNativeGameplayMonsterActivation_turnView()` is side-effect-free and returns
only the last serviced filtered view. The active-sequence movement orchestrator
still needs the raw producer counters and now reads the ordinary
`EspNativeGameplayMonsterTurn_view()` directly, without a linker bypass.
Synthetic no-attack/movement counter overrides mutate only the bounded filtered
cache and restore the saved producer counters after each member.

The deferred destructible-turn bridge is no longer triggered by a getter. Its
flush now runs explicitly in `MonsterActivation_serviceTurn` after the action
service has returned through its rollback window. The request is therefore
armed by line death, cancelled on rollback if necessary, and transported only
after commit.

Build witness for the exact code boundary:

```text
esp32-cyd CI #1064 = SUCCESS
static RAM = 45776 B
flash = 815677 B
artifact id = 11081649324
artifact sha256 = d7b2a5c920ac293f10c021e6bb6e22831b21909d4e3d0d138c270278594d0cb3
translation units = 173
active --wrap entries = 58
```

The real classic CYD validates all three relevant paths.

Ordered movement remains intact across two turns. Four active monsters are
serviced in order; ordinary sprites 218/237 commit normally and both subtype-4
members execute their bounded three-goal chains. Each turn closes with
`activeCount=4 delivered=4 sameMonsterTurn=yes ordered=yes`.

A real subtype-4 three-shot attack proves the explicit filtered view reaches
both consumers:

```text
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=1 reason=4 activated=yes
[MONSTERATKVIS] ARM ... loops=3 ... gameplayMutation=no
[MONSTERATKVIS] COMPLETE ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=33->30 armor=23->20 ... rollback=closed
```

The formerly hidden destructible side effect is also exercised directly with an
adjacent axe hit on a jammed subtype-3 line:

```text
[ACTIONENGINE] TRACE ... route=JAMMED_DOOR_CLEARED
[DESTRUCTIBLETURN] ARM ... rollback=armed request=deferred-until-action-service-closed
[DESTRUCTIBLE] COMMIT ... turnAdvance=deferred rollback=closed
[DESTRUCTIBLETURN] REQUEST ... rollbackWindow=closed monsterTurn=requested
[MONSTERTURN] SCHEDULE ... reason=PASS_TURN passSeq=3489661144 ...
[MONSTERACTIVESEQ] COMPLETE ... activeCount=4 delivered=0 ... ordered=yes
```

The jammed-door test begins and ends at the same live allocation witness
(`heap=82664 heap8=17112 largest8=7156`). Earlier movement/attack witnesses are
also stable at `heap=82704 heap8=17152 largest8=10228`; the different absolute
largest-block value reflects the later runtime/cache state, not drift during
the tested transaction.

Only one active monster-domain linker wrapper remains:

```text
EspNativeGameplayMonsterMovement_service
```

### Explicit HUB/automap action-feedback gate — REAL-CYD PASS (2026-09-29)

Commit `3f9b862bcbca3d2217efe0b388e3c9914d1d4b23` removes
`--wrap=EspNativeGameplayMonsterState_actionService` and replaces the hidden
linker interception with the explicit permanent API
`EspNativeGameplayHubActionGate_service(...)`.

The composition order is preserved exactly:

```text
MonsterCombat service
 -> HubActionGate service
 -> MonsterState actionService
 -> ActionEngine service
```

The gate still pauses only world/action feedback expiry while HUB or automap owns
the framebuffer; it does not rebase elapsed time and it resumes the unchanged
MonsterState/ActionEngine chain once world presentation is active again.

CI #1056 succeeds with byte-for-byte identical 45776 B static RAM and 815609 B
flash relative to merged main `8d5bbaf5445fb4557bbf4f66df6aa6d692bb9a56`.
Translation units remain 173 and active linker wraps drop 60 -> 59.

The real classic CYD validates both overlay branches:

```text
[HUBACTIONGATE] PAUSE owner=hub ...
[HUBACTIONGATE] PAUSE owner=automap ...
```

The second PAUSE itself proves the earlier HUB pause cycle returned through
RESUME, because the gate logs PAUSE only on a 0 -> 1 paused transition. After
the overlays, normal world movement and monster service resume: the same
four-member ordered turn runs, both subtype-4 three-goal chains complete, and:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=79136 ms heap=82704 heap8=17152 largest8=10228
[ALIVE] uptime=84137 ms heap=82704 heap8=17152 largest8=10228
```

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
```

### Obsolete MonsterState view witness retirement — REAL-CYD PASS (2026-09-29)

Commit `b2c22ee699213a04429669c6b3ca63c479918d1f` removes the final
three-goal census-only translation unit
`esp_native_gameplay_monster_three_goal_witness.c` after
`57da58cb23a9de923a2c1bc3cd158e5b5818a7b0` removes
`--wrap=EspNativeGameplayMonsterState_view`.

The deleted wrapper had no gameplay mutation: it called the real
`EspNativeGameplayMonsterState_view()`, emitted one-shot subtype 4/13
`WITNESS/CENSUS` diagnostics, then returned the same view unchanged. No
replacement API is needed.

CI #1049 succeeds with 45776 B static RAM and 815609 B flash. Relative to merged
main `da8c3632162ad8dc7a0a83e7c398d815a0fbfea2`, the ESP32 source count drops
174 -> 173 and active linker wraps drop 61 -> 60.

The real classic CYD proves the gameplay path is unchanged:

```text
sprite 218 subtype 3: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 237 subtype 5: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 0 subtype 4: 470 -> 471 -> 439 -> 440, MONSTER3GOAL COMPLETE
sprite 1 subtype 4: 534 -> 535 -> 536 -> 537, MONSTER3GOAL COMPLETE
MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes
ALIVE uptime=56336 ms heap=82704 heap8=17152 largest8=10228
```

Each subtype-4 continuation still passes `MONSTERMOVEACT ALLOW`, commits one
movement RNG byte, publishes position/topology and closes rollback before the
next member. The retired `MONSTER3GOAL WITNESS/CENSUS` lines are absent, as
expected, while the permanent `MONSTER3GOAL READY/ARM/PLAN/COMMIT/COMPLETE`
runtime remains intact.

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
```

### Explicit MonsterTurn post-move composition — REAL-CYD PASS (2026-09-29)

Commit `2976cf9f157fa3dfd1649efaaec77986450648cc` removes the active
`--wrap=EspNativeGameplayMonsterTurn_postMoveGoal`.

The ordinary-vs-three-goal decision now belongs explicitly to
`EspNativeGameplayMonsterTurn_postMoveGoal(...)`. Subtypes 4/13 are delegated
to `EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal(...)` before ordinary
turn-owner sync/probe accounting, preserving the previous wrapper ordering.
Other monster families continue through the unchanged ordinary post-move path.

CI #1036 succeeds with 45784 B static RAM and 816545 B flash. The active linker
wrap count is now 61, down from 62 on merged main
`0e66004c755cc050c5fa3f6eac91f85f943e4c8d`.

The real classic CYD validates this after V9 LOAD: movement restarts at `n=1`,
four active monsters are delivered in order, sprite 0 completes
470->471->439->440, sprite 1 completes 534->535->536->537, and both end in
`MONSTER3GOAL COMPLETE`. Every continuation passes `MONSTERMOVEACT ALLOW`,
commits exactly one movement RNG byte, publishes topology/position and closes
rollback before the next member. The turn closes with:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=49239 ms heap=82696 heap8=17144 largest8=10228
```

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
EspNativeGameplayMonsterState_view
```

### Explicit synthetic MovementView publication — REAL-CYD PASS (2026-09-29)

Commit `560e54bd2d32fe1f5d704cd9ef0d3737c57f765b` removes the active
`--wrap=EspNativeGameplayMonsterMovement_view`.

The three-goal continuation no longer intercepts the global movement view.
Instead, the publisher exposes the bounded permanent API
`EspNativeGameplayMonsterMovementPublish_afterProbeWithView(...)`, and the
three-goal owner passes its synthetic movement view explicitly only for the
continuation transaction. The normal movement path continues to publish against
the real `EspNativeGameplayMonsterMovement_view()`.

CI #1028 succeeds with 45784 B static RAM and 816509 B flash. The active linker
wrap count is now 62, down from 65 on merged main.

The real classic CYD proves both subtype-4 chains through the explicit view seam:

```text
sprite 0:
  first goal 470 -> 471 COMMIT
  goal 2/3 471 -> 439 COMMIT
  goal 3/3 439 -> 440 COMMIT
  MONSTER3GOAL COMPLETE

sprite 1:
  first goal 534 -> 535 COMMIT
  goal 2/3 535 -> 536 COMMIT
  goal 3/3 536 -> 537 COMMIT
  MONSTER3GOAL COMPLETE

MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes
ALIVE remains steady through uptime=55893 ms
```

Every continuation still passes `MONSTERMOVEACT ALLOW`, every live publication
closes RNG/position/topology/rollback state, and no
`probe-sequence-or-capture-mismatch` occurs. This candidate was therefore the authoritative hardware-tested code boundary
at that point.

Remaining monster-domain active linker wraps after this PASS:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
EspNativeGameplayMonsterTurn_postMoveGoal
EspNativeGameplayMonsterState_view
```

### Explicit MovementProbe reset composition — REAL-CYD PASS (2026-09-29)

Candidate `cb45792af62d8ad0946dc4d477b288ef92aecf3a` removes the active
`--wrap=EspNativeGameplayMonsterMovementProbe_reset` and places the same
ThreeGoal -> Publish -> Movement -> Position reset order directly in the
permanent MovementProbe reset API.

CI #1022 succeeds at 45784 B static RAM / 816509 B flash with 63 active linker
wraps. Real-CYD SAVE/LOAD proves full resident teardown, exact V9 monster
state/topology/position/activation restore, owner READY reinitialization and
`ENGINESESSION READY` with `shapeData=0x0 mediaTexels=0x0`.

The first MOVE after LOAD then proves consumers restart cleanly: movement probe
counter restarts at n=1, four restored active monsters are delivered in order,
all live moves commit, and both subtype-4 monsters complete goals 2/3 and 3/3
through the explicit movement activation/publication path. The turn closes with
`activeCount=4 delivered=4 ordered=yes` and a steady `[ALIVE]` witness.
This candidate is therefore hardware validated.

### Active MonsterPosition prepare wrap removal — REAL-CYD PASS (2026-09-29)

The active linker interception of
`EspNativeGameplayMonsterPosition_prepareCardinalMove` is now replaced by the
explicit permanent movement-domain API
`EspNativeGameplayMonsterMovementActivation_prepareCardinalMove`.

The first candidate `3d15ba1393...` converted ordinary movement but missed the
subtype-4/13 three-goal continuation call site. The real CYD caught that exact
dependency through `probe-sequence-or-capture-mismatch`; that candidate is
explicitly rejected as a hardware boundary.

Commit `aa7cb5c778264e1bb61d442d1c9864c09e6f37a3` routes the continuation through
the same explicit activation + publication-capture boundary. A complete
monster-source audit then found no gameplay caller bypassing it.

The corrected real-CYD run proves:

```text
activeCount=4 delivered=4 ordered=yes
sprite 0 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
sprite 1 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
all continuation moves pass MONSTERMOVEACT ALLOW
all live moves close rollback and topology publication
ALIVE uptime=340952 ms
```

CI #1020 succeeds on docs-only descendant `25cfb5f9...` with unchanged
45784 B static RAM and 816517 B flash. The active linker wrap count is now 64,
down from 65 at merged main, with no RAM/flash growth.

### Structural consolidation v2 / dead wrapper cleanup — REAL-CYD PASS (2026-09-29)

The post-merge audit starts from exact `main`
`3de74fc1899ea619874b9f2bce8fb3679016c1a4`. The production tree contains
175 ESP32 C/C++ translation units, 17 legacy `src/*.c` implementation units
still compiled for ESP32, and 65 linker `--wrap` entries. 50 of those linker
entries target native `Esp*` symbols, so native-to-native composition is now a
first-class consolidation target.

The first bounded cleanup removed the dormant historical
`EspNativeGameplayMonsterRetaliation_* -> EspNativeGameplayMonsterMovement_*`
wrapper footer and the link-only
`esp_native_gameplay_monster_movement_wrap_compat.c`. Those Retaliation
symbols were no longer present in the active `--wrap` list.

```text
translation units = 175 -> 174
active --wrap flags = unchanged at 65
CI #1015 attempt 2 = SUCCESS
static RAM = 45784 B
flash = 816517 B
```

The RAM/flash totals are byte-for-byte identical to a clean build of merged
`main`, consistent with this compatibility code already being dead at final
link.

The real classic CYD then exercised Sector 1 through a complete monster
hit/death transaction, a `PLAYER_ATTACK` monster turn, active-sequence service,
a committed MOVE from tile 508 -> 507, another MOVE-triggered monster turn, and
a steady `[ALIVE]` witness at about 99 seconds. No reboot or gameplay
regression was observed.

This validates only code through `4731d826...`. Later consolidation commits on
this still-active branch remain candidates until separately tested.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md`](MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md)

### Consolidation-v1 progression recovery — REAL-CYD PASS (2026-09-29)

The first translation-unit consolidation experiment on this branch was reverted
exactly; the reproduced stack failure was pre-existing and was fixed separately.
The lasting branch value is a set of generic runtime corrections exposed by
progressing from Entrance through Junction into Sector 1.

Hardware-proven current boundary:

```text
Junction PASS_TURN with enemies=0
Junction V9 SAVE with monsters=0 / positions=0
Junction V9 LOAD with empty monster owners
Junction event 62 direct CHANGEMAP showStats=0
/level01.bsp = Sector 1, map=2, gameplayLoadMapId=3, spawn=477, dir=192
Sector 1 first MOVE 477->509 committed
Sector 1 next MOVE 509->508 committed
Level01 monster activation and ordered live movement continue
```

Permanent generic fixes include independent SAVEGAME/CHANGEMAP map strings,
direct transition-door READY handoff, four-cardinal fresh-map spawn,
spawn-time EV_FORCEMESSAGE, and EV_FORCEMESSAGE inside the bounded atomic mixed
MOVEEVENT executor. None of these paths key behavior on Junction, Sector 1 or a
specific event number.

The post-review real-CYD witness is on
`7d64839a31376c4ca0a3ec4f0f5ae10395b04635`; CI #1009 succeeds at
45784 B static RAM and 816517 B flash.

The same hardware run proves the independent EV_SAVEGAME return route survives
the destructive Junction -> Sector 1 handoff and the session reset. A Sector 1
checkpoint save reported exactly:

```text
returnRoute=/junction.bsp/416,1824/192
```

after a committed Sector 1 MOVE from tile 477 -> 509. The subsequent V9 LOAD
restored Sector 1 at the saved pose and reached
`[ENGINESESSION] READY map=2 angle=128 ... TURN+MOVE=armed`.

The review fix that clamps fresh-spawn facing traces at 32x32 map edges mirrors
legacy `Game_trace()`: source/destination tile components are clamped to
`[0,31]` while raw world endpoints remain available for sprite-plane crossing.
CI covers the implementation, but the supplied hardware run did not exercise an
actual spawn within three facing-trace steps of a map edge; do not claim that
specific edge case as hardware-proven.

Detailed record:

- [`MILESTONE_NATIVE_JUNCTION_SECTOR1.md`](MILESTONE_NATIVE_JUNCTION_SECTOR1.md)

### Transition/loading polish — REAL-CYD PASS

The real classic CYD has now validated both checkpoint LOAD entry contexts on
the same native restore pipeline:

```text
MENU_MAIN -> Load Game             PASS
running gameplay -> SYS -> LOAD    PASS
```

The final loading owner remains full-screen through restore and session/cache
priming. Progress is paced through 10/30/60/75/85/90/93/96/100, and the final
100% loading frame is followed by a rebuilt gameplay frame, explicit loading
release, top+bottom HUD repaint, then the first visible gameplay present.

Important ownership corrections in the tested code boundary:

- checkpoint session teardown happens before `beginLoading()`, so the in-game
  HUB cannot clean up old presentation state after loading has acquired the
  shared framebuffer;
- each progress publication reconstructs the complete loading frame because
  gameplay writers can mutate the shared framebuffer even while their presents
  are suppressed;
- warm-cache resume no longer treats zero new LARGE-LEARN stores or post-render
  large-entry eviction as a functional load failure;
- `READY 100%` is not the gameplay framebuffer: `FINAL-READY` rebuilds the
  world, loading ownership is released without presenting, and
  `EspNativeGameplayHud_repaint()` restores both HUD bands before the final
  gameplay present.

The hardware-tested code boundary is `6903431a60700960127be95d95584728698a36c8`.
Subsequent commits on this branch are documentation-only.

Detailed record:
[MILESTONE_NATIVE_TRANSITION_PRESENTATION.md](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md)

### Ordinary attack post-review corrections — build-valid candidate

Two code-review findings after the hardware PASS exposed edge cases in the
presentation/resolution handshake:

1. A failed first `guardedRender()` consumed `observedAttackProbes`, cleared the
   visual sequence and left retaliation waiting forever for a completion that
   could no longer be published. The visual owner now advances the observed
   probe only after the first attack frame is presented. Until then the probe
   remains retryable and `isBusy()` keeps world input closed.
2. The last attack-to-idle redraw published completion immediately. The owner
   now holds that final idle pose for the subtype's full recovered 200–500 ms
   cadence before setting `completedProbe`, matching the legacy
   `Combat_monsterSeq()` transition into stage 2 / `Player_pain`.

Local `esp32-cyd` compilation succeeds at 45096 B static RAM and 782505 B flash.
These exact edge corrections remain candidates until exercised again on the
real CYD; they do not retroactively alter the earlier hardware evidence.

### SYS SAVE live-compass close correction — REAL-CYD PASS

A later real-CYD progression run exposed one remaining SAVE-return regression
that the earlier facing-label test did not cover: after the player had rotated,
the retained full-HUD model still carried the historical compass angle while the
live player view had a newer settled angle. HUB close repainted the stale full
HUD before checking the protected lower band, so a valid checkpoint could commit
but the close transaction returned `NOT_READY` before `Game saved` was queued.

Hardware failure signature:

```text
[NATIVESAVE] SAVE ... angle=64 ...
[GAMEPLAYHUD] REPAINT ... angle=192 ...
[HUB] CLOSE ... exactBottom=NO ...
[RESIDENTGAMEPLAY] HUB-RECOVER ... status=NOT_READY
```

The permanent close path now reuses the already-bounded compass dirty painter
after the base HUD repaint, sourcing the cardinal angle from the settled
`EspPlayerViewState`. No new retained owner or framebuffer-sized scratch was
added.

The real classic CYD validated the corrected sequence at code head
`a5b30a12b4bb51cd4f016d53212b74e967c19d6d`:

```text
[NATIVESAVE] SAVE ... angle=128 ...
[GAMEPLAYHUD] REPAINT ... angle=64 ...
[HUB] CLOSE ... hudBottom=5da12662 expectedBottom=5da12662 exactBottom=yes ...
[NATIVESAVE] SAVE-CLOSE ... feedback="Game saved" ...
[ACTIONFEEDBACK] PAINT kind=12 text="Game saved" ... durationMs=1200
[RESIDENTGAMEPLAY] HUB-CLOSE ... worldRedraw=yes ...
[ACTIONFEEDBACK] EXPIRE ... restored=topbar-only
```

The stale `GAMEPLAYHUD REPAINT angle=64` line is expected: it describes the
base retained HUD repaint before the live compass rectangle is reapplied. The
authoritative close witness is `exactBottom=yes`, followed by
`SAVE-CLOSE`, the 1200 ms message, and the normal facing-label fallback.

Build witness for the exact tested code:

```text
esp32-cyd CI #767 = SUCCESS
static RAM = 45096 B
flash = 782141 B
```

### Ordinary monster attack resolution — REAL-CYD PASS

The current ordinary monster attack path now resolves gameplay only after its
presentation lease completes. The attack probe remains transactional; while the
animation is active, player HP/armor and gameplay RNG remain unchanged and world
input is blocked.

Current rebased real-CYD witness:

```text
[MONSTERATKVIS] ARM ... sprite=315 ... visual=5 ... phaseMs=500 ...
[MONSTERRETAL] WAIT ... resolution=after-animation playerMutation=no rngConsumed=0 worldInput=blocked
[MONSTERATKVIS] COMPLETE ... visual=5->idle ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=23->19 armor=11->7 ... attackVisual=complete-before-resolution
```

The earlier multi-loop Troop test also visually proved the generic repeated-shot
presentation. Its perceived slowness remains a separate system-level performance
issue rather than a reason to retune this owner in isolation.

### Entrance event 74 mixed MOVE batch — REAL-CYD PASS

The Yellow Key trap on Entrance tile 697 uses a real mixed MOVE script:
state changes, SHOW commands and line lock/open operations. After the trap has
already fired, a later traversal can legitimately reduce to a completely handled
no-op batch with `mutation=no rollback=0`.

The first implementation still retained the static mixed rollback owner in that
case, so the next unrelated MOVE failed closed. The current code arms rollback
owners only for real mutations, immediately releases no-op mixed owners, and
passes the actual `mixedBatchOwner.active` value to the BLOCK diagnostic.

The real-CYD retest continued successfully from tile 697 through 665, 633, 601
and back to 633, with subsequent monster movement and combat. No
`stale-mixed-owner`, `transaction-busy` or
`FAILED reason=move-commit` recurred.

Runtime remained alive at the supplied tail:

```text
heap=81788
heap8=16236
largest8=8692
```

Detailed records:

- [`MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md`](MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md)
- [`MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md`](MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md)

The earlier barrel values below remain historical hardware boundaries. Their
code is now merged into `main`; keep the exact barrel SHA and its narrower PASS
claims when diagnosing that feature, independently of the current SAVE-return
validation.

### Native barrel subtype 1 — REAL-CYD PASS

The permanent native action route now owns Doom RPG type-12/subtype-1 explosive
barrels without restoring legacy world/entity ownership.

Hardware-proven transaction:

```text
native player attack
 -> root Barrel hit/removal
 -> logical sprite 180, frames 0..2
 -> recovered 8-cell radius
 -> neighboring barrels removed/queued causally
 -> same-radius sibling barrels animate concurrently
 -> each explosion consumes its own recovered RNG word
 -> nonlethal player radius damage uses native PlayerState
 -> PLAYER_ATTACK monster-turn request
 -> exact rollback remains available until commit
```

The chain is bounded to 16 barrels. Radius support is currently barrel + player:
four cardinal cells use the full blast component, four diagonals use half.
Other hurtable entity families remain fail-closed.

Distant real-CYD proof on the three-barrel cluster:

```text
[BARRELRADIUS] PREFLIGHT ... root=283 chain=3 ... visual=wave-batch-concurrent
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=3/3
[BARRELRADIUS] CHAIN source=283 target=276 ... relation=cardinal
[BARRELRADIUS] CHAIN source=283 target=303 ... relation=cardinal
[NATIVESPRITE] TRANSIENT ... anim=0 batch=1/2 pos=1568,1120 ...
[NATIVESPRITE] TRANSIENT ... anim=0 batch=2/2 pos=1696,1120 ...
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=3/3
[BARREL] COMMIT ... chainRemoved=3 playerRadiusHits=0 ... rollback=closed
```

The user explicitly accepted the resulting two-neighbor explosion as visually
correct. This closes the earlier presentation defect where both neighbors were
removed together but their explosions were then animated serially.

The close-range run additionally proved real player-radius mutation and the
current lethal boundary:

```text
[BARRELRADIUS] PLAYER-HIT source=283 ... relation=cardinal
    component=12 messageDamage=24 hp=32->20 armor=20->8
[BARRELRADIUS] PLAYER-HIT source=276 ... relation=diagonal
    component=10 messageDamage=20 hp=20->8 armor=8->0
[BARRELRADIUS] PLAYER-DEFER source=303 ... relation=diagonal
    component=6 status=1 hp=8 armor=0 mutation=no/lethal-deferred
[MONSTERTURN] ATTACK-CANCEL seq=5 cause=action-rollback scheduled=no
[BARRELRADIUS] ROLLBACK ... rollback=yes rng=yes player=yes world=yes
```

Player death is still intentionally outside the native boundary, so the final
would-be-lethal blast correctly cancels the requested turn and restores the
owned transaction instead of partially committing unsupported death state.

The aggregate multi-blast `<N> damage!` summary exists as a bounded candidate
because the full legacy five-message HUD queue is not yet native. It is **not
hardware-proven**: the close-range test reached lethal rollback before that
summary could commit. The user explicitly deferred this presentation retest
until health can be restored conveniently through inventory/medkits.

Detailed record:

- [`MILESTONE_NATIVE_BARREL_SUBTYPE1.md`](MILESTONE_NATIVE_BARREL_SUBTYPE1.md)

### Finger-first main menu redesign — REAL-CYD VISUAL/TOUCH PASS

The classic-CYD `MENU_MAIN` no longer presents four thin J2ME-style text rows.
The permanent presentation is a bounded 2x2 finger-first dashboard while the
existing four-item menu model and action routing remain authoritative:

```text
START   | LOAD
OPTIONS | HELP
```

The real-CYD visual iteration established the final proportions:

```text
Doom RPG logo = 90x62 logical
card target   = 68x23 logical = 136x46 physical at exact 2x
background    = true black between cards
focus         = amber
armed tap     = ivory + amber double border
confirmation  = existing released second tap on the same card
```

The first 74x28-card candidate was hardware-reviewed as too dominant and forced
the title down to 74x51. The final layout restores the proven 90x62 title size,
then removes the full-width grey/blue dashboard slab so the four industrial cards
float independently on black. The user explicitly accepted this final rendering
on the real classic CYD.

The old main-menu cursor feedback stored four 13x10 RGB565 underlay patches
(1040 B plus bookkeeping). The dashboard instead repaints its bounded card band
allocation-free, so permanent cursor-patch storage is zero. Relative to the
current main image, CI reports 1104 fewer bytes of static RAM:

```text
main       = 45744 B static RAM
redesign   = 44640 B static RAM
delta      = -1104 B
flash      = 767381 B
CI         = esp32-cyd #729 SUCCESS
code head  = cd557d7b727d600cecbff610d6e3e6a21d609853
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory. The redesign
does not add a framebuffer, map-wide asset owner, ZIP dependency, or gameplay
world mutation.

The inherited V8 cold main-menu LOAD path was already hardware-proven before this
presentation milestone. The final `cd557d7...` polish changed only main-menu
presentation/touch repaint geometry, but a fresh cold `MENU_MAIN -> Load Game`
sanity has not yet been explicitly reported on that exact code head. Do not claim
that replay until the real CYD produces it.

Detailed record:

- [`MILESTONE_MAIN_MENU_FINGER_FIRST.md`](MILESTONE_MAIN_MENU_FINGER_FIRST.md)

### Options dashboard reuse — REAL-CYD VISUAL/TOUCH PASS

The `MENU_MAIN_OPTIONS` child now reuses the same bounded 2x2 card renderer and
geometry as `MENU_MAIN`:

```text
BACK  | VIDEO
INPUT | SOUND
```

`BACK` is the only enabled card until the three settings backends are ported;
the others remain visible but subdued. Its first tap now paints the same bright
ivory/amber armed state as the parent dashboard, and the released second tap
still executes the real `MenuSystem_back()` transition. Touch ownership now
tracks the painter's runtime framebuffer hash rather than the removed fixed
Options framebuffer fingerprint. PlatformIO compilation passes; visual and
touch behavior, including the two-tap `BACK` route and subdued deferred cards,
has passed a focused check on the real CYD.

This rebased integration combines the current `main` four-page HUB/touch-
feedback redesign with the later native gameplay, V8 checkpoint, CHECK_KEY and
event43 work. The boot-time contiguous-heap regression is fixed, checkpoint
LOAD has been revalidated from both SYS and the cold main menu, and the missing
initial RNG seed has been hardware-validated with a non-zero crate consequence.
The original event43 PASS remains anchored to its historical code head, and the
same Entrance sequence has now also been freshly revalidated post-rebase after
the sprite-renderer stack fix: line102 opens 4/4, event43 commits SHOW x4 on
345->377, the SHOW lease closes on rendered frame commit, and 377->409 executes
only CLOSELINE 102 with another complete 4-frame animation.

### SHOW-exit -> ENTER-dialog reachability census — REAL-CYD PASS

The review fix that releases the static SHOW rollback owner after a destination
dialog opens remains correct defensive transaction hygiene. A temporary,
allocation-free/read-only census scanned all 93 Entrance events for all four
cardinal movement directions using both initial BSP script state and the current
restored checkpoint state.

Real-CYD result:

```text
[MOVEEVENTCENSUS] SUMMARY events=93 candidates=0 mode=initial+current mutation=no allocation=no
```

Therefore Entrance contains **no reachable adjacent movement pair** of the exact
form owned by that review corner: homogeneous EXIT-side EV_SHOW batch followed
by first-eligible ENTER-side EV_DIALOG/EV_DIALOGNOBACK. The Bull Demon/Lost Soul
line102 room is confirmed to be a different sequence: ENTER SHOW event43, then
later EXIT CLOSELINE.

The temporary census was removed after this witness. GitHub comparison confirms
the add/remove probe commits leave **zero code-file diff** versus the documented
pre-probe tree; from hardware-tested cold-load code head `133f678...` to the
post-removal tree, only documentation files differ.

### In-game HUB redesign — focused REAL-CYD smoke pass

The current HUB is:

```text
INV | WPN | STAT | SYS
```

`INV` uses a derived four-row scrolling list containing Notebook, carried
items and owned keys. Key rows reuse the same green/yellow/blue/red mini-card
language as STAT; Credits remain exclusive to STAT, while its key display is a
deliberate quick-status mirror. Any populated row can be selected directly by touch. No persistent list/scroll
owner was added, and item activation remains deferred. `WPN` is a complete 3x3 grid for normal
weapon IDs 0..8; familiar IDs 9..11 remain excluded. The weapon icon loader now
converts source BGR565 palettes to framebuffer RGB565, fixing the red Fire
Extinguisher and yellow-handled Axe presentation. `STAT` is read-only and no
longer covered by checkpoint controls. `SYS` owns full-width SAVE/LOAD cards,
two-step `SAVE?` / `LOAD?` confirmation and `NO SAVE` handling.

All four tabs are directly touch-addressable. The HUB temporarily owns a full
industrial top title bar and reconstructs the permanent gameplay HUD on close.
The 28-byte HUB owner and world/turn gating remain unchanged.

The current full-height layout also reclaims the lower 20 logical rows while
the HUB is open: the gameplay portrait/status strip is hidden, and the writable
menu surface is now 160x100 at `y=20..119`. INV uses four 19-row cards, WPN
uses a 3x3 grid extending to row 118, STAT distributes its read-only groups over
the added space without enlarging its HP/Armor cards, and SYS uses taller
SAVE/LOAD targets. Ordinary close still reconstructs the retained gameplay HUD,
reapplies the settled live compass, and requires the lower-HUD fingerprint to
match the pre-open witness exactly. Successful LOAD keeps its existing
whole-session replacement path. Local `esp32-cyd` compilation succeeds at
45224 B static RAM and 796045 B flash. Its presentation, direct inventory touch,
owned yellow-card row and ordinary gameplay-HUD restoration have passed a
focused real-CYD check.

The tab labels now use native 5x7 glyphs instead of a 3x5 bitmap enlarged in
software before the CYD's final 2x presentation. This removes the effective
4x4 physical pixel blocks that made `INV/WPN/STAT/SYS` appear soft, without
changing any of their 38x13 logical touch rectangles.

Two hardware failures were reproduced and fixed during the smoke pass:

```text
large 128x21 LOAD target -> 597 edits exceeded old 512-entry feedback owner
pickup flash/message + SELECT -> legitimate framebuffer drift failed full-FNV restore
```

The redesign initially enlarged the feedback owner to 768 entries with a
6-byte `{offset,saved,painted}` record. After rebasing, that added exactly
2560 B of static RAM versus the event43 hardware-pass image and starved the
contiguous allocation used by legacy `menu.bsp` sprite structures.

The permanent owner is now bounded at **640 compact 4-byte edits**. Each record
stores `saved RGB565` plus a 15-bit framebuffer offset and one halo/core bit;
the painted value is reconstructed exactly with `glowAdd565(saved, additive)`
during reverse restore. The original 128x21 SYS card needed 597 edits; the
full-height layout's 128x28 card needs 625, still within the same fixed owner
without spending another byte of static RAM. The owner saves 2048 B versus the
768x6 form. Overlay creation remains nonfatal and restoration remains
pixel-owned: newer overlays win.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md)

#### Compact STAT information layout — REAL-CYD VISUAL PASS

The read-only `STAT` content no longer spends the viewport on five legacy 9x12
text rows. It now uses compact 3x5 labels and intermediate 5x7 values in two
slightly reduced HP/Armor cards, level/XP plus a bounded progress bar, and a consistently aligned
2x2 DEF/STR/AGI/ACC grid. The slim card rails are real proportional gauges:
health is green (red at critical level) and armor is light blue. The footer no
longer leaks the internal hexadecimal key mask; owned bits 0..3 render as compact
green/true-yellow/blue/red key-card icons. Values retain a larger visual weight than
their labels. No content hitbox was added: the existing `STAT` tab is still the
page's only touch target. PlatformIO compilation succeeds. The current rebased
image uses 45224 B of static RAM; real-CYD testing accepted the native 5x7 tab
legibility, compact value typography, proportional rails and corrected key-card
colors.

The redesigned LOAD execution path is now hardware-proven from both the in-game
SYS page and the cold main menu. The two-tap SYS route no longer requires an
ordinary HUB-close HUD restoration before replacing the gameplay session;
`EspNativeGameplaySession_reset()` owns that transition.

### SYS SAVE return and facing-label fallback — REAL-CYD PASS (2026-09-25)

The successful two-tap SAVE route now closes the HUB immediately instead of
leaving `SAVED` on the SYS card. The first SELECT still arms `SAVE?`; the second
commits the V8 checkpoint, returns to the settled world and queues `Game saved`
through the shared bounded `STATUS_TEXT` feedback lease for about 1200 ms.

The first real-CYD run exposed a close-time false negative rather than a save
failure:

```text
[NATIVESAVE] SAVE ... version=8 ...
[HUB] CLOSE ... menuUnderlayRestore=exact hudRepaint=yes ... exactHud=NO
[RESIDENTGAMEPLAY] HUB-RECOVER ... status=NOT_READY
```

The full top band hash legitimately differed because it contained the derived
`Door` facing label. Repainting the base HUD before the next world frame cannot
be byte-identical to that old derived top-bar presentation. HUB close integrity
therefore now validates the exact protected lower HUD band; the top bar is
explicitly recomposed by the following world render.

The user then validated the complete visual sequence on the real classic CYD:

```text
Door -> SAVE -> SAVE? -> successful checkpoint -> gameplay
     -> Game saved -> timeout -> Door
```

Feedback expiry uses the normal top-bar priority rather than a stale framebuffer
snapshot: permanent FORCE_MESSAGE status first, current facing-entity label
second, empty bar last. Saving does not advance the gameplay or monster turn.

### Rebased boot regression — REAL-CYD RECOVERY

The first rebased firmware rebooted continuously while loading the real menu map:

```text
[MAPSTRUCT] Render_beginLoadMap result=1 heap8=17244 largest8=10740 ...
[MAPSTRUCT] -> real Render_beginLoadMapData()
Guru Meditation Error: StoreProhibited
EXCVADDR: 0x00000000
```

The exact ELF resolved the fault to `Render_beginLoadMapData()` at `src/Render.c:566`,
immediately after:

```c
render->mapSprites = SDL_calloc(render->numSprites, sizeof(Sprite_t));
mapSprite->x = DoomRPG_shiftCoordAt(...);
```

`SDL_calloc()` had returned NULL because the rebased static feedback owner had
consumed the contiguous internal-RAM margin needed by the legacy structural
loader. ELF/BSS comparison isolated the delta exactly:

```text
event43 hardware-pass image feedback owner = 2068 B
first rebased image feedback owner         = 4628 B
delta                                      = +2560 B
```

The compact 640x4 journal reduces the production image to:

```text
CI #683 = SUCCESS
static RAM = 45736 B
flash = 764349 B
```

An ESP32-only fail-closed check now guards the `mapSprites` allocation and logs
the exact requested byte count if it ever fails again instead of dereferencing
NULL. After flashing `6cd637c`, the user reports that the reboot loop is gone
and the firmware appears to run normally.

### Core gameplay RNG initial seed — REAL-CYD PASS

A long-running crate anomaly exposed a startup bug rather than bad luck: several
different crates, opened in different orders and even across runs, repeatedly
resolved as `TRAPPED_REMOVE` with `first=0`, followed by `rngByte=0`, `blast=5`
and the legacy message `10 damage!`.

The ESP32 bring-up allocates the real `DoomRPG_t` root with `SDL_calloc()`. That
made the embedded 128-byte `Random_t.randTable` all-zero with `nextRand=0`.
`DoomRPG_randNextByte()` does not refill until the table boundary, so the first
128 byte draws were deterministic zeroes. The desktop root is not calloc-zeroed,
and the native port must explicitly materialize the canonical first table.

Code head `6ab5d25216b52f096563a95749f1dbd8b33712dd` now calls exactly one
`DoomRPG_setRand(&doomRpg->random)` when the real core root is created. This
initializes the inherited hidden seed/reset state without changing later byte
or word draw cadence or the RNG replay guard.

The trap damage message itself was not doubled incorrectly. Legacy explosion
processing calls `Game_radiusHurtEntities(..., rnd+5, rnd+5, ...)`, and
`Player_pain()` displays the sum of its health and armor components. Therefore
`rngByte=0` legitimately gives `5 + 5 = 10 damage`; the bug was the repeated
zero RNG input.

Real-CYD proof after checkpoint LOAD:

```text
[CRATE] CONSEQUENCE seq=4 sprite=82
        first=99 second=0 secondValid=0
        outcome=TRANSFORM effectiveDefTile=92
        rngCombat=2 rngConsequence=1
        attackDamage=6 attackArmorDamage=4

[CRATE] COMMIT ...
        outcome=TRANSFORM
        effective=3/21/def92
        removed=0 transformed=1
        rollback=closed
```

`99` lies in the recovered legacy `24..149` bucket, so the resulting
`type=3/subtype=21` Armor Shard is exact. This proves the live RNG stream is no
longer the calloc-zero table after boot/load.

Build reference:

```text
esp32-cyd CI #693 = SUCCESS
static RAM = 45736 B
flash = 764741 B
```
### Facing-entity top-bar label — REAL-CYD PASS

Legacy `DoomCanvas_checkFacingEntity()` performs a short forward trace after a
settled pose. `Hud_drawTopBar()` then uses the current entity definition name
only as the lowest-priority gameplay fallback:

```text
timed HUD message
 > statBarMessage
 > logMessage
 > facingEntity->def->name (eType != 9)
 > empty
```

The native recovery is pointer-free. It keeps one 34-byte derived owner for the
current target and reads only that target's historical 16-byte EntityDef name
from `/entities.db` through the existing PAK-backed catalog. No map-wide name
table and no legacy `Entity_t*` ownership were introduced.

Recovered trace shape:

```text
origin = player center shifted 31 units forward
reach = 3 tile steps
shared-tile ordering = legacy linked order
line entities = supported
type 9 = trace blocker / no displayed label
```

Real-CYD witnesses include:

```text
Civilian  -> source=sprite index=19 distance=1
Computer  -> source=line   index=279 distance=2 then distance=1
Door      -> source=line   index=275 distance=3
empty     -> active=0 display=0 followed by FACINGLABEL CLEAR
```

The same run proved that closing the HUB restores the world with the existing
`Civilian` facing label intact, and pure rotation retargets the label while
`MONSTERTURN ROTATE-NO-TURN` remains unchanged.

Hardware runtime remained alive at the supplied steady witness:

```text
heap=81936
heap8=16384
largest8=11764
```

Detailed record:

- [`MILESTONE_NATIVE_FACING_LABEL.md`](MILESTONE_NATIVE_FACING_LABEL.md)


### EV_CHECK_KEY / Yellow Door — REAL-CYD PASS (2026-09-24)

The production SELECT route now consumes the permanent native
`EspNativeGameplayPlayerState.keys` bitmask instead of the old placeholder
`playerKeys=0`. Opcode 41 is the recovered generic `EV_CHECK_KEY` selector:
0/1/2/3 = Green/Yellow/Blue/Red.

Real-CYD validation on Entrance event 11 / tile 200 proved the missing-Yellow-Key
path end to end:

```text
[ACTION] SELECT seq=62 status=KEY_REQUIRED tile=200 event=11 eligible=1 unsupported=0
[CHECKKEY] BLOCK seq=62 event=11 cmd=0 keyId=1 mask=02 message="Need Yellow Key" sound=5065-deferred continuation=paused worldMutation=no removedMutation=no turnAdvance=deferred
[ACTIONFEEDBACK] PAINT kind=12 text="Need Yellow Key" ... durationMs=1200
```

No line/script/world mutation occurred and the following `EV_OPENLINE` remained
unexecuted, matching the recovered legacy pause semantics. The owned-key
continuation remains the same bounded atomic door-batch preview/commit/rollback
path; non-door variants remain fail-closed.

The previously blocking Entrance tile 377 / event 43 frontier is now hardware proven. The raw event is five commands:

~~~text
off0 EV_SHOW sprite=1  arg2=0x0000020f
off1 EV_SHOW sprite=2  arg2=0x0000020f
off2 EV_SHOW sprite=3  arg2=0x0000020f
off3 EV_SHOW sprite=4  arg2=0x0000020f
off4 EV_CLOSELINE line=102 arg2=0x000000e0
~~~

On ENTER, the four SHOW commands are eligible and execute in order; the CLOSELINE is not. Each SHOW links its target and sets its REMOVE bit. The MOVE transaction uses one bounded static 192-byte SHOW journal, not a stack-resident batch. Preflight applies all four SHOWs then rolls them back in reverse before MOVE commit; the real commit replays the same four results and retains exact rollback until the rendered destination frame commits.

Real-CYD proof at code head 48accf9:

~~~text
[MOVEEVENT] SHOW-OWNER resultBytes=76 ownerBytes=192 max=4 storage=static stackBatchBytes=0
[MOVEEVENT] SHOW-BATCH-PREFLIGHT event=43 count=4 ... exact=yes mutation=no
[MOVEEVENT] ENTER-PREFLIGHT ... tile=377 ... status=SHOW_OK event=43 eligible=4 opcode=7
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=0 sprite=1 tile=613 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=1 sprite=2 tile=619 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=2 sprite=3 tile=455 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=3 sprite=4 tile=457 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH event=43 count=4 eligible=4 mutation=yes removedCommands=4 rollback=1
[MOVEEVENT] COMMIT seq=6 exitEffect=0 enterEffect=1 render=ok rollbackLease=closed
[RESIDENTGAMEPLAY] MOVE ... tile=345->377 ... committed=yes
~~~

The next step out of tile 377 proves the four REMOVE bits changed eligibility exactly as intended: only the suffix EV_CLOSELINE line=102 remains eligible on EXIT, it closes the door through the normal four-frame door animation, and the MOVE to tile 409 commits.

The first implementation put the SHOW x4 journal inside EspNativeGameplayMoveEventResult, which overflowed the real loopTask stack during the unrelated four-frame door render and tripped the stack canary. The final implementation moved that journal to one bounded static owner. The same door then completed all four frames, Bull Demon combat/retaliation ran normally, and the event43 entry/exit sequence completed without reset.

Detailed record:

- [MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md](MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md)

### Previous merged boundary retained

The merged `main` base already contains the validated V7 Automap checkpoint
persistence, minimal neon-blue top-HUD touch locators and standalone
first-weapon acquisition help dialog. Those remain part of the hardware-proven
baseline and are not modified by this milestone.

### Next bounded frontier

This branch is merge-ready after the documentation-only tail. It did not finish
the intended structural consolidation; instead it established stronger generic
runtime behavior through Sector 1.

After merge, re-read the exact new `main` SHA and resume source/wrapper
consolidation from that SHA. Treat Entrance -> Junction -> Sector 1, Junction
zero-enemy SAVE/LOAD/PASS_TURN, and Level01 first movement as hardware
regression guards while reducing translation units, linker wrappers and legacy
implementation dependencies.

## Permanent architecture / hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, pointer-heavy desktop ownership, runtime ZIP dependence for migrated gameplay/map data, or map-wide decompression. `/DoomRPG-ESP32.pak` is the native backing store.

Gameplay/input and rendering stay decoupled. Doom RPG is turn-based; do not optimize `PlatformVideo_present()` prematurely.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Entrance raw-slot witness:

```text
pack = 2457398 B
entries = 241
index = 4820 B
metadata = 12288 B
excluded non-current BSPs = 12 / 203811 B
staged payload = 2248743 B
partition = 2752512 B
headroom = 491481 B
indexFNV = 3a51cc4d
payloadFNV = 9ec04e22
```

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Retained fresh-map fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

The save-v3 hardware mirror test also captured a deliberately mutated script snapshot with `scriptFNV=f9e59e9f`; this is a checkpoint-state fingerprint, not a replacement for the canonical fresh-map `f9e3d9df` above.

Resident cache baseline:

```text
owner = 23592 B
payload = 19456 B
range records = 288 x 12 B
resident entry slots = 24
large exact range = 2048 B
```

## Current hardware-owned gameplay frontier

Hardware-proven native behavior includes movement/turn/strafe, rotation-in-place without gameplay/monster turn advancement, collision/topology, event-first SELECT, bounded event/script families, dialog, regular doors, hardware-proven pure multi-line SELECT door batches and dynamic lines, mutable line textures, player state/resources, pickups, hazards, native weapon rendering/control/combat, outgoing player damage text plus bounded attack-frame blood spray, compact monster state/position/activation/movement/attack families, type-12/subtype-2 crate combat with exact transform RNG and transformed-pickup projection, type-12/subtype-1 barrel destruction with a bounded three-barrel real-CYD chain, concurrent sibling explosion waves and nonlethal player radius damage, the four-page HUB `INV/WPN/STAT/SYS`, raw-flash backing, bounded checkpoint save/load from both HUB and the main menu, resource consumed-overlay persistence, mutable script/event-state persistence, mutable line open/locked + texture-variant persistence, V5 action-owned removed-sprite persistence, checkpoint-resume HUD/cache/input rearm, HUB/world framebuffer ownership gating for transient action feedback, and the bounded native Automap core with live movement, visited-cell reveal, render-derived thin delimiters, pickup ownership retention and SELECT door interaction while the map owns the framebuffer.

The player root remains:

```text
EspNativeGameplayPlayerState = 52 B
```

The HUB root remains:

```text
EspNativeGameplayHubView = 28 B
pages = INV | WPN | STAT | SYS
WPN = complete 3x3 normal arsenal; source BGR565 -> framebuffer RGB565
STAT = read-only
SYS = dedicated two-step SAVE/LOAD checkpoint page
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

## Main-menu Load Game — REAL-CYD PASS

The CYD main-menu presentation is now:

```text
0 Start Game
1 Load Game
2 Options
3 Help/About
```

The original J2ME `Exit` row is gone. Double-tap confirmation on `Load Game`
calls the shared native checkpoint service. Historically, a readable V1-V6 record rebuilt
the immutable BSP, restores its versioned mutable owners and configures the
resume session directly in `ST_PLAYING`, without replaying the intro.

A missing or invalid record is fail-closed: the menu stays active, the selected
row displays red `No Save`, and its runtime framebuffer witness is rebased so
the menu remains interactive. The user confirmed both successful resume and
the no-save response on the real CYD at `18c1cfb`.

Detailed record:

- [`MILESTONE_MAIN_MENU_FINGER_FIRST.md`](MILESTONE_MAIN_MENU_FINGER_FIRST.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)

## Native checkpoint save/load v1 — REAL-CYD PASS

The original one-slot checkpoint established the permanent bounded save path:

```text
/sd/DoomRPG-ESP32.sav
magic = DRPGSAV1
version = 1
recordBytes = 132
atomic write = temp + verify + backup rename + commit rename + verify
```

V1 persists immutable BSP identity, full settled `EspPlayerViewState`, full `EspNativeGameplayPlayerState`, player/runtime fingerprints and CRC32. It never serializes pointers or a desktop object graph.

The final load reprime restores the semantic HUD owners directly after the saved settled view is reconstructed:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready mutation=owners-only turn=no
```

V1 hardware validation proved exact player/pose rollback and repeated-load stability, but map-local mutable owners were rebuilt fresh.

## Native checkpoint save/load v2 resources — REAL-CYD PASS

V2 preserves the exact proven v1 core and adds only one explicit pointer-free section:

```text
magic = DRPGSAV2
version = 2
recordBytes = 276
v1 read compatibility = retained
EspNativeGameplayPlayerResourcesSnapshot = bounded fixed section
max consumed payload = 128 B / 1024 sprites
Entrance used payload = 43 B / 344 sprites
```

The section persists only the semantic consumed-resource overlay plus explicit identity:

```text
sourceArenaFNV1a
spriteCount
consumedCount
consumedBytes
targetMapId
consumedBits[]
```

Hardware validation proved both directions:

```text
resource consumed before SAVE -> remains absent after LOAD
resource consumed after SAVE -> reappears after LOAD
```

The proof was exercised with Armor Shards and a Small Medkit through the normal native topology/render path, not merely as restored bookkeeping.

## Native checkpoint save/load v3 script — REAL-CYD PASS

V3 keeps the v1 core and v2 resource section and appends exactly one compact `EspMapScriptStateSnapshot`:

```text
magic = DRPGSAV3
version = 3
recordBytes = 808
v1/v2 read compatibility = retained
write format = v3
script snapshot max payload = 512 B
```

The script section persists pointer-free semantic bytes only:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
storage[] = packed event states + removed-command bits
```

Entrance uses:

```text
events = 93
byteCodes = 265
script storage = 81 B
```

The snapshot/restore API validates runtime identity, exact counts/sizes and unused tail bytes, restores only into the freshly rebuilt native script owner, and verifies the restored semantic fingerprint before session configure. Corrupt or incompatible sections fail closed.

The final hardware mirror SAVE captured already-mutated event state and three consumed resources:

```text
[NATIVESAVE] SAVE ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
             world=resources+script-restored+others-fresh
```

Gameplay then diverged after SAVE by consuming a Bullet Clip and Fire Ext; live state reached `playerFNV=a6e115a7`, weapon 1, weapons `0006`, ammo0=10, ammo1=12 and five consumed resources.

LOAD restored the exact checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] LOAD ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
             world=resources+script-restored+others-fresh
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

The strongest semantic witness came immediately after LOAD: event 79, which had been completed **before** SAVE, remained ineligible instead of reopening:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

Together with the earlier opposite-direction test where post-SAVE dialog/event mutations were rolled back and became executable again, V3 proves:

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)

### Historical v3 world boundary (superseded by V4)

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
```

Still intentionally fresh / not yet persisted:

```text
line open/locked state
line texture variants
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue save persistence owner-by-owner. Never replace this with a monolithic world/object dump.

Long-session hardware steady point observed during the V3 validation:

```text
heap8 = 17724
largest8 = 8692
```

This remained stable across the final LOAD/replay segment. Fragmentation/headroom remains below the advisory target and should stay on the review list, but the run does not demonstrate a new per-LOAD leak.

## Native checkpoint save/load v4 lines — REAL-CYD PASS

V4 keeps the proven v1/v2/v3 semantic sections and appends one compact line-family snapshot:

```text
magic = DRPGSAV4
version = 4
recordBytes = 1212
v1/v2/v3 read compatibility = retained
write format = v4
line count max = 1024
line bitset max = 128 B
Entrance lines = 480
Entrance bitset bytes = 60
```

The section persists only:

```text
open bit per line
locked bit per line
mutable locked/unlocked texture-10 variant bit
runtime identity + exact line count/size
line-state and texture-state fingerprints
```

The real-CYD load first rebuilt canonical Entrance line owners at `locked=7`, `texture10=0`, then restored the saved soldier-door state:

```text
[MAPLINECHECKPOINT] RESTORE ... lines=480 bytes=60
                    open=0 locked=6 texture10=1
                    lineFNV=69334d90 textureFNV=bda09634
[NATIVESAVE] LOAD ... version=4 bytes=1212
             resources=restored/5/43B
             script=restored/93/265/81B/26f291e3
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
```

After LOAD, line 352 opened normally with `locked=0` without replaying the soldier unlock script:

```text
[ACTION] DOOR line=352 opcode=15 status=OK open=0->1 locked=0 ...
[DOORANIM] COMPLETE ... transaction=committed
```

The first V4 hardware attempt also exposed a `loopTask` stack-canary reset when entering STAT. The final code boundary moved the large read workspace out of the HUB stack, removed large full-record CRC/verification copies, and passed both CI and the real-CYD INV -> STAT -> LOAD sequence without reset.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)

### Historical v4 world boundary (superseded by V5)

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
```

Still intentionally fresh / not yet persisted:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Post-LOAD session invariants remained intact:

```text
shapeData == NULL
mediaTexels == NULL
heap8 = 14076
largest8 = 6644
```

## Native checkpoint save/load v5 action removals — REAL-CYD PASS

V5 preserves the proven V1-V4 sections and appends one compact
`EspNativeGameplayActionRemovedSnapshot`:

```text
magic = DRPGSAV5
version = 5
recordBytes = 1356
V1/V2/V3/V4 read compatibility = retained
write format = V5
max removed-sprite payload = 128 B / 1024 sprites
Entrance payload = 43 B / 344 sprites
```

The section owns only the action engine's semantic removed-sprite bitset plus
runtime/map identity and a semantic fingerprint. It does not serialize legacy
entities, renderer objects or a generic mutable world graph.

Real-CYD SAVE witness:

```text
[NATIVESAVE] SAVE ... version=5 bytes=1356
             pos=288,1248 angle=0
             playerFNV=15cb16e4 runtimeFNV=c3882516
             resources=5/43B
             script=93/265/81B scriptFNV=26f291e3
             lines=480/60B open=1 locked=6 texture10=1
             lineFNV=c50b0721 textureFNV=bda09634
             actionRemoved=1/43B/a54be373
```

LOAD rebuilt the immutable Entrance runtime, restored the exact checkpoint
owners and reproduced the same action-removal fingerprint:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[PLAYERRES] RESTORE ... consumed=5 bytes=43
[MAPLINECHECKPOINT] RESTORE ... open=1 locked=6 texture10=1
[NATIVESAVE] LOAD ... version=5 bytes=1356
             actionRemoved=restored/1/43B/a54be373
[ENGINESESSION] RESUME checkpoint=restored freshFirstFrame=skipped dynamicLines=gameplay-wrapper
[RESIDENTGAMEPLAY] READY map=current entry=checkpoint-resume ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The user visually confirmed that the fire cleared before SAVE stayed absent
after LOAD while another fire that had never been cleared remained lit. A
stronger opposite-direction mirror (clear a second fire after SAVE and prove it
returns on LOAD) was not exercised in the supplied log and is not claimed.

The final code boundary is:

```text
d65e5b9be9947e92c700b2296790b003ff7b7df0
esp32-cyd #387 / 35704985512 = SUCCESS
REAL-CYD = PASS
```

### Current V5 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
EspNativeGameplayActionEngine action-owned removed-sprite overlay
```

Still intentionally fresh / not yet persisted:

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
serialize RNG state, so this is not listed as a missing original-save field.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

## HUB/action-feedback framebuffer ownership — REAL-CYD PASS

The v2 hardware test exposed an unrelated visual ownership race: a pickup top-bar message could expire while HUB owned the framebuffer, leaving a stale `Got ...` fragment over the MENU area and causing a later underlay mismatch/recover path.

The bounded fix is:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
  ESP32: pause action feedback while HUB owns framebuffer
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
  ESP32: gate world feedback service behind HUB ownership
CI #267 = SUCCESS
```

The timer remains based on real elapsed time, but world feedback/viewport-flash restore work is blocked while HUB owns the framebuffer. The user reproduced the original pickup-message/HUB sequence on the real CYD and confirmed the stale fragment is gone. Therefore `5a1020fd5d160c111ff09ecb8a480f37ea8d0578` is a hardware-valid code boundary for this ownership fix.

## Entrance -> Junction CHANGEMAP — REAL-CYD PASS

Final hardware-tested branch boundary:

```text
code head = e4acb92403dd48810f3c4e989d4dee16605125e3
main = 6cd8b6804cbec75538becab0d6cbe66e3c79d238
CI #829 = SUCCESS
RAM static = 45200 B
Flash = 789345 B
artifact id = 10873930025
```

Entrance event 1 / tile 69 now owns the first hardware-validated native
world-to-world transition:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> line 459, shared regular four-frame animation
```

The real CYD proved the complete bounded route: exact target BSP inventory is
read through a scoped authoritative-SD source probe while Entrance raw-flash
gameplay backing remains untouched; WAIT_STATS uses the current explicit one-tap
bridge; map-flash world identity then MISSes map 1 and rebuilds map 9 before the
source runtime is destroyed; Junction rebuilds from raw internal flash, spawns
at tile 943 / position 992,1888 / angle 64, primes the resident cache and reaches
the generic gameplay service with `shapeData=0x0` and `mediaTexels=0x0`.

The final hardware run reconfirmed the first Junction movement. EXIT tile 943
contains a locked `CLOSELINE`; matching legacy behavior, that failed line close
does not abort movement. ENTER tile 911 contains opcode 4 `MESSAGE`; the native
transaction commits the destination frame first and only then publishes
`"Junction"`. The player reaches tile 911 at position 992,1824 and gameplay
remains active.

The same final code head then proved that Junction dialog/continuation gameplay
remains live after the transition. Facing the Scientist on tile 878 selected
event 56 as `DIALOG_READY`; the native dialog opened a 97-byte / six-line
opcode-8 payload, handled fast-forward and page advance, closed with the pack
released, then resumed command offset 1. `DIALOGCHAIN` executed one bounded
state command and `DIALOG-RESUME` reported opcode 11 / `CHANGESTATE` with
`stateMutation=1`, followed by a successful world redraw. The dialog pipeline
now preserves the live PlayerState key context across validation, NOTE-prefix
filtering and continuation planning. The earlier Marine event 45 regression
that exposed the key-context mismatch was not itself replayed in the final
supplied trace; the final hardware witness is Scientist event 56.

Post-PASS code review found one failure-only rollback leak: if the shared
`SELECT-DOOR` render failed after staging the transition door, the line/script
rollback could succeed while the transition owner remained in `waitingDoor`.
The rebased code head now aborts that owner after successful world/script
rollback and before rendering `SELECT-DOOR-ROLLBACK`. The hardware-proven
successful path is unchanged; the reviewed render-failure path itself was not
hardware-triggered.

The statistics screen presentation itself is still deferred; only the explicit
WAIT_STATS acknowledgement bridge is claimed here. Generic Junction -> level
exits and unrelated opcode-4 MESSAGE routes remain separate milestones.

Detailed record:

- [`MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md`](MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md)

## Native rotation no-turn parity — REAL-CYD PASS

The legacy/J2ME split is now restored:

```text
movement completion -> advances gameplay turn
rotation completion -> updates facing/view only
```

Hardware-tested code boundary:

```text
b548321f477626777800371f0f82a9f3c2375bd9
```

The real-CYD test confirmed that turning in place no longer advances monster behavior. The same supplied hardware run reconfirmed that legitimate turn-producing actions still do:

```text
PLAYER_ATTACK -> [MONSTERTURN] SCHEDULE n=32 reason=PLAYER_ATTACK
MOVE          -> [MONSTERTURN] SCHEDULE n=33 reason=MOVE
MOVE          -> [MONSTERTURN] SCHEDULE n=34 reason=MOVE
MOVE          -> [MONSTERTURN] SCHEDULE n=35 reason=MOVE
```

The correction adds no allocation, gameplay RNG use, topology mutation, renderer mutation or player mutation on rotation. Detailed record:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)

## Next bounded milestone

After this branch merges, re-read the exact resulting `main` SHA before
opening the next `agent/*` branch.

Resume structural consolidation rather than adding another map-specific
progression patch. Consolidate by permanent native domain, remove native->native
linker wrappers where direct composition is clearer, and reduce the remaining
legacy implementation-unit dependency family by family. The newly proven
Junction -> Sector 1 path must remain a non-regression witness.

## Hardware-validated intro display polish

Real-CYD visual validation on code head `0d21332bd2524bca73d5284f70e053ca8ba6430d` confirmed the permanent native intro fit split:

```text
logical framebuffer = 160x120 RGB565
content viewport     = 120x120 centered at x=20
starfield background = 160x120 full width
animated intro scene = 160x120 full width
story text           = 156x120 centered at x=2
extra framebuffer    = 0 B
```

The dedicated animated intro scene now maps its starfield/layers/planet/spaceship/line geometry across the full logical display, while ordinary story-page decoration remains in the aspect-preserving 120x120 content viewport. Story glyphs use the separately tuned 156-pixel soft-wide horizontal mapping.

The real-CYD visual verdict for this exact split was PASS ("Superbe"). The production geometry witness is:

```text
[INTROFIT] ... content=120x120@(20,0) background=160x120@(0,0) animation=160x120@(0,0) text=156x120@(2,0) fit=aspect-content+full-animation+soft-wide-text extraFrameBytes=0
```

CI for the hardware-tested head retained the canonical no-PSRAM static-RAM boundary:

```text
RAM static = 44832 B
Flash      = 724633 B
```

No second framebuffer or frame-sized staging allocation was introduced.

## Hardware-validated pure multi-line SELECT door batch

Real-CYD validation on `/intro.bsp` proved the bounded pure-door batch path used by the hidden/secret door.

The SELECT hit tile `195`, event `10`, whose complete command sequence is exactly two eligible `EV_OPENLINE` commands:

```text
[ACTION] SELECT seq=139 status=DOOR_OK tile=195 event=10 eligible=2 unsupported=0
[DOORANIM] SNAP line=471 open=0->1 flags=00000928 reason=non-regular-door
[DOORANIM] SNAP line=470 open=0->1 flags=00001110 reason=non-regular-door
[ACTION] DOOR-BATCH event=10 count=2 status=OK [0]line=471/op=15/open=0->1/removed=0->1 [1]line=470/op=15/open=0->1/removed=0->1
[RESIDENTGAMEPLAY] SELECT n=10 seq=139 doors=2 firstDoor=471 committed=yes redraw=yes collision=live animation=bounded-batch sound=deferred entityRelink=deferred turnAdvance=deferred
[DYNAMICLINES] FRAME angle=64 open=4 adaptedReads=6 animatedReads=0 textureVariants=0 render=ok immutableRuntime=yes
```

Both lines are non-regular door geometry, so the native animator intentionally reports `SNAP` rather than scheduling the four-frame regular-door animation. The line-state transaction still commits both open bits and both remove-if-handled command bits atomically.

Walking through the opened secret-door tile then sees the event as exhausted:

```text
[MOVEEVENT] ENTER-PREFLIGHT ... tile=195 ... status=NO_ELIGIBLE event=10 eligible=0 ...
[MOVEEVENT] EXIT-PREFLIGHT ... tile=195 ... status=NO_ELIGIBLE event=10 eligible=0 ...
```

This validates the permanent rule: a SELECT event may execute a **pure** bounded batch of up to eight eligible line commands (`EV_MOVELINE/OPENLINE/CLOSELINE/MOVELINE2`), matching the legacy/native `openDoors[8]` capacity. The complete batch is previewed before mutation; mixed-family events, duplicate-line batches requiring sequential intermediate-state semantics, and batches beyond the bound remain fail-closed.

Hardware runtime after the traversal remained resident and stable in the submitted log:

```text
[ALIVE] ... heap=78832 heap8=13280 largest8=12276 ... MAPPINGS=ready MENUBSP=ready
```

CI for the validated code head retained the canonical static-RAM boundary:

```text
RAM static = 44832 B
```

## Native transition presentation — REAL-CYD PASS (2026-09-26)

The reusable native transition presentation is hardware-validated at
`a81dd38a6875154b37b9006a145eef5d73e85a66`.

It is shared by both native CHANGEMAP and checkpoint LOAD. The public owner
accepts target-map identity and progress/stage updates; the current visual skin
remains internal and fixed (`c.bmp` first frame, mini-HUB font, compact
amber/steel card and progress bar). This means later font/theme polish can be
implemented as a bounded presentation/config change without altering either
loading caller.

The final checkpoint regression was an ownership bug: session/input reset
requested a NULL touch callback, and the WAIT_STATS bridge reset the unrelated
checkpoint loading owner. The corrected bridge preserves an already-active
checkpoint loading presentation.

The permanent `Esp32PlatformVideo_present` wrapper now blocks gameplay
publication before Action/GIB/HIT decorators while loading owns the screen.
TransitionPresentation itself uses the real present leaf for its own progress
frames.

The same hardware run exposed a second issue: a monster already dead in the V8
checkpoint replayed its GIB effect on resume. Checkpoint resume now seeds the
bounded GIB presentation owner from restored monster state before the first
gameplay present. Dead restored monsters are historical; live monsters remain
eligible for future genuine death bursts.

Accepted visual result on the real CYD:

```text
loading remains full-screen through checkpoint/session priming
top gameplay bar does not leak through
bottom gameplay HUD does not leak through
restored death GIB does not replay
first visible gameplay frame appears only after loading release
```

Build witness:

```text
esp32-cyd CI #867 = SUCCESS
RAM static = 45224 B
Flash      = 795461 B
```

Detailed record:
[MILESTONE_NATIVE_TRANSITION_PRESENTATION.md](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md)

## Intentionally deferred / incomplete families

```text
save-v6 mutable-world persistence beyond each validated section
CHANGEMAP/script shapes beyond hardware-proven Entrance -> Junction -> Sector 1
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
audio
pickup sound / got-face
secondary hazard feedback
complete mixed movement resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation
simultaneous multi-monster ordering
special subtype-10 AI
player lethal/death transition
multi-loop weapon/monster mechanics
monster projectiles/messages/sound
rocket/BFG radius damage
familiar weapon slots / hazard redirection
remaining type-12 destructible/radius families beyond barrel/crate/jammed-door
special death consequences
Kronos-specific semantics
password late full-HUD repaint replay
EV_GIVEMAP hardware execution + remaining Automap action parity
Automap reveal-state checkpoint persistence
EV_CHECK_KEY production route
HUB Notebook activation
HUB consumable confirmation/use
HUB Automap / Options / store
```

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior family
 -> recover exact legacy behavior where relevant
 -> design a small permanent native API/owner
 -> keep different families fail-closed
 -> commit + push agent/*
 -> CI esp32-cyd
 -> test on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

Never merge into `main` without explicit user request.


## V8 monster-topology recovery + V9 spatial checkpoint — REAL-CYD PASS for V8 compatibility

Hardware-tested code boundary:

```text
7b3efeb8d590c027b94f08ac7c31c886938709d4
esp32-cyd CI #980/#981 = SUCCESS
```

This branch retains the complete prior Junction census history through
`49ad2fe9d63f454611f6d41a729e62905f456251`.

A real classic CYD successfully loaded an existing V8 checkpoint created after
the yellow-card trigger had revealed hidden monsters. The missing V8 spatial
owner was reconstructed only from durable script evidence: removed-command bit
set, REMOVE-if-handled set, opcode SHOW/HIDE. The yellow-card monsters were
present again after LOAD and normal gameplay resumed.

The branch also introduces the bounded V9 monster spatial record:

```text
MonsterState
monster topology (linked/unlinked, tile, link order, visual/alive bits)
MonsterPosition
MonsterActivation order
```

The final real-CYD V9 round-trip is now hardware-proven at
`7b3efeb8d590c027b94f08ac7c31c886938709d4`. Entrance writes a 43-record
topology snapshot covering its 30 enemies plus 13 destructibles, then restores
that exact scope on LOAD:

```text
SAVE  topology=43/7a4b0217 monsters=30/dcda5880
LOAD  tracked=43 enemies=30 destructibles=13
      scope=enemy+destructible-v9
      topologyFNV=7a4b0217
      positionFNV=a369df86
      exact=yes
ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
```

The review corner where EV_SHOW kills an enemy blocker is reconciled in the
checkpoint MonsterState snapshot only; no live gameplay side effects are
fabricated. EV_SHOW-mutated destructibles are included in the V9 topology
owner. The provisional enemy-only V9 remains readable through bounded one-shot
SHOW/HIDE replay compatibility.

Validation boundary: V8 compatibility recovery, V9 SAVE, and V9 LOAD of the
final enemy+destructible spatial checkpoint are all hardware-proven.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md)

## Native main-menu SELECT consolidation — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
cd5f24dd0ff538a85cebc02025de48cb5998401e
esp32-cyd CI #1135/#1136 = SUCCESS
RAM static = 45768 B
Flash      = 801373 B
firmware.bin = 801744 B
```

The finger-first MENU_MAIN no longer routes selections through
`MenuSystem_select()` or `Menu_select()`. The four visible actions are now
composed explicitly as START / LOAD / OPTIONS / HELP by the native dispatcher.

Real-CYD witnesses cover START -> fresh intro -> Entrance gameplay, valid V9
LOAD -> Sector 1 checkpoint resume, OPTIONS -> Back, HELP page-down/page-up ->
Back, and the common failed-dispatch recovery path. Help now owns both bounded
PAK parsing and bounded native paging over its 83 logical records; visual polish
is deferred.

Pre-merge review hardening added:
- bounded `help.txt` parsing with physical-size/item/line checks;
- common MENU_MAIN repaint + touch re-arm recovery while ST_MENU still owns UI;
- typed LOAD results: NO_SAVE / RECOVERED / TRANSITIONED / FATAL;
- shared `native_main_menu_present` hash/graphics-boundary primitives.

The first bounded-parser attempt intentionally failed closed on the real CYD
when it over-read past the 83 logical Help records; the log proved recovery
returned to framebuffer `522dc605` with touch rearmed. The corrected
`cd5f24dd...` parser was then re-tested successfully on hardware.

Final source review also removed a dead legacy START save precheck.
`Game_checkConfigVersion()` tests desktop `Config/Player/Player2/World`
files, not the native `/DoomRPG-ESP32.sav`. START is therefore now
unconditionally new-game-only and LOAD is the sole resume owner.

Final ELF:

```text
Menu_select        absent
MenuSystem_select  absent
```

Remaining explicit transitional seams include `MenuSystem_back()`,
`MenuSystem_setMenu()`, `Menu_initMenu()` and `Menu_startGame()`.
The natural next consolidation target is `MenuSystem_back()`.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md)

## Native main-menu Back consolidation — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
8d9b6d25f481d76fe4c79bc1af99d59bc314328f
esp32-cyd CI #1149 = SUCCESS
RAM static = 45768 B
Flash      = 801425 B
firmware.bin = 801792 B
```

HELP and OPTIONS no longer return through `MenuSystem_back()`. Both use the
shared semantic `DoomRPG_esp32MainMenuReturnToMain()` owner, which validates
the expected child/ST_MENU boundary, plays the exact Back cue 5042, rebuilds
MENU_MAIN through the native model owner, performs the opaque main repaint, and
requires exact framebuffer/touch/graphics invariants before success.

Real-CYD HELP proof:

```text
[MAINBACK] READY source=help child=2->1 frame=522dc605 touch=rearmed ... sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[MAINHELP] READY back=native mainFNV=522dc605 touch=armed
```

Real-CYD OPTIONS proof:

```text
[MAINBACK] READY source=options child=7->1 frame=522dc605 touch=rearmed ... sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[OPTIONBACK] FAST End framebufferFNV=522dc605 expected=522dc605 runtimeFNV=522dc605 menu=1 selected=0 touchActive=1 shapeData=0x0 mediaTexels=0x0
```

Heap stayed at `heap8=20916`, `largest8=10740` in both submitted traces.

Final ELF:

```text
MenuSystem_back       absent
MenuSystem_setMenu    present (445 B)
MenuSystem_playSound  present
```

`MenuSystem_setMenu()` remains a separate, broader seam with retained callers
outside this main-menu Back boundary.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md)



## Legacy DoomRPG_Init link-anchor retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
code head = f9ab3bda2cd9aadee2d1be0fc08d49600b7a9141
esp32-cyd CI #1159 = SUCCESS
RAM static = 45768 B
Flash      = 797697 B
firmware.bin = 798064 B
artifact id = 11101108652
firmware sha256 = 0da05325e6549e80fd887e6bab9e398a321022ff0718f47c0303f00671432d43
ELF sha256 = ea56e4757e9ff940a245fd6963d91a9193e44f4cad06463b347866287aefb8c8
```

The `MenuSystem_setMenu()` audit separated source-level call sites from the
actual linked ESP32 runtime. Source still contains legacy callers in menu,
death, credits, map-stats, store and cheat/debug paths, but the V9 ELF showed
that all of those enclosing functions were already dead except
`DoomCanvas_setupmenu()`.

The remaining linked chain was not a live runtime owner:

```text
main.cpp diagnostic
 -> DoomRPG_engineLinkAnchor()
 -> &DoomRPG_Init
 -> DoomCanvas_setupmenu()
 -> MenuSystem_setMenu()
```

`DoomRPG_engineLinkAnchor()` existed only so the bring-up diagnostic could
print the address of the inherited monolithic `DoomRPG_Init()`. The actual
ESP32 runtime already constructs and starts the engine through the staged
`DoomRPG_initEngineCore()`, layout/startup owners and native menu/gameplay
handoffs. The branch therefore removes only the diagnostic anchor and its API;
it does not introduce a replacement menu router.

Exact ELF comparison against the hardware-tested V9 artifact:

```text
V9 present:
  DoomRPG_Init              597 B
  DoomRPG_engineLinkAnchor    8 B
  DoomCanvas_setupmenu      132 B
  MenuSystem_setMenu        445 B

V10:
  DoomRPG_Init              ABSENT
  DoomRPG_engineLinkAnchor  ABSENT
  DoomCanvas_setupmenu      ABSENT
  MenuSystem_setMenu        ABSENT
  MenuSystem_back           ABSENT
  MenuSystem_select         ABSENT
  Menu_select               ABSENT

still intentionally present:
  Menu_initMenu             PRESENT
  MenuSystem_playSound      PRESENT
```

The link anchor retirement removes 17 global legacy functions in total and adds
no new global function. Besides the four symbols above, the dropped closure
includes `DoomCanvas_LoadMenuMap`, `DoomCanvas_unloadMedia`,
`Render_setGrayPalettes`, `Game_loadMapEntities`,
`Entity_initspawn`, `MenuSystem_moveDir` and `Player_selectWeapon`.
Linked flash and `firmware.bin` both shrink by 3728 B relative to V9 while
static RAM remains unchanged.

The real classic CYD validates the retained runtime after that link closure is
gone. HELP pages down/up and returns through the native Back owner; OPTIONS
returns through the same owner; both reproduce MENU_MAIN framebuffer
`522dc605`, keep `heap8=20916` / `largest8=10740`, and retain
`shapeData=0x0 mediaTexels=0x0`.

START then traverses the full native path:

```text
MENU_MAIN -> START
 -> ST_INTRO
 -> native intro clock/input
 -> bounded intro disposal
 -> TransitionPresentation loading
 -> native /intro.bsp resident runtime
 -> native spawn/session/cache priming
 -> ENGINESESSION READY map=1
```

The final session witness is:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] uptime=64666 ms heap=92492 heap8=26824 largest8=18420 ...
[ALIVE] uptime=69669 ms heap=92492 heap8=26824 largest8=18420 ...
```

The submitted runtime excerpt starts after cold boot, so it does not contain the
new informational `[LEGACYINIT] ... anchor=retired` line. That line is not
claimed as a hardware witness. Static ELF inspection proves the link retirement;
the real-CYD run proves the retained menu/intro/native-gameplay runtime remains
functional without that closure.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md`](MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md)


## Native main-menu model ownership — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
source-consolidation head = e5c52be0ed24c57df65477a1bed4efbf914d8320
final code head = 8cd0b019ebcb80491a27c2ed0bad3cf57f9ad47e
esp32-cyd CI #1175 = SUCCESS
RAM static = 45224 B
Flash      = 782265 B
firmware.bin = 782624 B
artifact id = 11110371227
artifact digest = sha256:31e9f777f42f22917fde7a72421c7e216b285a1cdf941949540722532af178c3
firmware sha256 = d66e4a1eac3f7911046916de9aa89dcb9a7d4c8e944f9424f7747534c3f181d0
ELF sha256 = 00d14a306c726d44737beca1a14aaab395ecd1aa09c0d0d7c7d92b3b84c86d71
```

The hardware-validated main-menu implementation is now consolidated from 13
`native_main_menu_*.c` translation units to 7 without adding a replacement
wrapper layer. LOAD lives with main actions, the tap gate lives with touch, and
presentation/recovery/scene bridge code lives with the touch-layout owner.
The obsolete pre-touch layout and historical overlay probe sources are gone.

On top of that consolidation, the three active ESP32 call sites of
`Menu_initMenu()` were replaced by bounded model construction in the existing
`native_main_menu_model.c`. The native owner supports only the pre-game
models actually needed here: fixed MAIN, fixed CONTINUE, fixed OPTIONS and the
already-bounded PAK-backed HELP parser. It does not recreate a generic menu
router.

Authoritative final-ELF inspection:

```text
Menu_initMenu                         ABSENT
Menu_LoadHelpResource                 ABSENT
Menu_startGame                        PRESENT  0x49 = 73 B
DoomRPG_esp32MainMenuModelBuildMain   PRESENT  0xbb = 187 B
DoomRPG_esp32MainMenuModelEnter       PRESENT  0x45f = 1119 B
```

The linker can therefore discard the broader legacy menu-construction closure,
including `Menu_fillStatus`, `Menu_setStore`, `Menu_setNotes`,
`MenuSystem_buildDivider`, `Menu_setYesNo` and the static
`vendingMenuTable`.

Relative to the hardware-tested source-consolidation baseline
`e5c52be0...`:

```text
                 e5c52be       8cd0b019       delta
static RAM       45768 B        45224 B        -544 B
Flash           797689 B       782265 B      -15424 B
firmware.bin    798048 B       782624 B      -15424 B
```

The real CYD exposes the RAM reduction directly. The stable menu heap moves
from `heap8=20916` to `heap8=21460`, exactly +544 B, and the final gameplay
ALIVE moves from `heap8=26824` to `heap8=27368`, also exactly +544 B.

Cold boot now also provides the previously-missing hardware witness for the V10
anchor retirement:

```text
[LEGACYINIT] STAGED runtime=ESP32-core/layout/startup legacy-DoomRPG_Init-anchor=retired
```

The new model owner preserves the validated presentation fingerprints:

```text
MENU_MAIN modelFNV = 292c7f95
MENU_MAIN finalFNV = 522dc605
HELP page0 FNV     = 5f22cf6b
HELP page8 FNV     = d0788359
OPTIONS modelFNV  = e1ef01f7
OPTIONS frameFNV  = 162d3999
```

Real-CYD traces show `builder=native-fixed-main` on HELP/OPTIONS Back and
`builder=native-fixed-options` entering OPTIONS, with
`shapeData=0x0 mediaTexels=0x0`. START then traverses the complete intro,
bounded disposal, Entrance resident load/cache prime and reaches
`[ENGINESESSION] READY map=1`; final ALIVE is stable at
`heap=93076 heap8=27368 largest8=18420`.

The local PlatformIO upload build reported 45224 B RAM, 782281 B Flash and a
782640 B firmware image; the 16-byte absolute Flash/image difference from CI
does not change the exact -15424 B CI delta or the final ELF symbol result.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md)


## Native START + intro escape closure retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 4c6071ebe7de01f47925bf7792123e8c8f9d7ff5
branch = agent/esp32-consolidation-main-menu-start-v12
code head = 6b565cd46172e384209a1d93e355c951f4d6c4fa
esp32-cyd CI #1185 = SUCCESS
RAM static = 45224 B
Flash = 782009 B
firmware.bin = 782368 B
artifact id = 11113402480
```

V12 removes the last legacy pre-game START/intro escape closure without adding
a file or a generic router.

`native_main_menu_start_action.c` now owns only the ESP32 new-game path:
`imgBG=NULL -> Player_reset -> totalDeaths=0 -> ST_INTRO`. The dedicated LOAD
card remains the only resume owner, and `skipIntro != 0` fails closed before
menu cleanup or player mutation rather than entering the desktop load-map path.

`native_story_fit.c` no longer delegates page changes to
`DoomCanvas_changeStoryPage()`. The renderer owns only the automatic bounded
animation transition `storyPage 1 -> 2`. The final page-2 Continue remains
owned by the native input/clock boundary:
`Esp32IntroClock_park("intro-exit-ready") -> Esp32IntroDispose_service()`.

Authoritative final-ELF inspection:

```text
Menu_startGame               ABSENT
DoomCanvas_loadState         ABSENT
DoomCanvas_changeStoryPage   ABSENT
DoomCanvas_disposeIntro      ABSENT
DoomCanvas_loadMap           ABSENT
```

Against merged main:

```text
                 main 4c6071e    V12 6b565cd    delta
static RAM       45224 B         45224 B         0 B
Flash           782265 B        782009 B      -256 B
firmware.bin    782624 B        782368 B      -256 B
```

The real classic CYD exercises the exact new boundary. After entering page 1,
no touch is sent; the native clock advances it after about 10.1 s:

```text
[INTROIN] CONTINUE storyPage=0->1 t=1443700 epoch=1443700
[INTROCLK] AUTO-PAGE 1->2 t=1453800 textPage=0 epoch=1453800
```

The final Continue then stays on the native disposal/bootstrap path:

```text
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=1481600 fullTextPresented=yes
[INTROCLK] PARK reason=intro-exit-ready ... heap8=43104 largest8=12276
[INTRODISP] READY ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

Detailed record:
[`MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md`](MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md)


### Explicit intro-dispose -> native-startup composition — REAL-CYD PASS (2026-09-30)

Commit `0f733d1a6ac680b0ff3f7954f4e40bcf44f942f8` removes the two active
linker interceptions around the already resource-only intro disposer:

```text
--wrap=Esp32IntroDispose_reset
--wrap=Esp32IntroDispose_service
```

The same ordering is now explicit at the sole live owner boundary:

```text
Esp32IntroClock_arm
 -> Esp32IntroDispose_reset
 -> EspNativeStartup_reset

Esp32IntroClock_service after intro-exit-ready
 -> Esp32IntroDispose_service
 -> EspNativeStartup_service
```

No new translation unit or generic router was added. CI #1191 succeeds with
unchanged 45224 B static RAM, 782009 B linked Flash and 782368 B firmware.bin.
The final ELF has 55 active `__wrap_*` symbols, down from 57 on merged main.
`Esp32IntroDispose_reset/service` and `EspNativeStartup_reset/service` are
present, while `__wrap_Esp32IntroDispose_reset/service` are absent. The V12
retired symbols `Menu_startGame`, `DoomCanvas_loadState`,
`DoomCanvas_changeStoryPage`, `DoomCanvas_disposeIntro` and
`DoomCanvas_loadMap` remain absent.

The real classic CYD validates the complete path. Native START arms the bootstrap,
the untouched page-1 animation advances automatically to page 2, final Continue
parks the clock, and bounded disposal recovers exactly 33772 B of 8-bit heap
without changing the framebuffer or loading a map:

```text
[NATIVEBOOT] RESET generic resident bootstrap armed
[INTROCLK] AUTO-PAGE 1->2 ...
[INTROCLK] PARK reason=intro-exit-ready ...
[INTRODISP] READY ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
```

The explicit startup service then takes ownership, builds Entrance and reaches
resident gameplay with legacy render pools still absent:

```text
[NATIVEBOOT] LOADING-TAKEOVER map=1 source=intro-disposed owner=transition-presentation
[NATIVEBOOT] RESIDENT map=1 ... arena=14095 ...
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md](MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md)


### Production menu.bsp runtime retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code head
`dd4161a40b28d2ed9c88370b2f8281b9045f2980` removes the historical
`menu.bsp` structural runtime from the normal `esp32-cyd` boot path.

Before V14, normal boot still built the legacy menu map structures through
`Render_beginLoadMap(MAP_MENU)` / `Render_beginLoadMapData()`, intercepted
the seventh `DoomCanvas_updateLoadingBar()` callback with `longjmp`, then
discarded the resulting 3D menu scene visually by painting the opaque native
dashboard on top.

The normal production path is now direct:

```text
config + immutable mappings
 -> DoomRPG_esp32MainMenuModelBuildMain
 -> DoomRPG_esp32RepaintOpaqueMainMenu
 -> native touch owner
```

The historical menu BSP structure/wall/sprite suite remains available only in
the explicit `esp32-cyd-bringup` profile. Its four linker compatibility flags
are no longer production flags.

CI #1197 succeeds with:

```text
                         merged main     V14           delta
static RAM               45224 B         45128 B        -96 B
linked Flash            782009 B        773089 B      -8920 B
firmware.bin            782368 B        773456 B      -8912 B
active __wrap_*              55             52            -3
```

Final-ELF inspection of the CI artifact confirms these legacy production symbols
are absent:

```text
DoomRPG_probeMenuBspHeader
DoomRPG_probeMenuMapRuntimeStructures
DoomCanvas_updateLoadingBar
Render_beginLoadMap
Render_beginLoadMapData
__wrap_DoomCanvas_updateLoadingBar
__wrap_Render_beginLoadMap
__wrap_longjmp
```

The permanent native menu owners remain present:

```text
DoomRPG_esp32MainMenuModelBuildMain
DoomRPG_esp32RepaintOpaqueMainMenu
```

Real classic-CYD cold boot proves the new production boundary:

```text
[CONFIGMAP] Render_beginLoadMap / BSP still NOT executed
[MAINOPAQUE] ... finalFNV=522dc605 ... heap8=35656 largest8=23540
[MAINBOOT] READY owner=native-opaque menuBspRuntime=skipped legacyMapStructures=not-created frame=522dc605
[ALIVE] ... heap=101580 heap8=35656 largest8=23540 ... MENU=ready
```

No `[MENUBSP]` or `[MAPSTRUCT]` stage appears in the normal boot log.
OPTIONS -> Back and HELP -> Back both restore exact MENU_MAIN FNV
`522dc605` with unchanged `heap8=35656 largest8=23540` and
`shapeData=0x0 mediaTexels=0x0`.

START confirms that the old menu map structures were never created:

```text
[MAINSTART] Begin ... heap8=35656 largest8=23540 shapeData=0x0 mediaTexels=0x0
[MAINMENU] Runtime cleanup ... nodes=0x0 lines=0x0 mapSprites=0x0 ... shapeData=0x0 mediaTexels=0x0
```

The unchanged intro/disposal/bootstrap path then reaches Entrance with the
memory invariants intact:

```text
[INTRODISP] READY ... heap8=43200->76972 recovered=33772 ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
```

A following real FORWARD move commits tile `904 -> 872`, renders the new
frame, services the monster turn, and leaves ALIVE stable at the same values.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md](MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md)


### First-frame fidelity wrapper retirement from production — REAL-CYD PASS (2026-10-01)

Hardware-tested code head
`5460c689b708468e3bdd618d0000753159a24109` removes the production
`--wrap=EspNativeFirstFrame_route` interception while preserving the real
native first-frame owner unchanged.

The removed production wrapper did only post-success diagnostics:

```text
EspNativeFirstFrame_route
 -> read-only viewport COLORSTATS
 -> remember Render* for optional BMP export
```

It did not own renderer state, first-frame publication, FNV calculation,
presentation, failure handling or gameplay transition. Those remain in the
real `esp_native_first_frame.c` implementation and are still consumed by
`EspNativeGameplaySession`.

The wrapper is retained only by `esp32-cyd-bringup` for historical fidelity
inspection.

CI #1202 succeeds with:

```text
                         merged V14 main   V15           delta
static RAM               45128 B           45120 B        -8 B
linked Flash            773089 B          772161 B      -928 B
firmware.bin            773456 B          772528 B      -928 B
active __wrap_*              52                51           -1
```

Final-ELF inspection confirms:

```text
EspNativeFirstFrame_route                 PRESENT
EspNativeFirstFrame_view                  PRESENT
EspNativeFirstFrame_renderGameplayViewport PRESENT
__wrap_EspNativeFirstFrame_route          ABSENT
Esp32FirstFrameDiagnostic_reset           ABSENT
Esp32FirstFrameDiagnostic_exportBmp       ABSENT
pendingDumpRender                         ABSENT
```

The real classic CYD proves that the production first-frame contract is
unchanged. Entrance still publishes the exact known first frame:

```text
[NATIVEFRAME] WALL requests=8 draws=8 spans=160 pixels=4430 ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

No `[JUNCTIONFRAME] COLORSTATS` line appears.

The 8-byte static-RAM reduction is visible exactly in hardware versus V14:

```text
MENU_MAIN heap8     35656 -> 35664
intro heap8         43200 -> 43208
resident ALIVE      27464 -> 27472
```

The gameplay session then reaches:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
```

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md](MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md)


### Compile-time ESP32 logging policy V16 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `5a320d50a5f88646382db1211114a31b671b34b2`.

V16 adds the allocation-free `doomrpg_log.h` compile-time policy:
ERROR / INFO / DEBUG / TRACE. Normal `esp32-cyd` defaults to INFO and
`esp32-cyd-bringup` explicitly selects TRACE. Disabled DEBUG/TRACE calls and
their format strings can disappear from the final image.

The bounded migration classifies per-present VIDEO timing, PAKIO SAMPLE,
PLANEPROFILE, successful periodic NATIVEPLANE summaries, per-frame
SPRITEPROFILE and expected RNG WORD-OOB-AVOIDED as TRACE. Detailed crate and
interaction inventories are DEBUG. Failures/OOM/FATAL remain ERROR, while
operational transitions and ALIVE remain visible at INFO.

No functional seam is removed. Final-ELF `nm` confirms the exact same 51
`__wrap_*` symbols as merged V15 main, including PlatformVideo, Render
plane/cull and all four EspAssetPack wrappers.

CI #1209:

```text
                         V15/main        V16            delta
static RAM               45120 B         45080 B         -40 B
linked Flash            772161 B        767581 B       -4580 B
firmware.bin            772528 B        767952 B       -4576 B
active __wrap_*              51             51              0
```

The 40-byte RAM reduction is exactly the normal-build removal of PAKIO profiling
state. The INFO ELF contains none of the targeted VIDEO/PAKIO/PLANEPROFILE,
NATIVEPLANE-success, SPRITEPROFILE, RNG WORD-OOB, CRATESTATE WITNESS or
INTERACTMAP OPCODE strings, while NATIVEPLANE failures, RNG FATAL,
CRATESTATE OOM/READY, MAPFLASH error/ARM, ALIVE and ENGINESESSION READY remain.

The real classic CYD validates cold boot, native MENU_MAIN, START, full intro
and disposal, Entrance bootstrap, exact first frame `71ca7465`,
`ENGINESESSION READY ... shapeData=0x0 mediaTexels=0x0`, committed movement
and rotation, crate transform/pickup, dialog resume and door open/close
animation. Multiple ALIVE witnesses remain healthy and none of the targeted
TRACE spam appears.

Remaining verbose families such as NATIVEFRAME, INTERACTCORPUS and semantic
gameplay traces were intentionally left outside this bounded milestone.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md](MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md)

### Interaction / CHANGEMAP recovery diagnostics scoped to bringup V17 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `53b548b5adf0d09c2d1e1ed4b673a3ae8054cac2`.

V17 removes the temporary production linker interception around
`EspNativeGameplayInteractionInventory_log()` and prevents the normal INFO
session chain from executing the one-shot interaction inventory at all.
`esp_native_changemap_probe.c` is compiled only at DEBUG/TRACE level, while
`esp32-cyd-bringup` retains the historical
`--wrap=EspNativeGameplayInteractionInventory_log` witness.

This is diagnostic retirement only. The permanent production transition path
remains unchanged:

```text
EspNativeGameplaySession
 -> EspNativeGameplayPlayerResources_sessionService
 -> EspNativeResidentGameplay
 -> EspNativeGameplayTransition
 -> EspNativeGameplayTransitionHandoff
```

CI #1214 succeeds with:

```text
                         merged V16 main   V17           delta
static RAM               45080 B           45072 B         -8 B
linked Flash            767581 B          764757 B      -2824 B
firmware.bin            767952 B          765120 B      -2832 B
active __wrap_*              51                50           -1
```

Artifact `11152203158` has digest
`sha256:3cf7e77078a257d81394e954f54ba3a12ad8ac3a505046b7f72ad88e03448adf`.
The CI firmware SHA-256 is
`06885150bee7ae651340a3b0cfb5557eae161bfc99a394bc9dae8355d2bf6425`;
the ELF SHA-256 is
`5b9bceb0f42f5391f522ac7a61bee9757310c534305b998c6195a72cafa23b37`.

Final-ELF inspection proves exactly 50 active `__wrap_*` symbols.
`__wrap_EspNativeGameplayInteractionInventory_log` and
`EspNativeGameplayInteractionInventory_log` are absent from the linked INFO
image; the real `EspNativeGameplayTransitionHandoff_service` remains present.
No `[CHANGEMAPPROBE]` or `[INTERACTMAP]` format string remains in the INFO
ELF. `[INTERACTCORPUS]` and the older `[JUNCTIONEXITCENSUS]` family remain
intentionally outside this bounded milestone.

The real classic CYD validates cold boot, native MENU_MAIN, START, full intro
and disposal, Entrance bootstrap, exact first frame `71ca7465`, and:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The same run commits movement and rotation, transforms a crate, resumes a real
dialog chain through opcode 19, picks up two Armor Shards, and keeps ALIVE
healthy. No `CHANGEMAPPROBE` or `INTERACTMAP` line appears.

Observed normal-build memory witnesses include:

```text
MENU_MAIN heap8=35712 largest8=23540
intro     heap8=43256 largest8=12276
gameplay  heap8=27520 largest8=18420   (before later dialog/resource allocations)
```

The local PlatformIO build reports the same 45072 B static RAM and a harmless
16-byte size difference from CI: 764773 B linked Flash / 765136 B firmware.bin.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md)

### Junction exit census scoped to DEBUG/TRACE V18 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `6733395845c289fa9f69cc6c61001c3a68f2d72d`.

V18 removes the historical `EspNativeGameplayTransition_probeJunctionExitCensus()`
from the normal INFO image. The session call, private
`junctionExitCensusDone` byte and the full census function are compiled only
for DEBUG/TRACE. The normal product build therefore neither calls nor links
this historical Junction SAVEGAME/CHANGEMAP corpus scanner.

The production transition owner is unchanged. In particular
`EspNativeGameplayTransition_trySelect()` and
`EspNativeGameplayTransitionHandoff_service()` remain linked and own the
live transition path.

CI #1216:

```text
                         V17              V18           delta
static RAM               45072 B          45072 B          0 B
linked Flash            764757 B         762617 B      -2140 B
firmware.bin            765120 B         762976 B      -2144 B
active __wrap_*              50               50            0
```

Artifact `11154118496`:
`sha256:fa4e82a7305596cfa36ca65897e3802573309db723d15e22e7993636d51d0ec6`.
Firmware SHA-256:
`fccb055e5d3019612650f8740453480a8f0ad9e418ee5e709031d2b1024d6db0`.
ELF SHA-256:
`a8197c29053631f9dcd22968c331eb868f8c3606efca33ec598ac610ea24b8ec`.

Final-ELF inspection proves `EspNativeGameplayTransition_probeJunctionExitCensus`,
`junctionExitCensusDone` and every `[JUNCTIONEXITCENSUS]` format string are
absent from normal INFO firmware. The 50-wrapper set is unchanged from V17.

The real classic CYD validates cold boot, START, full intro/disposal, Entrance
bootstrap and the exact first frame:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

The same run reaches `ENGINESESSION READY` with
`shapeData=0x0 mediaTexels=0x0`, commits multiple moves and rotations,
transforms a crate into an Armor Shard, picks up two shards, opens a regular
door with the four-frame native animator, crosses it, then observes its
four-frame auto-close. ALIVE remains stable at:

```text
heap=93444 heap8=27520 largest8=18420
```

No `[JUNCTIONEXITCENSUS]` line appears in the runtime transcript.

The local PlatformIO build reports the same 45072 B static RAM and the known
16-byte environment difference: 762633 B linked Flash / 762992 B firmware.bin.

The perceived gameplay smoothness is consistent with the cumulative
compile-time logging/profiling retirement since V16; V18 itself removes a
one-shot/session-readiness diagnostic rather than a per-frame renderer path.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md](MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md)

### Interaction-chain corpus wrapper scoped to bringup V19 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `a2dffc4243701a5f78fa02abf39a81da68b5c128`.

V19 removes the production linker interception of
`EspNativeResidentGameplay_service()` that existed only to emit the one-shot
`[INTERACTCORPUS]` opcode census. The census function, its private
`corpusLogged` byte and the wrapper are DEBUG/TRACE-only; bringup retains the
historical interposition.

The real event-chain implementation is unchanged. Dialog preflight, resume,
rollback and synchronous bounded event-chain execution remain production-owned.

CI #1218:

```text
                         V18              V19           delta
static RAM               45072 B          45064 B         -8 B
linked Flash            762617 B         762021 B       -596 B
firmware.bin            762976 B         762384 B       -592 B
active __wrap_*              50               49           -1
```

Artifact `11155223388`:
`sha256:647af5bbbc5bda0f091b4a5de83ba05d961cd0774f5ed50a62cfda17d2df6e55`.
Firmware SHA-256:
`b394438989fcfcfa4c1d63e5142fc7e56c8c017aa7134af65344d7faf336fffb`.
ELF SHA-256:
`540660ede5a2d4e5621e60c2d03c473f7c660ac09041e919e5c548187e7a96ba`.

Final-ELF inspection proves exactly 49 active `__wrap_*` symbols.
`EspNativeGameplayEventChain_logCorpus`,
`__wrap_EspNativeResidentGameplay_service`, `corpusLogged` and all
`[INTERACTCORPUS]` strings are absent. The direct
`EspNativeResidentGameplay_service`, transition selection and transition
handoff symbols remain linked.

The real classic CYD validates cold boot, START, full intro/disposal, exact
Entrance first frame `71ca7465`, and:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The same run commits MOVE and TURN, transforms the first crate, then opens and
fully resumes scientist dialog event 88 through opcode 19. ALIVE remains
stable before dialog at:

```text
heap=93448 heap8=27524 largest8=18420
```

and after the lazy dialog-chain owner allocation at:

```text
heap=92412 heap8=26488 largest8=18420
```

No `[INTERACTCORPUS]` line appears anywhere in the runtime transcript.

The hardware free-heap witnesses are all +8 B versus V18 before later lazy
allocations (for example MENU_MAIN `heap8=35720` versus 35712), matching the
8 B static-RAM reduction reported by CI.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md)

### Hot redraw success telemetry moved to TRACE V20 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `552ec9c529d1bd31a656c9847b0b267bacd713fa`.

V20 moves repetitive success-only presentation telemetry out of the normal
INFO firmware without changing rendering, gameplay, FNV guards, rollback or
failure/recovery behavior.

The following success families are TRACE-only in normal production:

```text
[NATIVEFRAME] BSP / WALL
[WEAPON] DRAW
[FACINGLABEL] REFRESH / PAINT / CLEAR
[RESIDENTGAMEPLAY] FRAME
[ACTIONENGINE] FRAME
[ACTIONFEEDBACK] PAINT / CLEAR / REFRESH
[DOORANIM] FRAME
[DYNAMICLINES] FRAME
```

Important recovery/error witnesses remain visible, including
`NATIVEFRAME FAILED`, cache/read failures, `LEGACY_GUARD`, `RETRY`,
`RECOVERED`, `RESIDENTGAMEPLAY RENDER-FAILED`,
`ACTIONFEEDBACK FAILED`, `VIEWFLASH FAILED`, plus functional
`DOORANIM ARM/COMPLETE` boundaries.

CI #1220:

```text
                         V19              V20           delta
static RAM               45064 B          45064 B          0 B
linked Flash            762021 B         759197 B      -2824 B
firmware.bin            762384 B         759568 B      -2816 B
active __wrap_*              49               49            0
```

Artifact `11157510597`:
`sha256:f891d1f121611cade66ed43eb9064919fa2da5336d39ac27d8a97f924802aded`.
Firmware SHA-256:
`956d0593fa16366448736ee602eba321f217a94b106c4dbcf7286503afe976e8`.
ELF SHA-256:
`559f03b35ab5950ce4e9ee51c4fe4c128e81975c708001b6c7aa52952a3b4c10`.

The local hardware workstation reports the same 45064 B static RAM and the
known +16 B environment delta: 759213 B linked Flash / 759584 B firmware.bin.

The real classic CYD validates cold boot, full intro/disposal, exact Entrance
first frame `71ca7465`, `shapeData=0x0 mediaTexels=0x0`, repeated MOVE/TURN,
resource pickups and regular-door animation. The hot success lines above are
absent from the runtime transcript, while a real renderer recovery remains
visible and succeeds:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

Gameplay ALIVE remains stable at:

```text
heap=93448 heap8=27524 largest8=18420
```

The hardware interaction is subjectively much more responsive after the
cumulative logging consolidation. V20 specifically removes repeated serial
traffic from each world redraw; no renderer algorithm or presentation path was
otherwise optimized.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md](MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md)

