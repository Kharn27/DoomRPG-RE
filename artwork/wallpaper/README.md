# Wallpapers de l'introduction — source PNG, BMP CYD à mémoire bornée

Les PNG de référence restent `artwork/wallpaper/c.png` (ciel étoilé,
192×128) et `e.png` (illustrations, 128×128).

- `sdroot/doomrpg/img/c.bmp` — **BMP4 12 406 B, SD actif** :
  à copier sous `/doomrpg/img/c.bmp`.
- `sdroot/doomrpg/img/e.bmp` — **BMP8 17 462 B, version corrigée conservée** :
  12 022 pixels transparents à l'index 0 / `#FF00FF`, sans confettis
  rose approchant. **Le firmware de ce comparatif utilise expressément
  le `e.bmp` du PAK**, même si ce BMP corrigé existe sur la SD.

La source originale `e.bmp` du PAK (128×128, 4 bpp) n'est pas
modifiée. Aucun besoin de retirer ni de remplacer le fichier
`/doomrpg/img/e.bmp` de la microSD : l'override est désactivé par
la sélection de source dans ce candidat. Il pourra être réactivé
lors d'un essai distinct sans perdre la correction de transparence.

Le ciel `c.bmp`, quantifié à 15 tons utiles + 1 réserve et 12 288 B
de pixels, est lu sur SD dès le **premier** cadre LOADING, avant l'ouverture
du PAK physique. Son propriétaire SD est libéré après le blit borné ;
le chargeur fixe la source entre le premier écran et les checkpoints.
En cas d'absence/erreur, le PAK reste le repli.

Windows BMP BI_RGB non compressé, palette BGR0, lignes bas-vers-haut.
Ne pas exporter en RGB24/RGBA32, changer les dimensions ou les noms :
les chargeurs natifs de la CYD imposent ces contrats.
Le PAK original n'est pas modifié et reste le fallback indépendant.

**Pourquoi ce compromis :** le premier test réel
`platformio-device-monitor-261122-175956.log` trouvait bien les
musiques mais refusait leur démarrage (`HEAP-POST-REJECT`).
La représentation `c4/e8` du second test reprend 13 248 octets résidents
dans le prologue par rapport à `c8/e8` ; la surcharge simultanée
ne représente plus que **9 152 octets théoriques** par rapport à
`c4/e4` du PAK. Le cache range gameplay L1 est resserré (19→15 KiB,
288→256 entrées) pour retrouver une marge audio sans abaisser le
garde 46 KiB post-ring. Le second essai CYD `platformio-device-monitor-261122-182021.log`
confirme **l'audio récupéré** : intro post-ring heap8=59 032 B,
Entrance post-ring heap8=49 300 B, `PLAY` des trois musiques et
`HEADROOM_OK` en gameplay. Le menu a des redraws partiels à 17–18 ms
(sdMs=0), sans preuve que le seul temps de rendu explique le ressenti.

La CI vérifie les blobs précis, le BMP8 corrigé conservé, le routage
PAK-only pour `e.bmp` et les vrais décodeurs. Seul le firmware
`esp32-cyd` doit être reflashé pour comparer l'illustration PAK ;
**aucun changement du `e.bmp` de la SD n'est requis**. Vérifier le son,
les visuels, la RAM et le gameplay. Aucun ZIP runtime, framebuffer supplémentaire ou
média texel map-wide.
