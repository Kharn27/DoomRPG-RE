# ARCH-RC15 — transactions OPENLINE + destructibles : PASS matériel CYD borné

**PR :** [#388](https://github.com/Kharn27/DoomRPG-RE/pull/388) — 19 novembre 2026.
**Base GitHub réelle après RC14 :** `main=5712961c0c9355ab1196d88d8f4a1791b17a7681`.
**HEAD code éprouvé selon contexte du test utilisateur :** `cbee916a7d3b643a556dabb8c9eb9125259e2f53`. Le moniteur Serial **ne contient ni SHA firmware ni environnement PlatformIO** : ne pas présenter le couplage capture/commit comme une attestation binaire.

## Frontière du milestone

Trois GNU ld `--wrap` retirés, remplacés par des façades natives publiques et des feuilles explicites :

| Ancienne interposition | Composition native permanente |
| --- | --- |
| `EspMapLineState_applyDoorCommand` | `EspMapLineState_applyDoorCommandBase` dans `esp_map_line_state.c` écrit uniquement l'état de ligne ; la façade publique `EspMapLineState_applyDoorCommand` dans `esp_native_door_animator.c` planifie l'animation des portes régulières après mutation réussie, conserve le retour fail-closed et le rollback en cas de refus. |
| `EspNativeGameplayDestructible_executeLineDeath` | `EspNativeGameplayDestructible_executeLineDeathBase` dans `esp_native_gameplay_destructible.c` effectue la transaction ; `EspNativeGameplayDestructible_executeLineDeath` dans `esp_native_gameplay_destructible_turn_bridge.c` arme l'intent monstre seulement si `status=OK`, mutation effective et rollback disponible. La porte bloquée appelle la **Base OPENLINE**, sans animation de porte régulière. |
| `EspNativeGameplayDestructible_rollbackLineDeath` | `EspNativeGameplayDestructible_rollbackLineDeathBase` fait le rollback de ligne, puis façade publique `rollbackLineDeath` annule uniquement un intent pending de ligne correspondante et rollback réussi. `EspNativeGameplayDestructibleTurn_flush` reste différé, avec retry fail-closed si le producteur de tour est occupé. |

Ordre invariant : **mutation → animation OU rollback**, et pour la porte bloquée **mutation → lease rollback → commit attaque → flush du tour monstre**. Aucun changement volontaire PAK/ZIP, framebuffer, audio, RNG, calcul de combat, sauvegarde ou état BSP. `shapeData == mediaTexels == NULL`.

L'inventaire ELF vérifié en CI passe de **19 à 16 wraps** en normal/inverted/trace, et de **22 à 19** en bringup ; `EspNativeGameplayFrame_renderTurn`, `EspMapRuntime_getLine` et `EspMapLineState_view` restent inchangés.

## Validation logicielle sur HEAD code

- [CI push SUCCESS #44421742410](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44421742410)
- [CI pull_request SUCCESS #44421812716](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44421812716)
- Oracle `ESP32/test/native_line_door_transaction_boundary_host.py` : cinq **corps de fonctions C réels** extraits des sources firmware, y compris mutation de ligne brute, façade d'animation, armement de mort, rollback et flush du tour. Scénarios nominal/inactif/fail-closed, refus d'animation + rollback réussi/échoué, contournement animation pour la porte bloquée, armement et annulation ciblés, requête différée occupée + retry. **12 mutants adversariaux rejetés**, scan 178+ TU, inventaire réel sur quatre ELF Xtensa ; suites historiques conservées. Ce n'est pas un test hardware des erreurs injectées.

## Validation matérielle reçue : vrai classic CYD

**Fichier :** `platformio-device-monitor-261119-014948.log`. **3 124 lignes / 402 864 octets / SHA256 `a1e9fb42518a938f40011468107eace8fb8c6d2b7e0f18adc9bfd67361f0b13b`**.

Le moniteur confirme un **ESP32-D0WD-V3 / dual-core 240 MHz, flash 4 194 304 B, 0 PSRAM**, logique 160×120 RGB565 (**38 400 B**) et TFT 320×240 en nearest-neighbor ×2, SD montée et PAK indexé 241 entrées.

Parcours réellement exercé :

1. Démarrage MENU → NEW GAME → prologue → **Entrance** `/intro.bsp`, gameplay natif complet, caisses, ramassages clignotants, dialogues scriptés/standalone et rotations/mouvements.
2. Quatre paires de portes animées en tout : **8 `DOORANIM ARM` et 8 `DOORANIM COMPLETE`**, avec **4 `HOLD` + 4 `RELEASE` + 4 `POST-MONSTER-COMPLETE`**, fermeture logique et collision dès le HOLD puis publication visuelle après le tour. Quatre `DOORANIM SNAP` non régulières hors animation normale. Ligne 275, 352 et 234 (avant EXIT) ; ligne 113 (après LOAD).
3. **Porte bloquée réellement détruite à la hache :** `[ACTIONENGINE] TRACE seq=140 weapon=0 distance=1 tile=686 ... line=201 type=12 subtype=3 route=JAMMED_DOOR_CLEARED` ; `[DESTRUCTIBLETURN] ARM n=1 line=201 eventTile=686` ; `[DESTRUCTIBLE] HIT ... open=0->1 rngConsumed=1`, `[DESTRUCTIBLE] COMMIT ... message="Door cleared!" xp=1-deferred ... rollback=closed` ; `[DESTRUCTIBLETURN] REQUEST n=1 ... taggedSeq=d00000c9 rollbackWindow=closed` puis `[MONSTERTURN] ORDERED-DISPATCH reason=PASS_TURN turnToken=59` et `[MONSTERACTIVESEQ] COMPLETE turn=59 ... delivered=1`. Aucun double armement/double request de cet événement.
4. Première session → System HUB **EXIT double sélection** → `RESIDENTRESET empty=1` → menu. Carte principale **LOAD V11 slot04** (checkpoint 5 604 B), état map+monstres+topologie+overlay repris exactement, `REPRIME-HUD refresh=pending clear=ready`, `ENGINESESSION RESUME checkpoint=restored` ; gameplay/portes/combats continuent, puis deuxième System EXIT double sélection, nouveau nettoyage complet et retour menu.

**Compteurs exacts :** `VIEWANIM END=91`, `VIEWANIM FRAME=117`, `PLAYERRES COMMIT=24`, `DIALOG OPEN=12`, `DIALOG CLOSE=12`, `DIALOGCHAIN RESUME=7`, `SFXGEN START=34`, `SFXGEN END=34`, `SFXGEN DROP=0`, **2× RAMBUDGET RESERVE HEADROOM_OK**, **2× RESIDENTRESET released=18008 empty=1**, **2× SYSEXIT MENU-READY resident=empty**, **2× ENGINESESSION READY shapeData=0x0 mediaTexels=0x0**. Aucun `panic`, `Guru Meditation`, WDT, `DOORANIM FAILED`, `MOVEEVENT FAILED` ou `ACTIONENGINE FAILED` relevé.

**Audio observé, hors scope :** **54 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`** : 26 pendant NEW GAME (première occurrence ligne 1473), 28 pendant/reprise et sortie de la deuxième session, **contrairement aux précédentes captures où les overflow étaient seulement après LOAD**. Cela relève du problème déjà suivi dans [#377](https://github.com/Kharn27/DoomRPG-RE/issues/377) ; **34 SFXGEN START/END et 0 DROP**, 16 lignes STREAM avec `underflows=0`, cinq `ENTRANCEMUSIC FINAL` avec `underruns=0`, `recoveries=0`, `gapsOver90us=0`. Ne pas en conclure un dropout audible, ni introduire un correctif audio dans RC15. [#382](https://github.com/Kharn27/DoomRPG-RE/issues/382) et [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371) demeurent hors périmètre.

## Limites, conclusion, politique de fusion

**PASS matériel borné RC15** : chemins nominaux des façades OPENLINE, animation et HOLD/RELEASE, destruction à la hache et **exactement une demande différée de tour**, LOAD V11 et nettoyage sont prouvés sur CYD. **Non prouvés par ce log :** refus de programmation d'animation/rollback forcé, annulation du pending destructible, saturation du service de tour et retry, écriture SAVE nouvelle ou LOAD directement depuis System HUB, CHANGEMAP/WAIT_STATS, panneau inversé et comparaison pixel-golden. Les comportements d'échec sont testés en C host, sans les attribuer au CYD. L'absence de `DESTRUCTIBLETURN CANCEL` ne constitue pas une régression, car aucune transaction de destruction n'a échoué pendant ce parcours.

**Après test :** exclusivement ce rapport et la mise à jour des deux index `PORTING_STATUS.md` et `DOCUMENTATION.md` ; aucun changement au code firmware déjà testé. Exiger double CI documentaire finale SUCCESS, vérifier HEAD/main/mergeabilité, fusionner #388 seulement sur autorisation explicite de l'utilisateur (présente dans la demande du 19 novembre). Puis relire le SHA réel du nouveau `main`.
