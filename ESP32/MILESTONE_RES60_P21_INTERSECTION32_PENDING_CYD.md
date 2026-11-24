# RES60-P21 — intersections cardinales 32 bits strictement bornées (PASS CYD borné)

**PR #364 DRAFT, branche `agent/esp32-res60-p1`.** Le P20 a passé le CYD avec 38 frames chaudes et `raysUs` N64 ≈59.986ms, dont ~66 432 tests de segments au nord. Sa réduction de traces Serial a diminué le coût de frame totale de 44–54%, mais **ne change pas le coût du raycast**. P21 teste une famille mathématique isolée sans modifier la projection, les hits, les pixels, la texture ni le LCD.

## Transformation réversible

`DOOMRPG_RES60_P21_INTERSECTION32=1` uniquement dans les deux profils `esp32-cyd-res60*` (le profil production 160×120 n'a aucun symbole ni changement). Le `rayHit` du preview original reste compilable intégralement avec `P21=0`, comme oracle arithmétique 64 bits. Avec P21, les **trois déterminants** `den`, `num` et `u` sont calculés en **int32_t**, puis évalués avec `num*192` en `uint32_t` pour les deux clips. Les sorties `depthNum/depthDen` restent en int64, de même que **`num*bestDen < bestNum*den`**, les égalités et l'ordre original des lignes : aucune approximation, aucun flottant, aucune mutation d'état. La prévalidation de toutes les lignes a lieu avant `rayHit`, comme en P18.

**Preuve des bornes pour cette seule géométrie** : `line.x/y∈[0,2048]`, `camera.x/y∈[0,2047]`, les quatre rays cardinaux ont `|rayX|+|rayY|≤383`, `PREVIEW_NEAR=4`, `PREVIEW_FAR=4096`. Donc `|num| ≤ 2·2048² = 8388608` (signé), `|den|,|u| ≤ 383·2048 = 784384`, `num*192 ≤ 1610612736 < INT32_MAX`, et `4096*den ≤ 3212836864 < UINT32_MAX`. **Le clip far 4096 doit rester `uint32_t`**, un signed32 déborderait ; la comparaison du meilleur hit reste 64 bits. Toute extension à d'autres angles/carte/taille nécessite un nouveau preuve/garde.

**Oracle** : le test P18 original a un mode `--p21` et compile l'ancien P15 C11 64-bit statique **contre** le P18 heap-local + P21 optimisé, pour 261 frames, 55296B chacune, pixels identiques, UV source, clips, égalités, canaries, diagnostics de corruption et rollback propriétaire. Test de profil P21 vérifie le prérequis P20/P18/P15, les bornes, le rejet P21=1 en production et le rejet 2. Double CI et deux builds RES60 nécessaires avant flash.

## Protocole CYD

Firmware normal de l'expérience `cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. START, N/E/S/W réitérés, MENU puis START. Observer `[RES60P20] WARM` + `[RES60P7] FRAME`, FNV geom exact N/E/S/W `ed9b68c4/48ae8442/2697640d/f43a2791`, FNV final **post-thème** `51b652a4/1c3a9b7f/63b41bb2/3b3c44f0`; menu `0532439e`. Confronter `raysUs` P20 N64 59.986ms, E0 20.900ms, S192 1.669ms, W128 7.404ms ; `totalUs` P20 N 145.338ms, E 105.694ms, S 86.732ms, W 92.379ms. `heap8=113924`, `minHeap8=100992`, `largest8=86004` baseline P20 avant le dernier merge de main. Vérifier original PAK fermé, starts reentrant, aucun problème de canary.

Si CPU ne gagne rien ou les empreintes diffèrent, revenir immédiatement à P21=0 et **arrêter cette piste**. On n'optimise pas `PlatformVideo_present()` et on ne démarre toujours aucun gameplay 192×144 à ce milestone.

**P21 ne possède aucune preuve CYD avant ce test, ne pas déclarer merge-ready.**

## CI code P21 — PASS (24 novembre 2026)

**Code exact double-CI : `a476eef3b5108eee1aa41c7a9f17e7024ccc7bb1`**. [push SUCCESS #45368084719](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45368084719) et [PR SUCCESS #45368100671](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45368100671) — 262 étapes SUCCESS chacune, y compris le nouveau test `RES60-P21` 261 frames C11, les profils de production et les deux firmwares expérimentaux RES60. Ces CI ne sont **pas** une mesure de gain Xtensa ni un test physique. Les commits suivants sont docs-only jusqu'au retour CYD.

## Verdict hardware P21 — PASS borné (24 novembre 2026)

Log exact `platformio-device-monitor-261124-110705.log`, **382 lignes, 44790 octets, SHA256 `b45866c62a5a75e8ea28328b94f22c51369ffdf9bff830f62a216aba73601cfe`**. Firmware associé par contexte au **commit code `a476eef3b5108eee1aa41c7a9f17e7024ccc7bb1`** (le Serial ne porte pas son SHA), base synchronisée `main=e84f2f9498fca0c2172af4c0ef03742e1d701b18`. **3 START, 3 retours MENU, 47 frames (3 froides et 44 chaudes)**, 4 rendus MENU. Aucun FAIL/panic/WDT, PAK fermé, aucune fuite visible. Ne prétend pas valider le gameplay, sprites ni audio RES60 : le boot dit toujours `gameplay=NOT_STARTED`.

Moyennes des **44 frames chaudes** (par angle, P20 précédent -> P21; quantités en ms) :

| Orientation | N frames P21 | P20 rays | P21 rays | Gain rays | P20 frame totale | P21 frame totale | Gain total |
|---|---:|---:|---:|---:|---:|---:|---:|
| N64 | 11 | 59.986 | **22.281** | **62.9%** | 145.338 | **107.095** | **26.3%** |
| E0 | 11 | 20.900 | **7.878** | **62.3%** | 105.694 | **92.250** | **12.7%** |
| S192 | 10 | 1.669 | **0.730** | **56.3%** | 86.732 | **85.300** | **1.7%** |
| W128 | 12 | 7.404 | **2.860** | **61.4%** | 92.379 | **87.375** | **5.4%** |

`preparedUs` passe à 59.203ms (N), 44.369ms (E), 37.422ms (S), 39.491ms (W). `mapUs` reste ~19.18–19.22ms et le LCD `PRESENT` ~36.85–36.89ms : cette opération n'a pas touché ces coûts. Le menu raster est stable 211.113–211.605ms (quatre passages), `fnv=0532439e`, `titleFNV=8c671ed4`.

**Concordance exacte, répétée sur les 3 sessions :** `rawP3FNV` N `ed9b68c4`, E `48ae8442`, S `2697640d`, W `f43a2791` ; framebuffer final post-thème N `51b652a4`, E `1c3a9b7f`, S `63b41bb2`, W `3b3c44f0` ; `p10FNV=e1f75a15` au nord, `swatchFNV=e627fa0d` partout. Ce constat est un contrôle FNV strict des frames échantillonnées, pas une vidéo ni un test du moteur de jeu réel.

**RAM constante** sur les 44 frames chaudes : `heap8=113924`, `largest8=86004`, `minHeap8=100992`. `ESP32-D0WD-V3`, 240MHz, 4MiB flash, zéro PSRAM, framebuffer unique 55296B, snapshot 6144B, carte compacte 14095B. L'optimisation n'ajoute pas de buffer ou d'owner.

**Verdict : P21 PASS CPU/rendu/heap/reentrance dans le preview**, sous double CI code SUCCESS. Par rapport à P19 N64 ~259ms total, la chaîne P20+P21 ramène à ~107ms (~59% de gain). Le **prochain effort raisonnable n'est pas une autre micro-optimisation** : les chemins gameplay 160x120 possèdent des contrats explicites de largeur/projection, HUD et sprites qui nécessitent une étude de portage borné avant de prétendre jouer en 192x144.
