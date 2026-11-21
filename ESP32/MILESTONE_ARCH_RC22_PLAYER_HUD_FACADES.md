# ARCH-RC22 — PlayerView MOVE et HUD sans interposition GNU

Base GitHub vérifiée: main 0ecf61366140f9ef3c279e33111055ea0f6dd8c8 (cinq commits après RC21, seulement assets, logs et documentation).
Branche: agent/esp32-arch-rc22-player-view-hud.
Statut: **PASS matériel CYD borné du code RC22 + double CI code SUCCESS**, mais exigences de fusion : CI documentaire finale et contrôle du vrai main.

## Frontières retirées, ownership et rollback

- EspPlayerView_commitPreparedMove: l'ancien corps raw de esp_player_view_state.c est renommé EspPlayerView_commitPreparedMoveBase. Le public reste défini dans esp_native_gameplay_player_resources.c et appelle cette feuille sans réentrance. Uniquement après retour OK, il arme la petite lease pendingBefore/pendingAfter; le commit inverse exact efface la lease. INVALID, NOT_READY, UNSETTLED, UNSUPPORTED ou STALE n'ont aucun effet. Le dispatcher garde EXIT, collision, publication de position, ENTER puis rollback; les pickups sont servis après publication, éventuellement différés par dialogue.
- EspNativeGameplayHud_view: le corps raw retenu de esp_native_gameplay_hud.c devient EspNativeGameplayHud_viewBase. La façade publique de PlayerResources retourne la vue brute si indisponible, sinon projette dans le même hudOverlay BSS existant les champs HP/maxHP, armure/maxArmure, arme, weaponsPresent, ammoType et quantité courants depuis PlayerState. Le HUD retained n'est pas modifié. Le pointeur overlay a une durée de vie transitoire; la chaîne repaintLive/snapshotLive des modales conserve ses propres gardes.
- Production: 4 vers 2 flags -Wl,--wrap. Bringup: 7 vers 5. Aucun alias __real_ ou __wrap_ pour ces deux symboles, leurs API publiques sont conservées et les feuilles Base sont explicitement déclarées.

## Les deux interpositions restantes

- EspNativeGameplayFrame_renderTurn: façade dans esp_native_dynamic_line_render.c, feuille dans esp_native_gameplay_frame.c. La portée englobe adapteurs de lignes et textures, previews sans commit, pose HOLD de fermeture après MOVE, prepare/finishFrame de portes, MoveEvents_onFrameResult et SFX seulement après la première image effectivement publiée. Une API publique composant la feuille renderTurnBase sera une étape distincte.
- Esp32PlatformVideo_present: façade dans esp_native_gameplay_present_gate.c et décorateur dans esp_native_gameplay_action_engine.c, feuille physique dans platform_video_c_bridge.cpp vers PlatformVideo_present(). La gate protège les suppressions one-shot, TRANSITION, WAIT_STATS et LEVELUP, puis ActionEngine compose top-bar/feedback/tint/pickup-mini et ancre les effets au présent physique. Le futur retrait devra préserver les bypass physiques explicites des modales et effets pain, sans optimiser la SPI.

Trois diagnostics supplémentaires bringup inchangés : EspNativeFirstFrame_route, EspNativeGameplayInteractionInventory_log et EspNativeResidentGameplay_service.

## Protections de code

L'oracle ESP32/test/native_player_resource_move_hud_boundary_host.py extrait et compile 12 vrais corps C (helpers PlayerView CAS, HUD raw, les deux façades PlayerResources et clearPending). Seuls stockage de fixture et dépendances externes PlayerState/Weapon sont simulés. Il teste les refus sans effets, le CAS strict, la lease, le rollback exact, les mouvements successifs, les projections HUD et la non-mutation retained. Quinze mutants adversariaux doivent compiler et échouer. Les tests historiques MOVE/RC19/RC20/RC21 conservent leurs garde-fous; l'inventaire ELF vérifie 2+3 et interdit la résurrection des aliases. Double CI du code HEAD 1eca14a5603297e8d4ebd959f383566ea94fa5ab SUCCESS : push 44892241255 et PR 44892246962 ; audit ELF 2/2/2/5 et appels directs identiques pour les trois release.

Aucun changement PAK, ZIP, BSP, V11, frame 160x120 RGB565 ni allocations. shapeData/mediaTexels restent requis NULL; aucune PSRAM.

