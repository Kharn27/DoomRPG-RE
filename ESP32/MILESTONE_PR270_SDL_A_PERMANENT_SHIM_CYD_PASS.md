# SDL-A — Shim SDL indexé permanent : PASS matériel classic CYD

**Date : 9 novembre 2026.** Issue [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269), PR [#270](https://github.com/Kharn27/DoomRPG-RE/pull/270). **Code et tests exécutés sur matériel : `16c6a00dda69ac6db1de4c585361576d87e18e8d`**, branch `agent/esp32-sdl-shim-permanent`, base originale `main=77649ebf992e57aa5f57ff6efd138b45d871c9c2`.

## Propriété et frontière

Le fichier permanent `ESP32/src/esp32_sdl.cpp` contient **exactement** le résultat effectif des cinq substitutions auparavant effectuées dans `ESP32/scripts/build_engine.py` : structure `SDL_Texture` indexée, adoption zero-copy `SDL_CreateTextureFromSurface`, `SDL_DestroyTexture` propriétaire des pixels/palettes, rejet des updates indexés dans `SDL_UpdateTexture`, décodage de pixels 1/4/8 bpp à l'échantillonnage dans `SDL_RenderCopy`. La comparaison complète entre ancien résultat généré et source permanent donne une **identité du texte des 26 047 caractères**. Ce n'est pas un retrait d'ABI SDL : les fonctions liées restent volontairement présentes.

PlatformIO compile le source permanent directement dans `ESP32/src` ; aucun `env.BuildSources` ou patch Python pour le shim ne subsiste. Le pré-script conserve les gardes desktop et supprime les copies/objets SDL périmés d'un build incrémental. Gameplay, PAK, natif 3D, input, présentateur et états de sauvegarde inchangés par SDL-A.

## Vérification logicielle

CI [GitHub Actions #42533716705](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42533716705) **SUCCESS** pour `16c6a00d…` : test hôte compilant et exécutant la vraie source permanente, corpus 1/4/8 bpp, palettes et conversion RGB565, transparency magenta, modulation, clipping/scaling, transfert Surface→Texture, libérations comptées, OOM et refus des mises à jour indexées. Tests hôte préexistants conservés.

Les trois configurations ont compilé en CI (c'est une preuve logicielle, **seul `esp32-cyd` normal est validé matériellement**). L'audit `compiledb` n'admet que la source permanente ; absence de source/objet généré. Les trois ELF ont exactement **19 fonctions SDL liées / 1 547 octets symboliques**, comme la baseline `main`. Par rapport à cette baseline, RAM statique suivie **0 octet de différence** pour chaque profil, sections chargées **0 octet normal**, **0 octet bringup**, **+4 octets trace (.flash.text)**. Taille des fichiers .bin inchangée ; les fichiers ELF bruts diffèrent de −88 / −88 / −76 octets (normal / bringup / trace). Aucun gain RAM, latence ou fluidité revendiqué.

## Rapport Serial du vrai classic CYD

**Fichier : `device-monitor-261109-001903.log`**, **1 683 lignes**, 190 944 octets sur disque, capturé sur l'environnement **normal `esp32-cyd`**. Boot : ESP32-D0WD-V3 240 MHz, flash 4 Mo, PSRAM 0, `Render=144 Game=0 Canvas=44 Total=948` (preuve que le test couvre la baseline antérieure à #268, et non le nouveau `Render=120`). Le framebuffer unique est `160x120 RGB565, 38400 bytes`, présentation nearest x2 en 320x240. PAK natif de 241 entrées, textures adoptées 4 bpp compactes observées 9 fois.

Parcours observés : OPTIONS→VIDEO (orientation, luminosité, thème, retour), HELP (défilement complet 77 lignes, pages avant/arrière et retour), START→intro→Entrance, portes à 4 frames et fermeture différée, caisses/loot, dialogues 8 et 26 avec reprise opcode19, HUD et mouvement/tours. EXIT via HUB/SYSTEM retourne au menu ; LOAD V11 slot04 Entrance restaure carte/script/lignes/monstres/topologie/positions/activation. Combat Malwrath sprite322 : 3 attaques player (HP ennemi 11→7→3→0), 2 ripostes multi-projectiles et commits dégâts joueur, death/corpse. Deuxième EXIT retourne au menu.

**Contrats :**
- `HEADROOM_OK` **2 fois** ; `largest8=86004` en jeu ; marges 8-bit signalées 51 132 et 50 320 octets.
- `RESIDENTRESET empty=1` **2 fois**, chaque reset libérant 18 008 octets selon les logs ; `SYSEXIT MENU-READY` **2 fois** ; heap8 menu après EXIT = 153 600.
- `shapeData=0x0` et `mediaTexels=0x0` aux étapes de chargement et teardown ; pas de ZIP runtime ou de gros cache de textures introduit par SDL-A.
- Aucune panic, WDT, corruption déclarée ou échec de rendu dans le fichier. Les témoins `[NATIVEFRAME] BOUNDARY-INLINE` apparaissent 6 fois ; ils ne sont pas corrigés ni attribués à SDL-A.

**Verdict : PASS matériel ciblé SDL-A** pour le firmware normal testé, avec retour utilisateur : « Si c'est ok, tu peux documenter puis fusionner et continuer ». Le log ne démontre pas la fidélité pixel-perfect exhaustive, la latence comparative A/B, ni le fonctionnement combiné avec le `main` avancé par #268. Ne pas extrapoler au bringup/trace matériel.

## Clôture et suite

Après ce test, **aucune modification fonctionnelle SDL-A** : commits de clôture documentaires seulement ; l'intégration éventuelle de nouvelles évolutions `main` doit rester identifiée séparément du SHA flashé. Fusion de #270 explicitement autorisée par l'utilisateur. La prochaine étape est SDL-B (code dormant/stubs), **sur une nouvelle branche créée depuis le SHA exact du `main` réellement fusionné** ; pas de SDL-C et pas de retouches fonctionnelles SDL-A dans cette clôture.
