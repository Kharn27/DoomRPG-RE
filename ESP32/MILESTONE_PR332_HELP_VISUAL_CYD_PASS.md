# PR #332 — HELP CYD : gameplay, HUD et sélecteur d'armes — PASS visuel

**Date du retour :** 13 novembre 2026.  
**PR :** [#332](https://github.com/Kharn27/DoomRPG-RE/pull/332).  
**Branche :** `agent/esp32-help-gameplay-turn-weapon-align`.  
**Base initiale :** `3637922e7ca83147642d2db7c1a0c375b88515ac`.  
**Candidat firmware testé et approuvé :** `e376d109ebfd1a1f5c488633d8a4b360abf1f995` (environnement normal `esp32-cyd`).  
**Intégration de `main` avant fusion :** `0bd4d69e43f23e5d1279f9bba3a555fb6b05dfca`, commit à deux parents `51b3fca33b47165ad8ed0876196489ebb56de5ef`.

## Retour réel sur le classic CYD

Le testeur a affiné la page HELP en plusieurs passes sur son appareil : d'abord le centrage WPN, puis les chevrons TURN, ensuite BOTTOM HUD BAR et enfin WEAPON SELECT. Après les trois dernières corrections et la CI verte, son verdict final est **« ok tu peux fusionner ;) »** : **PASS visuel des pages HELP couvertes**, autorisation explicite de fusion.

**Périmètre de cette preuve :** visuel et navigation de l'aide tels que rapportés par l'utilisateur. Aucun nouveau fichier de monitor Serial n'accompagne ce verdict : **ne pas attribuer à cette PR** de mesure heap8, marge RAM, allocation/consommation flash mesurée sur le CYD, stabilité longue durée ou nouveau test de gameplay.

## Production : trois pages HELP, sans modifier les opérations du jeu

### MAIN GAMEPLAY (et rangées de commandes)
- Symbole TURN LEFT/RIGHT composé d'une boucle et d'une pointe : boucles à leur décalage extérieur de 2 px ; pointes décalées **indépendamment de 3 px** vers l'extérieur pour ménager l'espace visuel réclamé, sans déplacer les hitboxes.
- Labels `WPN PREV` et `WPN NEXT` : deux rangées 5×7 centrées verticalement en **y78..92** dans la case y74..96, avec pad sombre qui suit les glyphes. L'utilisateur a confirmé « Les bouton WPN ok » avant les ultimes retouches.

### BOTTOM HUD BAR
- Miniature du HUD natif provenant des vrais `k/m/l/o/a.bmp`. Dans la **capture HELP uniquement**, retrait des traits d'angle orange hérités et de leur surimpression ; le HUD gameplay réel conserve les angles/touchers existants.
- Zone munitions x96..143 de la miniature 128×16 : **48×16 pixels entiers respirent** du gris original vers un bleu discret grâce au mélange RGB565 déjà utilisé par l'aide. Chaque trame repart du cache source immuable, sans accumulation du bleu et sans lecture PAK par animation.
- Sous-titre « STATUS AT A GLANCE » supprimé, aperçu relevé de **y43 à y31**. Quatre lignes placées en y52/64/76/88 : `LEFT: HP + ARMOR`, `CENTER: FACE / AMMO`, `RIGHT: DIRECTION / AMMO`, `Touch for weapon menu`. Les deux lignes longues ont des origines x dédiées pour rester à l'intérieur du framebuffer logique 160×120 et au-dessus du pied de page y100. Modèle HELP toujours **112 entrées**, pas de décalage des pages.

### WEAPON SELECT
- **12/12 armes** : les sprites sont toujours tirés des vrais bitshapes, mappings, texels et palettes PAK via `EspNativeGameplayHubWeaponGrid_paint` et un **petit joueur de démo local** avec masque `weapons=0x0fff`. L'aperçu déjà conservé en mémoire (144×60 RGB565, **17 280 octets**) reçoit un éclaircissement cyan **en place** sur les icônes, sans changer les bordures et fonds noirs. Aucun déverrouillage/modification d'un vrai joueur.
- Bouton d'arme en haut à droite agrandi de **36×12 à 46×15** (x108..153, y12..26) ; ce sont les pixels issus de la vraie zone munitions, avec le même *breath* gris→bleu sur **toute** la surface que BOTTOM HUD BAR, sans coin orange. Rythme piloté par le service de repaint HELP déjà existant, sans boucle FPS dédiée.

## Tests / invariants

- `ESP32/test/native_main_help_host.py` : catalogue, mise en page, bornes, positions WPN/TURN et nouveau bouton.
- `ESP32/test/native_help_snapshot_host.c` : compilation du vrai painter MAIN GAMEPLAY, limites et séparation des pixels respirants.
- `ESP32/test/native_bottom_help_breath_host.py` : vrai painter de la miniature et transition RGB565 0/128/255, **768 pixels** animés, **1280 voisins** préservés, retour original sans cumul.
- `ESP32/test/native_help_weapon_preview_host.py` : vraies fonctions C du cache 12/12, préservation des fonds/bordures, bouton 46×15 testé aux puissances 0/128/255 et source immuable.
- Les oracles historiques SDL-F17/F19 conservent leurs SHA Git originaux en reconstituant exactement les portions de HELP ajoutées depuis leurs milestones : `native_help_bottom_history.py`. Test Canvas : ABI et empreinte du moteur texte inchangées.
- **CI du candidat approuvé :** [push #43333644487](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43333644487) et [PR #43333652444](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43333652444), **SUCCESS** (hôtes + trois compilations PlatformIO).
- Après merge de `main` dans la branche, **les 4 blobs production HELP sont bit-à-bit identiques** au candidat approuvé :
  - `esp_native_gameplay_hud.c` : `afce15bbc36305af03941bed0c76b5607f2bb48b`
  - `esp_native_help_scene.c` : `7b874c630d17a0068520697db61fa30a511c804f`
  - `native_main_menu_actions.c` : `d931afd1343b3e5196e5d6b3ee78ef957a494e9c`
  - `native_main_menu_model.c` : `ce502276130ac28ce9a2dc85d9bf8d6e52482a31`
- Intégration du `main` SDL-F25/F26 sur **un commit de merge explicite**, seule la CI YAML étant combinée (tests SDL du nouveau main + tests HELP de la PR). Le dernier commit post-PASS ajoute **uniquement documentation**. Aucun retour aux assets ZIP runtime, ni `shapeData`/`mediaTexels` map-wide, aucune alloc mémoire ajoutée par les corrections, aucun toucher du gameplay réel.

**Décision :** PASS visuel, fusion explicitement demandée ; après CI verte sur la tête documentée, merge autorisé dans `main`.
