# PR #323 — MENU_MAIN sans cadre collectif et LOAD GAME rééquilibré — PASS visuel CYD

**Date :** 12 novembre 2026.  
**PR :** [#323](https://github.com/Kharn27/DoomRPG-RE/pull/323).  
**Branche :** `agent/esp32-main-menu-unframe-button-rail`.  
**Base initiale exacte `main` :** `9bd225d5564085ac44730761e80fd838e561cb2c`.  
**Commit firmware candidat soumis à la validation visuelle :** `280f6e477e44a85aa7eea6fe8543d616f07a2548`.

## Changements de production

### MENU_MAIN
- `ESP32/src/native_main_menu_touch_layout.c` : les quatre boutons industriels START / LOAD / OPTIONS / HELP ne sont plus entourés du cadre collectif gris (rectangle acier et rainure gauche).
- Chaque bouton conserve sa plaque métallique propre, son texte, sa palette, les mêmes rectangles 54×24 et hitboxes. Logo de gauche, double-tap, repaint de la seule colonne droite et absence d'allocations inchangés.
- Les écrans enfants OPTIONS/VIDEO/BRIGHTNESS/THEME/ORIENTATION conservent leurs rails, décorations et zones tactiles.

### LOAD GAME depuis MENU_MAIN exclusivement
- `ESP32/src/esp_native_gameplay_save.cpp` : titre compact en haut (zone y2..18, lettrage y6..14, ligne d'accent y19), les cinq emplacements sont à y24,38,52,66,80, hauteur 13px et pitch 14px, soit 8px remontés.
- PAGE UP/DOWN à y26..54 et y61..90 ; leurs hitboxes correspondent à ces nouvelles positions, avec numéro de page mini à y99.
- Bouton BACK devient 78×22 pixels, rectangle et hitbox identiques x34..111 / y97..118, séparés des emplacements (dernier emplacement termine y92). Retour vers le menu inchangé.
- SAVE/LOAD du HUB en gameplay, format V11, lectures SD, joueurs/cartes, caches, et framebuffer 160×120 inchangés.

## Couverture automatique

- `ESP32/test/main_menu_skin_integration_host.py` : le cadre extérieur n'appartient plus au MENU_MAIN mais demeure dans les sous-menus.
- `ESP32/test/main_load_selector_layout_host.py` : compilation C++ hôte des vraies fonctions de dessin et de touch du sélecteur, vérification géométrie, pagination, armement/confirmation, zones mortes et BACK ; gardes de non-régression pour le HUB.
- `ESP32/test/native_modal_theme_host.py` actualisé uniquement pour la nouvelle géométrie de la sélection LOAD principale ; palettes et autres surfaces conservées.
- Ces tests font partie de la CI PlatformIO. Les deux runs du **candidat `280f6e477e44a85aa7eea6fe8543d616f07a2548`** sont **SUCCESS** : [PR run #43126525497](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43126525497) et [push run #43126516017](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43126516017). Les trois profils sont compilés par CI ; ne pas prétendre une compilation locale.

## Retour matériel réel et limites

Sur le classic CYD, après test des améliorations proposées sur la branche, l'utilisateur confirme **« Impeccable tu peux fusionner :) »**. Ce retour est un **PASS visuel ciblé** de MENU_MAIN et LOAD GAME, suffisant pour intégrer cette frontière UI bornée. Il **n'accompagne pas un nouveau log Serial dans ce message** : aucune mesure de RAM, absence d'erreur série sur cette version, ni test de gameplay prolongé n'est revendiqué. Par inspection, aucun allocateur, cache supplémentaire ou second framebuffer n'a été introduit.

**Décision :** conserver exactement le firmware `280f6e477e44a85aa7eea6fe8543d616f07a2548` après PASS ; seuls des commits de documentation sont acceptables. Ne fusionner qu'après vérification de CI finale et de l'état réel de `main`.
