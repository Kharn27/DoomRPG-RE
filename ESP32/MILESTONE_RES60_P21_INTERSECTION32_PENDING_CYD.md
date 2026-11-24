# RES60-P21 — intersections cardinales 32 bits strictement bornées (CYD PENDING)

**PR #364 DRAFT, branche `agent/esp32-res60-p1`.** Le P20 a passé le CYD avec 38 frames chaudes et `raysUs` N64 ≈59.986ms, dont ~66 432 tests de segments au nord. Sa réduction de traces Serial a diminué le coût de frame totale de 44–54%, mais **ne change pas le coût du raycast**. P21 teste une famille mathématique isolée sans modifier la projection, les hits, les pixels, la texture ni le LCD.

## Transformation réversible

`DOOMRPG_RES60_P21_INTERSECTION32=1` uniquement dans les deux profils `esp32-cyd-res60*` (le profil production 160×120 n'a aucun symbole ni changement). Le `rayHit` du preview original reste compilable intégralement avec `P21=0`, comme oracle arithmétique 64 bits. Avec P21, les **trois déterminants** `den`, `num` et `u` sont calculés en **int32_t**, puis évalués avec `num*192` en `uint32_t` pour les deux clips. Les sorties `depthNum/depthDen` restent en int64, de même que **`num*bestDen < bestNum*den`**, les égalités et l'ordre original des lignes : aucune approximation, aucun flottant, aucune mutation d'état. La prévalidation de toutes les lignes a lieu avant `rayHit`, comme en P18.

**Preuve des bornes pour cette seule géométrie** : `line.x/y∈[0,2048]`, `camera.x/y∈[0,2047]`, les quatre rays cardinaux ont `|rayX|+|rayY|≤383`, `PREVIEW_NEAR=4`, `PREVIEW_FAR=4096`. Donc `|num| ≤ 2·2048² = 8388608` (signé), `|den|,|u| ≤ 383·2048 = 784384`, `num*192 ≤ 1610612736 < INT32_MAX`, et `4096*den ≤ 3212836864 < UINT32_MAX`. **Le clip far 4096 doit rester `uint32_t`**, un signed32 déborderait ; la comparaison du meilleur hit reste 64 bits. Toute extension à d'autres angles/carte/taille nécessite un nouveau preuve/garde.

**Oracle** : le test P18 original a un mode `--p21` et compile l'ancien P15 C11 64-bit statique **contre** le P18 heap-local + P21 optimisé, pour 261 frames, 55296B chacune, pixels identiques, UV source, clips, égalités, canaries, diagnostics de corruption et rollback propriétaire. Test de profil P21 vérifie le prérequis P20/P18/P15, les bornes, le rejet P21=1 en production et le rejet 2. Double CI et deux builds RES60 nécessaires avant flash.

## Protocole CYD

Firmware normal de l'expérience `cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. START, N/E/S/W réitérés, MENU puis START. Observer `[RES60P20] WARM` + `[RES60P7] FRAME`, FNV geom exact N/E/S/W `ed9b68c4/48ae8442/2697640d/f43a2791`, FNV final **post-thème** `51b652a4/1c3a9b7f/63b41bb2/3b3c44f0`; menu `0532439e`. Confronter `raysUs` P20 N64 59.986ms, E0 20.900ms, S192 1.669ms, W128 7.404ms ; `totalUs` P20 N 145.338ms, E 105.694ms, S 86.732ms, W 92.379ms. `heap8=113924`, `minHeap8=100992`, `largest8=86004` baseline P20 avant le dernier merge de main. Vérifier original PAK fermé, starts reentrant, aucun problème de canary.

Si CPU ne gagne rien ou les empreintes diffèrent, revenir immédiatement à P21=0 et **arrêter cette piste**. On n'optimise pas `PlatformVideo_present()` et on ne démarre toujours aucun gameplay 192×144 à ce milestone.

**P21 ne possède aucune preuve CYD avant ce test, ne pas déclarer merge-ready.**
