# WALL-SD-P1 — wallpapers c/e indexés SD, fallback natif — PENDING CYD

**Branche :** `agent/esp32-wallpaper-sd-overrides`.
**Base GitHub main vérifiée au départ :**
`a40d96eceb94844551a3a3fcd8036d0497d79d93`.

## Objectif et provenance

Les deux PNG ajoutés dans `artwork/wallpaper/` à la PR #432 sont
convertis en `sdroot/doomrpg/img/c.bmp` et `e.bmp`, sans modification
des originaux PAK. Le PAK fourni séparément par le testeur confirme les
entrées d'origine `c.bmp` 192×128 4 bpp / 12 408 octets et
`e.bmp` 128×128 4 bpp / 8 312 octets. L'entrée PAK reste la source
fail-closed et la référence géométrique, jamais un ZIP runtime.

Les deux nouveaux fichiers sont BI_RGB 8 bpp, palette 256 RGB565-aware,
`c.bmp` 25 654 octets et `e.bmp` 17 462 octets. La couleur transparente
magenta `#ff00ff` de la planète est réservée exactement à l'index 0 ;
les 7 527 pixels magenta de la source sont conservés comme tels. Le
`c.png` est opaque. Le choix 8 bpp cherche à protéger les nuances
du nouvel art contre une réduction agressive à 16 couleurs.

## Ownership

Extension ciblée du propriétaire existant `EspNativeFontSd` (un seul
propriétaire C++ des `File` SD) à deux identités supplémentaires
3/`c.bmp`, 4/`e.bmp`, sous `/doomrpg/img/`. Les IDs 1/2 conservent
sans changement les polices a/larger_font.

`EspNativeImage_loadPack` tente d'abord la surcharge SD si le lecteur est
sûr, vérifie le BMP indexé non compressé et sa géométrie, puis adopte sans
copie la palette et les pixels dans le propriétaire compact existant.
`sdFont` reste **0 pour c/e**, afin de ne pas coloriser les images avec le
thème destiné aux polices. `EspNativeIndexedBmp_open` conserve ses
lectures bornées de ranges (scratch 1 KiB), et reconnaît ces deux noms
quand un bail PAK flash-backed rend l'usage SD sûr.

Une mauvaise taille, une erreur I/O, des dimensions incorrectes, un BMP
24/32 bpp, un fichier manquant ou un lecteur SD indisponible entraînent
le repli sur l'entrée originale PAK, **indépendamment par fichier**.
Le propriétaire suspend ses handles avant toute ouverture physique SD du
PAK ou source-probe. Aucun second framebuffer, aucun média texel map-wide,
aucun `shapeData`, aucune allocation image supplémentaire ni ZIP runtime.

## Budget théorique

La représentation native allouée conserve les pixels packés et la
palette. Les nouveaux BMP8 occupent 24 576 + 1 024 octets de texture pour
c, puis 16 384 + 1 024 pour e, contre 12 288 + 64 et 8 192 + 64
respectivement en BMP4 PAK. Différence **+22 400 octets** si les deux
textures cohabitent, hors métadonnées communes et gestionnaire de fichiers.
C'est une prévision arithmétique, **pas** une mesure de RAM CYD.

## Vérification CI attendue

- `ESP32/test/wallpaper_sd_assets_host.py` lit PNG et BMP réels ;
  contrôle les tailles, blobs exacts, BI_RGB, géométrie, clé magenta
  au pixel près, et l'erreur quantifiée dans les contraintes RGB565.
- `ESP32/test/wallpaper_sd_override_host.c` compile les véritables
  décodeurs C PAK/SD : 8 bpp, lecture par ranges, 4 bpp de secours,
  transparence, rejet de mauvaise géométrie et 24 bpp, absence de SD.
- Régressions `font_sd_override_host.c`, `font_sd_assets_host.py` et
  autres oracles existants à conserver ; compilations PlatformIO normales
  et CI push/PR à vérifier sur HEAD final.

## Protocole CYD après CI

1. Installer les **deux** BMP sous `/doomrpg/img/` sur la microSD ; flasher
   `esp32-cyd` (pas bringup).
2. Vérifier `[WALLSD] ACTIVE name=c.bmp` et `name=e.bmp` lors du prologue,
   le défilement de l'étoilé, la transparence exacte de Mars, et la lisibilité.
3. Aller jusqu'à Entrance ; relever `heap8`, `largest8`,
   `HEADROOM_OK`, `shapeData/mediaTexels=NULL`, audio et absence
   de WDT/panic. Les logs Serial du vrai matériel restent la vérité.
4. Enlever uniquement `e.bmp`, redémarrer, vérifier planète du PAK
   et `c.bmp` toujours SD. Inverser pour le cas symétrique si utile.
5. Fournir les logs exacts ; ne documenter **PASS** et ne déclarer
   merge-ready qu'après revue du vrai test.

**Statut actuel : PENDING CYD.** Aucun test matériel ou mesure RAM
n'est inventé ; `main` n'est pas modifié ni fusionné.
