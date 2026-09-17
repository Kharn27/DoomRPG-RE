# Milestone — native gameplay save/load v1

Status: **REAL-CYD HARDWARE PASS**

Branch at hardware validation:

```text
agent/esp32-native-gameplay-changemap-transition
hardware-tested code boundary = 1c2cbe13d6001459eb8679f7262e495a51585476
base main = 9b085a9d8ed254edc98463f33f6d1534215326b9
```

GitHub Actions `esp32-cyd` run #258 / run ID `35194006759` completed successfully on the exact tested SHA.

## Scope

This milestone owns one bounded native checkpoint slot:

```text
/sd/DoomRPG-ESP32.sav
```

It is intentionally not the desktop/J2ME save graph and not a multi-slot UI. The record is a compact ESP32-native checkpoint persisted through the HUB `STAT` page.

Current file contract:

```text
magic = DRPGSAV1
version = 1
record bytes = 132
write = temp + reread + backup rename + commit rename + reread
```

The record owns:

- source BSP byte count and CRC32;
- immutable native runtime FNV;
- full `EspPlayerViewState`;
- full 52 B `EspNativeGameplayPlayerState`;
- player FNV;
- target map ID, gameplay load-map ID and load type;
- record CRC32.

The slot never serializes pointers or desktop/J2ME object graphs.

## Save contract

SAVE is accepted only from a settled fresh-map gameplay view with the resident map ready and the PAK closed. The checkpoint is validated before and after the atomic rename sequence.

Hardware witness:

```text
[NATIVESAVE] SAVE path=/sd/DoomRPG-ESP32.sav version=1 bytes=132
             map=1 gameplayLoadMapId=1
             pos=416,1696 angle=128
             playerFNV=363261d1 runtimeFNV=c3882516
             sourceBytes=21823 sourceCrc=623f34e4
             recordCrc=e2974fa8
             atomic=temp+backup+rename world=fresh-rebuild
```

At that checkpoint the real CYD player had picked up two Armor Shards:

```text
armor = 8/20
weapon = 2
weapons = 0004
ammo1 = 8
```

SAVE does not advance a turn.

## Load contract

LOAD first validates the record, then tears down the current session/resident owners, rebuilds the saved BSP from `/DoomRPG-ESP32.pak`, verifies immutable source/runtime identity, restores the exact player root and settled pose, and configures a fresh native gameplay session.

The first implementation exposed one missing semantic boundary: a restored settled `EspPlayerViewState` did not recreate the transient HUD semantic owners normally produced by the fresh-map spawn chain. The final hardware-tested fix restores those owners directly:

```text
EspHudRefresh_restorePending()
EspHudPostLoadClear_restoreSettled()
```

The bridge is deliberately owner-only: it does not replay tile-enter/facing/world mutations.

Successful hardware witness:

```text
[NATIVESAVE] REPRIME-HUD map=1 gameplayLoadMapId=1 angle=128
             refresh=pending clear=ready mutation=owners-only turn=no
[NATIVESAVE] LOAD ... pos=416,1696 angle=128 playerFNV=363261d1
                    runtimeFNV=c3882516 sourceBytes=21823 sourceCrc=623f34e4
                    world=fresh-rebuild session=reprime-pending
[ENGINESESSION] FIRST_FRAME map=1 angle=128 ...
[ENGINESESSION] HUD map=1 hp=30/30 armor=8/20 weapon=2 ammo=8 ...
[ENGINESESSION] SPRITES dependencyStatus=10 catalogSprites=46
[MAPFLASH] REUSE HIT requestedMap=1 ... rebuild=no
[MAPFLASH] ARM map=1 active=1 verified=1 reused=1 ... resident=1
[ENGINECACHE] PRIMED ...
[RESIDENTGAMEPLAY] READY ...
[ENGINESESSION] READY map=1 angle=128 residentCache=yes largeCache=yes
                shapeData=0x0 mediaTexels=0x0
```

Movement, turning, events and dialog execution were then exercised successfully after LOAD on the real board.

## Rollback proof

The user deliberately changed the player after the SAVE by progressing farther into Entrance. Before LOAD the live player had acquired Fire Ext, extra ammunition and a Small Medkit:

```text
playerFNV = a6e115a7
weapon = 1
weapons = 0006
ammo = 10/12/00/00/00/00
items = 01/00/00/00/00
```

LOAD restored the saved root:

```text
playerFNV = 363261d1
weapon = 2
weapons = 0004
ammo1 = 8
```

The post-load weapon-cycle witness confirmed Fire Ext was no longer owned:

```text
[WEAPONCONTROL] UNCHANGED ... weapon=2 weapons=0004
                reason=no-other-owned-usable mutation=no turn=no
```

This proves the persisted player checkpoint is not merely a visual respawn; post-SAVE player mutations are rolled back.

## Repeated-load / RAM witness

Multiple LOAD cycles completed to `ENGINESESSION READY`. Two consecutive cycles stabilized at:

```text
heap8 = 18356
largest8 = 11764
```

No monotonic per-LOAD loss was observed. After additional gameplay initialized more lazy owners, a later pre-load state was already lower; the following LOAD retained that lower boundary rather than consuming another fixed block. Continue monitoring RAM, but the hardware evidence does not show a systematic LOAD leak.

## Deliberate v1 limitation

`world=fresh-rebuild` is literal. V1 restores the player and pose but rebuilds mutable map-local overlays from immutable BSP data. It does **not** yet persist, among other families:

- consumed resource/pickup sprite bits;
- mutable map/script event state;
- door/open line state and line texture variants;
- automap reveal state;
- monster state/positions/activation;
- destructibles;
- gameplay RNG state.

Therefore an item consumed **before** SAVE can currently reappear physically after LOAD even though the saved player benefit remains. That is the next save-v2 frontier, not a v1 regression.

## Next bounded milestone

Do not jump to a monolithic world dump. The preferred next slice is the already-native **player-resource consumed bitset**. Entrance owns 344 map sprites, so the current resource owner uses only 43 B for consumed bits. Add explicit snapshot/restore APIs and a versioned save section, then prove on hardware that:

1. a pickup consumed before SAVE remains absent after LOAD;
2. a pickup consumed only after SAVE reappears after LOAD;
3. the saved player root still restores exactly;
4. unrelated world families remain explicitly fresh/fail-closed;
5. `shapeData == NULL` and `mediaTexels == NULL` remain invariant.

Only after that PASS should more mutable-world families be added one bounded owner at a time.
