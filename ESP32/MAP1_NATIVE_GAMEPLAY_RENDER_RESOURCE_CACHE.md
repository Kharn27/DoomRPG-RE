# MAP1 native gameplay render resource cache

## Status

```text
branch = agent/esp32-native-gameplay-render-resource-cache
base main = f98a0b8e9eb4cbd38bf5678a1ce60c4989766985
hardware-tested implementation SHA = 1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
environment = esp32-cyd
status = REAL-CYD HARDWARE PASS
```

Serial output from the real classic ESP32-2432S028R remains the final hardware truth.

## Bounded milestone

This milestone attacks measured renderer/storage latency only. It does not add gameplay semantics and does not modify the world/sprite/HUD raster algorithms.

The permanent boundary is an opt-in resident owner around `/DoomRPG-ESP32.pak`:

```text
one physical validated default PAK stays resident during gameplay
historical EspAssetPack_open()/close() remain logical leases
complete disk index validation occurs once at resident begin
exact immutable ranges <= 1024 B may be cached
large world texture reads remain PAK-backed and uncached
range payload capacity = 16384 B
range key capacity = 256
entry descriptor cache slots = 24
shapeData = NULL
mediaTexels = NULL
runtime ZIP = forbidden
```

The historical non-resident pack contract remains unchanged. Resident ownership is explicit and can be torn down explicitly.

## Why this frontier

The previous hardware milestone proved canonical North gameplay recomposition around 3.2–3.35 seconds while `PlatformVideo_present()` cost only about 35 ms. The dominant debt was repeated SD/PAK work:

```text
world: reopen + validate PAK + rebuild source descriptors
sprites: reopen + validate PAK + repeated small frame fragments
HUD: reopen + validate PAK + repeated small BMP reads
```

The new owner reuses the validated PAK and exact small immutable ranges across world/sprite/HUD phases and across subsequent MOVE/TURN redraws.

## Real-CYD predecessor hot-path witness

Immediately before enabling the resident owner, the same firmware reproduced the previous bit-exact North route:

```text
frame    = ba3e5182
viewport = 9206eb24
HUD      = 6c2aa46f
tempHud  = 0
routeNoPresent = 1
finalPresent   = 1
world   = 1295232 us
sprites = 1610031 us
HUD     =  400296 us
present =   34935 us
total   = 3350141 us
heap8   = 66372 -> 66372
largest = 29684 -> 29684
exact   = yes
```

This confirms the cache test started from the already hardware-proven renderer semantics.

## Resident owner hardware proof

The real CYD then created the owner successfully:

```text
owner struct = 21160 B
heap8        = 66372 -> 40832
observed heap cost = 25540 B
largest8 after owner = 13812 B
payload cache = 9225 / 16384 B after canonical cold frame
range entries = 195 / 256
physicalResident = yes
logicalPackClosed = yes
```

The allocator cost is a hardware witness, not a portable semantic fingerprint.

The RAM tradeoff is material on a no-PSRAM CYD, but the subsequent interactive run remained heap-stable at:

```text
heap8   = 40832 B
largest = 13812 B
```

## Canonical North COLD proof

First cached recomposition after owner creation:

```text
leases          = 3
resident reuse  = 3
physical opens  = 0
validation pass = 0
SD reads        = 280
SD bytes        = 55541
entry cache     = 6H / 9M
range cache     = 155H / 195M / 195 stores / 22 bypasses
world           = 1044890 us
sprites         =  505972 us
HUD             =  378835 us
present         =   34925 us
total           = 1974252 us
frame           = ba3e5182
viewport        = 9206eb24
HUD bands       = 6c2aa46f
exact           = yes
```

Even the cold frame is materially faster because three independent physical PAK open/index-validation cycles have become logical leases over one validated backing store.

## Canonical North WARM proof

Second immediate recomposition:

```text
leases          = 3
resident reuse  = 3
physical opens  = 0
validation pass = 0
SD reads        = 22
SD bytes        = 45056
entry cache     = 15H / 0M
range cache     = 350H / 0M / 0 stores / 22 bypasses
world           = 209454 us
sprites         =   9604 us
HUD             =   1317 us
present         =  34908 us
total           = 264828 us
frame           = ba3e5182
viewport        = 9206eb24
HUD bands       = 6c2aa46f
heap stable     = yes
shapeData       = NULL
mediaTexels     = NULL
```

