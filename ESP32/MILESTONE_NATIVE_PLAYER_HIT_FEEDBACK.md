# Native player hit feedback — outgoing damage text + blood pixels

Status: **REAL-CYD PASS**

## Recovery / code boundary

```text
main = 5ac68378363b77daf9f98203966726557dc9b0ad
main merge = PR #141
branch = agent/esp32-native-player-hit-feedback
candidate code = e070057d3b9466f87189c504f86099b7e9f2fb67
CI = esp32-cyd run #350 / 35611816011 SUCCESS
artifact = doom-rpg-esp32-cyd-e070057d3b9466f87189c504f86099b7e9f2fb67
```

The branch was created from the exact post-rotation merge `main`. No combat
math, monster HP semantics, gameplay RNG ordering, topology or weapon transaction
was broadened in this milestone.

## Legacy behavior recovered

The player attack result path in `src/Combat.c::Combat_playerSeq()` reports
monster damage using the **raw damage pair sum**:

```text
visibleDamage = totalDamage + totalArmorDamage
```

Legacy text is:

```text
normal hit -> "<N> damage!"
critical   -> "Crit! <N> damage!"
miss       -> "Missed!"
```

On a lethal ordinary enemy hit, legacy appends `" <name> died!"` to the same
message. The first bounded native milestone intentionally leaves that name suffix
out because the current native top-bar lease displays at most 21 characters;
damage/crit/miss parity is recovered without expanding the UI contract.

Legacy enemy impact blood is also explicit:

```text
Combat_explodeOnMonster()
 -> enemy + hitType != 0 + valid tile distance
 -> Combat_spawnBloodParticles(...)
```

Recovered blood colors:

```text
default                         = 0xBB0000 -> RGB565 b800
subtype 6  with parm == 467     = 0x0000BB -> RGB565 0017
subtype 11 with parm == 467     = 0x00C000 -> RGB565 0600
```

Particle intensity is derived from the committed hit's
`totalDamage + totalArmorDamage`, monster max HP/armor, lethal overkill and
tile-distance scaling. The native overlay keeps the legacy integer intensity
shape but never imports the legacy `ParticleSystem_t` graph.

## Native implementation

### Top-bar result text

A new dynamic feedback kind:

```text
ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT = 7
```

reuses the already hardware-proven bounded top-bar renderer. It requests no
viewport border flash.

The text is still armed only **after** the monster-combat attack frame succeeds.
Blood is different: hardware showed that arming it after that frame made the
impact visibly late, especially on lethal hits. The candidate now provisionally
arms only the blood owner immediately before the attack-frame render:

```text
combat roll/mutation
 -> arm provisional blood owner
 -> attack-frame render/present = pain/death + blood on the same frame
 -> if render fails: cancel blood before exact rollback redraw
 -> if render succeeds: rollback boundary closed
 -> arm result text
 -> settle-idle frame/present
```

The blood owner is therefore rollback-cancellable while result text remains
post-commit. A failed render cannot leave a persistent hit effect.

A fresh gameplay redraw can occur while a 1200 ms message lease is still active.
The action feedback presenter now repaints an already-visible top-bar message on
a fresh framebuffer without restarting its lease. Dialog-owned PAK access still
wins and the repaint remains fail-safe/deferred.

### Blood overlay

Permanent API:

```c
int EspNativeGameplayHitFeedback_arm(
    uint32_t sequence,
    uint16_t spriteIndex,
    uint8_t distance,
    int32_t healthBefore,
    int32_t armorBefore,
    int32_t totalDamage,
    int32_t totalArmorDamage);
```

The owner is a tiny fixed structure in the existing native presentation layer.
It uses:

```text
max particles = 64
lease = 350 ms
screen-space center = 80,52 in the 160x120 logical framebuffer
pixel area = world viewport y=20..99
visual RNG = local deterministic xorshift
gameplay RNG = untouched
heap allocation = none
legacy ParticleSystem = none
```

The overlay uses the existing centered/cardinal native SELECT target contract.
The first hardware candidate rendered every particle near its spawn point and
looked like a dense red stamp. The current candidate instead recomputes a tiny
deterministic legacy-shaped kinematic spray from the recovered start/velocity/
gravity ranges (-6..6, -9..11, -150..100, -160..-60, gravity 10). Most droplets
are one logical pixel (already 2x2 physical pixels on CYD); only larger droplets
get a one-pixel dark tail. The owner stores no ParticleNode array.

It is redrawn on physical presents during its short lease. At expiry, the normal
native world renderer restores the viewport. Because visible action feedback is
refreshed across fresh frames, clearing the blood overlay does not truncate the
damage message.

Expected Serial witnesses:

```text
[MONSTERHITFEEDBACK] ARM seq=... sprite=...
 hit=1 crit=0 message=8 damage!
 textQueued=yes blood=armed
 mutation=no gameplayRng=untouched

[HITFX] ARM seq=... sprite=... distance=...
 damage=5+3 total=8 color565=b800 particles=...
 visualRng=local gameplayRng=untouched

[HITFX] PAINT ... ageMs=... leaseMs=350 motion=legacy-kinematic-spray ...

[ACTIONFEEDBACK] PAINT kind=7 text="8 damage!" ...

[HITFX] EXPIRE ... restored=world-redraw gameplayRng=untouched

[ACTIONFEEDBACK] REFRESH kind=7 lease=preserved freshFrame=yes
```

For a miss:

