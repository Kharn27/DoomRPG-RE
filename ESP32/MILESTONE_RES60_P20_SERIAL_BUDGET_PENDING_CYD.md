# RES60-P20 — budget Serial des rotations chaudes (CI/CYD PENDING)

**PR #364 DRAFT / branche agent/esp32-res60-p1**, après merge non-force de main `ca7fd44b595660a5e1f7ed259dd11a97d84d3d69` au commit `4e16ad6508feca4a836fb59ddc9fb4b29029f1e0`. Le hardware P19 valide le code antérieur `75fc0854fe1edf57ff68f98d9970f5375fd4edb0` mais **ne valide PAS la combinaison main ni P20**.

## Cause motivante mesurée sur le vrai CYD
Log P19 du 24/11 `platformio-device-monitor-261124-091702.log`, SHA256 `d1042ec297ec5dde35ff0e5d76b20b19ad2543e231443e881c45fec7a9d0fb72` : 37 vues, 32 rotations, 5 START/MENU, N64 cache HIT : `preparedUs≈97411` mais `FRAME totalUs≈259000`, E0 HIT 57.8ms prepared contre 219ms FRAME, S192 38.8ms contre 190ms, W128 44.4ms contre 196ms. Les journaux P8/P9/P10/P11/P12/P13/P17/P7 totalisent plusieurs longs blocs Serial **entre** preparedUs et l'affichage. Ces journaux peuvent bloquer à 115200 bauds, mais ce n'est pas encore isolé par A/B matériel.

## Changement unique proposé
`DOOMRPG_RES60_P20_COMPACT_SERIAL=1` uniquement dans le profil expérimental RES60, avec P18/P13/P11/P1 obligatoires. Si **mapLeaseHit==1 ET texture.cacheHit==1**, émettre une unique ligne `[RES60P20] WARM` comprenant angle, turns, mapUs, raysUs, preparedUs, tests, quatre FNV (brut/final/matériau/swatch), et heap8/largest8/minHeap8. Garder le `[RES60P3] MAP` source et `[RES60P7] FRAME` avec `totalUs`, plus le PRESENT LCD. Au moindre MISS ou état mixte, tous les anciens journaux détaillés restent intacts. Le chemin `P20=0` conserve entièrement le verbose P19 pour A/B.

**Aucun changement** au map runtime, raycast, BSP, UV, PAK, pixels, input, allocations, framebuffer 55296B ou `PlatformVideo_present()`; aucune optimisation prématurée du LCD. Production 160x120 exclue. Tests C11 exercent la vraie décision warm/cold et les gardes de compilation, puis CI complète avec builds normaux et RES60.

## Critères hardware P20
`cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. START puis N/E/S/W 2 fois, MENU, START à nouveau. Observer `[RES60P20] WARM` seulement sur frames HIT ; sur MISS, anciennes lignes P8/P17/P9/P10/P11/P12/P13/P7. Comparer P19 **raysUs / preparedUs / totalUs** et 4 FNV, P10, RAM, PAK closed, compte START/MENU. Si réduction du `totalUs` sans régression de `preparedUs` et FNV : gain UX de trace confirmé. Si Serial n'explique pas les 100ms, ne pas prétendre l'inverse ; investiguer ensuite.

**Pas de PASS matériel P20, ni de gain CPU/UI post-merge revendiqué avant le nouveau Serial.**
