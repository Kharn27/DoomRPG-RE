# SDL-F27 — 10 unités C gameplay sans SDL.h direct natif : PASS CYD

## Contrat de production et origine

- [PR #335](https://github.com/Kharn27/DoomRPG-RE/pull/335), [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269). Branche `agent/esp32-sdl-f27-native-gameplay-headers`.
- Main exact au départ de F27 `0bd4d69e43f23e5d1279f9bba3a555fb6b05dfca` ; **firmware F27 testé sur classic CYD normal** correspond à la candidate `5644d7b06623bdb4a4b7fe53d4fd5e9a9ffcc857` de la PR. Le log n'imprime pas lui-même de SHA de firmware embarqué ; attribution par le parcours de test F27 déclaré par l'utilisateur.
- Lors de la validation, main a avancé par [PR #332 HELP](https://github.com/Kharn27/DoomRPG-RE/pull/332), maintenant `17e46f3f2f8bdfe40207c37560d8ba7ff73ac135`. Son code UI n'a modifié **aucune des dix TUs F27**. Commit code F27 rejoué sur ce main : `671853fc0b5ddc24e3c2734c964229e13eda83c7`, avec **blobs Git strictement identiques** aux dix TUs du firmware F27 testé. Le workflow initial plus les ajouts HELP sont combinés ; le script F18 historique / oracle F27 sont strictement identiques au candidat F27 validé par CI. L'intégration est compilée et auditée en CI après rebase, mais les nouvelles modifications HELP n'ont pas été testées dans le cadre matériel F27 seul.
- Production : dans dix TUs, remplacer seulement `#include <SDL.h>` par `#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE) ... #endif` ; corps C exact, allocation, RNG, bitmap, audio intent, world, V11, PAK, UI inchangés **par F27**.
- Les dix C : `ESP32/src/esp_native_gameplay_action_engine.c`, `ESP32/src/esp_native_gameplay_frame.c`, `ESP32/src/esp_native_gameplay_gib_fx.c`, `ESP32/src/esp_native_gameplay_hazard_touch.c`, `ESP32/src/esp_native_gameplay_pickup.c`, `ESP32/src/esp_native_gameplay_player_death.c`, `ESP32/src/esp_native_gameplay_player_resources.c`, `ESP32/src/esp_native_gameplay_session.c`, `ESP32/src/esp_native_gameplay_weapon.c`, `ESP32/src/esp_native_resident_gameplay.c`.
- Oracle `ESP32/test/sdl_f27_native_gameplay_header_host.py` vérifie les dix Git blob SHAs historiques, le corps sans types/symboles SDL, C11 des vrais `DoomRPG.h`/`Render.h` en natif/desktop/geometry, `Random_t==132`, `EspNativeRect` native ; compiledb 3 profils, ancien shim/BMP non compilés.
- Correction de l'ancien oracle F18 `ESP32/test/sdl_f18_native_alloc_libc_host.py` : reconstituer le préfixe historique initial pour `weapon` et `resident_gameplay` avant vérification SHA F18, sans toucher aux sources réelles.

## Preuve CI pré-rebase

- [Push #43335084464](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43335084464) : **SUCCESS 162/162**.
- [PR #43335093544](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43335093544) : **SUCCESS 162/162**.
- 3 builds ESP32 `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`, F18 historique PASS, 10 sources Git exactes PASS, C11 native/desktop/geometry PASS, compiledb 10 unités et absence de `esp32_sdl.cpp` + `esp32_bmp.cpp`, tous les ELF **zéro fonction SDL liée**. Build candidat avant HELP #332 : RAM statique/flash `esp32-cyd=80736/859693B`, `bringup=80880/882477B`, `trace=80776/877845B` ; ne pas reporter ces tailles comme celles de la nouvelle intégration HELP.
- Contrôle CI post-rebase+docs **requis**, aucun merge basé seulement sur les CI d'avant HELP.

## Log hardware exact

- `platformio-device-monitor-261113-230223.log`, **283290 octets, 2290 lignes**, SHA256 `ce8e93a702b9721a7d9fec3df3df42ddb9b05ba06e26d7c00cfbdbf8b75636d8`.
- Un boot `rst:0x1 (POWERON_RESET)`, ESP32-D0WD-V3 240 MHz, flash 4194304 B, PSRAM 0. SD montée, PAK 241 entrées, framebuffer 160×120 RGB565 38400B et nearest x2.
- HELP scroll page-down, aperçu de grille 12 armes puis BACK exact `4c01eb2b`, START, intro complète avant disposal, `/intro.bsp` Entrance MAP arena14095B, 2 sessions gameplay.
- Première session : interactions, caisses transformées en loot, 7 ouvertures/fermetures de dialogue **sur tout log** (6 event avec opcodes 8/26 et 1 dialogue standalone), 8 portes animées sur tout log, 22 mini-pickups START et 22 END, ressources/pickups et gameplay sans régression.
- EXIT1 depuis HUB : `[RESIDENTRESET] released=18008 empty=1`, `[SYSEXIT] MENU-READY frame=4c01eb2b ... saveWrite=no`.
- LOAD V11 slot04 : **5604B**, `[NATIVESAVE] LOAD ... version=11`, ressources, scripts, line overlays, action removals, crate transforms, automap, monster state, topology, positions, activation, monsterDrops **restored exact**, nouveau session ready avec `shapeData=0x0 mediaTexels=0x0`.
- Combat : **45× `[MONSTERACTIVESEQ] COMPLETE`**, **6× `[MONSTERRETAL] COMMIT`**, **6× `[MONSTERCOMBAT] COMMIT`**, 3 morts sprite322 (Malwrath **HP11→8→4→0**), sprite279 (HP13→8→0), sprite272 (HP5→0). Projectiles Malwrath séquencés, pain flash, coordonnées et RNG journales conservées.
- **Vrai mouvement observable de la famille spéciale subtype4** : 3 chaînes `MONSTER3GOAL ARM`, **5 `MONSTER3GOAL COMMIT`** (continuités 2/3 et 3/3), `topologyRelink=committed`, `renderer=snap-destination`, rollback RNG avant chaque commit, un `MONSTER3GOAL ATTACK-PROBE`. Aucun `MONSTERMOVE COMMIT` générique : cela ne prouve pas le déplacement de tous les autres sous-types.
- Deux `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`, marges heap8 **28092B** et **27280B**, largest8 gameplay **61428B**. Deux `[RESIDENTRESET] ... released=18008 ... empty=1`, deux `[SYSEXIT] MENU-READY frame=4c01eb2b`, 32× ALIVE, dernier uptime **167978ms**, heap8 **130560B** largest8 **86004B**. Aucun panic, WDT, reboot spontané, défaut majeur de SD/PAK, ou invariant shape/media violé.
- Limites: un `MONSTERDROP COMMIT def=99` au tile joueur puis `RENDER-CULL reason=player-tile` : visuel pas prouvé. Indications `NATIVEFRAME BOUNDARY-INLINE` préexistantes. Aucun nouveau SAVE V11, OPTIONS/audio (backend silent) ni rare loot defTile100 démontré.

## Intégration et clôture

- Après **PASS matériel original**, la production F27 garde exactement les mêmes 10 blobs, réappliqués à la base HELP plus récente. Commit documentaire après replay contient **uniquement** `ESP32/PORTING_STATUS.md`, `ESP32/DOCUMENTATION.md` et ce milestone. Ce nouveau main a sa propre validation de la PR #332.
- Attendre CI push + PR finale (merge virtuel avec HELP #332), merge explicitement autorisé par l'utilisateur, vérifier SHA exact de main ensuite. Pas de changement matériel post-PASS imputé à F27.
- Audit direct initial : avant F27 **23 imports SDL.h non gardés parmi les 173 TU C/C++ ESP32/src** (**hors** ancien shim SDL non compilé) ; dix enlevés, restent **13** dont groupe rendu/intro et menu. Ce comptage ne couvre pas les headers transitifs/desktop ; ne pas déclarer SDL totalement éliminé tant que ces chemins ne sont pas audités.
