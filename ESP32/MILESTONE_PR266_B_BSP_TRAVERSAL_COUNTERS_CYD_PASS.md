# PR #266-B — retrait des compteurs de parcours BSP de Render_t : PASS matériel CYD

**8 novembre 2026.** Repo `Kharn27/DoomRPG-RE`, PR [#266](https://github.com/Kharn27/DoomRPG-RE/pull/266), **même branche** `agent/esp32-render-retire-frac-half-width` et `main=bb3c03fde1bcafd83656f40893de142c094e3416`. **Code B testé et gelé : `9899d2af07429dd00923cc73a3cf6ce604a29002`**. Capture réelle `device-monitor-261108-040005.log`, **1568 lignes / 187372 octets**. L'utilisateur a demandé « Si c'est good on conitnu sur cette branche », en soumettant cette capture : acceptation conditionnée au verdict matériel, sans demande de merge.

## Frontière B

- **`Render_t` ESP32 : 156 → 148 octets** (-8 octets de structure), `Engine structs: Render=148 Game=0 Canvas=44 Total=952 bytes`, `[RENDERCORE] INIT owner=esp-native-bridge bytes=148` constatés au démarrage réel.
- Les vieux miroirs `Render_t::lineCount` / `Render_t::nodeCount` restent en **desktop et oracle géométrique** seulement ; ils sont absents des trois profils ESP32. Retrait des snapshots et mutations sans propriétaire dans `esp_native_bsp_visibility.c`, `esp_native_first_frame.c`, `esp_native_sprite_renderer.c`.
- Le parcours de première frame conserve son diagnostic de nœuds `FirstFrameWork.nodesTraversed` dans son scratch **heap-transient**, et le compte de lignes candidates `work->lineCandidates`. Le propriétaire de visibilité BSP conserve `state->nodes` ; les inventaires immuables `EspMapRuntime.nodeCount/lineCount` et les comptages de sprites restent inchangés.
- Aucun nouveau framebuffer, atlas, ressource PAK, allocations résidentes, sprite/monstre/entity hérités, ZIP runtime, RNG ni intervention sur input, tour ou présentation.

## CI logicielle

Code B initial `0cbd7d9060af6953a28cf61cda12ced7c40ceb7b`, correction uniquement du nom de pointeur `r->numLines` d'une assertion de test au commit `9899d2af07429dd00923cc73a3cf6ce604a29002`. Sur le dernier SHA, [CI push #42440393287](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42440393287) **SUCCESS (99 étapes / 0 échec)** et [CI PR #42440396940](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42440396940) **SUCCESS**. Tests hôte renforcés pour l'ABI (148), la conservation du diagnostic de première frame, la sémantique native BSP ; les trois firmwares `esp32-cyd`, bringup et trace compilés.

## Validation du vrai CYD — `device-monitor-261108-040005.log`

- ESP32-D0WD-V3 / 240 MHz / flash 4194304 B, PSRAM 0, logique 160×120 RGB565 (38400 B) sur TFT 320×240 x2, SD et PAK 241 entrées.
- NEW GAME intro/Entrance `/intro.bsp`, CRC `623f34e4`, arène `14095` B / FNV `c3882516`, première frame `71ca7465`, collisions, mouvements/strafe/rotations, sprites CROSS, caisses (y compris TRAPPED_REMOVE avec animation 180x3), ramassages, dialogues événement 88 opcode8 et 79 opcode26 suivis d'opcode19, porte 275 et fermeture différée, HUD/HUB ; retour MENU_MAIN avec `RESIDENTRESET empty=1`.
- LOAD V11 slot04 sur Entrance : restauration spatiale `exact=yes` (monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `df419482`), porte 113 / fermeture différée, Malwrath sprite322 : séquence 3× projectile204, 9 frames de trajectoire avant commit `playerHP=34→29`, armure 23→18 ; `PLAYERIMPACT` sprite184 frames0/2→2/2 ; contre-attaque au corps à corps `hp=11→0`, drop et gib FX, ramassage et HUB. Aucun acteur desktop.
- Dans ce même HUB : **LOAD V11 slot02 → Sector 1** `/level01.bsp` (map ID2, header legacy loadMapId3), arène `12476` B / FNV `82d2ee25`, 187 nœuds / 448 lignes / 278 sprites ; `NATIVESAVE` V11 restaure les overlays/monstres et la session se réarme, `[ENGINESESSION] RESUME-VISIBLE map=2` puis `READY ... shapeData=0x0 mediaTexels=0x0`. Retour menu final avec `RESIDENTRESET empty=1` et `SYSEXIT MENU-READY`.
- **RAM** : 3 fois `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`, marges `margin8=53440/52624/54180`, `largest8=86004` en jeu ; heap8 alives Entrance `109972`, puis `108788` en gameplay, après LOAD V11 `109968`, après changement de carte `108948`. Trois `RESIDENTRESET` finissent `empty=1`, deux `SYSEXIT MENU-READY` avec `saveWrite=no`, dernier `[ALIVE] heap8=155908 largest8=110580`.
- Toutes les frontières instrumentées `shapeData` / `mediaTexels` restent `0x0` ; aucune chaîne `panic`, `Guru Meditation`, `WDT`, `watchdog`, `abort` ou `[NATIVEFRAME] FAILED` dans les 1568 lignes.

## Portée de l'acceptation et suite

**PASS matériel ciblé du jalon B** : le Serial atteste les frontières observées, pas une signature SHA embarquée, une preuve de parité pixel-perfect, de vitesse A/B, d'endurance, de son audible ni du fonctionnement de toutes les cartes. Les -8 octets sont mesurés sur la taille de structure, **sans promettre -8 octets nets de heap**.

**Frontière B gelée au commit `9899d2af07429dd00923cc73a3cf6ce604a29002`.** Le présent rapport et ses entrées dans la documentation sont un commit docs-only relativement à B. L'utilisateur demande un **jalon C sur la même branche**, qui sera un nouveau code après ce PASS, avec **nouvelle CI et nouveau test CYD**. PR #266 maintenue Draft ; aucun merge vers main.
