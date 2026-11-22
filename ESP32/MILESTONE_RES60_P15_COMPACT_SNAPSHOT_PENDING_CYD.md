# RES60-P15 — snapshot compact 12 B, CI / CYD PENDING (22 novembre 2026)

**Branche** `agent/esp32-res60-p1` ; [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). Référence matérielle gelée : code P14 `547441d5805a7d8a89db0f9fad6bcb64426fa360`, [rapport P14](MILESTONE_RES60_P14_LINE_SNAPSHOT_PENDING_CYD.md). **P15 ne revendique aucun PASS matériel avant réception des logs du vrai CYD.**

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