Physical reads therefore dropped:

```text
280 -> 22
saved = 258 reads
reduction ~= 92.1%
```

The 22 remaining warm physical reads total exactly 45056 B, i.e. 22 x 2048 B. They are the deliberate >1024 B bypass class, making the next remaining render debt sharply bounded.

Compared with the immediate uncached predecessor in the same run:

```text
3350141 us -> 264828 us
~12.65x lower total recomposition time
~92.1% total-time reduction
```

The user reports the result is now clearly more playable.

## Interactive MOVE/TURN proof with owner kept resident

After the strict cold/warm probe, the owner remained resident and the user exercised real calibrated touch MOVE/TURN actions. All shown actions kept:

```text
heap8 = 40832 -> 40832
largest8 = 13812 -> 13812
legacyStable = yes
residentStable = yes
tempHud = 0
routeNoPresent = 1
finalPresent = 1
Game_advanceTurn = no
Game_executeTile = no
facingRefresh = deferred
```

Representative successful actions:

```text
FORWARD 943->911 total=255050 us  world=200882 sprite=8224  HUD=1367 present=34979
FORWARD 911->879 total=272550 us  world=219315 sprite=7272  HUD=1387 present=34981
TURN_RIGHT angle64->0 @ tile879 total=160497 us world=103077 sprite=3140 HUD=1363 present=34991
TURN_LEFT  angle0->64 @ tile879 total=272608 us world=219363 sprite=7289 HUD=1376 present=34976
TURN_LEFT  angle64->128 @ tile879 total=260054 us world=93246 sprite=4059 HUD=110734 present=34980
TURN_RIGHT angle128->64 @ tile879 total=272618 us world=219350 sprite=7294 HUD=1375 present=34994
FORWARD 879->847 total=235073 us  world=181064 sprite=8072  HUD=1359 present=34975
FORWARD 847->815 total=290621 us  world=200233 sprite=44406 HUD=1396 present=34964
TURN_RIGHT angle64->0 @ tile815 total=203124 us world=151849 sprite=5330 HUD=1355 present=34976
```

The one 110734 us HUD sample occurs on the first observed West-direction compass repaint in this session; subsequent already-warm compass paths return to ~1.3 ms. It is recorded as a witness, not hidden or generalized away.

## What the hardware proves

The milestone permanently proves that a bounded persistent PAK/resource owner is viable on the classic CYD and that repeated small immutable storage work was the dominant source of perceived input latency.

It also proves all of the following simultaneously:

```text
bit-exact canonical framebuffer remains unchanged
one physical PAK can serve repeated logical render leases
no repeated validation pass is needed per world/sprite/HUD phase
small exact-range reuse can collapse sprite/HUD latency to milliseconds
heap is stable once the permanent owner has been allocated
MOVE/TURN remain live at non-spawn positions
shapeData == NULL
mediaTexels == NULL
no runtime ZIP fallback
```

## Remaining measured renderer debt

On a fully warm canonical North frame:

```text
world   = 209454 us  ~79.1%
sprites =   9604 us  ~3.6%
HUD     =   1317 us  ~0.5%
present =  34908 us  ~13.2%
total   = 264828 us
```

The storage witness is even more useful than the timing split:

```text
warm SD reads = 22
warm SD bytes = 45056
22 x 2048 B exact bypasses
```

So the next renderer optimization should not reopen the broad cache problem. It should target the remaining 2048-byte wall/plane texture range class with a very small explicit RAM budget, ideally reusing already allocated resident payload capacity before increasing permanent heap cost.

`PlatformVideo_present()` is no longer negligible relative to the newly optimized frame, but it should still not be touched before the remaining 2048-byte PAK reads are understood and bounded.

## Still intentionally outside

```text
Game_eventFlagsForMovement
post-move tile-event execution
actual turn advancement
dynamic opened-line/entity relinking
SELECT interaction
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP gameplay execution
entity/monster AI
facing refresh after gameplay actions
weapon overlay
save persistence
sound
```

## Merge rule

The real-CYD hardware-tested code SHA is exactly:

```text
1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
```

No code may change after this SHA without another hardware run. Closeout commits after it must be documentation-only.
