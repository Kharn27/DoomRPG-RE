# MAP1 native gameplay renderer viewport hot path

## Status

```text
branch = agent/esp32-native-gameplay-render-hotpath
base   = b5a4426eb0df1ef1506893d4bc08b5538543a7b3
base PR = #94 — native cardinal movement + collision
hardware-tested implementation SHA = a07455e34eadbacca7d23fb068ba4308f0b7f80a
status = REAL-CYD HARDWARE PASS
merge-ready = yes after documentation-only closeout audit
```

Normal hardware environment: `esp32-cyd` on the classic ESP32-2432S028R CYD with no PSRAM.

The user also explicitly reported that the milestone produced **no notable perceived fluidity improvement versus `main`**. The new timing witnesses explain why: the eliminated intermediate physical present was only about 34 ms while world, sprite and HUD recomposition still consume roughly 2.8–3.2 seconds per canonical/heavy view.

## Objective

Replace the transitional successful-action gameplay compositor path:

```text
whole-frame historical clear
 -> render world
 -> physical intermediate world present
 -> restore 12.8 KiB saved HUD bands
 -> repaint compass
 -> physical final present
```

with a permanent gameplay-only world boundary:

```text
preserve HUD pixels in place
 -> redraw only logical world viewport 160x80 @0,20
 -> no intermediate physical present
 -> compose sprites/glows
 -> repaint only direction HUD region
 -> one final complete-frame present
```

The milestone changes renderer plumbing only. It deliberately adds no new gameplay semantics:

```text
Game_advanceTurn = no
Game_executeTile = no
post-move tile events = no
facing refresh = deferred
entity/monster gameplay = no
```

## Permanent native API changes

The historical first-frame renderer now exposes a gameplay-only route:

```text
EspNativeFirstFrame_renderGameplayViewport(...)
```

Contract:

- same compact native BSP/wall/plane source as the hardware-proven first frame;
- writes only the configured `160x80 @0,20` world viewport;
- does not clear pixels outside that viewport;
- does not present;
- does not mutate the historical global first-frame owner;
- remains PAK-backed with `shapeData == NULL` and `mediaTexels == NULL`.

`EspNativeGameplayFrameStats` was extended with exact phase fingerprints and timing witnesses:

```text
frameStatsBytes = 104 B
frameBeforeFNV
worldFrameFNV
frameAfterFNV
viewportBeforeFNV
viewportAfterWorldFNV
viewportAfterSpritesFNV
hudBandsBeforeFNV
hudBandsRestoredFNV
hudBandsAfterFNV
worldMicros
spriteMicros
hudMicros
presentMicros
totalMicros
temporaryHudBytes
worldRouteNoPresent
finalPresented
```

The gameplay compositor now owns bounded static scratch rather than a temporary 12.8 KiB HUD heap buffer.

## Native compact render-wrapper boundary

The final pre-PASS correction is:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
fix(esp32): activate native render wrappers for gameplay viewport
```

The hardware-proven wrappers around `Render_initColumnScale()` and `Render_cullBoundingBox()` historically identified the native render context using:

```text
!EspNativeFirstFrame_isReady()
```

That condition was only accidentally true for the boot first-frame route. The new gameplay viewport route intentionally leaves the historical first-frame owner ready, so the wrappers initially stopped activating during gameplay recomposition.

Observed real-CYD failure before the fix:

```text
[NATIVEFRAME] BSP ...
[NATIVEFRAME] WALL ...
[HOTPATHPROBE] FAILED viewport gameplay render
```

Visually, the framebuffer contained walls but sprites disappeared. The plane injection had not run, so the compositor failed closed before the sprite phase.

The permanent fix identifies the actual compact native world architecture instead:

```text
EspMapRuntime loaded
EspNativeGraphicsCatalog ready
EspAssetPack open
160x80 compact world render context
legacy lines/nodes/mapSprites/media offset arrays NULL
shapeData NULL
mediaTexels NULL
```

This restores native plane injection and `tmpLine` preservation for both boot and gameplay rendering without resetting or coupling to the historical first-frame state.

## Real-CYD canonical hot-path proof

Hardware-tested firmware:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

The strict canonical North probe re-rendered an already displayed gameplay frame and returned bit-exact:

```text
[NATIVEPLANE] rows=80 pixels=12800 textures=6 cache=12795H/5M/0E reads=10240B
[NATIVEFRAME] BSP nodes=39 leaves=12 nodeCull=8 lines=62 backface=20 clip=8 occluder=0 spriteSpanDeferred=0
[NATIVEFRAME] WALL requests=34 draws=34 spans=166 pixels=4341 cache=17H/17M/14E resolvedTextures=30 animationTime=0
[HOTPATH] READY
  frameStatsBytes=104
  frame=ba3e5182
  viewport=9206eb24
  hud=6c2aa46f
  tempHud=0
  routeNoPresent=1
  finalPresent=1
  heap=66452->66452
  largest=29684->29684
  exact=yes
