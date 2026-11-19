> **Archive de conception (PR #399).** Ce document décrit une étape
> intermédiaire avant la composition des quatre boutons. Les mentions
> « à faire » sont historiques et ne reflètent plus l'inventaire actuel.
> Lire le [README visuel](../README.md) pour les preuves finales,
> le dossier de livraison et l'état de validation.

# Boutons complémentaires du cadre UAC — LOAD / OPTIONS / HELP

Suite du lot START dans la **PR #399** (branche
`agent/menu-uac-frame-bmp-assets`). Source image :
`https://at.adobe.com/Y8K1xtmZWe8uhufl`, composition UAC utilisateur
1024 x 572. Textes EXACTS de la planche : `LOAD`, `OPTIONS` (pluriel),
`HELP` ; ne pas réécrire `OPTIONS` en `OPTION`.

- Découpes Adobe vérifiées visuellement : chacune isole exclusivement
  **son bouton**, sans panneau voisin (le premier auto-crop de LOAD
  incluait START : rejeté et remplacé par un recadrage borné).
- Fond damier remplacé par transparence réelle, sans pixels de
  damier apparent dans les contours.
- `active` : original orange / métal ; `inactive` : même traitement
  que START (brightness -26, saturation -68, contrast -12).
- Ratio conservé puis padding transparent et resizing
  **55 x 20** pour le cadre 120, **110 x 40** pour le cadre 240.
- Format **BMP V4 BGRA 32 bits BI_BITFIELDS**, masques RGBA explicites,
  `dibSize=108`, `pixelOffset=122`, masque alpha `0xFF000000`.
  Aucune couleur clé magenta ; **pas** de décompression runtime.
- Fichiers dans `sdroot/doomrpg/img/` :
  `menu_{load,options,help}_{active,inactive}_{120,240}.bmp`.
  Total attendu : **12 assets**, 6 par taille. Chaque état possède sa
  propre image. Les quatre variantes START restent inchangées.

Le cadre 120 fait 79 x 120, le cadre 240 fait 158 x 240.
Positionnement local indicatif des **4** boutons START/LOAD/OPTIONS/HELP :
`x=12`, `y=[10,37,64,91]` en 120 ;
`x=24`, `y=[20,74,128,182]` en 240. Dessiner les boutons avant le
cadre pour conserver ses bordures et reflets métalliques. Cette
disposition est **illustrative**, sans preuve sur CYD.

**Scope strictement asset :** aucun moteur SDL/desktop réintroduit,
aucune lecture ZIP, aucun changement firmware/menu/renderer.
Le lecteur BMP32 alpha SD et la validation CYD sont des étapes
séparées. Un import CI vérifie dimensions, canal alpha, round-trip
RGBA pixel à pixel et distinction actif/inactif.

## Vérification CI des 12 BMP

[Import CI GitHub Actions — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44572105162).
Vérifications automatisées : dimensions, BMP V4 alpha 0..255, comparaison
RGBA à l'identique du PNG Adobe après encodage, état actif distinct de l'inactif,
12 fichiers présents et importeur éphémère supprimé.

| Asset | SHA-256 |
|---|---|
| `menu_load_active_120.bmp` | `9fa5bf484ba8d3f213a9626454ee5596ad0c315587de87e322ad9edf3b1fad80` |
| `menu_load_active_240.bmp` | `8913ded7eb69e951d6f4176418916b398e14fa6623c9af2651b586826d165abc` |
| `menu_load_inactive_120.bmp` | `37716377cd3bf9f1bd773835b49eab9c4296066d749be82343ad20ec31015a5b` |
| `menu_load_inactive_240.bmp` | `0b8e30a1e1dfcd48d2248d306bd111598404cecd417bbb832b06b67a55020062` |
| `menu_options_active_120.bmp` | `8ad4bf03bae92326239ac44869b90ba9eb85a5ae1d2064d87849c7e5650df9aa` |
| `menu_options_active_240.bmp` | `c8b2bdd076218897c8159be80e7b54fc6e3548dce583955a4dead2d354900463` |
| `menu_options_inactive_120.bmp` | `0874fb1781b704d4e03fa0c6067cf84294de8a196d22a2a344ad68d8da566fb2` |
| `menu_options_inactive_240.bmp` | `af772df03205a770822434814e8940939cfbbe9f232208313c59c37ede6e4709` |
| `menu_help_active_120.bmp` | `3e07d8ea55520cbb47c6a73e5f01e0aac78a351738e7bac05afabacd1d86cc81` |
| `menu_help_active_240.bmp` | `63054b67f2c7c6024e4e1392dd34cb165e074eef6f810b518fca0804ad9ecb55` |
| `menu_help_inactive_120.bmp` | `c70c02fe8e8565ebc38460b924a8080d9c10f16057356faa74813d04a3936274` |
| `menu_help_inactive_240.bmp` | `a3aae4fb6f2f8de23ca4914110b980b1f2a12a6d47ede24786039f74a329d03d` |
