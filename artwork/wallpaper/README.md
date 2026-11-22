# Wallpapers de l'introduction — source PNG, BMP CYD à mémoire bornée

Les PNG de référence restent `artwork/wallpaper/c.png` (ciel étoilé
192×128) et `e.png` (illustrations 128×128). Les deux surcharges sont
à copier **ensemble** sur la microSD, dans `/doomrpg/img/` :

- `sdroot/doomrpg/img/c.bmp` — BMP4 indexé (16 couleurs, 12 406 octets)
- `sdroot/doomrpg/img/e.bmp` — BMP8 indexé (256 entrées, 17 462 octets)

Après le premier essai CYD, `c.bmp` a été quantifié à 15 tons utiles
+ 1 réserve afin de conserver le dessin du ciel dans **12 288 octets
de pixels** au lieu de 24 576. `e.bmp` garde ses détails en 8 bits.
Sur demande du testeur, `e.bmp` est **restauré à sa première version SD**
(blob Git `16fe8cd8ddff2b18e040d69134bfb355fe2bb32b`) :
7 527 pixels ont la clé rose exacte, et les nuances proches restent
visibles, volontairement pour une comparaison A/B. `c.bmp` reste BMP4.
Le chargeur sélectionne maintenant le `c.bmp` SD *avant* d'ouvrir le
PAK physique, puis verrouille cette source durant le chargement,
sans nouveau framebuffer ni lecteur SD simultané. Une source invalide
conserve le PAK d'origine en fallback.

Windows BMP BI_RGB non compressé, palette BGR0, lignes bas-vers-haut.
Ne pas exporter en RGB24/RGBA32, changer les dimensions ou les noms :
les chargeurs natifs de la CYD imposent ces contrats.
Le PAK original n'est pas modifié et reste le fallback indépendant.

**Pourquoi ce compromis :** le premier test réel
`platformio-device-monitor-261122-175956.log` trouvait bien les
musiques mais refusait leur démarrage (`HEAP-POST-REJECT`).
La nouvelle représentation `c4/e8` reprend 13 248 octets résidents
dans le prologue par rapport à `c8/e8` ; la surcharge simultanée
ne représente plus que **9 152 octets théoriques** par rapport à
`c4/e4` du PAK. Le cache range gameplay L1 est resserré (19→15 KiB,
288→256 entrées) pour retrouver une marge audio sans abaisser le
garde 46 KiB post-ring. Le second essai CYD `platformio-device-monitor-261122-182021.log`
confirme **l'audio récupéré** : intro post-ring heap8=59 032 B,
Entrance post-ring heap8=49 300 B, `PLAY` des trois musiques et
`HEADROOM_OK` en gameplay. Le menu a des redraws partiels à 17–18 ms
(sdMs=0), sans preuve que le seul temps de rendu explique le ressenti.

La CI vérifie les blobs précis, le format BMP, la clé rose originale et
les tests hôtes des vrais décodeurs. Le firmware normal `esp32-cyd`
devra être reflashé et le e.bmp original recopié ; vérifier son, image,
mémoire et gameplay. Aucun ZIP runtime, framebuffer supplémentaire ou
média texel map-wide.
