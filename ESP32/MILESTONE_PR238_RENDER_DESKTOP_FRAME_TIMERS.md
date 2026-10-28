# PR #238 — Retrait des trois timers raster desktop du shell Render_t — PASS classic CYD

Date des logs matériels : 28 octobre 2026.

## Identité et frontière

- Dépôt : `Kharn27/DoomRPG-RE` ; PR #238 : https://github.com/Kharn27/DoomRPG-RE/pull/238 ; branche `agent/esp32-render-retire-desktop-frame-timers`.
- Base exacte du vrai main après merge #237 : `0883682baa32ae218a6369c2e43b5689060d10dc`.
- **Code matériel attribué à la campagne de flash : `5fc43a920dcd9c9f7124d9f199d70bc980efb03d`**, firmware normal `esp32-cyd`. Pas de fingerprint SHA embarqué dans Serial.
- CI code : https://github.com/Kharn27/DoomRPG-RE/actions/runs/40777521640 (push) et https://github.com/Kharn27/DoomRPG-RE/actions/runs/40777530334 (PR) ; **SUCCESS, 75 étapes chacune**, tests hôte, firmware normal, bringup et trace.
- Deux commits de code avant test : retrait conditionnel des trois timestamps puis correction d'un commentaire qui faisait matcher à tort `Render_render` dans un garde hôte de l'ABI desktop. La correction est validée par les deux CI ci-dessus.
- Modifications code : `src/Render.h`, `ESP32/src/render_startup_bridge.c` (assertion 1376), `ESP32/test/native_render_raster_abi_retirement_host.py` (analyse déclarations et absence d'accès natif aux champs).
- `currentFrameTime`, `horizonFrameTime`, `frameTime` proviennent des phases de `Render_render()` desktop, non compilé dans la cible native. Conservation de `animFrameTime`, trigonométrie, culling, scratch et compteurs live. Aucun changement du framebuffer, palette, plans, BSP/cache leaf immutable 2048B, gameplay, RNG, sauvegarde, audio, assets ou `PlatformVideo_present()`.

## Matériel : boot réel

Classic ESP32-2432S028R ESP32-D0WD-V3 240 MHz, 4 MB flash, **PSRAM=0**, ILI9341 320×240 depuis framebuffer logique 160×120 RGB565 **38400 B**, microSD et PAK natif indexé **241 entrées**.

```text
Engine structs: Render=1376 Game=0 Canvas=44 Total=2180 bytes
[RENDERCORE] INIT owner=esp-native-bridge bytes=1376
[MAINBOOT] READY ... frame=0f2c9fc3
```

Mesure contre #237 : `Render_t 1388→1376` (−12 B). Le **Total 2192→2180** est la ligne agrégée du boot, pas juste la somme de Render/Game/Canvas.

## Chemin réel LOAD slot04 → EXIT

- Slot04 `/DoomRPG-ESP32-slot04.sav` V11 **5604 B**, lecture `READABLE-SPATIAL valid` et restauration `exact=yes`.
- `/intro.bsp` 21823 B CRC `623f34e4`, map arena native 14095 B FNV `c3882516`, 223 nodes / 480 lines / 344 sprites / 93 events / 265 byteCodes / 94 strings.
- 30 monsters `beaeaccc`, topology `50e16fe3`, positions `e2a9d90a`, activation `df419482`, `legacyReplay=0/0/0 exact=yes`.
- Cache current map flash : `MAPFLASH REUSE HIT`, `rebuild=no` et `backing=raw-partition` (aucun ZIP runtime).
- `NEXT_WEAPON` sans tour ; premier FORWARD tile281→282, Health Vial ; porte113 en 4 frames ; Malwrath322 active au rendu.
- Deux ripostes Malwrath : chacune **3 tirs** de projectile204, avec `MALWRATHPROJECTILE CLEAR` puis `MONSTERATKVIS COMPLETE` **avant** `MONSTERRETAL COMMIT`. Dégâts appliqués ensuite, pas de mutation prospective avant finalisation. HP joueur 34→28→15, armure 23→17→4 (RNG de la campagne, non comparé à #237).
- Deux tirs Pistol sur Malwrath : HP monstre 11→7→0, mort `death4→corpse2`, drop `Sm Medkit`, ramassage ; porte fermée HOLD pendant monster-turn puis RELEASE/COMPLETE, sprite324 `copies=4 draws=4→2→0`.
- Flak Jacket, secret event15, portes112/111, +5 XP, HUB system EXIT confirmé sans sauvegarde ; `RESIDENTRESET ... released=18008 empty=1`, menu `0f2c9fc3`.

## Chemin START après EXIT

- Fresh `PlayerState_resetFresh` FNV `e745fce9` ; intro t=0 `ade0195d`, reveal/more/skip/final continue natifs ; `INTRODISP recovered=34060` avec aucune mutation de map avant la relève.
- BSP initialisé à la même arène `c3882516`, `ENGINESESSION FIRST_FRAME frame=71ca7465 walls=8 pixels=4430`, puis cache normal.
- Premier MOVE tile904→872 ; caisse127 `TRANSFORM` vers Armor Shard, pickup ; dialogue88 opcode8 suivi `DIALOGCHAIN RESUME` état 1 opcode19 ; déplacement, autres Armor Shards ; porte275 quatre frames puis HOLD/RELEASE à la fermeture.
- HUB system EXIT double sélection, `DIALOGCHAIN OWNER-RELEASE recovered=1144`, `RESIDENTRESET ... released=18008 empty=1`, menu `0f2c9fc3`, `saveWrite=no`. Le `shapeData` et `mediaTexels` sont NULL aux frontières.

## Performance, mémoire et comparabilité

| Même checkpoint slot04, probes actifs, µs | #237 | #238 | Écart |
| --- | ---: | ---: | ---: |
| LARGE-WARM spriteUs | 37736 | 37775 | +39 |
| LARGE-WARM totalUs | 84599 | 84756 | +157 |
| Premier VIEWANIM MOVE après NEXT_WEAPON | 117793 | 117796 | +3 |

#238 `SMALL-WARM 41418/102678 µs`, #238 START premier MOVE `99869 µs`, en regard du #237 START `99269 µs`. La cadence n'accuse aucune régression reproductible dans ces traces. Le cache BSP leaf reste opérationnel ; #238 LARGE-WARM `leafUs=364`, `orderUs=17250`, `postScanUs=361 µs`. Les sondes `SPRITEORDER/SPRITEPHASE/SPRITEFLASH` sont actives sur les 16 premiers échantillons, puis silencieuses : les retests #237 sans sondes à ~90ms de MOVE constituent **un autre régime d'instrumentation**, pas un témoin interchangeable. Ne pas affirmer une parité pixel frame-by-frame entre sessions ou une cause prouvée pour les écarts microscopiques.

| Frontière heap8/largest8 | #237 | #238 |
| --- | ---: | ---: |
| Gameplay LOAD | 112012/86004 | 112020/86004 |
| Menu après EXIT | 157948/110580 | 157956/110580 |
| Gameplay START avec dialogue journal | 110868/86004 | 110876/86004 |

**+8 B heap8 réel** aux trois frontières, malgré −12 B du `Render_t`. `RAMBUDGET HEADROOM_OK`, deuxième EXIT propre, pas de fuite apparente sur ce parcours.

## Observations non résolues et clôture

- `NATIVEFRAME BOUNDARY-INLINE` (transition 20480/32768), positions x des projectiles de Malwrath loggées avec un offset `0x40000000`, et `HUB CLOSE ... exactHud=NO` ont été observés précédemment ; hors frontière et **pas déclarés corrigés ni innocents**.
- Pas de SAVE neuf, toutes cartes, endurance multi-heures, fingerprint SHA binaire, parité pixel stricte, ou évaluation visuelle indépendante des logs. PASS limité au parcours matériel observé.
- **Décision : PASS ciblé réel, code gelé à `5fc43a920dcd9c9f7124d9f199d70bc980efb03d`**. Tout commit ultérieur de la PR doit être **docs-only** ; vérifier diff et CI après doc, puis Ready-for-review. Le merge reste réservé à l'utilisateur.
- Pour éviter de découper/coller de longs logs pendant le gameplay, capturer le Serial par `pio device monitor --port /dev/ttyUSB0 --baud 115200 --filter default --filter log2file`, jouer toute la séquence, joindre un unique `.log` ensuite. Voir `ESP32/DOCUMENTATION.md` et la doc PlatformIO officielle.
