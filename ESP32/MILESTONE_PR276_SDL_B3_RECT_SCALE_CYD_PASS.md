# SDL-B3 — Retrait des entrées SDL rectangle-contour et integer-scale : PASS CYD normal

**Date : 9 novembre 2026.** [Issue SDL #269](https://github.com/Kharn27/DoomRPG-RE/issues/269), [PR #276](https://github.com/Kharn27/DoomRPG-RE/pull/276), **base exacte `main=99179777b227c3738f3bc6f72359f81e621ec511`** (SDL-B2 fusionné). **Code transmis pour flash : `d7151e59fa6a02792aceb9a10aa17f89e60efde7`**. Le log Serial n'inclut pas d'empreinte firmware, donc le lien avec ce SHA est celui de la procédure de flash utilisateur, non une attestation cryptographique embarquée.

## Frontière technique

Suppression uniquement de deux entrées dormantes ESP32 : `SDL_RenderDrawRect` (contour calculé à partir de quatre appels à la primitive ligne) et `SDL_RenderSetIntegerScale` (no-op retournant 0), avec leurs prototypes dans `ESP32/include/SDL.h`. Les primitives réellement utilisées `SDL_RenderDrawLine`, `SDL_RenderFillRect`, `SDL_RenderCopy`, `SDL_RenderSetClipRect`, `SDL_RenderPresent`, gestion des textures indexées 1/4/8 bpp, palettes/colorkey et `PlatformVideo` nearest ×2 restent inchangées. Rien dans le PAK, BMP runtime, gameplay natif, allocations persistantes, scripts ou oracle desktop.

Le garde fail-closed `ESP32/test/sdl_dead_render_helpers_host.py` vérifie que **350** fichiers sources/headers natifs n'appellent ni ne déclarent les deux symboles, et que les primitives actives, l'ownership indexed texture et l'appel `PlatformVideo_present()` sont conservés. L'audit compilé `sdl_shim_build_ownership_host.py` a aussi été étendu : les deux fonctions doivent être **absentes de l'ELF candidat et du baseline historique SDL-A**, dans chacun des trois profils, sans variation des ensembles de symboles réellement liés.

## Gate logicielle

**Push CI [#42542096950](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42542096950) SUCCESS et PR CI [#42542138823](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42542138823) SUCCESS.** Guard SDL-B3 350 fichiers PASS, SDL-A réel C++/tests hôte/guards B1-B2 PASS, `esp32-cyd` normal compilé et audité. Auxiliaires bringup et trace compilés et audités en CI seulement ; aucun test hardware exigé. **19 fonctions SDL liées**, **1548 octets de corps symboliques** contre 1547 octets dans **l'ancien** baseline SDL-A, delta RAM statique suivi **0**. Delta sections chargées `-332 B` sur `esp32-cyd` **vs ce baseline historique SDL-A** : non attribuable à SDL-B3 isolément ; aucune économie flash/RAM ni accélération FPS revendiquée, fonctions déjà non liées.

## Gate matérielle : `device-monitor-261109-020247.log`

**1 432 lignes / 174 227 octets** (journal commence à `[ALIVE] uptime=15003ms`, **pas de boot initial capturé**). Cycle `esp32-cyd` normal sur classic CYD sans PSRAM, observé après mise sous tension.

- MENU → OPTIONS → VIDEO → BACK → MENU ; HELP natif 77 lignes, boutons PAGE-DOWN (0→7→14→21) et PAGE-UP (21→14→7→0), retour natif menu ; assets `[SDL] Adopt packed indexed texture` visibles sur HELP et intro, propriétaire PAK préservé.
- START → intro narrative native → `FINAL-CONTINUE` → libération complète de l'intro `recovered=34068` → carte `/intro.bsp` Entrance (CRC `623f34e4`, arena `14095 B`) ; mouvements/rotations/strafe, destruction de caisse, armures/medkit/munitions, arme `Fire Ext` avec aide autonome, porte 275 (ouverture/fermeture animées), dialogues `EV_DIALOG=8` events 88 et 60, `EV_DIALOGNOBACK=26` event79, `EV_NEXTSTATE=19` en reprise ; event60 `show=1`, petit snapshot de topologie `2408 B`.
- HUB et EXIT n°1 sans sauvegarde automatique → menu ; LOAD V11 slot04 **5604 B**, ressources + script/lignes/caisses/automap/monstres/topologie/positions/activation restaurés, reprise carte, changement d'arme, porte 113.
- Malwrath `sprite=322`, `subtype=6` : **trois** `MALWRATHPROJECTILE CLEAR` (probes 1, 2, 3), **trois** `MONSTERRETAL COMMIT` jusqu'à `HP 34→28→22→5`, dégâts visuels/pain sprite 184 et bordure rouge ; **quatre** tirs joueur `MONSTERCOMBAT COMMIT` `hp 11→8→6→4→0`, mort avec corps/unlink et `xp=6`. EXIT n°2 et menu stable.
- **2× `[RAMBUDGET] ... advisory=HEADROOM_OK`**, marges `heap8=52040` et `51228` au-dessus du target de `57344`. Bloc libre maximal pendant jeu `largest8=86004` ; **2× `[RESIDENTRESET] ... released=18008 ... empty=1`** ; **2× `[SYSEXIT] MENU-READY`** FNV `1945fafa` ; dernier `[ALIVE]` `uptime=118166ms heap8=154508 largest8=110580`. `shapeData=0x0` / `mediaTexels=0x0` sur les points de contrôle. Pas de panic/WDT/abort/corruption/erreur d'allocation observé.
- **Sept** `[NATIVEFRAME] BOUNDARY-INLINE` (deux `logical=15`, cinq `logical=49`) ; diagnostics existants et sans échec de session associé dans ce log. **Leurs causes et la parité pixel-perfect générale ne sont pas établies par SDL-B3.** Pas de test de mort du joueur ou de sauvegarde écrite pendant cette capture.

## Verdict / publication

**PASS matériel normal SDL-B3**, avec les limites ci-dessus. Suite au log utilisateur et à son autorisation explicite, documentation du jalon après test : ce rapport + `PORTING_STATUS.md` + `DOCUMENTATION.md` ; **les commits qui suivent le SHA matériel sont docs-only**. La fusion attend la vérification CI post-documentation et l'état `main` actuel. Le prochain jalon doit repartir du SHA GitHub de `main` après fusion, pas d'un ancien souvenir.