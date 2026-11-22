# UAC-120 + FONT-NEUTRAL — intégration isolée pour test CYD (PENDING)

- Main vérifié : `1588b6983cc5724f7028ac0d2c18f96688aba990`.
- Source FONT-NEUTRAL PR #426 : `06c6151b2658d545057ed2adfce8c20f7d4b791a`.
- Source UAC PR #413 : `f5bc57788fb63f9c61a70b21e275896043704693`.
- Branche test : `agent/esp32-uac-neutral-theme-cyd-test`. Ne remplace ni la PR #413 ni la PR #426.
- Les changements UAC matériels/états/tactiles sont repris sur la base récente, mais **pas** l'ancien remplacement coloré de `sdroot/doomrpg/img/larger_font.bmp` ni ses générateurs de preuve. Les polices historiques restent identiques à FONT-NEUTRAL-P1.
- Le preview UAC applique maintenant les **vrais indices de l'atlas** et détecte les palettes **neutres SD** : palettes gris→CLASSIC (jaune), INFERNO (rose), PHOSPHORE (vert). Une police colorée ou la police PAK maintient les teintes métalliques historiques. Remapping de 16 entrées seulement au changement de thème et peinture FULL si le thème change, zéro framebuffer supplémentaire.
- Conservé : 2975 B cache temporaire de glyphes, 32720 B BMP menu-only, décodeur par petits blocs, invariants gameplay, paramètres 160×120, RAM et fichiers PAK.

## MicroSD

Dans `/doomrpg/img/` : conserver `menu_frame_120.bmp` ; **mettre le bouton corrigé `menu_generic_120.bmp` de cette branche** ; placer le **petit neutre** sous le nom `a.bmp` et le **grand neutre 208×102/4bpp** sous le nom `larger_font.bmp` (les variantes `*_neutral.bmp` ne sont pas chargées automatiquement). Garder `/DoomRPG-ESP32.pak`. Redémarrer après copie. Les BMP non utilisés `menu_generic_240.bmp` peuvent rester.

## Protocole matériel

Flasher le firmware NORMAL `esp32-cyd` de cette branche. Vérifier :
1. Menu UAC 120 au boot, aucun retour CLASSIC inattendu ; `[UAC] DEFAULT READY`, `[UAC] FONT120 READY ... backing=SD palette=neutral-theme`.
2. START/LOAD/OPTIONS/HELP : centrage, contour, glyphes, touches ; entrée/sortie HELP et OPTIONS.
3. OPTIONS→VIDEO→THEME : CLASSIC→INFERNO→PHOSPHORE→CLASSIC. Vérifier **couleur des lettres** sur le menu UAC, pas seulement le thème du gameplay ; chaque transition doit repeindre intégralement les textes, sans artefact.
4. BRIGHTNESS / VOLUME : seul le choix du pourcentage est single-tap ; la navigation reste double-tap.
5. START / LOAD / EXIT, logs `[RAMBUDGET]`, `[UAC]`, `[FONTSD]`, musique/sound, absence panic/WDT ; `shapeData/mediaTexels=NULL`.

**CI et tests hôte ne remplacent pas le PASS CYD**. Envoyer le log Serial réel et une photo par thème si possible.

## Retour CYD du 22 novembre 2026 — FAIL VISUEL, correctif candidat à re-tester

Log `platformio-device-monitor-261122-115811.log`, **522 lignes** : 6 annonces `[FONTSD] ACTIVE name=larger_font.bmp source=SD indexed=4 palette=16 bytes=10726` et 6 `[UAC] FONT120 READY ... backing=SD palette=neutral-theme` prouvent **le vrai 13×17 depuis SD** (25 glyphes utiles en cache 2975 B), non un fallback PAK. 33 `[UAC] PRESENT ... result=OK`, 4 `[UAC] DEFAULT READY`. Une transition de thème validée : `[OPTIONSTHEME] SELECT 0->1` (CLASSIC → INFERNO) avec sauvegarde config ; aucune transition PHOSPHORE attestée. Un FINAL musique MENU `underruns=0 recoveries=0`. Aucune ligne panic/WDT/FAIL-HEAP dans cette capture ; aucune preuve de gameplay ni test RAMBUDGET dans ce parcours. Utilisateur : **artefacts blancs sur arêtes/coins** et **absence de dégradé de thème**, rejet esthétique explicite.

### Causes techniques

1. **Bouton non identifié sur SD** : l'ancien `main` livre `menu_generic_120.bmp` SHA256 `6a188ff11105257f15ab50c1b0671675f6377cbe3fedbb1ea3efa6942268edda`, avec alpha 255 jusque dans les quatre coins et des pixels clairs. Le BMP corrigé de la PR #413 a le SHA256 `1aa91c6aa6a1ac515be55c9552ee84f22bea550a1c6e0951b3eef23a525b2274` ; son alpha/perimeter est identique au véritable bouton START, et ses quatre coins alpha 0. L'ancien préflight UAC vérifiait les dimensions mais **pas** le contenu (`kStrictSha256=false`). Le log ne donne pas le SHA du fichier physique : ancien bouton **plausible, non démontré**.
2. **Dégradé vraiment aplati dans le code** : la première intégration utilisait `EspMainMenuSkin_uiInk()` (une **unique** couleur par thème) pour coloriser les 16 nuances. L'information de gradient haut/bas de `EspMainMenuSkin_letterTone` était perdue.

### Correctif de la même branche test (PENDING re-test)

- Le `menu_generic_120.bmp` en SD est accepté **seulement si son SHA256 complet** égale `1aa91c6a...` ; hash du header 122 B et des pixels BGRA déjà en cache, pas de second gros chargement ni de mémoire persistante. Diagnostic `[UAC] GENERIC120 integrity=MATCH/REJECT expectedSHA256=1aa91c6aa6a1ac51`. En cas d'ancien asset/corruption : fallback CLASSIC au lieu d'afficher des coins blancs ; aucun bricolage silencieux des pixels.
- Pour la police **neutre SD uniquement** : interpolation RGB565 **haut sombre → milieu coloré → bas clair** entre les trois tons officiels CLASSIC/INFERNO/PHOSPHORE, sur **19/21/23 lignes physiques**, puis application des luminances originales de la planche et de l'atténuation de sélection. Les contours noirs restent noirs, la clé transparente n'est jamais peinte. Pas de LUT globale ni d'atlas complet ; 32 B palette source et 32 B palette temporaire sur stack par ligne, taille du cache 2975 B inchangée. Source colorée historique et fallback PAK conservent leurs couleurs.
- Un changement de thème provoque déjà un FULL UAC ; aucun changement de logique tactile, SD music, framebuffer ou gameplay.
- CI : oracle RGB565 réel (256 gris, 3 thèmes, 19/21/23 lignes), oracle source gradient, pixels alpha/START exacts + SHA, builds normal et inverted. Aucune validation visuelle revendiquée avant nouveau log/photos.

**Prochaine SD** : remplacer obligatoirement `/doomrpg/img/menu_generic_120.bmp` par **celui de la branche test**, SHA256 `1aa91c6a...`. Les deux polices neutres déjà en SD peuvent rester, si `[UAC] FONT120 READY backing=SD palette=neutral-theme` continue d'apparaître. Flasher `esp32-cyd` HEAD corrigé et contrôler `[UAC] GENERIC120 integrity=MATCH`, les quatre coins, les trois dégradés, puis les performances/heap. Si `REJECT` / fallback CLASSIC : le BMP sur SD n'est pas la version corrigée.
