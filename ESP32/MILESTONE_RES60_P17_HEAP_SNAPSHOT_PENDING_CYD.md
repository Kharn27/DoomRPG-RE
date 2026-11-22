# RES60-P17 — A/B statique contre heap 6144B, CI / CYD PENDING (22 novembre 2026)

**Branche :** `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). Dernier code matériel P16 `1d46b98600056807ca482f2d0573bb3720b2f3c9`, [rapport CYD P16](MILESTONE_RES60_P16_HEAP_TOPOLOGY_PENDING_CYD.md), Serial `platformio-device-monitor-261122-152531.log` SHA256 `b036c1538289d515c9d9ea1f1f95f15938f727e9fdbe537283043cc1c4e33470`. Le vrai CYD a montré 43 vues, 41 rotations, 2 START/MENU, toutes FNV exactes et `largest8=57332` constant après le framebuffer.

## Hypothèse isolée

Une allocation framebuffer RGB565 **55296B** se place à `0x3ffe46c4` dans la région DRAM **113840B** `0x3ffe4350`, laissant un bloc de **57332B**. Une autre région de **53344B** n'offre qu'un maximum de **51188B** libres et ne peut pas recevoir le framebuffer entier. Avant P14, `largest8` avait pourtant atteint **86004B** en P13. Il est *plausible mais non prouvé* que le snapshot BSS P15 6144B modifie le placement linker/heap et provoque une discontinuité de région bien supérieure à sa propre taille. P17 teste seulement cette hypothèse, sans changer ni la résolution ni la géométrie.

## Modification strictement expérimentale

`DOOMRPG_RES60_P17_HEAP_SNAPSHOT=1` uniquement pour `esp32-cyd-res60` et dérivé `-inverted`, dépend strictement de P16/P15/P13. **Pas de nouvelle allocation dans `EspRes60Preview_render`** : l'unique propriétaire `EspRes60Preview_snapshotBegin` réserve **512×12=6144B** une seule fois, depuis le heap 8-bit normal du classic CYD (0 PSRAM), **après allocation du framebuffer et après les témoins P16 pré/post framebuffer**. Réappeler begin est idempotent ; `snapshotEnd` libère et remet le pointeur à NULL, idempotent. Une allocation ratée est fail closed, le framebuffer déjà acquis est libéré, la vidéo ne démarre pas ; si begin a été omis ou après end, le rendu refuse **avant toute écriture framebuffer**.

Aucun changement des 480 entrées BSP et 346/120/8/41 candidates, aucun second framebuffer, map-wide ZIP/PAK cache, objet legacy, sprite, audio, hud 60%, ni modification de `PlatformVideo_present`. L'ancien P15 `candidateLines[512]` statique est compilable à P17=0 et sert d'oracle, avec rendu 55 296 octets et P10 UV identiques. Le nouveau pointeur remplace 6144B BSS, mais une allocation heap consommera 6144B+overhead au runtime : il faut mesurer le comportement **réel** de `largest8`, `heap8`, `minHeap8` sur CYD, et non présenter cela comme un gain garanti. Le log P17 affiche une fois adresse, heap et plus grand bloc avant/après l'allocation ; les lignes P17 par frame affichent `snapshotBss=0 snapshotHeap=6144`.

## Oracle et barrières

`ESP32/test/res60_p17_heap_snapshot_host.py` compile les vrais modules C en P15 original et P17 dynamique, reprend **261 frames/55 296 octets**, parité raw et overlay P10 (UV, clipping, canaries, original winning line), inclut les 512 candidats égaux, les erreurs géométrie/source, une **allocation refusée** et un **render pré-init** qui ne touche pas le framebuffer, begin/end idempotents, et analyse `nm -S` BSS (**6144B→4/8B**). Production normale 160×120 vérifiée sans symbole P17, profils diagnostic et P4 alignés. Tests plus compilations PlatformIO et double CI requis avant test matériel.

## Ordre de validation demandé

1. Après double CI SUCCESS exacte, flasher `pio run -e esp32-cyd-res60-inverted -t upload -t monitor`.
2. Vérifier au boot `[RES60P16] FRAMEBUFFER`, `[RES60P17] SNAPSHOT` avec `heap8Before/After`, `largest8Before/After`, et map 480 lignes.
3. Faire START, tourner N/E/S/W (plusieurs fois), MENU, recommencer START. Relever `[RES60P17] COMPACT`, `[RES60P11] PROFILE`, les FNV P3/P7, P8/P10 nord, `largest8` et `minHeap8`, absence de crash. Comparer au P16 **57332B largest / 114580B heap8 / 101648B minHeap8** et P13 largest **86004B**.
4. Si `largest8` ne s'améliore pas, ou si `minHeap8` descend sous le budget P16 ou CPU régresse franchement, envisager rollback P17 et une piste ELF/map plus prudente. La vérification mémoire et CPU reste hardware ; ne pas appeler PASS P17 sans log CYD.

**Statut : code candidat / matériel P17 PENDING. PR #364 DRAFT. Production 160×120 inchangée.**
