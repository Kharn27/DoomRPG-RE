# RES60-P15 — PASS CYD borné : snapshot compact 12 B (22 novembre 2026)

**Branche** `agent/esp32-res60-p1` ; [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). **Code matériel testé par contexte utilisateur** : `61a2041deea1756803cadd8ef70fc9b1bf46a2f4` ; firmware sans SHA binaire autonome dans Serial. CI strictes **SUCCESS** : [push #45011807432](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45011807432) et [PR #45011812062](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45011812062). Suite C11 P14↔P15 et builds normaux + deux RES60 PASS. Les seuls changements post-test P15 doivent être documentaires avant la prochaine étape isolée.

## Preuve matérielle exacte

Vrai classic CYD ESP32-D0WD-V3 240 MHz, 4MiB flash, **0 PSRAM** ; profil RES60 192×144 inverted (`alternate-panel inversion=off`). Serial `platformio-device-monitor-261122-130234.log`, **452 lignes / 65 810 octets**, **SHA256 `966f9d913671118025cad7127cb11296a9636e678b5dc8d69087897e75472c3d`**. La source est le PAK `/DoomRPG-ESP32.pak`, `/intro.bsp` CRC `623f34e4`, MAPRT 14095B FNV `c3882516`. Aucune preuve d'exécution de gameplay/audio/sprites dans le profil preview.

**32 rendus BSP, 30 rotations committed, 2 START, 2 MENU**, 2 reconstructions froides de MAPRT + inventory/lease MISS, **30 cache HIT**. 32 snapshots P15 : 480 lignes lues en préflight, 346/120/8/41 candidates par direction, mêmes 66432/23040/1536/7872 tests de géométrie qu'en P14. Les 32 frames ont `pak=closed`, carte retenue en session, aucun `FAIL`, panic, watchdog, WDT dans le log. Le retour MENU est visible avant une éventuelle release : **heap post-MENU n'est pas mesurée** ; les deux cold START prouvent l'invalidation fonctionnelle de la session.

Parité exacte sur les 32 vues et quatre directions :

| Direction | rawP3FNV | FNV final P7 | Fréquence |
| --- | --- | --- | ---: |
| N64 | `ed9b68c4` | `3d6bbdf0` | 9 |
| E0 | `48ae8442` | `075642f1` | 8 |
| S192 | `2697640d` | `55d83b1d` | 7 |
| W128 | `f43a2791` | `2cf4b66c` | 8 |

Texture P8 source logique 65 : `sampleFNV=e627fa0d`, matériau P10 nord **26 colonnes / 650 pixels**, `sampleFNV=e1f75a15` ; toutes empreintes identiques P14. La phase `[RES60P1] PRESENT` reste ~36,9ms.

## Mémoire réelle P14 → P15

| Métrique sur CYD | P14 | P15 | Écart |
| --- | ---: | ---: | ---: |
| Table snapshot + index BSS | 9216 B | 6144 B | **−3072 B** |
| `heap8` rendu à chaud | 111508 B | 114580 B | **+3072 B** |
| `minHeap8` en session | 98576 B | 101648 B | **+3072 B** |
| `largest8` | 57332 B | 57332 B | **0 B** |

Le snapshot compact a récupéré exactement 3072 B libres, **sans restaurer le plus gros bloc**. P13 avait `largest8=86004` ; P14 et P15 restent `57332`. Le delta `28672 B` semble lié au layout des zones de heap ou à leur fragmentation/positionnement et **n'est pas expliqué par le coût seul du snapshot**. Le relevé initial après premier rendu P15 `minHeap8=101700` puis la valeur stable de session `101648` est conservé sans dissimuler les 52B transitoires.

## Temps CPU à chaud comparables (instrumentation P11 activée, HIT uniquement)

| Direction | P14 raysUs | P15 raysUs moyenne | P14 preparedUs | P15 preparedUs moyenne |
| --- | ---: | ---: | ---: | ---: |
| N64 | 61,625 ms | **59,946 ms** (7 vues) | 99,243 ms | **97,435 ms** |
| E0 | 21,490 ms | **20,875 ms** (8 vues) | 58,479 ms | **57,773 ms** |
| S192 | 1,718 ms | **1,655 ms** (7 vues) | 38,981 ms | **38,807 ms** |
| W128 | 7,607 ms | **7,402 ms** (8 vues) | 44,819 ms | **44,457 ms** |

Aucune régression CPU matérielle détectée, petits gains de 0,5 à 3,7 % selon métrique/direction ; plusieurs effets de layout/cache/compilateur restent possibles et les petites améliorations ne doivent pas être revendiquées comme un algorithme nouveau.

## Verdict et prochaine frontière

**PASS CYD borné P15**, intégrité et économie statique confirmées ; **objectif `largest8` NON RÉSOLU**. Conserver la branche et PR #364 DRAFT, pas de merge dans main, pas d'extension gameplay/sprites/audio/HUD 60 %. Prochaine étape raisonnable : un P16 isolé d'audit des régions de heap/ELF statique et de leur topologie, instrumentation read-only sans allocation ni perturbation de `raysUs`/`preparedUs`, avant toute tentative plus risquée de déplacement du snapshot.

---

# RES60-P15 — conception et preuve host de la représentation 12 B

**Branche** `agent/esp32-res60-p1` ; [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). Référence matérielle gelée : code P14 `547441d5805a7d8a89db0f9fad6bcb64426fa360`, [rapport P14](MILESTONE_RES60_P14_LINE_SNAPSHOT_PENDING_CYD.md). Le verdict CYD est désormais consigné ci-dessus ; la proposition originale suit pour traçabilité.

## Audit mémoire et choix

Le vrai CYD P13 annonçait `heap8=119700`, `largest8=86004`, `minHeap8=106768`. P14 mesurait `111508`, `57332`, `98576`. Delta heap **-8192 B**, mais plus grand bloc **-28672 B**. Une augmentation BSS peut décaler les bornes des régions allouables et l'ordre d'allocation ; ce delta de largest8 ne prouve pas que 28672 octets sont directement récupérables.

P14 conserve simultanément `candidateLines[512]` de 16B par ligne et `wedgeIndices[512]` de 2B par index. Dans le vrai raycast P14, les quatre coordonnées et l'ID texture 16-bit sont utilisés pour intersections, UV et matériau. `flags` n'est consulté que pour les deux exclusions `SPRITE_SPAN` et `OCCLUDER_ONLY` ; les autres flags ne modifient pas ce preview. L'index source original sert au témoin de ligne gagnante, y compris les égalités.

P15 ajoute la macro strictement expérimentale `DOOMRPG_RES60_P15_COMPACT_SNAPSHOT` : entrée statique **12 B** composée de quatre `uint16_t` de coordonnées, `uint16_t` texture et `uint16_t` stockant l'index original sur 9 bits plus deux bits séparés pour les exclusions. La borne P9 impose au plus 512 lignes et le préflight vérifie toutes les entrées avant écriture. L'ordre de boucle et le comparateur strict (`num * bestDen < bestNum * den`) ne bougent pas. Pas de quantification, pas de changement de clipping/UV, pas de décompression ou d'allocation supplémentaire. Le code P14 reste compilable tel quel avec P15=0 pour oracle réel.

| Taille statique | P14 | P15 |
| --- | ---: | ---: |
| Tableau de segments | 8192 B | 6144 B |
| Indices P9 | 1024 B | 0 B |
| **Total BSS** | **9216 B** | **6144 B** |
| **Économie théorique** | | **3072 B** |

Une économie BSS n'est **ni** une garantie de `largest8` ni une garantie de vitesse. Le coût CPU de reconstruire l'entrée compacte doit être mesuré sur matériel.

## Contrôles

- Host : `ESP32/test/res60_p15_compact_snapshot_host.py` compile les deux **véritables modules C** P14 et P15 et compare les 55296 octets de chaque framebuffer, canaris, FNV, statistiques (intersections, tests, profondeur, indices des murs), plan des textures P10 et rendu de la texture artificielle (donc sourceU et clipping). 260 cas ordinaires + cas maximal à 512 segments identiques en profondeur, flags inconnus, source invalide et ligne hors borne. `nm -S` vérifie les tailles et la suppression du tableau P9.
- CI : normal `esp32-cyd`, normal inverted, les deux profils expérimentaux, host P4/P9/P14/P15, exclusion compile-time de P15 des profils production.
- **Vrai CYD** : `pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. Tourner N/E/S/W à chaud, START/MENU avec retour à froid ; comparer `heap8`, `largest8`, `minHeap8`, `raysUs`, `preparedUs`, les 4 FNV P3/P7 et FNV P8/P10, `[RES60P15] COMPACT`, PAK fermé et absence de watchdog.
- Si `largest8` ne se redresse pas ou si le raycast régresse de façon significative, approfondir le layout de RAM ou revenir au P14 ; pas de merge #364.

**Statut : CODE CANDIDAT / CI et vrai CYD non encore validés.** L'ensemble reste strictement preview BSP de MAP_INTRO sans gameplay, sprites, audio ou HUD complet en 192×144.
