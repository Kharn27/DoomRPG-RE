# RES60-P19 — START de nouveau relançable / menu PAK optimisé (CYD PENDING)

[PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), branche `agent/esp32-res60-p1` DRAFT. Diagnostic sur `platformio-device-monitor-261122-191955.log` (SHA256 `90ce11ef8ea56ea49276c970457eb7e407b60c658cd4cfe80cb6dae2dd6d2d5e`) : raster menu 1.197s / frame entière 1.245s, second START non exécuté après MENU; N64 P18 raycast 59.970ms, heap8=114452, largest8=86004.

P19 remplace uniquement pour le `c.bmp` PAK original 192x128 les 144 lectures ligne par ligne par deux appels au lecteur BMP batché natif (`blit` sur 128 lignes, puis les 16 premières répétées). Scratch 1KiB partagé, aucune nouvelle allocation, cache ou image retenue, même framebuffer 192x144. Dimensions différentes gardent `tile`. Logo PAK `j.bmp` inchangé. START devient une action au **premier tap dans sa hitbox**, y compris au retour de MENU; les autres cartes restent à deux taps et bloquées. Un reset de lease carte dupliqué est supprimé.

Tests hôte P5 (fallback), P6 (2 blits fond + logo, rendu RGB565/FNV et canaries), P7, P18 raycast/UV, double CI; production 160x120 inchangée. Refaire sur CYD `esp32-cyd-res60-inverted`: premier START, N/E/S/W, MENU, **nouveau START**, répétition, même empreintes; comparer `[RES60P6] MENU rasterUs` au 1.197s de P18 et vérifier heap8/largest8/PAK. **Aucun PASS ni gain P19 revendiqué sans Serial réel.**

## Résultat sur le vrai CYD — P19 PASS borné (24 novembre 2026)

**Code candidat testé par contexte** : `75fc0854fe1edf57ff68f98d9970f5375fd4edb0`; aucun SHA de firmware n'est imprimé au Serial. Capture `platformio-device-monitor-261124-091702.log`, **596 lignes / 83986 octets / SHA256 `d1042ec297ec5dde35ff0e5d76b20b19ad2543e231443e881c45fec7a9d0fb72`**.

Sur ESP32-D0WD-V3 4 MB flash, 0 PSRAM, profil RES60 inversé : **5 START / 5 MENU, 32 rotations, 37 frames BSP**. Chaque nouvel START après MENU déclenche `ACTION START`, puis un nouveau P12/P13 MISS attendu ; 6 rendus MENU (initial + 5 retours) et aucun `SELECT` parasite. `touch=START_ONE_TAP/OTHER_TWO_TAP` confirmé au boot. **Menu P19 raster 213302–213720 µs (moyenne 213409 µs)** contre ~1 197 000 µs P18 (−82,2 %) ; **temps complet MENU+LCD moyenne 261223 µs**, LCD ~36,8–36,9 ms. **24 lectures PAK** par menu, et toujours `titleFNV=8c671ed4`, `fnv=d176dbd9` sur les six menus. Le PAK reste fermé entre les présentations.

Les 37 vues couvrent les FNV bruts **N64=ed9b68c4 / E0=48ae8442 / S192=2697640d / W128=f43a2791** et les finales **923768cc / 433dbf6b / 03a4fe75 / 90dfc503**, stables. P10 sample nord `e1f75a15` (650 pixels), P8 swatch `e627fa0d`, catalog `29ffc14a`. Raycast cache HIT : N64 **59.987ms (7)**, E0 **20.891ms (11)**, S192 **1.672ms (7)**, W128 **7.429ms (7)**. Les PREPARED/HIT moyens sont respectivement ~97.411 / 57.768 / 38.787 / 44.371ms, mais le temps complet des rotations reste 190–259ms, dont la télémétrie Serial de laboratoire.

**RAM stable pendant les 37 vues** : `heap8=114452`, `minHeap8=101520`, `largest8=86004`; snapshot P17 = 6144 B, map P13 = 14095 B, aucun second framebuffer, `shapeData/mediaTexels` restent non alloués dans ce chemin isolé. Aucun `FAIL`, panic, WDT observé dans la capture. Limites : pas de gameplay, sprites ni audio RES60. Aucun benchmark sur la nouvelle base `main` combinée ; ne pas confondre gain du menu avec celui du raycast ou d'une future optimisation Serial.

**Verdict** : P19 PASS CYD **sur le candidat précédent**, conserver ce SHA. Les commits ultérieurs peuvent synchroniser `main` et ajouter P20 mais ne doivent pas s'approprier le PASS matériel P19 sans nouveau test.
