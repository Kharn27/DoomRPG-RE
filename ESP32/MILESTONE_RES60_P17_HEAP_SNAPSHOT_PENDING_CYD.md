# RES60-P17 — PASS CYD mémoire et rendu, CPU raycast à optimiser (22 novembre 2026)

**Branche :** `agent/esp32-res60-p1`; [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). **Code effectivement testé :** `4b7718054ace14f78b34def20ff7e21c5d57be15`; double CI SUCCESS [push #45033645348](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45033645348) / [PR #45033649494](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45033649494). **Preuve matérielle :** vrai CYD ESP32-D0WD-V3 240 MHz / flash 4MB / **0 PSRAM**, Serial `platformio-device-monitor-261122-161354.log`, **576 lignes, 81222 octets, SHA256 `fe19307c4d92f86bb630969ef299a9a6cd574f133cb0db9250763c04233d2fde`**. Le log ne contient pas de SHA firmware inscrit : attribution au code candidat via contexte de flash.

## Verdict matériel borné

- **37 frames BSP / 34 rotations engagées**, 3 START et 3 retours MENU ; 3 MISS cold MAPRT/PAK puis 34 HIT, invariants de cache inchangés. Aucun `FAIL`, `panic`, `Guru Meditation` ou `WDT`. Pas de gameplay/audio/sprites/HUD final 192×144 (toujours `NOT_STARTED`).
- Toutes les empreintes P3 brutes **N/E/S/W** = `ed9b68c4`, `48ae8442`, `2697640d`, `f43a2791` ; P7 final = `3d6bbdf0`, `075642f1`, `55d83b1d`, `2cf4b66c`, exactes P15/P16 dans toutes les vues. P8 `sampleFNV=e627fa0d`, P10 nord 26 colonnes/650 pixels `sampleFNV=e1f75a15`. PAK `closed` après rendu.
- **Mémoire stable** `heap8=114564` (P16 114580, **-16B**), `minHeap8=101632` (P16 101648, **-16B**), **`largest8=86004` (P16 57332, +28672B, **+50,0%**) sur la session**. La mémoire totale non allouée est quasi identique ; le bénéfice est une réserve contiguë retrouvée, PAS 28K de heap libre supplémentaire.
- **Déplacement d'allocation confirmé :** framebuffer 55296B à **`0x3ffd1ad0`**, dans la région `0x3ffd17a0`, maintenant taille **59488B** (P16 53344B). Avant et après ce `calloc`, plus grand bloc **110580B** inchangé. L'allocation séparée du snapshot 6144B réussit à **`0x3ffb2730`**, `heap8 162548→156388` (**−6160B overhead inclus**) et `largest8` reste **110580B**. Après MAPRT, la région `0x3ffe4350` héberge 26928B alloués, garde **86004B contigus**. **L'hypothèse placement linker/BSS est confirmée pour ce build** : sortir 6144B de BSS a élargi la région adjacente à 59488B, permettant d'y placer le framebuffer et de préserver l'autre gros bloc. Les adresses sont des résultats hardware de ce firmware, pas des garanties absolues sur des builds futurs.
- Observateurs : au boot P16 `freeDelta=-100 largestDelta=0 neutral=0` (effet d'amorçage Serial non neutre), au premier cache HIT `freeDelta=0 largestDelta=0 neutral=1`.

## Perf P16→P17 : comparaison rigoureuse du même protocole

Moyennes `RES60P11 cache=HIT` sur les logs réels P16 (`platformio-device-monitor-261122-152531.log`, 10/11/10/10 vues N/E/S/W) et P17 (8/9/8/9 vues) :

| Angle | P16 raysUs (ms) | P17 raysUs (ms) | delta | P16 preparedUs (ms) | P17 preparedUs (ms) |
|---|---:|---:|---:|---:|---:|
| 64 (N) | 59.963 | 64.026 | **+6.78%** | 100.487 | 101.621 |
| 0 (E) | 20.865 | 22.284 | **+6.80%** | 60.872 | 59.263 |
| 192 (S) | 1.655 | 1.745 | **+5.45%** | 41.873 | 39.000 |
| 128 (W) | 7.375 | 7.869 | **+6.69%** | 47.561 | 45.081 |

`loadUs` warm **baisse d'environ 3ms** (P16 ~22.5ms → P17 ~19.5ms), masquant une partie du surcoût raycast dans `preparedUs`. P17 **n'est pas un PASS de non-régression CPU stricte** : les tests mémoire/rendu PASS, mais les **rayons sont ~6–7% plus lents**. Le gain mémoire est utile, mais doit être consolidé via un P18 dédié, sans revenir à 6144B BSS ni sacrifier `largest8`. Ne pas conclure que P17 est définitivement optimal sur CPU.

## Suite

**P17 PASS CYD sur allocation, mémoire, FNV, stabilité et ownership.** Amélioration `largest8` +28672B confirmée, CPU raycast à revoir en P18. Préserver l'oracle P17/les mesures P16, refuser tout changement silencieux de géométrie ou de format, et continuer sur **la branche parallèle**, PR #364 DRAFT. Aucun merge en production `main`.

---

# RES60-P17 — protocole initial A/B statique contre heap 6144B, CI / CYD PENDING (22 novembre 2026)

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
