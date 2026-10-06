# Milestone — retire desktop Entity / EntityMonster translation units

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`1442bda7f7f19d578afac81d151edfe2fc85e58a`

Branch:
`fix/mainMenu`

## Boundary

The normal ESP32 build excludes:

- `src/Entity.c`
- `src/EntityMonster.c`

The inherited `Game_t` allocation remains for now, but its embedded entity and
monster arrays are deliberately dormant and are no longer initialized as a
desktop runtime graph. The native resident-map and gameplay owners remain the
only live entity/monster runtime.

Generated `Game.c` also retires the old `Entity_reset()` unload loop.
`Game_activate()` is preserved only as a fail-closed ABI symbol for residual
desktop linkage; it performs no activation. Production activation is native.

## Failed first build and fix

The first code boundary, `c332ed12c7078b6a555ab1ebe876e302acc12ce4`,
failed at link time:

```text
undefined reference to 'EntityMonster_getSoundID'
... Game_activate
```

This proved `Game_activate()` was the remaining linker edge into
`EntityMonster.c`. The final code boundary
`1442bda7f7f19d578afac81d151edfe2fc85e58a` replaces that inherited path with
a no-op and does not restore either retired translation unit.

## Build witness

```text
RAM:   45464 bytes
Flash: 781517 bytes
```

## Hardware witness

```text
[CORE] Legacy entity runtime retired arrays=dormant entities=0 monsters=0 owner=native-resident-map
[CORE] READY objects=5 heap used=46188 remaining=141040 largest=73716 clip=160x120
...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[MONSTERSTATE] READY ... noLegacyEntity=yes ...
[MONSTERCOMBAT] READY ... legacyEntity=no
[MONSTERACT] READY ... source=bsp-render-visible persistence=map-session ...
[ALIVE] ... heap=129164 heap8=63240 largest8=51188 ...
```

This establishes that the classic CYD reaches resident gameplay with desktop
Entity/EntityMonster behavior removed from the build.

## Next boundary

`Game_t` is now the obvious remaining heavy desktop owner: its 36468-byte
allocation still includes dormant `Entity_t[400]`, `entityDb[1024]` and
`EntityMonster_t[100]` storage. A future milestone should replace only the
remaining live Game responsibilities (notably config/map-name compatibility and
any cleanup ABI still referenced) before shrinking or retiring `Game_t`
itself.
