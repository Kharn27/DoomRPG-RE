# Native player death core

Status: **REAL-CYD PASS — 2026-10-01**

Hardware-tested code boundary:
`d6811e23db4580795887c97c3bdf5e6493224268`

Base main:
`88a5d3fa5bfe96fe16e213e78933493394264dcc`

Branch:
`agent/esp32-native-player-death-core`

## Scope

This milestone owns only the first terminal player-death path:

```text
PASS_TURN
 -> current-tile type-10/11 hazard
 -> authoritative PlayerState HP reaches 0
 -> native PlayerDeath ARM
 -> no MonsterTurn
 -> input locked
 -> camera fall
 -> viewport fade to black
 -> death-menu-ready
```

Lethal MOVE hazards, monster retaliation, barrel/radius damage and other lethal
producers deliberately remain fail-closed. The death menu itself is also not
owned here.

## Permanent native ownership

`EspNativeGameplayPlayerDeath` is a bounded 24-byte state owner. It does not
allocate a framebuffer or touch legacy world/entity/render state.

On arm it requires PlayerState health already committed to zero, then reproduces
the terminal subset of the legacy death transition:

- clears `weapon` and `weapons` so the first-person weapon is no longer drawn;
- consumes exactly one gameplay RNG byte for the recovered 5058/5059 death-sound
  choice while sound playback remains deferred;
- blocks resident gameplay input;
- prevents a post-death MonsterTurn;
- drives only `EspPlayerView.viewZ` for the fall;
- clamps the existing RGB565 world viewport during fade;
- finishes in `death-menu-ready` without silently falling back to legacy menus.

The recovered presentation timing is 750 ms for the fall and 3000 ms total to
the menu boundary. The viewport fade follows legacy `Render_fadeScreen()`
component clamping rather than alpha blending.

## Hardware proof

The real classic CYD reaches the lethal type-10 hazard from HP 1:

```text
[HAZARDPASS] LETHAL-COMMIT tile=535 sprite=248 type=10 hazards=1 rawDamage=1+2 hp=1->0 armor=0->0 ... deathOwner=pending rollback=armed monsterTurn=no
[GAMEPLAYHUD] REPAINT health=0/38 armor=0/28 weapon=0 ammo=13 ...
[PLAYERDEATH] ARM seq=25 tile=535 ... hp=0 weapons=000f->0000 weapon=0->0 rngByte=65 deathSound=5058-deferred ... viewZ=36 fallMs=750 fadeMs=750..3000 ... input=blocked ... ownerBytes=24
[PASSTURN] DEATH seq=25 tile=535 ... deathOwner=armed monsterTurn=no input=blocked ...
[PLAYERDEATH] PHASE seq=25 elapsedMs=845 fall=complete viewZ=6 fade=begin input=blocked
[PLAYERDEATH] READY seq=25 tile=535 elapsedMs=3004 phase=death-menu-ready viewZ=6 fade=0 frames=49 input=blocked menuOwner=deferred
```

The user confirmed the physical camera fall is clearly visible and the world
then becomes fully black. No MonsterTurn follows the lethal commit.

Repeated post-death samples stay stable:

```text
[ALIVE] ... heap=116264 heap8=50340 largest8=38900 ... PAK=ready VIDEO=ready CORE=ready ...
```

The non-responsive HUD after the black screen is expected: world input is owned
and blocked by `ST_DYING`, while the death-menu UI is intentionally deferred
to the next milestone.

## CI

GitHub Actions `esp32-cyd` run #1274: SUCCESS.

```text
static RAM  = 44960 B
linked Flash = 765497 B
artifact = 11193296410
digest = sha256:5770b336e4c4686fb6ec189a864f234d0017347a87adac83ab5eb0f67d4a24b5
```

## Next boundary

Add the native death menu on top of the now-proven terminal owner. Do not reopen
world input and do not route back through legacy `Menu_t`. Once the menu is
hardware-proven, connect additional lethal producers one family at a time.
