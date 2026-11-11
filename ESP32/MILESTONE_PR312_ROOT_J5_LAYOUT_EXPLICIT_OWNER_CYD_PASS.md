# Root-J5 — Layout startup explicit process owner (PR #312) — CYD PASS

**Date :** 11 novembre 2026  
**Dépôt :** Kharn27/DoomRPG-RE  
**Branche :** `agent/esp32-root-j5-layout-explicit-owner`  
**Candidat flashé attribué :** `792c56f0d376010423aab1e3767efa7778926cc9`  
**Base pendant le test :** `d41194ff2f3768d06269971dc5debeab512fd98f`  
**Main vérifié après SDL-F10 #311 :** `64fa2c935c75052ce76fb9f4e0f9bd5fe380a3e5`  
**Premier commit d'intégration sur ce main :** `03c348af903d0f26c750eb632b67813cbf1276b6`

## Frontière permanente

`DoomRPG_startEngineLayout(DoomRPG_t* root, DoomRpgLayoutReport* report)` prend son propriétaire explicitement, refuse un root nul avant l'initialisation du Canvas, puis relaie exactement le pointeur vers `EspNativeEngineLayout_startupCanvas(canvas, root)`. `engine_metrics.h` expose une forward déclaration `struct DoomRPG_s`; `main.cpp` transmet le `doomRpg` de bootstrap. Suppression des **deux lectures globales** dans la fonction de LAYOUT, sans suppression de l'identité processus 1B ni de ses autres usages (teardown/fatal). Les corps des fonctions hors LAYOUT, la géométrie 160×120 / viewport 160×80, l'ordre Render_setup→startupMap→font a.bmp, les préconditions, l'idempotence, les buffers, le PAK et les mutations world sont inchangés.

## Vérifications avant hardware

- CI du candidat Root-J5 [run #43066936982](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43066936982) : SUCCESS. Tests host J2/J3/J4/J5, SDL-F9, builds PlatformIO normal/bringup/trace, audits de l'ELF.
- Test Root-J5 hôte : extraction et compilation C réelle avec `-Werror`, 4096 cycles, deux tokens roots, refus CORE/root/Canvas/Render et géométrie/colonne, cache single-attempt.
- Restauration inverse et empreintes Git exactes des blobs source `44b9f32c303b9beb5b1d48f0b3bc202ab34801c7`, header `98bf1ccd7577a38a1b0f6202fe297e04d7b26ccf` et `main.cpp` `1f510e2219aeefd1c38ac7be34627a3c7edd04a4`.
- Firmware normal : **859321 B flash / 80736 B RAM statique**, comparé au Root-J4 **859477 / 80768**. Pas d'économie runtime mesurée/affirmée.

## Test physique fourni

**Source** `device-monitor-261111-183021.log` : 1659 lignes / 192706 octets, un reset POWERON, ESP32-D0WD-V3 dual core 240MHz, flash 4 MB, **0 PSRAM**, SD 240 MiB, PAK 241 ressources.

Parcours observé : CORE (Render=108 B, Game=0 B, Canvas=36 B), LAYOUT, PRERENDER, RENDERSTART (FB 38400 B, palettes 3280), CONFIGMAP (mappings.bin), MENU ; OPTIONS/VIDEO, luminosité 100→25→100 et sauvegarde de config, retour natif ; HELP défilement, retour ; START/INTRO 50ms, `INTRODISP READY` avec **34080 B récupérés**, chargement `/intro.bsp` Entrance ; gameplay (déplacement/rotation, dialogs, portes, ramassages et miniatures) ; HUB→EXIT propre, LOAD V11 slot04 de **5604 B**, gameplay restauré avec trois ripostes Malwrath en séquences projectile et retour EXIT propre.

| Garde | Observation |
| --- | --- |
| Dialogues | 5 OPEN / 5 CLOSE (incluant standalone) |
| Portes | 4 DOORANIM COMPLETE |
| Miniatures PAK | 13 START / 13 END |
| Malwrath | 3 MONSTERRETAL COMMIT / 3 MALWRATHPROJECTILE CLEAR |
| RAM session fresh | HEADROOM_OK ; margin8 28268 B, marginDMA 69228 B |
| RAM session resume | HEADROOM_OK ; margin8 27456 B, marginDMA 68416 B |
| Teardown | 2 RESIDENTRESET released=18008 empty=1 |
| Menu après sortie | 2 SYSEXIT MENU-READY frame=3721ecf7 |
| Invariants map-wide | 13 mentions shapeData=0x0 mediaTexels=0x0 |
| Crashes | Aucun panic/Guru/WDT/abort recensé |
| Diagnostic connu | 8 NATIVEFRAME BOUNDARY-INLINE non fatals |

Les mentions `fail-closed` concernent les actions volontairement différées, non des erreurs. Ce test cible le chemin normal `esp32-cyd`, ne couvre pas les cas OOM ni tous les BSP ou assets. Le firmware n'imprime pas son SHA : la correspondance avec le candidat dépend du flash rapporté par l'utilisateur.

## Intégration après fusion parallèle SDL-F10

SDL-F10 [#311](https://github.com/Kharn27/DoomRPG-RE/pull/311) a été fusionnée à `main=64fa2c935c75052ce76fb9f4e0f9bd5fe380a3e5` après la préparation de Root-J5. Le commit d'intégration `03c348af903d0f26c750eb632b67813cbf1276b6` reconstruit la branche **depuis ce main exact** : les **9 blobs J5** (code/tests) sont copiés bit-à-bit depuis le candidat `792c56f...`, et le workflow ne fait qu'ajouter l'étape J5 au workflow SDL-F10 existant. Les changements après le test ne modifient aucun de ces 9 fichiers ; ensuite, **seulement ces trois fichiers de documentation** sont ajoutés/modifiés.

La CI intégrée finale doit être vérifiée sur le SHA documentaire final avant fusion. **Aucun second PASS physique combiné SDL-F10 + Root-J5 n'est revendiqué.** Conserver `shapeData == NULL`, `mediaTexels == NULL`, et le singleton identitaire résiduel tant qu'une vraie dépendance l'utilise.

## Suite

Après merge et relecture du SHA de `main`, auditer les dernières lectures actives du singleton root dans les TU natifs. Préférer la prochaine API de propriétaire explicite **bornée**, avec harnais C réel et gel de code après PASS, sans débrancher d'un coup les chemins fatal, SDL ou audio.
