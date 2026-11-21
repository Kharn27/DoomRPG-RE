# UAC-120-GENERIC-LARGER — bouton unique + police Doom, correctif palette/détourage (PENDING re-test CYD)

Base initiale après PR #411 fusionnée (assets seuls) : **main `b15efb4d9e04cccaac62f1432a613550533e92a3`**. Branche ensuite **réancrée sur main `4ece448f523d32db6ddcf85d1ca4492513339dbb`** (ARCH-RC23, historique CI conservé), sans chevauchement des fichiers fonctionnels UAC.

## Périmètre

Le cycle de comparaison de MENU_MAIN reste **CLASSIC → 120 ×2 → 240→120 ×2 → 240 direct → CLASSIC**. Seul le deuxième cran, `kNative120`, change :

- Châssis `menu_frame_120.bmp` **59×120**, identique.
- Un unique fond métallique vierge `/doomrpg/img/menu_generic_120.bmp` **55×20 BGRA V4**, partagé pour START/LOAD/OPTIONS/HELP et leurs deux états, agrandi en nearest-neighbor ×2. Le bouton est ouvert **une seule fois** pendant le FULL et réutilisé pour les quatre labels ; sur sélection, les deux rectangles 110×40 sont seuls rafraîchis.
- Les vrais dessins de glyphes de **`larger_font.bmp`** (208×102, 13×17, source grille ASCII `c-33`, 16 colonnes) sont centrés et projetés physiquement à 1:1 sur les boutons. Les couleurs sélectionnées suivent la vraie palette `a.bmp` argent/cuivre, les non sélectionnées sont atténuées. Les glyphes viennent du BMP, pas d'une police système ou des anciens boutons texte.
- Seules les 12 majuscules utiles sont mises en cache, **12×17×7 = 1428 octets de heap TEMPORAIRE uniquement en mode 120**, en indices palette Doom 4 bits (0 transparent, 1..15 couleurs canoniques). L'allocation est bornée et peut échouer proprement vers CLASSIC. **Libération explicite** quand on quitte le mode 120, lors d'un reset/retour gameplay et en cas d'échec de préparation : **aucune charge permanente de 1428 B sur la marge heap8 gameplay**. Pas d'atlas complet en RAM, pas de copie du BMP 55×20 persistante. Scratch glyph RGB565 **442 octets stack** pendant la préparation ; scanout BSS historique inchangé **6680 octets**.
- La source préférée est `/doomrpg/img/larger_font.bmp` sur SD si atlas indexé compatible ; si absent, mauvais format, géométrie ou erreur de lecture de glyphe, **repli vers le `larger_font.bmp` natif du `/DoomRPG-ESP32.pak`**, via la même API `EspNativeIndexedBmp` (sans ZIP). Les lectures sont terminées avant la peinture et la musique n'ouvre pas de second producteur.
- Si le bouton générique, le cadre ou les deux polices sont indisponibles, retour fail-closed au menu CLASSIC comme précédemment. Les 16 boutons historiques, les trois autres modes et la logique tactile restent inchangés.

**Cas important du PAK :** en MENU_MAIN le PAK est normalement fermé pour laisser vivre le lecteur WAV SD. Le nouveau `EspNativeIndexedBmp_openSdFontStandalone()` essaie la police SD sans ouvrir le PAK, puis le repli ouvre une **lease PAK temporaire** sans laisser le fichier ouvert pendant le scanout. La lecture glyphique SD est close avant le prochain morceau BMP. Un échec de glyph SD tardif rejette l'override et reprend tous les glyphes depuis le PAK, sans garder un cache partiellement valide.

## Validation logicielle et preuves

- `ESP32/test/font_sd_override_host.c` étend l'oracle qui compile le vrai décodeur C : accès à `larger_font.bmp` sur SD **PAK fermé**, refus d'autres atlas, PAK déjà ouvert, glyphes 13×17, mauvaise géométrie et invalidation.
- CI GitHub : `artwork/menu-uac/generic/tools/export_generic.py --check` valide la conversion et les SHA256 des BMP 120/240 ; le pilote existant `uac_bmp32_host.c` contrôle alpha, batches, parité FULL/PATCH et clipping ; compilation PlatformIO `esp32-cyd`.
- **Aucun résultat de compilation locale ni PASS CYD réel revendiqué avant l'exécution des checks et le retour Serial.**

## Test demandé sur classic CYD

