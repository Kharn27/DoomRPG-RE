# SDL-F21 — ABI Z_Zone native détachée de SDL.h : PASS classic CYD

## Portée et provenance
- PR : https://github.com/Kharn27/DoomRPG-RE/pull/328, suivi https://github.com/Kharn27/DoomRPG-RE/issues/269
- Base exacte : `fcc79365fa278af013183031396181a46eede940` sur `main`.
- Tête **code/test** soumise sur CYD : `c39dd5fc9ec338deb16c945f8e5f0ab6cc9c8eee`. Le code C/H n'a pas changé depuis `abac0c91f1ae43295e69b8ec81bc9ebe7c4da8fe`.
- `engine_stubs.c` : garde SDL.h uniquement pour hôte/desktop/oracle geometry ; les définitions réelles des quatre fonctions `Z_{Malloc,Calloc,Realloc,Free}` utilisent `DOOMRPG_ZONE_CALL`.
- `src/Z_Zone.h` : `#include <stddef.h>` explicite ; `DOOMRPG_ZONE_CALL` vide en ESP32, alias `SDLCALL` en desktop. Corps des fonctions, appels libc, nombres d'allocations, tailles de propriétaires, RNG, PAK, framebuffer, sauvegardes et comportements inchangés.
- Nouveau test `ESP32/test/sdl_f21_zone_call_boundary_host.py` et inversions F12/F16/F19 : deux blobs Git antérieurs exacts (dont header CRLF) + compilation/exécution réelle des quatre wrappers old/new native/desktop. Oracles historiques conservés.

## CI du candidat matériel
- Push https://github.com/Kharn27/DoomRPG-RE/actions/runs/43251295021 — SUCCESS, 156/156.
- PR https://github.com/Kharn27/DoomRPG-RE/actions/runs/43251308717 — SUCCESS, 156/156.
- Trois builds PlatformIO, compiledb, ELF et tests hôte passent. Flash/RAM statique strictement équivalents à SDL-F20 : normal `859757/80736` B ; bringup `882541/80880` B ; trace `877909/80776` B. Ce ne sont pas des mesures heap dynamique.

## Vérité matérielle : Serial normal esp32-cyd
- Fichier : `platformio-device-monitor-261112-180431.log`, 1940 lignes / 240652 octets ; SHA256 `d9cbac1253a64f6e37f04f06020fe9dc53ec0855cc17db35d25879adf8e221a5`.
- Démarrage ROM complet, `rst:0x1 (POWERON_RESET)` unique, ESP32-D0WD-V3 240 MHz, flash 4 Mo, PSRAM=0, PAK=241 et écran 160×120×2 nearest-neighbor. Pas de panic ni reset secondaire.
- HELP avec défilement et BACK, OPTIONS/BACK, START et intro complète, map `/intro.bsp` Entrance, navigation, portes, caisse transformée et prises d'objets, dialogues natifs 8 et 26 avec reprise 19, HUB/EXIT, LOAD V11 slot04 5604 B depuis menu, reprise et deuxième HUB/EXIT.
- Comptes exacts du log : `[DIALOG] OPEN` 6 / `CLOSE` 6 ; `DOORANIM COMPLETE` 7 ; `PICKUPMINI START` 16 / `END` 16 ; `MONSTERCOMBAT COMMIT` 7 ; `MONSTERRETAL COMMIT` 5 ; `MONSTERDROP COMMIT` 2 (def=92, un cullé sur tile joueur).
- `[RAMBUDGET] RESERVE ... HEADROOM_OK` 2 fois, marges heap8 28092 B et 27280 B, `largest8=61428 B` ; `[RESIDENTRESET] released=18008 empty=1` 2 fois ; `[SYSEXIT] MENU-READY frame=4c01eb2b` 2 fois ; `[ENGINESESSION] READY` 2 fois, `shapeData=0x0 mediaTexels=0x0` dans les frontières observées.
- Absence de watchdog, OOM, Guru, `fail=SPRITES`, `SETTLE-FAILED`, `DEATH-REDRAW-FAILED`.
- Limites : `NATIVEFRAME BOUNDARY-INLINE` 9 fois et coordonnées projectiles avec bit haut persistantes (antérieurs au milestone SDL), mais pas d'échec visible ; ni NOTE, ni `CATALOG-PRIME` / defTile=100, ni SAVE neuf, ni audio matériel évalués.

## Décision
PASS ciblé sur vrai CYD confirmé après inspection du log. Aucun changement C/C++/H après le test. Seule documentation Markdown ajoutée, vérifier la CI finale avant fusion autorisée par l'utilisateur. Le jalon **ne signifie pas** retrait total des ABI historiques SDL.
