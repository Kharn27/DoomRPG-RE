# RES60-P13 — PASS matériel CYD borné (22 novembre 2026)

**Branche/PR:** [agent/esp32-res60-p1, PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). **Code P13 testé par contexte:** `67608a586502802656c58f5b26c867253c0b88c2` (après merge conservant historique de `main=1588b6983cc5724f7028ac0d2c18f96688aba990`). Les deux CI du code sont [push SUCCESS #45000978752](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45000978752) et [PR SUCCESS #45000983583](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45000983583). Aucun SHA binaire cryptographique n'est imprimé par Serial ; attribution par les tags P13 et le contexte de flash demandé. Le firmware testé reste gelé ; changements post-test **docs-only**.

**Preuve réelle :** `platformio-device-monitor-261122-103251.log`, **579 lignes / 83 315 octets / SHA256 `1ed19537687f2e748fa865829062159ae30540bad43fe80d155838a704c3cda5`**. Vrai classic CYD ESP32-D0WD-V3, 240MHz, flash 4MiB, **PSRAM=0**; profil `RES60` expérimental 192×144, panneau déclaré `alternate-panel inversion=off` sur ce log (le flag d'environnement et l'observation physique ne sont pas ici une empreinte binaire). **46 rendus**, **44 rotations validées** (23 RIGHT/21 LEFT), **2 START** et **2 MENU**; précisément **2 P12 inventory MISS et 44 HIT**, **2 P13 MAP lease MISS et 44 HIT**, **2 P8 texture MISS et 44 HIT**, seulement **2 MAPRT READY**, et **2 scans [BSPREAD]**. Sans FAIL/panic/WDT.

Chaque frame se termine avec `mapRuntime=RETAINED pak=closed audio=NOT_STARTED`. Empreintes de la carte inchangées : source `/intro.bsp` 21 823B / CRC32 `623f34e4`, arène **14 095B / FNV `c3882516`**, FNV de projection P3 pour N64 `ed9b68c4`, E0 `48ae8442`, S192 `2697640d`, W128 `f43a2791`. P8 ID65 `sampleFNV=e627fa0d`, P10 N64 26 colonnes / 650 pixels `sampleFNV=e1f75a15`. Le MENU est resté stable dans **ce** log : `titleFNV=8c671ed4, fnv=2ffd5e29`, sélection START `691e8fa2`. Ce sont les valeurs de la capture ; ne pas substituer une empreinte menu d'une autre révision.

**Temps de préparation à chaud (profilage P11 comparable, avant Serial/LCD), moyennes sur les 44 HITs P13 :**

| Angle | Vues P13 HIT | P12 réel (ms) | P13 réel (ms) | Gain absolu | Gain relatif |
| --- | ---: | ---: | ---: | ---: | ---: |
| N64 | 11 | 214,84 | 175,852 | 38,99ms | 18,1% |
| E0 | 12 | 123,99 | 84,995 | 39,00ms | 31,5% |
| S192 | 11 | 79,61 | 40,627 | 38,98ms | 49,0% |
| W128 | 10 | 92,76 | 53,779 | 38,98ms | 42,0% |

P13 `mapUs/loadUs` sur HIT : **19,524ms en moyenne**, au lieu des **~51,2ms** de P12. Le gain `preparedUs` (~39ms) inclut d'autres différences de mesures/chemins et ne doit pas être attribué exclusivement aux 31,7ms de mapUs économisés. Au froid, `mapUs≈50,8ms` et `catalogUs≈1,319s` restent. LCD `PRESENT≈36,9ms` inchangé.

**RAM** : P13 sur les 46 frames `heap8=119700B`, `largest8=86004B`, `minHeap8=106768B` fixes. P12 sur son vrai CYD précédent `heap8=133812B` ; consommation supplémentaire **14 112B** en régime BSP (une arène nominale de 14 095B + allocateur). Le plus grand bloc ne baisse pas à ce stade, pas de dérive. **Nuance MENU** : `[RES60P6] FRAME` donne encore 119700B car la capture affiche le menu **avant** l'appel de reset de session ; le deuxième START reconstruisant deux fois l'inventaire et le MAPRT est une preuve forte du reset fonctionnel, **pas** une mesure de la heap immédiatement après MENU. La dernière capture s'arrête au second MENU, pas de troisième START.

**Verdict** : PASS matériel borné P13 pour l'unique `MAP_INTRO` en vidéo/preview : cache de session valide, réduction de préparation ~39ms/tour, stabilité et FNV. **NON** validé : gameplay/sprites/audio en 60%, charge multi-cartes, source PAK remplacée avec même index (P13 ne ré-hashe que l'arène RAM et compare la *métadonnée* CRC déclarée dans le PAK). Le vrai moteur 160×120 conserve déjà sa carte, donc ceci ne constitue **pas** une accélération production directement transférable. PR #364 reste draft, non merge-ready.

## Suite bornée

P14 candidat : réduire le coût des **relectures/décodages de lignes de BSP à chaque rayon** dans le seul preview de RES60. 480 lignes validées à chaque vue, P9 recense jusqu'à 346 candidates nord, et P11 montre `raysUs≈138ms` nord. Toute pré-décodification doit garder le bon ordre des indices, la même géométrie et chaque FNV, être bornée sans stack/heap map-wide et prouver un coût RAM explicite. Aucune revendication de vitesse sans nouvelle mesure CYD.

---
