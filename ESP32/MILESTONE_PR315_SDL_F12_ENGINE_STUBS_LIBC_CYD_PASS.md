# SDL-F12 — propriétaires root/layout sans alias mémoire SDL — PASS CYD

**Date :** 11 novembre 2026  
**PR :** [#315](https://github.com/Kharn27/DoomRPG-RE/pull/315)  
**Issue :** [SDL #269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f12-engine-stubs-libc`  
**Commit firmware candidat flashé :** `8228cd503b693e01813c89d2fc408966e46ac574`  
**Base GitHub exacte :** `b46049fc15ccafd817abc5235e745dd07920acf0` (fusion Root-J6 #314)

## Périmètre de production

Dans **`ESP32/src/engine_stubs.c` uniquement**, 3 appels `SDL_malloc`, 6 appels `SDL_free` et 7 appels `SDL_memset` deviennent leurs appels libc directs `malloc/free/memset`. Ajout explicite de `<string.h>`. Les macros de `ESP32/include/SDL.h` étaient des alias directs des mêmes fonctions, sans heap SDL.

**Inchangés** : tailles/arguments et ordre des allocations, responsables de libération, réentrance/layout, gestion OOM/fatal, propriétaire de l'identité processus Root-J6, garde `doomrpg == NULL || doomrpg != doomRpg`, destructeurs menu→render→canvas, framebuffer partagé 160×120 RGB565, fichiers PAK/SD, géométrie/rendu, monde, état gameplay et rollback. Les types et fonctions ABI `SDL_Rect`, `SDLVideo_t`, `sdlVideo`, `SDL.h`, `SDL_Video.h`, `SDL_CloseAudio`, `SDL_Close` restent volontairement dans la frontière.

## Oracles et CI

- `ESP32/test/sdl_f12_engine_stubs_libc_host.py` reconstruit **octet par octet la totalité** de `engine_stubs.c` antérieur à F12 à partir des 16 substitutions (et de l'include) : **blob Git `47783298dfcc4824585ef6fbd733ade6a1d5402b`** sur la base Root-J6, vérifié aussi par GitHub `create_blob`.
- Les oracles historiques Root-J5 et Root-J6 inversent d'abord F12 pour garder les SHA historiques ; constructeur/teardown Canvas et Render continuent de compiler/exécuter le C réel, dont **4096 cycles**, OOM et ordre de libération. Les FNV d'origine ne sont pas affaiblis. SDL-F10/F11, Root-J2/J3/J4 et les autres gardes sont maintenus.
- CI du commit matériel : [push #43076366303](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43076366303) et [PR #43076378039](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43076378039) **SUCCESS**, trois builds PlatformIO, compiledb, comparateurs d'ELF. Normal **859321 B flash / 80736 B RAM statique**, bringup **882053/80880**, trace **877445/80776**. Aucune économie dynamique annoncée ni optimisation de présentation.

## Log Serial matériel normal `esp32-cyd`

**Source :** `device-monitor-261111-192453.log`, **1783 lignes, 205807 octets**, première ligne ALIVE de la session précédente, puis seul `POWERON_RESET`. Le SHA n'est pas affiché par le firmware ; le PASS est attribué au commit flashé d'après le contexte utilisateur, sans confirmation binaire indépendante.

- **Matériel/stockage :** ESP32-D0WD-V3 2 cœurs 240MHz, 4MiB flash, **PSRAM=0** ; SD 240MiB montée, `DoomRPG-ESP32.pak` indexé 241 entrées. CORE/PRERENDER/LAYOUT/RENDER/MAPPINGS/MENU READY ; `Render=108 Game=0 Canvas=36`, PAK `mappings.bin=8392 B`, payload **8376 B**, unique framebuffer RGB565 **38400 B**, affichage ×2 320×240.
- **UI pré-gameplay :** OPTIONS→VIDEO→THEME 0→1 écrit en config v3, BACK ; HELP 112 lignes, pagination descendante jusqu'à 56 puis remontée et BACK. START→intro avec texte/touch 50ms, `INTRODISP READY` récupérant **34080 B** ; Entrance `/intro.bsp`, CRC32 `623f34e4`, arena immutable **14095 B**, plusieurs scènes/dialogues/portes/loot.
- **Gameplay :** **5 `DIALOG OPEN` / 5 `CLOSE`**, dont 1 standalone, **4 `DOORANIM COMPLETE`**, **14 `PICKUPMINI START` / 14 `END`**. HUB→EXIT sans SAVE ; LOAD V11 `slot04` **5604 B** (restauration script/ressources/lignes/automap/monstres/topologie/position/activation/drops), reprise visible ; **3 `MONSTERRETAL COMMIT` et 3 `MALWRATHPROJECTILE CLEAR`** (triple tir 204). Second EXIT sans SAVE et menu retrouvé.
- **Mémoire / propreté :** 2×`HEADROOM_OK` avec margins heap8 **28268 et 27456 B** ; DMA **69228 et 68416 B** ; 2×`RESIDENTRESET` chacun **released=18008 empty=1**, 2×`SYSEXIT MENU-READY` `frame=e687f0c7`, 13×`shapeData=0x0 mediaTexels=0x0` ; aucune trace panic/Guru/WDT/abort/Backtrace. 11 diagnostics `NATIVEFRAME BOUNDARY-INLINE` préexistants non fatals, aucune preuve d'un défaut nouvelle migration.

**Interprétation :** **PASS matériel ciblé SDL-F12 + Root-J6 combinés**, supérieur à la simple CI du rebase. Le chemin `DoomRPG_Error` / OOM n'a pas été provoqué sur carte et reste prouvé par tests C hôte seulement ; ne pas revendiquer tous les niveaux, tous les événements ou un profil de performance A/B. Aucune PSRAM, aucune lecture ZIP runtime, aucun buffer map-wide ni doublon du framebuffer.

## Gel et suite

**Après ce test, seul ce rapport, `ESP32/PORTING_STATUS.md` et `ESP32/DOCUMENTATION.md` sont autorisés à changer sur F12.** La CI du SHA de documentation doit rester verte avant fusion, et `main` réel doit être relu juste après merge. La suite SDL doit commencer par l'inventaire du code compilé restant (types de compatibilité vidéo, stubs ABI et éventuels alias mémoire dans d'autres TUs), puis prendre une frontière F13 autonome, pas supprimer tous les headers/stubs en bloc. Le portage audio Bluetooth et les autres sujets sont indépendants de SDL-F12.
