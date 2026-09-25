# Native Entrance -> Junction CHANGEMAP — REAL-CYD PASS

## Hardware-tested boundary

The final real-CYD validation boundary for this branch is:

```text
hardware-tested code SHA = e4acb92403dd48810f3c4e989d4dee16605125e3
tested base main = 6cd8b6804cbec75538becab0d6cbe66e3c79d238
CI #829 = SUCCESS
RAM static = 45200 B / 327680 B = 13.8%
Flash = 789345 B / 1310720 B = 60.2%
artifact id = 10873930025
```

This final hardware run reconfirmed the already-proven Entrance -> Junction
happy path on the rebased branch, including the first committed Junction
movement and post-render `EV_MESSAGE "Junction"`. It also exercised the final
native Junction dialog pipeline on a real NPC through OPEN, pagination,
FASTFORWARD, CLOSE and script RESUME.

The same branch still contains the post-PASS code-review closure on the
SELECT-door render-failure rollback path. That failure-only cleanup path itself
was not hardware-triggered.

## Recovered Entrance event

Entrance event 1 on tile 69 executes the bounded sequence:

```text
SAVEGAME  -> /junction.bsp, targetMapId 9, savePos 992,1888, angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, gameplayLoadMapId 2,
             showStats 1, spawnParam 0
OPENLINE  -> line 459, regular four-frame door
```

The SAVEGAME leg is consumed here as transition/save-position semantics. This
milestone does not claim new checkpoint-format coverage beyond the separately
validated checkpoint milestones.

## Source preflight without gameplay fallback

The active Entrance raw-flash slot intentionally excludes Junction BSP payload.
The final transition uses one explicit read-only source-PAK probe lease for exact
target-BSP inventory/CRC validation:

```text
[PAKSOURCE] PROBE-OPEN ... /junction.bsp ... scope=exact-entry
[PAKSOURCE] PROBE-READ ... backing=sd-authoritative
                         activeMapFlashUntouched=yes
                         cachePollution=no
```

This is not a general gameplay fallback. Normal renderer/gameplay reads remain:

```text
requested-map raw internal-flash slot
 -> 19 KiB resident L1
 -> native renderer/gameplay
```

Silent SD fallback during active gameplay remains forbidden.

## Door and WAIT_STATS boundary

The exit door uses the shared generic regular-door transaction:

```text
[NATIVECHANGEMAP] WAIT_DOOR ...
[DOORANIM] FRAME 1/4 ...
[DOORANIM] FRAME 2/4 ...
[DOORANIM] FRAME 3/4 ...
[DOORANIM] FRAME 4/4 ...
[DOORANIM] COMPLETE ... transaction=committed
[NATIVECHANGEMAP] DOOR-COMPLETE ... phase=WAIT_STATS
[NATIVECHANGEMAP] STATS-BRIDGE ... callback=one-tap
```

The original statistics presentation is still deferred. The current bounded
bridge waits for one explicit tap before continuing the already-preflighted
transition. This milestone validates transition ownership, not the final stats
screen UI.

## Target backing switch and destructive commit

After acknowledgement, source and target inventories are revalidated through
the exact source-probe lease. Only then is resident L1 released and the requested
target raw-flash working set prepared.

The hardware run proved the required world-identity MISS/rebuild:

```text
[MAPFLASH] REUSE MISS requestedMap=9 ... cachedMap=1 ... rebuild=yes
[MAPFLASH] PLAN map=9 ...
[MAPFLASH] ERASE bytes=2752512 ...
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=4196bd53 verified=yes
[MAPFLASH] READY map=9 staged=2247971 ... buildUs=14101292
[MAPTRANSITION] BACKING sourceMap=1 targetMap=9
                targetFlash=ready resident=0
                sourceRuntime=still-resident switch-before-reset=yes
```

Only after target backing is ready is the source compact runtime destroyed:

```text
[RESIDENTRESET] ... empty=1
```

Junction then rebuilds from the target raw slot:

```text
[MAPRT] READY arenaBytes=8867 ... arenaFNV=bc432a0f
[MAPSTATE] READY bytes=1024 ...
[MAPLINESTATE] READY lines=207 ...
[NATIVECHANGEMAP] COMMIT sourceMap=1 targetMap=9 ... rollback=no
[NATIVECHANGEMAP] SPAWN targetMap=9 ... tile=943 pos=992,1888 angle=64
[NATIVECHANGEMAP] SESSION targetMap=9 configured=yes ...
```

The first different-map switch therefore costs about 14.1 s in this hardware
trace because the single raw-flash slot is rebuilt synchronously. That is a
presentation/performance debt, not a correctness failure.

## Junction session re-arm

The generic native session starts on map 9 and reuses the newly committed slot:

```text
[ENGINESESSION] FIRST_FRAME map=9 angle=64 ...
[MAPFLASH] REUSE HIT requestedMap=9 current=/junction.bsp cachedMap=9 ...
[MAPFLASH] ARM map=9 ... resident=1
[ENGINECACHE] PRIMED map=9 ...
[RESIDENTGAMEPLAY] READY ... moveEvents=door15/16+message4+force24+...
[ENGINESESSION] READY map=9 ... shapeData=0x0 mediaTexels=0x0
```

Permanent memory/storage invariants remain intact:

```text
shapeData == NULL
mediaTexels == NULL
no PSRAM
no map-wide legacy texel ownership
no runtime ZIP dependency for this transition
no silent SD gameplay fallback
```

## First Junction movement parity

