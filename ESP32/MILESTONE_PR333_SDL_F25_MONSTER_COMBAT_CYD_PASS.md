# SDL-F25 — quatre unités natives combat monstre SDL.h isolées : PASS CYD

## Provenance, portée et ABI

- [PR #333](https://github.com/Kharn27/DoomRPG-RE/pull/333), [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269), branche `agent/esp32-sdl-f25-monster-combat-header-boundary`.
- Base GitHub vérifiée avant développement : `main=1e352fb3510e1d637a67d693000b7954fb3e21e0`. Firmware candidat **effectivement flashé et testé** : `cfb8ca39b8e46221572f4e762725e4fe443ced4a`, profil normal PlatformIO `esp32-cyd`.
- Uniquement la première inclusion `SDL.h` est gardée dans ces **quatre unités C réellement compilées** : `ESP32/src/esp_native_gameplay_monster_active_sequence.c`, `esp_native_gameplay_monster_attack_visual.c`, `esp_native_gameplay_monster_combat.c`, `esp_native_gameplay_monster_retaliation.c`.
- En firmware (`DOOMRPG_ESP32` sans oracle), l'import SDL.h est éliminé ; desktop et `DOOMRPG_ESP32_GEOMETRY_ORACLE` conservent l'include historique. **Corps C / signatures / fonctions / layouts / allocations / rollback RNG / V11 / PAK / rendu et comportement gameplay : aucun changement**.
- Nouvel oracle `ESP32/test/sdl_f25_monster_combat_header_host.py` reconstitue les quatre sources antérieures byte-exact et contrôle les véritables Git blobs : active sequence `3d3a1b3433c688a81f2c68326573d2820f825d5f`, attack visual `5efa3429e745d68ef319927a6c7a9ea19097b821`, combat `9337b170163d43f6b16efc8b1fc17f409d9ab223`, retaliation `7693d7c031fe1df985a9f844befa6df810e971ba`.
- Oracle C11 natif / desktop / geometry sur les **vrais** `DoomRPG.h` et `Render.h`, sans réintroduire SDL.h dans le cas natif. `compiledb` prouve les quatre sources compilées dans chacun des trois profils et les deux anciens TUs SDL exclus ; ELF legacy vérifie **0 fonction SDL liée**. Oracle C réel du timeline d'attaque monstre conservé.

## CI du firmware testé

- Commit firmware `cfb8ca39b8e46221572f4e762725e4fe443ced4a`.
- [Push Actions #43328655140](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43328655140) : **SUCCESS 160/160 étapes**.
- [PR Actions #43328730991](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43328730991) : **SUCCESS 160/160 étapes**.
- Tailles statiques CI, **identiques F24** : `esp32-cyd` flash **859693 B**, RAM statique **80736 B** ; bringup **882477 / 80880 B** ; trace **877845 / 80776 B**. Aucun chiffre de heap dynamique imputé au code de F25.

## Validation sur le vrai classic CYD

Log : `platformio-device-monitor-261113-003300.log`, **1926 lignes / 238904 B**, SHA256 **`d9f90bedf23ccac981753a84a123476e77cb4af74a5ac9a90bad18709951fdee`**.

- Boot ROM unique `rst:0x1 POWERON_RESET`, puce ESP32-D0WD-V3 rev 3 à 240 MHz, flash 4194304 B, **0 PSRAM**. SD montée, PAK 241 entrées ; framebuffer 160×120 RGB565 38400 B, LCD 320×240 x2.
- HELP/BACK, START→intro complète→`/intro.bsp` Entrance (CRC `623f34e4`), mouvements et rotations, interactions portes et dialogues. **6× DIALOG OPEN et 6× CLOSE** (8, 26, standalone), **7× DOORANIM COMPLETE**, **12× PICKUPMINI START/END**.
- HUB→SYSTEM→EXIT confirmé, retour `MENU_MAIN` **deux fois** : 2× `RESIDENTRESET released=18008 empty=1`, 2× `SYSEXIT MENU-READY frame=4c01eb2b`.
- LOAD V11 slot04 **5604 B** : restoration exacte enregistrée de ressources, scripts, line/texture overlays, action removals, caisses, automap, monstres, topologie, positions, activation et drops.
- Combat post-LOAD : Malwrath `sprite=322` HP **11→8→5→0**, **3 ripostes** avec séquences 3 tirs/animations et RNG guard. Deux autres monstres `sprite279` et `sprite272` attaqués, puis morts ; **5× `MONSTERRETAL COMMIT`** dans le log, avec dommage joueur et effets de douleur.
- RAM : **2× `HEADROOM_OK`** marges heap8 **28092 / 27280 B**, largest8 jeu **61428 B** ; 2× `ENGINESESSION READY shapeData=0x0 mediaTexels=0x0`. **26× ALIVE**, dernier uptime **136714 ms** avec heap8 **130560 B**, largest8 **86004 B** après retour menu. Aucune panique/Guru/WDT/reset secondaire, aucune `fail=SPRITES`, `SETTLE-FAILED` ou `DEATH-REDRAW-FAILED`.
- `MONSTERDROP COMMIT def=92` : créé logiquement mais **`RENDER-CULL reason=player-tile`** ; apparition graphique non prouvée, **drop rare defTile=100 non observé**. Des `NATIVEFRAME BOUNDARY-INLINE` et positions projectile Malwrath à bit haut subsistent, déjà présents avant F25 et hors de ce jalon.
- Non couverts par cette capture : visite OPTIONS, NOTE, nouveau SAVE et sortie audio matérielle.

## Frontière SDL résiduelle et clôture

- Les 3 ELFs n'ont toujours **aucune fonction SDL liée**, mais **cela ne démontre pas SDL-free au niveau des headers**.
- `ESP32/include/esp_native_rng_replay_guard.h` importe encore directement `SDL.h` (transitif) ; d'autres sources C en importent toujours directement. `ESP32/include/SDL_mixer.h` est déjà gardé côté firmware et `esp32_sdl.cpp` / `esp32_bmp.cpp` restent exclus du build mais préservés pour les oracles hôtes.
- Après PASS CYD du firmware `cfb8ca39b8e46221572f4e762725e4fe443ced4a`, seuls `PORTING_STATUS.md`, `DOCUMENTATION.md` et ce milestone Markdown changent. CI complète post-doc à confirmer **avant** fusion. L'utilisateur a expressément autorisé la fusion après ce PASS.

