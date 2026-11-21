# Polices Doom alternatives — ESP32 classic CYD

Deux planches **4 bits, indexées, prêtes à copier sans renommage** dans
`/doomrpg/img/` sur la carte SD. Le jeu garde ses polices PAK d'origine
en fallback si le BMP SD est absent ou incompatible.

| Fichier runtime (dans `sdroot/doomrpg/img/`) | Taille | Cellule | Stockage |
| --- | --- | --- | --- |
| `a.bmp` | 144 × 72 | 9 × 12, 16 × 6 glyphes | **5 302 o**, BMP 4 bits, palette 16 couleurs |
| `larger_font.bmp` | 208 × 102 | 13 × 17, 16 × 6 glyphes | **10 726 o**, BMP 4 bits, palette 16 couleurs |

Les doublons `a_doom.bmp` et `larger_font_doom.bmp` possèdent
exactement les **mêmes octets** que leurs noms runtime respectifs. La variante
`larger_font_doom_compat3.bmp` (3 couleurs, 10 676 o) reste disponible en
référence. **`larger_font_doom_24bit.bmp` (63 702 o) a été retirée du
répertoire SD** : le chargeur natif n'accepte pas les BMP RGB24. Son ancienne
version reste accessible dans l'historique Git de la PR #407.

## Correction ciblée de `a` et `o` minuscules

Le placement original avait produit un `o` minuscule coupé sur la petite
planche et des `a`/`o` déformés à différentes tailles. Les quatre glyphes
ont été reconstruits depuis les originaux métalliques, projetés sur les
**silhouettes des polices Doom RPG de référence**, puis quantifiés avec les
**palettes 16 couleurs existantes**. La silhouette évite toute modification
de l'avancement et de la découpe des cellules ; le dégradé argent/cuivre
reste conservé.

**Contrat d'absence de régression sur les assets** : les **94 autres
caractères de chaque BMP sont strictement identiques pixel par pixel**.
Entêtes, ordre des caractères, palette, transparence magenta, tailles,
stride et encodage ne changent pas. Le test
`ESP32/test/font_sd_assets_host.py` vérifie ces empreintes, les glyphes
réparés, les doublons et l'absence du BMP RGB24.

## Couleurs à la volée

Le Canvas natif `EspNativeCanvas_drawText` appelle
`EspNativeTexture_setColorMod` avec `doomCanvas->fontColor`.
`EspNativeTexture_blit` multiplie ensuite chaque composante **RGB565**
de la palette BMP par la composante demandée (0–255 / 255) :
le résultat conserve les nuances locales argent/cuivre mais n'est **pas**
une simple couleur uniforme appliquée à une police blanche. Le BMP SD
demeure inchangé. Cette modulation est propre aux chemins de rendu qui
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
