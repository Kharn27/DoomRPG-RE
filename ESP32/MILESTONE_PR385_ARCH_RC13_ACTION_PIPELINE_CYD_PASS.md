# ARCH-RC13 — quatre wrappers SELECT/service/reset retirés : double CI SUCCESS et CYD PASS borné

**18 novembre 2026 — [PR #385](https://github.com/Kharn27/DoomRPG-RE/pull/385)**, branche `agent/esp32-arch-rc13-action-pipeline`, créée depuis le vrai `main=82c566488eb35edef6ee1339d29641ac4a920c46` (après la fusion d'ARCH-RC12 #383). HEAD **du code validé**, connu par contexte de l'envoi utilisateur : `7054abb33b0d3b2a0dcd173d0d3e0d507f785050`. **Le Serial n'embarque ni SHA Git du binaire flashé ni environnement PIO** : ne pas présenter l'association au code de la PR comme une attestation embarquée.

## Frontières et invariant d'exécution

- **4 GNU ld `--wrap` retirés ensemble** : `EspNativeGameplayAction_executeSelect`, `EspNativeGameplayActionEngine_executeSelect`, `EspNativeGameplayActionEngine_service`, `EspNativeGameplayActionEngine_reset`.
- **SELECT explicite** : `EspNativeResidentGameplay` → `EspNativeGameplayTransition_executeSelect` → `EspNativeGameplayMonsterCombat_executeSelect` → `EspNativeGameplayActionEngine_executeSelect` → `EspNativeGameplayAction_executeSelect` brut. Les événements et les transitions ont la priorité ; le fallback tracing entités/monstres garde l'ordre de décision. Aucun changement des calculs de dégâts, munitions ou mutations de monde.
- **Service explicite** : `PlayerResources_sessionService` → `MonsterCombat_actionService` → `HubActionGate_service` → `MonsterState_actionService` (admission zéro-ennemi sans allocation ou ensure) → `ActionEngine_service` brut ; puis combat en attente/redraw, avec les sorties anticipées originales. **Reset** : `MonsterCombat_actionReset` → `MonsterCombat_reset` → `MonsterState_actionReset` → `MonsterState_reset` → `ActionEngine_reset` brut. Les appelants session/menu sont dirigés vers cette composition explicite.
- Wraps `renderTurn/getLine/view` et autres frontières audio/PAK conservés. `shapeData == NULL`, `mediaTexels == NULL`, aucun ZIP runtime, aucune PSRAM, pas de grosse allocation map-wide réintroduite.

## Preuves CI exactes du HEAD de code

Double GitHub Actions **SUCCESS** au même HEAD `7054abb33b0d3b2a0dcd173d0d3e0d507f785050` :
- [push #44402023502](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44402023502)
- [pull_request #44402033326](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44402033326)

Les 178+ TU de production C/C++ sont scannées pour l'absence de flags/alias réintroduits et le propriétaire exclusif des callsites. `native_action_owner_chain_host.py` compile et exécute **les vrais corps C** des deux couches MonsterCombat/MonsterState service/reset (admission HUB, fast path zéro-ennemi, ensure, refus ActionEngine, pending, présentation, ordre des trois resets), avec **11 mutants sémantiques rejetés**. Les anciennes empreintes Git SDL-F18/F25/F26/F27 restent immuables par des inverses strictes dans les oracles seulement, ainsi que les autres régressions.

Quatre **vrais ELF Xtensa** (déclarés / liés / témoins d'appels directs) : `esp32-cyd=23/23/23`, `esp-cyd-inverted=23/23/23`, `esp32-cyd-trace=23/23/23`, `esp32-cyd-bringup=26/26/26`. Bilan **27 → 23** production/inverted/trace et **30 → 26** bringup. Les trois wraps de logging toolchain ne sont pas des wrappers Doom RPG.

## Capture CYD réelle — PASS matériel borné

Fichier transmis **`platformio-device-monitor-261118-224036.log`** : **2333 lignes**, **311897 octets**, SHA256 **`e13fbe25bbf477166298a137853e097207ffdc6131f2a1e56dca7827f0dd16b4`**. Matériel : ESP32-D0WD-V3 / 240 MHz / 4 MiB flash / **0 PSRAM**, framebuffer partagé 160×120 RGB565. Preuve de fonctionnement sur vrai matériel, pas une simulation hôte.

**Scénario complet observé :** démarrage depuis MENU_MAIN → NEW GAME Entrance (`/intro.bsp`, CRC `623f34e4`, arène immuable 14095 B, 30 ennemis) → déplacements, rotations, deux caisses, dialogues, pickups, portes et fermetures différées → HUB/System/EXIT sans save → MENU_MAIN → LOAD V11 `/DoomRPG-ESP32-slot04.sav` (5604 B, carte 1, `backupRecovery=no`) → reprise du monde avec 30 monstres restaurés, porte, activation/animation du monstre 322, trois coups du joueur et trois ripostes, mort + drop + poursuite de la progression, porte secrète → HUB/System/EXIT → MENU_MAIN actif longtemps sans reboot.

**Témoins SELECT/event-first :**
- `ACTIONENGINE TRACE` sur les caisses `seq=3` et `22`, suivi de `ACTION SELECT status=NO_EVENT` et de **deux `CRATE COMMIT`** (BREAK_REMOVE puis TRANSFORM), sans rupture de la chaîne native ; `seq=33` : `NO_ELIGIBLE` sur ligne, retour à `NOTHING_TO_USE` avec `worldMutation=no`.
- **14 `[ACTION] SELECT`**, dont des portes réellement commitées, des scripts de dialogue et des cas NO_EVENT/NO_ELIGIBLE.
- **5 `[DIALOG] OPEN` scriptés** (opcode 8 en SELECT, opcode 26 sur MOVE), **1 OPEN-STANDALONE**, **6 CLOSE**, **4 DIALOGCHAIN RESUME** (opcode19), **1 BACK `mode=back-cancel`** avec `stateMutation=no`. L'événement 60 a une reprise à 3 commandes avec `show=1 state=2 removed=1 topologySnapshot=2408B`, puis un BACK cancel sur sa nouvelle branche.
- **6 `DOORANIM COMPLETE`**, **3 HOLD/RELEASE différés**, dont `collision=closed-now` avant restauration de l'animation, et **2 SNAP** pour des lignes non regular. Les `MONSTERTURN DOOR-REQUEST` restent déclenchées après commit. `61 VIEWANIM END`, `85 VIEWANIM FRAME`.
- LOAD : `MONSTERSTATE STAGE-RESTORE` puis `RESTORE rngCalls=0`; `MONSTERCOMBAT RESUME-DEAD normalized=13`; `MONSTERACTIVESEQ COMPLETE` **27** occurrences, y compris 3 livraisons avec une seule attaque en vol.
- Combat : `MONSTERCOMBAT` sprite 322 HP `11→7→3→0`, ammo `16→15→14→13`, `death4→corpse2`, `xp=6-applied` et `MONSTERDROP COMMIT visible=1` ; **3 `MONSTERRETAL COMMIT`**, après `WAIT` animation, dont HP joueur `34→28→19→13` dans la capture. Aucun indice d'un commit spéculatif pendant WAIT.
- HUB : `HUBACTIONGATE PAUSE` observé deux fois et `RESUME` à la reprise de session ; `worldDispatch=blocked`, `turnAdvance=no` pour les sélections des pages System.
- **2 × `RESIDENTRESET ... after=0/0/0/0/0/0/0 empty=1`** (18008 B libérés chacun), suivis de **2 × `SYSEXIT MENU-READY session=off resident=empty saveWrite=no checkpoint=unchanged`** ; deux chargements **`RAMBUDGET HEADROOM_OK`**, `shapeData=0x0 mediaTexels=0x0`, aucun panic/WDT/fatal/transaction cassée observé. Le Serial se termine au menu avec `[ALIVE] uptime=204617ms heap8=126600 largest8=86004`.
- Musique : `ENTRANCEMUSIC STREAM` avec `underflows=0 recoveries=0`, et `FINAL underruns=0` dans les deux sessions, **aucun `SFXGEN DROP`**. Ne pas confondre ce constat transport avec une preuve de continuité perceptuelle.

## Anomalie hors scope conservée

**30 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`**, **uniquement après LOAD V11**, dont deux au second EXIT (lignes 2287 et 2289). C'est **déjà le sujet de l'[issue #377](https://github.com/Kharn27/DoomRPG-RE/issues/377)**, enrichie par cette capture. Il s'agit d'évictions d'intentions dans la FIFO : pas de preuve de son audiblement perdu ; musique 0 underrun/recovery et aucun `SFXGEN DROP`. Aucun changement audio dans RC13 ni nouvelle issue redondante. Problèmes des tonneaux #371 et audio #382 restent parallèles, aucune causalité RC13 démontrée.

## Limites et clôture

**PASS matériel borné**, pas une couverture exhaustive du jeu. La sélection réelle `SAVEGAME→CHANGEMAP` avec `WAIT_STATS`, la voie zéro ennemi du HUB, les erreurs injectées et toutes les formes de rollback ne sont pas exercées dans cette capture ; elles restent strictement gardées par le code et les tests hôte. **Aucun contrôle visuel pixel à pixel contre la version desktop**, ni preuve acoustique subjective. Les décisions de l'architecture native reposent sur les vrais logs de cette campagne.

Post-PASS : ajouter ce rapport + références dans `PORTING_STATUS.md` et `DOCUMENTATION.md` **en commits docs-only** ; vérifier le diff à partir du SHA du code déjà validé, obtenir une nouvelle double CI **SUCCESS**, puis laisser la PR **ouverte** pour fusion explicite par l'utilisateur (ne jamais merger `main` sans autorisation).