1. Copier `sdroot/doomrpg/img/menu_generic_120.bmp` et `sdroot/doomrpg/img/larger_font.bmp` sur la microSD dans `/doomrpg/img/`. Ne pas toucher aux autres BMP ; garder `/DoomRPG-ESP32.pak`.
2. Compiler/flasher la branche **`agent/uac-120-generic-larger-font`**, env normal **`esp32-cyd`**. Sur le logo : CLASSIC → **120 avec texte Doom** → 240→120 inchangé → 240 direct inchangé → CLASSIC. Vérifier visuellement OPTIONS, centrage, alignement des boutons, état actif/inactif, aucune disparition de glyphes.
3. Sur le mode 120 : changer rapidement 10 fois de bouton, ouvrir OPTIONS puis Back, HELP puis Back, START puis retour MENU ; surveiller `[UAC] FONT120` (`backing=SD`), `PREFLIGHT OK ... generic=1`, `PRESENT ... paint=BUTTONS result=OK`, `buttonOpens`, SD/lcd temps, audio underruns/recoveries, HEAP8 et panic/WDT.
4. **Fallback** : retirer uniquement `larger_font.bmp`, redémarrer et vérifier `[UAC] FONT120 ... backing=PAK`, bouton toujours lisible. Ensuite remettre la police. Retirer le bouton générique pour vérifier retour au CLASSIC ; aucun besoin de retirer le PAK.
5. Communiquer un log Serial complet et idéalement les photos comparatives des modes 120 et 240. **PENDING CYD** jusqu'au verdict matériel.

Aucun merge automatique prévu ; la PR reste ouverte pour revue et test.

## Correction post-retour utilisateur (21 novembre 2026) — NON VALIDÉE SUR CYD

Le premier retour sur PR #413 était **négatif** (traits blancs sur les
arêtes, forme incorrecte du générique, couleur de `larger_font` différente
de `a.bmp`). Il **invalide toute déclaration de conformité visuelle** du
premier prototype.

Audit GitHub Actions des pixels des masters BMP/PNG :
- premier rang 120 : **54** pixels opaques génériques contre **20**
  START ; premier rang 240 **109** contre **36**. Cause fond rectangulaire
  au lieu de vraie forme du métal.
- `a.bmp` et `a_doom.bmp` : SHA Git et SHA256 strictement identiques ;
  variante inutile. `larger_font.bmp` et `larger_font_doom.bmp` étaient
  aussi identiques **avant** ce correctif.
- `larger_font` et `a.bmp` : palettes 16 couleurs réellement distinctes
  bien que les deux soient indexés 4 bpp et transparents sur magenta.

Corrections sur la même branche :
- **120 et 240 génériques** : alpha de START original et biseau
  périphérique copiés exactement ; centre générique conservé. BMP SD +
  PNG sources + manifest régénérés, pixels vérifiés par script déterministe.
- **Première étape de grande police SD (désormais remplacée)** :
  permutation des 16 couleurs depuis `a.bmp` ; 35 octets de palette modifiés
  sans modifier les glyphes. **Ce n'était pas suffisant visuellement** :
  voir la reprise complète des 94 glyphes ci-dessous. Même rendu couleur en mode 120
  avec fallback PAK grâce à une palette canonique en ROM.
- **Rendu UAC 120** : abandon de la conversion couleurs→luminance→blanc
  unicolore ; conservation des indices réels de glyphes Doom, transparent
  et contours noirs inclus. Cache temporaire toujours **1428 B**,
  retiré à la sortie du mode, aucun autre mode comparatif modifié.
- **SD dédupliquée** : suppression de `a_doom.bmp` (identique à
  `a.bmp`), suppression des variantes non-runtime `larger_font_doom.bmp`
  et `larger_font_doom_compat3.bmp` du répertoire SD ; ces deux dernières
  sont déplacées, par identité de blob, dans `artwork/fonts/sources/`.
  Tests host font mis à jour pour contrôler couleurs, géométrie et
  empreintes exactes des glyphes après correction.

**Nouveau test CYD demandé :** sur la microSD, remplacer
`/doomrpg/img/menu_generic_120.bmp`,
`/doomrpg/img/menu_generic_240.bmp`,
`/doomrpg/img/larger_font.bmp`. La petite police `a.bmp` est inchangée.
Après reset, dans le mode 120, vérifier au grossissement LCD les 4 boutons
et leurs biseaux/arrondis, la silhouette exacte par rapport aux modes 240,
les contrastes de caractères multicolores et l'absence de traits au bord.
Puis retirer seulement `larger_font.bmp` pour vérifier le même rendu
palette Doom via PAK. Serial demandé pour confirmer `[UAC] FONT120`,
`PREFLIGHT`, `PRESENT` et RAM/audio. **Aucun PASS CYD revendiqué.**

## Refonte des 94 autres glyphes (retour utilisateur supplémentaire, 21 novembre 2026) — PENDING CYD

L'utilisateur constate que **seuls les `a` et `o` minuscules** du grand atlas
sont satisfaisants ; la correction de palette seule n'a pas réparé le tracé
des 94 autres caractères. Une première variante de détourage qui conservait
le métal pollué de l'ancien grand atlas a été **rejetée après revue des
preuves PNG**, malgré 3215 pixels et 94 cellules modifiés : des parasites
gris/blancs restaient visibles.

