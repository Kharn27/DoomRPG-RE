# Boutons START (cadre UAC)

Quatre assets distincts pour le premier emplacement du cadre UAC ajouté en
PR #399. La source graphique est le bouton START orange situé dans la
composition UAC originale de l'utilisateur (1024 x 572), **pas** les BMP
`j.bmp` (logo Doom RPG) ni `larger_font.bmp` (glyphes à couleur-clé rose).

Adobe : détourage du START, suppression du damier incrusté (transparence
réelle), état actif original orange, état inactif assombri et désaturé.
Redimensionnement avec conservation du ratio et padding alpha très léger.

| Asset | Dimensions du bouton | Cadre associé |
|---|---|---|
| `menu_start_active_120.bmp` | **55 x 20** | `menu_frame_120.bmp` (79 x 120) |
| `menu_start_inactive_120.bmp` | **55 x 20** | `menu_frame_120.bmp` (79 x 120) |
| `menu_start_active_240.bmp` | **110 x 40** | `menu_frame_240.bmp` (158 x 240) |
| `menu_start_inactive_240.bmp` | **110 x 40** | `menu_frame_240.bmp` (158 x 240) |

Tous les boutons sont des **BMP V4, BGRA 32-bit, BI_BITFIELDS, masque alpha
0xFF000000 explicite**, comme les cadres : `pixelOffset=122`,
`dibSize=108`, sans compression. Transparence réelle pour le contour et
le padding ; ne **pas** traiter leur alpha comme une couleur-clé magenta.

Position de prévisualisation dans un espace local au cadre (suggestion) :
`x=12, y=10` (cadre 120) et `x=24, y=20` (cadre 240), avec le
**cadre dessiné au-dessus** du bouton pour couvrir les quelques pixels
de bordure. Les trois autres boutons restent volontairement à faire.

**Assets seulement :** aucun lecteur BMP32 alpha ni branchement du menu
ESP32 modifié. L'utilisation runtime nécessitera un décodage SD par
lignes, sans allocation image complète, et la validation matérielle
normale `esp32-cyd`.

Vérification automatique GitHub Actions : [import quatre boutons](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44569358998),
**SUCCESS** (lecture PNG, alpha 0..255, contrôle dimensions, réouverture
BMP et comparaison des octets RGBA pixel par pixel). Empreintes SHA-256 :

| BMP | SHA-256 |
|---|---|
| `menu_start_active_120.bmp` | `2a4967a80b7accb57e8481ebaaa27c86006fbff53cb512a96d8f7c9a1bef8a56` |
| `menu_start_inactive_120.bmp` | `101b13334059f448586bf0e0e5cdc60d476d80380e3300142d2bf68a4499548e` |
| `menu_start_active_240.bmp` | `91fd666bde662870c6d1d3934158b0c535ca7919d3df19df7ff6a6704fde9c5a` |
| `menu_start_inactive_240.bmp` | `c70c56e7cf9f050c1eeb1f0698a96ecd1f1beace8499894807f94e3b55755319` |
