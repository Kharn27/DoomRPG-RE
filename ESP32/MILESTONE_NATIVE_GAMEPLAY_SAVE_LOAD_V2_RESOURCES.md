# Milestone — Native gameplay checkpoint v2: resource consumed overlay

Status: **REAL-CYD PASS**

This milestone extends the bounded one-slot native checkpoint without serializing a desktop/legacy world graph. The only new persistent world section is the permanent native player-resource consumed overlay.

## Code boundary

```text
branch = agent/esp32-native-gameplay-changemap-transition
hardware-tested save-v2 resource boundary = f52d3f272e75ed29f68037fd343e40252d2ec6bf
hardware-tested HUB feedback ownership boundary = 5a1020fd5d160c111ff09ecb8a480f37ea8d0578
docs head before this update = 48f8bf50c3f59acf2260d4274467bc78080690a8
CI save-v2 = esp32-cyd run #265 / run ID 35196771704 / SUCCESS
CI HUB feedback fix = esp32-cyd run #267 / run ID 35198140562 / SUCCESS
CI previous docs tail = esp32-cyd run #268 / run ID 35198645805 / SUCCESS
```

## Save format

```text
path = /sd/DoomRPG-ESP32.sav
v1 magic = DRPGSAV1
v1 bytes = 132
v2 magic = DRPGSAV2
v2 bytes = 276
write format = v2
v1 read compatibility = retained
```

V2 is the exact proven 132-byte semantic core plus one pointer-free `EspNativeGameplayPlayerResourcesSnapshot` section. The section is fixed/bounded and stores explicit identity and bitset data only:

```text
sourceArenaFNV1a
spriteCount
consumedCount
consumedBytes
targetMapId
consumedBits[128]
```

No runtime owner pointer, heap address, renderer object, legacy Entity/Player object, or map-wide desktop graph is persisted.

Entrance uses:

```text
sprites = 344
consumed bitset = 43 B
runtimeFNV = c3882516
```

## Hardware SAVE witness

The user consumed one Armor Shard before SAVE. The exact checkpoint witness was:

```text
[NATIVESAVE] SAVE path=/sd/DoomRPG-ESP32.sav
version=2 bytes=276 map=1 gameplayLoadMapId=1
pos=480,1696 angle=128
playerFNV=548397a5 runtimeFNV=c3882516
sourceBytes=21823 sourceCrc=623f34e4
recordCrc=4d3bce59
resources=1/43B sprites=344
atomic=temp+backup+rename
world=resources-restored+others-fresh
```

The saved player root contained Armor 4/20.

## Post-SAVE mutation

After the checkpoint, the user continued gameplay and consumed additional resources. Before LOAD the live player fingerprint had changed to:

```text
playerFNV=549e6620
```

The test intentionally included additional Armor Shard / medkit pickup state after SAVE.

## Hardware LOAD witness

The load rebuilt `/intro.bsp` from the native PAK, verified the immutable runtime identity, recreated the normal bounded resource owner, and imported only the saved semantic bitset:

```text
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 ...
[PLAYERRES] RESTORE map=1 arena=c3882516 sprites=344 consumed=1 bytes=43
                 mutation=consumed-overlay-only allocation=owner-bounded
[NATIVESAVE] REPRIME-HUD map=1 gameplayLoadMapId=1 angle=128
             refresh=pending clear=ready mutation=owners-only turn=no
[NATIVESAVE] LOAD ... version=2 bytes=276 map=1
             pos=480,1696 angle=128 playerFNV=548397a5
             runtimeFNV=c3882516 sourceBytes=21823 sourceCrc=623f34e4
             resources=restored/1/43B
             world=resources-restored+others-fresh
```

The complete native session returned:

```text
[ENGINESESSION] HUD ... hp=30/30 armor=4/20 weapon=2 ammo=8 ...
[MAPFLASH] REUSE HIT ... rebuild=no
[MAPFLASH] ARM ... resident=1
[ENGINECACHE] PRIMED ...
[RESIDENTGAMEPLAY] READY ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

## Material world proof

The real-CYD visual/gameplay result matched the required two-direction contract:

```text
pickup consumed before SAVE -> remains absent after LOAD
pickup consumed after SAVE -> reappears after LOAD
```

The user explicitly confirmed this with an Armor Shard and the same behavior with a medkit. Therefore the resource-consumed overlay is not merely restored in memory: its normal topology/render projection reproduces the saved world state.

## Validated boundary

Hardware-proven after this milestone:

```text
pose/player root = restored exactly
resource consumed overlay = restored exactly for the saved map/runtime identity
post-SAVE resource mutations = rolled back
pre-SAVE consumed resources = remain hidden/consumed
post-SAVE consumed resources = become available again
unowned world families = intentionally fresh
shapeData = NULL
mediaTexels = NULL
```

Still intentionally fresh / not persisted by v2 yet:

```text
script/event mutable state
line open/locked state
line texture variants
automap reveal state
monster state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

## HUB/action-feedback ownership bug — REAL-CYD PASS

The save-v2 test firmware exposed an unrelated framebuffer lease race: action feedback could expire while HUB owned the screen, causing a stale pickup-message fragment over the MENU area and a recover path:

```text
[ACTIONFEEDBACK] EXPIRE ... restored=topbar-only
...
[HUB] CLOSE ... menuUnderlayRestore=FAILED ... exactHud=NO
[RESIDENTGAMEPLAY] HUB-RECOVER ...
```

The branch added two bounded ownership commits:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
  ESP32: pause action feedback while HUB owns framebuffer
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
  ESP32: gate world feedback service behind HUB ownership
```

The fix keeps timers based on real elapsed time but prevents world-feedback restore work from touching framebuffer pixels while HUB owns the screen. CI #267 passed, and the user then reproduced the original pickup-message/HUB sequence on the real CYD and explicitly confirmed that the stale message fragment no longer appears. This visual ownership fix is therefore hardware-valid at `5a1020fd5d160c111ff09ecb8a480f37ea8d0578`.

## Next bounded persistence slice

Continue owner-by-owner. Do not broaden v2 into a monolithic world dump. The next candidate is native script/event mutable state, followed separately by line state/texture variants, automap, monster state/position, and RNG as independent milestones.
