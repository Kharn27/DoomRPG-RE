# AUDIO #377 — FIFO AudioIntent avec ACK consommateur : PASS matériel CYD borné

**PR :** [#390](https://github.com/Kharn27/DoomRPG-RE/pull/390), issue [#377](https://github.com/Kharn27/DoomRPG-RE/issues/377). **Date :** 19 novembre 2026.

**Main de départ exact :** `73520c22afa0e849807cd75de3540d9a5f826d45`.
**HEAD CODE inchangé après le test :** `81a93de01c755491f26f6a1346d6f33e9fca17a0`. Il s'agit du code de la branche fourni à tester ; **le moniteur Serial ne contient ni SHA du firmware ni environnement PlatformIO**. La corrélation au binaire relève donc du contexte de la validation utilisateur, non d'un fingerprint inscrit dans le log. Le firmware attendu est le normal `esp32-cyd`.

## Preuve utilisateur : true classic CYD ESP32-D0WD-V3 sans PSRAM

Capture : `platformio-device-monitor-261119-111849.log` — **2 580 lignes, 341 990 octets**, SHA256 `60798a294eb57b2bbec0140bc1eb0851a2e1ab3bb46d9e0988a0222fbf11ec92`.

Scénario observé : démarrage, menu et intro, **NEW GAME Entrance**, destruction de caisses et tir 5046, ramassages 5060, dialogues avec PAK, portes animées 5063/5064, déplacements ; HUB/System EXIT → menu ; **LOAD V11 /DoomRPG-ESP32-slot04.sav** (5604 B, map 1) ; combats de monstres, audio ambiant/sight/idle, pickups, portes, HUB/System EXIT → menu. **2 RESIDENTRESET empty=1**, **2 SYSEXIT MENU-READY**, **2 RAMBUDGET HEADROOM_OK**. L'anomalie de rendu rapportée par l'utilisateur est explicitement **hors périmètre** et transmise à un autre développeur.

## Preuve FIFO : faux positifs éliminés, événements conservés

La capture antérieure `platformio-device-monitor-261119-020828.log` avait **29 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`** pour des intentions anciennes déjà consommées.

Nouvelle capture :
- **63 `[AUDIOINTENT] PUBLISH` + 2 `STOP_ALL` = 65 intentions** cumulées, sans reset de séquence ; **0 OVERFLOW**, **0 CURSOR-GAP** dans tout le log.
- Les **5 `[AUDIOINTENT] AUDIT`** affichent tous `queueOverflow=0`, `consumerCursorGap=0`, `audibleSfxDropped=0`, `pending=0`.
- **Audit final** (ligne 2555) : `consumed=59 handoffDiscarded=6 pending=0 sfxPlayedTrack=47 sfxRejectedTrack=0`. La conservation est exacte : **59+6=65** ; les six discards sont explicites aux frontières de sessions, pas des évictions silencieuses. La première session se termine `consumed=12 handoffDiscarded=3` (ligne 1281) ; le LOAD ajoute les 47 mixages SFX de la seconde session et 3 autres intentions hors gameplay.
- Un `[SFXP2] DEFER id=5060 reason=pack-lease-open` (ligne 923) est suivi par **`[SFXP1] MIX id=5060`** (ligne 937), après fermeture du dialogue. Le peek n'ACK pas prématurément l'intent différé.

## Preuve mixeur et musique (borne réelle)

- **39 `[SFXGEN] START` / 39 `[SFXGEN] END` / 0 `[SFXGEN] DROP`**, avec 7 démarrages/achèvements 5063 et 4 de 5064, ainsi que SFX monstres (5072, 5081, 5099, 5094, 5108 etc.).
- **19 `[SFXP1] MIX` complets** : 5 × 5046 et 14 × 5060 ; **0 `[SFXP1] DROP` et 0 `[SFXP1] PARTIAL`**. Le long effet 5045 est intégralement publié en 3 étapes `[SFXP2] CHUNK/CHUNK/MIX` : `17640/17640` output frames (lignes 1209, 1213–1214).
- `[DOORSFX] OPEN-START` 5 fois (première image confirmée des ouvertures régulières) ; total 7 × 5063 avec deux ouvertures SNAP ; 4 × 5064 fermeture, chacun `SFXGEN START/END`.
- **5 finales de musique** (`MENU_END_ONCE`, `INTRO_EXIT_BEFORE_MAP`, `MENU_ENTRY_REPLACE_PRIOR` deux fois, `MENU_ACTION_BEFORE_PAK`) : toutes `underruns=0 recoveries=0 maxSilentFrames=0 gapsOver90us=0 gapsOver500Ms=0`. **12 STREAM** avec `underflows=0`. Six clocks mesurées `22013 Hz`, dans le guard `20900..23200`.
- Dernières sessions Entrance : `M_STALKS.wav`, `maxPublishGapMs=466` puis `482`, **aucun gap >500 ms**, `maxSdReadUs <=4336`, `slowReadsOver20ms=0`. Le `M_DM2TTL.wav` de menu redémarre à chaque retour, sans lecteur SD concurrent ni ISR/ring supplémentaire.
- Le compte `audibleSfxDropped` mesure les rejets/partiels connus du mixeur : **0 ici**, mais cela ne constitue pas un enregistrement acoustique, et les logs seuls ne prouvent pas l'absence de toute anomalie subjective subtile.

## Portée et limites

**PASS matériel borné** pour la correction #377 : les fausses alertes de FIFO ne se reproduisent pas même après LOAD V11, aucun vrai burst non consommé ni perte PCM n'est observé, la consommation et les discards se réconcilient exactement, STOP_ALL/transition ne rejouent pas de sons périmés, musique stable. Aucune correction du rendu n'est faite. Pas de preuve d'un burst matériel volontairement >16 événements simultanés non traités : ce chemin **fail-open visible** n'a été exercé que par l'oracle host.

**Validation code initiale** : double CI SUCCESS sur le HEAD matériel `81a93de01c755491f26f6a1346d6f33e9fca17a0` :
- [CI PR #44439421061](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44439421061)
- [CI push #44439414707](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44439414707)

Après le test, seuls ce rapport, les index et les liens documentaires peuvent être modifiés. Il faudra relever la double CI documentaire **sur le SHA final** avant de déclarer la PR merge-ready. **Ne pas fusionner `main` sans demande explicite.** La clôture #377 est appropriée après fusion du correctif et conservation de ce rapport. La problématique séparée de sous-alimentation audio #382 reste hors scope.