## Protocole matériel après double CI

Firmware normal esp32-cyd sur classic CYD. NEW GAME/INTRO/Entrance, déplacements, rotations et strafes, pickups/caisses, HUD de ressources et armes, portes avec HOLD/RELEASE, monstre activation/déplacement/attaque/décès/drop, menus HUB/System EXIT, LOAD V11 et second EXIT.

Dans Serial: PLAYERRES COMMIT, MOVE/MOVEEVENT COMMIT, DOORANIM COMPLETE, HEADROOM_OK, RESIDENTRESET empty=1, SYSEXIT MENU-READY, shapeData=0x0, mediaTexels=0x0, aucun panic/WDT ni audio overflow/drop/underrun. Les cas non exercés sont explicitement hors preuve. Aucun PASS matériel avant lecture du vrai log. Après PASS, commits docs-only, double CI finale, merge-ready mais aucun merge non autorisé.


## Rapport matériel exact (21 novembre 2026) — PASS CYD borné

- Source : `platformio-device-monitor-261121-111456.log`, 3864 lignes, 502174 octets, SHA256 `3fafdd66310f3f58628431d57c951b96896a7dcdd1431a8928bd1af8f3a34190`. Contexte de test utilisateur sur la branche RC22; aucun SHA firmware ni environnement de compilation imprimé dans le Serial.
- HELP/About + BACK ; NEW GAME / INTRO / Entrance : 107 `VIEWANIM END` (89 move, 18 rotate), 30 `PLAYERRES COMMIT` (armor 8, health 4, ammo 4, inventory 2, weapon 2, credits 10), `PLAYERRES REDRAW-SATISFIED` et un redraw IDLE à la demande. État HUD vivant correct attesté lors des `GAMEPLAYHUD REPAINT`; aucun saut double de position, previews `gameplayStable=yes` et `logicalCommit=once`.
- Portes : 16 `DOORANIM COMPLETE` ; huit `HOLD`, huit `RELEASE`, huit `POST-MONSTER-COMPLETE` ; 5063 ouverture et 5064 après premier frame visible de fermeture. 15 dialogues clos. Aucune faute animation.
- Monstres : 8 `MONSTERCOMBAT COMMIT` dont 5 décès/drops, 7 `MONSTERRETAL COMMIT`, 2 `MISS-COMMIT`, 12 `MONSTER3GOAL COMMIT` du sprite272 (relink topology=committed). Pas de preuve directe dans cette trace d'un Pinkie traversant une porte; ne pas présenter ces 12 déplacements comme une telle preuve.
- HUB armes : `HUB CLOSE ... weapon=1->0 exact=yes ... hudBottom exactBottom=yes`, deux `GAMEPLAYHUD REPAINT` à la fermeture, mutation weapon-only sans tour. EXIT `RESIDENTRESET empty=1` + `SYSEXIT MENU-READY`, puis LOAD V11 slot04 5604B avec script/lignes/topologie/positions/drops restaurés, reprise gameplay et second EXIT propre.
- RAM : 2 `HEADROOM_OK`, `margin8=17292/16476`, sans PSRAM, framebuffer 38400 B, `shapeData/mediaTexels=0x0`. Audio : cinq `AUDIOINTENT AUDIT` à zéro overflow, consumerCursorGap et dropped, cinq `ENTRANCEMUSIC FINAL` à zéro underrun/recovery, zéro gap >500 ms, aucun panic/WDT.
- Limites : pas de SAVE neuve, CHANGEMAP, panel inversé, WAIT_STATS/LEVELUP, rollback MOVE fautif injecté ni comparaison acoustique instrumentée. La preuve est celle du matériel et du scénario, pas une couverture exhaustive des chemins.
- **Anomalie préexistante hors RC22, issue [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371)** : deux tentatives de tonneaux en chaîne `chain=3` seq72/73 affichent wave1 puis `PLAYER-DEFER` létal, `ATTACK-CANCEL`, `ROLLBACK rng/player/world=yes`. La défense est fail-closed mais l'animation de l'explosion est déjà visible. Nouvelle reproduction rattachée à #371, aucun nouveau ticket et aucune modification de gameplay dans RC22.

**Validation : PASS CYD strictement borné. Après capture : uniquement documentation. Fusion uniquement sur ordre utilisateur explicite, après double CI documentaire.**
