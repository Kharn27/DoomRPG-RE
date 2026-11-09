# SDL-B6 — six façades utilitaires SDL dormantes : PASS classic CYD

**Date du test : 10 novembre 2026** · [PR #283](https://github.com/Kharn27/DoomRPG-RE/pull/283) · [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269)

## Code validé et frontière

- `main` de départ de cette branche : `91cdacae0ffd1856b57dc0b91d097f90ef71e01f`, après fusion B5 #281.
- **SHA exact du code B6 testé sur matériel selon la procédure utilisateur : `240877d291615657c7d8e344ad514bce3fbd7ab1`.** Le rapport Serial ne contient pas de SHA firmware embarqué ; cette attribution dépend du flash indiqué par l'utilisateur.
- Six seules façades ESP32 supprimées de `ESP32/src/esp32_sdl.cpp` et `ESP32/include/SDL.h` : `SDL_SwapLE16`, `SDL_SwapLE32`, `SDL_RWwrite`, `SDL_LoadBMP` (wrapper de chemin), `SDL_SetRenderDrawBlendMode`, `SDL_RenderClear`. Rien dans les fonctions SDL réellement liées. Conserver `SDL_LoadBMP_RW`, `SDL_RWFromMem/File`, `SDL_RWread/seek/close`, textures indexées 1/4/8 bpp, palette/color-key/modulation, `SDL_CreateTexture` RGB565 et `SDL_UpdateTexture` (exercés par le test permanent), `SDL_RenderCopy/Present`, framebuffer et `SDL_ShowMessageBox` pour `DoomRPG_Error`.
- Garde `ESP32/test/sdl_dead_utility_facades_host.py` : **352** sources/headers natifs inspectés ; commentaires C/C++ exclus des faux appels, les véritables déclarations/appels bannis, assertions de survie des chemins actifs. Le fichier propriétaire `esp_doomrpg_image_resources.c` est inchangé octet pour octet : son ancien `//loadedSurface = SDL_LoadBMP(fileName);` n'était pas un appel vivant ; `doomrpg_image_resources_host.py` garantit l'owner exact.
- Audit ELF : six noms absents dans les trois profils candidats ET baseline SDL-A historique, **19 fonctions `SDL_*` liées dans chaque profil candidat**, **1586 B symboliques candidat**, contre **1547 B baseline historique**. Les deltas contre un ancien main incluent des fonctionnalités natives concurrentes, et **ne constituent pas un coût ni un gain isolé de B6**.
- [CI push #42717122248](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42717122248) et [CI PR #42717130955](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42717130955) : **SUCCESS** au code `240877d2`. Builds `esp32-cyd` normal et profils auxiliaires, plus tests hôte. Les deux premières tentatives CI B6 ont échoué sur un commentaire / owner exact, corrigées **avant** ce test matériel.

## Preuve Serial : `device-monitor-261110-003400.log`

**2802 lignes / 342014 octets.** Boot ROM `POWERON_RESET` complet, ESP32-D0WD-V3 dual core 240 MHz, flash 4194304 B, **PSRAM=0**. PAK 241 entrées et framebuffer logique partagé `160×120 RGB565=38400 B`, sortie nearest-neighbor `320×240`. `Render_t=112 B`, `Game=0 B`, `DoomCanvas=40 B`. Invariants `shapeData=0x0` / `mediaTexels=0x0` répétés aux checkpoints.

**Menu et prologue** : Options est ouvert puis Back natif (retour FNV `3721ecf7`), HELP natif 112 lignes avec huit PAGE-DOWN jusqu'à scroll 56, retour natif, START avec intro texte/animation complète, disposal récupère **34056 B**, transition loading et map Entrance `/intro.bsp` CRC `623f34e4`, arena immutable `14095 B`. **Ni VIDEO, ni page HELP jusqu'à scroll105** ne sont visités durant ce rapport.

**Gameplay session 1** : nombreux mouvements/strafe/rotations, portes normales et secrètes, secret découvert (+5 xp), dialogue événements 79 et 60 (opcodes 8 et 26, resume 19, show/hide/unlock), armes, armor, health, ammo, `PICKUPMINI` `24×24` non bloquant. Combats natifs : sprites 220 et 264 tués, effets hit/gib et drop natif observés, dégâts et changement HP validés. EXIT via SYSTEM : `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=3721ecf7`.

**Gameplay session 2** : LOAD via UI depuis slot **04 V11 (5604 B)**, reprise Entrance. Malwrath sprite 322 : **trois ripostes à trois tirs**, HP joueur **34→27→23→17**, puis mort du Malwrath avec gib ; deuxième série de combats contre sprites 279 et 272, drops et pickup associés. Deuxième EXIT correctement rendu au menu, deuxième release **18008 B** `empty=1`.

**Comptages exacts de ce log** : 163 `RESIDENTGAMEPLAY QUEUE`, 75 `RESIDENTGAMEPLAY MOVE`, 8 `DIALOG OPEN` et 8 `DIALOG CLOSE`, **22 `PICKUPMINI START` / 22 END**, 10 `MONSTERRETAL COMMIT`, 7 `MONSTERCOMBAT COMMIT`, 4 `MONSTERDROP COMMIT`, 3 `GIBFX PAINT`. Deux `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK` avec **margin8=28376/27564 B**, marginDMA=69336/68524 B, `largest8=61428 B` durant gameplay, **deux** `RESIDENTRESET ... empty=1` et **deux** `SYSEXIT ... MENU-READY`. Pas de panic, Guru Meditation, watchdog, abort ni erreur fatale.

**Diagnostics suivis hors B6** : **10 occurrences** de `[NATIVEFRAME] BOUNDARY-INLINE` (logical15×7, logical49×3), déjà constatées avec d'autres jalons ; aucune preuve que B6 les crée, ni garantie de parité pixel-perfect. Ce rapport n'exerce pas l'erreur fatale `DoomRPG_Error` ni la séquence PLAYERDEATH/RETRY. Aucun nouveau benchmark A/B ni gain RAM/flash/FPS B6 ne peut être déduit de ce log.

## Conclusion

**PASS matériel ciblé pour SDL-B6 sur firmware normal `esp32-cyd`**. Conserver le SHA code matériel cité et rendre **strictement docs-only** les commits de clôture qui suivent. Des changements indépendants (PR #282 Root-D) sont entrés dans `main` pendant le test : avant de fusionner, intégrer ce vrai `main` et vérifier deux CI de combinaison ; ne pas prétendre que cette combinaison est elle-même physiquement testée. Après fusion, relire le SHA exact de `main` avant tout B7.
