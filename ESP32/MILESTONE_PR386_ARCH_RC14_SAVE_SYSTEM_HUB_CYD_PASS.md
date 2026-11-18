# ARCH-RC14 — Retrait SAVE/System HUB/Input/Session — PASS CYD borné

**PR :** [#386](https://github.com/Kharn27/DoomRPG-RE/pull/386) — 19 novembre 2026.  
**Base originale :** main post-RC13 `5d0e3fd8d2479e7310521f7573e6b8ebe62eb807`.  
**Base GitHub effectivement intégrée avant test :** main `fe85909babd686bb2041cc74dadf69c514edb519` (inclut GAME-PICKUP #384).  
**HEAD code de la PR au moment de la capture :** `1af71efebfebacefafbba89ff5f14b14a0debc05`. Le Serial ne contient **ni SHA Git embarqué ni env PlatformIO** ; le lien matériel/HEAD est établi uniquement par le contexte du test utilisateur, et non cryptographiquement.

## Périmètre et propriété native

Retrait groupé de 4 interpositions GNU ld, sans modifier la logique de monde/PAK/rendu/audio :

| Ancien `--wrap` | Nouvelle API publique de composition / leaf brute |
| --- | --- |
| `EspNativeGameplayHub_handleAction` | `EspNativeGameplayHub_handleAction` dans `esp_native_gameplay_save.cpp` → `EspNativeGameplayHub_handleActionBase` dans `hub.c`. Les deux fermetures HUB internes appellent explicitement **Base**, évitant une réentrance SAVE. |
| `EspNativeGameplayHubTouchUi_classify` | `EspNativeGameplayHubTouchUi_classify` dans `save_touch.c` → `classifyBase` dans `hub_touch_ui.c`, préservant les zones System, slots, onglets et Back. |
| `EspNativeGameplayInput_consume` | `EspNativeGameplayInput_consume` dans `save_touch.c` → `consumeBase` dans `input.c`, consommant l'intention une fois puis appliquant la sémantique SYS/slot, fail-closed. |
| `EspNativeGameplaySession_configure` | `EspNativeGameplaySession_configure` dans `save_reprime.c` → `configureBase` dans `session.c`, avec la préparation HUD de reprise préservée avant la configuration brute. |

Inventaire ELF revendiqué et contrôlé en CI : **23 → 19** wrappers normaux/inverted/trace ; bringup **26 → 22**. `EspNativeGameplayFrame_renderTurn`, `EspMapRuntime_getLine`, `EspMapLineState_view` sont conservés.

## Validation logicielle

Deux workflows GitHub **SUCCESS** sur le HEAD code intégré :
- [push #44412808159](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44412808159)
- [pull_request #44412820257](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44412820257)

Scan 178+ unités de compilation réelles ; tests hôte SYS/slots compilant le vrai `esp_native_gameplay_save_touch.c` (géométrie, neuf routes, fail-closed), **11 mutants sémantiques adversariaux rejetés** ; suites antérieures et oracles SHA historiques conservés. La validation de firmware sur poste local n'est pas revendiquée : les ELF et oracles passent dans GitHub Actions.

## Validation matérielle reçue

**Log :** `platformio-device-monitor-261119-000640.log` — **2618 lignes / 346835 octets / SHA256 `906a0a354d054e746632082082e874edc4dc2ba59617f404194c28d315350332`**. Classic CYD ESP32-D0WD-V3 à 240 MHz, 4 Mio flash, **0 PSRAM**, écran logique 160×120 RGB565 (38400 B), physique 320×240. Le test n'imprime ni SHA de firmware ni environnement PlatformIO.

Parcours réel :
1. Nouvelle partie Entrance `/intro.bsp`, intro, déplacements, caisses/pickups, scripts/dialogues, portes ; ouverture HUB System directe, appui tactile sur EXIT, **ARM puis CONFIRM**, sortie propre vers MENU.
2. Depuis MENU, **LOAD V11 via la carte Load Game**, panneau des dix slots, `MAIN-ARM slot=4`, `MAIN-CONFIRM slot=4` ; lecture du checkpoint `/DoomRPG-ESP32-slot04.sav` de **5604 B**, état restauré (monstres 30, topologie/positions/activation, ressources et overlays), `[NATIVESAVE] REPRIME-HUD refresh=pending clear=ready`, `ENGINESESSION RESUME checkpoint=restored` puis session visible.
3. Combat post-reprise (notamment monstre sprite322 de HP 11→7→3→0, décès et loot), portes, dialogues, déplacements, ramassages ; nouveau HUB System direct, EXIT ARM/CONFIRM, retour menu propre.

**Compteurs vérifiés :** 65 `VIEWANIM END` / 86 `VIEWANIM FRAME`, 8 `DOORANIM COMPLETE`, 3 HOLD et 3 RELEASE, 8 dialogues ouverts et 8 fermés, 7 `DIALOGCHAIN RESUME`, 20 `PLAYERRES COMMIT` ; **2 × `ENGINESESSION READY`**, **2 × `RAMBUDGET ... HEADROOM_OK`**, **2 × `RESIDENTRESET ... released=18008 ... empty=1`**, **2 × `SYSEXIT MENU-READY ... resident=empty`**. Invariants `shapeData=0x0`, `mediaTexels=0x0`, aucun panic/Guru/WDT/FAILED, pas de fallback de prévisualisation ou faute de géométrie détectée.

**Audio dans cette capture :** 44 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`, **tous en seconde session après LOAD** ; 28 `SFXGEN START` et 28 `SFXGEN END`, 0 `SFXGEN DROP`. Tous les `ENTRANCEMUSIC FINAL` reportent `underruns=0`, `recoveries=0`, `gapsOver90us=0` ; les STREAM intermédiaires ont `underflows=0`. Le débordement de file audio est **préexistant / hors périmètre**, suivi par [#377](https://github.com/Kharn27/DoomRPG-RE/issues/377) ; ceci n'établit pas de perte sonore audible. Les sujets audio [#382](https://github.com/Kharn27/DoomRPG-RE/issues/382) et barils [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371) restent séparés.

## Frontière du PASS et décision

**PASS matériel borné pour le retrait des quatre wrappers et la non-régression du parcours exercé.** Les routes directes System EXIT, main-menu LOAD V11, HUD reprime, input/slot et retour MENU sont prouvées. **Non exercés dans ce log :** nouvelle écriture System SAVE, LOAD depuis le panneau System du HUB, changement de carte, retour par bouton Back du HUB, échec d'écriture/injection d'erreurs, et comparaisons pixels golden ou affichage sur le panneau inverted. Ne pas les présenter comme validés matériellement. Le code d'écriture SAVE lui-même n'est pas modifié par RC14 ; le refactoring étant restreint et la CI intégrée doublement verte, cette limite n'est pas considérée comme bloquante pour cette PR, sous réserve de l'accord explicite de l'utilisateur.

**Post-test :** présent document et index `PORTING_STATUS.md` / `DOCUMENTATION.md` uniquement, aucun changement firmware après la capture. Revérifier double CI sur le HEAD documentaire avant fusion.
