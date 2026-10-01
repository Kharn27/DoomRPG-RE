# ESP32 V22 — retire legacy ParticleSystem startup

Date: 2026-10-01

## Boundary

```text
base active branch head = 16594a86c26afa8b81ca91ebe2092b892ce80adf
code = 28cc43cff7d0bee49731ff2c3382939914c75e41
branch = agent/esp32-retire-legacy-particle-startup-v22
current main at branch lineage = 72e351f8d26f4c3ac22d766a086de6646bbaf77b
```

This milestone retires one desktop-derived production startup path without
changing native gameplay behavior.

## Audit result

Before V22, the ESP32 core graph allocated `ParticleSystem_t`, and prerender
startup called `ParticleSystem_startup()`. That startup initializes 64 legacy
particle nodes and loads the gibs image pair even though the native resident
gameplay/render path does not call the legacy particle renderer.

The V21/review-fix ELF retained the particle startup closure. After V22, direct
ELF inspection shows:

```text
present:
  ParticleSystem_init
  ParticleSystem_free

absent:
  ParticleSystem_startup
  ParticleSystem_unlinkParticle
  ParticleSystem_render
  ParticleSystem_spawnParticles
  ParticleSystem_calculateParticles
```

The remaining init/free pair exists only because the core object is still
allocated. V22 deliberately does not remove that object yet.

## Code change

Only `ESP32/src/esp_legacy_prerender_startup.c` changes:

- remove `ParticleSystem_startup()`;
- remove `gibs_24.bmp` from required prerender assets;
- stop requiring `doomRpg->particleSystem != NULL` for prerender readiness;
- retain MenuSystem and EntityDef startup unchanged;
- keep a boot witness that ParticleSystem startup is retired.

No gameplay, input, RNG, map, renderer, event, rollback or save semantics change.

## CI

CI #1233: SUCCESS.

```text
static RAM       44952 B
linked Flash    757505 B
firmware.bin    757872 B
artifact id     11176725567
artifact digest sha256:4adda6b89004b7482d052a151e2664a2fdc2cddb51fb38d8d6cd0e0adc1dac83
firmware sha256 2169260cb3288d31aff2bbb66ccaefe6f1bf6b779391b2c7b3374fa2d0516352
ELF sha256      203c3afcfdfe4c50361153b09043c70e6e23520fe1ed0a4a1569155ee9051b0d
active wraps    49
```

Relative to the preceding review-fix image:

```text
static RAM   45064 -> 44952 B   (-112 B)
linked Flash 758389 -> 757505 B (-884 B)
```

## Real-CYD validation

Cold boot confirms the production prerender source is now exactly:

```text
[PRERENDER] Resource preflight (4 files)
p.bmp
q.bmp
j.bmp
entities.db
...
[PRERENDER] ParticleSystem startup retired
```

There is no gibs preflight/load and no ParticleSystem startup stage.

The native MAIN framebuffer remains functional and ALIVE is stable for roughly
100 seconds at:

```text
heap=120472 heap8=54548 largest8=32756
```

The preceding V21 menu witness was
`heap=101644 heap8=35720 largest8=23540`; V22 therefore returns 18828 B of
free heap8 at the menu boundary and increases the largest 8-bit block by 9216 B.

START then validates:

- native new-game transition;
- complete intro animation/input/disposal;
- native Entrance BSP/runtime build;
- exact first frame FNV `71ca7465`;
- `shapeData == NULL`, `mediaTexels == NULL`;
- resident gameplay service;
- MOVE/TURN;
- armor/medkit/ammo/weapon pickups;
- regular door open/close and move-event COMMIT;
- opcode-26 move dialog and opcode-19 continuation;
- repeated renderer compact-guard recovery.

Before the first lazy dialog owner, gameplay ALIVE is stable at
`heap=112284 heap8=46360 largest8=36852`. After the 1020-byte dialog-chain
owner it is stable at `111248/45324/36852`.

## Result / next boundary

V22 is REAL-CYD PASS.

The boot still proves:

```text
[CORE] ParticleSystem used=2280
```

The next bounded retirement is therefore to leave the legacy
`DoomRPG_t::particleSystem` pointer NULL on ESP32 and remove its core
constructor/stage/metrics ownership, while preserving all other object graph
members and runtime behavior.
