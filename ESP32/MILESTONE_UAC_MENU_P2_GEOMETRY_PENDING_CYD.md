# UAC-MENU-P2 — correction du cadre trop large, retour utilisateur (PENDING CYD)

Base de compilation : **agent/uac-menu-preview**, HEAD `3c6c61ec5a8f311ce39aade3b472db17559a4ce8` (P1 audio SD music pump + repaint partiel conservé). Branche de test : `agent/uac-menu-balance-128` / PR #402, titre historique. `main` GitHub vérifié `b1c6660d55f39623c4e24d874b049cd9b452ad47`. PR #400 toujours ouverte et en draft.

## Défaut constaté
La première tentative P2 rétrécissait les boutons de 110 à 100 **en même temps** que le cadre de 158 à 128. Il subsistait 14 px de marge de chaque côté. Retour utilisateur : « le cadre est encore plus large que les bouton ». Cette réduction conjointe était une erreur de géométrie.

## Correction
- **16 boutons strictement identiques aux BMP du main #399**, 55×20 en 120 et 110×40 en 240 ; aucune compression de typo, pictogramme, alpha, hauteur ou logique de sélection.
- Seuls **les deux BMP de cadres** sont reconstruits : 79×120→**59×120**, 158×240→**118×240**. Les découpes centrales locales `x12..66` (120), `x24..133` (240) sont copiées **octet pour octet** aux nouveaux `x2..56` et `x4..113`. Les bords latéraux d'origine (12 ou 24 px) sont rabattus par échantillonnage horizontal BOX alpha prémultiplié (2 ou 4 px). Les détails du panneau situés au centre ne sont pas déformés.
- Sur écran 320×240 : zone logo **202 px** (au lieu de 162), panneau **118 px**, boutons tactiles **x206..315** avec 4 px de bord de chaque côté. Le logo logique occupe 102 px : largeur à l'écran presque 204 px idéale (écart ~0,98 %).
- Même mode 120 nearest ×2 / mode 240 natif, mêmes 4 actions, même double-tap, y=[20,74,128,182] et hauteur 40. Les zones mortes restent entre les boutons, pas de contact logo.
- Le budget de buffers image est statiquement **6680 B BSS** (outRows 2560 + frameRows 1888 + buttonRows 1760 + pixels 472). Maximum deux BMP ouverts simultanément, pas de PSRAM, aucun changement au framebuffer gameplay 160×120 ni au backing PAK/ZIP.
- `esp_uac_menu_preview.cpp` est basé sur **la dernière P1**, donc `EspEntranceMusic_pumpMenuPreview()`, le repaint partiel des boutons et son instrumentation sont préservés. Le patch de selection est déplacé x206, la découpe du cadre relative devient -4.
- 18 empreintes SHA256 du firmware / manifeste mises en cohérence (16 empreintes d'origine + deux cadres nouveaux). Préflight refuse les anciennes images de cadre. 20 PNG de preuve actualisés dans `artwork/menu-uac/previews`, jamais dans sdroot.

## Retest physique requis
1. Copier les **deux cadres BMP modifiés** de la branche et conserver/restaurer les 16 BMP bouton originaux de #399 dans `/doomrpg/img/` sur la SD ; garder `credits.bmp` et le `/DoomRPG-ESP32.pak`.
2. Compiler/flasher `agent/uac-menu-balance-128`, environnement **esp32-cyd normal**. Photos CLASSIC → 120 ×2 → 240 natif, juger le cadre, logo non aplati, lisibilité de OPTIONS et contours sans découpe.
3. Tester double-tap de START/LOAD/OPTIONS/HELP et retour, zones mortes, conservation du son et absence de FD error.
4. Fournir logs Serial complets : MODE, PRESENT, sdMs/lcdMs/paint, audio, heap8 et éventuels panic/WDT.

**PENDING : ce correctif n'a pas été testé sur le vrai CYD, ne pas fusionner avant preuve hardware.**

## Réconciliation avec P1 final (avant fusion)

La PR #400 a ajouté depuis la base P2 un quatrième choix `240→120` (BMP 240 échantillonné à 120 puis doublé), avec tests de parité d'échantillonnage et optimisation du scanout audio. **Ces fonctions sont conservées** : le mode se déroule maintenant CLASSIC → 120×2 → 240→120×2 → 240 direct → CLASSIC. La géométrie 202/118 et les boutons 110px sont identiques dans les trois aperçus.

Le code P1 actuel a volontairement `kStrictSha256=false` pour accepter des BMP d'artiste conformes au format V4 ; le préflight vérifie toujours les en-têtes/masques/dimensions/tailles et les 18 empreintes compilées correspondent aux livrables. Ne pas revendiquer de contrôle SHA actif à l'exécution. Le rendu `240→120`, le moteur audio et les tests host de décimation P1 sont maintenus par cette réconciliation. Retour utilisateur « Pour moi c'est ok ;) tu peux fusionner :) » : **acceptation visuelle**, non preuve Serial d'une nouvelle build fusionnée. La validation matérielle technique reste bornée aux retours/photos réellement fournis.
