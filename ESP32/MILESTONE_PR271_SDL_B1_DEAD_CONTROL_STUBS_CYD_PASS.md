# SDL-B1 — Retrait de huit stubs SDL fenêtre/manettes : PASS CYD normal

**Date : 9 novembre 2026.** Suivi [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269), [PR #271](https://github.com/Kharn27/DoomRPG-RE/pull/271), base exacte **`main=c9dc7a289bd932492f9cfcf1d54241168c7e0849`** (SDL-A fusionné). **Firmware au SHA `a77a31af57a401cdc7fa7005e21d82c0ee73ad26` réellement testé sur le classic CYD**, profil normal `esp32-cyd`, sans PSRAM.

## Périmètre et justification

Suppression **uniquement** des définitions dans `ESP32/src/esp32_sdl.cpp` et déclarations ESP32 dans `ESP32/include/SDL.h` de huit API de compatibilité jamais liées dans les ELF : `SDL_SetHint`, `SDL_ShowCursor`, `SDL_SetWindowFullscreen`, `SDL_NumJoysticks`, `SDL_IsGameController`, `SDL_GameControllerRumble`, `SDL_HapticRumblePlay`, `SDL_HapticRumbleStop`. Pas de modification des 19 symboles SDL réellement liés, des pixels indexés 1/4/8 bpp, du format RGB565, du PAK, des overlays, du traitement vidéo, du gameplay, de l'audio intent, des sauvegardes ou du desktop/oracle.

Le nouveau garde `ESP32/test/sdl_dead_control_stubs_host.py` vérifie l'absence de ces huit appels/définitions dans **350** sources et headers ESP32, tout en vérifiant la présence des API image/texture/blit/present et le framebuffer natif. Le vrai test hôte compilé de SDL-A reste actif.

## Gate logicielle

[GitHub Actions #42535863933](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42535863933) : **SUCCESS**. Test `SDL-B PASS retired 8 never-linked control/window/haptic stubs, no users across 350 native C/C++/headers`; test réel du shim 1/4/8 bpp et de ses propriétaires PASS ; firmware **`esp32-cyd` SUCCESS**, plus builds auxiliaires en CI uniquement (pas de flash physique de bringup ni trace). Contrôles compiledb et ELF : **19 symboles SDL liés / 1 547 octets de corps symboliques**, identiques à la baseline SDL-A ; aucune régression de compilation. Ne pas attribuer à SDL-B seul les deltas des sections de la CI héritée, dont la baseline prédatant l'intégration de #268. Les huit stubs n'étaient déjà pas liés : **aucune économie de RAM/flash ni gain FPS revendiqués**.

## Gate matérielle — log utilisateur complet

**`device-monitor-261109-010645.log` : 1 787 lignes, 215 533 octets**, profil `esp32-cyd` normal. Boot ESP32-D0WD-V3 (240 MHz), **4 Mo de flash, PSRAM 0**, structures `Render=120 Game=0 Canvas=44 Total=924 B`, framebuffer partagé `160×120 RGB565 = 38 400 B`, présentation `320×240` nearest x2, PAK 241 entrées. Plusieurs `[SDL] Adopt packed indexed texture` 4 bpp sur écran menu/HELP et intro ; absence de `shapeData/mediaTexels` dans les chargements et après reset (`0x0` à chaque témoin).

Séquence réellement observée :
1. Menu→HELP (77 lignes, défilements et retour), puis START, intro native, carte Entrance, mouvements/rotations/strafe, caisses→armure, porte 275 avec fermeture différée, dialogues 8/26 et reprise d'event opcode 19, pickups, HUD ; HUB/SYSTEM→EXIT confirmé, retour au menu.
2. LOAD V11 **slot04 Entrance** : restauration exacte des états resources/script/lines/topology/position/activation et monstres ; porte113 quatre frames ; Malwrath sprite322, deux coups joueur `hp=11→7→3`, ripostes avec **4 séquences de trois projectiles** et résolution différée ; 4 drains `[MALWRATHPROJECTILE] CLEAR`. Dernier coup mortel du Malwrath : **mort du joueur**, `[PLAYERDEATH] READY`, menu de mort puis `[DEATHMENU] DISPATCH action=RETRY` → session neuve Entrance. Ce scénario **teste aussi la voie de mort et RETRY**, pas une troisième sortie menu.
3. Jeu après RETRY : armes, mouvements, HUD/HUB puis EXIT confirmé, retour au menu exact.

**Invariants stricts :** `HEADROOM_OK` **3×** (marges heap8 51 356, 50 540, 51 356 B par session), `largest8=86 004` en jeu, `RESIDENTRESET empty=1` **3×**, chaque `released=18 008 B`, `SYSEXIT MENU-READY` **2×**, menu FNV `e687f0c7` pour chaque EXIT, heap8 menu post-EXIT **153 824 B**, `shapeData=mediaTexels=NULL`, aucun `panic` / `Guru Meditation` / WDT / FAIL / ERROR détecté. Les rares `[NATIVEFRAME] BOUNDARY-INLINE` sont des témoins diagnostics déjà connus, sans conclusion de correction. Pas d'essai de pixel-perfect global ni de comparaison de latence A/B.

## Verdict / suites

**PASS matériel SDL-B1**, pour le **code effectivement flashé à `a77a31af…`**. Accord explicite utilisateur : « Si c'est ok tu fusionnes et tu continues ;) ». Aucun changement fonctionnel après ce PASS ; seules les documentations et métadonnées de clôture sont ajoutées avant fusion. **Ne pas cocher SDL-B entier dans l'issue #269 : ce n'est que le premier sous-jalon B1.** Suite : créer une nouvelle branche `agent/*` depuis le vrai SHA de `main` après fusion ; petit retrait SDL-B2, ciblé et fail-closed, puis CI et gate matériel normal indépendante.
