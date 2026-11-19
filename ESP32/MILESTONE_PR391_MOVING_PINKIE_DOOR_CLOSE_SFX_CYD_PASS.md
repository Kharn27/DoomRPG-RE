# PR #391 — Pinkie BSP mobile visible et fermeture 5064 après premier frame : PASS CYD borné

**PR :** https://github.com/Kharn27/DoomRPG-RE/pull/391  
**Date :** 19 novembre 2026.  
**Branche agent :** `agent/fix-moving-monster-bsp-leaf-visibility`. **HEAD code testé attribué par contexte :** `78525ce7e5ae2032d054b09a2d7ab8fda678dd8c` (inclut l'intégration de ARCH-RC16 `main=2e8f070cddbd4409c222e9d9c6f4fad4ec7e7193`). Le Serial ne contient pas de SHA binaire ni le nom de l'environnement PlatformIO ; ne pas transformer la corrélation contextuelle en attestation du firmware.

## Défaut et contrat des corrections

Le Pinkie (map sprite 279, subtype 5) franchissait la porte 102 de Entrance et devenait invisible alors que sa position mutée restait attaquable. Le cache natif de feuilles BSP conservait la feuille de spawn ; `EspNativeGameplayMonsterMovementPublish_isProjected` permet au lecteur de visibilité de recalculer la feuille à partir des coordonnées projetées réellement possédées par le runtime, **sans enregistrer la valeur dynamique dans le cache de 2 Ko**. Les sprites immobiles conservent le chemin caché. Cartes de plus de 1024 sprites : lecture et calcul live bornés ; invalides : fail-closed. Aucun `shapeData`, `mediaTexels` ou cache map-wide réintroduit.

Lors d'un MOVE BACK sur la porte régulière 102, la collision logique était déjà fermée, mais `DOORANIM HOLD` préservait l'ouverture visuelle pendant le tour du monstre. Le 5064 publié au commit était donc audible trop tôt. Désormais le commit **stage** le 5064 uniquement dans `reserved[1]` du slot animateur existant de 8 octets, sans nouveau heap ; `RELEASE` suit toujours le tour du monstre ; le premier frame de fermeture correctement présenté consomme le cue une seule fois puis déclenche la pompe audio coopérative à PAK fermé. Le 5063 d'ouverture reste au premier frame réussi et les CLOSE sans HOLD gardent l'ancien commit. Aucune modification de collision, IA, ordre des tours, cadence RNG ni déplacement de monstres.

## Validation automatisée

- [CI push code intégrée #44530795604 — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44530795604).
- [CI pull request code intégrée #44530808665 — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44530808665).
- `ESP32/test/native_bsp_moving_sprite_visibility_host.py` : extraction des vrais corps C et oracle de déplacement multi-feuilles, rollback, sprites statiques, reset, capacité et mutant négatif.
- `ESP32/test/native_door_held_close_sfx_host.c` : vrais propriétaires C animateur/SFX, HOLD/RELEASE, premier frame, unicité même avec double commit, deux fermetures, échec frame, rollback et close direct.
- Les oracles historiques SDL-F24/F28 et l'ancienne fixture audio sont conservés par inverses exacts/stand-ins stricts pour leurs instantanés de source.

## Preuve matérielle — ESP32-2432S028R classic CYD sans PSRAM

**Capture utilisateur :** `platformio-device-monitor-261119-144824.log`, **839 lignes, 123 815 octets, SHA256 `3bdbfd3e96d69e33fe805457b1c129e97ed2ec6c1e5e55c1f3434f491e92290c`**. Boot ESP32-D0WD-V3 240 MHz, 4 Mo flash, PSRAM 0, framebuffer natif 160×120 RGB565 (38 400 B) / nearest ×2. MENU → LOAD V11 slot04 (5604B) Entrance `/intro.bsp` → action sur porte 102 et combat → HUB System EXIT → MENU.

1. **Ouverture saine** : `[DOORANIM] ARM line=102 open=0->1` ligne 431, `[DOORSFX] OPEN-START ... sound=5063 ... first-visible-frame` ligne 440, `[DOORANIM] COMPLETE` ligne 447.
2. **Pinkie en mouvement** : sprite 279 subtype5 `tile=377->345` attaque lors du SELECT_DOOR (ligne 465), puis `345->313` après BACK (ligne 531). La cible reste visible selon l'utilisateur (« Impeccable ») et sélectionnable en tile 313 après fermeture ; trois `MONSTERCOMBAT COMMIT` lignes 592, 670 et 723 : PV `13->9->4->0`, mort et unlink final. Cette capture n'est pas une preuve de toutes les positions/map BSP.
3. **Synchronisation critique de fermeture** : ligne 493 `ARM line=102 open=1->0`, 494 `HOLD ... visual=open`, 500 `MOVEEVENT COMMIT`, 501 `DOORSFX CLOSE-STAGED ... queued=no`, 502 et 509–548 `HELD-FRAME` au cours du tour et de l'attaque du Pinkie. Ligne 553 `RELEASE after-monster-turn`, **ligne 556 premier `RA2B FRAME` de fermeture présenté**, ligne 557 `AUDIOINTENT PUBLISH resource=5064`, ligne 559 `DOORSFX CLOSE-START ... phase=first-visible-frame`, ligne 560 `SFXGEN START id=5064`, ligne 561 `END`, ligne 565 `DOORANIM COMPLETE`, ligne 566 `POST-MONSTER-COMPLETE ... visual=closed`. **Une seule publication 5064** pour ce CLOSE ; aucun 5064 avant RELEASE/présentation. L'utilisateur confirme explicitement « Impeccable tu vas pouvoir fusionner ».
4. **Robustesse** : `RAMBUDGET ... HEADROOM_OK` ligne 287 avec marge heap8 23 252B. Heap8 59 752B puis 59 712B durant la session, largest8 45 044B. `RESIDENTRESET ... released=18008 ... empty=1` ligne 806, `SYSEXIT MENU-READY` ligne 836. `shapeData=0x0`, `mediaTexels=0x0` en entrée session et en sortie. `AUDIOINTENT AUDIT` ligne 813 : `queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 consumed=19 handoffDiscarded=3 pending=0`; STREAM et FINAL `underruns=0 recoveries=0`. Aucun panic, WDT, DOORANIM FAILED, SFXGEN DROP observé.

## Portée du PASS et règle d'intégration

**PASS matériel borné** : un scénario réel de fermeture avec Pinkie en mouvement/attaque, combat jusqu'à la mort, nettoyage et retour au menu. Pas de test CYD d'échec forcé de premier frame/rollback, de batch multi-portes, de plus de 1024 sprites, de toutes les cartes, ni de validation instrumentée acoustique ou d'identification SHA binaire. Ces frontières sont couvertes seulement en host lorsqu'indiqué.

Après ce test : **code gelé ; commits propres uniquement documentation**, puis intégration de l'évolution courante du `main` et double CI finale avant fusion demandée par l'utilisateur. Cette validation ne prouve pas un nouveau run CYD des commits intervenus parallèlement dans `main`.
