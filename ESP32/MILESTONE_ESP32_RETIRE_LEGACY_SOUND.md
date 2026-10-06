# ESP32 legacy Sound retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-combat`

Parent hardware boundary:
`766d1e0cf2280d784f2c073f552e3f57889dc541`

Hardware-tested code boundary:
`fffe6f6d780ae1d7444c49cb08df747fe5f4ca0c`

## Goal

Retire the inherited desktop `Sound_t` allocation while preserving the current
project policy that real audio playback is deferred.

The permanent ESP32-side semantic owner is `EspNativeAudioIntent`: code may
publish sound/music intents in gameplay order without requiring a resident
desktop mixer object.

## Ownership change

At the final code boundary:

- `DoomRPG_initEngineCore()` no longer calls `Sound_init()`;
- `doomRpg->sound` must remain `NULL`;
- `Sound_init/free` are no longer linked;
- `DoomRPG_free()` no longer owns `Sound_free()`;
- `Game_loadConfig()` still reads the legacy volume integer to preserve file
  layout, but discards it because playback is deferred;
- `DoomCanvas_castState()` no longer dereferences `soundEnabled`;
- `DoomCanvas_run()` no longer resets `nextplay`.

Compatibility `Sound_playSound`, `Sound_stopSounds` and
`Sound_freeSounds` remain NULL-safe no-op ABI shims for legacy functions that
are still linked. They do not own memory or playback state.

## CI

Normal `esp32-cyd` CI #1578: SUCCESS.

```text
RAM:   13.9% (45472 / 327680 B)
Flash: 59.7% (782837 / 1310720 B)
artifact id: 11302676326
artifact sha256: 502243e5bdd8abc9c9b2764428939098440619e44e5f7b3871442925028501e9
```

No local PlatformIO build is claimed.

## Real-CYD acceptance

The hardware boot explicitly shows:

```text
[CORE] Sound retired object=NULL owner=native-audio-intent playback=deferred
[CORE] READY objects=7 heap used=47460 remaining=139760 largest=73716 clip=160x120
```

The config/mappings stage remains healthy with no config file:

```text
[CONFIG] -> Game_loadConfig()
loadConfig: (unable to open file)
[CONFIG] DONE heap delta=0 ...
[CONFIGMAP] READY ...
```

The user then successfully traverses the entire START transition:

```text
[MAINSTART] READY native new-game -> Player_reset -> ST_INTRO
...
[INTRODISP] READY ...
...
[NATIVEBOOT] READY map=1 ...
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Resident gameplay reaches a stable witness:

```text
heap=123640 heap8=57716 largest8=51188
```

The previous Combat-retirement witness was:

```text
heap=123436 heap8=57512 largest8=51188
```

The +204 B difference is consistent with retiring the old ~220 B Sound object,
but is treated as supporting evidence rather than exact allocator attribution.

## Invariants

```text
doomRpg->sound == NULL
doomRpg->combat == NULL
doomRpg->entityDef == NULL
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
audio playback = deferred
audio semantics = bounded EspNativeAudioIntent
no runtime ZIP fallback
```

## Closure

Hardware-tested code boundary:
`fffe6f6d780ae1d7444c49cb08df747fe5f4ca0c`.

Post-test closure is documentation-only.
