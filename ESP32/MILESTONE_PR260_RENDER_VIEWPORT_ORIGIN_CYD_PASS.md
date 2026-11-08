# PR #260 — retrait de l'origine viewport desktop : PASS fonctionnel CYD ciblé

**Date :** 8 novembre 2026. **PR :** https://github.com/Kharn27/DoomRPG-RE/pull/260. **Branche :** `agent/esp32-render-retire-desktop-screen-origin`. **Base exacte :** `ab27bb23d3881bf95e354bef03df55089350f0f7`. **Code observé/testé dans le contexte de la PR :** `995ff6aead71dbf9fd441494e37fbe1c9c591675`. Le fichier Serial ne porte pas lui-même de SHA firmware embarqué : attribution par la session de test et l'ABI `Render_t=176`, pas par une signature binaire indépendante.

## Frontière et régression de code

Retrait de `Render_t::screenX` et `screenY` sur les profils ESP32 normal/bringup/trace. Ces champs restent présents pour le desktop et l'oracle hôte `DOOMRPG_ESP32_GEOMETRY_ORACLE`. Le `SDL_Rect` de setup conserve l'origine pour le message `[RENDERCORE] SETUP`, et les consommateurs natifs BSP, première frame, sprite, gameplay, arme et COLORSTATS/BMP lisent les constantes `DOOMRPG_VIEWPORT_X/Y` de `platform_video_config.h`. Aucune autre allocation, duplication de framebuffer, décompression ZIP runtime, modification gameplay ou RNG introduite.

ABI souhaitée **184 → 176 octets** (gain de 8 octets **par structure**, pas un gain mesuré de heap), `sizeof(Render_t)==176U`, FNV test de setup `14bba767` ; tests hôtes 4096 cycles, parité pair/impair, réentrance, trois OOM et gardes de présence de lecteurs natifs. CI du commit code [#42429005622](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42429005622) : **SUCCESS** (tests hôte, `esp32-cyd` normal, `esp32-cyd-bringup`, `esp32-cyd-trace`). Les échecs intermédiaires ayant révélé des consommateurs oubliés ont été corrigés sur la même PR avant cette CI.

## Preuve Serial sur classic CYD

**Capture utilisateur :** `device-monitor-261108-012736.log` ; 1 447 lignes, 171 529 octets. La capture débute par cinq `[ALIVE]` puis contient un `POWERON_RESET` et un boot complet ; le log ne permet pas d'attribuer la cause du redémarrage initial. ESP32-D0WD-V3 à 240 MHz, flash 4 MB, **PSRAM=0**, PAK natif 241 entrées et framebuffer logique **160×120 RGB565 / 38 400 B**.

- `[RENDERCORE] INIT ... bytes=176` ; `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=1280B` ; `Engine structs: Render=176 Game=0 Canvas=44 Total=980 bytes`. Le rendu natif initialise le framebuffer et les palettes sans échec.
- **START neuf** → intro → Entrance : `[MAPRT] READY arenaBytes=14095 arenaFNV=c3882516` ; `[ENGINESESSION] FIRST_FRAME ... frame=71ca7465` (même FNV de première frame que le témoin PR #255) ; mouvements, rotations, strafe, porte 275 et fermeture différée, pickups, dialogues 8/26 et reprises `EV_NEXTSTATE`, HUB puis EXIT sans écriture de sauvegarde.
- **LOAD V11 slot04** → Entrance : restauration d'état spatial et des monstres, `[ENGINESESSION] RESUME-VISIBLE`, arme suivante, porte 113, Malwrath sprite 322 : deux `[MONSTERRETAL] COMMIT` non létaux, deux `[PLAYERPAINFX] FIRST-PRESENT` suivis de deux expirations, 18 frames `[MALWRATHPROJECTILE] FRAME`, trois `[MONSTERCOMBAT] COMMIT` (dernier létal, cadavre), puis déplacement, porte et second HUB/EXIT.
- **Deux** `[ENGINESESSION] READY`, **deux** `[TRANSITIONLOAD] RELEASE`, **deux** `[RESIDENTRESET] ... empty=1` et **deux** `[SYSEXIT] MENU-READY frame=13074b1a`. `shapeData=0x0` et `mediaTexels=0x0` aux deux entrées gameplay et au teardown.
- **RAM :** session neuve `[ALIVE] heap8=109776 largest8=86004` ; session rechargée `heap8=109732 largest8=86004` stable sur plusieurs messages. Deux `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK` avec `margin8=53244` et `52428`. Retour menu après le second EXIT : `heap8=155712 largest8=110580`.
- Aucune ligne `Guru Meditation`, panic, WDT, `[... ] FAILED` ou erreur fatale de gameplay dans cette capture. Les sept `[NATIVEFRAME] BOUNDARY-INLINE` et les nombreux `[NATIVESPRITE] CROSS` sont des diagnostics présents hors de ce changement ; aucun lien de régression établi.

## Décision et limites

**PASS matériel fonctionnel ciblé de l'ABI et des parcours rendus observés**, corroboré par la CI complète. **Pas de PASS visuel pixel-perfect** : pas de capture TFT ou de comparaison A/B de tous les pixels/framerate avec le firmware précédent. Les 8 octets sont une économie de structure, pas une mesure de diminution de heap libre. Pas de validation de tous les BSP, orientation INVERSE, sauvegarde nouvelle, audio audible, familiar/gib, ni fermeture `[HUB] CLOSE` (deux HUB quittés par EXIT). Le premier reset `POWERON_RESET` n'est pas une preuve de watchdog. La preuve de log n'embarque pas le SHA firmware.

**Geler le code `995ff6aead71dbf9fd441494e37fbe1c9c591675`** ; post-test, seulement ce commit de documentation. Vérifier la CI documentaire puis sortir de Draft pour laisser à l'utilisateur le merge éventuel. Ne jamais merger automatiquement dans `main`.
