# ESP32 V24 — retire legacy Menu root

Date: 2026-10-01

## Boundary

```text
branch = agent/esp32-retire-legacy-particle-startup-v22
base documented head = 374ab5586f6ad87eedf18ea1e22d0d461a645f2c
first code candidate = 675b4a554498014c666af55205d3355fcbb39ad1
hardware-tested correction = 21ee2c95afd351af5c20ba38d6ef897bd81d1d05
current main in lineage = 72e351f8d26f4c3ac22d766a086de6646bbaf77b
```

## Objective

Retire the inherited desktop `Menu_t` root without changing the still-live
`MenuSystem_t` owner.

Before V24, final-ELF audit showed that the only surviving `Menu_*` symbol was
`Menu_init`. The native pre-game flows already own MAIN, OPTIONS, HELP, Back,
START and LOAD semantics. The ESP32 runtime did not consume `Menu_t` data.

## First candidate and hardware fail

Commit `675b4a554498014c666af55205d3355fcbb39ad1`:

- removes the `Menu_t` core stage/allocation;
- removes `Menu_t` from core metrics;
- requires `doomRpg->menu == NULL`;
- removes one obsolete native-main-model object-graph guard.

The real CYD core graph passed and showed:

```text
[CORE] Menu root retired object=NULL owner=native-menu-models
[CORE] READY objects=10 heap used=53804 ...
```

but the boot then failed closed:

```text
[MAINOPAQUE] FAILED dashboard presentation contract menu=1 selected=0
[MAINBOOT] FAILED native MENU_MAIN model/presentation
```

This was a useful hardware failure. It proved another stale legacy precondition
still existed rather than indicating a live `Menu_t` consumer.

## Correction

The remaining dependency was in
`DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe()`, which required
`doomRpg->menu != NULL` even though its actual contract checks only:

- DoomRPG / Render / DoomCanvas / MenuSystem roots;
- framebuffer availability;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- inactive native wall/sprite caches.

Commit `21ee2c95afd351af5c20ba38d6ef897bd81d1d05` removes only the obsolete
`Menu_t` condition. No fallback, synthetic Menu object or legacy path is
reintroduced.

## CI / ELF

CI #1239: SUCCESS.

```text
static RAM       44936 B
linked Flash    757585 B
firmware.bin    757952 B
artifact id     11179380857
artifact digest sha256:c039482dacd4f6af3c491ba3793ea4c6153fdbffef4ea4086d2bde6ec60dd5e4
firmware sha256 35a9987bd83c03c0f6787c0dfb4cde4b2cf994745089c2fa1ec98e74ec3dd807
ELF sha256      a8f146550c1610fba2077318b41637a53fd6987088ff60ad8be70e09d48cbc1e
active wraps    49
```

Final ELF:

```text
Menu_* symbols = 0

retained MenuSystem_*:
  MenuSystem_init
  MenuSystem_startup
  MenuSystem_playSound
  MenuSystem_free
```

The local hardware workstation reports the known +16 B linked-image delta:
757601 B linked Flash / 757968 B firmware.bin.

## Real-CYD PASS

Cold boot after the correction reaches:

```text
[CORE] ParticleSystem retired object=NULL owner=native-gibfx
[CORE] Menu root retired object=NULL owner=native-menu-models
[CORE] READY objects=10 heap used=53804 remaining=133952 largest=73716
...
[MAINOPAQUE] READY finger-first MENU_MAIN painted without BSP/wall/sprite replay
[MAINBOOT] READY owner=native-opaque ...
```

Memory comparison:

```text
V23 core used       53880 B
V24 core used       53804 B
delta                 -76 B

V23 MAIN heap8      56852 B
V24 MAIN heap8      56972 B
delta                +120 B

V24 MAIN largest8   32756 B
```

The menu interaction run validates:

- MAIN selection and touch re-arm;
- OPTIONS model and all three disabled cards;
- OPTIONS native Back;
- HELP PAK parse with 83 items;
- repeated PAGE-DOWN and PAGE-UP;
- HELP native Back;
- exact MAIN framebuffer restoration to FNV `522dc605`;
- no scene rerender on ordinary main selection;
- no SD read on main card selection;
- `shapeData == NULL`, `mediaTexels == NULL`;
- stable `heap8=56972 largest8=32756` across menu interactions.

The exact V24 run does not re-enter START/gameplay; those paths were unchanged
by the V24 source delta and were hardware-proven at the immediately preceding
V23 boundary.

## Result

V24 is REAL-CYD PASS.

The legacy `Menu_t` root and all `Menu_*` code are absent from the
production ESP32 image. `MenuSystem_t` remains the next distinct ownership
question and must not be retired until its remaining bounded model/resource
consumers are replaced explicitly.
