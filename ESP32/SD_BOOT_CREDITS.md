# Crédits de démarrage — classic CYD

Le firmware essaie de lire **`/doomrpg/img/credits.bmp`** sur la carte
microSD au démarrage. Copier le fichier de ce dépôt :

`sdroot/doomrpg/img/credits.bmp`

vers le dossier `doomrpg/img/` **à la racine** de la carte SD
(sans dossier `sdroot/` sur la carte).

Le fichier fourni fait **128 × 640 pixels**, BMP **RGB 24 bits sans
compression**, en-tête Windows V5 (DIB 124 octets), **245 898 octets**.
SHA-256 : `d1fe400eb1d7200da604fc1315a43ce1fb6fbc63493768de6b4f730719912c43`.

Il contient cinq panneaux de 128 × 128 pixels empilés de haut en bas.
Chaque panneau s'affiche **3 secondes**, avec possibilité de passer par
un toucher, comme avant.

La carte SD reste facultative pour ces crédits : en l'absence du fichier,
avec un BMP invalide, ou en cas de lecture SD interrompue, le firmware
retrace automatiquement les **4 panneaux originaux de `g.bmp` (PAK)**,
sans cinquième écran. L’ancien écran « ESP32 CYD PORT » reste supprimé. Le PAK reste intact ; aucune
allocation image complète ni décodeur PNG n'est ajouté au CYD sans PSRAM.

Test matériel de référence : `esp32-cyd`, attendre les cinq panneaux
sans toucher, puis refaire le démarrage après renommage de
`credits.bmp` pour vérifier le fallback. Les logs `[BOOTLEGALS]`
permettent de distinguer `source=sd-bmp24` et le fallback `g.bmp`.

Le PNG de travail ne fait plus partie de la branche : le dépôt contient
uniquement le BMP final fourni par l'utilisateur. La validation sur vrai CYD
a été effectuée par l’utilisateur avant la fusion demandée. La mise à jour
sur le dernier `main` respecte la suppression récente du cinquième écran
par défaut.
