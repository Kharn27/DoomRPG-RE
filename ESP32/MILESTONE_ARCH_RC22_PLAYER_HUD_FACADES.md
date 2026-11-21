# ARCH-RC22 — PlayerView MOVE et HUD sans interposition GNU

Base GitHub vérifiée: main 0ecf61366140f9ef3c279e33111055ea0f6dd8c8 (cinq commits après RC21, seulement assets, logs et documentation).
Branche: agent/esp32-arch-rc22-player-view-hud.
Statut: NON VALIDÉ SUR CYD. L'oracle hôte et la CI doivent être vérifiés avant le flash.

## Frontières retirées, ownership et rollback

- EspPlayerView_commitPreparedMove: l'ancien corps raw de esp_player_view_state.c est renommé EspPlayerView_commitPreparedMoveBase. Le public reste défini dans esp_native_gameplay_player_resources.c et appelle cette feuille sans réentrance. Uniquement après retour OK, il arme la petite lease pendingBefore/pendingAfter; le commit inverse exact efface la lease. INVALID, NOT_READY, UNSETTLED, UNSUPPORTED ou STALE n'ont aucun effet. Le dispatcher garde EXIT, collision, publication de position, ENTER puis rollback; les pickups sont servis après publication, éventuellement différés par dialogue.
- EspNativeGameplayHud_view: le corps raw retenu de esp_native_gameplay_hud.c devient EspNativeGameplayHud_viewBase. La façade publique de PlayerResources retourne la vue brute si indisponible, sinon projette dans le même hudOverlay BSS existant les champs HP/maxHP, armure/maxArmure, arme, weaponsPresent, ammoType et quantité courants depuis PlayerState. Le HUD retained n'est pas modifié. Le pointeur overlay a une durée de vie transitoire; la chaîne repaintLive/snapshotLive des modales conserve ses propres gardes.
- Production: 4 vers 2 flags -Wl,--wrap. Bringup: 7 vers 5. Aucun alias __real_ ou __wrap_ pour ces deux symboles, leurs API publiques sont conservées et les feuilles Base sont explicitement déclarées.

## Les deux interpositions restantes

- EspNativeGameplayFrame_renderTurn: façade dans esp_native_dynamic_line_render.c, feuille dans esp_native_gameplay_frame.c. La portée englobe adapteurs de lignes et textures, previews sans commit, pose HOLD de fermeture après MOVE, prepare/finishFrame de portes, MoveEvents_onFrameResult et SFX seulement après la première image effectivement publiée. Une API publique composant la feuille renderTurnBase sera une étape distincte.
- Esp32PlatformVideo_present: façade dans esp_native_gameplay_present_gate.c et décorateur dans esp_native_gameplay_action_engine.c, feuille physique dans platform_video_c_bridge.cpp vers PlatformVideo_present(). La gate protège les suppressions one-shot, TRANSITION, WAIT_STATS et LEVELUP, puis ActionEngine compose top-bar/feedback/tint/pickup-mini et ancre les effets au présent physique. Le futur retrait devra préserver les bypass physiques explicites des modales et effets pain, sans optimiser la SPI.

Trois diagnostics supplémentaires bringup inchangés : EspNativeFirstFrame_route, EspNativeGameplayInteractionInventory_log et EspNativeResidentGameplay_service.

## Protections de code

L'oracle ESP32/test/native_player_resource_move_hud_boundary_host.py extrait et compile 12 vrais corps C (helpers PlayerView CAS, HUD raw, les deux façades PlayerResources et clearPending). Seuls stockage de fixture et dépendances externes PlayerState/Weapon sont simulés. Il teste les refus sans effets, le CAS strict, la lease, le rollback exact, les mouvements successifs, les projections HUD et la non-mutation retained. Quinze mutants adversariaux doivent compiler et échouer. Les tests historiques MOVE/RC19/RC20/RC21 conservent leurs garde-fous; l'inventaire ELF vérifie 2+3 et interdit la résurrection des aliases. Toutes les preuves CI restent en attente des Actions réelles.

Aucun changement PAK, ZIP, BSP, V11, frame 160x120 RGB565 ni allocations. shapeData/mediaTexels restent requis NULL; aucune PSRAM.

## Protocole matériel après double CI

Firmware normal esp32-cyd sur classic CYD. NEW GAME/INTRO/Entrance, déplacements, rotations et strafes, pickups/caisses, HUD de ressources et armes, portes avec HOLD/RELEASE, monstre activation/déplacement/attaque/décès/drop, menus HUB/System EXIT, LOAD V11 et second EXIT.

Dans Serial: PLAYERRES COMMIT, MOVE/MOVEEVENT COMMIT, DOORANIM COMPLETE, HEADROOM_OK, RESIDENTRESET empty=1, SYSEXIT MENU-READY, shapeData=0x0, mediaTexels=0x0, aucun panic/WDT ni audio overflow/drop/underrun. Les cas non exercés sont explicitement hors preuve. Aucun PASS matériel avant lecture du vrai log. Après PASS, commits docs-only, double CI finale, merge-ready mais aucun merge non autorisé.