```

The canonical renderer fingerprints therefore remain unchanged:

```text
complete gameplay frame = ba3e5182
world viewport           = 9206eb24
HUD bands                = 6c2aa46f
```

There is no 12.8 KiB HUD save/restore allocation and no physical presentation inside the world route.

## Real-CYD timing truth

Canonical North hot-path timing:

```text
world   = 1261184 us
sprites = 1572941 us
HUD     =  387161 us
present =   34930 us
total   = 3265801 us
```

Approximate share of total time:

```text
world   ~38.6%
sprites ~48.2%
HUD     ~11.9%
present  ~1.1%
```

The four measured phases sum to 3256216 us; the remaining ~9585 us is compositor/probe accounting overhead.

This is the key performance result of the milestone: `PlatformVideo_present()` is **not** the dominant successful-action cost. Removing one redundant physical present can save only about 34 ms in a 3.0–3.3 s heavy frame, which matches the user's report of no notable perceptual improvement.

## TURN proof on the new path

### North -> East

```text
TURN_RIGHT angle 64->0
frame ba3e5182 -> 8cfdfe34
viewport 9206eb24 -> 17c48c15
HUD 6c2aa46f -> 1d908304
worldRouteNoPresent=1
tempHud=0
finalPresent=1
world=1166044 us
sprite=233857 us
hud=387430 us
present=34931 us
total=1835575 us
heap=66452->66452
largest=29684->29684
legacyStable=yes
residentStable=yes
```

### East -> North exact round-trip

```text
TURN_LEFT angle 0->64
frame 8cfdfe34 -> ba3e5182
viewport 17c48c15 -> 9206eb24
HUD 1d908304 -> 6c2aa46f
canonicalRoundTrip=exact
worldRouteNoPresent=1
tempHud=0
finalPresent=1
world=1260943 us
sprite=1572910 us
hud=387212 us
present=34910 us
total=3265560 us
heap=66452->66452
largest=29684->29684
legacyStable=yes
residentStable=yes
```

The large sprite-time difference between East and North is legitimate visibility workload: East admitted only four sprite candidates while canonical North admitted the full 21-base/7-glow population.

## MOVE proof on the new path

Three consecutive forward moves remained live and stable.

### 943 -> 911

```text
pos 992,1888 -> 992,1824
frame ba3e5182 -> 66da9d16
walls=32 / 4384 pixels
planes=12800
sprites=19 / 2097 pixels
glows=5 / 112 pixels
spriteReads=148
world=1252578 us
sprite=1354506 us
hud=387152 us
present=34921 us
total=3038743 us
tempHud=0
routeNoPresent=1
```

### 911 -> 879

```text
pos 992,1824 -> 992,1760
frame 66da9d16 -> fc7a5142
walls=30 / 4411 pixels
planes=12800
sprites=15 / 1123 pixels
glows=3 / 199 pixels
spriteReads=136
world=1270416 us
sprite=1250343 us
hud=387204 us
present=34911 us
total=2952445 us
tempHud=0
routeNoPresent=1
```

### 879 -> 847

```text
pos 992,1760 -> 992,1696
frame fc7a5142 -> 3625f7a7
walls=28 / 4437 pixels
planes=12800
sprites=15 / 2658 pixels
glows=3 / 456 pixels
spriteReads=136
world=1233537 us
sprite=1251120 us
hud=387183 us
present=34918 us
total=2916327 us
tempHud=0
routeNoPresent=1
```

All successful samples preserved:

```text
heap8=66452->66452
largest8=29684->29684
stackHighWater=172
legacyStable=yes
residentStable=yes
orientationStable=yes for MOVE
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Why the remaining renderer is slow

The timing probe turns the previous general performance suspicion into a concrete next target.

Current world rendering still performs a transient asset session for every recomposition:

- opens `/DoomRPG-ESP32.pak`;
- validates/scans the complete PAK index on every open;
- performs disk-backed binary searches for entries;
- rebuilds all 30 resolved wall texture descriptors;
- uses only a three-slot transient wall cache;
- closes the PAK at the end of the world phase.

Current sprite rendering then starts another independent transient asset session:

- opens and validates the PAK again;
- resolves mappings/palettes/bitshape/texel sources again;
- loads one sprite frame at a time into a single workspace;
- canonical North performs 21 base frame loads + 7 glow frame loads and 172 PAK range reads despite only nine unique base logical sprite IDs;
- closes the PAK again.

Current direction HUD phase starts yet another asset session:

- opens/validates the PAK again;
- opens `k.bmp`, `o.bmp`, `a.bmp` again;
- performs 63 PAK reads for a small compass update;
- takes ~387 ms independently of view complexity.

These repeated bounded reads are architecturally legal but are now the measured dominant performance debt.

## Permanent invariants preserved

```text
shapeData == NULL
mediaTexels == NULL
legacy Game.entities == 0
legacy Game.monsters == 0
runtime ZIP map/graphics access == forbidden
backing store == /DoomRPG-ESP32.pak
world redraw == on demand after successful action
world viewport == 160x80 @0,20
HUD bands preserved in place during world/sprite phases
intermediate world present == none
temporary HUD save == 0 B
```

No broad legacy `DoomCanvas_playingState()` or `Render_render()` path is enabled.

## Closeout / merge boundary

The implementation tested on hardware is exactly:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

No code may change after that SHA without another real-CYD run. All closeout commits after it must be documentation-only.

## Preferred next bounded milestone after merge

Do **not** optimize `PlatformVideo_present()` next. The hardware benchmark proves it is only ~1% of the canonical heavy frame.

The next coherent performance milestone should be a **bounded persistent native render-resource/cache owner**, keeping `/DoomRPG-ESP32.pak` as backing storage while eliminating repeated metadata/index/frame reconstruction.

Preferred order inside that separate milestone:

1. retain validated immutable PAK entry/source metadata needed by gameplay rendering so each phase does not reopen/rescan the complete pack index;
2. add a small bounded sprite-frame cache keyed by logical/actual frame so repeated visible objects do not reload identical bitshape/palette/texel payloads;
3. retain the tiny HUD compass resources/metadata in bounded native storage so a direction repaint does not cost 63 SD reads;
4. only then evaluate a bounded persistent wall/plane texel cache with explicit RAM budget and eviction policy.

The milestone must keep exact framebuffer canons, `shapeData == NULL`, `mediaTexels == NULL`, no map-wide asset expansion, no runtime ZIP dependency, stable heap, and fail-closed behavior.
