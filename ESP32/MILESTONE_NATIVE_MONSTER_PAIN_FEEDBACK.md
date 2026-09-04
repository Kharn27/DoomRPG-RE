# Milestone — Native monster retaliation player-pain feedback

## Git / hardware boundary

```text
base main = 4b95d382ab9b120dcd7e020d4614a48d01001d1c
branch = agent/esp32-native-monster-pain-feedback
hardware-tested code boundary = b7bf6bb692f5987f9307a7c02a42601fcf3232e1
status = REAL-CYD PASS
branch policy = LOCKED; docs-only tail only
```

`b7bf6bb6...` is the exact code boundary exercised on the real classic CYD.
Commits after this SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33854099003` completed successfully for
this exact SHA and uploaded artifact
`doom-rpg-esp32-cyd-b7bf6bb692f5987f9307a7c02a42601fcf3232e1`.
CI is only the compile/link gate; the real-CYD serial witness below is the
runtime authority.

## Goal

Recover the bounded presentation half of legacy monster-to-player pain without
broadening into monster attack animation, sound, shake, pain face or player
death.

Permanent path:

```text
MonsterTurn exact attack probe
 -> MonsterRetaliation exact RNG replay
 -> shared 52 B PlayerState pain mutation
 -> existing bounded ActionEngine DAMAGE feedback
 -> top-bar raw damage text
 -> red viewport-border flash
 -> native frame commit
```

No new persistent gameplay owner or heap allocation was introduced.

## Legacy behavior recovered

Legacy monster combat eventually calls:

```text
Player_pain(player, combat->totalDamage, combat->totalArmorDamage)
```

The visible damage amount is the raw pair sum, not merely the resulting HP loss:

```text
visible damage = totalDamage + totalArmorDamage
```

Armor then absorbs the armor component when available and spills any shortfall
into HP according to the existing shared PlayerState pain rule.

Legacy combat also clears the prior HUD message when combat begins. The native
retaliation route therefore supersedes the already-painted `"Turn passed."`
message on a hit instead of introducing a message queue solely to preserve it.

## Real-CYD retaliation witness — PASS

Final tested firmware produced a nonlethal Hellhound hit:

```text
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN
  sprite=179 subtype=1 weapon=13
  firstRandHit=13 firstCalcHit=193
  firstRandDamage=243
  totalDamage=3 armorDamage=3 crit=0
  playerHP=30->27 armor=8->5
  rngRollback=yes playerExact=yes

[ACTIONFEEDBACK] PAINT kind=6 text="6 damage!" ... durationMs=1200
[VIEWFLASH] PAINT color565=b800 viewport=0,20,160,80
  thickness=2 pixels=944 durationMs=500

[MONSTERRETAL] COMMIT probe=2 reason=PASS_TURN
  sprite=179
  totalDamage=3 armorDamage=3
  playerHP=30->27 armor=8->5
  message="6 damage!" damageTotal=6
  redFlash=b800/500ms
  passMessage=legacy-superseded
  rollback=closed
  attackVisual=deferred
  painFace=deferred
  shake=deferred
  sound=deferred
  playerDeath=fail-closed
```

The user physically observed the red damage border. The flash expired cleanly:

```text
[VIEWFLASH] EXPIRE elapsedMs=514 targetMs=500 color565=b800
  restored=viewport-border-only
```

A prior test of the first code commit on this branch independently produced
`"6 damage!"` and `"4 damage!"` witnesses, but the locked runtime authority is
the final `b7bf6bb6...` boundary above.

## Feedback-owner interaction bug found by hardware

The first implementation commit was:

```text
18a76296d9739489cf7806e7dc7beb6a8bd09d1d
```

Its retaliation presentation passed on hardware, but the same real-CYD session
exposed an existing ActionEngine/Pak ownership failure when a pickup message
expired while a native dialog owned the PAK for typewriter rendering.

Failure signature:

```text
pickup feedback visible
 -> DIALOG OPEN ... pack=open
 -> feedback reaches 1200 ms
 -> ActionEngine tries to repaint top bar
 -> paintFeedback() sees PAK already open
 -> ACTIONFEEDBACK FAILED kind=0
 -> ACTIONENGINE FAILED reason=service fatal=1
