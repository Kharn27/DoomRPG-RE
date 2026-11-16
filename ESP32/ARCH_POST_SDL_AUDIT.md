# Audit post-SDL — fermeture firmware et dette native restante

**16 novembre 2026 — [PR #351](https://github.com/Kharn27/DoomRPG-RE/pull/351)**. Base vérifiée du véritable `main` après fusion SDL-F36 #349 :
`c3be0e7b9bb1a5b521a3962bc0210065add8214f`.

## Décision : désengagement SDL du firmware terminé

Le chantier **SDL-F** est **clos au niveau du firmware**. Il n'existe aucune justification pour créer SDL-F37 par simple continuité de numérotation. Il reste des contrats ABI desktop dans du code natif et des oracles historiques, **ce qui n'est pas une dépendance SDL du binaire ESP32**.

Preuves indépendantes, conservées en CI :

1. `ESP32/scripts/build_engine.py` admet les vrais propriétaires `ESP32/src` ; ses gardes refusent le retour de `esp32_sdl.cpp`, `SDL.h`, `SDL_mixer.h` et `esp32_sdl_platform.h` en production, ou la disparition des oracles. Les **21** TUs C desktop originaux `src/*.c` ne sont jamais enregistrés ni générés pour le firmware.
2. Les sources et headers historiquement SDL (shim, lecteur BMP, headers) demeurent **byte-exact** dans `ESP32/test/oracles/` : spécifications exécutables, tests C/C++ et ABI host/geometry, **pas des sources firmware**.
3. `ESP32/test/sdl_f30_compiler_header_closure_host.py` lit la véritable compilation `compile_commands.json` et les dépendances GCC `.d` de chaque TU `ESP32/src`. Il refuse tout `SDL.h`, `SDL_Video.h`, `SDL_mixer.h`, `Z_Zip.h`, direct ou transitif, ainsi que la compilation du shim ou du vieux BMP.
4. `ESP32/tools/audit_desktop_elf.py` contrôle les objets admis et les owners / symboles du **vrai ELF**, pas un simple grep du code. La CI vérifie également les guards d'imports, les comparaisons avec SDL-A, les oracles F2-F36 et l'absence d'anciens symboles liés.
5. Les deux profils release `esp32-cyd` et `esp-cyd-inverted` possèdent le **même ensemble de TUs et de defines du moteur** (seuls sélecteur LCD et métadonnées d'environnement diffèrent). Bringup et trace gardent leurs diagnostics isolés.

### Fermeture explicite du quatrième profil dans la PR #351

Avant cette PR, les builds et la parité de l'inverted étaient testés, mais le test F30 de **fichiers compilateur `.d` réels** portait uniquement sur `esp32-cyd`, bringup et trace. L'ancienne baseline SDL-A ne possède pas l'environnement inverted : il aurait été incorrect de la forcer à servir d'ELF comparatif.

La PR #351 ajoute donc **une preuve native indépendante** sur l'ELF et les `.d` de `esp-cyd-inverted`, sans modifier la campagne historique SDL-A ni le moteur :

- `sdl_f30_compiler_header_closure_host.py --profile esp-cyd-inverted` ;
- audit ELF du retirement `Render.c`/`DoomRPG.c` et présence des vrais owners natifs présentation, clock et RNG ;
- vérification après le build inverted autonome, le build normal et les builds diagnostiques ;
- conservation de tous les tests de parité et d'exclusion préexistants.

**Code testé** : `5c2e83a2bf8035927a1835b358d85d190395c03f`.
Double CI **SUCCESS** : [PR #43778489195](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43778489195) et [push #43778458637](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43778458637).
L'étape `Audit inverted-panel firmware GCC dependencies and desktop ELF retirement` a réussi dans les deux runs.
Le HEAD documentaire final doit repasser les mêmes CI avant une éventuelle fusion.

## Le moteur est natif, mais pas encore débarrassé de toute ABI desktop

La migration n'est pas un prétexte pour supprimer les oracles ou réécrire le gameplay existant :

| Domaine | État actuel | Dette / action rationnelle |
| --- | --- | --- |
| Chargement | `/DoomRPG-ESP32.pak` natif, accès bornés, jamais de ZIP runtime | préserver la provenance octet par octet |
| Carte et gameplay | modules `esp_map_*`, `esp_native_gameplay_*` réellement actifs | limiter les owners, clarifier les lifetime/contrats |
| Renderer | framebuffer logique 160×120 RGB565, caches bornés, `shapeData==mediaTexels==NULL` aux témoins matériels | isoler résolution de texel et raster sans changer les pixels |
| API héritées | certains `Render_t`, `DoomRPG_t`, headers historiques nécessaires au code natif | extraits progressifs par consommateur prouvé ; éviter une copie desktop |
| Composition | **45** `-Wl,--wrap=` dans `esp32-cyd` (plus cinq spécifiques bringup) | inventorier leurs interpositions ELF réelles et retirer **une frontière par PR** avec preuves |
| Écran | deux profils LCD, moteur commun, nearest-neighbor ×2 | réglages chromatiques matériels distincts de la logique RGB565 |
| Audio | backend PCM/DAC natif AUDIO-R2d/R2e intégré à `main` | aucun changement pendant cet audit |

**Preuves et limites du renderer :** issue [#340](https://github.com/Kharn27/DoomRPG-RE/issues/340), R-A1/R-A2a/R-A2b. Le cache mural conserve des blocs 4-bpp de **2 048 B** et un garde strict d'au plus **quatre octets successeurs** (quatre descripteurs en BSS). À `packedIndex==2048`, le successeur de l'atlas compact desktop peut être une autre texture map-selected, **non l'octet physique suivant du PAK**. Sur Entrance, **7 des 37 frontières** ne sont pas équivalentes à l'adjacence brute ; le témoin actual40/source20480 → actual68/source32768 vaut `11`, contre `b5` pour l'octet brut. `packedIndex>2048` échoue fermé. Ne jamais clampler, faire un modulo ou reconstituer `mediaTexels` map-wide.

Le vrai CYD a déjà validé les cartes et 19 poses R-A2b avec FNV, et les oracles C de murs couvrent des cas synthétiques. **Ce n'est pas encore une parité reproductible de frames complètes avec le renderer desktop sur les vraies ressources** : le run du binaire oracle C sur le PAK privé et le golden full-frame desktop restent des travaux distincts. Les assets privés ne doivent pas entrer dans Git.

## Prochaine amélioration recommandée — issue #340, pas SDL-F37

**R-B en deux frontières vérifiables :**

1. **Consolider la preuve pixel et la provenance** : rendre reproductible l'exécution de l'oracle C avec le PAK original hors CI et un golden full-frame/hors firmware pour quelques poses/phases, sans exposer les assets. Relever les cas de frontière, les négatifs PAK corrompu/absent et les limites de cache.
2. **Découpler résolution et raster** : une petite interface native immutable pour un bloc mural + sa politique de successeur prouvée, avec cache inchangé 2 048 B/slot, limites d'accès PAK et échec fermé. Déplacer progressivement la recherche de successeur hors de la boucle pixel ; comparer strictement les pixels/FNV avant et après. Nouveau firmware normal `esp32-cyd` à valider sur CYD réel avant de documenter un PASS matériel.

Ensuite seulement, **R-C** : audit ELF des 45 `--wrap` et retrait ciblé d'une interposition native à la fois. **R-D** : réduire la surface des headers desktop par de petits types et interfaces explicitement possédés. Éviter les refactorings motivés uniquement par le nom d'une API ou la taille d'un fichier.

## Frontière de validation

Cette PR est **CI-only**, sans changement `ESP32/src`, `src/`, `ESP32/platformio.ini`, écran, audio, PAK, scripts événementiels ou ABI firmware. Aucun gain RAM/flash, changement de pixels, nouveau test sur CYD ou PASS matériel nouveau n'est revendiqué. L'audio R2e garde son statut propre. **Pas de merge sans autorisation explicite.**
