# Native barrel subtype-1 milestone

Status: **REAL-CYD PASS for barrel destruction, causal chain reactions and concurrent chain visuals**.

```text
branch                         = agent/esp32-native-barrel-destructible
hardware-tested code head      = 1b93651699d981e34b2a10318936ddfa0cf7b2e8
esp32-cyd CI                   = #739 SUCCESS
static RAM                     = 44648 B / 327680 B
flash                          = 776205 B / 1310720 B
barrel entity family           = type 12 / subtype 1
explosion logical sprite       = 180
explosion animation frames     = 3
bounded barrel chain capacity  = 16
radius cells                   = 4 cardinal + 4 diagonal
```

The real classic CYD serial log is the runtime authority for this milestone.

## Goal

Recover the smallest permanent native path for Doom RPG explosive barrels
without reopening the legacy entity/world renderer.

The supported route is:

```text
native player attack
 -> type 12 / subtype 1 barrel target
 -> normal weapon ammo + combat RNG
 -> one damaging hit removes the root barrel
 -> 3-frame native explosion
 -> recovered radius damage
 -> neighboring barrels chain
 -> player radius damage when nonlethal
 -> normal PLAYER_ATTACK monster-turn request
```

Unrelated radius-hurtable families remain fail-closed.

## Legacy behavior recovered

The desktop/J2ME reference makes barrel death asynchronous through a GameSprite.
A killed barrel is removed and an explosion sprite is started at the barrel
position. When that explosion expires, radius damage is applied.

A critical visual ordering detail is that multiple barrels killed by the same
radius update each create their own explosion in the same legacy update. Those
sibling explosions therefore animate concurrently. Their radius callbacks still
execute in deterministic allocation/update order when the common lifetime
expires.

The native implementation mirrors that causal shape as bounded waves:

```text
wave 1 = directly shot barrel
  -> 3 explosion frames
  -> radius callback discovers/kills neighbors

wave 2 = every neighbor killed by wave 1
  -> all members animate together for the same 3 frames
  -> radius callbacks execute in deterministic chain order

later waves = same rule, up to 16 barrels total
```

The renderer extension is still bounded and pointer-light. Gameplay supplies a
small caller-owned array of transient world coordinates; one decoded explosion
frame is shared across all simultaneous members of the wave. No map-wide
transient object graph or second framebuffer is introduced.

## Radius contract

The recovered native barrel radius currently owns:

```text
4 cardinal cells = full blast component
4 diagonal cells = half blast component
barrel targets   = causal chain/removal
player target    = native nonlethal damage path
other hurtable families = fail-closed
```

Each explosion consumes one recovered RNG word/low byte and derives the blast
component with the legacy formula already used by the native trap path.

The entire candidate chain is preflighted before root commit. Runtime chain
growth is then checked against that preflight count; RNG, player state, removed
barrels and the requested monster turn all retain rollback until the transaction
closes.

## Real-CYD proof: distant shot

The user shot the center barrel from three tiles away. The hardware run proved a
three-barrel causal chain with no player radius hit:

```text
[ACTIONENGINE] TRACE ... distance=3 ... sprite index=283 ... route=BARREL_SUBTYPE1
[BARRELRADIUS] PREFLIGHT ... root=283 chain=3 ... visual=wave-batch-concurrent
[BARREL] HIT ... sprite=283 ... ammo=23->22 ... removed=0->1

[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=3/3

[BARRELRADIUS] CHAIN source=283 target=276 ... relation=cardinal mutation=removed
[BARRELRADIUS] CHAIN source=283 target=303 ... relation=cardinal mutation=removed
[BARRELRADIUS] BLAST ... source=283 wave=1 ... triggered=2 playerHits=0

[NATIVESPRITE] TRANSIENT ... anim=0 batch=1/2 pos=1568,1120 ...
[NATIVESPRITE] TRANSIENT ... anim=0 batch=2/2 pos=1696,1120 ...
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=3/3

[BARRELRADIUS] BLAST ... source=276 wave=2 ... playerHits=0
[BARRELRADIUS] BLAST ... source=303 wave=2 ... playerHits=0
[BARREL] COMMIT ... chainRemoved=3 playerRadiusHits=0 ... rollback=closed
```

