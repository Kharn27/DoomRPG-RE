# SDL-F13 — Retrait façades input/modes vidéo inertes — PASS sur classic CYD

**Date :** 11 novembre 2026  
**PR :** [#317](https://github.com/Kharn27/DoomRPG-RE/pull/317)  
**Issue :** [SDL #269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f13-dead-input-mode-facades`  
**Candidat firmware attribué au flash :** `859544efb53b9b8380b43fee85ac2ccdb3182d99`  
**Base de branche `main` exacte après fusion SDL-F12 :** `fdfad091cc0067f19bde64e2adcb4a67cff7bc77`

## Périmètre gelé

Un seul fichier de production : `ESP32/src/engine_stubs.c`. Retrait de 4 vieux stubs manette/touch noms et de 2 globaux inertes de compatibilité : `SDL_GameControllerGetButtonID`, `SDL_GameControllerGetNameButton`, `SDL_MouseGetNameButton`, `SDL_JoystickGetButtonID`, `sdlController`, `sdlVideoModes[14]`. Le tactile XPT2046 est géré par le propriétaire natif, la géométrie vidéo effective est fixe. Les prototypes desktop `src/SDL_Video.h`, `SDL.h`, `SDLVideo_t sdlVideo`, `SDL_GetVideo`, `SDL_InitVideo/Close`, `SDL_InitAudio/CloseAudio`, `fluidSynth`, processus Root-J6, distributions PAK, framebuffer unique et gameplay sont **intacts**.

Le test `ESP32/test/sdl_f13_dead_input_mode_host.py` restaure octet pour octet le blob Git source complet de `engine_stubs.c` pré-F13 **`13b622cff53cfe20d4368d0739b1e492ae88ebc4`** (SHA vérifié indépendamment par GitHub), inventorie le code non-commenté de **359 sources/headers natifs** et vérifie l'absence des six symboles dans les **trois ELF**. L'oracle F12 inverse F13 avant sa propre restitution : tests historiques Root-J2/J3/J4/J5/J6, SDL-F10/F11/F12 et essais C de Canvas/Render conservés. GitHub Actions sur candidat [push #43079848048](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43079848048) et [PR #43079925612](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43079925612) **SUCCESS** (hôte, compilations PlatformIO, compiledb/ELF). Tailles normal **859329 B flash / 80736 B statique**, bringup **882053/80880**, trace **877453/80776**. L'écart flash vs F12 n'est pas un gain de RAM dynamique.

## Preuve sur vrai ESP32-2432S028R

Source : **`device-monitor-261111-194711.log`**, **1759 lignes / 203873 octets**. Un unique `POWERON_RESET`, 22 `ALIVE` de la même session ; le log commence par le boot. Candidat déterminé par le contexte de flash utilisateur ; aucun SHA de firmware imprimé sur Serial.

- **Démarrage :** ESP32-D0WD-V3, dual core 240MHz, flash 4 MB, **PSRAM 0**. SD montée **240 MiB**, PAK **241 entrées** ; framebuffer logique **160×120 RGB565 / 38400 B**, panneau **320×240 ×2**, `Render=108 Game=0 Canvas=36`. CORE/LAYOUT/PRERENDER/RENDERSTART/CONFIGMAP/MENU READY ; mapping `mappings.bin` **8392B / 8376B** payload, buffers natifs et scratch framebuffer.
- **Menu/intro :** Options→Vidéo→Luminosité **100→50 %** persistée sur SD, BACK ; AIDE **112 lignes**, pagination 0→49, BACK. Start Game→intro tactile/50ms→`INTRODISP READY` libérant **34080 B**→Entrance `/intro.bsp`, CRC **623f34e4** et arena immutable **14095 B** ; premiers événements natifs et gameplay opérationnels.
- **Gameplay complet testé :** **8× `DIALOG OPEN`** dont **1 OPEN-STANDALONE**, **8× `DIALOG CLOSE`** ; **4× `DOORANIM COMPLETE`**, **13× `PICKUPMINI START` / 12× `END`** (la dernière démarre près du second retour menu, sans délai pour atteindre sa fin temporelle, mais nettoyage session terminé), HUB/EXIT sans SAVE ; menu LOAD V11 **slot04 5604 B**, restauration resources/script/lines/crates/automap/monsters/topology/positions/activation/drops, gameplay reprise `RESUME-VISIBLE`, **3× `MONSTERRETAL COMMIT` / 3× `MALWRATHPROJECTILE CLEAR`** pour triple tir 204, puis nouveau EXIT→MENU.
- **RAM et invariants :** **2× `advisory=HEADROOM_OK`** : marges heap8 **28268 / 27456 B**, DMA **69228 / 68416 B** ; **2× `RESIDENTRESET` libèrent 18008 B**, état `empty=1` ; **2× `SYSEXIT MENU-READY`**, frame **`e687f0c7`**, session off, resident empty ; **13× `shapeData=0x0 mediaTexels=0x0`**. Aucun `Guru Meditation`, `WDT`, `panic`, `Backtrace`, `abort`, `[ERROR]` ni `[FAILED]`. **10× `NATIVEFRAME BOUNDARY-INLINE`** : diagnostics connus non fatals, non attribuables à ce diff purement inert.
- **Limites de preuve :** seul ce parcours normal `esp32-cyd`, pas de test volontaire `DoomRPG_Error`/OOM, d'autres maps, de TOUS les scénarios ni benchmark A/B. L'absence de SHA du binaire dans Serial interdit une attestation indépendante de correspondance binaire ; attribution test utilisateur seulement.

## Statut et suite

**PASS matériel ciblé confirmé**. Le commit code flashé reste **gelé** ; après le log, seuls ce rapport, `PORTING_STATUS.md` et `DOCUMENTATION.md` sont ajoutés. CI finale documentaire requise avant fusion #317, puis relire le vrai `main` et son SHA. Suite SDL : audit des derniers vrais consommateurs du miroir `SDLVideo_t/sdlVideo`, de `SDL_GetVideo`, `SDL.h` et des entrées fatal/vidéo/audio ; **ne pas supprimer des ABI actives en bloc**, ni restaurer SDL2/ZIP runtime/PSRAM, et ne pas confondre nettoyage de facade et gain RAM.
