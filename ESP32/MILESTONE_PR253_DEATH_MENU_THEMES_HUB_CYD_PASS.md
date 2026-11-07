# PR #253 — validation classic CYD : menus industriels, thèmes, HUB et RETRY (7 novembre 2026)

## Source / frontière

- PR : https://github.com/Kharn27/DoomRPG-RE/pull/253 ; branche unique `agent/esp32-death-menu-industrial-actions`.
- Code réellement présent au moment du test : `712348dbced422cbf2497e45876f7db8ee6e89f1` (dernier commit fonctionnel). Base `main=67494a4f7b99f04ddd0bbec657ffb685e3c0205e` (#255 déjà fusionnée). Aucun nouveau code fonctionnel ni rebase après ce test.
- Vrai ESP32-2432S028R classic CYD, firmware normal `esp32-cyd`, source de preuve : `device-monitor-261107-010724.log` (2 593 lignes, 323 453 octets), retour utilisateur : « les test.. ca marche niquel on va pouvoir merger !! ».
- CI du SHA testé : [push #42315451726](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42315451726) et [PR #42315456256](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42315456256), SUCCESS. Tests hôte + firmwares normal/bringup/trace selon la PR.

## Observations matérielles positives, strictement bornées

1. **Options/VIDEO/THEME** : version config native V3 (12 octets), démarrage `theme=1`, SELECT confirmé `theme=1->2` puis `2->0` avec `[NATIVECONFIG] SAVE result=committed` et retour automatique à VIDEO. La sélection CLASSIC/PHOSPHORE et les deux confirmations tactiles sont prouvées ; pas de reboot après la nouvelle sélection dans cette capture, donc persistance inter-redémarrage de ces choix non prouvée ici. Ne pas déclarer un essai distinct de LOADING sous PHOSPHORE : le thème est revenu à CLASSIC avant LOAD.
2. **LOAD V11 slot04** : restauration validée d'Entrance, monstres/topologie/positions/activation, HUD et session. Malwrath #322 : projectiles transitoires `204` (3 tirs par riposte), animations et commits `MONSTERRETAL` séquencés ; plusieurs `DAMAGEBORDER` FRONT/RIGHT. Perte de vie réelle jusqu'à `PLAYERDEATH`.
3. **Death menu / RETRY** : `[PLAYERDEATH] READY` après chute/fade, `[DEATHMENU] TAP ... route=RETRY`, `[DEATHMENU] DISPATCH action=RETRY`, `[DEATHROUTE] BEGIN`, `[RESIDENTRESET] ... empty=1`, `[NATIVEBOOT] READY`, `[ENGINESESSION] READY map=1` : reprise neuve de /intro.bsp depuis PAK, `saveWrite=no`. Route RETRY matériellement démontrée ; les routes de mort **LOAD / JUNCTION / MAIN** ne sont pas exercées dans cette capture, même si LOAD normal et transition de niveau le sont.
4. **Gameplay fresh après RETRY** : déplacements, rotations, ouverture et fermeture différée de porte275, dialogue opcode26 + continuation opcode19, pickups et combats fonctionnels.
5. **HUB topbar** : touch `logical=54,10` vers INVENTORY, `101,8` vers STATUS, `139,9` vers SYSTEM ; journal `[HUB] PAGE`, `[HUBSTAT] FRAME`, `[NATIVESAVE] UI page=system rows=SAVE/LOAD/EXIT`. Inventory, notebook, consommation de medkits, sélection et chargement V11 slot06, contrôles du HUB sans dispatch monde (worldDispatch=blocked). Six OPEN, cinq CLOSE dans l'extrait ; une session remplace le HUB lors du chargement.
6. **Fin de niveau** : `[LEVELSTATS] PRESENT` suivi d'un `STATS-ACK`, `[NATIVECHANGEMAP] COMMIT sourceMap=1 targetMap=9`, `[LEVELCOMPLETION] MARK bit=00000001` et `[ENGINESESSION] READY map=9`. Aucun écran externe nécessaire.
7. **RAM/invariants** : `Render_t=184 B`, `Game=0`, `Canvas=44`, 0 PSRAM, PAK 241 entrées, `shapeData=0x0`, `mediaTexels=0x0`. `heap8` session Entrance LOAD ~111 276 B, RETRY fresh ~112 100 B, Junction ~120 900 B ; `largest8=86 004 B` en jeu, `HEADROOM_OK` sur les sessions contrôlées. Aucune `panic`, `WDT` ni reset anormal observés dans la capture.

## Anomalie explicite : restauration HUD exacte non validée

Deux fermetures purement navigatoires du HUB produisent simultanément `[HUB] CLOSE ... exactHud=NO exactBottom=NO` et `[RESIDENTGAMEPLAY] HUB-RECOVER status=NOT_READY` (log lignes ~2065/2066 et ~2196/2197). Le garde exact de `EspNativeGameplayHub_handleAction` retourne effectivement `ESP_NATIVE_GAMEPLAY_HUB_NOT_READY` lorsque `restoredProtected != baselineProtected` ; `EspNativeResidentGameplay` force alors une restauration world après reset HUB. Le jeu demeure jouable après recovery, donc **ce n'est pas un crash**, mais le PASS pixel-exact du HUD n'est **pas** acquis pour ces fermetures. À conserver en dette/régression traçable, sans prétendre que le terminal prouve le bon rendu visuel final à chaque pixel. D'autres CLOSE sont `exactBottom=yes`, et les mutations d'items utilisent légitimement `exactHud=topbar-pending`.

## Conclusion et clôture

**PASS matériel fonctionnel ciblé** de la composition actuelle #253 selon le retour utilisateur, du RETRY de mort, du tactile des thèmes, du HUB déplacé, de LOAD V11 et d'Entrance→Junction ; **réserve HUD connue**, et aucune extension de preuve aux trois routes de mort non exercées, à la persistance PHOSPHORE après reboot, ni à tous les BSP. Le code testé reste gelé. Après cette validation, seuls des commits de documentation sont autorisés ; CI de clôture à contrôler, PR à passer de Draft à Ready for review ; **merge exclusivement par l'utilisateur**, sans mutation de `main` par l'agent.
