# SDL-F30 — fermeture des dépendances GCC SDL + garde ZIP native : PASS classic CYD

## Provenance

- PR [#339](https://github.com/Kharn27/DoomRPG-RE/pull/339), [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269), branche `agent/esp32-sdl-f30-compiler-header-closure`.
- Base de la branche F30 : **main exact `e74e53d1d7db0da5f09af1b72c085d7d4f0a7bf6`** (fusion F29). Candidat matériel testé : **`50f5356d2991ec703dd48d3381ffc3f28c8feb82`** ; la capture Serial ne porte pas le SHA du binaire, donc l'attribution au candidat repose sur la branche déclarée testée.
- Capture classic CYD **`platformio-device-monitor-261114-193843.log`** : **1998 lignes, 240652 octets, SHA256 `bc6205a052dbd306f03009e81ad9cf2c62cea3e523385fc683eaa7ace8076dab`**. Test matériel réel communiqué le 14 novembre 2026.
- Après la création de F30, `main` a évolué indépendamment avec la PR #336 (palettes de thèmes), SHA `40a36643efec2a49f733377f02dc033c189ebba6`. **Ne pas attribuer le test F30 à cette version de main**. Toute intégration future devra être revalidée séparément.

## Changement logiciel et preuve

- Première CI [43597665630](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43597665630) **a échoué à bon escient** : `engine_stubs.c.d` contenait `include/SDL.h`. Chaîne exacte `engine_stubs.c → Z_Zip.h → SDL.h`.
- Seul changement de production : dans `ESP32/src/engine_stubs.c`, garde `!DOOMRPG_ESP32 || DOOMRPG_ESP32_GEOMETRY_ORACLE` autour de `#include "Z_Zip.h"`. `src/Z_Zip.h` et la référence desktop/oracle sont inchangés. Aucun corps de fonction C, branche d'exécution, PAK runtime, allocation, graphics ou mutation du monde modifié.
- Test `ESP32/test/sdl_f30_compiler_header_closure_host.py` : inversion exacte du garde et restauration du blob Git pré-F30 `fbec028fc332d4b1bd8bf516f1676757a74dd5ad`, cas adversariaux parsing make/GCC `.d` et chemins échappés, association `compile_commands.json` → objet GCC, contrôle de chaque dépendance réellement produite.
- Les anciennes empreintes des oracles F12/F13/F14/F16/F19/F21/Root-J restent vérifiées, grâce à inversion du garde F30 avant reconstruction des versions historiques.
- CI finale du candidat [43598246206](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43598246206) : **SUCCESS**. Les trois firmwares `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace` compilés. **171 sources ESP32/src et 171 fichiers GCC .d par profil, soit 513 fermetures contrôlées : 0 inclusion directe ou transitive de `SDL.h` ou `SDL_Video.h`; `esp32_sdl.cpp` et `esp32_bmp.cpp` exclus ; les oracles ELF maintiennent zéro fonction SDL liée**.
- Ressources CI : normal RAM statique **80736 B**, flash **860177 B** (F29: 80736 B / 860173 B) ; bringup **80880/882953 B**, trace **80776/878329 B**. Pas de mesure heap dynamique déduite du seul ELF.

## PASS matériel : scénario réellement observé

- Un `POWERON_RESET`, ESP32-D0WD-V3 240 MHz, flash 4 MB, **PSRAM=0**, SD montée, PAK indexé **241 entrées**, framebuffer **160×120×2 octets=38400 B** en 320×240 nearest ×2. Aucun nouveau boot, panic/Guru, exception, watchdog, Backtrace, erreur ou échec runtime décelé.
- MENU_MAIN → HELP/ABOUT, pages UP/DOWN, BACK retour `ed91dc73`; START → ST_INTRO, texte complet, 7 taps, `INTRODISP` libère **34116 B** ; `/intro.bsp` CRC32 `623f34e4`, runtime immuable `14095 B` FNV `c3882516`; frame jouable Entrance et HUD présentés.
- Gameplay frais : navigation et rotations/strafe, **7 `DOORANIM COMPLETE` sur la capture totale**, **5 dialogues OPEN/CLOSE** (dont one-shot arme), deux caisses cassées et plusieurs ramassages, 4 `MONSTERCOMBAT COMMIT`, 4 `MONSTERRETAL COMMIT`, deux séquences `MONSTER3GOAL` avec étapes 2/3 et 3/3 COMMIT et COMPLETE. Ces scènes prouvent la continuité réelle du jeu ; la capture n'est pas un inventaire exhaustif de tous les événements possibles.
- HUB/SYSTEM → EXIT (sans sauvegarde) → `RESIDENTRESET empty=1` libère **18008 B** et `SYSEXIT MENU-READY frame=ed91dc73`. Depuis MENU_MAIN, LOAD **slot 04, V11, 5604 B**, restore exact des ressources/script/lines/automap/monster state/topology/positions/activation/drop, `RESUME-VISIBLE` ; nouvelle exploration, combats et deuxième EXIT → mêmes `RESIDENTRESET` et `SYSEXIT`. **Aucune nouvelle écriture de sauvegarde V11 testée dans cette capture**, seulement lecture/restauration.
- OPTIONS/VIDEO/THEME puis `NATIVECONFIG SAVE result=committed` (config V3 12 B) et BACK au menu ; dernier `ALIVE uptime=220854 ms` soit 3 min 40,854 s depuis boot.
- Les deux `RAMBUDGET RESERVE` concluent `HEADROOM_OK`, marges heap8 **28092 B** puis **27280 B** ; plus gros bloc heap8 en jeu **61428 B** ; `shapeData=0x0` et `mediaTexels=0x0` restent visibles aux transitions. Aucun buffer ZIP map-wide, surface SDL ou texture map-wide n'a été introduit.
- **8 diagnostics `[NATIVEFRAME] BOUNDARY-INLINE`** : garde de lecture texture BSS borné connu et successful, pas un SDL-F30 FAIL. Capture sans drop monstre nouvellement généré visible (le V11 restauré annonce `MONSTERDROP visible=0`), donc aucune nouvelle preuve de rendu du drop dans ce log. Positions de projectile `1073743xxx` déjà signalées et non auditées ici.

## Périmètre de conclusion et suite

**PASS F30** pour l'élimination des dépendances transitives `SDL.h` / `SDL_Video.h` du code ESP32/src compilé ; CI compilateur et vrai appareil concordent. Les fichiers SDL historiques et leur ABI desktop restent dans le dépôt, sans être liés en fonctions SDL firmware. Cela **ne prouve pas** que `SDL_mixer.h` (header opaque) ou tous les vestiges SDL textuels sont absents des includes natifs : à auditer séparément, sans réintroduire le ZIP runtime.

L'utilisateur demande explicitement de **continuer sur la même branche** : ne pas fusionner la PR #339 dans main pour le moment. Si un prochain changement de production est ajouté, il aura son propre candidat CI et un nouveau PASS matériel avant merge.
