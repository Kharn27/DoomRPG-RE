# RES60-P16 — diagnostic de la topologie heap, CI / CYD PENDING (22 novembre 2026)

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

**Statut : CANDIDAT P16 / CYD PENDING. Le gameplay, audio, HUD final et sprites 192×144 restent NOT_STARTED.**
