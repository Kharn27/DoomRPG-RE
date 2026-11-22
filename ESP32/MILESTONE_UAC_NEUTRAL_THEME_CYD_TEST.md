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
