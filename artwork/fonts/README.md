# Polices Doom alternatives — ESP32 classic CYD

Deux planches **4 bits, indexées, prêtes à copier sans renommage** dans
`/doomrpg/img/` sur la carte SD. Le jeu garde ses polices PAK d'origine
en fallback si le BMP SD est absent ou incompatible.

| Fichier runtime (dans `sdroot/doomrpg/img/`) | Taille | Cellule | Stockage |
| --- | --- | --- | --- |
| `a.bmp` | 144 × 72 | 9 × 12, 16 × 6 glyphes | **5 302 o**, BMP 4 bits, palette 16 couleurs |
| `larger_font.bmp` | 208 × 102 | 13 × 17, 16 × 6 glyphes | **10 726 o**, BMP 4 bits, palette 16 couleurs |

### Livraison SD dédupliquée et palette harmonisée (PR #413)
`a.bmp` est **le seul fichier petite police** : `a_doom.bmp` était son
**doublon binaire exact** et a été retiré du dossier de déploiement.
`larger_font.bmp` est désormais le seul grand atlas runtime ; sa palette
16 couleurs reprend **exactement les 16 entrées de `a.bmp`**, en conservant
le tracé de chaque glyphe, ses indices 4-bpp, le magenta transparent, les
96 cellules et le format BMP original à l'octet près hors palette.

La grande police historique non recolorée et la variante distincte
`larger_font_doom_compat3.bmp` (3 couleurs) sont conservées uniquement dans
`artwork/fonts/sources/` pour référence et comparaison, **pas** dans
`sdroot/`. L'ancienne variante BMP RGB24 non prise en charge reste accessible
dans l'historique Git de la PR #407. Aucun alias `_doom` n'est consommé
par le runtime ni requis sur la carte SD.

## Correction ciblée de `a` et `o` minuscules

Le placement original avait produit un `o` minuscule coupé sur la petite
planche et des `a`/`o` déformés à différentes tailles. Les quatre glyphes
ont été reconstruits depuis les originaux métalliques, projetés sur les
**silhouettes des polices Doom RPG de référence**, puis quantifiés avec les
**palettes 16 couleurs existantes**. La silhouette évite toute modification
de l'avancement et de la découpe des cellules ; le dégradé argent/cuivre
reste conservé.

**Contrat d'absence de régression sur les formes** : les **94 autres
caractères de chaque BMP sont strictement identiques pixel par pixel**.
Entêtes, ordre, clé magenta, tailles, stride et encodage restent identiques.
Seule **la palette de `larger_font.bmp`** change dans la PR #413, sans
aucune modification des octets de glyphes. Le test
`ESP32/test/font_sd_assets_host.py` contrôle la palette canonique,
les empreintes des 94 glyphes, les deux petits caractères corrigés,
l'absence des variantes redondantes en SD et la conservation de la
version originale dans `artwork/fonts/sources/`.

## Couleurs à la volée

Le Canvas natif `EspNativeCanvas_drawText` appelle
`EspNativeTexture_setColorMod` avec `doomCanvas->fontColor`.
`EspNativeTexture_blit` multiplie ensuite chaque composante **RGB565**
de la palette BMP par la composante demandée (0–255 / 255) :
le résultat conserve les nuances locales argent/cuivre mais n'est **pas**
une simple couleur uniforme appliquée à une police blanche. Le BMP SD
reste source de couleur et n'est jamais réécrit par le rendu à la volée. Cette modulation est propre aux chemins de rendu qui
appliquent `setColorMod`, pas à tous les blits natifs indistinctement.

## Source des fichiers et preuve matérielle

Test réel antérieur au correctif des glyphes :
`platformio-device-monitor-261121-110206.log`, SHA-256
`950da58b2f5b04bbb7901e13375e8995a9be23858d9a3b56f5ebbf8625eb095d`.

- `[FONTSD] ACTIVE name=a.bmp source=SD indexed=4 palette=16 bytes=5302` ;
- Help/About atteint, texte de 112 lignes, pagination observée ;
- aucun panic/WDT dans les 328 lignes fournies ;
- `larger_font.bmp` **n'est pas observé comme chargé dans cet extrait**.

Ce log prouve le fonctionnement du **loader SD de la petite police avant
retouche**, pas la nouvelle forme des glyphes, ni le chargement de la grande,
ni les cas de fallback. Les fichiers corrigés exigent une nouvelle inspection
sur le vrai CYD pour déclarer l'aspect validé.

## Installation / fallback

Copier `sdroot/doomrpg/img/a.bmp` et
`sdroot/doomrpg/img/larger_font.bmp` vers les chemins SD de même nom ;
redémarrer le CYD après copie (pas de hot-reload).
Fichier absent/invalide : fallback sur `/DoomRPG-ESP32.pak` natif,
**jamais** sur le ZIP runtime. Le lecteur indexé accepte uniquement BMP
non compressés 1, 4 ou 8 bits ; la palette magenta est la clé transparente.

Le runtime garde un framebuffer logique 160 × 120 et des lecteurs bornés,
sans chargement d'un BMP24 de 63 Ko en RAM. Les thèmes, le code du moteur,
les autres glyphes et le gameplay ne sont pas modifiés par cette retouche.

## Gain de lisibilité de la teinte sur les glyphes SD (moteur)

Dans le chemin Canvas, `fontColor` module chaque composante de la palette
métallique. Pour éviter qu'une couleur de thème assombrisse trop le caractère,
la texture indique dorénavant explicitement si elle a été décodée **depuis
la SD**. Seulement lorsque cette texture est effectivement teintée,
`EspNativeFontTint_apply` relève les canaux RGB565 modulés d'environ 25 %,
avec saturation bornée. Noir, contours et canaux nuls demeurent noirs/nuls,
et la clé de transparence reste inchangée. Une police blanche/non teintée
et les textures PAK conservent exactement leur rendu historique.

La retouche ne modifie **aucun BMP** et ne s'applique **pas** aux autres
surfaces du thème, ni aux blits indexés directs du HUD/dialogue/HUB qui ne
passent pas par `EspNativeTexture_setColorMod`. Les chemins de recoloration
sont volontairement conservés distincts. Effet précis à vérifier sur CYD
avec les différentes couleurs de thème ; pas de validation matérielle
du gain de luminosité encore revendiquée.

### Vérification des nouveaux assets (PR #413)

`python artwork/menu-uac/generic/tools/repair_assets.py --check` refait
l'audit exact de la silhouette des deux boutons génériques et de la
palette de la grande police. La police PAK n'est pas modifiée ; le
rendu spécifique du mode UAC 120 applique la palette canonique même
lorsque le chargement se replie sur le PAK. La coloration générale du
Canvas reste dans son chemin de modulation existant.
