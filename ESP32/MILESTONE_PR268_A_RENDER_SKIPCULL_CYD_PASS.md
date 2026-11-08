# PR #268-A — retrait de Render_t::skipCull : PASS matériel classic CYD

**Date : 8 novembre 2026.** Dépôt `Kharn27/DoomRPG-RE`, [PR #268](https://github.com/Kharn27/DoomRPG-RE/pull/268), branche `agent/esp32-render-retire-skipcull-state` basée sur `main=77649ebf992e57aa5f57ff6efd138b45d871c9c2` (#266 fusionnée). **SHA de code/test hardware : `b985b94b4f0a8311c86469f13173e4008e059af9`**. Log utilisateur réel : `device-monitor-261108-224813.log`, 1 373 lignes (163 542 octets sur disque). Retour : « si c'est ok on continue sur cette branche » ; validation ciblée du jalon A, autorisation de poursuivre sur **la même branche**, **aucune autorisation de merge**.

## Responsabilité retirée

Le toggle `r_skipCull` est une variable du menu développeur desktop (`src/Menu.c`) qui court-circuite le culling de bounds dans `src/Render.c`. La construction ESP32 `EspNativeEngineRoot_initRender` écrivait une seule fois `skipCull=0` ; les deux producteurs de `EspNativeRenderCullView` (première frame et visibilité BSP) lisaient cet état jamais modifié. Le culling standard est maintenant explicite et immuable (`ESP_NATIVE_BSP_CULL_STANDARD=0`) aux deux frontières natives. Aucun nouvel état mutable/heap/copie, géométrie de cull identique. `Render_t::skipCull` absent des firmwares normal/bringup/trace, conservé dans desktop/oracle pour le toggle historique et les tests différentiels avec valeurs 0 et 1.

**Taille** : `Render_t` ESP32 **144 → 140 octets**, mesuré par `Engine structs: Render=140 Game=0 Canvas=44 Total=944 bytes` et `[RENDERCORE] INIT owner=esp-native-bridge bytes=140`. Ce gain est la taille du struct, pas une preuve du gain net heap.

## CI et gardes avant flash

- [Push #42523265828](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42523265828) **SUCCESS 100/100**.
- [PR #42523269645](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42523269645) **SUCCESS 100/100**.
- Builds `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`; `render_geometry_host.py` différentiel 10 000 cas × configurations ; nouveau `native_render_skipcull_retirement_host.py` : champ inaccessible dans les 3 profils, présent desktop/oracle, références natives éteintes. Les corrections des harnais et assertions ont toutes précédé le flash.

## Résultat sur le vrai ESP32-D0WD-V3

- Boot classic CYD dual-core 240 MHz, flash 4 Mo, **PSRAM 0** ; PAK 241 entrées, framebuffer **160×120 RGB565 38 400 octets** affiché en ×2 ; `[RENDERCORE] SETUP view=160x80@0,20 arrays=1280B`.
- NEW GAME `/intro.bsp` Entrance (CRC `623f34e4`, arène compacte 14 095 B FNV `c3882516`, première frame `71ca7465`) : mouvements/tours/strafes, transformation de caisse, pickups avec flash, dialogues événement88/op8 puis événement79/op26, reprise opcode19, porte 275 ouverture/fermeture différée après tour, HUD et HUB.
- EXIT #1 : `[RESIDENTRESET] ... released=18008 ... empty=1` et `[SYSEXIT] MENU-READY frame=1945fafa session=off resident=empty saveWrite=no`.
- LOAD V11 slot04 Entrance (5604 B), restauration ressources/scripts/portes/caisses/automap/monstres/topologie/positions/activation/drops : `[ENGINESESSION] RESUME-VISIBLE`, `READY shapeData=0x0 mediaTexels=0x0`. Porte 113, CROSS sprite324, Malwrath sprite322 : **trois probes d'attaque, chacune à trois projectiles 204**, effets de douleur/impact sprite184 et trois `MALWRATHPROJECTILE CLEAR`. Dernière attaque fatale du joueur : `[MONSTERCOMBAT] DEATH-SETTLE sprite=322 visual=4->corpse2`. Secret event15 : lignes 112/111 ouvertes, `[SECRET] FOUND ... xp=5`. EXIT #2 propre, HUB et menu exacts.
- **2× `[RAMBUDGET] ... advisory=HEADROOM_OK`**, `largest8=86004` en session ; **2× `[RESIDENTRESET] ... empty=1`**, **2× `[SYSEXIT] MENU-READY`**, `shapeData=0x0 mediaTexels=0x0` à toutes les frontières instrumentées. Aucune trace panic/Guru/WDT/abort/backtrace, ni erreur renderer dans la capture.

## Portée du PASS et continuation

**PASS matériel ciblé de A** : le firmware normal exécute les routes ci-dessus sans régression constatée dans les logs ; pas de preuve d'une parité pixel-perfect, d'une amélioration A/B de vitesse, d'un gain net heap de 4 B ni d'endurance multi-heures. **Le code A reste figé exactement au SHA testé.** Ce rapport et les deux entrées d'index sont un commit **documentation-only post-test**, avant tout futur jalon B sur **la même PR #268**. Le B aura son SHA, ses CI et son nouveau log de validation séparés. **Pas de merge main sans instruction explicite.**
