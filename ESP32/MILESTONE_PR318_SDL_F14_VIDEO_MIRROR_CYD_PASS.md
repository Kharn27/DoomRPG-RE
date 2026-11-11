# SDL-F14 — miroir vidéo SDL inerte retiré — PASS sur classic CYD

**Date :** 11 novembre 2026  
**PR :** [#318](https://github.com/Kharn27/DoomRPG-RE/pull/318)  
**Issue :** [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f14-retire-video-mirror`  
**Base `main` exacte (après fusion F13) :** `7cd69d5ab4b14e955ed3748a1f93115f8c217c38`  
**Commit firmware candidat matériel :** `03e184c7aca5e9b37bc1fd79f346ef4289378513`

## Périmètre et non-régressions

Un seul TU de production modifié : `ESP32/src/engine_stubs.c`, retrait de `SDLVideo_t sdlVideo`, `SDL_GetVideo` et `SDL_InitVideo` no-op. Le faux `SDL_Renderer* = 1` de la façade n'est plus nécessaire : le véritable framebuffer 160×120 RGB565 et la présentation TFT sont possédés par `PlatformVideo`. Aucune mutation des chemins fatals `SDL_Close`/Root-J6, fermetures audio `SDL_CloseAudio`, `fluidSynth`, prototypes desktop dans `src/SDL_Video.h`, `SDL.h`, `SDL_Rect`, du PAK/SD, des cartes, du gameplay, des allocations ou de la logique écran.

L'oracle `ESP32/test/sdl_f14_video_mirror_host.py` reconstruit la **source C entière** d'avant ce jalon, blob Git **`de49f01a5883b6c3d8cc2616e0902fe28716b161`** (indépendamment vérifié via GitHub Git Blob), et scanne **359 sources natives** ; `xtensa-esp32-elf-nm` confirme les 3 symboles absents de chacun des trois ELF. Les tests F13 et F12 composent l'inversion F14, **tous les oracles Root-J2..J6 et SDL-F10..F13 sont préservés**.

CI candidat sur `03e184c7aca5e9b37bc1fd79f346ef4289378513` : [push #43082985063](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43082985063) et [PR #43083061290](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43083061290) : **SUCCESS**, tous les tests C/Python, trois PlatformIO, compiledb/ELF. Firmware normal **859321 B flash / 80736 B RAM statique**, bringup **882053/80880 B**, trace **877445/80776 B**. Flash normal/trace -8 B par rapport à F13 ; aucun gain de heap ni de performance inféré.

## Vraie preuve matérielle

**Source unique :** `device-monitor-261111-202334.log`, **1528 lignes / 180854 octets**. Une première ligne `[ALIVE]` est antérieure au redémarrage, puis une **unique** séquence `rst:0x1 (POWERON_RESET)`. Toutes les validations suivantes correspondent à la séquence complète après reboot ; pas d'autre reset ni trace de panic.

- **Démarrage :** classic CYD ESP32-D0WD-V3 dual-core 240MHz, 4MB flash, **PSRAM = 0 B** ; SD montée 240MiB, PAK **241 entrées**, framebuffer natif 160×120×16bits = **38400 B** à ×2 sur TFT 320×240. `Render=108 Game=0 Canvas=36`, CORE/LAYOUT/PRERENDER/RENDERSTART/MAPPINGS/MENU READY, mappings payload **8376 B**.
- **Options, retour et intro :** VIDEO→THEME **1→0**, LUMINOSITÉ **50→100%** avec deux commits SD-v3 et BACK to MENU_MAIN, FNV final **1945fafa** pour le thème choisi. START→ST_INTRO→FINAL-CONTINUE→`INTRODISP READY recovered=34080` ; `/intro.bsp` Entrance CRC32 **623f34e4**, immuable arena **14095B** ; bootstrap/gameplay natifs.
- **Gameplay réel :** **2 `[DIALOG] OPEN` et 2 CLOSE**, **4 DOORANIM COMPLETE**, **12 PICKUPMINI START / 12 END** non bloquants. Premier HUB EXIT→MENU ; LOAD fichier **`/DoomRPG-ESP32-slot04.sav` V11 5604 B** restauration ressources, scripts, lines, crates, automap, monstres/topologie/positions/activation/drops puis RESUME-VISIBLE. **3× `MONSTERRETAL COMMIT`** et **3× `MALWRATHPROJECTILE CLEAR`** des triple tirs 204, tirs `MONSTERCOMBAT COMMIT` jusqu'à mort du monstre ; second HUB EXIT→MENU.
- **Résilience et RAM :** 2× `advisory=HEADROOM_OK`, marges `heap8` **28268 / 27456B** et DMA **69228 / 68416B** ; 2× `RESIDENTRESET` libère **18008 B** avec `empty=1`; 2× `SYSEXIT MENU-READY frame=1945fafa session=off resident=empty`, checkpoint inchangé. **13 occurrences** `shapeData=0x0 mediaTexels=0x0`, et **0 crash / Guru / panic / WDT / abort / ERROR / FAILED**. Une ligne `NATIVEFRAME BOUNDARY-INLINE` : diagnostic préexistant non fatal, sans rapport causal avec F14 débranchant trois symboles inertes.

## Ce que ce PASS ne prouve pas

Le HELP/ABOUT **n'est pas parcouru** pendant cette session (les tests existants le couvrent par ailleurs, pas ce log). Aucun chemin fatal/OOM n'est volontairement forcé, pas de stress autres cartes, pas de benchmark A/B RAM/perfs. Serial n'imprime pas le SHA du firmware ; correspondance avec le commit candidat attribuée au flash utilisateur, non authentification du binaire.

**Décision :** PASS pour le périmètre F14 testé et non-régression des interfaces natives. Après ce test, **exclusivement commit documentaire** sur 3 fichiers (`MILESTONE_PR318_SDL_F14_VIDEO_MIRROR_CYD_PASS.md`, `PORTING_STATUS.md`, `DOCUMENTATION.md`), CI finale obligatoire avant fusion #318. Ensuite relire le vrai `main` GitHub et faire un inventaire des entrées `SDL_Close`, `SDL_CloseAudio`, `SDL.h` et fatal Root-J6 avant de toucher aux interfaces encore reliées.
