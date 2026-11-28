# RES60-P16 — PASS CYD borné : topologie heap confirmée (22 novembre 2026)

**Branche** `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). **Code réellement testé par contexte** : `1d46b98600056807ca482f2d0573bb3720b2f3c9`; double CI P16 SUCCESS : [push #45021102354](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45021102354) et [PR #45021107341](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45021107341). Le log ne contient pas de SHA du firmware compilé, cette identification reste contextuelle.

**Vrai ESP32-D0WD-V3 240 MHz, 4 Mo flash, 0 PSRAM** ; Serial `platformio-device-monitor-261122-152531.log`, **633 lignes / 90050 octets**, SHA256 `b036c1538289d515c9d9ea1f1f95f15938f727e9fdbe537283043cc1c4e33470`. 43 rendus BSP, 41 rotations, 2 START, 2 retours MENU, 2 cold MAP lease MISS et 41 HIT. PAK fermé, shapeData/mediaTexels NULL par invariants architecture (non directement imprimés dans ce log), aucun FAIL/panic/WDT. Les empreintes P3 N/E/S/W `ed9b68c4/48ae8442/2697640d/f43a2791` et FNV P7 `3d6bbdf0/075642f1/55d83b1d/2cf4b66c` identiques P15 ; P8 `e627fa0d`, P10 nord 26 colonnes/650 pixels `e1f75a15` toujours exacts.

## Observation physique : framebuffer

```text
[RES60P16] FRAMEBUFFER wasAllocated=0 bytes=55296 addr=0x3ffe46c4
  freeBefore=211840 freeAfter=156528
  largestBefore=110580 largestAfter=57332
  minBefore=206500 minAfter=151188
  freeBlocksBefore=10 freeBlocksAfter=10