```

The crash was not a dialog-script failure and not a low-memory allocation
failure. It was a temporary storage-owner conflict incorrectly escalated to a
fatal gameplay error.

## Bounded PAK-lease fix — hardware PASS

Commit `b7bf6bb692f5987f9307a7c02a42601fcf3232e1` changed only
`esp_native_gameplay_action_engine.c` and adds no allocation. Feedback repaint
now waits while another bounded owner has the PAK open.

The viewport flash expiry remains independent, so a pickup/damage border still
restores on schedule even if a dialog owns the PAK.

Final reproduction sequence on the real CYD:

```text
[PLAYERRES] FEEDBACK tile=782 message="Got Halon Can"
  flash=white-border/500ms
[VIEWFLASH] EXPIRE elapsedMs=501 targetMs=500 color565=ffff
  restored=viewport-border-only

[ACTION] SELECT seq=92 status=DIALOG_READY tile=814 event=83
[DIALOG] OPEN event=83 cmd=1 resume=2 opcode=26
  string=61 bytes=51 lines=3 pack=open

# dialog remains active past the 1200 ms pickup-message lease
# no ACTIONFEEDBACK FAILED
# no ACTIONENGINE FAILED

[DIALOG] FASTFORWARD pageStart=0 lines=3
[DIALOG] CLOSE event=83 resume=2 mode=resume ... packClosed=yes
[DIALOGCHAIN] RESUME event=83 start=2 handled=1 state=1 mutation=1
[RESIDENTGAMEPLAY] DIALOG-RESUME ... redraw=yes dialog=closed
[ACTIONFEEDBACK] CLEAR mode=topbar-only
[ACTIONFEEDBACK] EXPIRE kind=5 elapsedMs=2911 targetMs=1200
  restored=topbar-only

# session remains live and opens another dialog
[DIALOG] OPEN event=83 cmd=3 resume=4 opcode=8 ... pack=open
```

This proves the expired feedback lease is deferred rather than lost or promoted
to fatal, then is cleaned immediately after the dialog releases the PAK.

## RAM witness

Before the lazy NOTE/dialog owner is allocated, repeated real-CYD `ALIVE` lines
retain the established normal-gameplay baseline:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
```

After the computer interaction logs:

```text
[NOTE] OWNER bytes=1416 allocation=lazy-gameplay
...
heap = 81084 B
heap8 = 15352 B
largest8 = 13300 B
```

The lower post-dialog values match the explicit lazy NOTE owner allocation; this
run does not establish a leak. Permanent project invariants remain
`shapeData == NULL`, `mediaTexels == NULL`, no PSRAM and PAK-backed native data.

## Performance observation — not a correctness blocker

The user reported a general impression that this firmware felt slower. The serial
sample contains a real signal worth investigating separately:

```text
previous comparable SPRITEPROFILE samples: about 22-25 ms
final tested run comparable samples:       about 46-49 ms
VIDEO present in both runs:                 about 34.4 ms
normal NATIVEPLANE samples:                 broadly about 79-109 ms
```

A dialog-resume frame also temporarily fell to a 4/6 plane-cache lease and read
55296 B, producing:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=4/6 leaseBytes=2048 totalLeaseBytes=8192
[PLANEPROFILE] us=230457 ok=1
```

The PAK-lease hotfix itself only adds two `EspAssetPack_isOpen()` guards and is
not a plausible source of a broad rendering slowdown. Do not claim a regression
cause from this single run; preserve it as a performance investigation lead,
separate from this correctness milestone.

## Explicit deferred boundaries

This milestone intentionally leaves these families untouched:

```text
monster attack visual / sprite attack animation
monster attack message beyond recovered damage text
pain face
screen shake
sound playback
status warnings
player lethal/death transition
PASS_TURN current-tile type10/type11 hazard touch
secondary burn text
multiple-monster activation ordering
monster movement interpolation/animation
materialized drops / corpse trimming
```

Monster attack animation remains explicitly `attackVisual=deferred` in the final
hardware log. Its absence is expected at this boundary and should be recovered as
a separate bounded presentation family.

## Merge rule

The code boundary is locked at
`b7bf6bb692f5987f9307a7c02a42601fcf3232e1`. Only documentation may follow on
this branch. After merge, recover the exact new GitHub `main` SHA before creating
the next `agent/*` branch.