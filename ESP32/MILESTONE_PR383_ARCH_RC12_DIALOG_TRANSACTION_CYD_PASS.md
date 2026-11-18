# ARCH-RC12 — retrait des trois wrappers Dialog/NOTE/EventChain : double CI SUCCESS + test matériel CYD PASS borné

**18 novembre 2026 — [PR #383](https://github.com/Kharn27/DoomRPG-RE/pull/383)**, branche `agent/esp32-arch-rc12-dialog-chain-transaction` issue du vrai `main=35b45f570fe570ae4b1c75153a1b900cc2213f2d`. HEAD du **code testé par contexte de la validation utilisateur** : `cf8c3f520b6b2b2e74b614071a44e2174b6e723f`. Le Serial ne transporte **ni SHA Git du firmware réellement chargé ni environnement PlatformIO** ; ne pas confondre association contextuelle et attestation de binaire.

## Frontière de propriété conservée

- **3 GNU ld `--wrap` retirés ensemble** : `EspNativeGameplayDialog_begin`, `EspNativeGameplayDialog_resume`, `EspNativeGameplayDialog_rollbackResume`. Inventaire firmware : **30 → 27** normal/inverted/trace et **33 → 30** bringup (à chaque profil, déclarés = liés = témoins directs).
- **Ouverture** : `EspNativeGameplayNotePrefix_beginDialog` garde le préfixe NOTE et son mécanisme de validation, candidature, commit/rollback ; il appelle `EspNativeGameplayEventChain_beginDialogCommand`, qui conserve preflight intégral, masque temporaire de continuation, ouverture du propriétaire Dialog puis restitution du masque. `EspNativeGameplayDialog_begin` demeure la feuille brute inchangée ; chemins PASSWORD toujours via EventChain.
- **Fermeture et rollback** : `EspNativeGameplayEventChain_resumeDialog` / `EspNativeGameplayEventChain_rollbackDialogResume` restent propriétaires des commandes enchaînées, journal de mutations, pauses WAIT et restauration ; l'affichage reste possédé par Dialog. Pas de nouveau `--wrap`, pas de nouvelle allocation permanente, comportement C de l'ancien journal conservé.
- Sont **volontairement conservés** les wrappers `EspNativeGameplayFrame_renderTurn`, `EspMapRuntime_getLine`, `EspMapLineState_view`. Aucun code audio, RNG, entités, renderer, portes ou PAK modifié pour RC12. `shapeData` et `mediaTexels` restent NULL ; flash PAK native, pas de lecture ZIP runtime.

## Preuves CI du code

Sur le HEAD de code `cf8c3f520b6b2b2e74b614071a44e2174b6e723f`, **double GitHub Actions SUCCESS** : [push #44385792838](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44385792838) et [pull_request #44385807550](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44385807550).

- Scan strict de **178+ fichiers de production C/C++**, signatures et callsites exacts, interdiction de réintroduire les 3 flags/alias GNU.
- Compilation/exécution du **vrai corps C NOTE begin** avec succès, échec PAK, allocation, préflight, rollback, non-réentrance ; **11 mutants adversariaux rejetés**.
- `paid_interaction_host.c` compilant le **vrai EventChain C** : reprise de scripts, WAIT, gate de présentation, gestion de trames et refus des continuations falsifiées ; `script_startup_automap_host.c` mock réaligné aux API permanentes.
- Empreintes SHA historisées SDL-F18, F19, F24, F27 et régressions antérieures préservées par inverses `arch_rc12_history.py` appliqués uniquement aux oracles, jamais au firmware.
- Quatre vrais ELF Xtensa : **esp32-cyd 27/27/27 ; esp-cyd-inverted 27/27/27 ; esp32-cyd-trace 27/27/27 ; esp32-cyd-bringup 30/30/30** (flags/liens/direct-call). Les 3 alias du logging toolchain sont hors inventaire des wrappers Doom RPG.

## Capture matérielle — 18 novembre 2026

Fichier utilisateur **`platformio-device-monitor-261118-212921.log`**, **2 507 lignes**, **340 277 octets**, SHA256 **`469273ee2983d6282fab6631c3e395524c720fefae31353d478ea19fae4aac95`**. Identité observée : ESP32-D0WD-V3 révision 3, dual-core 240 MHz, flash 4 194 304 octets, **PSRAM=0**, RGB565 logique **160×120 / 38 400 octets**, sortie physique 320×240. Configuration écran `inversion=off`. Deux sessions : **NEW GAME Entrance**, EXIT vers MENU, **LOAD V11 slot04 (5604 B) Entrance** avec restauration exacte des états script/lines/resources/topologie/monstres, puis combat jusqu'à la mort et retour **DEATHMENU → MAIN**.

| Domaine vérifié | Preuves et lignes de capture |
|---|---|
| Ouverture Dialog | **4** dialogues scriptés : 3 `EV_DIALOG opcode8` via SELECT (events 88,60,33) et 1 `EV_DIALOGNOBACK opcode26` sur MOVE (event79), plus **1** standalone après pickup Fire Ext. **5 fermetures correspondantes** ; leases PAK `packClosed=yes`. |
| Annulation | Event88 : `CLOSE mode=back-cancel` ligne **611**, `DIALOG-CANCEL ... resume=no stateMutation=no turnAdvance=no` ligne **613**. Le contrat BACK n'est pas celui du dialogue opcode26 (back=0). |
| Reprise & mutations | **3× DIALOGCHAIN RESUME** : ligne **943** event79 state=1 ; **1065** event60 `handled=3 show=1 state=2 removed=1 topologySnapshot=2408B` ; **1481** event33 state=1. `DIALOG-RESUME` lignes **944, 1066, 1482**, opcodes de continuation **19,19,11** ; pas d'échec ni d'erreur de rollback observés. |
| Images & portes | **70 `VIEWANIM END`, 98 `FRAME`** (18 rotations/36 frames ; 52 moves/62 frames), toutes les `FRAME` `rendered=yes presented=1 gameplayStable=yes`, aucun fallback. **10× DOORANIM ARM et COMPLETE**, **5× HOLD → RELEASE → POST-MONSTER-COMPLETE** ; aucun `DYNAMICLINES PREVIEW-DEFER` ou `DOORANIM GEOMETRY-FAULT`. |
| Entrée, monde, gameplay | Pickup/caisses, déplacements et combats, LOAD V11 `version=11 bytes=5604` ligne **1229**, état script et topologie restaurés ; mort réelle suivie de `PLAYERDEATH READY` puis MAIN ; aucune désynchronisation dialogue/world observée. |
| RAM / teardown | **2× ENGINESESSION READY**, **2× TRANSITIONLOAD RELEASE progress=100**, **2× RAMBUDGET HEADROOM_OK** (marges 8-bit **24584 et 23324 octets**), **2× DIALOGCHAIN OWNER-RELEASE** (dont topologyCapacity 2408B) suivis de **2× RESIDENTRESET released=18008 empty=1** (lignes **1137, 2476**), **2× SYSEXIT MENU-READY resident=empty** (lignes **1164, 2507**). |
| Invariants | `shapeData=0x0 mediaTexels=0x0` sur les frontières imprimées ; aucun panic/Guru/WDT, fatal renderer ou heap corruption dans cette capture. |

**Important sur `activeAtTeardown=1`** : les logs `DIALOGCHAIN OWNER-RELEASE` indiquent un journal encore alloué/armé juste avant teardown, **puis libéré** (récupérations 3568B et 1144B) ; `RESIDENTRESET empty=1` est le témoin final. Ce drapeau n'est pas à lui seul un rollback manqué ou une fuite.

### Limites strictes de la preuve

**Aucun `[NOTE]` réel** dans ce test : les chemins NOTE+dialogue restent couverts par test C/mutants mais **non démontrés sur le CYD**. Aucune faute forcée de rendu, réentrance volontaire, ni rollback d'échec injecté matériellement. Aucun `[SCRIPTWAIT]` détecté : WAIT validé par test C hôte, non traversé matériellement. Un Serial ne prouve ni les empreintes golden de pixels, ni le SHA exact du firmware chargé. Il n'y a pas de comparaison de son subjectif.

### Anomalies audio hors scope, suivies avant fusion

1. **[#377](https://github.com/Kharn27/DoomRPG-RE/issues/377)** : **38× `AUDIOINTENT OVERFLOW drop=oldest capacity=16`**, toutes dans la session LOAD, première ligne **1536**, dernière **2480** ; aucune dans NEW GAME. Mise à jour de l'issue existante ; **ne pas créer un doublon** ni assimiler FIFO à preuve de son perdu.
2. **[#382](https://github.com/Kharn27/DoomRPG-RE/issues/382)** : NEW GAME `M_STALKS` terminé à **0 underrun / 0 recovery** (ligne 1141). Après LOAD, `STREAM underflows=513 recoveries=1 maxSilentFrames=513 maxPublishGapMs=590 gapsOver500Ms=3` (ligne **2433**) et `FINAL underruns=513 recoveries=1` (ligne **2484**) ; **1× `SFXGEN DROP` id5099 `reader-caught-up mixed=10847/19404` ligne **2372**. Problème déjà consigné, sans relation causale démontrée avec RC12 ni les 38 FIFO overflows.
3. **[#371](https://github.com/Kharn27/DoomRPG-RE/issues/371)** : incident tonneaux conservé hors scope, non reproduit ici.

**Verdict : PASS CYD borné, aucune régression RC12 attribuable observée.** Les issues audio sont maintenues séparément ; seuls commits post-test **docs-only** autorisés sur la PR. Vérifier les deux CI de ce HEAD documentaire avant fusion, puis ne merger qu'après autorisation utilisateur (donnée dans la demande de validation).
