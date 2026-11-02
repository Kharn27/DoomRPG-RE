# PR #246B — quatre comptes BSP desktop hors Render_t natif (CANDIDAT)

## Base et état de A

Base main GitHub vérifiée 3142432f5b6870c5a8579a5cd293f82e39f0fa8f, unique branche agent/esp32-render-retire-legacy-map-header et PR #246. A source 23f57b9758c47990abc1b5df4347ccc6ba0d7daa, préhardware 37539e5c98fca6f359906a9d7c47b4b4720a6495 ; CI [push SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41492050151) et [PR SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41492107380). **A PASS réel classic CYD normal**, log utilisateur collé le 2 novembre 2026. Render=1256, CORE=2224 B, 0 PSRAM, shapeData/mediaTexels NULL ; heap8 menu/START prime/LOAD prime/EXIT=153236/112956/112144/158080 B, largest8 jeu/menu=86004/110580 B. PAK241, arena Entrance 14095/c3882516, FIRST_FRAME=71ca7465. START intro, caisses/pickups/dialogues88/89/79/reprises, portes/CROSS/HUB/EXIT ; LOAD slot04 V11 état exact FNV beaeaccc/50e16fe3/e2a9d90a/df419482, porte113, Malwrath322 11→7→3→0, deux ripostes triples projectile204 drainées avant dégâts, mort/drop, secret+5XP et EXIT. Deux RESIDENTRESET released=18008 empty=1, deux menus 0f2c9fc3 ; HEADROOM_OK, pas de panic/WDT. L'utilisateur indique une bonne nervosité, non une mesure A/B chronométrique causale. Le SHA n'est pas explicitement signé par le firmware mais la taille boot correspond à A.

## B : propriétaire natif et fail-closed

Les quatre champs Render_t nodesLength, linesLength, numMapSprites, mapStringCount viennent du chargeur/libérateur de graphes BSP desktop. Le runtime ESP32 utilise EspMapRuntime compact et ne leur confie plus les comptes de monde. Source B a6d3b621abc61022f70827fca7c5795362c64529 : déclarations conservées sur desktop/DOOMRPG_ESP32_BRINGUP_PROBES uniquement ; les derniers resets dans EspNativeRenderRuntime_releaseCompatibility et le constructeur sont gardés pour bringup. Tous les propriétaires live, géométrie, sprites CROSS #244, PAK, RNG, gameplay, audio, framebuffer et sauvegarde inchangés. Pas de nouveaux buffers ni de ZIP runtime.

Tests hôtes alignés d9719e4db855ab554f232797cbf81d14e281aeae et 8a3683b96fb798d2cd7afa1c857c910171d2a58f : empreinte constructeur C extrait e72704f0, release runtime C extrait 90894947 ; gardes des quatre champs, vrai destructeur et MENU_MAIN START mockés. Static assert sizeof(Render_t)==1240U ; **−16 B de structure et CORE=2208 B attendus**, mais aucune mesure hardware B et aucune promesse de gain de nervosité.

## Test B à réaliser avant PASS

1. Attendre les CI du **dernier HEAD** B (tests hôte, normal esp32-cyd, bringup, trace). Aucune compilation locale revendiquée.
2. Rester sur la branche agent/esp32-render-retire-legacy-map-header ; dans ESP32/ faire git pull --rebase, vérifier git rev-parse HEAD, puis pio run -e esp32-cyd -t clean et pio run -e esp32-cyd -t upload. Capturer le boot Serial complet et la partie.
3. Attendus non mesurés : Engine structs Render=1240 Game=0 Canvas=44 Total=2044, RENDERCORE INIT bytes=1240, CORE used=2208, shapeData/mediaTexels 0, PSRAM0, mappings152/252, PAK241.
4. START intro→Entrance : FNV 71ca7465, arena c3882516, caisse/pickup flash, dialogues, porte275, CROSS, HUB/EXIT. LOAD V11 slot04 : FNV restaurés beaeaccc/50e16fe3/e2a9d90a/df419482, porte113, Malwrath322 11→7→3→0, projectiles204 triple drainés, drop/pickup/secret+5XP et EXIT.
5. Deux RESIDENTRESET released=18008 empty=1, deux SYSEXIT MENU-READY frame=0f2c9fc3 saveWrite=no, HEADROOM_OK, zéro WDT/panic. Comparer heap8 et sensation de nervosité avec le vrai A, sans comparer causalement des timings de parcours différents.
6. Jusqu'au **PASS matériel B**, PR #246 reste Draft. Après PASS : geler le code, documentation seule, CI post-doc/diff contrôlé, Ready for review et merge par l'utilisateur uniquement.

NATIVEFRAME BOUNDARY-INLINE et positions x décalées dans les logs projectile sont préexistants. Audio, automap-open, SAVE neuf, toutes cartes, endurance et parité pixel exhaustive hors scope.
