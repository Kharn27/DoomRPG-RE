# PR #322 — introduction Mars : étoiles vers la droite, grille fixe — PASS visuel CYD

**Date du retour utilisateur :** 12 novembre 2026 (heure France).  
**PR :** [#322](https://github.com/Kharn27/DoomRPG-RE/pull/322).  
**Branche :** `agent/esp32-intro-scene3-scroll-direction`.  
**Base `main` exacte au départ :** `e432ffa7ff09d8332a286896dcb4a138a62aa024`.  
**Code firmware gelé testé visuellement :** `c6a099d1ec51f6962be26a334b2a2b88b05f05ad`.

## Problème, première correction et retour matériel

Le prologue comporte deux écrans narratifs initiaux, une animation d'insertion du vaisseau sur Mars et un écran narratif final. Dans `ESP32/src/native_story_fit.c`, le fond `c.bmp` de la scène animée (`storyPage=1`) se déplaçait à gauche, contrairement au défilement vers la droite des écrans narratifs (`storyPage=0/2`). La première itération a inversé le déplacement du fond et de la couche `d.bmp`, faisant **bouger la grille avec les étoiles**. Le retour sur CYD a identifié cette régression visuelle.

## Correction finale gelée

- `c.bmp` : défilement droit de `elapsedAnim/457` avec répétition bornée d'images (pas de nouveau framebuffer ni mémoire), empêchant de découvrir le bord gauche.
- `d.bmp` : **grille immobile** à `left,top` ; elle ne suit plus les étoiles.
- `e.bmp` : planète fixe, inchangée.
- `f.bmp` : vaisseau gardant `shipX=left+elapsedAnim/142` et `shipY=centerY+22+elapsedAnim/-333` ; traits de visée et cadence identiques.
- Les écrans narratifs gardent le défilement initial `/157` ; la scène Mars a volontairement une vitesse du fond plus lente (`/457`). L'utilisateur l'a observée, la juge cohérente avec l'insertion et **a approuvé le résultat sans demander d'ajuster la vitesse**.
- Transitions, input, durée de scène, `PlatformVideo`, assets PAK, jeu et budgets mémoire inchangés.

Le test hôte `ESP32/test/native_intro_scroll_direction_host.py` compile le vrai helper C de défilement droit avec couverture du viewport et garde statiquement la position de la grille et le mouvement du vaisseau. L'oracle historique `sdl_f7_native_visual_no_sdl_host.py` inverse explicitement ces modifications avant de vérifier le fingerprint initial ; la restitution intégrale du `native_story_fit.c` de `main` a été comparée byte-à-byte pendant la préparation de cette PR. Le workflow `.github/workflows/esp32-cyd.yml` exécute le test ajouté.

## Preuves matérielles et limites

**Retour utilisateur réel sur classic CYD après le correctif final :** « C'est good pour moi, le fond défile toute fois un peu moins vite que sur les autres scene mais je pense que c'est calé avec la vitesse de l'icone de vaisseau qui se deplace ». Cela valide **visuellement** le sens du fond et le rendu de la grille après retouche ; aucune mesure de vitesse instrumentée n'est revendiquée.

Le **Serial communiqué lors de la première itération** documente le parcours complet Menu→Start→Intro→Entrance mais n'est **pas** un log recueilli spécifiquement après le dernier commit `c6a099d1`. Il confirme, pour cette première itération, `heap8=100040` octets et `largest8=63476` constants pendant l'animation, 190 frames rendues sur un clock de 50 ms, `INTRODISP recovered=34116` octets, `shapeData=mediaTexels=NULL`, chargement de `/intro.bsp` et `HEADROOM_OK margin8=28092` après prise en charge gameplay. Ces valeurs ne sont pas extrapolées à un nouveau journal non fourni. Le code final ajoute **zéro allocation**, sans mesure de RAM finale indépendante.

**Décision : PASS visuel matériel ciblé de la scène 3**, sur confirmation explicite de l'utilisateur. Ne changer aucun code firmware après ce PASS : documentation uniquement, CI finale à vérifier avant fusion. Ne pas revendiquer de PASS Serial post-correction finale non observé.
