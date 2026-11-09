# PR #272 — Retrait des tableaux floorColor/ceilingColor : PASS matériel classic CYD

**Date : 9 novembre 2026.** [PR #272](https://github.com/Kharn27/DoomRPG-RE/pull/272), branche `agent/esp32-render-retire-plane-color-arrays`. **SHA exact du firmware validé : `4dd32d51466d4f7bb85690895d1fd6fef5f1d591`**. Base `main=c9dc7a289bd932492f9cfcf1d54241168c7e0849` (PR #268 et #270 déjà fusionnées). Source primaire : `device-monitor-261109-010129.log`, **1 408 lignes**, session d'environ 101 secondes, vrai `esp32-cyd` normal. Accord utilisateur pour fusion si conforme et poursuite.

## Frontière retirée

- `Render_t::ceilingColor` et `Render_t::floorColor` étaient deux tableaux desktop de `short[screenWidth]` dont les seules manipulations en production étaient init, setup, vérification et destruction, sans consommateur de rendu natif. Le rendu natif lit les couleurs et textures des plans depuis `EspMapRuntime`/PAK et ses petits propriétaires.
- **`Render_t=120→112 B`** en normal, bringup et trace ; les membres desktop restent dans `src/Render.h` pour l'oracle historique. Suppression de deux `SDL_malloc(160 * sizeof(short))` et de leur destruction : **640 B de charge utile d'allocations évitées** au setup. `columnScale` (640 B) reste alloué avec garde OOM. La géométrie, les 4 frames de porte, caméra/sprites, palette, trig, plan native, framebuffer partagé 160×120 RGB565, pitch et chemins de présentation ne changent pas.
- `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=640B` et `[LAYOUT] ... arrays=640B` confirmés. Ne pas confondre le payload supprimé avec un gain heap net chiffré : allocations et overhead/fragmentation changent, et aucun comparatif A/B hardware strict n'a été réalisé.

## Tests et CI avant flash

- **Code figé au SHA `4dd32d51466d4f7bb85690895d1fd6fef5f1d591`** après un commit initial de retrait `f614b0fbf27298e581ed1d55e9f9d05595238878` et un second commit de **trois libellés de probes FNV seulement**. Aucun changement source runtime après cette validation.
- GitHub Actions push [#42537287617](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42537287617) et PR [#42537291171](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42537291171) : **SUCCESS** ; compilations `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`, tests hôte, ABI négative normal/bringup/trace/oracle, présence desktop, OOM, 4 096 cycles setup/teardown, audits compiledb/ELF et SDL-A. Seul le firmware normal a été flashé et éprouvé physiquement.

## Preuves Serial, log réel

- Chip ESP32-D0WD-V3 dual-core 240 MHz, 4 Mo flash, **0 PSRAM**, PAK 241 entrées ; boot `Engine structs: Render=112 Game=0 Canvas=44 Total=916 bytes`, `[RENDERCORE] INIT ... bytes=112`, `[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes`, `[RENDER] Shared framebuffer 160x120 pitch=320`.
- **NEW GAME Entrance** : intro, transition `/intro.bsp` CRC32 `623f34e4`, immutable `arenaBytes=14095` FNV `c3882516`, première frame `71ca7465`, animations `gameplayStable=yes`, caisse→pickup, armures/munitions/Fire Ext et aide d'arme, porte275 ouverte/fermée en 4 frames, dialogues opcode8 event88 et opcode26 event79 avec reprise opcode19, HUB puis premier EXIT. `[RESIDENTRESET] ... released=18008 ... empty=1`, `[SYSEXIT] MENU-READY frame=e687f0c7`.
- **LOAD V11 slot04 Entrance** (5 604 octets, sauvegarde validée, restore complet ressources/script/lignes/caisses/monstres/topologie/position/activation/drops) : `[ENGINESESSION] RESUME-VISIBLE`, porte113 4 frames, Malwrath sprite322 **trois séquences de triple projectile204** avec `[MALWRATHPROJECTILE] CLEAR` ×3, `[MONSTERRETAL] COMMIT` ×3, impacts originaux sprite184, bordure rouge, shake/tint, tirs et mort Malwrath 11→7→3→0, drop ramassé, exploration puis second HUB/EXIT. Deuxième `[RESIDENTRESET] ... empty=1` et `[SYSEXIT] MENU-READY frame=e687f0c7`.
- Réserves : **deux** `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`. NEW : `heap8=109380` au prime, `margin8=52036`; LOAD : `heap8=108568`, `margin8=51224`, `largest8=86004` en jeu. Après second EXIT : `[ALIVE] uptime=101401 ms heap8=154504 largest8=110580`. `shapeData=0x0 mediaTexels=0x0` aux bornes des deux sessions ; aucun `ERROR`, `FAILED`, `panic`, `WDT`, `Guru Meditation`, `abort` dans ce log.
- **PASS matériel ciblé**, pas une preuve de pixel-perfect exhaustive, de FPS comparés, d'endurance long terme ni de gain net heap chiffré. Les marqueurs `[NATIVEFRAME] BOUNDARY-INLINE` et les coordonnées de projectile `1073743xxx` ne sont pas nouveaux et ne sont pas traités ici.

## Fermeture du jalon

Après le PASS du firmware `4dd32d51466d4f7bb85690895d1fd6fef5f1d591`, **seulement** ce rapport et les index `PORTING_STATUS.md`/`DOCUMENTATION.md` sont modifiés (commit docs-only). PR #267 HELP non modifiée, shim SDL-A #270 non modifié. Le code validé reste gelé. Fusion de #272 explicitement autorisée par l'utilisateur sous condition de PASS ; **vérifier la CI du commit documentaire et le nouveau SHA exact de main après fusion avant tout nouveau agent/***.
