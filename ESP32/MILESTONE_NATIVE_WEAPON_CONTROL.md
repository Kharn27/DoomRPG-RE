# Native generic weapon-control milestone

Status: **REAL-CYD HARDWARE PASS** for generic native NEXT-weapon selection,
weapon/HUD redraw, and live Pistol firing through the existing generic player
combat backend.

Hardware-tested code boundary:

```text
branch = agent/esp32-native-weapon-control
base main = 8b9f23324ff314bf207cc3ffab01f11f76438515
code boundary = 17c30561bd7c080cdd55436caa6d67cae7250970
CI = PlatformIO esp32-cyd SUCCESS
```

Commits after that boundary are documentation-only.

## Goal

Replace the previous effectively-fixed selected-weapon behavior with a permanent,
data-driven native weapon-control path. Weapon ownership, selected slot and ammo
remain in the existing 52 B `EspNativeGameplayPlayerState`; no new mutable weapon
owner is introduced.

Permanent composition:

```text
PREV_WEAPON / NEXT_WEAPON input
 -> allocation-free legacy-compatible selector
 -> shared PlayerState snapshot
 -> selected weapon commit
 -> complete native weapon + HUD redraw
 -> exact PlayerState rollback if redraw fails
```

Weapon cycling itself does **not** schedule a monster turn.

## Recovered legacy selector contract

The selector follows `Player_selectNextWeapon()` / `Player_selectPrevWeapon()`:

```text
slots = 0..11 circular
candidate must be owned
candidate is selectable when ammoUsage == 0 OR ammo[ammoType] > 0
selection does not require ammo >= ammoUsage
```

That last rule is intentional legacy behavior: a high-consumption weapon may be
selectable with insufficient ammo and later reject the actual shot.

Slots 0..8 use the standard player-weapon metadata already owned by native
CombatMath. Slots 9..11 remain the historical familiar weapon slots and consume
no ammo for selection purposes. Unsupported firing mechanics remain fail-closed
at their mechanical family boundaries.

## Real-CYD weapon-cycle witness

The tested player had `weapons=0007`, therefore Axe, Extinguisher and Pistol were
owned. Two consecutive NEXT_WEAPON actions exercised the same generic selector:

```text
[WEAPONCONTROL] COMMIT seq=107 action=NEXT_WEAPON
  weapon=0->1 weapons=0007 ammoType=0 ammo=10 inspected=1
  playerFNV=fa441c23->1e469366 redraw=yes turn=no rollback=closed

[WEAPON] DRAW weapon=1 logical=241 actual=606 frame=0 pose=idle

[WEAPONCONTROL] COMMIT seq=108 action=NEXT_WEAPON
  weapon=1->2 weapons=0007 ammoType=1 ammo=12 inspected=1
  playerFNV=1e469366->464910f5 redraw=yes turn=no rollback=closed

[WEAPON] DRAW weapon=2 logical=242 actual=610 frame=0 pose=idle
```

This proves live selected-weapon publication into the shared PlayerState and
renderer/HUD projection without turn advancement.

The submitted serial witness does not contain a PREV_WEAPON action, so this
milestone does **not** claim an independent real-CYD PREV witness. PREV uses the
same circular selector in reverse and remains part of the implemented contract.

## Real-CYD Pistol firing witness

After selecting weapon 2, SELECT traced the already-moved Hellhound on tile 718
at distance 2 and armed the existing generic type-1 monster combat backend:

```text
[ACTIONENGINE] TRACE seq=110 weapon=2 distance=2 tile=718
  target=sprite index=179 type=1 subtype=1
[MONSTERCOMBAT] ARM seq=110 sprite=179 tile=718 subtype=1 mType=1
  weapon=2 distance=2 hp=6/6 armor=2/2
```

The exact native combat roll then executed and rendered the Pistol attack pose:

```text
[MONSTERCOMBAT] ROLL seq=110 weapon=2 distance=2 worldDist=16384
  loops=1 hitLoops=1 firstRandHit=22 firstCalcHit=306
  firstCritLimit=15 firstRandDamage=63
  totalDamage=5 armorDamage=3 crit=0 rngCalls=4

[WEAPON] DRAW weapon=2 logical=242 actual=611 frame=1 pose=attack
```

The attack committed persistent monster/player consequences through the shared
owners:

```text
[MONSTERCOMBAT] COMMIT seq=110 sprite=179 weapon=2
  hp=6->0 armor=2->0 alive=1->0
  ammo=12->11
  visual=death4->corpse2/250ms+unlink
  xp=5-applied level=1->1 levelUps=0
  rollback=closed
```

The idle Pistol pose was restored on the settle frame (`actual=610`, frame 0).
This is a real live Pistol shot, not a renderer-only animation probe.

## Direct standard-weapon boundary

At this milestone the existing generic single-target player-combat transaction
owns these direct standard weapons:

```text
0 Axe
2 Pistol
3 Shotgun
5 Super Shotgun
```

The weapon metadata itself is already generic for the nine standard player
weapons. Remaining mechanical families are deliberately separate:

```text
1 Extinguisher special entity/fire semantics already have their own native path
4 Chaingun multi-loop presentation/commit
6 Plasma multi-loop presentation/commit
7 Rocket radius damage
8 BFG radius damage
9..11 familiar weapon attack semantics
```

Do not replace those families with per-weapon hard-coded branches.

## Post-kill conservative turn witness

After the Pistol killed sprite 179, the subsequent PLAYER_ATTACK MonsterTurn
found no living movement candidate while the map-session activation owner still
reported one active bit:

```text
[MONSTERTURN] COMPLETE reason=PLAYER_ATTACK candidates=0 ...
[MONSTERMOVE] DEFER ... candidates=0 activeCount=1
  cause=active-order-not-owned mutation=no rngConsumed=0
```

This is not a weapon-control/combat failure. The monster is already committed
dead and unlinked. Activation cleanup / multiple-active ordering remains a
separate fail-closed monster-turn boundary.

## Performance observation, not a correctness blocker

The physical 2x presentation remained about 34 ms in this run. The large attack
latencies instead coincided with world/sprite cache work, including a
`SPRITEPROFILE` around 478 ms and a later settle `worldUs` around 1.0 s. Correct
behavior and memory ownership remain the priority; presentation/cache tuning is
deferred until the native engine frontier is more complete.

## Memory witness

The real CYD remained at the established representative gameplay baseline after
the cycle and live Pistol kill:

```text
heap = 84608 B
heap8 = 18844 B
largest8 = 13812 B
shapeData = NULL
mediaTexels = NULL
PSRAM = none
```

No new weapon-control heap owner was introduced.
