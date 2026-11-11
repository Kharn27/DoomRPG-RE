# SDL-F3 — shim SDL non compilé : PASS matériel classic CYD (11 novembre 2026)

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/297 ; **issue SDL** : https://github.com/Kharn27/DoomRPG-RE/issues/269
- **Base exacte** : `main=acdb4751963dc2cc68af457f776d895517b2fe0e` après fusion de SDL-F2 #296.
- **Code candidat testé** : `504973472a0ad3f51ae53f3c7450efa056284a73`. Le Serial **n'imprime aucun SHA firmware** ; attribution au candidat à partir du flash et du parcours matériel de l'utilisateur, pas un fingerprint embarqué.
- **CI du code avant flash** : https://github.com/Kharn27/DoomRPG-RE/actions/runs/43011477981 **SUCCESS** après correction d'une assertion hôte obsolète (nouveau diagnostic « host-only SDL shim oracle »). Tests hôte et oracle de pixels, compilation `esp32-cyd`/`esp32-cyd-bringup`/`esp32-cyd-trace`, contrôle `compiledb` et audit ELF : **0 TU `esp32_sdl.cpp` compilé, 0 objet `src/esp32_sdl.cpp.o`, 0 symbole `Esp32Sdl_attachDisplay`, 0 symbole `SDL_*` lié**, pour les trois profils.
- **Tailles compilées** : normal **859445 B flash / 80768 B RAM statique**, bringup **882181/80912 B**, trace **877585/80808 B**. Versus SDL-F2 normal **859921/80768 B** : −476 B flash et RAM statique stable ; aucun benchmark de temps/heap réel A/B.
- **Serial** : `device-monitor-261111-123300.log` (**1766 lignes / 204235 octets**), POWERON_RESET complet, vrai ESP32-D0WD-V3 dual-core 240 MHz, flash 4 MB, **PSRAM=0**, PAK natif 241 entrées.

## Frontière de code

`ESP32/platformio.ini` exclut **explicitement** `esp32_sdl.cpp` de `build_src_filter`, transmis aux trois profils. `main.cpp` ne référence plus `esp32_sdl_platform.h` ou `Esp32Sdl_attachDisplay`. Le TFT est initialisé par `display.begin()` puis donné directement au propriétaire `PlatformVideo_begin(&display)` **inchangé** ; le framebuffer 160×120 RGB565 de **38400 B** et la présentation nearest-neighbor ×2 vers 320×240 restent inchangés. `PlatformVideo_present()` n'a pas été touché.

`esp32_sdl.cpp` reste présent **uniquement comme oracle source pour tests hôte** : pas de compilation ESP32 ; les alias `SDL_Texture` / `SDL_Rect`, `SDL.h` et certaines API de compatibilité de noms utilisées dans les autres fichiers **restent en source**. La suppression du TU ne signifie pas que tous les headers ou identifiants SDL sont retirés. Aucune lecture ZIP runtime, seconde image frame map-wide ou nouvelle allocation.

## Observations matérielles

- **Boot** : `[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes` puis sortie x2, `[TFT] Ready`, `[NATIVECONFIG] LOAD ... valid` (luminosité 100, orientation NORMAL, thème 0), 241 entrées PAK, objet Game desktop nul. **Aucun `[SDL] CYD display attached`** : effet direct attendu du retrait du raccord, distinct de l'absence de symboles liée prouvée par CI.
- **Menu et HELP** : OPTIONS puis retour direct MAIN, HELP **112 lignes** paginé `0→7→14→21→28→35→42→49`, fond/preview depuis PAK, retour `MAINOPAQUE finalFNV=1945fafa`, touch réarmé. Pas de changement d'orientation ou de sauvegarde de config pendant ce test (contrairement à SDL-F2).
- **Intro + première session** : neuf images `NATIVEIMAGE DECODE/ADOPT` indexées **4-bpp**, intro en 7 taps puis libération complète `INTRODISP ... recovered=34112` ; `/intro.bsp` CRC `623f34e4`, arène immutable `14095 B`, `ENGINESESSION FRESH-VISIBLE`, gameplay Entrance. Affichage, animations de déplacement/rotation, destruction/ramassage, source PAK maintenus.
- **Interaction** : **7 `[DIALOG] OPEN` (dont 1 OPEN-STANDALONE), 7 CLOSE**, 7 `DOORANIM COMPLETE` (incluant fermetures différées), **1 `CRATE COMMIT`**, **14 `PLAYERRES COMMIT`**, 14 miniatures START et 13 END pendant la capture : la dernière est déclenchée peu avant le dernier passage dans HUB/EXIT et ne génère pas de END observé. Ne pas prétendre 14/14 END ; sortie réussie via teardown. **1 `MONSTERRETAL COMMIT`** Malwrath sprite 322 à 3 coups.
- **Cycles** : HUB → EXIT confirmé, `RESIDENTRESET released=18008 empty=1` puis `SYSEXIT MENU-READY frame=1945fafa`, LOAD checkpoint **V11 slot04 5604 B** avec restaurations de script, lignes, overlays, automap, monstres, positions/activation, `ENGINESESSION RESUME-VISIBLE`, gameplay et deuxième EXIT confirmé → **2e reset vide et 2e menu ready**, dernier `ALIVE` sans perte d'état menu.
- **RAM** : 2× `RAMBUDGET RESERVE ... HEADROOM_OK` avec marges **heap8=28428/27616 B** et **DMA=69388/68576 B**. `largest8=61428` en session active, menu final `heap8=130896 largest8=86004`. Invariants `shapeData=0x0` et `mediaTexels=0x0` constatés **13 fois chacun** aux frontières.
- **Diagnostics / absence d'erreurs** : aucun `panic`, `Guru Meditation`, `WDT`, `abort` dans le log ; **7× `NATIVEFRAME BOUNDARY-INLINE`**, connus et non fatals, causalité F3 non établie. Pas de tests matériels déclenchés pour OOM/fatal, autres formats BMP 1/8-bpp, orientation inverse ou benchmark FPS A/B.

## Verdict et suite

**PASS matériel ciblé SDL-F3**, source-hôte SDL complètement hors de la compilation firmware, sans changement du présentateur, du contenu PAK, ni du comportement d'entrée / sortie observé. Test strict d'un candidat déjà certifié CI ; **après réception de ce Serial, uniquement clôture docs-only**. Autorisation explicite utilisateur de fusionner. Prochaine frontière : auditer puis retirer par petits lots les types/headers SDL encore référencés par les sources natives. Ne pas retirer l'oracle hôte tant que les tests de parité en dépendent.
