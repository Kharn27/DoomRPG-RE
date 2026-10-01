# ESP32 — native Fire Ext monster combat semantics

Date: 2026-10-01

## Boundary

```text
branch = agent/esp32-retire-legacy-particle-startup-v22
base hardware boundary = 21ee2c95afd351af5c20ba38d6ef897bd81d1d05
code = ef8dc9b5f06dd93c34c5179f6b95935dd0af13d2
current main in lineage = 72e351f8d26f4c3ac22d766a086de6646bbaf77b
```

## Problem

The native monster-combat gate incorrectly classified Fire Ext (`weapon=1`)
as an unsupported special entity-rule family. That prevented attacks against
Phantoms and other enemies.

The desktop/J2ME reference does not do that for monsters. Once the selected
target is an enemy (`eType == 1`), Fire Ext uses ordinary
`CombatEntity_calcHit()` / `CombatEntity_calcDamage()` combat. Its
extinguisher-specific behavior is an impact/presentation rule, not a ban on
monster combat.

## Native ownership

Commit `ef8dc9b5f06dd93c34c5179f6b95935dd0af13d2`:

- adds weapon 1 to the direct monster-combat mask;
- passes weapon identity into the bounded HITFX owner;
- gives subtype-4 Phantom hits exactly 15 gray `RGB565 ce79` particles;
- suppresses normal blood particles for Fire Ext hits on all other subtypes;
- preserves ordinary HP/armor/ammo/RNG combat ownership;
- uses legacy `No effect!` for an actual Fire Ext miss;
- does not restore or depend on legacy `ParticleSystem`.

## CI

CI #1247: SUCCESS.

```text
static RAM       44936 B
linked Flash    757869 B
firmware.bin    758240 B
artifact id     11189341147
artifact digest sha256:42e47397c8792f82f0129c8c85340bd1535374ad42787ec38c0c0a5757febdc0
firmware sha256 6f14c4b71ef9a51a43f0ea26ce480417cd54cf66ec2b6eddb40a11cfb5592833
ELF sha256      76d902338352539e18e6dc640de7d219bac2b942d98e34a85347edb800fea5ed
active wraps    49
```

Static RAM is unchanged from the V24 hardware boundary.

## Real-CYD validation

A V9 Sector 1 checkpoint is loaded. Four active monsters are restored and the
existing ordered movement/three-goal logic remains live.

### Phantom witness 1

```text
[MONSTERCOMBAT] ARM seq=4 sprite=1 tile=506 subtype=4 mType=4 weapon=1 ...
[MONSTERCOMBAT] ROLL ... totalDamage=7 armorDamage=28 crit=1 rngCalls=3
[HITFX] ARM ... subtype=4 weapon=1 mode=extinguisher-gray ... color565=ce79 particles=15 ...
[MONSTERHITFEEDBACK] ... message=Crit! 35 damage! ... impact=extinguisher-gray-armed ...
[MONSTERCOMBAT] COMMIT ... hp=5->0 armor=3->0 alive=1->0 ... ammo=14->13 ...
```

The following native death overlay is also exercised:

```text
[GIBFX] PAINT ... legacyParticleSystem=no
[GIBFX] REPAINT ...
[HITFX] EXPIRE ... gameplayRng=untouched
[GIBFX] EXPIRE ... gameplayRng=untouched
```

### Phantom witness 2

```text
[MONSTERCOMBAT] ARM seq=5 sprite=0 ... subtype=4 ... weapon=1 ...
[HITFX] ARM ... mode=extinguisher-gray ... color565=ce79 particles=15 ...
[MONSTERCOMBAT] COMMIT ... hp=5->0 armor=3->0 alive=1->0 ... ammo=13->12 ...
```

This independently repeats the same subtype-4 rule.

### Non-Phantom witness

The next Fire Ext attack targets subtype 5:

```text
[MONSTERCOMBAT] ARM seq=6 sprite=237 ... subtype=5 ... weapon=1 ...
[MONSTERCOMBAT] ROLL ... totalDamage=0 armorDamage=1 ...
[MONSTERHITFEEDBACK] ... message=1 damage! ... impact=none-extinguisher ...
[MONSTERCOMBAT] COMMIT ... hp=14->14 armor=6->5 alive=1->1 ... ammo=12->11 ...
```

There is no HITFX spray and no blood path, while combat and ammo commit
normally.

No `reason=weapon-entity-rule-family` appears anywhere in the tested sequence.

## Memory / regression witness

After LOAD, ordered movement, two Phantom Fire Ext kills, subtype-5 Fire Ext
damage and live monster retaliation, ALIVE remains stable:

```text
heap=116288 heap8=50364 largest8=38900
```

The run also preserves `shapeData=0x0 mediaTexels=0x0` at session READY.

## Next boundary exposed by the same log

Subtype-4 movement reaches the player after its three-goal sequence, but the
attack is intentionally still stopped at:

```text
[MONSTER3GOAL] ATTACK-GATE-DEFER ... loops=3 cause=multi-loop-attack-family-deferred
```

The next bounded milestone is therefore to connect the already-native
three-loop monster roll/presentation/retaliation path to the subtype-4/13
post-move three-goal attack gate, preserving exact activation order, RNG and
fail-closed lethal-player behavior.
