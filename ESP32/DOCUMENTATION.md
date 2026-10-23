# Doom RPG ESP32 — guide de travail

## PR #227 / Milestone D — Solid BG Render inutilisée retirée : PASS CYD (23 octobre 2026)

**Code firmware normal `esp32-cyd` matériellement testé : `1c480682a3a2fb5e31e87c1b83bcfb7b12ff8c6a`**, branche `agent/esp32-retire-solid-bg-render-abi`, [PR #227](https://github.com/Kharn27/DoomRPG-RE/pull/227), à partir de `main=89b7fa27732cababdbe83f8d89ba1ccf649bae4a` (merge PR #226). [CI push](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40020264072) et [CI PR](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40020315456) **SUCCESS**, tests différentiels et des ABI retirées PASS, trois firmwares compilés. Le code testé est gelé ; seules les docs peuvent changer avant la clôture.

**Contrat permanent :** `Render_renderFloorAndCeilingSolidBG` était une fonction non liée au firmware malgré sa définition ESP32 dans `esp_render_geometry_primitives.c`. Son corps natif a été retiré, son prototype `src/Render.h` est à présent desktop-only, son vrai corps `src/Render.c` est préservé comme oracle. Le gardien `native_render_raster_abi_retirement_host.py` interdit sa résurrection aux côtés des quatre raster ABI retirées par #226. Le host différentiel `render_geometry_host.py/.c` exerce toujours les **sept primitives géométriques encore actives** sur 10 000 cas seedés et les quatre orientations. **Ne pas retirer ni changer directement ces sept fonctions :** les renderers world/BSP/sprite utilisent encore leurs ABI `Render_t` ; leur désengagement exigera un nouveau contrat de frame-scratch et des tests exacts. Les couches compactes PAK, `EspMapRuntime`, plans natifs, rendu, monster-turn, dialogue, framebuffer 38 400 B, RNG et mémoire ne changent pas dans cette PR.

**Témoins Serial fournis, vrai classic CYD :** START depuis `MENU_MAIN` armée `fec57f30`, PlayerState fresh `e745fce9`, première intro `ade0195d`, `INTRODISP recovered=34056` ; Entrance `/intro.bsp` CRC `623f34e4`, arène `14095/c3882516`, première world frame `71ca7465`. Déplacements jusqu'à tile839, Armor Shard 0→4, dialogue `event82 opcode8` et continuation `opcode19 stateMutation=1`. EXIT sans SAVE, `RESIDENTRESET released=18008 empty=1`, MENU final `0f2c9fc3`, heap8 `159704/largest 110580`.

Deuxième parcours LOAD réel V11 slot07 (5604 B), `81` ressources consommées et 30 monstres restaurés, activation `22→24`, porte160 animée 4 frames, riposte après animation HP joueur `30→27` / armure `10→8` ; tir au pistolet sprite7 HP `6→3` / armure `4→2`, balles `38→37`, `gameplayRng=untouched` pour HITFX ; heap8 gameplay `113768/largest 86004` stable sur cinq `ALIVE`. Second EXIT : `RESIDENTRESET released=18008 empty=1`, MENU `0f2c9fc3`, heap8 `159704/largest 110580` stable sur deux `ALIVE`, toujours sans SAVE. `shapeData=mediaTexels=NULL` à toutes les frontières observées.

**Interprétation et limites :** PASS ciblé sans panic/reboot ni anomalie RNG observée. `NATIVEFRAME BOUNDARY-INLINE` est connu. `DIALOGCHAIN activeAtTeardown=1` marque un journal de reprise encore actif au reset, **libéré avec `recovered=1144`**, et ne démontre ni fuite ni rollback fatal exact. Les FNV des frames de cache ou de fin d'animation ne sont pas garanties cross-boot. Pas de SAVE nouvellement écrit, autres cartes ou fatal teardown testés. Après PASS, **aucune modification au C testé** ; commits docs-only, CI verte, PR merge-ready, merge par l'utilisateur.

---

## PR #226 / Milestone C — Derniers rejets raster Render retirés, PASS CYD (23 octobre 2026)

**SHA du code réellement testé sur classic CYD, firmware normal `esp32-cyd` : `71244d1ca94a56c1eb01cecf75c192de95a9c9dc`**, branche `agent/esp32-retire-legacy-plane-test-abi`, [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226), base PR `main=a7bfffca4f13abd4cbdf5844615ab5559930fb30`. CI de ce SHA [push #39844070050](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39844070050) et [PR #39844080187](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39844080187) **SUCCESS**, incluant tests host et trois builds. Budgets RAM/flash inchangés : normal **49536/825497 B**, bringup **49688/846301 B**, trace **49584/842265 B**. Aucun build local ni test matériel non fourni par l'utilisateur n'est revendiqué.

**Contrat permanent** : `Render_render`, `Render_renderFloorAndCeilingBG`, `Render_drawplane`, `Render_spanPlane` sont des API desktop de référence **absentes de l'ABI ESP32**. Leurs prototypes restent sous `#ifndef DOOMRPG_ESP32` dans `src/Render.h` ; les implémentations d'origine demeurent dans `src/Render.c` uniquement desktop. Le fichier **`ESP32/src/esp_legacy_render_reject.c` n'existe plus** et ne doit jamais revenir : `ESP32/scripts/build_engine.py` bloque sa résurrection pour tout profil. Le test `ESP32/test/native_render_raster_abi_retirement_host.py`, lancé par `.github/workflows/esp32-cyd.yml`, vérifie desktop oracle, gardes des prototypes, absence de références dans toutes les sources et headers natifs, vrai propriétaire `esp_render_geometry_primitives.c` intact. Les anciens tests plane-`*_Test` et BSP/map vérifient eux aussi l'absence du TU. **Ne pas rebrancher les chemins de rendu legacy ni leurs allocations map-wide ; `EspMapRuntime`, planes PAK-native, framebuffer partagé, états mutables compacts restent les propriétaires.** Les véritables primitives géométriques `Render_initColumnScale`, `Render_cullBoundingBox`, projection et clipping n'ont pas été retirées par ce milestone. Le prochain chantier éventuel est de remplacer leurs dépendances résiduelles à `Render_t`, sous un nouveau contrat testable, pas de les supprimer aveuglément.

**Serial physique START→EXIT** : boot sans PSRAM, PAK 241, framebuffer 160×120/38400 B, Render 1532/Game 0/Canvas 44, MENU `0f2c9fc3`, START `fec57f30`, intro première frame `ade0195d`, dispose +34056 B ; Entrance CRC `623f34e4`, arena **14095/`c3882516`**, FIRST_FRAME `71ca7465`. Deux Armor Shards `0→4→8`, porte275 animée quatre frames puis fermeture différée, dialogue **event79 opcode26** puis `DIALOGCHAIN RESUME opcode19` et medkit. `DIALOGCHAIN OWNER-RELEASE activeAtTeardown=1 recovered=1144` signale journal de continuation encore actif au reset mais mémoire récupérée, `RESIDENTRESET released=18008 empty=1`, menu final `0f2c9fc3`, heap8 **159704/largest 110580** stable. `NATIVEFRAME BOUNDARY-INLINE` est un fallback connu, pas une nouvelle assertion de corruption.

**Serial LOAD→combat→EXIT** : slot07 V11 5604 B, 30 monstres/topologie/position/activation/drops exacts restaurés et **22→24** actifs après apparition, porte160, mouvement/strafe, contrôles armes `1→2→3→2`. Monstre sprite7 attaque, rendu d'attaque terminé avant riposte `MONSTERRETAL COMMIT`, HP joueur **30→27** et armure **10→8** ; riposte au pistolet `weapon=2` avec coup réussi sur monstre HP **6→3**, armure **4→2**, ammo **38→37**, `HITFX gameplayRng=untouched`. Heap8 gameplay **113768/largest 86004** stable sur plusieurs `ALIVE` jusqu'à 183 s. **Second EXIT après LOAD confirmé**, `RESIDENTRESET released=18008 empty=1`, MENU FNV `0f2c9fc3`, heap8 menu **159704/largest 110580** jusqu'à 188 s, `saveWrite=no`.

**Frontière de preuve** : PASS du retrait raster et des deux retours menu ; aucune nouvelle écriture SAVE, aucune autre BSP, aucun fatal teardown testés. Ne pas comparer la FNV de la dernière frame animée de l'intro entre deux cadences tactiles différentes. Après ce PASS, **code `71244d1c` gelé, commits docs-only et CI verte avant merge-ready** ; ne jamais merger automatiquement dans main.

---

## PR #226 — Retrait des ABI Render BSP/map/event : contrat et PASS CYD (22 octobre 2026)

**Code exact testé** : `10ff0b4b7b75095f7e12fa1d0fc0bdf15c18d4df`, normal `esp32-cyd`, [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226), branch `agent/esp32-retire-legacy-plane-test-abi`. CI [push #39688241930](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39688241930), [PR #39688246727](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39688246727) SUCCESS. Statique normal RAM/flash 49536/825497 B, bringup 49688/846301 B, trace 49584/842265 B.

**Contrat** : `Render_relinkSprite`, `Render_renderBSPNoclip`, `Render_findEventIndex`, `Render_beginLoadMap`, `Render_beginLoadMapData`, `Render_renderBSP`, `Render_walkNode` ne sont plus des ABI ESP32 : ni stub de rejet ni prototype natif visible. `src/Render.c` reste l'oracle desktop, `Render.h` garde ses prototypes uniquement desktop. Gardien CI `ESP32/test/native_render_bsp_abi_retirement_host.py`. Les 4 stubs de rendu `Render_render`, `Render_renderFloorAndCeilingBG`, `Render_drawplane`, `Render_spanPlane` restent inchangés, à auditer dans un prochain jalon. Les cartes, événements, rendu et PAK demeurent natifs compacts sans `shapeData` ni `mediaTexels`.

**Preuves matérielles** : BOOT/MENU/START/INTRO, 2 Armor Shards, porte 275 et dialogue op8 annulé, puis HUB EXIT sans SAVE, résident vide. FNV MENU `0f2c9fc3`, intro `ade0195d`, Entrance CRC `623f34e4`, arena `c3882516`, premier monde `71ca7465`, dispose 34056 B, resident reset 18008 B, heap menu final 159704/110580 stable. LOAD V11 slot07 5604 B, 30 monstres restaurés, 22→24 activations, porte 160, séquence riposte animée + commit HP 30→27→24, armure 10→8→6. Tir arme1 extincteur sans touche, `No effect!`, ammo 12→11. Heap8 pendant combat 113768/largest 86004 stable sur cinq ALIVE. Aucune erreur visible. `NATIVEFRAME BOUNDARY-INLINE` déjà présent auparavant.

**Limites du PASS** : pas de second EXIT depuis la session LOAD, pas de SAVE nouvelle écrite, autres maps, ni fatal teardown. Le code `10ff0b4b` est gelé après cette preuve ; commits post-test docs-only, CI à valider avant merge-ready, merge réservé à l'utilisateur.

---

## PR #226 — Entrées Render planes *_Test retirées : contrat et PASS CYD (22 octobre 2026)

**Code testé sur firmware normal `esp32-cyd` : `ba758095e82e594c232c410f737e005ccae518fc`**, [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226) sur `agent/esp32-retire-legacy-plane-test-abi`, base `main=a7bfffca4f13abd4cbdf5844615ab5559930fb30`. [CI push #39686642437](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39686642437) et [PR #39686742624](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39686742624) **SUCCESS**, budgets statiques inchangés **49 536/825 497 B** (normal), **49 688/846 301 B** (bringup), **49 584/842 265 B** (trace).

**Contrat permanent** : les trois fonctions de test desktop `Render_renderFloorAndCeilingBG_Test`, `Render_drawPlane_Test`, `Render_spanPlane_Test` ne doivent plus être des ABI ESP32 : ni stub `esp_legacy_render_reject.c`, ni prototype visible sous `DOOMRPG_ESP32`, ni référence dans les sources natives. `Render.h` garde les prototypes desktop seulement et `src/Render.c` garde les implémentations d'origine comme oracle. Les plans du monde sont toujours rendus via les propriétaires natifs compacts et le PAK ; **ne pas réintroduire `mediaTexels` map-wide ni utiliser les entrées *_Test comme raccourci**. La CI exécute `native_render_plane_test_retirement_host.py` pour bloquer toute réapparition. Aucun remplacement fonctionnel, changement gameplay, allocation framebuffer, RNG, asset source, cadence de `present()` ou mutation World.

**Preuve Serial reçue** : CYD réel sans PSRAM, PAK 241 entrées, framebuffer 160×120 RGB565 / 38 400 B partagé ; `Game=0`, `Canvas exports=0`, `Render=1532`. MENU_MAIN `0f2c9fc3`, START `fec57f30`, INTRO initial `ade0195d`, dispose `34056 B` sans changer le framebuffer ; Entrance CRC `623f34e4`, arena **14095 / `c3882516`**, `FIRST_FRAME=71ca7465`, `FRESH-VISIBLE` ; deux Armor Shards (0→4→8), porte 275 animée, déplacement jusqu'au dialogue opcode26 événement79, continuation native et Sm Medkit. `ALIVE heap8=113768 largest8=86004` stable avant dialogue ; `DIALOGCHAIN` journal de **1128 B** libéré à EXIT (`activeAtTeardown=1` = journal transactionnel encore actif, détruit au reset), `RESIDENTRESET released=18008 empty=1`, retour MENU_MAIN FNV `0f2c9fc3`, puis quatre `ALIVE heap8=159704 largest8=110580` sans dérive observée.

**Limites du PASS** : pas de test nouveau des LOAD/SAVE V11, combat, mort joueur, reboot, fatal root teardown, rollback sur échec de rendu, ni preuve exhaustive multi-session ou pixel par pixel. `BOUNDARY-INLINE` déjà connu et variation des états RNG/cache ne sont pas des régressions attribuables ici. Le **SHA de code hardware est gelé** ; seuls les documents sont modifiés après les logs, et la PR ne devient merge-ready qu'après CI docs-only verte. L'utilisateur effectue lui-même le merge.

---

## PR #225 — Direct mappings PAK natifs : validation CYD et contrat permanent (22 octobre 2026)

**Code gelé réellement testé, normal `esp32-cyd` : `d284329b937204452ceb9d5f2d7b26a37fc99f72`** sur `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), base main `61bf1519730af5d81aeef5faeeba31bed210bfac`. CI pour ce SHA : [push #39682532690 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39682532690), [PR #39682540173 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39682540173). Budgets : normal `RAM=49536 / flash=825497`, bringup `49688 / 846301`, trace `49584 / 842265` octets.

**Contrat logiciel permanent** : `Render_loadMappings(Render_t*)` est retiré de l'ABI ESP32. Son adaptateur ne faisait que transmettre à `EspLegacyMappings_load`. CONFIGMAP appelle directement le vrai chargeur natif : `mappingsResult = EspLegacyMappings_load(render) ? true : false;` ; `src/Render.h` déclare `Render_loadMappings` uniquement sur desktop. Les tableaux persistants `mediaTexelOffsets`, `mediaBitShapeOffsets`, `mediaTexturesIds` et `mediaSpriteIds` conservent leurs plans de mémoire et données. Aucun `mediaTexels` monolithique, `shapeData`, backing ZIP runtime ou décompression carte globale. Source du parseur `EspLegacyMappings_load` **FNV `da45ea1f`** préservée, test host de **4096 cycles** du vrai C startup (succès/réutilisation/fail-closed), guard native ABI du build, audit du test desktop-keybinding recentré sur le propriétaire natif. La source desktop reste l'oracle de comportement, pas un moteur compilé ESP32.

**Témoins matériel Serial `d284329b`** : SD 240 MiB, PAK 241 entrées ; palette 3280 entrées / 6560 B, framebuffer logique partagé 38400 B ; `[MAPPINGS] -> EspLegacyMappings_load()` et `result=1`, `mappings.bin=8392`, `INSTALLED payload=8376`, en-tête `592/1300/152/252`, heap8 `163300→154860` (`used=8440`), `scratch=framebuffer noInflatedHeap=yes`. MENU_MAIN FNV **`0f2c9fc3`**, START armed **`fec57f30`** ; first INTRO **`ade0195d`**, `INTRODISP recovered=34056`; carte Entrance source CRC **`623f34e4`** et arena **14095/c3882516**, `FIRST_FRAME=71ca7465`, `FRESH-VISIBLE/READY` ; navigation/rotation, deux pickups armure **0→4→8**, porte **275** ouverte en 4 frames et franchie ; `ALIVE heap8=113768 largest8=86004` en gameplay ; HUB System EXIT `saveWrite=no`, `RESIDENTRESET released=18008 empty=1`, menu final FNV **`0f2c9fc3`**, heap8 **159704 largest8=110580**.

**Limites explicites** : ce jalon valide uniquement BOOT/START/INTRO/gameplay/EXIT, pas LOAD V11, SAVE, combats, ni le teardown fatal de `DoomRPG_FreeAppData`. Les trames warm-cache peuvent avoir des FNV distincts sans altérer les témoins fixés. Le retrait du wrapper ne modifie pas le corps du parseur PAK ni le gameplay/RNG. Les commits suivant le PASS sont **docs-only** ; PR à rendre prête au merge une fois CI docs-only verte, sans merge dans `main` par l'agent.

---

## PR #225 — Render_setGrayPalettes retiré : PASS CYD ciblé (21 octobre 2026)

**SHA de code firmware normal testé sur classic CYD : `3acd358e15a13430e8aa991f2797da73d75ac91f`**, branche `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225). [CI push #39677462114](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39677462114) et [PR #39677472767](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39677472767) **SUCCESS**. Statique normal **49 536 B RAM / 825 509 B flash**, bringup **49 688 / 846 317 B**, trace **49 584 / 842 261 B**.

**Contrat permanent** : suppression de la fonction desktop `Render_setGrayPalettes` du bridge natif ESP32. Son ancien comportement mutait `mediaPalettes` et les couleurs floor/ceiling (et les `mediaPlanes` desktop), donc ne doit jamais être réintroduit comme mutation implicite des buffers carte. Aucun usage dans les sources/headers ESP32 d'après le test `native_render_gray_retirement_host.py` qui inspecte les références non commentées ; l'API desktop dans `src/Render.h` reste disponible uniquement côté non-ESP32 et le vrai corps `src/Render.c` est inchangé, FNV **`6ff4291e`**. Tout nouvel effet grayscale natif doit être un autre jalon, avec propriétaire explicite et buffers bornés.

**Serial normal, ce SHA** : BOOT `PSRAM=0`, PAK **241 entrées**, palette `3280/6560 B`, RGB565 framebuffer partagé **38 400 B** ; `RENDERSTART used=6576 heap8=163300`; MENU_MAIN FNV **`0f2c9fc3`**, `ALIVE heap8=154860 largest8=110580` sans dérive sur échantillons jusqu'à 180 s. START `fec57f30`, `[MAINMENU] Runtime cleanup gained=8440`; première frame INTRO **`ade0195d`**, `INTRODISP recovered=34056`; `/intro.bsp` CRC `623f34e4` arena **14095/c3882516**, `FIRST_FRAME=71ca7465` ; session `FRESH-VISIBLE/READY`, MOVE `904→872→840`, TURN `64→128`, MOVE `840→839→838` ; armure ramassée `0→4→8`, porte **275** ouverte par SELECT avec 4 frames d'animation. Gameplay `ALIVE heap8=113768 largest8=86004`, `shapeData/mediaTexels=0`; HUB System EXIT sans SAVE, `RESIDENTRESET released=18008 empty=1`, cleanup `gained=0`, menu FNV **`0f2c9fc3`**, `ALIVE heap8=159704 largest8=110580` après retour.

**Ne pas surinterpréter** : `LARGE-LEARN-HOT` durant warm cache, frames d'entraînement cache non constantes et `BOUNDARY-INLINE` déjà connu n'invalident pas ces checkpoints ; absence de perte mémoire observée uniquement sur fenêtres transmises. Pas de LOAD V11, SAVE neuf, combat, teardown fatal du root ni preuve photo des couleurs dans ces logs. **PASS de non-régression hardware de ce périmètre seulement**. Le SHA `3acd358e` reste gelé ; docs-only après PASS ; pour continuer sur cette branche, nouveau SHA de code, CI complète puis test matériel nouveau. Aucun merge automatique.

---

## PR #225 — Palette Render native privée : PASS CYD (21 octobre 2026)

**SHA firmware normal réellement testé : `2080e250a0c60d81d3304988d8c89ea9fd328f7b`**, sur `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225). [CI push #39675709558](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39675709558) et [CI PR #39675715983](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39675715983) **SUCCESS**. Normal `esp32-cyd` **49 536 B RAM / 825 517 B flash**, bringup **49 688 / 846 321 B**, trace **49 584 / 842 269 B**. Nouvelle API privée `static EspNativeRenderStartup_loadPalettes` appelée uniquement par `EspNativeRenderStartup_initialize`, ancienne ABI `Render_loadPalettes` exclue côté ESP32 ; fonction source originale conservée à l'octet près hormis déclaration, FNV normalisé **`3573ad2c`**, tests C **4096 cycles** avec pixels RGB565/replacement et `paletteMemory`, startup FNV normalisé **`f6343a76`**. Le loader reste PAK, pas de réouverture du ZIP.

**Hardware logs de ce SHA** : classic CYD ESP32-D0WD-V3 **sans PSRAM**, `PAK entries=241`, `/palettes.bin=6564`, `[RENDER] palettes loaded entries=3280 bytes=6560`, framebuffer `160x120 RGB565 38400` partagé, `RENDERSTART result=1 used=6576 heap8=163300`. MENU_MAIN FNV **`0f2c9fc3`** et `ALIVE heap8=154860 largest8=110580` répété jusqu'à 125s. START initial `fec57f30`, `[MAINMENU] Runtime cleanup gained=8440` sans champs legacy ; intro `ade0195d` puis disposition `recovered=34056` ; `/intro.bsp` CRC `623f34e4` arena `14095/c3882516`, première frame mondiale **`71ca7465`**, HUD 30/30 ; mouvements `904→872→840`, rotation `64→128`, déplacements `840→839→838`, pickup armure **0→4→8** ; gameplay `heap8=113768 largest8=86004`. HUB/SYS EXIT sans SAVE : `RESIDENTRESET released=18008 empty=1`, cleanup `gained=0`, menu FNV **`0f2c9fc3`**, heap après EXIT **159704**, largest8 **110580**. `shapeData/mediaTexels=0` aux frontières.

**Portée exacte** : PASS de non-régression boot, palettes, menu, intro, première scène, gameplay pickups et EXIT. Non couverts sur ce code : LOAD après EXIT, SAVE neuf, combat, teardown fatal root, preuve photographique de pixels ou recherche exhaustive de fuites. La diversité des graines RNG entre boots ne prouve pas une divergence. Code matériel `2080e250` gelé, clôture docs-only ; un jalon ultérieur sur la même branche impose CI et nouveau test CYD. Pas de merge automatique.

---

## PR #225 — Render_freeRuntime retiré, cleanup natif à deux appelants : PASS CYD (21 octobre 2026)

**Nouveau SHA réellement testé en firmware normal `esp32-cyd` : `3b551a6d0c17f28b392fe04675f5f428b4d545a8`**, branche `agent/esp32-retire-render-free-abi` ([PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), base main `61bf1519730af5d81aeef5faeeba31bed210bfac`). [Push CI #39650761704](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39650761704) et [PR CI #39650771777](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39650771777) **SUCCESS**, trois environnements : RAM/flash normal **49 536/825 529 B**, bringup **49 688/846 341 B**, trace **49 584/842 281 B**.

**API retenue** : `Render_freeRuntime` n'est plus un export ESP32, remplacé par `EspNativeRenderRuntime_releaseCompatibility(Render_t*)` pour **deux cycles de vie** : `DoomRPG_esp32ReleaseMainMenuMemory` (START) et `EspNativeEngineRoot_releaseRender` (teardown fatal). Son nettoyage des slots compatibilité `Render` conserve strictement l'ordre de `SDL_free` et des affectations NULL, sans gérer les compact immutable map arenas ni le framebuffer partagé. La première tentative strictement privée du root (`4b6c93d`) a été corrigée après échec CI : START est un appelant indispensable. FNV de parité `8042d34c` (`Render_freeRuntime`) et `7e988863` (`Render_free`) ; tests C sur les vrais corps **4096 cycles START**, **4096 cycles destructeur**, plus failure guards, partiel/complet/shared/owned/freePtr.

**Serial CYD exact du nouveau code** : MENU `fec57f30`, `[MAINMENU] Runtime cleanup ... gained=8440 heap8=154860->163300`, puis intro `ade0195d` et `INTRODISP recovered=34056` ; Entrance `CRC 623f34e4`, arena `14095/c3882516`, `FRESH-VISIBLE/READY`, ramassage Armor Shards `armor 0→4→8`; premier HUB/System/EXIT `RESIDENTRESET released=18008 empty=1`, `[MAINMENU] ... gained=0`, MENU `0f2c9fc3` ; premier LOAD slot07 V11/5604 B `world=...restored-exact`, `MONSTERSTATE RESTORE rngCalls=0`, 22 monstres activés, `RESUME-VISIBLE/READY` ; PASS_TURN, porte160 quatre frames, activation 22→24, attaque du sprite7 avec `MONSTERATKVIS COMPLETE` puis `MONSTERRETAL COMMIT HP30→27 armor10→8` et `DAMAGECUE DONE delayed=no` ; second HUB/System/EXIT, `released=18008`, `gained=0` et menu FNV `0f2c9fc3`. Heap de gameplay **113 768 B**, largest8 **86 004 B** stable dans les `ALIVE` transmis ; après chaque EXIT **159 704 B**, largest8 **110 580 B**. `shapeData/mediaTexels=0` aux frontières.

**Limites / prochain milestone** : il s'agit d'un **PASS de régression sur matériel**, pas d'un test du teardown fatal `DoomRPG_FreeAppData` sur CYD (preuve hôte seulement) ; pas de nouveau SAVE, reboot, mort joueur, ni garantie de fuite universelle. Le SHA testé `3b551a6` doit rester immuable ; même branche possible pour un futur jalon, mais **nouveau SHA = nouvelle CI + nouveau hardware test**. Le PASS précédent de `Render_free` concernait `dfd77e6` et est conservé ci-dessous. Clôture uniquement documentaire, pas de merge automatique.

---

## PR #225 — Render_free devenu destructeur privé du root : PASS CYD ciblé (21 octobre 2026)

**Firmware normal testé sur le classic CYD : `dfd77e63629a0a457ffe693d28db140cf30d2240`** ([PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), base main exact `61bf1519730af5d81aeef5faeeba31bed210bfac`). [CI push #39554989115](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39554989115) et [CI PR #39555113606](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39555113606) **SUCCESS**. RAM/flash normal **49 536/825 529 B**, bringup **49 688/846 341 B**, trace **49 584/842 281 B**. Le code du destructeur `Render_free` a été déplacé intact, renommé `static EspNativeEngineRoot_releaseRender` dans `engine_stubs.c`. `DoomRPG_FreeAppData` l'appelle à la destruction fatale du root. `Render_freeRuntime` reste pour le prochain jalon ; `Render_free` desktop n'est ni déclaré ni exporté côté ESP32. La protection contre le `free` du framebuffer RGB565 partagé, l'ordre de destruction et `freePtr` restent identiques : fingerprint C normalisé **`7e988863`**, 4096 cycles hôte shared/owned FB, complete/partial, freePtr oui/non et test root 4096 cycles. Les chaînes `[RENDERFREE]` historiques ne signifient pas un export.

**Test matériel transmis** : START/intro `ade0195d` et disposition `34056` B ; `/intro.bsp` `CRC 623f34e4`, arena `14095/c3882516`, `MAPFLASH REUSE HIT` ; MOVE `904→872`, rotation, strafe, puis pickups Armor Shard `0→4→8` ; HUB System EXIT sans SAVE et `RESIDENTRESET released=18008 empty=1` ; menu revenu à `0f2c9fc3`, `heap8=159704 largest8=110580` ; premier MENU_MAIN LOAD → slot07 V11/5604 B, restaurations monde/spatial/monstres exactes, 22 actifs, `MONSTERSTATE RESTORE rngCalls=0`, `RESUME-VISIBLE/READY` ; MOVE/STRAFE, porte160, activation de deux monstres `22→24`, riposte sprite7 `HP30→27 armor10→8` avec RNG commit après animation, puis tir Pistol `hp6→3 ammo38→37`. `ALIVE heap8=113768 largest8=86004` répété sans dérive en jeu neuf et restauré. `shapeData/mediaTexels=0` aux frontières. `NATIVEFRAME BOUNDARY-INLINE` est le fallback antérieur ; un `COMBAT-INPUT-BLOCK queued=no` pendant l'animation reste la politique d'admission préexistante, non corrigée ici.

**Limites de preuve** : PASS matériel de non-régression ; le root fatal n'a **pas** été invoqué physiquement, sa parité provient du vrai C compilé et simulé côté hôte. Le SYS EXIT normal ne lance pas `Render_free`. Pas de SAVE neuf, reboot, mort, teardown fatal ni preuve universelle d'absence de fuite. Le SHA de code matériel `dfd77e6` est **gelé** ; toute suite sur cette même branche reçoit son propre SHA de code, CI et test matériel, sans héritage implicite de ce PASS. Clôture docs-only, pas de merge automatique.

---

## PR #224 — correctif review P2 initial-WORLD / LOAD : test CYD et CI (20 octobre 2026)

**Référence production la plus récente pour `esp32-cyd`** : `f15c412c757c40eceacd353eec3c3439d33462c2` ([PR #224](https://github.com/Kharn27/DoomRPG-RE/pull/224), main de base `6b1d0c44e90414dc95c8650fbe6ad048c5a70ae1`). [CI push #39463831546](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463831546), [CI PR #39463839288](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463839288) **SUCCESS**. Normal **49 536 B RAM / 825 581 B flash** ; bringup **49 688/846 385**, trace **49 584/842 329**. La review P2 est fondée : durant `SESSION-ARM`, le premier `present()` du monde précède `gameplayState.active=1`. Le premier garde de la PR masquait le compositeur du label ; le lease `firstWorldPresentArmed`, détenu uniquement durant cette présentation, corrige le cas sans activer input/tours en avance ni autoriser le HUD sur MENU/SAVESLOTS. Annulation explicite du lease même si la présentation échoue. Test C de la vraie composition et garde extrait ×4096. Thread Codex traité et résolu.

**Preuves Serial CYD pour le nouveau code** : START → intro FNV `ade0195d` → `INTRODISP recovered=34056` → Entrance `CRC 623f34e4`, arena `14095/c3882516` → `SESSION-ARM` / `FRESH-VISIBLE` / `RESIDENTGAMEPLAY READY fresh-first-frame`; `MAPFLASH REUSE HIT`; puis MOVE `904→872`. SYS EXIT `RESIDENTRESET released=18008 empty=1`, MENU FNV `0f2c9fc3`, `heap8=159704`; premier LOAD selector `SAVESLOTS MAIN-OPEN`, restauration réelle slot04 V11/5604 B `world=...restored-exact`, monstres30, actifs13, `RESTORE rngCalls=0`, `RESUME-VISIBLE/READY` ; PASS_TURN, MOVE `281→282` + Health Vial HP33→34, porte113 animée, activation sprite322, attaque Malwrath 3 salves de projectiles, riposte `HP34→28 armor23→17`, `DAMAGECUE DONE delayed=no`. `heap8=113768 largest8=86004` stable sur fenêtre observée avant/après EXIT et reprise. `shapeData/mediaTexels=NULL`. Les logs n'ont pas de hash du label, donc l'absence de défaut de pixels précis n'est **pas** une preuve issue de Serial seul.

**Bruit des logs** : `[NATIVESPRITE] CROSS` et `TRANSIENT` sont actuellement émis par des `printf` sans niveau dans `esp_native_sprite_renderer.c` par frame ; le bon jalon séparé sera de les déplacer vers `DRPG_LOGT` (normal silencieux, `esp32-cyd-trace` explicite), sans modifier les compteurs, les warnings, ni la sémantique sprite. Éviter de retoucher le code de cette PR après son test. L'observation d'input/latence ~300–400 ms reste un audit à part, sans cause prouvée par les seules lignes CROSS.

**Limites** : label initial sans hash/pixel preuve directe, pas de nouveau SAVE, mort joueur, reboot ni fatal teardown. Code `f15c412c` gelé après test, clôture **docs-only**, l'utilisateur conserve le merge de main.

---

## PR #224 — Render_startup 1→0 + premier LOAD après EXIT : PASS matériel ciblé (20 octobre 2026)

**Dernier code CYD normal testé** : `0686740c1361068f3c96d05eab95e4713b37aa49`, [PR #224](https://github.com/Kharn27/DoomRPG-RE/pull/224), base main `6b1d0c44e90414dc95c8650fbe6ad048c5a70ae1`. [CI push #39343404155](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39343404155) et [PR #39343408609](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39343408609) **SUCCESS**, trois profils et audits ELF. Normal : RAM statique **49 536 B**, flash **825 537 B**. `Render_startup` retiré de l'ABI ESP32, corps privé `EspNativeRenderStartup_initialize` dans `render_startup_bridge.c`, FNV de parité C **`f6343a76`**, hôte 4096 succès et 5 échecs fail-closed. L'ancienne chaîne de log `Render_startup result=1` ne signifie pas un export.

**Correction écran LOAD** : état `facing.dirty` réarmé par reset de session après SYS EXIT provoquait, sur le premier `Esp32PlatformVideo_present` du sélecteur LOAD, un repaint `k.bmp` gris sur les 20 premiers pixels de hauteur via l'ActionEngine gameplay, malgré l'effacement complet 160×120 par SAVESLOTS. La composition gameplay est désormais autorisée seulement quand `EspNativeResidentGameplay_isActive()`, les menus et transitions se présentant directement. Host C vérifie 4096 passages + test réel du wrapper/compositeur de dégâts, sans modifier gameplay/PAK/RNG. Code final `0686740c` = correctif de comportement `98f748b0` et deux commits tests seulement.

**PASS visuel réel** : boot normal (Canvas44, Render1532, Game0), framebuffer 160×120 / palette3280, MENU FNV **`0f2c9fc3`** ; START intro **`ade0195d`**, `INTRODISP recovered=34056`, map Entrance CRC **`623f34e4`**, arena **`14095/c3882516`**, `shapeData=mediaTexels=NULL`, gameplay READY et `heap8=113768 largest8=86004` stable. Mouvements/rotation, Armor Shard `0→4` ; SYS EXIT `released=18008 empty=1`, MENU FNV canonique, `heap8=159704`; **première ouverture LOAD directement après EXIT** `SAVESLOTS MAIN-OPEN` et confirmation utilisateur : **plus de barre grise**. `MAPFLASH REUSE MISS cachedMap=2` explique le chargement initial de 14,67 s dû à une recopie ponctuelle de partition ; ne pas le confondre avec la latence de gameplay à chaque action.

**Non revérifié sur ce SHA** : BACK→LOAD, OPTIONS, LOAD effectif V11 et combats (présents sur le code précédent `82b08e4d`), nouveau SAVE, mort joueur, reboot, teardown fatal. Le retard ressenti **300–400 ms entre actions hors combat** demeure un sujet séparé ; aucun changement de cadence ni de touches ici. Clôture post-PASS **docs-only** ; ne pas merger automatiquement.

---

## PR #223 — Render_setup 1→0 : PASS CYD / observation réactivité (20 octobre 2026)

**Référence matérielle la plus récente** : code `esp32-cyd` **`eb93bf2fefe3aa48cd87b933648d3a84c78f9743`**, [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223), base main `59e5cd90c9360d1b3a08d83d33cd28ed0732e5b1`. [CI push #39338723652](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39338723652) et [PR #39338727944](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39338727944) **SUCCESS** : normal **49 536 B RAM / 825 525 B flash** ; bringup/trace compilent et passent audits. `Render_setup` n'est plus une ABI ESP32 et son corps est privé au layout natif `EspNativeEngineLayout_setupRender`, FNV source `ec9d1151`. Test vrai C hôte **4096** cycles dimensions paires/impaires + reset allocations + trois OOM. Dimensions CYD `160x80@0,20` et trois tableaux 1280 B inchangés, ainsi que Canvas layout FNV `9166e429`.

**Serial normal confirmé** : `[RENDERCORE] SETUP ... arrays=1280B`, `[LAYOUT] READY`, `Render_startup result=1`, menu FNV `0f2c9fc3` ; START → intro `ade0195d` → libération 34056 B → Entrance CRC `623f34e4` / arena `14095/c3882516`, jeu neuf, deux Armor Shards, porte275 ; EXIT sans SAVE → `RESIDENTRESET released=18008 empty=1` → menu original ; LOAD slot07 V11 / 5604 B, 30 monstres, 22 actifs puis 24, `world=...restored-exact`, porte160 animée, ripostes réelles et esquives, Pistol tue sprite6 `hp8→2→0`, Pistol puis Axe tuent sprite7 `hp6→3→0` avec drop `type6 subtype1` et GIBFX. `shapeData/mediaTexels=0` aux frontières. `heap8=113768 largest8=86004` stable pendant jeu neuf et LOAD/combat jusqu'à 144 s observés. Le `NATIVEFRAME BOUNDARY-INLINE` est un fallback historique borné.

**Point de suivi gameplay input/animations** : utilisateur ressent une perte de nervosité et doit retapoter depuis l'ajout des animations ; confirmation concrète `COMBAT-INPUT-BLOCK tap=8/9 queued=no` pendant `MONSTERATKVIS` 500 ms, `DAMAGECUE delayed=yes` sur certains overlays, mouvements ~100–125 ms et rotations ×2. Les événements tactiles en combat bloqué ne sont pas bufferisés : **pas une panne de touch matériel établie**, mais une politique d'input qui peut rendre les commandes perdues perceptibles. Traiter comme jalon ergonomie distinct : audit input ownership et tour/animations avant tout tampon borné, sémantique fail-closed, pas de mutation anticipée ou de RNG déplacé, pas de boucle FPS aveugle. `MENUTOUCH MISS` isolé hors carte ≠ blocage combat.

**Portée** : pas de nouveau SAVE, mort joueur, reboot, fatal teardown ou EXIT après LOAD. Aucun changement de code après PASS : uniquement documentation de ce SHA ; tout milestone suivant demande CI et nouveau test CYD. Pas de merge dans main.

---

## PR #223 — constructeur Render_init 1→0 : PASS matériel (20 octobre 2026)

**Référence matérielle la plus récente** : code normal `esp32-cyd` **`19f52749b18cf60cc14ecbacd2a32e4756aac2b1`**, [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223) sur branche `agent/esp32-retire-final-canvas-free-abi` ; [CI push #39336850874](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39336850874) / [PR #39336855926](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39336855926) **SUCCESS**. RAM statique **49 536 B**, flash **825 581 B**. `Render_init` (constructeur initialement lié dans `render_startup_bridge.c`) appartient maintenant comme `static EspNativeEngineRoot_initRender` à `engine_stubs.c`; corps conservé, FNV source `884791fe`, test C réel ×4096 allocation / pointeur préexistant / échec d'allocation. `Render_t` reste **1532 B**. `Render_init` imprimé sur Serial est une chaîne de compatibilité et ne signifie pas qu'une ABI legacy est revenue.

**CYD observé** : boot `Canvas exports=0`, `Render used=1548`, `Render_startup result=1`, menu `0f2c9fc3`; START intro `ade0195d`, `FINAL-CONTINUE`, récupération **34 056 B** ; Entrance `CRC 623f34e4`, arena `14095/c3882516`, jeu neuf + deux Armor Shards (`0→4→8`). SYS EXIT sans SAVE, reset `released=18008 empty=1`, menu FNV d'origine ; LOAD slot07 V11 5604 B et reprise effective (30 monstres, 22 actifs puis 24) ; mouvements, STRAFE, porte animée/fermée après tour monstre, attaque ennemie esquivée, ripostes `HP30→28→26`, tirs réels et mort sprite6 `hp8→2→0` avec GIBFX, sprite7 blessé. `shapeData/mediaTexels=0` aux frontières, `heap8=113768 largest8=86004` identiques durant jeu neuf et chargement/combat (ALIVE 128–170 s). `NATIVEFRAME BOUNDARY-INLINE` ponctuel = fallback antérieur et borné.

**Portée** : pas de SAVE neuf, mort joueur, reboot, fatal teardown Render/Canvas ni sortie après LOAD dans ces logs ; pas de preuve universelle de non-fuite. Le SHA n'est pas imprimé par firmware. **Code `19f52749` gelé après PASS**. Commits de clôture docs-only puis, si on poursuit sur la même branche, chaque nouveau SHA de code demandera build/CI et preuve CYD indépendants. Pas de merge automatique.

---

## PR #223 — dernier export Canvas retiré : PASS réel / procédure de suivi (20 octobre 2026)

**Code normal `esp32-cyd` matériellement éprouvé** : `201232031e595563a42ca1e0d0f34d0252b1ab07` sur [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223), issue du main exact `59e5cd90c9360d1b3a08d83d33cd28ed0732e5b1`. [CI push #39334948960](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39334948960) et [CI PR #39334955182](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39334955182) **SUCCESS**, normal RAM statique **49 536 B**, flash **825 609 B** ; bringup/trace compilent et passent audits. `[DOOMCANVASBRIDGE] INIT exports=0 nativeText=1 desktopTU=no bytes=44`. L'ancien destructeur compatibilité a été transféré à `EspNativeEngineRoot_releaseCanvas`, ordre story/intro/font/Canvas inchangé. Le raster glyphes reste dans `esp_native_canvas_text.c`, FNV `4c4a9a93`; les gardes hôte empêchent la réintroduction d'un appel `DoomCanvas_free` côté ESP32.

**Parcours réellement vu** : MENU FNV `0f2c9fc3`, Options `0acffb35`, Help/About 83 lignes/pages puis retours exacts ; START → intro `ade0195d` → `FINAL-CONTINUE` → disposer `recovered=34056` → Entrance CRC `623f34e4`, arena `14095/c3882516` → nouveau jeu. HUB SYS EXIT sans sauvegarde, reset résident `released=18008 empty=1`, puis MENU_MAIN LOAD effectif de slot07 V11/5604 B : restore exact, 30 monstres/22 actifs, mouvement, porte et activation à 24. Deux attaques Pistol, ripostes réelles (HP 30→27 puis 27→25), troisième attaque létale contre sprite6 `hp2→0`, sang/GIBFX. `shapeData/mediaTexels=0x0` aux frontières. Boot menu `heap8=154860`, intro `129244`, fresh gameplay `113768`; gameplay chargé `heap8=113768 largest8=86004` stable aux 5 points entre 55 et 76 s. Un `DAMAGECUE TIMEOUT` pendant combat chargé abandonne seulement une cue visuelle, pas la transaction gameplay.

**Précision de couverture** : pas de nouveau SAVE, de mort joueur, de fatal teardown hardware, ni de preuve universelle sans fuite. Le body C du destructeur root a été testé côté host 4096 fois, ce n'est pas une exécution hardware. Le SHA n'est pas inscrit dans le Serial. **Gel du code `201232031e595563a42ca1e0d0f34d0252b1ab07`** ; commit documentaire séparé, puis nouveau jalon de code (et nouveaux tests) possible sur la même `agent/*` avec accord utilisateur. Le PASS ci-dessus ne s'étend pas au code futur. Aucun merge automatique.

---

## PR #222 — Canvas init ABI 2→1 : PASS sur CYD / clôture (20 octobre 2026)

**Dernier code firmware normal `esp32-cyd` réellement éprouvé** : `adb075773c8698fd2cd6fc8a913e07aa00412c21` sur [PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222), [CI push #39332204874 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39332204874), [CI PR #39332212609 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39332212609). RAM **49 536 B**, flash **825 629 B** (−20 B face à 3→2). Sur ESP32, l'allocation du Canvas n'est plus une ABI desktop : `EspNativeEngineRoot_initCanvas` locale à `engine_stubs.c` garde l'allocation, le zéro, les champs par défaut et le diagnostic ; source normalisée verrouillée par FNV **`b621a796`**. `DoomCanvas_free` est **le seul export restant** du bridge Canvas (il contient encore les routines de raster natif privées). **Canvas vaut toujours 44 B**.

**Parcours matériel contrôlé** : `INIT exports=1 bytes=44`, `CORE READY used=2500` ; layout HUD `160x80@0,20`, framebuffer 160×120, MENU FNV `0f2c9fc3`; START → intro `ade0195d`, `FINAL-CONTINUE` puis `INTRODISP recovered=34056` ; Entrance CRC `623f34e4`, arena **14095 B/c3882516**, rendu frais et PlayerState. Deux Armor Shards donnent `armor 0→4→8`. HUB SYS → EXIT (`RESIDENTRESET ... empty=1 saveWrite=no`) → MENU FNV canonique → MENU_MAIN LOAD **slot07 V11/5604 B** réellement restauré (`state=3 intro=skipped`, monde `restored-exact`, 30 monstres / 22 actifs), puis mouvements/portes/activation à **24**. Tir réel sur `sprite7` `MONSTERCOMBAT hp6→3 armor4→2 ammo38→37`, et riposte **après l'animation** `MONSTERRETAL hp30→27 armor10→8 damageTotal=5`, cue de dégâts et feedback clos. `shapeData/mediaTexels=0x0` aux frontières chargement/menu/reprise.

**Mémoire constatée, sans inférence de fuite universelle** : boot MENU `heap8=154860 largest8=110580` aux 5/10/15 s ; intro `129244/98292`; gameplay neuf aux 52, 57, 62, 72 s et gameplay chargé aux 85, 90, 96, 101, 106, 112 s : **`heap8=113768 largest8=86004` constants**. `NATIVEFRAME BOUNDARY-INLINE` isolé est le fallback borné déjà connu ; pas de crash. Pas de SAVE neuf ni de mort sur ce SHA, ni SHA imprimé par le Serial. Les fingerprints et chiffres doivent rester attribués à ce seul code testé.

**Après PASS** : gel strict du code `adb075773c8698fd2cd6fc8a913e07aa00412c21`. Documentation exclusivement après test, CI doc puis revue/merge par l'utilisateur ; **ne pas retirer `DoomCanvas_free` dans cette PR**. Après fusion confirmée, relire le nouveau `main`/SHA et repartir sur une nouvelle branche dédiée au dernier cycle de vie Canvas.

---

## PR #222 — ABI Canvas startup 3→2 : procédure et PASS CYD (19 octobre 2026)

**SHA matériel normal `esp32-cyd`** `5c76e7f73e019200c3296a5567891b21170093df`, [PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222). [CI push SUCCESS #39330881971](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39330881971), [CI PR SUCCESS #39330886161](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39330886161), RAM **49 536 B** et Flash **825 649 B**. `DoomCanvas_startup` est maintenant une **fonction locale du stage `DoomRPG_startEngineLayout`** `EspNativeEngineLayout_startupCanvas`, plus un export du pont Canvas ; seulement **init/free** restent. `Render_setup`/police `a.bmp` et `startupMap/skipIntro` sont inchangés par contrat hôte (FNV `9166e429`).

**Séquence de test effectuée sur le classic CYD** : `INIT exports=2`, layout `160x80@0,20`, menu FNV `0f2c9fc3`; luminosité 25 % réellement restaurée depuis config SD ; START `state=9`, intro FNV `ade0195d`, disposition récupérant **34 056 B** ; Entrance `CRC=623f34e4`, arena 14095B / `c3882516`, gameplay (mouvements et dialogue event88 : `EV_DIALOG` puis `EV_NEXTSTATE`/`EV_CHANGESTATE` via `DIALOGCHAIN`); EXIT avec `RESIDENTRESET ... empty=1`; puis chargement réel slot07 V11/5604B → `ST_PLAYING=3` sans intro. La reprise conserve 30 monstres, 22 actifs initialement, mouvement/arme/porte ; activation à 24, `MONSTERRETAL COMMIT` réel de `sprite=7` (`HP30→27, armor10→8`, feedback), tir manqué consommant **1 munition**, tours livrés. `shapeData/mediaTexels=0` toujours confirmés. `heap8=113568 largest8=86004` identiques aux derniers `ALIVE` après reload.

**Interprétation** : ce PASS concerne la transition du startup et ses principales non-régressions ; pas de test de mort, nouveau SAVE ni absence universelle de fuite. Les valeurs de RNG peuvent varier lors du seed sans être une erreur ; l'ordre d'intention audio, pas son `seq` absolu, fait foi. La version testée `5c76e7f73e019200c3296a5567891b21170093df` est gelée ; les commits suivants forment un nouveau jalon et demandent une nouvelle validation hardware. Toujours branche agent, aucun merge automatisé.

---

## PR #222 — Canvas state ABI 4→3 : parcours utilisateur matériellement éprouvés (19 octobre 2026)

**Code testé sur `esp32-cyd` : `ebba9c6586ecb8919937f8067442faac08e2a2f8`** ([PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222), base `main 8b89a46151c1645ae77493b27e4f6925df6d99d1`), [CI push verte #39328930441](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39328930441), [CI PR verte #39328938740](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39328938740). `DoomCanvas_setState` est retiré de l'ABI CYD : `INIT exports=3`, désormais init/free/startup uniquement ; MENU, intro START et PLAYING LOAD sont routés via `EspNativeMainMenu_transitionCanvasState` sur les quatre appelants natifs. Aucun nouvel owner lourd, aucune mutation du monde par cette refactorisation ; RAM statique **49 536 B**, Flash **825 653 B** (+8 B par rapport au dernier #221).

**Protocole exécuté sur le vrai classic CYD** : boot `exports=3` et menu FNV `0f2c9fc3`; Help/About 83 lignes (scroll bas/haut, retour), Options → Video → luminosité 100→25 persistée → retour avec FNV exact; START → `ST_INTRO=9`, audio STOP_ALL→5039, première frame `ade0195d` → dernier Continue → teardown **34 056 B** → Entrance `CRC=623f34e4`, arena `14095/c3882516`, jeu jouable. Déplacement, deux pickups Armor Shards, ouverture de porte. HUB SYS → EXIT sans SAVE, reset `released=18008 empty=1` et menu `0f2c9fc3` retrouvés. Enfin LOAD **effectif** du slot07 `V11/5604 B` → `ST_PLAYING=3` directement sans nouvelle intro : 30 monstres, 22 actifs, MOVE/STRAFE et automap opérationnels après reprise. Ce nouveau témoin valide **LOAD depuis MENU_MAIN** après EXIT ; il ne prouve pas un LOAD déclenché depuis le menu de mort.

**RAM / limites** : gameplay neuf à 280112/285451 ms et gameplay rechargé à 316979/321983 ms : `heap8=113568 largest8=86004` constants aux points fournis ; pas de garantie universelle sur toutes les cartes. `shapeData/mediaTexels=0x0` aux frontières START, EXIT et fin de LOAD. `NATIVEFRAME BOUNDARY-INLINE` attendu sur une rotation, sans échec dans ce segment. Aucune mort, aucun coup réellement porté par les 22 monstres, aucune capture visuelle indépendante ni SHA firmware Serial sur ce test. **Code gelé** au commit `ebba9c6586ecb8919937f8067442faac08e2a2f8`; clôture documentaire seulement jusqu'à revue/merge humain.

---

## PR #221 — PASS_TURN sur automap : PASS matériel et clôture (19 octobre 2026)

**Firmware normal `esp32-cyd` éprouvé :** code `c7faa293a702d2a259e51faa8a17b174fe896e32` ([PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221)), base `main 90cb0d054e9ec234690c27935e68a19cc805a464` intacte. [CI push #39324723379 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39324723379), [PR #39324731800 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39324731800), incluant le nouveau test hôte `pass_turn_automap_host` (turn, hazard, rollback, lethal, WORLD) et le guard de routage. RAM statique normal **49 536 B**, flash **825 645 B**. Ce SHA remplace l'étiquette « dernier code testé » du précédent `bb4668b` décrit ci-dessous ; ses preuves historiques restent valides.

**Comportement utilisateur :** en automap, le bouton `PASS_TURN` **fait réellement jouer les ennemis** et laisse la carte à l'écran ; auparavant, il était ignoré. La route réutilise exactement le même `PassTurn_execute` natif que le gameplay `WORLD`, y compris l'interaction sur danger de sol type10/11 et son rollback. La présentation 3D reste filtrée pendant la carte. Les dégâts ennemis non létaux continuent de faire clignoter le fond rouge/noir avec `N DAMAGE!` ; la fermeture restaure HUD, arme, HP et armure depuis les propriétaires actuels. Un danger de sol lors de `PASS_TURN` ne repeint pas le HUD 3D : sa notification `AUTOMAPDAMAGE` ne peut être armée qu'après acceptation du tour. Aucune nouvelle allocation permanente ni boucle FPS fixe.

**PASS du vrai CYD** : `PASS_TURN` en mode carte aux séquences **14, 20–23, 26–29** ; pour chaque appui `AUTOMAP-PASS status=0`, `MONSTERTURN ORDERED-DISPATCH reason=PASS_TURN` avec `activeCount=24`. Le monstre `sprite=6` frappe et rate selon le RNG ; des `AUTOMAPDAMAGE ARM total=4/6`, `FRAME`, `DONE` puis retour au FNV canonique `ccdb8916` attestent le feedback. Pick-up Armor Shard `8→12` sous carte puis `AUTOMAP-CLOSE` suivi du HUD live `health=25/34 armor=10/23 weapon=2 ammo=38`. En restant dans l'automap, le joueur a également testé la **mort réelle** : `PASS_TURN seq=29` → `PLAYERDEATH ARM hp=0 hudPixels=0` → `LETHAL-COMMIT` → `PLAYERDEATH READY phase=death-menu-ready input=death-menu load=available` après **3010 ms** ; slot01 reconnu lisible. Mémoire stable `heap=179012 heap8=113768 largest8=86004` de 31,909 à 82,531 s. L'utilisateur conclut « Pour moi c'est clean ».

**À ne pas surinterpréter :** les `PASS_TURN` matériels étaient sur `tileTouch=none`, donc ni hazard type10/11 ni rollback n'ont été rejoués sur le CYD (seulement host/CI). Le menu LOAD est disponible et le slot lisible, sans preuve d'un chargement effectif. Pas de CRIT, pas de pixels individuels filmés, pas de SHA imprimé par le firmware. Après cette validation, **aucun changement de code avant merge**, seulement documentation puis CI du commit documentaire. `main` ne doit pas être mergé par l'agent.

---

## PR #221 — automap, feedback de dégâts natif : PASS visuel CYD (19 octobre 2026)

**Dernier code firmware normal effectivement éprouvé par l'utilisateur :** `bb4668b25be487aa80b85f8f3ac5f3871a006acc` ([PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221)). [CI push #39301322358 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39301322358) et [PR #39301350182 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39301350182). Statique normal **49 536 B**, Flash **825 129 B** ; +16 B de petite mémoire de feedback automap, sans framebuffer supplémentaire. Ce jalon supplante **uniquement l'étiquette « dernier code testé »** des sections historiques P1/P2 ci-dessous.

**Usage / propriété** : ouvrir l'automap via zone tactile supérieure droite, utiliser les mouvements/rotations/select ; les tours et dégâts continuent. L'automap couvre entièrement le framebuffer : les rendus 3D des monstres, leurs topbars, bordures rouges et overlays de direction ne doivent jamais la recouvrir. Lorsqu'un monstre touche réellement le joueur, **après le COMMIT**, la carte affiche une alerte dédiée : le **fond noir pulse deux fois en rouge sombre** (0–120 ms et 220–350 ms depuis l'armement) ; un bandeau supérieur affiche `N DAMAGE!` ou `CRIT N DAMAGE!`, N étant le total santé+armure annoncé par le moteur. Après 900 ms, elle redevient noire, avec FNV de carte canonical. Une phase de pulse peut être déjà écoulée lors du premier repaint si un tour de monstre dure longtemps ; ne pas promettre deux frames visibles à chaque attaque. Les couleurs des éléments du plan sont conservées, aucune mutation de monde ni de tour n'est ajoutée par l'effet. La fermeture de la carte redessine le HUD/arme live et la vue monde ; si elle échoue, la propriété revient à la carte.

**Protocole de validation ciblé** : sur `esp32-cyd`, entrer en automap avant une riposte et capturer `[MONSTERRETAL] COMMIT ... damageTotal=N`, `[AUTOMAPDAMAGE] ARM total=N`, les `FRAME phase=0..3` qui ont pu être présentées, puis `DONE`; confirmer à l'œil l'absence de barre HUD 3D durant la carte. Après fermeture, vérifier `GAMEPLAYHUD REPAINT`, `AUTOMAP-CLOSE` puis arme/munitions conservées. **PASS réel du 19 octobre** : probe=1 `damageTotal=5`, phases `0/1/2/3`, FNV rouge `e4e48586`, noir `dbed1196`, retour `7c0cfbc6`. Probe=2 `damageTotal=4` après animation de porte, phases 2/3 et retour `ed365836` ; normal. `AUTOMAP-CLOSE hudPixels=7522` avec `REPAINT health=27 armor=8 weapon=1 ammo=12`. Confirmation utilisateur explicite de l'effet, non déduite du seul host. `[ALIVE]` à 27–47 s : `heap=179012 heap8=113768 largest8=86004` constants.

**Limites** : coups non létaux seulement sur ce SHA, cas CRIT et wrap horloge couverts côté test host, pas de test CYD simultané ni létal. Le micro-état de feedback appartient au renderer automap, pas au moteur de combat ; les fichiers de sauvegarde et le cycle RNG ne changent pas. Le diagnostic `NATIVEFRAME BOUNDARY-INLINE` observé lors de la session antérieure provient d'un garde borné de textures murales et n'est pas modifié ici. Clôture docs-only après PASS ; pas de merge automatique dans `main`.

---

## PR #221 — clôture après les deux reviews Codex : PASS CYD (19 octobre 2026)

**Dernier code testé sur le vrai classic CYD normal :** `813987e02b09c558364fba0d89b6ae4b62ca7656` ([PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221), branche `agent/esp32-retire-desktop-canvas-text-abi`). [CI push SUCCESS #39251963619](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39251963619) et [PR SUCCESS #39251973504](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39251973504), incluant tests C réels des deux corrections et builds/audits ELF des trois profils ; normal RAM statique **49 520 B**, Flash **823 197 B**. Ce jalon post-review **remplace le statut de dernier code testé** `0482f13` décrit plus bas, dont la preuve antérieure reste valide pour les seuls parcours Pistol→Shotgun→Axe.

**P1** : le render HUD live accepte le tuple réel de mort `HP=0, weapons=0, weapon=0`, `weaponsPresent=0`, afin que le renderer natif cache l'arme sans interrompre la chute. Un joueur vivant sans armes ou une arme non possédée reste fail-closed. **P2** : le bouton tactile `WEAPON_MENU` en bas droit possède désormais son glyphe vectoriel jaune, rendu par le compositor de feedback réversible existant. Le test hôte vérifie `begin`, journal borné, temporisation **120 ms**, `restore`, framebuffer et FNV exacts ; aucun état gameplay mutable. Les deux threads de review GitHub sont résolus.

**Témoins hardware après ces modifications** : `WEAPON_MENU zone=13` ouvre directement les armes à deux reprises ; **Axe 0→Fire Ext 1**, `hudBottom=3ec7786a=expectedBottom`, puis **Fire Ext 1→Shotgun 3**, `hudBottom=e6ba2d3c=expectedBottom` ; à chaque fois `hudRepaint=yes exactBottom=yes`, `GRID-SELECT-CLOSE`, `HUB-CLOSE turnAdvance=no`. Pas de `TOUCHFEEDBACK SKIP` sur les ouvertures fournies ; cela prouve que le routage ne perd plus son feedback, sans prouver à lui seul l'apparence visuelle du glyphe.

**Régression critique de mort** : coup létal `playerHP=5→0`, `[PLAYERDEATH] ARM seq=11 hp=0 weapons=000f->0000 weapon=3->0`, `[MONSTERRETAL] LETHAL-COMMIT deathOwner=armed`, `[PLAYERDEATH] PHASE elapsedMs=836 fall=complete viewZ=6 fade=begin`, puis **`[PLAYERDEATH] READY elapsedMs=3001 frames=50 phase=death-menu-ready input=death-menu load=available`**. Le chemin chûte/fondu/menu est réussi sur le matériel. Les frames FALL individuels sont masqués dans le logging normal, pas attestés une par une. `[NATIVESAVE] READABLE-SPATIAL path=/DoomRPG-ESP32-slot01.sav bytes=5604 result=valid` constate uniquement la lisibilité du slot depuis le menu, sans prouver une action LOAD.

**Mémoire** : de 207 285 ms à 257 871 ms (~50,586 s) `heap=179028 heap8=113784 largest8=86004` constants, combats et mort inclus. Pas de `REBASE-FAILED`, de rollback d'arme ni de `HUB-RECOVER` dans l'extrait. Autres tests non présents : boutons BACK, cases non possédées, onglets INV/STAT/SYS, SAVE/LOAD réel, screenshot du HUD, lecture d'un SHA firmware dans les logs. L'ancien PASS Canvas demeure propre au code `f9d8061`.

**Processus** : clôturer par commit **docs-only** et CI verte. La PR peut repasser en revue une fois ce contrôle réussi ; seul l'utilisateur fusionne `main`. Le repo GitHub garde priorité sur les souvenirs des conversations.

---

## PR #221 — accès direct aux armes depuis le HUD : PASS CYD ciblé (19 octobre 2026)

**Code CYD testé** : `0482f13c2f62b9fc79e650a0b80bfc9695d55bd4` sur `agent/esp32-retire-desktop-canvas-text-abi`. [CI push #39246653544 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39246653544), [CI PR #39246661282 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39246661282), RAM statique **49 520 B**, Flash **823 037 B** en normal. Le PASS Canvas texte précédent reste distinct : `f9d8061`.

**Guide d'usage final** : en jeu, un seul tap dans le bas droit du HUD (munitions/boussole ; zone logique **x100..159 / y100..119**) ouvre l'écran **WEAPONS** avec grille 4×3, arme courante préselectionnée ; les armes possédées sont en couleur, les autres grisées. Un tap sur une arme possédée la sélectionne et **ferme immédiatement le menu sans faire avancer le tour** ; BACK annule sans sélection. La navigation HUB top/menu affiche exclusivement **INV/STAT/SYS** ; l'ancien onglet **WPN** est retiré. Les trois icônes de familiers 9–11 absentes des données sont affichées selon le comportement fail-closed existant ; ne pas leur inventer d'asset.

**Diagnostic causatif** : sur `45a549a5`, une sélection correcte `2→3` produisait un `REBASE-FAILED` (`firstBottom=4ad75e77`, `secondBottom=f1d77c03`) puis `GRID-CLOSE-ROLLBACK` ; le personnage gardait donc son arme précédente. La cause se trouvait dans la coexistence d'un HUD retained obsolète (arme/munitions/angle) et de la réapplication non idempotente de la boussole après un repaint complet. Le correctif `0482f13` a introduit un **instantané local depuis le PlayerState canonique** : HP, armure, arme, ammo-type (même table que le cycle d'armes), quantité et angle. Un même modèle live alimente le HUD et le sprite du weapon en vue subjective. Pas de nouvelle copie persistante, pas de mutation de player par le renderer, pas de suppression du contrôle FNV ; **la fermeture refait encore deux peintures et impose le même FNV de bas HUD**.

**Résultats matériels exacts** : tap `WEAPON_MENU zone=13`, `HUB-OPEN source=hud-ammo-compass`, puis Pistol `2→3` : deux `REPAINT weapon=3 ammo=22 angle=128`, `HUB CLOSE exactBottom=yes` avec `f1d77c03` identique à l'attendu, `GRID-SELECT-CLOSE target=3` et `HUB-CLOSE seq=2`. Réouverture `equipped=3 selected=3` ; sélection Axe `3→0`, deux `REPAINT weapon=0`, `exactBottom=yes` avec `966e71ad`, `GRID-SELECT-CLOSE target=0`, `HUB-CLOSE seq=4`. `turn=no` aux deux sorties. Quatre `[ALIVE]` entre **80,261 s** et **95,362 s**, `heap8=113784 largest8=86004` constants. Le `TOUCHFEEDBACK SKIP ... overlay-unavailable inputQueued=yes fatal=no` indique uniquement l'indisponibilité du feedback facultatif ; les deux inputs ont bien ouvert la grille. `exactHud=topbar-pending` n'invalide pas le bas exact, la barre haute est recomposée au rendu monde.

**Non couvert dans cet extrait** : visibilité subjective vérifiée par capture/inspection visuelle, case grise, BACK, arme identique, test tactile des trois onglets, SAVE/LOAD/combat/EXIT, ni impression de `shapeData/mediaTexels` ou du SHA firmware. Les nouveaux tests C/CI gardent leurs couvertures propres mais ne s'étendent pas au matériel sans log. **État daté avant reviews** : cette clôture concernait `0482f13`. La première section de ce guide documente désormais le nouveau dernier code `813987e`, son PASS matériel post-review et le processus de clôture actualisé.

---

## PR #221 — retrait du texte Canvas desktop : validation réelle CYD (19 octobre 2026)

Base `main 90cb0d054e9ec234690c27935e68a19cc805a464`, branche `agent/esp32-retire-desktop-canvas-text-abi`, [PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221), **dernier SHA code texte validé** `f9d8061fd342e77d4c83271075c077a470b0207a`. [CI push #39181089435 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39181089435) et [CI PR #39181183706 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39181183706).

**Canvas desktop ABI désormais à 4** (`init/free/setState/startup`). Le rendu texte passe par `EspNativeCanvas_drawText(DoomCanvas_t*, const char*, x, y, flags)` ; corps du raster inchangé `b4296a8f`, blitter privé `4c4a9a93`. Le moteur desktop original n'est pas modifié pour les builds non ESP32. Tests hôte + symboles ELF empêchent la réintroduction de `DoomCanvas_drawString1` dans les trois profils.

**PASS ciblé sur le vrai CYD** : Help/About 83 lignes, navigation `0→8→16→24→32→40→32→24` et BACK ; FNV stables `5f22cf6b/d0788359/9213df95/b0191189/4c944ee5/943f0b77`, menu `0f2c9fc3`. Options `0acffb35`, luminosité 75→100→25→100 via SD native et retours OK. START→intro première image `ade0195d`→sortie (+34 056 B)→Entrance CRC `623f34e4`, arena `14095 B` FNV `c3882516`, resident ready. Deux `ALIVE` jeu `heap8=113584 largest8=86004` à ~38,7 et ~43,7 s ; `shapeData/mediaTexels=NULL`. L'extrait n'a ni header boot `exports=4` ni SHA embarqué, ni SAVE/LOAD, combat ou EXIT. **Ce témoin reste attaché au code texte `f9d8061`, pas à un firmware UI ultérieur.**

Le jalon B (grille d’armes depuis le HUD, retrait de WPN et fermeture après sélection) a un **premier PASS matériel** sur `0482f13`, puis un **PASS matériel post-review** sur `813987e` détaillé en tête du guide.

---

## Animation d'ambiance native — PR #220, régression post-review validée sur CYD (19 octobre 2026)

[PR #220](https://github.com/Kharn27/DoomRPG-RE/pull/220), base `main` `16155aec8e741151c5f057e95c7fe395a15521e8`.
**Dernier commit de code** : `5b0439f801532cf070d8dae57f13a7345d0eea29`, [CI SUCCESS #39179286818](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39179286818) (56 étapes). Les deux P2 de review sont corrigées ; **la session CYD post-review a été jugée OK par l'utilisateur**, avec limites ci-dessous. L'ancien PASS de respiration rouge était sur `3b8c2135bc2f4307eaa0deb633f7314c93ea1c27`. La PR peut être remise en Ready for review pour le merge humain.

La présentation **reste pilotée par les changements de phase, pas un FPS permanent**. Les sprites immobiles éligibles utilisent la politique `esp_native_ambient_sprite.h`, avec retraitement des animations fixes interdit. Pour les voyants, l'overlay natif des lignes doit être actif *également dans `renderAmbient`* : sinon une porte déverrouillée retombe visuellement sur la texture immutable rouge, ou un rendu sur porte ouverte échoue. Le wrapper `EspNativeGameplayFrame_renderAmbient` dans `esp_native_dynamic_line_render.c` préserve cette frontière.

**Cadences finales** (champs en `esp_native_wall_indicator_animation.h`) : rouge/verrouillé logique 9, `0,0,1,2,3,3,2,1` à 360 ms par phase, respiration **2,88 s** ; vert/déverrouillé logique 10, `0,1,2,3` à 144 ms par phase, **576 ms**. Ce sont des indices dans les quatre textures physiques PAK, sans création d'assets. Le masque de voyants visibles et l'heure d'échantillonnage sont publiés uniquement après le raster mural réussi ; la comparaison des phases absolues décide du prochain rendu. Une action de navigation ne rallonge donc pas le cycle. Jeu/RNG/world/activation jamais mutés par le simple redraw.

**Corrections P2 post-review, CI + régression hardware ciblée** : la famille monstre sous-type 4 anime les frames 0/3 en ~250 ms (les autres monstres en ~4 s) sans mutation de l'état ; les témoins `ambientCandidates` / `ambientFastCandidates` / `ambientRapidCandidates` sont désormais comptés **après écriture effective de pixels** dans `drawAt` et non à l'admission de BSP. Les copies CROSS et sprites glow utilisent leurs compteurs de pixels respectifs. En l'absence de sprites rasterisés et de voyant visible, aucune re-présentation idle ne doit être déclenchée ; les actions restent indépendantes de l'horloge. Ces chemins spécialisés sont prouvés par tests hôte/CI ; le dernier log CYD ne démontre pas individuellement chaque cas de culling ni la cadence Phantom.

**Playtest normal CYD sur le code antérieur** : armor shards et flamme visibles animés, vert durable après EV_UNLOCK dans un essai précédent ; utilisateur approuve le rouge ralenti sur la révision `3b8c2135`. Déplacements, dialogue, pickups et ouverture/fermeture dans le dernier journal ; `heap8` stable à `110216` sur les dernières mesures, plus grand bloc `86004`. `shapeData` / `mediaTexels` NULL, 21 TU desktop toujours exclues. Absence de FAIL limitée aux extraits fournis, pas de garantie sur toutes les cartes. Voir [PORTING_STATUS.md](PORTING_STATUS.md) pour les limites détaillées.

**Régression après corrections P2 sur le CYD** : « Ça m'a l'air ok :) ». Logs `esp32-cyd` avec MOVEs, TURN, dialogue opcode 26 et fermeture, pickups et flashes, sans `[AMBIENT] FAIL` visible dans l'extrait. Entre `[ALIVE] uptime=228293` et `268626 ms`, `heap8=112640 largest8=86004` constants (environ 40 secondes). Les tours de monstre observés ont `activeCount=0`, donc il **n'y a pas de test matériel de l'animation subtype 4** dans ce segment : ne jamais déclarer ce chemin matériellement éprouvé. La CI du code corrige le contrat et les tests hôte. Pas de nouvelle modification de code après ce PASS.

**Reprise après merge** : vérifier le SHA GitHub réel de `main`, et relire `PORTING_STATUS.md`, ce guide et les dernières notes avant tout nouveau `agent/*`. Ne pas modifier la mécanique de combat à l'occasion de ce jalon d'animation ; investiguer séparément le spam `[NATIVESPRITE] CROSS` en idle si un nettoyage des logs est choisi.

---

## Utilisation et validation du firmware courant — PR #219 (18 octobre 2026)

La prise en main et les commandes actuelles sont dans
**[README.md](README.md)** (PAK, flash normal, menu, jeux,
sauvegardes V11, luminosité et HUB). Les détails matériels et
mesures à jour sont dans **[PORTING_STATUS.md](PORTING_STATUS.md)** ;
le design invariant est dans **[ARCHITECTURE.md](ARCHITECTURE.md)**.
Les anciens lots A–D avec leurs SHA/CRC et leurs limites sont
conservés dans [l'archive #219](archive/PR219_DESKTOP_RETIREMENT_MILESTONES.md).
Les sections datées #218 et antérieures ci-dessous sont
**des archives de décisions**, pas des propriétaires runtime.

**Dernier SHA code #219** :
`a59554ed524502d02dbab69149777a94050b0a26`
([CI #39162920720 SUCCESS, 55 étapes](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39162920720)).
`Game_t` n'est plus alloué (`Game=0`), le générateur
`DoomRPG.c` desktop est retiré, le runtime ne lit pas ZIP et
le pont Canvas exporte **cinq** fonctions, contre six avant.
`DoomCanvas_drawImageSpecial` est privé et son corps de rendu
est inchangé. Le label `Loading...` est passé sous forme
d'un tableau `char[]` local à la signature historique.
Les trois warnings de chaîne `BACK/UP/DOWN` de Help/About
restent **connus** en CI pour ce SHA ; ne pas les éliminer
par cast silencieux.

**PASS CYD réel ciblé** : Help/About 83 lignes, défilements
jusqu'au décalage 32 et retour correct ;
Options puis BACK ; START, intro
(six taps dans le dernier extrait car premier texte achevé
naturellement), dispose 34 056 B, Entrance
`FRESH-VISIBLE` et resident gameplay prêt. FNV :
menu `0f2c9fc3`, Options `0acffb35`, première image
d'intro `ade0195d` ; en gameplay,
`heap8=113816 largest8=86004` stables.
`shapeData/mediaTexels=NULL`.
Le dernier extrait n'inclut **pas** son boot, donc ne prouve
pas directement l'affichage `exports=5` sur la carte ;
il ne rejoue pas non plus LOAD, EXIT ou combat.
Ces derniers chemins possèdent des preuves matérielles
antérieures dans l'archive, non extrapolées au nouveau SHA.
Le firmware ne journalise pas son empreinte SHA.

Pour confirmer une future révision :
```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Contrôler les premiers logs `Game=0` et `exports=5`,
SD/PAK, le FNV menu, un cycle Help/About/Options, l'intro,
puis les invariants et la mémoire en jeu.
`esp32-cyd-bringup` et `esp32-cyd-trace` ne
remplacent jamais le test normal. La CI vérifie
toujours les 21 TU C desktop rejetées, les symboles
retirés, et les trois builds normaux/diagnostiques.

---

## Historique PR #218 — luminosité native et retrait du keybinding desktop (état daté du 18 octobre 2026)

> **Archive historique** : les nombres de fonctions Game et les états
> « à faire » de cette section étaient exacts au SHA #218 mais sont
> remplacés par le statut #219 ci-dessus. Ne pas les appliquer au
> firmware courant.


**Base `main`** :
`8a9c28940b03ab1ee10d5cdc720a26b6f08cbb4e` ;
branche `agent/esp32-native-backlight-options`,
[PR #218](https://github.com/Kharn27/DoomRPG-RE/pull/218).
**Dernier SHA de code validé sur le vrai CYD classic** :
`3c36f7544e537ec96f77c7fa077b66636cd6db0f`.
[CI SUCCESS #39127502174](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39127502174)
et [#39127499576](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39127499576)
(host, `esp32-cyd`, bringup, trace, ELF).
Le test matériel transmis couvre boot + Options/Luminosité + Load Game slot09
et déplacements/combats ; il valide cette frontière, pas tout Doom RPG.
L'utilisateur merge lui-même après review.

### Utiliser le réglage sur la carte

1. Flash normal : `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor`.
   À l'initialisation, `PlatformVideo_begin` installe le PWM
   **GPIO21 actif HIGH**, 5 kHz/8 bits, canal 7, valeur initiale
   **100 %** ; puis `SD.begin` et
   `EspNativeConfig_loadAtBoot(sdReady ? 1 : 0)`.
2. Naviguer **MENU_MAIN → Options** (deux taps relâchés sur Options),
   puis **LUMINOSITE** (deux taps relâchés sur sa carte).
   Dans le sous-menu du rail droit, **un tap** choisit
   25, 50, 75 ou 100 %. La carte courante est soulignée
   par le focus rouge. BACK du sous-menu est à un tap ;
   Back d'Options demande deux taps et ramène au menu principal.
   INPUT et SOUND restent désactivés.
3. Le fichier persistant est exclusivement
   **`/DoomRPG-ESP32.cfg` à la racine de la microSD**,
   non `Config`, non `.sav`, non le `.pak` et non le ZIP.
   Format : **12 B**, `DRCF`, version 1, pourcentage (25/50/75/100),
   réservés zéro et checksum FNV-1a LE32.
   Aucune option PC, souris ou manette n'est migrée.
   **Premier boot sans fichier** : c'est valide, 100 %,
   aucun fichier créé automatiquement.
4. Une sélection différente commence par
   `[NATIVECONFIG] SAVE result=committed version=1 percent=75 ...`,
   puis `[BACKLIGHT] SET percent=75 duty=191/255`.
   Écriture : `.cfg.tmp` puis read-back + `.cfg.bak` puis
   renommage et relecture. Si SD en erreur, **pas de
   changement PWM** ; si primaire absente/invalide
   mais backup valide, chargement backup ; sinon
   défaut sûr 100 %. Le loader ne lit jamais une
   entrée desktop `Config` v23.
5. Débranchement/rebranchement réapplique le fichier
   au début du boot. Le **PASS de restauration réellement
   montré** est celui de 50 % sur la première révision du
   sous-menu : `LOAD source=primary ... brightness=50`.
   Le dernier SHA démarre aussi avec fichier valide à
   `brightness=100`, puis enregistre correctement 75 %.
   **Un redémarrage après le changement à 75 % n'a
   pas été fourni** ; ne pas lui attribuer un PASS fictif.

### Ce qui a été désengagé, et ce qui reste

- Le firmware ESP32 ne compile pas `src/Game.c`. Son pont
  `esp_legacy_game_bridge.c` ne possède plus que **3**
  fonctions de compatibilité :
  `Game_init`, `Game_unloadMapData`, `Game_activate`.
  `Game_loadConfig`, ainsi que les lecteurs desktop
  `File_readByte` et `File_readInt` sont retirés,
  anciens tests et audits positifs remplacés.
- Les trois tableaux de configuration clavier
  `keyMapping[12]`, `keyMappingTemp[12]`,
  `keyMappingDefault[12]`, plus leur fonction
  `DoomRPG_setDefaultBinds`, ne sont plus dans
  les TU ESP32. Leurs **1584 B** sont récupérés
  en stockage statique. Le décodage souris/clavier/manette
  de `src/DoomRPG.c` n'est pas lié et ce fichier reste
  référence du port desktop, intact.
- Le toucher sur classic CYD utilise
  `platform_input.cpp`, les dispatchers UI et
  `esp_native_gameplay_input.c`. Le lecteur
  natif de ressources PAK `Render_loadMappings` est
  **maintenu** ; ne pas confondre mappings de textures
  avec key bindings.
- L'audit ELF
  `--require-retired-doomrpg-keybinds` vérifie
  que les six symboles desktop sont absents des
  owners et du binaire final sur les trois profils.
  `desktop_keybind_retirement_host.py` couvre
  les frontières touch/mappings, et
  `native_config_host.py` valide stockage simulé,
  version, corruption, redémarrage et rollback.
  Ils ne remplacent pas les logs réels de la carte.

### Mesures et critères matériels clos

Le firmware normal passe de **51072 à 49488 B**
de RAM statique (**1584 B économisés**), et de
**821881 à 821285 B** en flash (**596 B économisés**).
`heap8` post-mappings progresse de `153308`
à `154892`, `largest8=110580`.
Après changement SD : `154692` (delta
de -200 B dû à l'utilisation SD, observé avant/après retrait).
Sur le gameplay restauré du slot09 :
`heap8=113592`, `largest8=86004` stables.

Trace du 18 octobre, même firmware :
`[NATIVECONFIG] LOAD source=primary ... brightness=100`,
`[OPTIONSBRIGHTNESS] SELECT percent=100->75`,
`[NATIVECONFIG] SAVE ... percent=75`,
`[BACKLIGHT] SET percent=75`,
BACK enfant puis parent, `[NATIVESAVE] LOAD ... version=11
bytes=5604 map=1`, `V9-SPATIAL-STAGE ... exact=yes`,
`[ENGINESESSION] RESUME-VISIBLE map=1`.
Les taps monde confirmés incluent `TURN_LEFT`,
`TURN_RIGHT`, `STRAFE_LEFT`, `STRAFE_RIGHT`,
`NEXT_WEAPON`, `SELECT`, `FORWARD`.
Attaques et ripostes commit, monté de niveau 1→2
avec popup/tap, gib et drop natifs présents.
`shapeData==NULL`, `mediaTexels==NULL`
sur les frontières contrôlées ; aucun avertissement
de divergence RNG dans ces logs.

**Statut : merge-ready sous réserve de la review**,
clôture post-hardware en commits **docs-only**.
Pas de nouveau code après les logs de PASS.
En cas de nouveau commentaire review impliquant un changement
de code, refaire la CI et un test ciblé sur le CYD :
la validation matérielle ne se transmet pas
automatiquement à une variante non flashée.
Ne pas merger dans `main` sans demande explicite.
Au prochain jalon après merge, lire le nouveau SHA de
`main` et ses deux documents de référence.

---

## PR #217 — follow-up de code review : Options aligné sur MENU_MAIN (16 octobre 2026)

Après la clôture initiale `7c866696`/docs `909e7e37`,
la review Codex GitHub a identifié un P2 réel :
`paintOptionsBounded()` composait le logo 102×120 à `x=29`
(car il le centrait dans 160px) tandis que le menu principal
l'affiche à `x=0`. Le rail `x=102..159` masquait sa droite.
**Nouveau SHA de code :**
`e4d0d03844ebfbd431318b8b1e0c9f2936c5d961`.
Le painter Options utilise désormais exactement
`DOOMRPG_CANVAS_X + DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X`,
comme MENU_MAIN, sans modifier y/w/h ou les quatre boutons.
Le test hôte vérifie explicitement l'égalité des destinations
`logoDst` dans les deux renderers et interdit le recentrage.

Il s'agit d'une **nouvelle frontière de code** : ne pas
présenter les photos précédentes comme un test matériel de
cette variante. Pour validation sur le classic CYD normal,
ouvrir MENU_MAIN → OPTIONS et vérifier que le logo occupe
la même zone gauche `x=0..101`, puis BACK. Les autres
limitations audio et gameplay listées ci-dessous subsistent.
Seul l'utilisateur merge dans `main`.

---

## PR #217 — clôture et mode d'emploi du menu DOOM / terminal (16 octobre 2026)

Base réelle `main 4434e47a36b70826c401fc17a8b24d371789098c`,
branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
**SHA code définitif de ce lot :**
`7c866696f4ce1d263b4a514544d7f83c06146d27`.
[CI du SHA : SUCCESS (51 étapes)](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38877111031) ;
le retour utilisateur du 16 octobre autorise la clôture documentaire
et la préparation au merge, **sans demander à l'agent de merger**.

### Rendu MENU_MAIN : contrat désormais retenu

En logique 160×120, le logo source PAK **108×74** est copié vers
`SDL_Rect{x=0,y=0,w=102,h=120}`. Il est donc aligné **au bord haut**,
occupe toute la hauteur et est **volontairement déformé** ; ne pas
« corriger » son ratio sans nouveau retour visuel. Aucun encadrement
ni montage rouge autour du logo. Le panneau droit, seul habillé,
commence à `x=102` et contient les boutons `START`, `LOAD`,
`OPTIONS`, `HELP` : `x=104..157`, hauteur 20, `top=13+25*i`
pour `i=0..3`. `native_main_menu_160x120_layout.h`
définit la géométrie partagée par dessin, sélections, touch gate,
hitboxes de debug, Options Back et retour `NO SAVE`. La carte
reste neutre jusqu'au premier tap, devient rouge armée, puis
le **second tap après relâchement** confirme ; la logique
d'action/touch existante ne change pas.

### Terminal et code d'accès

L'owner permanent `esp_native_gameplay_password.c` dessine
l'en-tête de question sur deux lignes, un séparateur unique,
un champ de saisie en creux (sans les deux anciens traits
rouges isolés), et un keypad natif 3×4 avec touches différenciées
DEL rouge / VALID vert. Les seules étiquettes déplacées sont
DEL et VALID (`y+5` au lieu de `y+2`) ; rectangles tactiles
inchangés. Un curseur bloc 5×9 (ligne `y=32`) clignote
toutes les 450 ms dans le chemin modal résident, à la suite
du code saisi, sans relecture des glyphes ni tour supplémentaire.
Les 5×9 pixels sont modifiés dans le framebuffer, puis
`Esp32PlatformVideo_present()` est appelé normalement ;
ne pas confondre ce rafraîchissement logique borné avec
un transfert matériel partiel.

### Preuve et test en cas de nouvelle révision

Le dernier firmware avant documentation compile dans les profils
`esp32-cyd`, `bringup` et `trace` ; les tests hôte
`ESP32/test/native_ui_doom_polish_host.py` et
`native_audio_handoff_host.py` ainsi que les gardes
compiledb/ELF passent. L'ancien hardware PASS audio
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc` a prouvé
intro/Entrance/EXIT et les invariants mémoire, mais
**n'est pas un essai matériel du logo final 102×120**.
Le retour visuel utilisateur accepte le jalon pour merge,
sans nouveau Serial/capture indexé au SHA final.

Si le layout doit encore bouger, tester sur carte réelle uniquement
en configuration normale :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Sur les logs, repérer `[MAINOPAQUE] TARGET` indiquant
`logo=102x120@0,0 stretched=yes unframed=yes`,
puis vérifier les quatre doubles taps et Options→Back,
LOAD `NO SAVE`, le champ code et son curseur. Les FNV
de framebuffer seront nécessairement différents des étapes
historisées. Audio : le nouvel `EspNativeAudioIntent_stopAll()`
et le PLAY 5039 utilisent encore un backend **silent**,
sans FIFO ni sortie sonore réelle ; ce chantier sera testé
ultérieurement. Ne pas réintroduire de ZIP runtime, de
`shapeData` ou `mediaTexels`, ni de source desktop
compilée. Pour tout prochain milestone après merge, relire
le vrai `main` et ses SHA de référence.

---

## PR #217 — handoff audio natif depuis l'intro, test CYD (16 octobre 2026)

Depuis le `main 4434e47a36b70826c401fc17a8b24d371789098c`,
branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
Le dernier code firmware normal flashé et observé :
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc`.
[CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38824713309),
avec nouveau test hôte `python3 ESP32/test/native_audio_handoff_host.py`,
trois builds et gardes ELF.

L'intro ne passe plus par `Sound_playSound`; le pont Canvas émet
une commande native PLAY `5039 flags=11 priority=5`.
Deux anciens sites `Sound_stopSounds` utilisent désormais
`EspNativeAudioIntent_stopAll()` : `resourceId=0 flags=0x80`,
dans le même record borné à 8 octets. Le backend reste **silent**
et `lastIntent` conserve uniquement la dernière intention ; ne
pas prétendre que les sons successifs ou la musique jouent déjà.
Les autres stubs `Sound_*` sont hors de ce jalon.

Pour la preuve physique, flasher **uniquement le profil normal** :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Chemin validé par les logs utilisateurs : Start Game → intro complète
→ dispose → Entrance → HUB SYSTEM → EXIT confirmé → menu principal.
On observe `[AUDIOINTENT] STOP_ALL seq=5` puis
`PUBLISH seq=6 resource=5039 flags=11 priority=5`, les 7 taps
d'intro, `INTRODISP READY` récupérant 34 056 octets,
`ENGINESESSION FRESH-VISIBLE` et `RESIDENTGAMEPLAY READY`.
Deux `ALIVE` en session à `heap8=112576 largest8=86004`.
Le reset EXIT vide les owners (`released=18008 empty=1`) et
retrouve le menu de FNV `522dc605`, `heap8=158256`,
`saveWrite=no`, avec `shapeData/mediaTexels=NULL`.

**Ne pas extrapoler :** aucun STOP_ALL observé **pendant EXIT**,
aucune sortie sonore réelle, aucun combat ni nouveau SAVE→LOAD sur
ce code, et pas de comparaison pixel exhaustive. Toute mutation firmware ultérieure exige un nouveau test matériel ;
les commits documentaires qui suivirent ce premier témoin audio
étaient docs-only, puis une passe UI distincte fut conduite sur
cette même PR, clôturée avec son propre code SHA ci-dessus. La cadence UI sous charge
est une dette distincte et reste documentée dans PORTING_STATUS.

---

## PR #216 — état du firmware natif et retour d'impact (16 octobre 2026)

**Firmware normal `esp32-cyd` joué sur CYD :**
`3361cdd1d4208f69c9d9eb12c41207b9016f1ac4`, depuis
`main 0ca15d756c2c6a64a5b4bc6b3e2b0ec89d83b7bd`.
[PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216) ;
[CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822809348) /
[PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822814390).
La copie générée de `DoomRPG.c` sert encore d'artefact de
vérification, mais **aucune TU desktop C générée n'est liée** ;
la racine `doomRpg` et les fonctions de sortie/cleanup vivent
dans `ESP32/src/engine_stubs.c` (durée process).

### Frontière native du combat

- `MONSTERATKVIS` présente les poses de tir et revient à idle ;
  la **dernière pose idle réellement présentée** publie
  `completedProbe` dans le même passage, sans bail
  `final-idle` de 500 ms supplémentaire. Pour les familles à trois
  tirs, les phases *intermédiaires* gardent leur cadence ; un
  projectile 204 actif impose une dernière frame de CLEAR réussie.
  Une frame non présentée ou altérant le RNG est refusée/retry,
  sans autoriser prématurément le COMMIT.
- `MONSTERRETAL` reste seul responsable du commit HP/armure,
  des RNG gameplay et de la décision létale ; le `MONSTERTURN`
  maintient un seul probe en vol. Le `DAMAGE` non létal
  regroupe **message + contour rouge + première bande de direction**
  dans une frame physique, après le commit et le traitement de
  mouvement du passage courant. Le début réel de la lease de
  1200 ms du message et de 500 ms du contour est mesuré au présent,
  pas à l'intention. Décès/pièges/barils hors de ce regroupement.
- `MONSTERACTIVESEQ` exécute au maximum **un membre vivant
  entier par service**, et sauvegarde l'ordinal/préfixe ordonné
  jusqu'au prochain passage de loopTask. `isBusy()` bloque
  touches et intentions déjà en attente tant que le tour n'est
  pas clos. Aucun `delay`, aucune nouvelle task, aucun nouveau
  buffer map-wide. Le budget est une limite d'actions complètes,
  pas un plafond en microsecondes.

### Reproduction et preuve

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Sur `/intro.bsp` / Entrance, reprendre le checkpoint près de la
porte `line=396`, SELECT pour l'ouvrir, laisser attaquer
`sprite=157 subtype=0`, puis PASS_TURN. Les logs validés
pour ce firmware montrent le `STEP phase=final-idle phaseMs=0`,
`COMPLETE finalIdleHoldMs=0`, `MONSTERRETAL COMMIT`,
`DAMAGECUE ON redBorder=visible presented=1` **avant**
`MONSTERACTIVESEQ COMPLETE` : le composeur reçoit désormais une
opportunité de rendu entre les autres membres. Préflight du RNG
`rngRollback=yes` ; dégâts réels `HP 30→26→22` et
armure `20→17→15`. Les `[ALIVE]` disponibles donnent
`heap8=112320 largest8=86004` inchangés. L'utilisateur constate
une amélioration nette, mais ne revendique pas une cadence parfaite.

**Défaut encore ouvert :** sous charge `DAMAGECUE DONE`
`onMs=272/120 gapMs=281 delayed=yes` puis
`onMs=321/120 gapMs=237 delayed=yes` ; malgré un
`VIEWFLASH targetMs=500`, expiration à `675/681ms`.
Ne pas reporter `nominalOnMs=85 nominalGapMs=45` comme temps
physiques atteints. Le prochain jalon doit auditer la durée des
transactions de membre/renderer et établir des opportunités de
service UI réellement bornées ; ne pas accélérer arbitrairement
les tirs 3-shot ni retoucher le RNG.

**Limites de couverture :** ce playtest est un PASS ciblé
porte/riposte/ordonnancement/heap, pas un nouveau test
SAVE→LOAD/EXIT sur ce SHA, ni une preuve de parité frame-à-frame
desktop, de projectile Malwrath physique ou de `Config present=yes`.
Le test hôte de `monster_active_sequence_budget_host.c` valide
la politique du budget, tandis que les logs du vrai CYD valident
l'ordre de présentation ; ne pas intervertir leurs preuves.

**Clôture :** après le dernier SHA de code joué, seules des
modifications documentaires sont admises avant merge utilisateur ;
nouveau jalon de timing sur une branche `agent/*` issue du prochain
`main` réellement fusionné. Ne pas merger automatiquement.

---


## Config key bindings natifs : premier lot PR #216 validé sur CYD (15 octobre 2026)

Branche `agent/esp32-own-config-keybinds`, code firmware
`8436de9caa83c8b3efd1d3d39fbc340c5293c89f`,
[PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216).
Les 3 tableaux process-lifetime `keyMapping`, `keyMappingTemp`,
`keyMappingDefault` (12 × 44 B chacun) et
`DoomRPG_setDefaultBinds` résident désormais dans
`esp_legacy_config_mappings_startup.c` au lieu de la copie
`DoomRPG.c` générée. Le source desktop reste inchangé.

Le script `build_engine.py` rejette toute dérive CRC ou
textuelle du tableau de defaults et du corps de reset.
Le test hôte `python3 ESP32/test/doomrpg_config_binds_host.py`
compile les définitions native et desktop et exerce 8 192
modifications/restaurations de la map. Le contrôle
`tools/audit_desktop_elf.py --require-native-doomrpg-config-binds`
vérifie compiledb et la propriété native, sur trois
environnements. `COMMON` objet GCC Xtensa est légitime pour
les états non initialisés, la **définition finale doit être BSS**
et unique. [CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747810890)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747821709).

Le vrai CYD normal a démarré : `Config file present=no`,
`CONFIGMAP READY`, menu `522dc605`, chargement V11 slot07
CRC/runtime/états spatiaux identiques, déplacements/portes/
combat/FX/pickup/retour EXIT opérationnels ;
`heap8=112328 largest8=86004` constants pendant le gameplay.
Exit sans sauvegarde, resident vidé, menu rétabli.
**Ne pas déclarer le chemin `Config present=yes` validé** :
la lecture positive n'a pas été exercée sur la dalle/SD.
Les logs attestent des présentations de frames, mais aucune
comparaison visuelle pixel à pixel. Le correctif CI post-code
n'a pas modifié le firmware. Ce lot est accepté ;
tout nouveau lot de code sur #216 réclame un nouveau PASS
matériel et une clôture documentaire propre, distincte.


## Sprite mural en angle : règles du renderer legacy rétablies (15 octobre 2026)

Code CYD validé `a088b485f3891d515537ae2961a27e0d0a40e5eb` ;
[PR #215](https://github.com/Kharn27/DoomRPG-RE/pull/215).
Après suppression du retry mural, les motifs d'un mur rouge
d'Entrance étaient absents en angle oblique et présents de face.
La comparaison avec `src/Render.c` a révélé des règles visuelles
non encore portées : les sprites avec le flag
`0x00800000` sont décalés d'une unité suivant leur direction
et ont une priorité de tri `INT32_MAX`. Les sprites 184..190
reçoivent la profondeur spéciale `0`, quand le test de flags
précédent ne s'applique pas. Le helper purement visuel
`esp_native_sprite_visual_legacy.h` rétablit ces règles pour
`esp_native_sprite_renderer.c` (projection/tri) et
`esp_native_bsp_visibility.c` (BSP leaf) ; il **ne modifie pas
la carte immutable ni les positions persistantes**.

```sh
python3 ESP32/test/wall_sprite_visual_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Host **25 920** comparaisons/compilation native, CI
[push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734975334)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734982820).
Sur CYD normal, l'utilisateur constate que les motifs restent
dessinés face au mur et de biais dans la pièce auparavant
défectueuse. Log de non-régression : mouvement/rotation,
porte160 animée, monstres6/7 et combat/riposte/FX/drop/pickup
opérationnels ; RNG des probes rollback exact,
`heap8=112328 largest8=86004` stables. Pas de nouveau
problème de rendu remonté.

Conserver le correctif `BOUNDARY-INLINE` précédent, ses tests
et ses propres preuves matérielles. Ce PASS est celui de
**`a088b485f3891d515537ae2961a27e0d0a40e5eb`**, pas une validation pixel parfaite de
tous les sprites du jeu. Ne pas déplacer ces décalages vers
le runtime map immutable ou la sauvegarde. Après les logs
de validation, seuls des changements documentaires sont
autorisés avant le merge utilisateur.



## Renderer natif : frontiere murale sans retry — hardware PASS (15 octobre 2026)

Depuis `main c5384c7664f062a64b0e7ed3c928b2dcde8cd89b`, code
`a71ed3c4cb0b12e2a0ad6ab2e06292637bbb39d5` sur
`agent/esp32-wall-boundary-inline`. [PR #215](https://github.com/Kharn27/DoomRPG-RE/pull/215),
[CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38728936980),
[CI PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38729192919).
Le renderer conserve la semantique historique du sampler des murs :
2048 octets de texels packed pour un mur 64x64, avec possibilite
d'un octet provenant de la texture successeur aux limites. La
resolution inline `BOUNDARY-INLINE` utilise mappings.bin et
wtexels.bin via la lease PAK existante, reutilise le garde sur
le meme rendu, et ne refait plus l'image. Quatre gardes BSS
de 16 B max, `SPAN_OOB` fail-closed si indice >2048,
successeur/PAK invalide ou limite depassee. Pas de tableau
`mediaTexels`, pas de changement des objets gameplay/sauvegarde/RNG.

Test de preuve hote (vraies fonctions sampler/resolver compilees) :
```sh
python3 ESP32/test/native_wall_boundary_inline_host.py
```
2 000 cas deterministes, nibble/pixel, garde reutilisee,
octet exact du successeur, refus borne et pixels sentinelles
preserves sur les erreurs. Pour reproduire sur classic CYD :
```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
LOAD slot07 -> Entrance -> STRAFE_LEFT 310->309. Le CYD a emis
`BOUNDARY-INLINE logical=15 actual=40 source=20480
successorActual=68 successorSource=32768 byte=11` ;
meme temoignage au retour STRAFE_RIGHT 308->309. Aucun
`RETRY/RECOVERED` et pas de `NATIVEFRAME FAILED` visibles.
LOAD V11 identique, portes et combats actifs, RNG guard intact,
RAM residente `heap8=112328 largest8=86004`,
`shapeData/mediaTexels=NULL`. Le test n'etablit pas un hash
de frame identique a la reference desktop ni un delta FPS
mesure. L'ancien probleme SPAN_OOB dans les autres sections
de documentation est historique, pas une anomalie ouverte
observee sur ce cas depuis le correctif. Apres le PASS,
**ne modifier que la documentation**. Merge par l'utilisateur.



## Lecteurs RWops de Config — extraction native (15 octobre 2026)

Code testé `eaa915aacf5171dbd118758ab33bb402f9f9edee`;
[CI PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38717722550).
`src/esp_doomrpg_config_read.c` est maintenant propriétaire
des deux corps identiques au desktop `File_readByte` et
`File_readInt`. Suppression des corps du `DoomRPG.c` généré
avec garde d'empreinte/égalité du générateur et contrôle de
l'ELF (exactement 2 exports dans la TU native, zéro dans le généré).
Preserver `char` signé, `SDL_SwapLE32`, et la sémantique SDL_RWops :
**ne pas prétendre avoir corrigé les lectures courtes ou NULL**.

Tests locaux reproductibles :
```sh
python3 ESP32/test/doomrpg_config_read_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
CI : parité hôte 4 096 appels, 8 194 octets, profils
normal/trace/bringup PASS. Vrai CYD normal : boot, absence
de Config traitée correctement, LOAD V11 slot07 via le HUB,
restauration exacte des overlays et activation, gameplay,
attaques et ripostes avec RNG probes intacts, RAM stable,
`shapeData/mediaTexels=NULL`. **La trace indique
`Config file present=no` : aucun fichier Config réel n'a
été lu physiquement.** Ce PASS couvre l'intégration et
l'absence de régression sur ce chemin, pas un parse physique
de Config présent. Si un futur test utilise un Config réel,
valider sa lecture sans modifier le fichier de sauvegarde.
Le renderer signale toujours `LEGACY_GUARD/SPAN_OOB/RECOVERED`,
hors périmètre du lecteur binaire. Pas d'autre changement
de firmware après cette validation.


## RNG provider extracted — hardware PASS (15 October 2026)

Permanent owner: `esp_doomrpg_process_rng.c`. Four exact legacy
functions and process-lifetime hidden globals `_seed` and
`resetRand` moved out of generated DoomRPG.c. Generator pins CRC
and body parity. Byte/int replay guard unchanged.
Code `a7968169a0af2e2ad93322214547089102b2d292`,
[CI PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38714139429):
12,000 RNG operations per scenario across 4 initial ticks,
reservation and rollback, word/byte boundary, three builds and ELF.

CYD PASS: LOAD V11 slot07, hundreds of actions, combat,
multi-hit sprite286 `rngExact=yes`, save slot06,
Entrance full 6/6 secrets and 30/30 monsters; transition to Junction.
Real `MONSTERMOVERNG next=127->0 prepared=1` followed by
`COMMIT reservation=consumed-by-live-move`.
Heap and null legacy map invariants preserved.
Repeated compact renderer `LEGACY_GUARD/SPAN_OOB/RECOVERED`
remains unresolved. This is a scoped, non-exhaustive RNG proof.
Run `python3 ESP32/test/doomrpg_process_rng_host.py` and flash
normal `pio run -e esp32-cyd -t upload -t monitor` for regression.


## Process clock natif : validation CYD (15 octobre 2026)

Base du lot : `main e99286b9dc1c3a120f24f76ed3741ee22cc6be0c`,
branche `agent/esp32-extract-doomrpg-process-clock`, commit de code
`d757502976f3eaaa37e5bf28bc8dd63c7e9c002a`.
`esp_doomrpg_process_clock.c` possède les corps intacts de
`DoomRPG_GetTimeMS` / `DoomRPG_GetUpTimeMS` ainsi que
`basetime` ; le générateur les retire strictement de `DoomRPG.c`.
**Ne pas réinitialiser basetime** à l'ouverture d'une carte ou au LOAD ;
respecter la sentinelle zéro et les ticks SDL Uint32.

Validation reproductible :
```sh
python3 ESP32/test/doomrpg_process_clock_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
[Test CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38711767983) :
10 000 pas hôte d'horloge en ordre mixte, trois profils compilés,
audit `--require-native-doomrpg-clock` (définitions et BSS).
CYD réel normal : START→intro→Entrance→LOAD slot07, déplacements,
rotations, dialogues, portes, pickup, attaque/riposte de sprite5,
HUB SYS EXIT vers MENU_MAIN `522dc605`, reset résident vide.
LOAD restaure monster/topology/position/activation FNV ; heap8
`112328` stable après LOAD puis `111184` avec contexte dialogue ;
`shapeData/mediaTexels=NULL`. Pas d'écriture SAVE.
Le renderer compact `LEGACY_GUARD/SPAN_OOB` reste un incident
indépendant et ouvert ; ne pas confondre recovery et résolution.
Cette documentation ferme **le lot horloge seulement** ; les commits
RNG ultérieurs imposent une nouvelle validation matérielle.


## Extraction DoomRPG — présentation SDL/clip native (CYD validé le 15 octobre 2026)

La famille de sept méthodes **réellement liées** de présentation
`DoomRPG_setClipFalse/True`, `DoomRPG_setColor`,
`DoomRPG_setFontColor`, `DoomRPG_fillRect`,
`DoomRPG_drawLine` et `DoomRPG_flushGraphics` a un propriétaire
permanent : [`src/esp_doomrpg_presentation.c`](src/esp_doomrpg_presentation.c).
Le script [`scripts/build_engine.py`](scripts/build_engine.py)
contrôle les corps adaptés (CRC + égalité exacte) et enlève ces sept
définitions de la **copie générée** de `DoomRPG.c`.
L'original desktop reste intact. Aucun framebuffer supplémentaire ;
le raster utilise le contexte SDL/CYD existant et le framebuffer
**160×120 RGB565**, avec son clip et les couleurs habituelles.
`drawRect` et `drawCircle` n'étaient pas parmi les exports liés
identifiés : **ne pas les compter comme migrés**.

Vérification hôte :

```sh
python3 ESP32/test/doomrpg_presentation_host.py
```

Le test compare les vraies définitions adaptées aux sources natives
et exerce **1 600 opérations déterministes** de clip, rectangles,
lignes, changement de couleur/font, flush et sentinelles dans un
simulateur SDL/RGB565. Pour le binaire, compiler les trois profils
`esp32-cyd`, `esp32-cyd-trace`, `esp32-cyd-bringup` et utiliser
l'audit `tools/audit_desktop_elf.py` avec
`--require-native-doomrpg-presentation` (intégré en CI).
Dernier code testé `1840009295909088f97d94b806320585ef832e41` ;
[CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38693375415).
Pour flasher le **profil RAM de référence** sur classic CYD sans PSRAM :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

La validation matérielle de l'utilisateur couvre les transitions
UI et MAP_INTRO, les dialogues et le monde, les déplacements/rotations,
l'attaque des monstres, le chain-blast de trois barils, les effets
visuels et SAVE V11 / LOAD. Test post-trigger utile à conserver :
après ouverture de porte `line=87`, activation
`sprite=10 activeCount=21` ; SAVE slot07 à `1632,992 / 192`
(dos au monstre) puis LOAD et un PASS_TURN **sans rotation**.
`activation FNV aca2058f` et `monster FNV 966ac091` sont
restaurés ; le monstre `sprite=10` attaque avec
`DAMAGECUE side=BACK`. `shapeData/mediaTexels` restent NULL et
`heap8=112128 largest8=86004` dans ce passage.

**Portée limitée** : une même activation après LOAD n'établit pas
que tous les monstres SHOW par trigger sont restaurés correctement.
Les protections `NATIVEFRAME LEGACY_GUARD/SPAN_OOB/RECOVERED`
restent un problème de renderer compact distinct. Tests hôte,
CI et playtest ne prouvent pas la parité absolue des pixels,
ni une accélération. La présentation ne doit pas être utilisée
pour réintroduire un framebuffer ou la dépendance ZIP runtime.


## Deuxième extraction DoomRPG — images/PAK (CYD accepté le 15 octobre 2026)

Sur la même branche que l'extraction math/binary,
`esp_doomrpg_image_resources.c` possède maintenant
`DoomRPG_createImage`, `DoomRPG_freeImage` et
`DoomRPG_fileOpenRead`, retirés de la **copie C générée**. Il
préserve strictement les corps **déjà adaptés** pour PAK et
`Esp32Bmp_LoadRW`, sans lecture ZIP runtime ni map-wide
`mediaTexels`/`shapeData`. La source `src/DoomRPG.c` est toujours
une référence, pas une unité compilée telle quelle.

Test hôte depuis la racine :

```sh
python3 ESP32/test/doomrpg_image_resources_host.py
```

Il épingle les CRC des corps d'origine/adaptés et vérifie
l'owner concret : lecture allouée PAK, nom slash, RGB565,
color key, tailles, passage surface→texture, imageMemory,
free et double-free sans texture ; les interfaces BMP/PAK/SDL
sont **mockées**, pas des lectures SD physiques.
Le générateur refuse une différence de corps.
CI commit `e648f2fc` :
[push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648524728)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648529981)
sur normal/trace/bringup et vérifications compiledb/ELF.

Le boot utilisateur confirme BMP indexés 144×72 / 108×74,
logo et menu natif, READ du slot V11 07 depuis MENU_MAIN,
entrée /intro.bsp CRC `623f34e4`, viewport, HUD, PAK,
mouvement 552→520, porte ouverte et combat du sprite 106
jusqu'à corpse/drop, avec feedback FRONT et
`heap8=112328 largest8=86004`, `shapeData/mediaTexels=NULL`.
**PASS ciblé du chargement et de la session**, sans test direct de
teardown et sans preuve d'accélération sur le même scénario.
Ne pas interpréter la différence entre `Wrote 809936 bytes`
(esptool, image écrite) et `809565 B Flash` (mesure CI PlatformIO)
comme une incohérence de taille : ce sont deux métriques distinctes.

L'incident dormant/invisible après LOAD reste à reproduire séparément.
Pour toute future migration d'un provider lié, build/test CI puis
flash du profil **normal** et logs réels avant doc de clôture.
Le commit documentaire ne touche pas aux fichiers de firmware.



### Clôture matérielle du lot DoomRPG math/binary — 15 octobre 2026

Depuis `main bb5b599f`, la PR [#212](https://github.com/Kharn27/DoomRPG-RE/pull/212)
sort six helpers stateless de la copie générée `DoomRPG.c` sans déplacer
les RNG, clocks, images ou états persistants. Owner source :
`src/esp_doomrpg_primitives.c`. Test reproductible depuis la racine :

```sh
python3 ESP32/test/doomrpg_pure_primitives_host.py
```

Il compare les corps C desktop pinés et natifs (60 000 vecteurs seeded,
endian, offsets et quelques limites) ; le générateur rejette un drift CRC
ou une divergence exacte de corps. CI sur `9fec89d1` :
[38645127794 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38645127794),
trois firmwares + tests + gardes compiledb/ELF.

**Log CYD utilisateur sur le firmware normal demandé** : boot/menu PAK
READY ; LOAD V11 slot 07 /intro.bsp ; monde visible après session prime ;
mouvement `552->520` ; porte ouverte, sprite 106 frappé trois fois
(7→5→3→0 HP), retaliation 27→23 HP, feedback directionnel FRONT
superposé au cadre rouge. `shapeData` et `mediaTexels` restent NULL,
`heap8=112328` et `largest8=86004` à la session. Pas de SHA flash
imprimé dans la capture ; tests qualitatifs ciblés, pas tous les états du
jeu ni preuve d'égalité binaire. Les anomalies post-LOAD préexistantes
restent indépendantes et ouvertes.

**Procédure pour le lot suivant :** construire les trois profils, comparer
les références source + ABI, et tester uniquement `esp32-cyd` sur le vrai
CYD avec boot, LOAD, mouvement, combat et Serial. Tant que le provider
lié change, ne pas réutiliser une preuve d'identité ELF du jalon Render.c
pour éviter un reflash. La documentation après PASS est docs-only.



Documentation **opérationnelle** du port classic CYD. Pour l'état exact et les limites hardware : [`PORTING_STATUS.md`](PORTING_STATUS.md). Pour le design de long terme : [`ARCHITECTURE.md`](ARCHITECTURE.md).

## Matériel et stockage

- **ESP32-2432S028R classic CYD**, 4 MB flash, sans PSRAM ; écran ILI9341 320×240, touch XPT2046, carte microSD.
- Framebuffer logique 160×120 RGB565 (38 400 octets), présentation nearest-neighbor ×2.
- Fichier runtime obligatoire sur SD : `/DoomRPG-ESP32.pak`.
- `/DoomRPG.zip` n'est utilisé que comme source/référence pour la génération ; **aucune lecture ZIP en jeu**.
- L'audio est hors périmètre de priorité immédiate. Ne pas déplacer la mémoire graphique vers des buffers map-wide.

## Build, flash et logs

À partir de `ESP32/`, environnement normal (celui à tester sur le vrai CYD) :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t upload -t monitor
```

Selon le poste, PlatformIO peut être invoqué avec `platformio` au lieu de `pio`. La cible d'upload configurée dans le projet peut dépendre du port série (historique : `/dev/ttyUSB0`).

`esp32-cyd-bringup` sert aux diagnostics/probes supplémentaires ; **ne pas** tirer de conclusions RAM production à partir de ce profil. La vérité matérielle vient du boot et des interactions réelles, via Serial.

### Diagnostic d'un incident gameplay

`esp32-cyd-trace` hérite de **la production** : même layout, optimisations
release, stockage et propriétaires. Il active TRACE sans définir
`DOOMRPG_ESP32_BRINGUP_PROBES`, sans overlay tactile et sans probes de startup.
Les écritures Serial modifient le timing : ce n'est ni un benchmark ni une
preuve d'identité du firmware normal. Bringup reste un diagnostic plateforme,
avec d'autres layouts et un démarrage différent, pas un simple « mode debug ».

Les anciens profils `esp32-cyd-story-teardown-probe` et
`esp32-cyd-divider-probe` et leurs blocs firmware sont retirés. Leur couverture
utile est exercée par `test/diagnostic_profiles_host.py` : vrais formatter,
story-hand et `DoomCanvas_free`, I/O image/cleanup intro mockés. Pas de preuve
PAK ou d'équivalence heap ESP32 apportée par ce test hôte.

Pour l'incident zombie actuel, `DOOMRPG_ESP32_TRACE_SPRITE=106` dans
`platformio.ini` sélectionne **l'index immuable du sprite de la carte**, pas
son subtype ni son ordre d'activation. Changer uniquement cette valeur pour
un autre incident, sans créer d'environnement par monstre. `[SPRITETRACE]`
observe les données déjà lues par le renderer : position projetée, tile et
flags topology, pose, caméra, preview, admission hidden/BSP/candidate,
near/backface/clip/frame/span failure ou raster avec nombre de pixels.
`result=rasterized pixels=0` n'est pas une preuve d'affichage visible ; un rejet
ou une frame réussie ne suffit pas à diagnostiquer un rollback gameplay.
Les déplacements commis et décisions de chaque membre deviennent également
visibles via les logs TRACE existants. Aucun buffer ni owner persistant ajouté.

Depuis `ESP32/` :

```sh
pio run -e esp32-cyd-trace
pio run -e esp32-cyd-trace -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Fermer tout autre lecteur Serial avant le monitor. Vérifier au boot
`[TRACEPROFILE] production-path sprite=106 ... bringup=no`, puis charger la
sauvegarde existante près du zombie **sans la réécrire**. Capturer la sortie
depuis LOAD, tous les tirs jusqu'à la disparition, puis le premier mouvement
qui le fait réapparaître. Conserver les lignes complètes, pas seulement les
SPRITETRACE. Une sauvegarde est une reproduction, pas la preuve d'une cause.
Après correction éventuelle, refaire le scénario avec `esp32-cyd` normal.
Ce jalon ajoute l'observabilité ; il ne corrige pas encore la disparition.

Le jalon depuis main `b941048` retire la copie C desktop Render générée dans
les deux profils. L'owner géométrique natif sert maintenant aussi bringup ;
normal reste bit-identique chargé dans la comparaison locale. DoomRPG.c reste
généré pour ses utilitaires/états réellement liés, et le shim SDL packed reste
fonctionnel. Cartographie ELF et priorités : `CONSOLIDATION_AUDIT.md`.

Pour refaire la cartographie sur **un build courant**, depuis ESP32 :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t compiledb
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --calls
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --require-retired-render
```

Répéter avec bringup et régénérer compiledb pour le profil choisi. Les vieux
Render.o/Render.c de .pio ne prouvent pas une compilation ; SCons/map/ELF sont
croisés. Le CSV b941048 est un snapshot de la base **avant** retrait, pas un
inventaire automatique du HEAD. Un symbole lié ou un call direct ne prouve
jamais un chemin réellement exécuté sur le CYD.

L'ancienne suite graphique `menu.bsp`/ZIP est retirée : son point d'entrée
retournait déjà 0 pour tous les builds ESP32. Le rapport bringup `[MENUBSP]
RETIRED` et `MENU BSP FAIL` reste donc attendu ; il ne teste pas le menu opaque
production. Les diagnostics actifs première frame, CHANGEMAP, opcodes et touch
ne sont pas supprimés avec cette suite historique.

La politique de logs compile-time est définie dans `ESP32/include/doomrpg_log.h` : `DRPG_LOGE/I/D/T`. Normal = INFO, bringup = TRACE. Les messages de succès récurrents sur VIDEO, PAKIO, redraw et tour doivent rester hors du flux INFO ; ne pas masquer les erreurs, transitions, guards et recoveries.

Mouvements de monstres : le normal conserve les résumés BEGIN/COMPLETE,
ORDERED-DISPATCH, les erreurs de mouvement/publication, ainsi que les témoins
attaque-probe/retaliation nécessaires au watcher. Les succès PROBE/ALLOW/COMMIT,
obstacles de pathfinding ordinaires et détails MEMBER/PAUSE/RESUME sont TRACE ;
les mêmes PROBE/prepare/MEMBER restent affichés si rollback/admission/publication
échouent. Utiliser **esp32-cyd** pour juger la latence : bringup garde les logs
exhaustifs. Ce retrait de texte ne change ni le nombre de redraws, ni le RNG,
ni la cadence d'attaque ; pas de gain FPS hardware revendiqué.

Le dernier retour CYD confirme l'absence de gain ressenti et un SAVE V11 slot 3
suivi du retour au jeu/expiration de « Game saved » ; pas de LOAD dans cet
extrait. Le goal 2/3 de sprite 202/subtype 4 reste différé sur une géométrie
special-plane non possédée (premier déplacement déjà commis), code inchangé
depuis la base main. Détails et limites de preuve : `PORTING_STATUS.md`.

Rollback de l'interaction opcode 23 depuis automap : en cas d'échec de
démarrage du script/dialogue, le resident restaure son mode et redessine
l'automap ; depuis WORLD, il redessine le monde. Si le redraw de restauration
échoue, le gameplay se désactive. Le test hôte utilise le vrai routage/mode
resident avec startup/render/assets mockés ; il ne prouve pas un échec I/O
réel sur CYD. Pour un smoke matériel, ouvrir normalement le dialogue depuis
automap ; ne pas retirer/corrompre le PAK pour fabriquer une panne.

## Feedback directionnel des dégâts et réserves post-LOAD

Sur le CYD, un hit monstre non mortel commité affiche le contour rouge
historique de 2 px logiques / 500 ms **et**, pendant cette période lorsque
le compositor publie immédiatement la première impulsion, une bande de
10 px logiques (20 px physiques) dans le côté de provenance réel :
`FRONT` en haut de la vue, `RIGHT` à droite, `BACK` en bas,
`LEFT` à gauche. Deux impulsions de 85 ms avec 45 ms de pause ;
le feedback est distinct du séquenceur IA et ne doit pas retarder
l'attaquant suivant. En INFO, comparer `[MONSTERRETAL] COMMIT`,
`[VIEWFLASH] PAINT/EXPIRE`, `[DAMAGECUE] QUEUE` et
`[DAMAGECUE] ON ... redBorder=visible presented=1`. Le
`concurrent=yes` du log signifie que les deux effets *peuvent* coexister,
pas que chaque impulsion a réellement recouvert le rouge : c'est
`redBorder=visible` qui l'atteste.

Correction de synchronisation après la capture `814a5034...` : le rectangle
était présenté dans la transaction de dégâts, mais la direction ne démarrait
qu'au prochain service, après les autres monstres. Le rectangle expirait
alors avant l'impulsion (818 ms dans le témoin à distance).
L'enqueue **après commit réussi** tente désormais immédiatement la première
impulsion via le compositor existant, sans nouveau rendu monde ni attente
des deux clignotements. La direction est servie avant l'expiration des effets ;
le rouge ne se restaure pas sous une direction active **ou encore en file**.
Les barrières touch/PAK/level-up/transaction et retry/timeout UI restent actifs,
la file reste ordonnée, et un échec du présent UI ne rejoue pas les dégâts.
Ce présent overlay borné peut ajouter le coût d'un transfert écran ; ce n'est
pas une optimisation de performances. Les durées 500/85/45 ms sont nominales :
un loopTask occupé peut allonger l'affichage, sans attendre ni bloquer le tour
logique pour terminer le feedback. Tests hôte du vrai service/compositor
avec clock/assets/présentation mockés. **Acceptation matérielle qualitative
du correctif donnée le 14 octobre 2026** : l'utilisateur n'a pas reproduit la
désynchronisation et accepte le résultat. Aucun nouveau log/SHA de boot/profil
fourni ; ce n'est pas une validation exhaustive des cas ci-dessous ni une
mesure de timings. La preuve logicielle et ce retour matériel restent distincts.

Scénario de non-régression à conserver sur **esp32-cyd normal** : hit zombie à distance avec plusieurs monstres
actifs, puis hit au corps-à-corps ; vérifier la première impulsion sur le rouge,
et l'absence de pixels persistants après un mouvement/changement d'arme.
En Serial, attendre `DAMAGECUE ON ... redBorder=visible` pour le premier hit
avant les traitements suivants, pas `VIEWFLASH EXPIRE` puis `ON ... none`.
Le log `MONSTERRETAL COMMIT` peut suivre le `DAMAGECUE ON` : le commit des
owners player/RNG/frame est déjà clos avant l'enqueue, le texte est imprimé
après. L'incident distinct du zombie invisible reste ouvert/non reproduit.

Pour un tonneau, tester séparément la cible et son explosion.
Un blast **unique** mortel (`chain=1`) peut désormais mettre
`playerHP=0` dans l'API terminale, réserver exactement un octet RNG
de mort après le blast et avant les conséquences monstres, puis armer le
vrai menu de mort une fois la transaction commise. Chercher
`BARRELRADIUS PLAYER-HIT`, `DEATH-RNG-RESERVE`,
`MONSTERRADIUS COMMIT`, `LETHAL-COMMIT`, `PLAYERDEATH ARM/READY`.
Le vrai CYD a validé ce chemin avec deux monstres voisins ; une
**chaîne létale de plusieurs tonneaux** reste explicitement non supportée.

Lorsqu'un monstre né d'un trigger `SHOW` refuse de bouger après
LOAD jusqu'à ce que le joueur se retourne, **ne pas** réactiver tout
le voisinage ou scanner à 360° comme correctif. V9/V11 enregistre déjà
activation + ordre (snapshot 268 B), topologie et positions. Garder
un slot avant le trigger et un autre après apparition/activation,
puis comparer `[MONSTERACT] ACTIVE` avant SAVE,
`[NATIVESAVE] SAVE ... activation=count/FNV`,
`[MONSTERACT] RESTORE ... activeCount/stateFNV` et le premier
`[MONSTERACTIVESEQ] BEGIN/COMPLETE`. Une apparition après rotation
prouve une activation par visibilité BSP, **pas** une perte du snapshot
à elle seule. L'enquête root-cause SAVE/LOAD/SHOW doit rester un
jalon distinct ; détails dans `PORTING_STATUS.md`.

## Playtest libre : captures automatiques des incidents

Pour jouer normalement **sans surveiller le terminal**, depuis `ESP32/` :

```sh
# Python utilisé par PlatformIO a généralement déjà pyserial :
python3 -m pip install pyserial  # seulement si non installé
python3 tools/playtest_watch.py --port /dev/ttyUSB0 --bell
```

Cet outil remplace le moniteur Serial pendant le playtest : **ne pas ouvrir `pio device monitor` simultanément**, car les deux processus se disputeraient le port série. Aucun reflash n'est nécessaire. Les règles explicites reconnaissent les frontières non implémentées ou les anomalies importantes (ex. `[HUB] SELECT-DEFER ... kind=item`, `[RESIDENTGAMEPLAY] SELECT-DEFER`, `[MONSTERMOVE] DEFER`, `[TURNFRAME] DIAG fail=WORLD_RENDER`, `[HUB] CLOSE ... exactHud=NO`). Les simples textes `sound=...-deferred`, `turnAdvance=deferred` et les messages READY ne provoquent **pas** d'alerte.

À chaque incident : une alerte `[WATCH]` et un fichier `playtest-captures/incident-....log` de ~75 lignes précédentes, le déclencheur, puis jusqu'à 20 lignes suivantes. Un cooldown de 45 s par catégorie limite les duplications ; ce n'est **pas** une garantie de capture exhaustive de chaque occurrence. `Ctrl+C` arrête seulement le moniteur PC et écrit `snapshot-....log` contenant les dernières lignes, utile si un problème visuel ne correspond à aucune règle. **Le CYD continue à tourner : aucun arrêt logiciel, watchdog, ou écriture microSD n'est impliqué.** Ne pas confondre capture du contexte et pause du gameplay.

Options : `--echo` réaffiche tous les logs, `--before 120 --after 30` augmente le contexte, `--cooldown 90` réduit les alertes. On peut valider sans matériel avec `python3 tools/playtest_watch.py --self-test`, ou lire un flux enregistré : `python3 tools/playtest_watch.py --stdin < ancien-log.txt`. Les fichiers de capture sont ignorés par Git. Pour une pause du jeu réellement ciblée sur des actions non possédées, ajouter plus tard un mode diagnostic ESP32 **opt-in** avec reprise explicite, jamais un `while(1)` qui déclenche le watchdog.

Cheats : jalon séparé, à auditer par rapport aux commandes réelles du legacy ; ne pas inventer de code secret ni modifier les progressions de sauvegarde dans ce jalon.

## Régression native : projectiles de monstres et tonneaux

Le **sprite logique 204** est une ressource de projectile créée en combat, pas un sprite BSP permanent. Sa paire de mappings est ajoutée *sparsely* au catalogue **avant** le gameplay dès que la topologie contient un Imp (subtype 3) ou un Malwrath/Cacodemon (subtype 6). Ce chemin de présentation est autorisé **uniquement pour l'attaque primaire** de ces deux subtypes : Imp = un projectile, Malwrath = trois. Chaque projectile est un effet monde transitoire, peint aux positions bornées par le renderer, sans ajouter de sprite persistant ni consommer de gameplay RNG. Les autres familles de missiles demeurent hors de ce contrat limité.

Sur le vrai CYD, contrôler `[ENGINESESSION] SPRITES ... monsterProjectile204=primed` si l'une de ces familles est présente ; puis, pour une attaque du Malwrath, `[MONSTERATKVIS] ARM ... projectile=malwrath204-3shots-armed`, les `[MALWRATHPROJECTILE] FRAME ... logical=204 ... presented=1` (trois points par vol) et deux `REARM shot=2/3` / `shot=3/3`. Le `[MONSTERATKVIS] COMPLETE` doit précéder la résolution réelle `[MONSTERRETAL] COMMIT` ou `MISS-COMMIT`; les HP/armure/RNG et le tour restent les propriétaires gameplay existants. Un `projectile=none` sur cette attaque primaire, des `DEFER` de rendu ou un sprite 204 absent ne sont **pas** un PASS. La validation matérielle ciblée du Malwrath est consignée dans `PORTING_STATUS.md`.

Pour les tonneaux, distinguer **visée** (`[ACTIONENGINE] TRACE ... route=BARREL_SUBTYPE1`) et **préflight du rayon d'explosion** (`[BARRELRADIUS] PREFLIGHT` ou `DEFER`). Une ligne liée `type=0` est inerte pour `Game_hurtEntityAt()`, quel que soit son subtype, sans rendre les autres entités destructibles ignorables. Quand plusieurs tonneaux et monstres participent à la chaîne, l'ordre natif est tirage de l'explosion, dégâts/conséquences et RNG des monstres, puis explosion suivante, dans une transaction à rollback borné ; chercher `[MONSTERRADIUS] SEQUENCE-BEGIN`, `[BARRELRADIUS] BLAST` et `[BARRELRADIUS] INTERLEAVED-COMMIT`. **Ne pas qualifier un ancien `DEFER` ou une impression visuelle seule de PASS ciblé** : reproduire le scénario exact et vérifier la consommation d'ammo, les monstres, les drops et l'absence de rollback partiel.

Enfin, si les taps remontent en `[TOUCH]` mais que `[ALIVE] SD=unavailable PAK=unavailable ... MENU=unavailable`, rechercher l'échec du montage SD avant d'incriminer le menu ou les correctifs gameplay. Un firmware qui démarre sans `/DoomRPG-ESP32.pak` ne peut pas initialiser le jeu.

## Validation d'une nouvelle frontière

### Interactions payantes avec dialogue et attente

Le script original ne propose pas de YES/NO : terminer le dialogue d'offre
avance son état, puis une nouvelle interaction contrôle/débite les crédits.
Il est toutefois possible d'interrompre **EV_DIALOG** avec sa carte **BACK**
tactile (en haut à gauche du viewport, au-dessus du texte). Ce tap annule sans
reprise du script ni débit ; ailleurs, le tap reste SELECT (texte/page/suite).
**EV_DIALOGNOBACK**, notamment après paiement, n'a pas cette carte et reste
SELECT-only : aucun remboursement ou nouvelle branche script n'est ajouté.

Le SELECT natif admet maintenant un `EV_REQSTAT` crédits (23, stat 2) suivi
du débit `EV_DECSTAT` correspondant (22), d'un dialogue sans Back (26), des
particules de capture (35, famille 3), d'un son publié comme intent silencieux
(42) et d'une attente (37). Après l'attente : SHOW/HIDE, refresh de vue (36),
note (40) et mutations d'état (11/19/20). Les opérandes et toute cette fermeture
sont préflightés avant le paiement ; SHOW utilise les gardes natives existantes.
Les autres stats, particules et formes de chaîne restent fail-closed.

Fonds insuffisants : texte BSP de l'opcode 23, aucun débit ni continuation.
Paiement : snapshot PlayerState et rollback si l'ouverture du dialogue échoue ;
le débit est commité seulement lorsque le dialogue est présenté. WAIT garde
l'entrée monde fermée, sans sleep ni action différée derrière le timer. Le
timer monotonic démarre après la première frame d'effet réussie, accepte le wrap
32 bits, et reste borné à 10 s. Le fond de l'effet passe par `Frame_composeTurn`
(pas de present/activation/automap/fresh-frame), puis l'effet est ajouté et une
seule frame physique publiée. Ne pas armer PresentGate avant renderTurn : sa
garde refuse cette composition et laisserait le WAIT armé indéfiniment.
Une note est préparée dans le candidate/scratch
NOTE existant et commise après le rendu final ; échec I/O/rendu = rollback du
segment et retry, pas de second paiement. Pas de nouveau fichier moteur ni de
nouveau propriétaire Player/Notebook/topologie/PAK.

**PASS CYD ciblé consigné** : event67/tile675, débit 19→9, effet publié,
HIDE/SHOW/NOTE, activation puis combat/mort du monstre, retour WORLD et aucune
seconde facture dans la trace. Portée précise et réserve d'identité firmware :
PORTING_STATUS. BACK visible est accepté ; son annulation sans débit n'est pas
prouvée par cet extrait. Pour compléter : depuis une sauvegarde pré-paiement,
annuler l'offre via BACK puis réouvrir ; avec <10 crédits, le dialogue de refus
ne doit changer ni crédits ni monde. Vérifier le notebook à l'écran et SAVE/LOAD.
Les logs doivent montrer `SCRIPTWAIT PRESENT` puis `SCRIPTSTAT COMPLETE` ;
`ARM` seul ne vaut pas succès. Garder les logs
`[SCRIPTSTAT]`, `[SCRIPTWAIT]`, `[NOTE]`, et vérifier `shapeData/mediaTexels=0`.
Le timer, le son silencieux et les mocks hôte ne prouvent pas une parité visuelle
ou RNG exhaustive avec le desktop. Ajouter un smoke-test dialogue ordinaire,
HUB/SAVE/LOAD et SHOW/HIDE (les types/formats de sauvegarde restent inchangés).

### Protocole général

1. Lire le vrai SHA `main`, [`PORTING_STATUS.md`](PORTING_STATUS.md) et [`ARCHITECTURE.md`](ARCHITECTURE.md) ; examiner les fonctions legacy concernées dans le repo.
2. Partir **du SHA exact** après chaque merge ; créer une branche `agent/*`. Jamais de merge automatique dans `main`.
3. Choisir **une famille cohérente bornée** ; introduire uniquement le propriétaire natif durable et les guards de parité nécessaires. Les cas non couverts restent **fail-closed**.
4. CI sur `esp32-cyd` ; vérifier symboles, mémoire, allocations, signatures, flux d'appel, invariants. Les tests locals/CI ne sont pas présentés comme tests hardware.
5. Push de la branche, flash du firmware normal par l'utilisateur, collecte des **logs Serial réels**, comparaison avec le comportement attendu. Diagnostiquer, corriger, committer et push sans redemander.
6. Après PASS : geler la frontière du code testé ; commits suivants **docs-only** avant merge-ready. Toute nouvelle modification de code nécessite sa propre validation.
7. Actualiser **seulement** les faits du statut courant dans `PORTING_STATUS.md`. Pas de nouveau fichier `MILESTONE*.md` par micro-changement. Les commits et PR forment l'historique.

### Mini check-list runtime

- Boot normal, SD et PAK prêts, menu stable.
- Identité de la carte et FNV non contradictoires avec l'état attendu.
- `shapeData == NULL`, `mediaTexels == NULL` ; pas de source ZIP runtime.
- Allocations et cache bornés, pas de changement sournois d'owner mutable.
- Tester uniquement les trajets pertinents à la frontière (START, mouvements, rotations, portes, dialogue, HUD, LOAD, monstres, EXIT si concernés). **Ne jamais présenter les autres comme testés.**
- Sur Render, distinguer explicitement appel réel du chemin retiré et simple non-régression d'une session.
- Le résultat d'un test est documenté avec **SHA de code précis** et éventuelles réserves ; ne pas recopier tous les logs.

## Où trouver les spécifications

| Besoin | Référence |
| --- | --- |
| Objectifs/invariants mémoire/architecture native | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Derniers SHA, RAM, FNV, frontières en cours, caveats | [PORTING_STATUS.md](PORTING_STATUS.md) |
| Implémentation exacte | Code source et scripts de génération dans le repo |
| Parité Doom RPG originale | Sources desktop/J2ME retenues dans le repo |
| Historique d'un jalon | Commits/PR GitHub et fichiers historiques au [snapshot du 8 octobre 2026](https://github.com/Kharn27/DoomRPG-RE/tree/60d34d174bed0d4d4e13137f306070b5018c0fa9/ESP32) |

## Présentation du menu de mort

Après la chute et le fondu existants, le menu de mort occupe le framebuffer
entier et reprend la palette industrielle et les polices partagées du HUB et
du bilan de mission : cadre sombre, bandeau `YOU DIED` rouge, sous-titre
`MISSION FAILED`, puis quatre cartes à deux lignes. Aucun asset ni buffer
supplémentaire n'est chargé ; le propriétaire de mort reste inchangé.

`LOAD GAME` ouvre désormais un sélecteur tactile de **10 slots SD** (deux pages de cinq), qui peut charger l'ancien checkpoint via le slot 1. Deux pressions sur un slot lisible confirment LOAD ; les slots vides sont désactivés. En jeu, HUB → SYSTEM → SAVE/LOAD ouvre le même inventaire de slots, avec sauvegarde V11 atomique indépendante par slot. Pagination : gros chevron supérieur **précédent** (actif page 2), inférieur **suivant** (actif page 1) dans une colonne à droite ; numéro de page dans l'en-tête. Style visuel accepté comme provisoire sur CYD, finition Doom à reprendre en fin de port. Le slot AUTO n'est pas implémenté.
`GO TO JUNCTION`, `RETRY SECTOR` et `MAIN MENU` restent visibles et atténués,
avec `NOT AVAILABLE` : leurs backends ne sont pas implémentés par cette refonte.
Le tap unique LOAD, son dispatch différé au service et les autres actions
fail-closed sont conservés. Les cartes font 140×19 pixels logiques, à
`x=10..149`, `y=33..51 / 54..72 / 75..93 / 96..114` ; peinture et hit-test
utilisent les mêmes constantes. Les marges et interlignes ne déclenchent rien.

Ne pas confondre `MAIN MENU` sur cet écran de mort, encore différé, avec
`SYS → EXIT TO MENU` dans le HUB en jeu : ce dernier possède son propre retour
natif fonctionnel. La refonte graphique ne débranche ni ne modifie cette route.

Validation locale : commande du test dans [`test/README`](test/README), build
`esp32-cyd` et inspection du vrai painter avec/sans sauvegarde. Ces preuves
ne remplacent pas le matériel. Sur CYD : mourir avec un checkpoint, vérifier
la lisibilité puis charger et reprendre le gameplay ; répéter sans checkpoint,
vérifier `NO SAVE` et que les options différées ne quittent pas l'écran.

## Contrat d'évolution

L'audit architectural depuis `main 9d203e5` est dans
[`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md). Son inventaire distingue
sources présentes, unités compilées, sections liées et preuves d'exécution.
Premier lot : retrait de neuf anciens markers post-load Junction inaccessibles,
sans fusion d'états actifs. La comparaison des sections allouées des deux ELF
est stricte (octets/adresses/layout), avec `tools/compare_elf_alloc.py` ; les
tests hôte et la CI n'attribuent aucun nouveau PASS gameplay/matériel.

Deuxième lot sur la même branche : retrait de l'ancien renderer/probes de
menu ZIP (16 sources, 13 headers), quatre directives de wrap inutilisées et
allègement du diagnostic fermé. Après les deux lots : **158 sources C/C++,
152 headers include**. Sections allouées et RAM/flash inchangées dans les deux
profils. Trois fixtures SYS/progress/death-menu sont réalignées et intégrées
à la CI ; elles ne valident pas la restauration SD réelle. Les bridges de menu
et caches encore liés/partagés restent explicitement différés dans l'audit.

Ne pas introduire de runtime propre à une map, de gros index pointer-heavy, ni de nouveaux wrappers universels simplement pour remplacer ceux du desktop. Le PAK reste le backing store, `EspMapRuntime` reste immuable, chaque mutation a un propriétaire explicite. Différer l'optimisation de `PlatformVideo_present()` : Doom RPG est turn-based.

La documentation ne doit plus être un second journal de développement : décrire **le fonctionnement actuel**, son protocole de test et ses limites. L'historique exhaustif vit dans Git.
