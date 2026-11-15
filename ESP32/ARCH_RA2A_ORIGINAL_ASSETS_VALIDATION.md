# Issue #340 — R-A2a : véritables DoomRPG-ESP32.pak + DoomRPG.zip, provenance des murs Entrance

**Date : 15 novembre 2026.** Continuation de [PR #341](https://github.com/Kharn27/DoomRPG-RE/pull/341) sur \`agent/esp32-arch-ra-real-span-oracle\`, base \`main=839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1\`. Ce document consigne **la vérification directe sur les deux fichiers originaux fournis par l'utilisateur** et la distingue expressément des CI host sur données synthétiques. Il ne publie **aucun texel, palette ni BSP source**.

## Provenance et identité (vérification locale, fichiers originaux)

| Fichier | Taille | SHA-256 |
|---|---:|---|
| \`DoomRPG-ESP32.pak\` | 2 457 398 B | \`cd60f352962531cda0c3b6ffc08a0e02bb5d4a028ac5ab957f063b9c43952464\` |
| \`DoomRPG.zip\` | 1 940 320 B | \`9e783fd6fae7185c2fd62f5578f3a8e58e7ca775e99780cd738a1030eac7baed\` |

PAK v2 \`DRPGESP2\` : header 24 B, index 241 × 20 B, \`dataOffset=4844\`. **241/241 ZIP entries** retrouvées par noms normalisés/FNV dans le PAK. Tailles, CRC32 et **payloads byte-for-byte identiques** pour chaque fichier, aucune entrée manquante et aucune divergence. Cette vérification a été exécutée localement en Python sur les uploads et pas seulement déduite de la CI. Les ressources centrales et leurs CRC :

| Ressource | Taille | CRC32 |
|---|---:|---|
| \`intro.bsp\` | 21 823 B | \`623f34e4\` |
| \`mappings.bin\` | 8 392 B | \`fc4fd8fa\` |
| \`palettes.bin\` | 6 564 B | \`6b880ec2\` |
| \`wtexels.bin\` | 116 740 B | \`32e758b5\` |

## Atlas compact réel et goldens sur Entrance

\`intro.bsp\` est **structurellement fermé à 21 823 B** : 223 nodes, 480 lines, 344 map sprites, 93 events, 265 bytecodes, 94 strings. Reconstruction des textures demandées via le contrat réel \`ESP32/src/esp_bsp_reader.c\` : **33 ressources logiques**, **132 références de phases** et **38 offsets sources uniques** après déduplication et tri, soit 38 × 2048 = **77 824 B d'atlas compact purement *hôte***. Aucun tel atlas n'est créé dans le runtime CYD. Empreintes du flux des 38 blocs **en ordre source croissant**, indépendamment vérifiées sur le PAK :

- FNV-1a32 bytes atlas : \`76568395\`.
- SHA-256 bytes atlas : \`ce1eeabced574e0b9b6036867c4bfead67c328d3bfaeb4c9d765831ae10adc9b\`.
- Parmi les **37 frontières** entre blocs distincts, **7 octets voisins bruts du PAK ne correspondent pas** au premier octet du successeur sélectionné. Une lecture physique aveugle n'est donc pas équivalente à l'atlas legacy.

Deux témoins **observés dans le Serial du vrai classic CYD**, puis retrouvés ici dans les assets avec palette RGB565 dérivée de la véritable BGR565 :

| Témoin | Source → successeur compact | Voisin PAK brut | Octet compact attendu | FNV32 des 4096 pixels RGB565 |
|---|---|---|---|---|
| logical15, actual40, palette208 | 20480 → actual68/source32768 | \`b5\` | \`11\` | \`2d6445f1\` |
| logical49, actual112, palette480 | 65536 → actual116/source69632 | \`a7\` | \`a7\` | \`a57dc6c1\` |

Pour logical15, la substitution erronée \`b5\` donnerait \`4acc\` / \`10a3\` en RGB565 aux deux nibbles, au lieu de \`ffff\` / \`ffff\` avec le bon byte \`11\`. C'est un **contre-exemple pixel effectif sur l'asset original**, pas simplement une différence de valeurs brutes. Pour logical49, les deux voies coïncident, ce qui ne suffit pas à justifier une optimisation généralisée.

Les empreintes brutes et paramètres attendus sont consignés dans \`ESP32/test/ra2_original_asset_fingerprints.json\` : **uniquement des empreintes, des offsets et des métadonnées**, aucun payload propriétaire.

## Test permanent sur la branche et protocole

Au code candidat \`30442a4028c2fbb4a304db6b7ba8b20444efbdc2\`, \`native_wall_pak_provenance_host.py\` accepte désormais :

\`\`\`sh
python ESP32/test/native_wall_pak_provenance_host.py \
  --pak /chemin/DoomRPG-ESP32.pak \
  --zip /chemin/DoomRPG.zip \
  --expect-original
\`\`\`

\`--zip\` compare tout le contenu ZIP au PAK. \`--expect-original\` impose les SHA-256, longueurs, CRC, nombre de textures de la carte, empreintes de l'atlas source et les FNV32 des deux murs issus **des fichiers fournis**. Le runner standard compile les **véritables fonctions C extraites** \`Render_SpanMode0\`, \`sampleWallSpan\`, \`prepareLegacyWallGuard\` et compare leurs résultats. Aucun fichier asset n'est ajouté à Git ou au firmware.

**Distinction probatoire stricte :** la validation locale exécutée ici porte sur le PAK/ZIP, le BSP, les mappings, les octets originaux, les palettes et les goldens calculés de façon indépendante en Python. Le binaire C de l'oracle **n'a pas été exécuté localement avec ces uploads** faute de copie locale du dépôt source ; il reste à exécuter par le runner ci-dessus. Les deux CI du **code** [PR 43708950493](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43708950493) et [push 43708945976](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43708945976) sont **SUCCESS 167/167** et exécutent le C sur les **fixtures synthétiques**, pas sur les fichiers privés. Le nouveau HEAD Markdown doit lui-même refaire sa double CI.

## Limites et décision architecturale

- **Preuve désormais acquise sur les vrais fichiers** : origine ZIP↔PAK, exactitude des offsets, provenance des deux gardes, reconstruction du compact source et goldens pixel de deux murs.
- **Non prouvé** : pixels d'une image complète 160×120 avec vrais sprites, plans et occlusion ; plusieurs poses/phases temporelles ; compteurs \`packedIndex==2048\` en runtime ; latence/reads réelles sur microSD ; parité originale du binaire C jusqu'au run \`--pak\` effectif.
- **Ne pas** remplacer le resolver par un voisin physique brut, ni réintroduire \`mediaTexels\` map-wide ou des assets ZIP en runtime. **Aucun changement de \`ESP32/src\`, PAK, RAM ou flags \`--wrap\`** dans cette étape.
- La prochaine étape est de lancer l'oracle C en hôte sur ces fichiers via la commande, puis R-A2b frame golden + mesures matériel, **avant** R-B. Le PASS CYD R-A1 reste attaché au firmware de \`main\` inchangé.
