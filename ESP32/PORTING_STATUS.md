# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Current Git boundary

```text
current main = 8dd660ce1017540c364591cad54514c9c788acf5
branch = agent/esp32-native-level-stats-loading
hardware-tested code boundary = a81dd38a6875154b37b9006a145eef5d73e85a66
hardware = native level stats + reusable fixed-background loading + checkpoint LOAD presentation REAL-CYD PASS
CI = esp32-cyd #867 SUCCESS
static RAM = 45224 B
flash = 795461 B
status = HARDWARE PASS; POST-TEST TAIL DOCS-ONLY
```

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

### Options dashboard reuse — BUILD PASS / REAL-CYD PENDING

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
touch behavior still require confirmation on the real CYD.

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

`INV` uses a centered three-card list. `WPN` is a complete 3x3 grid for normal
weapon IDs 0..8; familiar IDs 9..11 remain excluded. The weapon icon loader now
converts source BGR565 palettes to framebuffer RGB565, fixing the red Fire
Extinguisher and yellow-handled Axe presentation. `STAT` is read-only and no
longer covered by checkpoint controls. `SYS` owns full-width SAVE/LOAD cards,
two-step `SAVE?` / `LOAD?` confirmation and `NO SAVE` handling.

All four tabs are directly touch-addressable. The HUB temporarily owns a full
industrial top title bar and reconstructs the permanent gameplay HUD on close.
The 28-byte HUB owner and world/turn gating remain unchanged.

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
during reverse restore. The largest measured SYS card needs 597 edits, so the
large touch target remains covered while the owner saves 2048 B versus the
768x6 form. Overlay creation remains nonfatal and restoration remains
pixel-owned: newer overlays win.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md)

#### Compact STAT information layout — BUILD PASS / REAL-CYD PENDING

The read-only `STAT` content no longer spends the viewport on five legacy 9x12
text rows. It now uses compact 3x5 labels and intermediate 5x7 values in two
slightly reduced HP/Armor cards, level/XP plus a bounded progress bar, and a consistently aligned
2x2 DEF/STR/AGI/ACC grid. The slim card rails are real proportional gauges:
health is green (red at critical level) and armor is light blue. The footer no
longer leaks the internal hexadecimal key mask; owned bits 0..3 render as compact
green/true-yellow/blue/red key-card icons. Values retain a larger visual weight than
their labels. No content hitbox was added: the existing `STAT` tab is still the
page's only touch target. PlatformIO compilation succeeds with static RAM
unchanged at 45128 B; real-CYD legibility and color balance remain to be
confirmed.

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

This branch is merge-ready. After merge, re-read the exact new `main` SHA and
branch from it.

The next major gameplay frontier remains the native **CHANGEMAP / Entrance
level-exit transition** already identified below. Recover the smallest complete
real-CYD route and keep unrelated opcode families fail-closed.

Two qualifications remain intentionally explicit:

```text
event43 SHOW x4 PASS is anchored to pre-rebase code head 48accf9
SHOW-exit + ENTER-dialog Codex lease fix is CI-valid but not directly hardware-reached
```

Neither qualification blocks this merge because the rebased boot, HUB LOAD
session replacement, main-menu LOAD, and RNG initialization regressions have
now been exercised successfully on the real CYD.
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

After this branch merges, re-read the exact resulting `main` SHA before opening
the next `agent/*` branch.

The first world-to-world transition is hardware-proven only for the bounded
Entrance event 1 route. Keep the next transition work equally narrow. Strong
candidates are:

```text
- restore the real statistics presentation for showStats=1 before removing the
  temporary one-tap WAIT_STATS bridge; or
- generalize and hardware-prove one exact Junction -> LevelXX exit while keeping
  the other CHANGEMAP script shapes fail-closed.
```

Do not infer generic CHANGEMAP coverage from the Entrance -> Junction PASS.
Unrelated deferred Junction families such as OPENSTORE, INCSTAT, PLAYSOUND,
CHECK_COMPLETED_LEVEL and general MESSAGE handling remain separate milestones.
The final log additionally proves one normal Junction DIALOG + CHANGESTATE
continuation, not arbitrary Junction dialog/script coverage.

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
generic CHANGEMAP routes beyond hardware-proven Entrance -> Junction
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
