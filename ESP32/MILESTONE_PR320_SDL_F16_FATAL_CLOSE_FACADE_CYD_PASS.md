# SDL-F16 — fermeture SDL no-op retirée du chemin fatal — PASS classic CYD

**Date :** 11 novembre 2026  
**PR :** [#320](https://github.com/Kharn27/DoomRPG-RE/pull/320)  
**Issue :** [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f16-retire-fatal-close-facade`  
**Base exacte `main` après fusion F15 #319 :** `e432ffa7ff09d8332a286896dcb4a138a62aa024`  
**Tête candidate testée sur CYD :** `0a19c57bb824aff644bd6c3c024d2d9662ae40d9`  
**Log matériel :** `device-monitor-261111-211704.log` (1471 lignes, 167370 octets)

## Frontière native (un seul TU de production)

`ESP32/src/engine_stubs.c` retire l'include `SDL_Video.h` et les derniers points d'entrée **vides** `SDL_CloseAudio` et `SDL_Close`, ainsi que leurs invocations dans `DoomRPG_Error`. L'original exécutait `DoomRPG_FreeAppData(doomRpg)`, deux no-op et `exit(0)` ; le nouveau effectue **la même libération** immédiatement suivie de `exit(0)`. Aucune ressource audio SDL ou vidéo matérielle n'était possédée par ces no-op. Préservation de la chaîne réelle menu→render→canvas, garde d'identité Root-J6, detach du framebuffer `PlatformVideo`, même formatage et logs de l'erreur fatale ; native audio intent reste `backend=silent`, sans I2S/BT. Les typedefs de compatibilité `SDL_Rect` dans `ESP32/include/SDL.h`, `src/SDL_Video.h` desktop et autres headers de référence ne sont pas supprimés. Aucun retour au ZIP runtime, aucun `shapeData` map-wide, aucun `mediaTexels`.

## Oracles + CI avant le test physique

- Reconstruction inverse de la **source entière** `engine_stubs.c` pré-F16, git blob **`d20b081200a75226d88a7b5279142eab178fd508`** confirmé séparément par GitHub. Chaîne historique F16→F15→F14→F13→F12 reconstruite sans changer les baselines.
- Test `ESP32/test/sdl_f16_fatal_close_facade_host.py` : compile **les vrais corps C avant/après `DoomRPG_Error`**, 5 entrées formatées (normale, format %, texte long tronqué, entier et texte borné), `cc -std=c11 -O2 -Wall -Wextra -Werror`, compare sorties octet pour octet, vérifie `DoomRPG_FreeAppData` une fois avant `exit(0)`, jamais de retour normal.
- `Root-J6` reste 4096 cas C compilés pour pointeurs et ordre de destruction ; **359 sources/headers** natives inventoriées hors `esp32_sdl.cpp` non compilé ; trois ELF sans `SDL_Close` ni `SDL_CloseAudio` ; test F12 corrigé pour exiger `SDL_Video.h` dans son **état historique reconstruit**, non dans le source actuel.
- CI tête `0a19c57bb824aff644bd6c3c024d2d9662ae40d9` : [push #43092970321](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43092970321) et [PR #43092980150](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43092980150) **SUCCESS / 150 étapes chacune**. Normal `esp32-cyd` flash **859317B**, RAM statique **80736B** ; bringup **882049/80880B**, trace **877441/80776B**. **−4B** flash par profil vs F15, RAM identique ; aucune revendication de gain heap dynamique/performance.

## Validation matérielle `esp32-cyd`

- Trois lignes `ALIVE` de la session précédant l'unique `POWERON_RESET` (flash utilisateur), puis démarrage normal et prolongé ; chip ESP32-D0WD-V3 rev3 dual-core 240MHz, flash 4194304 B, **PSRAM=0** ; microSD **240MiB**, `DoomRPG-ESP32.pak` indexé **241** entrées ; framebuffer logique **160×120 RGB565 = 38400B**, présentation x2.
- MENU_MAIN→Options→Video→Back→Options→BACK : retour opaque modèle **FNV=1945fafa**, touch réarmé. HELP `112` lignes, **six PAGE-DOWN** (0→7→14→21→28→35→42), puis BACK vers MENU_MAIN exact **FNV=1945fafa**.
- START→intro complète (7 TAP, pages/texte)→`INTRODISP READY recovered=34116B`→`BSPREAD /intro.bsp` map Entrance CRC32 **623f34e4**, arène runtime immutable **14095B**→`FRESH-VISIBLE` ; sans dépendance ZIP. Gameplay : actions MOVE/TURN/STRAFE, une caisse transformée et ramassée, dialogues scientifiques natifs script (`opcode=8`), dialogue déplacement `opcode=26`, une aide arme standalone : **4 `DIALOG OPEN` classiques + 1 `OPEN-STANDALONE`, 5 `CLOSE`**. **4 `DOORANIM COMPLETE`**, **12 miniatures `START` / 11 `END`** ; la dernière miniature démarre juste avant HUB→EXIT, le teardown arrête la session volontairement, donc pas de fin attendue dans cette trace. `NOTE APPEND` non observé.
- Premier HUB→SYSTEM→EXIT confirmé **sans écriture de sauvegarde**, `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=1945fafa`. MAIN→LOAD **slot04 V11 5604B**, `resources/script/lines/crates/action removals/automap/monsters/topology/position/activation/drops` restaurés, `RESUME-VISIBLE`. Sur checkpoint : Malwrath sprite 322 attaque, **trois tirs / un `MALWRATHPROJECTILE CLEAR`**, **un `MONSTERRETAL COMMIT`** (joueur 34→28 HP, armure 23→17), puis **un `MONSTERCOMBAT COMMIT`** (ennemi 11→0 HP), gore natif et loot ramassé. Deuxième HUB→SYSTEM→EXIT confirmé ; **deuxième retour MENU-READY** même FNV.
- **Deux `HEADROOM_OK`** : heap8 margins **28468B et 27656B**, DMA margins **69428B et 68616B** ; **deux `RESIDENTRESET released=18008 empty=1`**, **deux `SYSEXIT MENU-READY frame=1945fafa`**, 13 messages avec `shapeData=0x0 mediaTexels=0x0` ; 0 panics/Guru/WDT/watchdog/abort/ERROR/FAILED. Quatre messages `NATIVEFRAME BOUNDARY-INLINE` préexistants (pas de panne) ; aucun `DoomRPG_Error` volontaire, OOM ni test de toutes les maps. Pas de SHA firmware imprimé en Serial : l'attribution résulte du flash de la branche F16 par le testeur.
- **Verdict dans frontière F16 : PASS matériel.** Post-test : modifier uniquement ce rapport + `PORTING_STATUS.md` + `DOCUMENTATION.md`, faire CI finale **push + PR** avant merge. Aucune ligne C/CPP, header, script, tests ou workflow ne doit changer après le firmware flashé.

## Suite après fusion

Lire le **main GitHub réel et son SHA**, puis `PORTING_STATUS.md`, `DOCUMENTATION.md` et ce rapport. Auditer avant de modifier `ESP32/include/SDL.h` et les types/macros actifs (p.ex. `SDL_Rect`, `SDL_Texture`, `SDL_Color`), l'absence des unités SDL dans compiledb / ELF, et leur propriété ESP32-native. Créer un jalon F17 seulement avec une frontière sûre et des consommateurs prouvés, ne pas supprimer le header/ABI desktop `src/SDL_Video.h` de référence. Construire et tester le firmware normal sur le vrai CYD avant documentation et fusion. SDL peut être presque terminée, mais ne pas déclarer `SDL-free` sans preuve d'inventaire source et binaire.