```

La seule allocation du framebuffer réduit le plus gros bloc par **53248 B** et la mémoire libre par **55312 B** (55296 + 16 octets de surcoût d'allocation) ; le nombre global de blocs libres ne change pas. Après début du rendu, `heap8=114580`, `minHeap8=101648` et **`largest8=57332`** restent constants sur les 43 vues. La chute de `largest8` est donc présente **avant** le BSP et ne provient pas d'une fuite récurrente pendant les rotations.

Résumé officiel `heap_caps_print_heap_info(MALLOC_CAP_8BIT)` après framebuffer, sélection des régions pertinentes :
- `0x3ffe4350` : région 113840B ; **55296B alloués**, 57644B libres, bloc contigu maximum **57332B**. Le framebuffer `0x3ffe46c4` se trouve effectivement dans cette région.
- `0x3ffd2fa0` : région 53344B ; bloc libre initial **51188B**, **trop petit** pour héberger les 55296B du framebuffer (manque 4108B de bloc contigu). Après chargement carte + rendu, cette région contient **41024B alloués**, reste 11472B libres et bloc max 11252B.
- `0x3ffe0440` : région 15072B ; plus grand bloc libre 14324B, stable.
- Autres petites régions 8-bit : aucune ne peut recevoir un framebuffer entier 55296B.

```text
[RES60P16] REGIONS phase=framebuffer-ready beforeFree=156528 beforeLargest=57332 ...
[RES60P16] OBSERVER phase=framebuffer-ready freeAfter=156428 largestAfter=57332 freeDelta=-100 largestDelta=0 neutral=0
[RES60P16] REGIONS phase=first-map-hit beforeFree=114580 beforeLargest=57332 ...
[RES60P16] OBSERVER phase=first-map-hit freeAfter=114580 largestAfter=57332 freeDelta=0 largestDelta=0 neutral=1
```

**Important :** le premier dump a fait baisser de 100B le total libre, vraisemblablement à cause des sorties Serial/lazy logging ; il ne faut donc **pas** prétendre que l'observateur P16 était neutre dès le boot. Le dump post premier HIT est neutre. La mémoire stable et les preuves de rendu sont valides, mais les petites différences de durée `preparedUs` entre P15/P16 ne doivent pas être interprétées comme optimisations. P16 `loadUs` HIT ~22,5ms (contre P15 ~19,5ms), `preparedUs` nord ~100,49ms (P15 ~97,44ms), le raycast lui-même nord ~59,96ms (P15 ~59,95ms). Variations d'instrumentation/layout possibles.

## Verdict / suite

**P16 PASS CYD pour le diagnostic de topologie et la non-régression d'image** ; **pas de restauration du bloc largest8**. La source du plafond actuel est un ensemble de contraintes de placement : le framebuffer entier doit tenir dans une région contiguë, et la région secondaire ~53KiB est trop courte. L'écart historique P13 `largest8=86004` reste à expliquer : P16 ne prouve pas encore pourquoi le layout initial pré-P14 différait ; ne pas attribuer la totalité des 28672B à une seule allocation sans A/B matériel.

**Piste P17 expérimentale, indépendante :** déplacer uniquement le snapshot P15 compact 6144B, actuellement en BSS, vers un propriétaire heap unique et borné alloué une seule fois **après** le framebuffer, conserver un chemin P15 statique en oracle, comparer le déplacement réel du `largest8`, et permettre un rollback. Aucun second framebuffer, aucune nouvelle carte map-wide, aucun ajout dans le hot raycast, et pas de changement production 160×120. **Pas de merge PR #364**.

---

# RES60-P16 — protocole candidat initial (conservé pour traçabilité)

**Branche conservée :** `agent/esp32-res60-p1` ; [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). Base matériel P15 `61a2041deea1756803cadd8ef70fc9b1bf46a2f4`, [rapport P15](MILESTONE_RES60_P15_COMPACT_SNAPSHOT_PENDING_CYD.md) et log `platformio-device-monitor-261122-130234.log` SHA256 `966f9d913671118025cad7127cb11296a9636e678b5dc8d69087897e75472c3d` ; 32 vues, 30 rotations, 2 START/MENU, FNV exacts. La PR reste expérimentale ; pas de merge dans main.

## Pourquoi P16

P15 a économisé **3072 B BSS** : `heap8=111508→114580`, `minHeap8=98576→101648`, sans régression CPU (`raysUs` nord ~59,946 ms contre P14 61,625 ms). Pourtant `largest8=57332` est strictement inchangé, alors qu'il valait `86004` en P13. Ce décalage dépasse la taille du snapshot et se voit dès le boot RES60 après allocation framebuffer ; le seul total libre ne permet pas de diagnostiquer une vraie fragmentation ni une répartition entre régions DRAM. **Ne pas déplacer à l'aveugle les allocations sans connaître la topologie.**

## Nouvelle frontière read-only

Le drapeau `DOOMRPG_RES60_P16_HEAP_TOPOLOGY=1` est **uniquement** actif dans `esp32-cyd-res60` et son dérivé `esp32-cyd-res60-inverted`. Il exige P15/P13/P11. Le code `platform_video.cpp` capture un `multi_heap_info_t` sur la pile **avant** le `calloc` existant de l'unique framebuffer 55296B ; aucune sortie Serial n'est émise avant ce `calloc`. Immédiatement après, le probe publie la différence pre/post framebuffer ainsi que le pointeur et le nombre de blocs libres, puis `heap_caps_print_heap_info(MALLOC_CAP_8BIT)` détaille les régions disponibles.

Un deuxième relevé est produit **une seule fois** après le premier rendu `mapLeaseHit`, **après** la capture `preparedUs`, le log P11, le framebuffer final et `PlatformVideo_present()`. Aucune sortie P16 dans les boucles géométrie, textures, map ou présentation. Un octet statique de garde contre répétition est ajouté (plus alignement compilateur éventuel), aucun tableau, second framebuffer, allocation, PSRAM, ZIP ou mutation world/entity. Chaque dump trace `beforeFree/largest/freeBlocks/allocatedBlocks` puis `freeAfter/largestAfter/freeDelta/largestDelta/neutral` : si le diagnostic lui-même change la mémoire, ce fait doit être explicite et la comparaison P15 directe invalidée.

L'API `heap_caps_get_info` donne des agrégats par capabilities, tandis que le dump par région expose leurs plages et blocs. Le Serial P16 est volontairement plus bruyant : ne pas utiliser `totalUs` du premier HIT comme témoin chronométrique ; seules les captures P11 prises avant le dump restent comparables. La CI `ESP32/test/res60_p16_heap_topology_host.py` impose les flags, refuse l'activation production et compile les **mêmes C natifs** raycast/P10 en P15 avec P16=0 et P16=1, jusqu'aux objets binaires strictement identiques.

## Test attendu

Attendre les deux CI (host, PlatformIO normal, les deux RES60). Flasher ensuite `pio run -e esp32-cyd-res60-inverted -t upload -t monitor` depuis `ESP32` et sauvegarder le log intégral. Démarrage à froid, START, rotations N/E/S/W, retour MENU, nouvel START. Lire `[RES60P16] FRAMEBUFFER`, `REGIONS phase=framebuffer-ready`, `OBSERV ER` (orthographe exacte `OBSERVER`), `REGIONS phase=first-map-hit`, tableau de région `heap_caps_print_heap_info`, les FNV P3/P7/P8/P10 et les relevés RAM/CPU P11/P15. **Aucun PASS P16 revendiqué sans nouveau Serial CYD.**

## Suites possibles, pas encore autorisées

Si la chute du plus grand bloc vient d'une allocation framebuffer dans une région spécifique, envisager un futur P17 avec politique d'allocation explicite, comparaison contrôlée et rollback sur échec ; si elle vient du layout BSS, regarder les sections ELF/linker. Ne pas ajouter de grand cache ni optimiser `PlatformVideo_present()` prématurément.

**Statut historique pré-test : CANDIDAT P16 / CYD PENDING. Le gameplay, audio, HUD final et sprites 192×144 restent NOT_STARTED.**
