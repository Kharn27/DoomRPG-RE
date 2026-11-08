# PR #268-C — Render_t::clipRect retiré : PASS matériel classic CYD

**Date du test : 9 novembre 2026.** Dépôt `Kharn27/DoomRPG-RE`, [PR #268](https://github.com/Kharn27/DoomRPG-RE/pull/268), branche `agent/esp32-render-retire-skipcull-state`. **Firmware et tests C effectivement flashés au SHA `22cdb04a7a8eca1b756ebb580ca2a175e395d55a`**, base de travail `main=77649ebf992e57aa5f57ff6efd138b45d871c9c2`. Source de vérité du test : `device-monitor-261109-000624.log`, **1 449 lignes, 175 244 octets**. Accord utilisateur : « Si c'est OK tu peux fusionner et me donner le prompt pour continuer de désengager doomDesktop ».

## Frontière du jalon C

- `Render_t::clipRect` (`SDL_Rect` de 16 octets) a été supprimé des trois profils ESP32 normal, bringup et trace ; conservé pour le desktop/oracle. Aucun changement des appels de clipping en jeu : le rectangle historique était uniquement initialisé dans `EspNativeRenderStartup_initialize()` puis comparé aux mêmes constantes dans le contrôle de startup. L'audit des 178 TU ESP32 natifs n'a trouvé aucun lecteur gameplay/raster du champ.
- Le startup contrôle désormais la **dimension réelle du SDL renderer `sdlVideo.rendererW/H`** contre `DOOMRPG_CANVAS_WIDTH/HEIGHT`, avant toute I/O `sintable.bin`. Il refuse explicitement 159×120 et 160×119 dans le harnais hôte ; l'adresse framebuffer partagée, la taille réellement disponible et le pitch restent vérifiés. Pas de nouvel état ni allocation et aucun relâchement des gardes fail-closed.
- Taille `Render_t` attendue `136→120 B` avec `_Static_assert(sizeof(Render_t) == 120U)` en production ; l'oracle et le desktop préservent leur ABI. La chaîne complète PR #268 (A `skipCull`, B `halfScreenHeight`, C `clipRect`) réduit le shell `144→120 B`, sans prétendre à un gain net de 24 B de heap.
- Nouveau test `native_render_clip_rect_retirement_host.py` et assertions dans `native_render_startup_retirement_host.py` (4 096 démarrages et dimensions invalides), plus mises à jour des tests ABI et du hash FNV startup.

## CI du SHA C, avant le flash

- [Push GitHub Actions #42532684123](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42532684123) : **SUCCESS, 102/102 étapes**.
- [PR GitHub Actions #42532686674](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42532686674) : **SUCCESS, 102/102 étapes**.
- Les trois builds ESP32 (normal `esp32-cyd`, bringup, trace) et les gardes hôte/ELF ont réussi. Ce ne sont pas des preuves hardware à eux seuls.

## Preuves du vrai firmware normal, log 9 novembre

- Hardware `ESP32-D0WD-V3`, 240 MHz dual-core, 4 MB flash, **0 PSRAM** ; `[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes`, présentation 2× vers 320×240. Au boot : **`Engine structs: Render=120 Game=0 Canvas=44 Total=924 bytes`** ; `[RENDERCORE] INIT ... bytes=120`. Startup réel `[RENDERSTART] READY shared framebuffer, sintable and palettes initialized`, framebuffer partagé 160×120, pitch=320, 3280 palettes, PAK 241 entrées.
- **NEW GAME, Entrance `/intro.bsp`**, PAK CRC `623f34e4`, runtime compact `arenaBytes=14095`, FNV `c3882516`, première frame `71ca7465`. Intro chargée/relâchée, directions et rotations avec interpolation et `gameplayStable=yes`, caisse transformée/loot, pickups armor shard/medkit/bullets/Fire Ext et aide à la première acquisition, porte275 animée 4 frames puis fermeture différée, dialogue opcode8 event88 et opcode26 event79 puis reprise opcode19, HUB système/EXIT. `[ENGINESESSION] FRESH-VISIBLE`, `[RESIDENTRESET] ... released=18008 ... empty=1`, premier `[SYSEXIT] MENU-READY frame=3721ecf7`.
- **LOAD V11 slot04 Entrance**, 5604 octets : restauration des ressources 48 consommés, états scripts/lines/texture, caisses, automap, monstres (30), topologie (43 tracked), positions, ordre d'activation, drops ; `[ENGINESESSION] RESUME-VISIBLE`. Deux changements d'arme sans tour ; pickup health, porte113 quatre frames ; Malwrath sprite322 attaque **4 fois**, chaque séquence de **3 projectiles 204** visualisés, `[MALWRATHPROJECTILE] CLEAR` ×4, `[MONSTERRETAL] COMMIT` ×4, impact original sprite184, red border/flash ; joueur riposte au shotgun, `[MONSTERCOMBAT] DEATH-SETTLE sprite=322`, drop armor shard commit, puis secret event15 deux portes et +5 XP. Deuxième HUB système/EXIT : second `[RESIDENTRESET] ... empty=1`, second `[SYSEXIT] MENU-READY frame=3721ecf7`.
- **RAM** : deux `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK` ; `largest8=86004` en jeu, `largest8=110580` menu. Deux resets complets `after=0/0/0/0/0/0/0 empty=1`, deux sorties au menu, `shapeData=0x0 mediaTexels=0x0` aux bornes journalisées. **Aucune occurrence `ERROR`, `FAILED`, `panic`, `Guru Meditation`, `WDT` ou `abort` dans les 1 449 lignes**.
- Le journal prouve le **PASS matériel ciblé C**, pas la parité pixel-perfect exhaustive, la stabilité longue durée ou un gain net heap 16 B.

## Contrat post-test et fusion

Le code réellement flashé reste **figé au SHA C `22cdb04a7a8eca1b756ebb580ca2a175e395d55a`**. Ce rapport et les deux entrées d'index `PORTING_STATUS.md` et `DOCUMENTATION.md` sont un **commit docs-only après test**. L'utilisateur a explicitement demandé de **fusionner la PR #268 si PASS** et d'enchaîner avec un prompt pour la suite du désengagement du desktop. Vérifier CI du commit de documentation, dernier état PR/main, puis fusionner ; relire le SHA exact de `main` fusionné avant le prochain jalon. Ne jamais réutiliser le SHA de branche pré-merge comme base d'une nouvelle branche.