**Implémentation retenue :** reconstruction **totale** de chacune des
94 cellules depuis le dessin de `a.bmp` petite police (9×12 → 13×17),
interpolation bicubique puis quantification dans **les 16 couleurs exactes**
de `a.bmp`, transparence magenta conservée. La source 13×17 d'origine
à palette harmonisée est archivée dans
`artwork/fonts/sources/larger_font_pre_full_repair.bmp`, et seules les
deux minuscules `a`/`o` préalablement validées sont recopiées à
l'identique depuis ce master. **10 653 pixels / 94 cellules exactement
changés**, zéro changement d'en-tête/palette/stride/dimensions 208×102,
16×6 ASCII `c-33`, format 4bpp BMP 10 726 o.

**Oracles :** `artwork/fonts/preview_full_repair.py --check`,
`artwork/menu-uac/generic/tools/repair_assets.py --check`,
`artwork/menu-uac/generic/tools/render_corrected_120.py --check`,
`ESP32/test/font_sd_assets_host.py` (empreinte totale
`1fb05564fd1041c94dd3b10970c826300155f2a9221aa7fac07ea0bb9c85bdcf`,
94 cellules `b4a46914a75ce8daee36f290c80f66fa5766e1a6887e87a47856d338b56b2bed`,
a/o strictement identiques à la source, palette exacte et structure BMP).
Les nouveaux PNG depuis le BMP final sont
`artwork/fonts/previews/larger_font_clean_zoom.png` et
`artwork/menu-uac/generic/previews/proof_120_four_selections.png` ;
**ils ne constituent pas un test LCD réel**.

**Retest :** mettre à jour **`/doomrpg/img/larger_font.bmp`** sur SD
(depuis la présente branche) et conserver le nouveau
`menu_generic_120.bmp` pour tester le menu 120. Vérifier toutes les
lettres de START/LOAD/OPTIONS/HELP, les minuscules et ponctuations
dans les écrans qui font appel à la grande police, la couleur gris/cuivre
et les bords des boutons. Un fallback PAK affiche le **font original du
PAK**, dont les silhouettes restent d'origine : la couleur canonique
est appliquée en menu 120, mais **ce fallback ne possède pas la nouvelle
reconstruction des 94 glyphes** ; ne pas annoncer une équivalence
pixel parfaite avec l'atlas SD. Logs Serial + photos requis ; aucun
PASS CYD revendiqué à ce stade.

## Harmonisation finale des minuscules `a` et `o` — re-test CYD

**Retour utilisateur après revue de la reconstruction complète :** les
**94 autres lettres/caractères sont jugés parfaits**, mais les minuscules
`a` et `o` dénotent car elles étaient volontairement préservées depuis
l'ancien atlas. C'était une erreur de cohérence stylistique, malgré leur
ancienne qualité intrinsèque.

**Correctif final borné :** suppression de l'exception `KEEP` dans
`artwork/fonts/preview_full_repair.py`. Les 96 cellules de 13×17
sont désormais générées par **un seul algorithme** à partir des 96 cellules
9×12 de `a.bmp` ; interpolation bicubique, seuil transparent et
quantification dans la palette métallisée inchangés.

Écart calculé par le vrai générateur de BMP :
- `a` minuscule : **90 pixels modifiés** par rapport au master précédent ;
- `o` minuscule : **71 pixels modifiés** ;
- **94 autres glyphes strictement identiques**, oracle SHA256 des 94
  `b4a46914a75ce8daee36f290c80f66fa5766e1a6887e87a47856d338b56b2bed` ;
- Total 96 cellules modifiées depuis l'ancien atlas historique :
  **10 814 pixels**. Pas de changement de format, header, palette,
  magenta, RAM, firmware, audio ou autres modes de menu.
- BMP SD final SHA256
  `9e16011cc6a6755bad5226143c342ec7339e47088f0c0dbdf3340518858f5350`.

Les preuves `artwork/fonts/previews/larger_font_clean_zoom.png`
et `artwork/menu-uac/generic/previews/proof_120_four_selections.png`
sont recalculées depuis le même BMP. Le menu est visuellement inchangé
par ce dernier ajustement car il n'utilise que des **majuscules**.
Les minuscules doivent être contrôlées dans l'aide, les dialogues ou
tout écran utilisant cette grande police.

**Statut matériel du correctif `a`/`o` : PENDING CYD.** Le jugement
utilisateur sur les 94 caractères est pris en compte ; aucun nouveau
PASS visuel n'est revendiqué pour les deux minuscules. Remplacer seulement
`/doomrpg/img/larger_font.bmp` sur SD suffit si les boutons génériques
corrigés sont déjà installés. Aucun merge sans validation utilisateur.
