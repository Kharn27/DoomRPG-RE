# SDL-F26 — mouvement monstre et header RNG sans SDL.h natif : PASS sur classic CYD

## Portée et provenance

- PR [#334](https://github.com/Kharn27/DoomRPG-RE/pull/334), suivi [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269). Branche `agent/esp32-sdl-f26-monster-movement-rng-header`, **base main vérifiée** `3a97726d5ebe57309b84f3c56c0c0ccf6a130f44`.
- **Firmware effectivement flashé et testé** : `f4561781dd7f58fd97a802a6a480b4554bdcf2cc` en `esp32-cyd`, sur ESP32-2432S028R classic CYD, 4 Mo de flash et **zéro PSRAM**.
- Six frontières `<SDL.h>` désormais uniquement host/desktop ou `DOOMRPG_ESP32_GEOMETRY_ORACLE` : `ESP32/src/esp_native_gameplay_monster_movement.c`, `ESP32/src/esp_native_gameplay_monster_movement_publish.c`, `ESP32/src/esp_native_gameplay_monster_three_goal_turn.c`, `ESP32/src/esp_native_gameplay_monster_turn.c`, `ESP32/src/esp_native_gameplay_monster_state.c`, `ESP32/include/esp_native_rng_replay_guard.h` (ancien include **transitif** non conditionné).
- Seule modification production : guard `#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)` autour de `#include <SDL.h>`. Cinq corps C rigoureusement identiques, toutes déclarations RNG inchangées ; ni moteur desktop réintroduit, ni changement de RNG, map, audio, HUD, gameplay, alloc, V11, PAK ou heap.
- Oracle `ESP32/test/sdl_f26_monster_movement_rng_header_host.py` reconstruit le **Git blob original exact** pour chacun : movement `593140ab6737e0034c3cd7cb4c0da289ef67e0e0` ; movement_publish `c5df65702973409928dab3a466d0e57182d358d2` ; three_goal_turn `29fc86f16578de50de7e5dd7a5d0233cee779d20` ; turn `ce81c39d4097154ffe6ce5f30182d59985a29840` ; state `5932839b2583157a20610328050eba45e5d7dd7a` ; header RNG `be4dbd482abdeef36abebad78b69d7c6caa9accc`.
- C11 des vrais headers `esp_native_rng_replay_guard.h`, `DoomRPG.h`, `Render.h` en ESP32 native sans SDL et host/geometry avec SDL. `compiledb` : cinq TUs + RNG replay TU, SDL/BMP historiques exclus. ELF : zéro fonction SDL liée.

## Preuves de CI (firmware candidat)

- [CI push #43331590701](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43331590701) : **SUCCESS 161/161**.
- [CI PR #43331648772](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43331648772) : **SUCCESS 161/161**.
- 3 builds PlatformIO et oracles host, audit ABI, SDL-F3 compilations exactes. Aucune revendication de récupération RAM/flash dynamique due aux seuls guards. Ram test hardware : largest8 jeu 61428 B, HEADROOM_OK à deux reprises.

## Serial matériel, démonstration et limites

Log : `platformio-device-monitor-261113-005232.log` (**1799 lignes / 215667 octets**, SHA256 `f941ebe69151bcc7f9c30d480872aec58254cd85fb6a308930aabff24aaf355c`). Un seul `rst:0x1 POWERON_RESET`, ESP32-D0WD-V3 rev 3, 240 MHz, flash 4194304 octets, PSRAM 0 ; SD OK / PAK 241 entrées ; framebuffer 160×120 × 2 en nearest.

- HELP paginée puis BACK, START, séquence intro complète, `/intro.bsp` Entrance, gameplay et HUB. 5 × `[DIALOG] OPEN` et 5 × `[DIALOG] CLOSE`, dont dialog 8/26 et une boîte standalone ; un `[NOTE] APPEND`. **6 × DOORANIM COMPLETE**, **16 × PICKUPMINI START** et **16 × END** ; caisse transformée et caisse détruite.
- Sortie HUB→SYSTEM→EXIT **deux fois** : `[RESIDENTRESET] released=18008 empty=1` puis `[SYSEXIT] MENU-READY frame=4c01eb2b`, aucune sauvegarde à l'EXIT.
- LOAD V11 depuis slot04 de **5604 octets** : `resources=restored`, `script=restored`, `lines=restored`, `actionRemoved=restored`, `crateTransforms=restored`, `automap=restored`, `monsters=staged`, `topology=restored`, `positions=staged`, `activation=restored`, `monsterDrops=restored`. `[ENGINESESSION] READY` x2 `shapeData=0x0 mediaTexels=0x0`.
- Combat Malwrath sprite322 après LOAD : **11→7→3→0 HP**, 3× `[MONSTERTURN] MEMBER-ATTACK-PROBE`, 3× `[MONSTERRETAL] COMMIT` après animation 3 rafales/projectile, RNG rollback puis consommation committée, player hp34→29→23→15, mort monstre et corpse ; 33× `[MONSTERACTIVESEQ] COMPLETE` dont 3 delivered=1, autres 0, ordonnancement stable.
- **Aucun `MONSTERMOVE COMMIT` live** : `MONSTERMOVE READY ... liveMove=no`, `MONSTERMOVELIVE CHECKPOINT-PROJECTION projected=10`. Le log vérifie la compilation et la non-régression du parcours mais **pas de vraie translation monstre committée**.
- **2× `[RAMBUDGET] ... HEADROOM_OK`**, marge heap8 **28092 et 27280 B**, `largest8=61428` en gameplay ; 2× `RESIDENTRESET ... released=18008 empty=1`. **25 × ALIVE**, jusqu'à uptime **128697ms**, heap8 130560, largest8 86004. Aucun panic, WDT, reset parasite, ni `SPRITES fail`.
- `[MONSTERDROP] COMMIT ... def=92` puis `RENDER-CULL ... player-tile` ; l'apparence visuelle du drop **n'est pas démontrée** et le rare drop defTile100 non rencontré. Les `[NATIVEFRAME] BOUNDARY-INLINE` et grands champs pos de projectile Malwrath subsistent (observés avant F26), hors portée de cette extraction d'includes.
- OPTIONS, SAVE neuf et audio matériel non testés. L'essentiel de F26 est l'isolation d'includes, pas une nouvelle capacité de movement runtime.

## Clôture et résidu

- Après le hardware PASS `f4561781dd7f58fd97a802a6a480b4554bdcf2cc`, ne modifier **que** `ESP32/PORTING_STATUS.md`, `ESP32/DOCUMENTATION.md` et ce milestone Markdown avant CI finale. Ne pas modifier le binaire flashé.
- SDL shim et BMP historique exclus des 3 profils, zéro fonction SDL liée, mais imports SDL directs/transitifs subsistent dans d'autres fichiers. **Ne pas déclarer l'arbre de sources intégralement SDL-free.**
- Validation de fusion donnée explicitement par l'utilisateur après son test : merge après CI post-doc.

