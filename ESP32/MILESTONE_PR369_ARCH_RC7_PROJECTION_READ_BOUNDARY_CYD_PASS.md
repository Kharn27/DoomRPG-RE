# ARCH-RC7 — retrait des deux --wrap de lecture ActionEngine : PASS CYD borné

**17 novembre 2026 — [PR #369](https://github.com/Kharn27/DoomRPG-RE/pull/369).** Base exacte du vrai `main` au lancement : `9f005541314e6b69e31a93ad303bba2690a92c90`. Branche `agent/esp32-arch-rc7-projection-read-hooks`. **Code testé selon le scénario utilisateur : `be27cabc03d780075fce68fb9f5e6c0e775aa9cf`**, après cinq commits de code/test/CI ; les commits post-Serial sont **documentation uniquement**. Le Serial ne contient **pas** de SHA Git encodé dans le firmware : l'attribution relève de la branche communiquée et du test utilisateur, non d'une mesure cryptographique embarquée.

## Frontière architecturale

Deux interpositions GNU ld supprimées comme **lot homogène**, après audit de 40 wrappers actifs :
- `-Wl,--wrap=EspNativeGameplayActionEngine_getVisualState`
- `-Wl,--wrap=EspNativeGameplayActionEngine_getEntity`

Les anciens corps du propriétaire `esp_native_gameplay_projection_overlay.c` deviennent des points d'entrée **explicites** `EspNativeGameplayProjectionOverlay_getVisualState/getEntity`, composés directement dans `esp_native_gameplay_monster_combat.c` et `esp_native_gameplay_monster_trace.c`. La vue combat ajoute son overlay liveness/pain, la vue trace ne réinjecte pas le combat (pas de récursion). L'ordre legacy de masquage, caisses, ressources consommées, animation monstre, et les fail-closed sont conservés ; aucun nouvel owner persistant, grosse allocation ou changement du PAK/ZIP, framebuffer, RNG, audio ou présentateur.

**Ne pas retirer dans ce lot** les interpositions `EspMapSpriteTopology_getVisualState/getEntity`, ni `EspMapRuntime_getMapSprite` : elles servent plusieurs consommateurs (topologie, collisions, rendu, entités, save et lecture brute) et demandent un contrat de migration plus large.

## Logiciel et CI sur le code testé

Oracle hôte des **cinq corps C réels** : **17 cas** succès/échec + **13 mutants adversariaux** rejetés ; état de caisse sur deux passes et interruption avant/après chacune, suppression des entités consommées, null/out-of-range, pose attaque. Audit du véritable linker Xtensa avec 4 ELF et témoin de call direct :
- `esp32-cyd` **38/38/38** ; `esp-cyd-inverted` **38/38/38** ; `esp32-cyd-trace` **38/38/38**
- `esp32-cyd-bringup` **41/41/41** (trois wrappers de diagnostic propres au profil)

Deux workflows **SUCCESS** sur le code HEAD `be27cabc...` :
- [push 44002349374](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44002349374)
- [PR 44002356702](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44002356702)

Les trois wrappers de logging de l'outil ESP32 ne sont pas comptés dans ces frontières métier.

## Vrai test utilisateur CYD — PASS RC7 borné

Capture **`platformio-device-monitor-261117-192034.log`** : **2149 lignes, 283740 octets, SHA256 `f027aeb72fb9edd8ba3fa8c9e9424c5dde91e4dcab423f37cc1d67a5e6a8c482`**. Matériel ESP32-D0WD-V3, 240 MHz, flash 4MiB, **0 PSRAM**, 160×120 RGB565 = 38400B, TFT x2. Le Serial annonce `alternate-panel inversion=off` ; cela documente le profil d'écran observé, **pas la chaîne PlatformIO choisie lors du flash**. La parité firmware normal/inverted est démontrée en CI, non sur deux cartes dans ce log.

**Session 1 :** MENU_MAIN→OPTIONS/SOUND→OFF→START→intro complète→Entrance neuf. `MAPFLASH REUSE HIT`, `TRANSITIONLOAD RELEASE ... progress=100`, `ENGINESESSION READY map=1 angle=64 shapeData=0x0 mediaTexels=0x0`. Deux `CRATE COMMIT` (sprites **127** et **56**, transformés en resource tile92) et pickups subséquents de caisse ; trois dialogues fermés et reprises opcode19 (deux en session fraîche, troisième avant reprise de checkpoint selon lignes 604, 792, 883). Portes animées, déplacements/rotations, HUB inventory/system puis EXIT : `RESIDENTRESET heap8=109520->127528 released=18008 ... empty=1`, `SYSEXIT MENU-READY resident=empty`.

**Session 2 :** LOAD V11 **slot04 5604 B** Entrance, avec état monstres/topologie/positions/activation et 4 caisses transformées restaurés. `MAPFLASH REUSE HIT`, `TRANSITIONLOAD RELEASE progress=100`, `ENGINESESSION READY map=1 angle=0 shapeData=0x0 mediaTexels=0x0`. `MONSTERACT ACTIVE sprite=322` à l'ouverture de la porte; projectile Malwrath logique 204, plusieurs `MONSTERATKVIS ARM/COMPLETE`, **4 `MONSTERRETAL COMMIT`** sur le log, **2 `MONSTERCOMBAT COMMIT`** sprite322 : HP 11→7 puis 7→0 ; `MONSTERDROP COMMIT` slot2, pickup, suivi d'une progression ample (mouvement, portes, objets). En fin de scène : `MONSTER3GOAL COMMIT` deux goals additionnels sprite272, `MONSTERRETAL COMMIT` sprite279, puis HUB/EXIT : deuxième `RESIDENTRESET ... released=18008 ... empty=1` et `SYSEXIT MENU-READY resident=empty`.

Éléments transversaux : **2** `ENGINESESSION READY`, **2** `MAPFLASH REUSE HIT`, **2** `TRANSITIONLOAD RELEASE`, **2** `RAMBUDGET RESERVE ... HEADROOM_OK`, **2** `RESIDENTRESET empty=1`, **2** `SYSEXIT MENU-READY`, **19** `PLAYERRES COMMIT`, **3** `DIALOG CLOSE`, **2** `CRATE COMMIT`, **2** `MONSTERCOMBAT COMMIT`, **1** `MONSTERDROP COMMIT`. Heap8 `80448` dans la première session et `81512` au minimum des échantillons de la seconde ; `largest8=61428` en gameplay, `127528/86004` après les deux sorties. Pas de panic/Guru/WDT ni de marqueur `FAILED` détecté dans le Serial.

**Limites à ne pas extrapoler :** SON explicitement désactivé (`AUDIOSWITCH SET ON->OFF` ; `LOADAUDIO-L1 PRESTART ... SOUND_OFF` à chaque session), donc aucune preuve de qualité/mixage audio. Pas de nouvelle carte, de re-stage map-flash MISS, de golden full-frame, de refus PAK, ni de test d'échec owner en conditions réelles. Le Serial ne fournit pas le SHA de son binaire.

## Incident indépendant exclu de RC7

Un autre log `platformio-device-monitor-261117-144846.log` a montré un rollback d'une **chaîne létale de plusieurs tonneaux** après animations. Cause déjà présente sur `main` (limitation explicite `EspNativeGameplayBarrelRadius_terminalDeathOwned(count)==(count==1)`), **aucune modification de cette source sur RC7**. [Issue #371](https://github.com/Kharn27/DoomRPG-RE/issues/371), confiée à un autre développeur. La capture RC7 du présent rapport **ne teste aucun baril**, ne valide ni ne corrige cette issue.

## Sortie de milestone

Après PASS : commits uniquement `ESP32/*.md`, revue que le diff code depuis `be27cabc...` est vide et **double CI du HEAD documentaire** avant fusion #369 demandée explicitement par l'utilisateur. Après merge, relire le vrai `main` et son SHA. Pour RC8 : regrouper des frontières cohérentes par propriétaire / effet / preuve, pas un flash par wrapper ; garder `EspMapRuntime_getMapSprite` et les getters de topologie publics pour un audit dédié si trop transversal.
