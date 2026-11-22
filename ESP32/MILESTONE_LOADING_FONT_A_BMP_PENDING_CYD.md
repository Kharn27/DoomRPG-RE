# LOADING-FONT-P1 — titre « LOADING » en a.bmp + thème natif (candidat CYD)

**Statut : PASS visuel + parcours matériel sur code LOADING, correctif de verbosité post-test non reflashé.** Code préparé sur la branche `agent/esp32-loading-a-bmp-themed-title` depuis le `main` exact `a40d96eceb94844551a3a3fcd8036d0497d79d93` (22 novembre 2026). N'affirmer ni validation physique ni gain de performance avant les logs du classic CYD.

## Portée

Le panneau de chargement de `ESP32/src/esp_native_transition_presentation.c` utilise toujours le fond `c.bmp`, les plaques industrielles et la jauge inchangés. **Seule la ligne « LOADING »** (ancien logiciel 3×5 agrandi) est remplacée par les sept glyphes originaux de `a.bmp`, avec le dégradé du thème courant (`EspMainMenuSkin_letterTone`, `EspUacFontTheme_verticalTone`, traitement monochrome/couleur identique aux boutons UAC). La ligne `ENTERING` et le libellé de carte conservent le rendu mini historique. Aucune modification de la progression, des étapes flash, de l'audio ou des actions gameplay.

La police `a.bmp` est l'atlas ASCII indexé **144×72, 4 bpp, 9×12 par caractère**, index 0 magenta transparent. Sept caractères sont prélevés et mis en cache (7 × 12 × 5 = **420 octets** d'indices compressés + 32 octets de palette + indicateurs ; pas de texels atlas complets, ni allocation heap, ni deuxième framebuffer). La destination est un titre centré de 84×14 px logiques, origine (38,32), le panneau restant (16,26)–(143,69). Couleur appliquée **à chaque redraw** depuis le thème actif ; colorkey magenta et contour noir conservés. La police et le cache sont réinitialisés avec l'état de présentation après fin, abandon ou abort du chargement.

Le **SD override** `/doomrpg/img/a.bmp` est privilégié **avant l'ouverture du PAK physique** lorsqu'il est disponible : lecture des sept glyphes, fermeture de son unique handle SD, puis ouverture du backing PAK. Si le PAK est déjà ouvert, le décodeur existant choisit SD seulement lorsque son propriétaire l'autorise, sinon PAK. Sur absence/dimensions/couleurs invalides/échec de lecture, conserver le mini-texte d'origine plutôt que bloquer le chargement. Aucun ZIP runtime. Un PAK ouvert durant les callbacks de progression ne relit plus les glyphes : il réutilise les seuls 420 octets cachés. Aucun changement `shapeData` / `mediaTexels`.

## Témoins attendus

- À l'entrée de chaque chargement : `[LOADFONT] READY name=a.bmp backing=SD|PAK glyphs=7 inkBytes=420 reads=... bytes=... theme=...`, puis `[TRANSITIONLOAD] BEGIN ... font=a.bmp-themed theme=N ...`.
- En cas d'anomalie : `[LOADFONT] FALLBACK ...`, puis `[TRANSITIONLOAD] BEGIN ... font=mini-hub-fallback` ; la transition continue sans changement de sémantique.
- Le test hôte `ESP32/test/loading_doom_title_host.py` extrait et **compile le vrai corps C** du painter ; il vérifie le rendu de trois thèmes, l'encre noire, la transparence magenta, les limites géométriques, le fallback, ainsi que l'ordre SD→PAK et l'absence de lecture PAK dans le painter. Les autres tests et le build normal `esp32-cyd` sont les portes CI.

## Protocole vrai CYD (profil normal, carte SD inchangée)

```sh
cd ESP32
pio run -e esp32-cyd -t upload
pio device monitor -b 115200
```

Exercer **START → intro → Entrance** ainsi que, si possible, un **LOAD de sauvegarde** (et une transition de carte). Vérifier visuellement la silhouette de `LOADING`, le bon thème CLASSIC/INFERNO/PHOSPHORE, et que `ENTERING`, les noms, la jauge et le fond restent alignés. Relever `LOADFONT`, `TRANSITIONLOAD BEGIN/FRAME/RELEASE`, `RAMBUDGET`, `LOADAUDIO-L0`, et les `AUDIO ... PRESTART` / `STREAM underflows` ; comparer fluidité et RAM au firmware normal de `main` uniquement si deux logs équivalents existent. **Le test visuel et les logs réels sont les critères de PASS.** Ne jamais le supposer par la seule CI.

## Validation réelle CYD du 22 novembre 2026 et amélioration de verbosité

L'utilisateur valide explicitement la nouvelle présentation (« niquel »). Capture normale `platformio-device-monitor-261122-180507.log`, **1 194 lignes** : `[LOADFONT] READY name=a.bmp backing=SD glyphs=7 inkBytes=420 reads=9 bytes=6166 theme=0` puis `[TRANSITIONLOAD] BEGIN targetMap=1 ... font=a.bmp-themed theme=0 ...` ; huit frames de progression, `RELEASE ... progress=100` ; `[RAMBUDGET] RESERVE ... margin8=12228 ... HEADROOM_OK`; musique Entrance `M_STALKS.wav` démarrée avant première présentation du monde, cinq `STREAM` sans `underflows` ni `recoveries`, `[RESIDENTRESET] ... empty=1`, `[SYSEXIT] MENU-READY`. Pas de panic/Backtrace. Seul le thème 0 (CLASSIC) est attesté matériellement dans cette capture ; les autres thèmes restent validés par le test hôte.

La capture révèle **188 occurrences** de `[FONTSD] ACTIVE name=a.bmp ...` sur 1 194 lignes (15,7 %), dues au log émis pour chaque ouverture d'atlas par `EspNativeIndexedBmp_open`. Le changement demandé après cet essai introduit un **masque de 1 octet**, afin de n'annoncer `[FONTSD] ACTIVE` qu'une seule fois par police (`a.bmp`, `larger_font.bmp`) et par boot, aussi bien dans `EspNativeIndexedBmp_open` que dans `EspNativeIndexedBmp_openSdFontStandalone`. Les erreurs `[FONTSD] FALLBACK` restent inchangées ; chemin SD/PAK et comportement visuel inchangés. Un nouveau test C11 extrait et exécute le vrai verrou de log avec 350 ouvertures simulées : exactement 2 annonces, sans changer la logique de décodage. **La baisse effective du nombre de messages du firmware modifié n'a pas été mesurée sur un nouveau flash physique**. Fusion sur accord explicite de l'utilisateur, après CI finale de la variante corrigée.
