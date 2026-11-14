# PR #336 — CLASSIC / INFERNO / PHOSPHORE : palette native RGB565, PASS visuel

**Date de validation :** 14 novembre 2026.  
**PR :** [#336](https://github.com/Kharn27/DoomRPG-RE/pull/336).  
**Branche :** `agent/esp32-classic-palette-red-copper`.  
**Base de départ vérifiée :** `17e46f3f2f8bdfe40207c37560d8ba7ff73ac135`.  
**Dernier commit code + tests, avant documentation :** `71f3c2fddf5a06b47482b5241a297c51681a45d2`.  
**Main observé avant fusion :** `e1fd6faabd715ce1b64eea54ecfdf99d64724a8c` (intègre SDL-F28 #337).  

## Retour utilisateur et frontière de preuve

Le testeur a ajusté plusieurs fois les tons CLASSIC et INFERNO, puis a demandé les trois étages PHOSPHORE. Après livraison de la dernière version PHOSPHORE et CI verte, son retour final : **« T uvas pouvoir fusionner :) »**. Ce verdict vaut accord **visuel et autorisation de fusion** pour les trois thèmes dans le périmètre de la PR.

Aucun nouveau log Serial ou relevé de mémoire n'a été fourni pour cette PR : ne pas attribuer à cet accord un nouveau PASS Serial, une mesure heap/flash ni une stabilité matérielle longue durée. Le workflow ESP32 CYD Build de la dernière candidate `71f3c2f` est **SUCCESS, 163/163 étapes** (GitHub Actions run `43596365897`). Les builds de firmware et tests hôte passent ; ce sont des preuves CI, non un substitut aux observations CYD.

## Couleurs RGB565 validées visuellement

| Thème | Haut (5/7/9 glyph rows) | Milieu | Bas |
|---|---|---|---|
| CLASSIC | `0xff34` jaune plus pâle | `0xf5aa` bronze plus jaune | `0xd225` cuivre rouge vif |
| INFERNO | `0xfe78` rouge pâle | `0xe30c` vrai rouge intermédiaire | `0xc002` rouge Doom historique inchangé |
| PHOSPHORE | `0xc7b8` vert pâle | `0x87e0` vert électrique historique inchangé | `0x2638` turquoise |

Les échelons de dégradé des glyphes (5, 7, 9 rangées) et les `EspMainMenuSkin_uiInk()` partagent la tonalité haute des thèmes. Les jauges LOADING de six rangées CLASSIC et PHOSPHORE ont été harmonisées aux teintes demandées ; les six rangées rouges du LOADING INFERNO restent strictement inchangées.

## Contrats et couverture

Production : `ESP32/src/esp_main_menu_skin.c` et commentaires API `ESP32/include/esp_main_menu_skin.h`. Tests : `ESP32/test/main_menu_skin_host.c`, `ESP32/test/main_menu_skin_integration_host.py`, `ESP32/test/test_death_menu.c`.

Aucune modification de glyph mask, géométrie, hitboxes, textures, framebuffer, moteur de jeu, entrée, ownership, PAK ni allocations. Les accents métalliques, contours et couleurs sémantiques gameplay ne sont pas remappés. `EspMainMenuSkin_letterTone()` reste fail-closed sur les tailles/indices hors contrat. Les tests vérifient les couleurs exactes, les états et les bounds, notamment le milieu INFERNO strictement entre ses deux extrémités sur chaque canal RGB565, et le vert électrique PHOSPHORE conservé.

**Intégration :** la branche a été ouverte depuis un `main` antérieur à #335/#337 ; vérifier l'intégration et la CI sur le vrai `main` post-fusion sans assimiler cette vérification au test visuel du firmware combiné.