```text
[MONSTERHITFEEDBACK] ARM ... hit=0 ... message=Missed!
 textQueued=yes blood=none-miss
```

No `[HITFX] ARM` should occur for that attack.

## Real-CYD validation

The corrected kinematic spray and the stack-headroom repair are now
**hardware-proven on the real classic CYD**. A lethal Hellhound hit produced:

```text
[MONSTERCOMBAT] ROLL ... totalDamage=5 armorDamage=3 ...
[HITFX] ARM seq=17 ... total=8 ... particles=44 ...
[HITFX] PAINT seq=17 ... pixels=44 ... ageMs=190 ...
             motion=legacy-kinematic-spray ...
[VIDEO] Present ...
[MONSTERHITFEEDBACK] ARM ... message=8 damage!
             blood=armed timing=attack-frame ...
[MONSTERCOMBAT] COMMIT ... hp=6->0 ... visual=death4->corpse2 ...
```

The user confirmed the spray looks correct and is synchronized with impact.

The previous firmware had then hit a **loopTask stack canary** while attacking
the first zombie. The decisive failing sequence had been:

```text
[MONSTERCOMBAT] ROLL seq=37 ... totalDamage=2 armorDamage=1 ...
[HITFX] ARM seq=37 ... particles=6 ...
[NATIVEFRAME] WALL ...
Guru Meditation Error: Stack canary watchpoint triggered (loopTask)
```

There was no `[HITFX] PAINT` for that old zombie frame; the panic happened
inside the nested attack render before the presentation decorator completed.

The exact CI artifact ELF for code boundary
`6dfe67d3f639e5f9aad7849db6638042b7bdc508` shows
`servicePending()` reserving **1040 bytes** of automatic stack while it calls
the native world/sprite renderer. The largest unnecessary automatic object was
the 324-byte full `MonsterCombatOwner` rollback snapshot, alongside three
104-byte frame-stat records.

The stack repair at
`e070057d3b9466f87189c504f86099b7e9f2fb67`:

- moves only the 324-byte rollback owner to a bounded non-reentrant BSS owner;
- reuses one 104-byte frame-stat record for attack/rollback/settle instead of
  three simultaneous automatic records;
- leaves combat math, hit FX timing, RNG ordering and rollback semantics intact;
- follows the already-established project pattern used to recover previous
  loopTask canary failures in HUB/save paths.

CI #350 passed. The resulting ELF reduces the
`servicePending()` CFA/automatic frame from **1040 B to 608 B**. The real CYD
has now validated that repair too.

Runtime witness now advertises:

```text
[MONSTERCOMBAT] READY ... rollbackOwner=324B/static
                         renderStats=single-frame ...
```

## Final real-CYD witnesses

The repaired build completed both lethal and nonlethal player-hit paths without
a reboot.

Lethal Hellhound:

```text
[MONSTERCOMBAT] ROLL seq=84 ... totalDamage=5 armorDamage=3 ...
[HITFX] ARM seq=84 ... total=8 ... particles=44 ...
[HITFX] PAINT seq=84 ... pixels=44 ... ageMs=197 ...
             motion=legacy-kinematic-spray ...
[MONSTERHITFEEDBACK] ARM ... message=8 damage! ... timing=attack-frame ...
[MONSTERCOMBAT] COMMIT ... hp=6->0 ... visual=death4->corpse2 ...
[MONSTERTURN] SCHEDULE ... reason=PLAYER_ATTACK ...
[HITFX] EXPIRE ... paints=2 pixels=88 ... gameplayRng=untouched
```

Nonlethal zombie — this is the path that previously tripped the stack canary:

```text
[MONSTERCOMBAT] ROLL seq=109 ... totalDamage=2 armorDamage=2 ...
[HITFX] ARM seq=109 ... total=4 ... particles=7 ...
[HITFX] PAINT seq=109 ... pixels=7 ... ageMs=208 ...
[MONSTERHITFEEDBACK] ARM ... message=4 damage! ... timing=attack-frame ...
[MONSTERCOMBAT] COMMIT ... hp=4->2 armor=5->3 alive=1->1 ...
[MONSTERTURN] SCHEDULE ... reason=PLAYER_ATTACK ...
[MONSTERRETAL] COMMIT ... playerHP=27->24 armor=10->8 ...
[HITFX] EXPIRE ... paints=2 pixels=14 ... gameplayRng=untouched
```

A second zombie hit also completed the lethal branch:

```text
[MONSTERCOMBAT] ROLL seq=110 ... totalDamage=2 armorDamage=2 ...
[HITFX] PAINT seq=110 ... pixels=7 ... ageMs=190 ...
[MONSTERCOMBAT] COMMIT ... hp=2->0 ... visual=death4->corpse2 ...
[MONSTERTURN] SCHEDULE ... reason=PLAYER_ATTACK ...
[HITFX] EXPIRE ... paints=2 pixels=14 ...
```

No stack canary, Guru Meditation or reboot occurred. The damage text exactly
matches `totalDamage + armorDamage` in the supplied witnesses, the spray is
presented on the attack frame, its 350 ms lease restores the world cleanly, and
the 1200 ms top-bar lease remains independent.

Miss and crit formatting remain recovered from the legacy implementation but
were not required for this hardware PASS because the ordinary nonlethal, lethal,
retaliation and previously-crashing render paths are all exercised.

## Merge boundary

```text
hardware-tested code = e070057d3b9466f87189c504f86099b7e9f2fb67
post-test changes = documentation only
status = merge-ready
```
