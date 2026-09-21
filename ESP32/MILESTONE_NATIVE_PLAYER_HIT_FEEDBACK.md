# Native player hit feedback — outgoing damage text + blood pixels

Status: **CANDIDATE — awaiting real-CYD validation**

## Recovery / code boundary

```text
main = 5ac68378363b77daf9f98203966726557dc9b0ad
main merge = PR #141
branch = agent/esp32-native-player-hit-feedback
candidate code = 6dfe67d3f639e5f9aad7849db6638042b7bdc508
CI = esp32-cyd run #346 / 35602149336 SUCCESS
artifact = doom-rpg-esp32-cyd-6dfe67d3f639e5f9aad7849db6638042b7bdc508
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

## Required real-CYD validation

Use normal `esp32-cyd` firmware from the exact code boundary.

1. Hit a normal enemy without killing it. Confirm a visible `"<N> damage!"`
   top-bar message and a **spray**, not a compact red blob. Blood should appear
   on the same attack/pain frame, not one frame after it.
2. Verify Serial `N` equals `totalDamage + armorDamage` from the preceding
   `[MONSTERCOMBAT] ROLL`.
3. Confirm `[HITFX] ARM`, `PAINT` and `EXPIRE`; the world must restore
   cleanly after the 350 ms lease while the message can remain for its normal
   1200 ms lease.
4. Kill an ordinary enemy and confirm the impact spray appears while the death
   pose is being presented, rather than only after the monster has fallen.
   Existing corpse/gib behavior must remain intact. The monster-name death suffix
   is intentionally deferred.
5. If a miss is encountered, confirm `"Missed!"` and no blood.
6. If practical, a crit should display `"Crit! <N> damage!"`.
7. Confirm the already-proven `PLAYER_ATTACK` monster-turn scheduling remains
   unchanged and no extra gameplay RNG is consumed by hit FX.

Only after the real classic CYD witness should the branch be locked to a
documentation-only tail.
