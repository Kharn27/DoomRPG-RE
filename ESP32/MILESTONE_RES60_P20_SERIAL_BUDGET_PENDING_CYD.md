# RES60-P20 — budget Serial des rotations chaudes (PASS CYD borné)

**PR #364 DRAFT / branche agent/esp32-res60-p1**, après merge non-force de main `ca7fd44b595660a5e1f7ed259dd11a97d84d3d69` au commit `4e16ad6508feca4a836fb59ddc9fb4b29029f1e0`. Le hardware P19 valide le code antérieur `75fc0854fe1edf57ff68f98d9970f5375fd4edb0` mais **ne valide PAS la combinaison main ni P20**.

## Cause motivante mesurée sur le vrai CYD
Log P19 du 24/11 `platformio-device-monitor-261124-091702.log`, SHA256 `d1042ec297ec5dde35ff0e5d76b20b19ad2543e231443e881c45fec7a9d0fb72` : 37 vues, 32 rotations, 5 START/MENU, N64 cache HIT : `preparedUs≈97411` mais `FRAME totalUs≈259000`, E0 HIT 57.8ms prepared contre 219ms FRAME, S192 38.8ms contre 190ms, W128 44.4ms contre 196ms. Les journaux P8/P9/P10/P11/P12/P13/P17/P7 totalisent plusieurs longs blocs Serial **entre** preparedUs et l'affichage. Ces journaux peuvent bloquer à 115200 bauds, mais ce n'est pas encore isolé par A/B matériel.

## Changement unique proposé
`DOOMRPG_RES60_P20_COMPACT_SERIAL=1` uniquement dans le profil expérimental RES60, avec P18/P13/P11/P1 obligatoires. Si **mapLeaseHit==1 ET texture.cacheHit==1**, émettre une unique ligne `[RES60P20] WARM` comprenant angle, turns, mapUs, raysUs, preparedUs, tests, quatre FNV (brut/final/matériau/swatch), et heap8/largest8/minHeap8. Garder le `[RES60P3] MAP` source et `[RES60P7] FRAME` avec `totalUs`, plus le PRESENT LCD. Au moindre MISS ou état mixte, tous les anciens journaux détaillés restent intacts. Le chemin `P20=0` conserve entièrement le verbose P19 pour A/B.

**Aucun changement** au map runtime, raycast, BSP, UV, PAK, pixels, input, allocations, framebuffer 55296B ou `PlatformVideo_present()`; aucune optimisation prématurée du LCD. Production 160x120 exclue. Tests C11 exercent la vraie décision warm/cold et les gardes de compilation, puis CI complète avec builds normaux et RES60.

## Critères hardware P20
`cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. START puis N/E/S/W 2 fois, MENU, START à nouveau. Observer `[RES60P20] WARM` seulement sur frames HIT ; sur MISS, anciennes lignes P8/P17/P9/P10/P11/P12/P13/P7. Comparer P19 **raysUs / preparedUs / totalUs** et 4 FNV, P10, RAM, PAK closed, compte START/MENU. Si réduction du `totalUs` sans régression de `preparedUs` et FNV : gain UX de trace confirmé. Si Serial n'explique pas les 100ms, ne pas prétendre l'inverse ; investiguer ensuite.

**Pas de PASS matériel P20, ni de gain CPU/UI post-merge revendiqué avant le nouveau Serial.**

## Verdict matériel P20 — PASS borné (24 novembre 2026)

Capture `platformio-device-monitor-261124-103548.log` : **355 lignes, 41151 octets, SHA256 `058c1d8a6d013540619be156d762dced39e6a791c06aa31fe831318fd254cb27`**. Candidat testé par contexte de branche `47d205655f866d0022f752139f16474d38b1269f` (le Serial ne porte pas son SHA firmware). Classic CYD, 4 MB flash, 0 PSRAM, P17 snapshot heap 6144 B, framebuffer unique 55296 B.

**3 START, 3 MENU, 41 frames de prévisualisation (3 froides et 38 chaudes)** ; 4 menus affichés. Les 38 vues chaudes montrent un seul `[RES60P20] WARM` après `[RES60P3] MAP`, une présentation LCD puis `[RES60P7] FRAME`. Les vues froides conservent toute la télémétrie P8/P17/P9/P10/P12/P13/P11/P7. Aucun FAIL/WDT/panic observé, PAK fermé entre frames, pas de gameplay/audio/sprites.

Sur les 38 frames chaudes, moyenne par angle :
- N64 (8) : `raysUs=59.986ms`, `preparedUs=97.374ms`, `totalUs=145.338ms`, contre P19 ~259ms.
- E0 (11) : `raysUs=20.900ms`, `preparedUs=57.725ms`, `totalUs=105.694ms`, contre P19 ~219ms.
- S192 (10) : `raysUs=1.669ms`, `preparedUs=38.772ms`, `totalUs=86.732ms`, contre P19 ~190ms.
- W128 (9) : `raysUs=7.404ms`, `preparedUs=44.421ms`, `totalUs=92.379ms`, contre P19 ~196ms.

**Gains de la frame totale ~44–54 %, sans amélioration significative du raycast**, cohérents avec l'hypothèse de blocage Serial. `PRESENT` stable ~36.9ms ; menu raster 213.163–213.878ms, moyenne 213.458ms (quatre menus), sans régression.

**RAM stable sur toutes les previews : `heap8=113924`, `minHeap8=100992`, `largest8=86004`**, soit −528B de heap/min par rapport au P19 antérieur ; cette différence peut venir des nouveaux modules du `main` intégré, pas de l'optimisation pure Serial. Les empreintes géométriques brutes restent N/E/S/W `ed9b68c4/48ae8442/2697640d/f43a2791`, et texture P10 N `e1f75a15`, swatch `e627fa0d`. Les **FNV framebuffer finaux ont changé** : N `51b652a4`, E `1c3a9b7f`, S `63b41bb2`, W `3b3c44f0` ; menu `0532439e`. Ils restent invariants sur chaque répétition ; cette différence avec P19 est reliée au changement upstream intentionnel `ESP32/src/esp_main_menu_skin.c` : couleur globale Classic `CLASSIC_PALE_AMBER` `0xedceU` → `0xf4caU` (composant de peinture des cartes et des étiquettes), présent dans le `main` synchronisé, **sans changement du moteur RES60 de projection, des FNV bruts, du matériau, du menu ni de la navigation**. Le changement de hash final n'est pas traité comme une détérioration de la projection ; pas de comparaison bit-à-bit des pixels physiques dans cette capture Serial.

**P20 PASS fonctionnel et de performance sur le firmware combiné testé**. Il ne démontre aucun gain de raycast, ni la faisabilité du gameplay 192×144. Prochain choix : P21 strictement borné au raycast, avec un oracle C bit-exact et ancien chemin 64-bit compilable, pas d'optimisation `PlatformVideo_present()`, pas de relâchement des gardes PAK/map lease.
