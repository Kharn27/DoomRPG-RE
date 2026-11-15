# Issue #340 — R-A1 : oracle réel de span mural (PASS CI/CYD, 15 novembre 2026)

## Base et portée

- Base GitHub `main=839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1`, après SDL-F31 / PR #339.
- **Modifications : tests hôte uniquement**. Aucun octet des sources firmware, du PAK, du linker, du rendu ou des allocations runtime n'est modifié.
- L'oracle de texture actuel `ESP32/test/native_wall_boundary_inline_host.py/.c` extrait et compile maintenant **`Render_SpanMode0` de `src/Render.c`** avec **`sampleWallSpan` + `prepareLegacyWallGuard` réels** de `ESP32/src/esp_native_first_frame.c`. Le moteur desktop n'entre pas dans le firmware.

## Contrat fixed-point et domaine

Le sampler reçoit une position et un pas signés en fixed-point *effectivement 13 bits fractionnaires par octet compacté* :

- `packedIndex = localPosition >> 13` : index du byte 4-bpp parmi `WALL_PACKED_BYTES = 2048` (64×64 = 4096 nibbles).
- `(localPosition >> 10) & 4` vaut **0 (nibble bas)** ou **4 (nibble haut)** ; `(packed >> shift) & 0xf` indexe les 16 couleurs RGB565.
- Le point de départ du mur est `(i13 << 6) << 12`, avec `i13` issu d'une projection de 0 à 63 ; le pas vertical est `i12 >> 3`, et un clipping supérieur peut soustraire `step * lignes` avant le premier pixel.
- `0 <= packedIndex < 2048` lit uniquement le cache 2048 B ; `packedIndex == 2048` demande exactement **un byte de continuation** résolu dans les mappings, sous limite de quatre guards BSS par frame ; `packedIndex > 2048` ou négatif/échec de résolution => **fail closed**.
- Pas d'accès séquentiel aveugle `PAK[source + 2048]` : le byte successeur peut appartenir à une texture qui **n'est pas physiquement adjacente** (fixture : logical15/actual40/source20480 -> actual68/source32768). La résolution parcourt les IDs du catalogue et les phases `0..3`, choisissant le plus petit offset source strictement supérieur au courant, après vérification des mappings, de l'alignement et des bornes PAK.
- Côté code desktop : `Render_SpanMode0` lit `mediaTexels[param_4 >> 13]`, puis applique le même shift nibble. Ce tableau legacy global **n'est pas autorisé** sur CYD ; seul son algorithme de pixel sert d'oracle.

## Ce que teste R-A1

- Les 2 000 prélèvements déjà présents sont conservés : les deux nibbles du byte successeur, le refus du 2049e index, la limite quatre guards, les mappings corrompus et le PAK fermé.
- **4 098 comparaisons supplémentaires** via le vrai `Render_SpanMode0` compilé depuis `src/Render.c` : chaque nibble de 2049 positions (0..2048), byte pattern déterministe, et FNV-1a 32-bit des pixels RGB565 little-endian attendu `ae456812`.
- Un span supplémentaire de **9 pixels** traverse le point 2048 et force la voie legacy **déroulée par blocs de huit**, en plus de la voie scalaire. Le hash et les pixels des deux versions doivent coïncider bit à bit.
- Le double PAK compte les appels et octets lus ; les répétitions du même guard ne doivent ajouter aucune lecture. La détermination de la provenance de continuation et la parité nibble sont vérifiées **séparément**.

### Limites de la preuve

L'atlas legacy de ce test est **synthétique** : l'octet de continuation résolu par le double PAK est placé au bord du flux compacté logique pour comparer la méthode de rasterisation. Ceci ne constitue ni une extraction de `mappings.bin/wtexels.bin` réels, ni un oracle complet de frame depuis les données originales, ni une mesure de latence microSD.

**R-A reste ouvert** : constituer des fixtures tirées des vrais assets originaux, comparer des FNV complets de frames et phases animées, instrumenter sans allocations supplémentaires la fréquence de `packedIndex == 2048`, les conditions `> 2048`, les lectures et latences SD sur le vrai `esp32-cyd`. Seulement ensuite envisager R-B. Ne pas attribuer à ce test un PASS matériel ni un gain heap.

## Garde-fous

`shapeData == NULL`, `mediaTexels == NULL` **dans le firmware** ; pas de ZIP en runtime ; 2048 B par cache slot, quatre guards maximum, rendu et RNG non modifiés. `--wrap` (45 flags normal) reste inchangé pour le futur audit R-C.


## Validation R-A1 après test matériel

PASS sur classic CYD normal, candidat `6a9f4b384649259674e99d3ec82ecc29699be217`, avec aucun changement production. Détails et SHA256 du log : [MILESTONE_PR341_ARCH_RA1_WALL_PARITY_CYD_PASS.md](MILESTONE_PR341_ARCH_RA1_WALL_PARITY_CYD_PASS.md). **14 BOUNDARY-INLINE** correspondant à 2 véritables couples de textures, aucun échec de rendu ; il ne faut pas confondre ce comptage avec une fréquence par pixel. Les vrais assets/frames golden et métriques SD restent hors de R-A1. Documentation seule après test.
