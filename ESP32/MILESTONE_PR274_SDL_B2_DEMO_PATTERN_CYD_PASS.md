# SDL-B2 — Retrait du diagnostic plein écran SDL : PASS matériel classic CYD

**Date : 9 novembre 2026.** Issue [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269), [PR #274](https://github.com/Kharn27/DoomRPG-RE/pull/274), base initiale exacte **`main=392173acc21878dfbbb45fa92100c4ae42d4c34d`** après SDL-B1. **SHA de code de la PR transmis pour le flash : `e0b19cd96bc2a053b86cf2a22cbf2889722bf9e2`.** Le journal matériel ne fournit pas de SHA logiciel embarqué : l'association du firmware flashé à ce commit repose sur le parcours de test utilisateur, pas sur une attestation cryptographique du log.

## Frontière et propriété

Retrait de **`Esp32Sdl_showTestPattern()`**, de ses deux helpers de cercle `SDL_RenderDrawCircle()` et `SDL_RenderDrawFillCircle()` dans `ESP32/src/esp32_sdl.cpp`, et de la déclaration dans `ESP32/include/esp32_sdl_platform.h`. Une référence dormant derrière `#if DOOMRPG_ESP32_SCREEN_DIAGNOSTICS` dans `ESP32/src/main.cpp` est aussi retirée ; ce macro vaut 0 sur le firmware normal. Le direct-TFT diagnostic restant n'est pas modifié. Les éléments supprimés étaient **absents de l'ELF normal**. Le patch de code retire **64 lignes** du TU SDL, ajoute de courts commentaires, et ne modifie aucune primitive SDL encore liée (blit 1/4/8 bpp, textures/palettes, rectangle/ligne, clip, present), le framebuffer RGB565, le PAK, le moteur native gameplay ou l'oracle desktop.

Le test fail-closed `ESP32/test/sdl_dead_demo_pattern_host.py` vérifie **350** sources C/C++ et en-têtes natifs, bloque les déclarations et appels résiduels aux trois symboles, et garde les fonctions SDL actives et les chemins de propriété indexée. Il a d'abord détecté l'appel dormant dans `main.cpp` puis a été conservé strict après correction, sans exception permissive.

## Gate logicielle

CI push [#42539941992](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42539941992) **SUCCESS**, CI pull request [#42539946603](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42539946603) **SUCCESS**. Test SDL-A réel en C++, garde SDL-B1, garde SDL-B2 et tests hôte natifs : PASS. Build normal `esp32-cyd` SUCCESS, compiledb et audits ELF PASS. Builds auxiliaires bringup/trace en CI seulement ; aucun test matériel exigé. **19 symboles SDL toujours liés**, corps symboliques **1548 octets** contre 1547 dans la référence SDL-A historique ; RAM statique suivie inchangée. Le delta `loaded_sections=-300 B` est mesuré contre un **ancien** baseline SDL-A, pas une preuve d'économie imputable à B2 ; pas de gain FPS/RAM/flash revendiqué.

## Gate matérielle — Serial utilisateur

**`device-monitor-261109-013402.log` : 1 432 lignes, 171 117 octets, normal `esp32-cyd`, sans PSRAM**. Démarrage ESP32-D0WD-V3 240 MHz ; **4 Mo flash** ; structures `Render=112 B Game=0 Canvas=44 Total=916 B` (incluant le retrait Render #272 sur `main`). Framebuffer logique `160×120 RGB565` 38 400 octets, affichage 320×240 nearest x2. Sept entrées `[SDL] Adopt packed indexed texture` 4 bpp, ownership compact actif.

Séquence réellement constatée :
1. Menu principal → OPTIONS → VIDEO → THEME et changement de thème `1→0` persisté `/DoomRPG-ESP32.cfg`, BACK → START → intro native complète (`FINAL-CONTINUE`), Entrance.
2. Déplacements/rotations/strafe, destructions de caisses et pickups, portes `275` ouvertes/fermées et animations bornées, dialogues **opcode 8** events 88 et 89 avec resume **opcode 19**, dialogue d'entrée **opcode 26** event79, aide de première obtention d'arme, HUB puis EXIT propre.
3. MENU → LOAD V11 `slot04` (5604 B) : ressources/états scripts/lignes/crates/monstres/position/topologie/activation restaurés, Entrance reprise, portes `113`, Malwrath `sprite=322`. Deux probes de riposte triple projectile `logical=204` terminés via `[MALWRATHPROJECTILE] CLEAR` et `[MONSTERRETAL] COMMIT`, trois coups joueur `hp=11→7→5→0`, mort ennemie/unlink, dégâts visuels. Exploration/loot et second HUB → EXIT propre.
4. Le log **ne montre ni ouverture HELP ni mort du joueur / RETRY** sur ce run : ne pas prétendre ces chemins testés sous SDL-B2.

**RAM et nettoyages :** **2× `[RAMBUDGET] ... advisory=HEADROOM_OK`**, marges heap8 **51 836 et 51 024 B** au-delà de `total8Target=57 344`; `largest8=86 004` en jeu ; **2× `[RESIDENTRESET] ... released=18008 ... empty=1`**, **2× `[SYSEXIT] MENU-READY ... resident=empty`**, frame FNV `1945fafa` à chaque sortie, heap8 menu **154 304 B**, dernier `[ALIVE]` stable à 122 243 ms. `shapeData=0x0`, `mediaTexels=0x0` sur tous les témoins ; aucune panic, WDT, abort, erreur ou OOM détectée. **Quatre `[NATIVEFRAME] BOUNDARY-INLINE`** diagnostiques connus dans le log ; aucune attribution à B2 ni validation globale pixel-perfect.

## Verdict

**PASS CYD normal SDL-B2.** Aucun changement du code exécutable après le test ; clôture uniquement documentaire. Utilisateur a donné l'autorisation explicite de documenter, fusionner puis continuer. Ce PASS prouve les séquences ci-dessus, **pas** un gain mémoire ni la parité de chaque pixel, et **pas** un test matériel bringup/trace. Le retrait SDL-B se poursuit par petits sous-jalons sur une branche neuve dérivée du SHA réel de `main` après fusion.
