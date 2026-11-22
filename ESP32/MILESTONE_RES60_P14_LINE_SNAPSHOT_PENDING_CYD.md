# RES60-P14 — PASS CYD borné : snapshot 8 KiB des lignes candidates (22 novembre 2026)

**Branche :** `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). **Firmware testé par contexte :** `547441d5805a7d8a89db0f9fad6bcb64426fa360` (pas de fingerprint SHA firmware autonome dans Serial). [CI push SUCCESS #45008683469](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45008683469) ; [CI PR SUCCESS #45008687278](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45008687278). Le code P14 est gelé après ce test ; uniquement documentation ensuite, avant nouvelle étape de code distincte.

## Source matérielle exacte

`platformio-device-monitor-261122-105309.log` : **822 lignes / 116 796 octets**, SHA256 **`8bd07655a730fdcf12546c65635345d13e2bbdaf1686965c475043e6a4383cd0`**. Vrai ESP32-D0WD-V3 240 MHz, 4 MiB flash, **PSRAM = 0** ; profil expérimental 192×144 RGB565, LCD 320×240 en nearest 5/3. `alternate-panel inversion=off` selon ligne de boot, sans en faire une preuve visuelle autonome. L'arène BSP vient exclusivement de `/DoomRPG-ESP32.pak`, pas du ZIP. `/intro.bsp` source CRC `623f34e4`, size 21 823 B, MAPRT 14 095 B FNV `c3882516`.

**Exécution : 59 rendus [RES60P7] RASTER, 55 TURN committed, 4 START et 4 ACTION MENU**, 4 `[BSPREAD] ENTRY`, 4 `[MAPRT] READY`, 4 P12 inventory MISS + 55 HIT, 4 P13 map lease MISS + 55 HIT, 59 P14 SNAPSHOT. 10 actions MENU OPTIONS/HELP/LOAD bloquées comme attendu (`60pct-gameplay-route-not-ported`), pas un crash. Aucune ligne panic, watchdog, abort ou FAIL ; PAK fermé (`pak=closed`) sur les 59 frames ; audio/gameplay restent `NOT_STARTED`.

Tous les `[RES60P14] SNAPSHOT` : **480 `readerCalls`** et `bytesBss=8192`. `wedgeCandidates` selon angle N64=346 / E0=120 / S192=8 / W128=41, donc `avoidedDecodes` 66 432 / 23 040 / 1 536 / 7 872. **Le nombre de tests géométriques `rayTests` n'a PAS changé** : le nombre de redécodages de source a diminué, pas le nombre de rayons géométriques.

## Préparation CPU P13 → P14, vrais CYD et profil P11 comparables, HITs uniquement

| Angle | Vues P13/P14 HIT | P13 raysUs | P14 raysUs | Gain raysUs | P13 preparedUs | P14 preparedUs | Gain preparedUs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Nord 64 | 11 / 13 | 138,397 ms | 61,625 ms | **−55,5 %** | 175,852 ms | 99,243 ms | **−43,6 %** |
| Est 0 | 12 / 15 | 48,100 ms | 21,490 ms | **−55,3 %** | 84,995 ms | 58,479 ms | **−31,2 %** |
| Sud 192 | 11 / 14 | 3,494 ms | 1,718 ms | **−50,8 %** | 40,627 ms | 38,981 ms | **−4,1 %** |
| Ouest 128 | 10 / 13 | 16,701 ms | 7,607 ms | **−54,5 %** | 53,779 ms | 44,819 ms | **−16,7 %** |

P13 témoin `platformio-device-monitor-261122-103251.log` (SHA256 `1ed19537687f2e748fa865829062159ae30540bad43fe80d155838a704c3cda5`). P14 `mapUs` HIT ~19,4–19,7 ms, comme P13 ; les économies viennent du seul hot path du raycast. Premier cold START P14 `catalogUs` environ 1,327 s et `preparedUs` environ 1,641 s : **pas d'optimisation P8 catalog**. `[RES60P1] PRESENT` ≈36,9 ms inchangé. Les durées totales incluent Serial/PAK/LCD et ne sont pas une fréquence FPS de gameplay.

## Parité et RAM : conditions essentielles

**Parité exacte sur le vrai matériel** pour les quatre directions (toujours la même empreinte à chaque réapparition) :

| Angle | rawP3FNV | FNV frame finale P7 |
| --- | --- | --- |
| N64 | `ed9b68c4` | `3d6bbdf0` |
| E0 | `48ae8442` | `075642f1` |
| S192 | `2697640d` | `55d83b1d` |
| W128 | `f43a2791` | `2cf4b66c` |

P8 texture 65 `sampleFNV=e627fa0d`, P10 nord 26 colonnes / 650 pixels `sampleFNV=e1f75a15`, FNV menu `titleFNV=8c671ed4`, menu `fnv=2ffd5e29`. Le test hôte C11 [P14 oracle](test/res60_p14_line_snapshot_host.py) compare mêmes 55 296 octets, canaris, FNV, plan UV et 260 vues contre la version P13 non optimisée ; la preuve réelle CYD porte uniquement sur MAP_INTRO.

**RAM P14** `heap8=111508`, `largest8=57332`, `minHeap8=98576` stables sur les 59 frames. **P13** `heap8=119700`, `largest8=86004`, `minHeap8=106768`. Écart **−8 192B** heap8 et minHeap8 cohérent avec l'ajout BSS P14 ; mais **−28 672B sur largest8**, une chute nettement supérieure aux seuls 8KiB, probablement liée à la disposition/fragmentation des zones d'allocation. **Ne pas en déduire que les 8KiB de BSS sont simplement un coût négligeable.** Le preview reste stable à 57 332B pour le plus grand bloc ; **aucune preuve de viabilité pour gameplay complet / audio / sprites / autres cartes**.

**Reset MENU** : le relevé de heap dans `[RES60P6] FRAME` est fait *avant* le reset P13, donc ne valide pas la mémoire libérée instantanément. Les 4 START froids et relectures MAPRT démontrent que la session est réinitialisée pour la prochaine entrée. La protection P13 sur HIT contrôle le FNV de l'arène RAM et la métadonnée CRC PAK déclarée ; elle ne re-hashe pas le contenu PAK entier à chaque rotation. Aucune preuve d'un fichier PAK changé pendant session.

**Verdict : PASS CYD borné P14 sur MAP_INTRO**, avec gains CPU mesurés, FNV exacts, 59 frames et mémoire stable. **Non merge-ready** : laboratoire parallèle sans gameplay 60%, PR #364 reste draft. Ne pas transposer tel quel en production 160×120 (déjà BSP natif résident).

## Suite conseillée (à arbitrer au début de la prochaine conversation)

Compte tenu de la chute `largest8 86004→57332`, le prochain milestone cohérent pourrait être **RES60-P15-MEM : audit et réduction de l'empreinte du snapshot/caches P9/P14**, avant d'augmenter encore les fonctionnalités graphiques. Relire le vrai `main`, `ESP32/PORTING_STATUS.md`, `DOCUMENTATION.md`, l'état du code et le dernier milestone ; concevoir une réduction de BSS **bornée**, avec compilation 160×120 intacte et oracle C de parité P14. Tester aussi une voie de réduction de la fragmentation, sans inventer les résultats CYD. Ne pas ajouter sprites/audio/gameplay avant preuve de marge mémoire.