The first step from spawn exposed one legacy ordering rule:

```text
source tile 943 EXIT: CLOSELINE line 35, locked
destination tile 911 ENTER: MESSAGE "Junction"
```

Legacy movement ignores the failed/locked CLOSELINE result on EXIT; it does not
abort the step. The destination MESSAGE is executed only after movement has
committed and the destination frame has rendered.

The final native transaction reproduced that order:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... status=DOOR_LOCKED ... opcode=16
[MOVEEVENT] ENTER-PREFLIGHT ... status=MESSAGE_READY ... opcode=4
[MOVEEVENT] EXIT ... status=DOOR_LOCKED ... mutation=no
[MOVEEVENT] ENTER ... status=MESSAGE_READY ... mutation=no
[MOVEEVENT] WORLD-READY ... enterMessage=opcode4/event59/cmd0
[ACTIONFEEDBACK] PAINT ... text="Junction"
[MOVEEVENT] MESSAGE ... text="Junction" present=yes rollbackLease=closed
[RESIDENTGAMEPLAY] MOVE ... tile=943->911 ... pos=992,1824 ... committed=yes
```

The MESSAGE owner is deliberately bounded and ENTER-only in this milestone.
Unsupported EXIT MESSAGE presentation remains fail-closed.

## Junction dialog + continuation on final code head

The final real-CYD run also proved that gameplay remains usable beyond the first
step and that the native dialog owner/continuation machinery survives the map
transition. The player moved deeper into Junction, faced the Scientist on tile
878 / event 56, and SELECT opened a six-line opcode-8 dialog:

```text
[ACTION] SELECT ... status=DIALOG_READY tile=878 event=56 ...
[DIALOGCHAIN] OWNER bytes=1020 allocation=lazy-gameplay
[DIALOG] OPEN event=56 cmd=0 resume=1 opcode=8 string=21 bytes=97 lines=6 ...
[RESIDENTGAMEPLAY] SELECT-DIALOG ... active=yes pauseScript=yes skipTurn=yes
```

Touch input then exercised both page fast-forward and page advance before close:

```text
[DIALOG] FASTFORWARD pageStart=0 lines=4 ...
[DIALOG] PAGE start=4/6 ...
[DIALOG] FASTFORWARD pageStart=4 lines=2 ...
[DIALOG] CLOSE event=56 resume=1 mode=resume ... packClosed=yes
```

The saved continuation resumed natively and committed the following state opcode:

```text
[DIALOGCHAIN] RESUME event=56 start=1 handled=1
              show=0 hide=0 unlock=0 givemap=0 state=1 removed=0 mutation=1
[RESIDENTGAMEPLAY] DIALOG-RESUME ... event=56 offset=1 opcode=11
                   stateMutation=1 redraw=yes ... dialog=closed
```

The final fix on this code head makes dialog/NOTE/continuation revalidation use
the same live `EspNativeGameplayPlayerState.keys` context as the original
SELECT filter instead of silently rebuilding the filter with `keys=0`. The
final supplied log hardware-exercised the corrected generic dialog chain on
event 56. The earlier Marine event 45 regression that originally exposed the
mismatch was not replayed in the final supplied trace, so this document does not
claim an exact event-45 retest.

## Post-PASS code-review rollback closure

Code review identified a failure-only owner leak: if the shared
`renderActionCurrent(..., "SELECT-DOOR")` failed for the staged transition
door, the generic line/script rollback could succeed while
`EspNativeGameplayTransition` remained in `waitingDoor`. A retry would then
see a transition owner whose recorded open/removed state no longer matched the
restored world.

The rebased code head closes the same transaction in this order:

```text
SELECT-DOOR render fails
 -> restore any secret feedback/player state
 -> EspNativeGameplayAction_rollbackSelect()
 -> if transition door: EspNativeGameplayTransition_abortDoor()
 -> render SELECT-DOOR-ROLLBACK
 -> only then return to live gameplay
```

`abortDoor()` is attempted only after the world/script rollback succeeds. On
any cleanup failure the service still fails closed. This change affects only the
render-failure rollback path; the hardware-proven successful transition path is
unchanged.

## Scope proven / not proven

Proven on the real CYD:

```text
Entrance event 1 exact SAVEGAME -> CHANGEMAP -> OPENLINE sequence
target BSP source preflight while Entrance gameplay backing stays untouched
regular four-frame exit door
WAIT_STATS + explicit one-tap bridge
map 1 -> map 9 raw-flash world-identity switch
source teardown only after target backing is ready
Junction compact runtime/state rebuild
target spawn at tile 943 / 992,1888 / angle 64
generic Junction renderer/HUD/cache/resident gameplay re-arm
first Junction movement 943 -> 911
locked EXIT CLOSELINE treated as non-blocking legacy no-op
ENTER EV_MESSAGE "Junction" published after committed destination render
Junction Scientist event 56 DIALOG open / page / close / resume
post-dialog opcode 11 CHANGESTATE mutation + world redraw
```

Still deliberately separate:

```text
real statistics screen presentation for showStats transitions
generic proof of every Junction -> Level01..Level07 CHANGEMAP exit
other SAVEGAME / CHANGEMAP script shapes
long/general EV_MESSAGE ownership outside the bounded MOVE ENTER case
other deferred Junction opcodes such as OPENSTORE, INCSTAT, PLAYSOUND,
CHECK_COMPLETED_LEVEL and general MESSAGE events
```

This is the first hardware-validated native world-to-world transition in the
ESP32 port.
