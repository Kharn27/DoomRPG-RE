# Wallpapers de l'introduction — sources PNG et BMP CYD

Sources humaines : `c.png` (192×128, décor étoilé), `e.png` (128×128, Mars).
Les fichiers compatibles avec l'importeur natif sont :

- `sdroot/doomrpg/img/c.bmp` → copier sur la SD sous `/doomrpg/img/c.bmp`
- `sdroot/doomrpg/img/e.bmp` → copier sur la SD sous `/doomrpg/img/e.bmp`

Le BMP exporté est **Windows BMP indexé 8 bpp, BI_RGB (non compressé)**,
palette BGRA 256 entrées, orientation bas-vers-haut, ligne alignée sur 4 octets.
Il conserve les dimensions originales et une palette adaptative RGB565;
le nom `e.bmp` réserve l'index 0 au magenta `#FF00FF` (clé transparente).
`c.bmp` est entièrement opaque.

Taille des fichiers : 25 654 et 17 462 octets, respectivement.
La CI vérifie les blobs Git exacts, le format, les pixels-clés et l'écart
colorimétrique après quantification RGB565. Les PNG restent les masters.

**Source de repli** : les `c.bmp` et `e.bmp` 4 bpp immuables du
`/DoomRPG-ESP32.pak`. Aucun nouveau ZIP ni repack du PAK au runtime.
Si la SD manque, si un fichier est invalide, si sa géométrie diffère
ou si un autre accès exclusif SD détient le lecteur, le PAK demeure prioritaire
en sécurité. Ne pas renommer les fichiers, changer leurs dimensions ou
exporter en BMP 24/32 bpp : le décodeur natif les refuserait.

**Mémoire** : passage PAK 4 bpp → SD 8 bpp : +12 288 octets de pixels
pour `c.bmp` et +8 192 pour `e.bmp`, plus 960 octets de palette par
image. Si les deux textures sont présentes ensemble, delta théorique des
pixels/palettes **+22 400 octets** (non mesuré sur CYD). Pas de double
framebuffer ni de copie BMP complète, et libération habituelle des textures
à la fin des séquences.

**Test matériel requis avant fusion** : firmware normal `esp32-cyd`,
copier les deux BMP sur microSD, lancer l'introduction, vérifier le défilement
de `c.bmp`, la planète `e.bmp`, sa transparence, la fluidité, le son, les
logs `[WALLSD] ACTIVE`, la marge heap8 et le retour gameplay. Retirer ensuite
un BMP pour confirmer le fallback PAK individuel. Pas de PASS matériel
revendiqué ici.