The user explicitly reported the resulting animation as visually correct. This
closes the earlier defect where both neighbor barrels disappeared together but
their explosion animations were then played serially one after another.

## Real-CYD proof: close shot and lethal boundary

A second hardware run was performed one tile from the root barrel. It proves the
player-radius path up to the intentionally deferred lethal transition.

Root blast:

```text
[BARRELRADIUS] PLAYER-HIT source=283 ... relation=cardinal
    component=12 messageDamage=24
    hp=32->20 armor=20->8
[BARRELRADIUS] BLAST ... source=283 wave=1 ... triggered=2 playerHits=1
```

The two neighbor explosions then rendered concurrently as wave 2. The first
diagonal blast committed another nonlethal player hit:

```text
[BARRELRADIUS] PLAYER-HIT source=276 ... relation=diagonal
    component=10 messageDamage=20
    hp=20->8 armor=8->0
```

The second diagonal blast would have been lethal. Player death is not yet a
native-owned transition, so the permanent boundary remains fail-closed:

```text
[BARRELRADIUS] PLAYER-DEFER source=303 ... relation=diagonal
    component=6 status=1 hp=8 armor=0 mutation=no/lethal-deferred
[MONSTERTURN] ATTACK-CANCEL seq=5 cause=action-rollback scheduled=no
[BARRELRADIUS] ROLLBACK ... reason=runtime-radius-contract-or-player-death
    rollback=yes rng=yes player=yes world=yes
```

This is not counted as a barrel failure. It proves the transaction reaches the
known player-death boundary and restores the owned player/RNG/world state rather
than partially committing an unsupported lethal result.

## Damage-message qualification

The code can currently collapse several nonlethal barrel-radius player messages
into one bounded native summary because the full legacy five-message HUD queue
has not yet been migrated.

That presentation path is **not part of this hardware PASS**. The close-range
test reached lethal-deferred rollback before a final aggregate feedback commit,
and the user explicitly chose to postpone that check until normal inventory
healing/medkits make repeated nonlethal tests practical.

Do not claim the aggregate message as hardware-proven. Revisit it separately,
preferably when the native inventory/consumable path can restore health between
tests or when the proper legacy-style HUD message queue is migrated.

## Persistence qualification

Barrel removals use the existing compact action-engine removed-sprite ownership.
This milestone did not perform a dedicated SAVE/LOAD mirror test for destroyed
barrels, so barrel-specific checkpoint persistence is not newly claimed here.

## Memory and architecture

CI #739 on the exact hardware-tested code head:

```text
RAM   = 44648 / 327680 B = 13.6%
Flash = 776205 / 1310720 B = 59.2%
result = SUCCESS
```

Permanent constraints remain intact:

```text
shapeData == NULL
mediaTexels == NULL
no PSRAM
no map-wide explosion owner
no legacy Entity_t world ownership
no runtime ZIP dependency
/DoomRPG-ESP32.pak remains the native asset backing store
```

The transient batch capacity and barrel chain are both bounded at 16 entries.

## Closed vs deferred boundary

Hardware-proven now:

```text
direct barrel targeting at distance
weapon/ammo/combat RNG integration
root barrel removal
logical-180 explosion frames
cardinal/diagonal barrel chain discovery
causal wave ordering
simultaneous sibling explosion rendering
three-barrel chain commit
nonlethal player radius mutation
lethal player-radius detection + exact transaction rollback
PLAYER_ATTACK turn request/cancel ownership
```

Still intentionally deferred:

```text
native player death transition
radius damage to other hurtable entity families
barrel-specific SAVE/LOAD mirror
aggregate multi-blast damage-message hardware check
full legacy HUD five-message queue
audio/shake presentation
```

## Merge discipline

The real-CYD barrel code boundary is
`1b93651699d981e34b2a10318936ddfa0cf7b2e8`.

All commits after that hardware-tested code head must remain documentation-only
for this branch. Do not merge into `main` automatically.
