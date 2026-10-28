# PR #237 — Retrait des compteurs mémoire desktop de Render_t, PASS classic CYD

Date du témoin matériel : 28 octobre 2026.

## Identité, frontière et qualification

- Dépôt : `Kharn27/DoomRPG-RE` ; [PR #237](https://github.com/Kharn27/DoomRPG-RE/pull/237), branche `agent/esp32-render-shell-ownership-audit`.
- Base exacte du vrai `main` après merge #236 : `b90e9dc8b8cc169ba8440ac80d795ad7f0489f31`.
- **Code matériel testé : `ba1f713c620afc5a79b667da1b290c948125525e`** ; attribution par la campagne de flash de l'utilisateur, sans SHA imprimé par le firmware.
- CI pré-CYD du code : [push #40619473318](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40619473318) et [PR #40619478493](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40619478493), SUCCESS **75 étapes chacune**, host et firmwares normal `esp32-cyd`, bringup et trace. Le bringup ne sert pas de témoin RAM.
- Modification unique : dix compteurs d'anciens loaders desktop dans `Render_t` deviennent `#ifndef DOOMRPG_ESP32` : `mapMemory`, `stringMem`, `eventMem`, `memory`, `bitShapeMemory`, `lineMem`, `spriteMem`, `nodeMem`, `texelMemory`, `mappingMemory`. `paletteMemory` est **conservé** et écrit dans `render_startup_bridge.c`. Garde hôte native et assertion `sizeof(Render_t)==1388`.
- Diff du code testé : `src/Render.h`, `ESP32/src/render_startup_bridge.c`, `ESP32/test/native_render_raster_abi_retirement_host.py`. Cache leaf BSP immutable BSS 2048 B (#236), PAK, map overlays, rendu, framebuffer, présentations vidéo, sauvegardes, RNG, gameplay et scratch natif inchangés.

## Matériel et boot

Classic CYD ESP32-2432S028R, ESP32-D0WD-V3 240 MHz, 4 MB flash, **0 PSRAM**, normal `esp32-cyd` ; display 320×240 depuis framebuffer logique 160×120×RGB565 (38400 B). Boot :
```text
Engine structs: Render=1388 Game=0 Canvas=44 Total=2192 bytes
[RENDERCORE] INIT owner=esp-native-bridge bytes=1388
[DATA] Native PAK indexed, entries=241
[MAINBOOT] READY ... frame=0f2c9fc3
```
`Render_t` baisse de 1428 à 1388 octets. `paletteMemory` n'est pas sacrifié. Les frontières observées conservent `shapeData=0x0`, `mediaTexels=0x0`.

## Parcours de validation sur le vrai CYD

**Premier passage :** boot → LOAD slot04 → Malwrath → loot/pickups, portes 113/112/111, secret+5XP, HUB → EXIT → START → intro → Entrance (première frame) → déplacements/dialogues/porte275/pickups → EXIT.

**Retests complémentaires :** après retour au menu, deux nouveaux LOAD successifs slot04 → NEXT_WEAPON → premier FORWARD → porte113 → combat Malwrath / projectiles → pickup → secret → HUB → EXIT ; **aucun reboot ou reflash entre les retests**, même firmware normal et même sauvegarde V11 (5604 B). Les différentes salves de monstres peuvent varier par RNG ; ce sont les invariants et la cadence qui sont comparés.

Observations :
- Carte `/intro.bsp` : CRC `623f34e4`, arène compacte 14095 B `c3882516`, sauvegarde 30 monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, restauration exacte `legacyReplay=0/0/0 exact=yes`.
- Native flash : `MAPFLASH REUSE HIT`, `rebuild=no` à chaque LOAD ; aucune dépendance runtime ZIP.
- Porte113 quatre frames, deferred close `HOLD → RELEASE → POST-MONSTER-COMPLETE` ; sprite324 `copies=4`, `draws=4→2→0`, collision/clipping à proximité conservés. Secret lignes112/111 +5 XP.
- Malwrath322 : combats natifs, `MONSTERATKVIS` à 3 projectiles204 par riposte, `MALWRATHPROJECTILE CLEAR` avant chaque `MONSTERRETAL COMMIT` ; mort `death4→corpse2`, drop et pickup. Le deuxième retest comporte trois attaques monstres ; un `COMBAT-INPUT-BLOCK` pendant une animation est conforme à la sérialisation.
- START après EXIT : player fresh `e745fce9`, intro première frame `ade0195d`, `INTRODISP recovered=34060`, `ENGINESESSION FIRST_FRAME=71ca7465` (8 murs/4430 pixels), caisse127 transformée, Armor Shards, dialogue88 opcode8→19, dialogue79 opcode26→19, porte275 ouverte/refermée après monster-turn.
- EXIT à chaque session : `[RESIDENTRESET] released=18008 empty=1`, `[SYSEXIT] MENU-READY frame=0f2c9fc3`, `saveWrite=no` et invariants shell NULL.

## Mesures performance : conserver le statut de l'instrumentation

| Scène LOAD slot04 / mesure | Premier passage avec probes | Retest LOAD #1, probes silencieux | Retest LOAD #2, probes silencieux |
| --- | ---: | ---: | ---: |
| LARGE-WARM spriteUs | 37 736 µs | 19 814 µs | 19 920 µs |
| LARGE-WARM totalUs | 84 599 µs | 56 644 µs | 56 538 µs |
| Premier VIEWANIM MOVE, après NEXT_WEAPON | 117 793 µs | 90 042 µs | 89 920 µs |
| Heap8 gameplay | 112 012 B | 112 012 B | 112 012 B |
| Largest8 gameplay | 86 004 B | 86 004 B | 86 004 B |

Dans la session initiale, `[SPRITEORDER]` et `[SPRITEPHASE]` donnent `leafUs=398 µs`, `orderUs=17252 µs` à LARGE-WARM, contre témoin #236 instrumenté `~460/17469 µs`. Cache immutable 2048 B préservé. Le LARGE-WARM #236 **avec probes** : `spriteUs=37950 µs`, `totalUs=85205 µs` ; les valeurs initiales #237 sont similaires, sans régression observée de cette phase.

Le premier MOVE du premier passage (117793 µs) a déclenché un contrôle. Deux retests donnent 90042/89920 µs, soit écart **122 µs (~0,14 %)**, près du témoin #236 `91560 µs`. Les probes de phase, limités à 16 samples, n'impriment plus de lignes durant les deux retests, et leur coût est une explication **plausible mais non isolée** de la différence, y compris les ~18 ms de phase sprites. Ne pas considérer 19,8 ms comme un gain algorithmique prouvé sur #236, et ne pas comparer des campagnes avec instrumentation différente comme un benchmark A/B contrôlé. Les FNV LARGE-WARM des retests sont `6f93d442` puis `92d59b31` : dépendance au contexte d'animation, **pas de parité stricte pixel prouvée entre exécutions**. Le ralentissement initial du premier MOVE n'est **pas reproductible** dans les deux retests, et ne bloque plus #237.

## RAM, limites et décision

Heap8 gameplay LOAD `112012/largest86004`, contre #236 `111972/86004` ; menu post-EXIT `157948/largest110580` contre #236 `157908/110580`. **+40 B libres** aux deux frontières, cohérents avec le shell retiré. `RAMBUDGET HEADROOM_OK`, aucune fuite visible après les LOAD/EXIT. START frais avec journal dialogue a `heap8=110868/largest86004`, puis retourne au même menu `157948`.

Préexistants et hors périmètre : `NATIVEFRAME BOUNDARY-INLINE` et log de position x des projectiles `+0x40000000`, non corrigés ni déclarés inoffensifs. Pas de fingerprint binaire, SAVE neuf, all-maps, stress long, preuve pixel par pixel ni appréciation visuelle explicite issue de ces seuls logs.

**Décision : PASS fonctionnel, mémoire et cadence ciblés sur vrai CYD** ; **code gelé au SHA `ba1f713c620afc5a79b667da1b290c948125525e`**, clôture **docs-only**. Après contrôle du diff post-test et CI post-doc verte, PR Ready-for-review ; le merge reste exclusivement effectué par l'utilisateur.
