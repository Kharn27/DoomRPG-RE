# Issue #340 — R-A2a : vérifier le véritable atlas mural compact via PAK-v2 (host-only)

## Origine et portée

Continuation de [PR #341](https://github.com/Kharn27/DoomRPG-RE/pull/341) **sur la même branche**, sans merger. Base GitHub main `839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1`. R-A1 et son PASS CYD `platformio-device-monitor-261115-103040.log` restent inchangés. Le code R-A2a test/CI au SHA `358aeb05adf3e078859e039cf7e3606dfd39034b` passe les [CI PR 43701781214](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43701781214) et [push 43701777718](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43701777718), chacune **167/167 SUCCESS**. La CI après ce rapport doit être contrôlée séparément.

- Nouveau runner : `ESP32/test/native_wall_pak_provenance_host.py`.
- Extension **hôte uniquement** : `ESP32/test/native_wall_boundary_inline_host.c`. L'atlas de 65536 B est un **buffer de test desktop**, jamais une allocation du firmware ESP32.
- CI : un test supplémentaire (167 étapes incluant la construction et les audits existants).
- **Aucune modification** de `ESP32/src`, `src/Render.c`, du PAK runtime, des buffers ou de `platformio.ini`.

## Découverte indispensable : deux adjacences différentes

Le véritable `src/Render.c::Render_loadTexels` **trie** `mapTextureTexels` suivant `mediaTexelOffsets`, **copie 2048 octets** pour chaque texture source distincte requise, et construit un **`mediaTexels` compact** pour la carte. `Render_SpanMode0` lit ce nouvel atlas, non `wtexels.bin` brut. `Render_addMapTextures` inscrit les quatre phases `base+0..3`. Un `packedIndex==2048` consulte donc le *premier octet de la prochaine texture distincte sélectionnée par la carte*, et pas automatiquement `wtexels[source/2 + 2048]`.

Le test R-A1 injectait ce voisin calculé dans un atlas synthétique : parité de l'algorithme de raster, **mais preuve circulaire insuffisante du prochain bloc**. R-A2a distingue explicitement ces deux notions et exige de dériver la sélection du BSP.

## Chaîne originale de preuves

1. Lit un fichier `DoomRPG-ESP32.pak` v2 **sur le poste hôte** (magic `DRPGESP2`, v2, index trié par FNV, offsets et tailles bornés, CRC32 streamé pour les quatre ressources). **Aucun ZIP runtime**.
2. Décode `intro.bsp` avec le layout réellement suivi par `EspBspReader` : header 33 B, nodes 10 B, lines 10 B (texture à +4), sprites 5 B (`0x40000` tile-texture, skip `0x20000000`), events 4 B, byteCodes 9 B, chaînes, blockmap 256 B, planes 2048 B, floor/ceiling et compagnons legacy (34→92, 33→91, 9↔10). Forme le set des textures demandées par Entrance.
3. Reconstitue pour chaque ID les quatre variantes avec `mappings.bin`, impose pair offsets alignés et cache 2048 B borné, choisit le plus petit *source offset distinct supérieur* parmi les ressources de la carte. Le choix doit correspondre aux vrais diagnostics Serial, pas à un simple index de fichier.
4. Charge par *ranges* seulement les 2048 octets muraux, la palette 16 couleurs BGR565 convertie exactement en RGB565, le byte voisin brut du PAK **pour diagnostic**, et le premier byte du successeur compact **pour la parité desktop**.
5. Compile, en extraction des véritables sources, `src/Render.c::Render_SpanMode0` (scalaire et ancien déroulage ×8) et `ESP32/src/esp_native_first_frame.c::sampleWallSpan` avec `prepareLegacyWallGuard`. Compare les deux nibbles des 4096 pixels par texture, soit **8192 pixels** pour les deux cas, FNV32 par pixel RGB565 LE plus quatre nibbles de frontière. La fixture hôte transitoire ne contient que deux murs de 2048 B + palettes + métadonnées.

Les deux cas sont les **witnesses matériels**, non des données de texture inventées :
- `logical15 actual40 source20480 → successorActual68 source32768 byte11` : le successeur sélectionné n'est pas contigu au bloc brut `wtexels.bin` (`20480+4096 != 32768`).
- `logical49 actual112 source65536 → successorActual116 source69632 bytea7` : ici l'offset successeur coïncide avec la contiguïté brute, ce qui **ne permet pas** de généraliser.

## Ce qui est réellement vérifié par CI

Le dépôt ne publie **ni les véritables `DoomRPG.zip` ni `DoomRPG-ESP32.pak`**. La CI crée donc un **mini-PAK synthétique** qui respecte les structures on-disk, avec un BSP cohérent et quatre groupes de textures. Il sert à vérifier le lecteur et le runner, **pas à inventer de pixels Doom originaux**.

- Test positif : 8192 pixels et 4 nibbles de frontière de l'atlas compact reconstitué identiques entre C legacy et C natif.
- Test de **faux voisin PAK** : modifier seulement le byte physiquement adjacent au premier mur **ne doit pas** provoquer de faux échec, puisque le desktop trie/compacte les blocs sélectionnés.
- Test de **vraie divergence** : falsifier indépendamment le byte de l'atlas compact dans la fixture doit donner un **exit 2**, fail closed, sans considérer l'atlas R-A1 comme une preuve.
- CRC corrompu, magic corrompu, offset source incorrect, octet successeur différent du Serial : refus explicite.

La lecture du PAK original et le golden original **ne sont PAS exécutés dans CI** : aucun fichier source autorisé n'est présent dans le dépôt ou les pièces jointes.

## Mode optionnel authentique

Depuis la racine du repo, avec la copie personnelle du PAK présent sur microSD :

```bash
python ESP32/test/native_wall_pak_provenance_host.py --pak /chemin/vers/DoomRPG-ESP32.pak
```

Le runner exécute d'abord les négatifs synthétiques, puis contrôle le vrai PAK (index/CRC/BSP/mappings/palettes/2048 octets x2, provenance et pixels). Sorties `[RA2 ORIGINAL PAK]` puis `RA2 PACK ... boundaryParity=EXACT`. Si les preuves ne concordent pas avec le Serial, il échoue au lieu de forcer une équivalence. Aucun asset n'est copié dans Git, ni injecté dans le firmware.

## Limites strictes / suite

R-A2a **ne couvre pas** le framebuffer complet 160×120, l'occlusion/sprites/plans, le rendu des phases animées, les FNV de plusieurs poses sur vrais assets, la fréquence `packedIndex==2048` sur CYD ni le débit/latence microSD. La parité des deux témoins originaux attend le vrai PAK. **Ne pas ouvrir R-B (isolation raster/resolve) sur cette seule étape**. Pas de changement mémoire, input/gameplay, save, shapeData/mediaTexels ou --wrap. R-A2b doit capturer des vrais golden frames avec domaine et temps de mesure bornés, puis faire le test hardware normal `esp32-cyd`.
