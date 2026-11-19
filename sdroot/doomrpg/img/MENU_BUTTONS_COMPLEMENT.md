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
