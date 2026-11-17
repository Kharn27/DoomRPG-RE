# ARCH-RC8 — composition explicite PlayerResources → MonsterTurn : PASS CYD borné

**17 novembre 2026 — [PR #372](https://github.com/Kharn27/DoomRPG-RE/pull/372)**. Branch `agent/esp32-arch-rc8-monster-turn-session-chain`. Base GitHub réelle `main=9e892bbfbc01dd57e3dca0a5e7033ad463546464`. HEAD code/test soumis au test matériel selon la conversation : `20946aa9ea9cfc8cd118d48b62b557dce94087ed`. **Le Serial n'encode pas le SHA Git**, donc la correspondance repose sur la branche communiquée et la séquence de test utilisateur, et non une attestation cryptographique embarquée.

## Frontière et ordre exact

Lot homogène de deux `--wrap` GNU ld supprimés :
- `EspNativeGameplayPlayerResources_sessionService`
- `EspNativeGameplayPlayerResources_sessionReset`

L'ancienne interposition dans `esp_native_gameplay_monster_turn.c` s'insérait immédiatement après la leaf PlayerResources/ActionEngine ; elle observait les tours puis délivrait l'activation fail-closed. Le coordinateur existant `esp_native_gameplay_gib_fx.c` appelle maintenant directement la leaf `EspNativeGameplayPlayerResources_sessionService(doomRpg)`, **puis** `EspNativeGameplayMonsterTurn_afterPlayerResourcesService(doomRpg)` qui fait `observeAndProbe` puis `EspNativeGameplayMonsterActivation_serviceTurn`. Le hook est exécuté même si la leaf ressources se termine prématurément (contrat ancien du wrapper). Reset : `EspNativeGameplayMonsterTurn_reset` **avant** `EspNativeGameplayPlayerResources_sessionReset`, puis les autres owners. La composition outer session, les fenêtres d'animations, le rollback de combat, la transaction player/world, l'ordre RNG et la publication de l'attaque restent inchangés. Pas de nouvelle allocation, de propriétaire permanent, de read ZIP runtime, ni changement audio/PAK/barils.

Les interpositions `EspNativeGameplaySession_service/reset`, les getters publics de topologie, `EspMapRuntime_getMapSprite`, RNG, `Esp32PlatformVideo_present` et les frontières de mutation continuent à fonctionner comme avant : ne pas les déclarer supprimés.

## Oracle, builds et lien réel

`ESP32/test/native_monster_turn_resource_chain_host.py` extrait et compile trois **véritables fonctions C** : hook MonsterTurn et outer service/reset GibFX ; appels leaf simulés uniquement, pas les corps testés. **5 scénarios** : service normal, activation échouée fail-closed, pointeur DoomRPG NULL, teardown, teardown répété ; **12 mutants** adversariaux refusés (ordre reset/service, omission/duplication de l'activation, omission de la phase de fin, de la transition et des FX, NULL gate, etc.). Retraits refusés par l'audit de déclarations. Historique SDL-F26/F27 : les modifications RC8 sont inversées exactement dans les oracles, recomposition des blobs SHA `ce81c39d4097154ffe6ce5f30182d59985a29840` (monster turn) et `12254012ee7c9089ee1a5e1eacbe200dd5626a55` (gib FX), correspondant aux empreintes gelées.

Double CI **SUCCESS** sur `20946aa9ea9cfc8cd118d48b62b557dce94087ed` :
- [push #44176234035](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44176234035)
- [PR #44176445485](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44176445485)

Quatre vrais ELF Xtensa audités (déclarations / symboles liés / témoins d'appels directs) : `esp32-cyd` **36/36/36**, `esp-cyd-inverted` **36/36/36**, `esp32-cyd-trace` **36/36/36**, `esp32-cyd-bringup` **39/39/39**. Trois interpositions de logging toolchain externes au port exclues ; le témoin d'appel direct n'établit pas l'exécution de tous les chemins.

## Test du vrai CYD — PASS RC8

Capture `platformio-device-monitor-261117-224224.log` : **1 613 lignes / 224 710 octets**, SHA256 `f15b1fb8f5fd54af5c0d76f816abe9624bbdad8e861763aebd896cc8a6762563`. Chip ESP32-D0WD-V3, 240 MHz, 4 MiB flash, **0 PSRAM**, framebuffer **160×120 RGB565 / 38 400 octets**, x2 nearest-neighbor ; `inversion=off` observé. Le log ne désigne pas explicitement l'environnement PlatformIO choisi au flash, donc aucune validation physique du profil inverted n'est prétendue.

- **Session 1 — START neuf** : PAK et MAPFLASH `REUSE HIT`, Entrance, `ENGINESESSION READY`, rotations, déplacement/strafe, PASS_TURN, trois caisses (une retirée et deux transformées), sept pickups cumulés sur les deux sessions, portes animation ouverte/fermée, 3 dialogues fermés, hub inventory/system, `SYS EXIT`. Tours natifs dispatchés avec `activeCount=0` (cas valide, sans attaque immédiate). `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY`.
- **Session 2 — LOAD V11 slot04** : 5 604 octets, overlay resources/lines/script/crates/monsters/topology/position/activation/drop restaurés, Entrance, `ENGINESESSION READY`. Activation sprite **322** derrière porte, `activeCount=14`, quatre `MONSTERTURN MEMBER-ATTACK-PROBE` et quatre `MONSTERACT DELIVER` / résolutions `MONSTERRETAL` (trois non létales, une létale). Deux attaques player `MONSTERCOMBAT COMMIT` Malwrath **11→7→3 HP**. Dernière attaque monster triple projectile complétée, puis `PLAYERDEATH ARM` et `PLAYERDEATH READY` ; sélection MAIN sur death menu, `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY`. Aucune mort de monstre/drop revendiquée pour cette session.
- **Deux sessions** : 2 `ENGINESESSION READY`, 2 `RAMBUDGET HEADROOM_OK`, 2 `RESIDENTRESET empty=1`, 2 `SYSEXIT MENU-READY`, `shapeData=mediaTexels=0x0`, allocations natives bornées. Heap8 échantillonné au cours de la musique en jeu ~**59 120–60 276 B** (source : ALIVE pendant le gameplay, non mesure continue d'un minimum absolu), `largest8=45044`; retour au menu après fermeture musique `heap8=127052 largest8=86004`. `M_STALKS.wav` joué en deux sessions avec SFX `MIX`, intro également musicale ; `underruns=0` aux signaux et finals pertinents, aucune récupération. **Ne pas extrapoler une qualité sonore perçue**.
- Pas de `Guru Meditation`, panic, WDT, `FAILED` ni teardown incomplet dans ce log.

**Limites** : ni toutes cartes ni frame golden ni flash MISS ni carte inverted physique ; aucune injection de faute rollback host→CYD ; pas de preuve de fonctionnement sans fichier musical ; pas de preuve audio perceptive. **Aucun changement ni test ciblé des chaînes de barils létales**, problème séparé [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371). La mort du joueur est une conséquence de combat cohérente observée, et non une panne.

## Clôture de milestone

Les changements post-test propres à RC8 sont **documentation seulement**. Durant la fenêtre de travail, `main` a avancé indépendamment par la PR BOOT-LEGALS [#370](https://github.com/Kharn27/DoomRPG-RE/pull/370), SHA observé `51208686828b5887db54c00d2cc378010b1ad1ac` ; les apports sont distincts (initialisation/intro et workflow CI), mais une résolution éventuelle du workflow en convergence doit conserver les deux familles de tests. Ne pas prétendre que le nouveau `main` combiné a été testé physiquement sur ce log. Double CI sur le HEAD documentaire et combinaison main exigées avant merge ; fusion uniquement avec autorisation utilisateur (accord formulé après analyse de ce Serial).
