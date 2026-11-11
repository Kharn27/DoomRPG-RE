# SDL-F15 — bootstrap audio SDL et FluidSynth factices retirés — PASS sur classic CYD

**Date :** 11 novembre 2026  
**PR :** [#319](https://github.com/Kharn27/DoomRPG-RE/pull/319)  
**Issue :** [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f15-inert-audio-boot-retirement`  
**Base `main` exacte (après fusion F14 #318) :** `d7d62425a8c8bc91361d18e5519b303370187bc8`  
**Commit firmware candidat CYD :** `ae2c0fd81516284045e77ebf0a9ee518ec1b77eb`

## Périmètre et propriété des systèmes

Un seul TU C productif modifié : `ESP32/src/engine_stubs.c`, suppression de la **variable globale `FluidSynth_t fluidSynth = {NULL, NULL, NULL};`**, simple façade desktop non consommée, et de l'init audio SDL **`SDL_InitAudio(void) {}`** vide. Les **`SDL_CloseAudio()`** et **`SDL_Close()`**, leur ordre d'appel depuis `DoomRPG_Error`, `DoomRPG_FreeAppData` et `exit(0)` **restent exactement inchangés** pour préserver Root-J6. Source et ABI desktop `src/SDL_Video.h` conservent la spécification de comportement. Audio natif en `AUDIOINTENT backend=silent` ; pas de pipeline I2S, BLE/A2DP, charge audio ou fermeture runtime ZIP réintroduits. Les chemins PAK/SD, cartes, joueur, gameplay, rendu et framebuffer unique `PlatformVideo` ne changent pas.

## Vérification automatisée du code testé

L'oracle `ESP32/test/sdl_f15_inert_audio_boot_host.py` inverse exactement les deux retraits dans la **source intégrale** puis vérifie le blob Git original **`8cf67bc7c31c5982e1f343b30663df9dba1dd594`**, indépendamment confirmé par l'API GitHub Git Blob. Il rejette les deux anciens symboles `fluidSynth` et `SDL_InitAudio` dans **359 fichiers C/C++/headers natifs** et dans chacun des trois ELF Xtensa. F14 doit inverser F15 avant son propre SHA historique ; F13 et F12 inversent F14 en chaîne. Les tests Root-J6 (4096 cas C réels de teardown), tous les autres oracles natifs et les trois builds sont conservés.

CI du candidat `ae2c0fd81516284045e77ebf0a9ee518ec1b77eb` : [push #43088387854](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43088387854), [PR #43088464166](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43088464166), **SUCCESS / 148 étapes chacun**. Résultats : normal `esp32-cyd` **859321B flash / 80736B RAM statique**, bringup **882053/80880B**, trace **877445/80776B**. Tailles **inchangées vs F14** : l'éditeur de liens excluait les symboles inertes ; aucun gain de RAM dynamique, latence ou son revendiqué.

## Véritable preuve matérielle — Serial normal `esp32-cyd`

**Source :** `device-monitor-261111-204308.log`, **1816 lignes / 208236 octets**. Trois premières `[ALIVE]` précèdent `rst:0x1 POWERON_RESET` et le nouveau démarrage complet ; **une seule séquence reboot**, aucune relance ou erreur fatale après.

1. **CYD, PAK et rendu :** ESP32-D0WD-V3 rev3, 2×240 MHz, 4MB flash, **0 PSRAM** ; SD 240MiB, PAK **241 entrées**, rendu 160×120 RGB565 = **38400 octets** et nearest-neighbour x2 320×240. `Render_t 108B`, `Game_t 0B`, `DoomCanvas_t 36B`; CORE/LAYOUT/PRERENDER/RENDERSTART/MAPPINGS/MENU READY. `AUDIOINTENT` reste `backend=silent`.
2. **Navigation et démarrage :** Options→VIDEO→BACK→Options→BACK, aucune modification durable des réglages dans ce parcours. HELP natif **112 lignes**, six `PAGE-DOWN`, assets `c.bmp` + aperçu PAK, BACK vers menu **FNV=1945fafa**. START→introduction entière→`INTRODISP READY recovered=34116B`→chargement natif Entrance `/intro.bsp` CRC32 **623f34e4**, arène immutable **14095B**. `shapeData/mediaTexels=NULL` avant/après.
3. **Gameplay complet :** **9 ouvertures de dialogue (8 `[DIALOG] OPEN`, 1 `OPEN-STANDALONE`), 9 fermetures** ; un dialogue de déplacement `opcode=26`, des reprises d'événements et **2 `[NOTE] APPEND`**. **4 `DOORANIM COMPLETE`**, **12 `PICKUPMINI START` et 11 `END`** ; la 12e miniature `logical=96` commence ligne 1735, puis HUB/EXIT est déclenché ligne 1740, teardown natif ensuite : absence du END liée à sortie sans attente, non erreur. Armes, objets, dialogues scientifiques, notes, portes et déplacements utilisés. SAVE/LOAD : **EXIT HUB** (sans écriture)→menu→**LOAD `/DoomRPG-ESP32-slot04.sav` V11 5604 B**, ressources, script, lignes, crates, automap, monstres/topologie/positions/activation/drops restaurés exactement, RESUME-VISIBLE. Combat Malwrath: **3 `MONSTERRETAL COMMIT`, 3 `MALWRATHPROJECTILE CLEAR`** avec animations 3-shot complètes ; second **EXIT HUB**→menu sans écriture.
4. **RAM et résilience :** **2× `HEADROOM_OK`**, marges `heap8` **28468B / 27656B**, `DMA` **69428B / 68616B** ; **2× `RESIDENTRESET released=18008 empty=1`**, **2× `SYSEXIT MENU-READY frame=1945fafa`**, checkpoint inchangé. **13** mentions `shapeData=0x0 mediaTexels=0x0` ; **0 panic, Guru, WDT, abort, [ERROR], [FAILED]**. Des messages `[NATIVEFRAME] BOUNDARY-INLINE` sont des diagnostics existants non fatals, sans changement démontré ici.

## Limites / décision

Ce test ne force pas `DoomRPG_Error`, OOM ou tout autre chemin fatal ; aucune comparaison A/B heap/performance ni visite exhaustive d'autres cartes. Le log Serial **n'imprime pas de SHA** du firmware, donc attribution au candidat via contexte de flash CYD utilisateur, non authentification du binaire.

**Décision : PASS matériel dans la frontière F15**. Ne changer **aucune ligne du firmware testé** après le log : seulement `MILESTONE_PR319_SDL_F15_INERT_AUDIO_BOOT_CYD_PASS.md`, `PORTING_STATUS.md`, `DOCUMENTATION.md`. Exiger CI finale push + PR success avant fusion #319. Après fusion, vérifier le SHA du **vrai main GitHub** puis auditer les consommateurs compilés `SDL.h`/`SDL_Video.h`/`SDL_Close`/`SDL_CloseAudio` et la frontière fatale Root-J6. Ne pas considérer SDL totalement retiré sans inventaire du binaire et des includes.
