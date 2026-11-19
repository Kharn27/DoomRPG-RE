# AUDIO-DOOR — SFX 5063 ouverture au premier frame, fermeture 5064 conservée : PASS matériel CYD borné

**PR :** [#387](https://github.com/Kharn27/DoomRPG-RE/pull/387) — 19 novembre 2026.  
**Code testé (HEAD branche d'après séquence de développement, non attesté dans le Serial) :** `30484489fe5d028a130dc7d2e1b466acc75fdfee`, base initiale `5712961c0c9355ab1196d88d8f4a1791b17a7681`. Le log ne contient **ni SHA Git firmware, ni environnement PIO** : la corrélation tient au contexte du test utilisateur. Le normal de production est `esp32-cyd`, pas le bringup.

## Contrat / cause / correctif

Le desktop `src/Game.c::Game_performDoorEvent` utilise le WAV natif `5063` lors d'un `OPEN 0→1` et `5064` lors d'un `CLOSE 1→0`, flags=0, priorité=2. La première implémentation PR #387 publiait `5063` à la fin des quatre frames d'ouverture ; la capture antérieure `platformio-device-monitor-261119-014401.log` montrait 4 frames totalisant **415 816 µs** avant `[AUDIOINTENT] PUBLISH resource=5063` et `[SFXGEN] START id=5063`. L'utilisateur constatait une forte latence d'ouverture, mais pas de fermeture.

La correction sur le même HEAD code utilise le petit `esp_native_door_sfx.c` : état **BSS borné à 8 lignes**, staging SELECT des **ouvertures admissibles seulement**, aucune publication speculative. Le propriétaire d'animation `esp_native_dynamic_line_render.c` publie `5063` **uniquement après le premier frame réellement présenté avec succès**, puis appelle le service audio coopératif `EspEntranceMusic_pumpGameplay()` sur le même task, PAK fermé. Après le frame 4, `finishSelect` évite toute seconde publication ; une ouverture non animée `SNAP` suit le fallback post-commit habituel. **La fermeture 5064 sur MOVE et HOLD/release reste au point de commit préexistant.** Pas de nouvelle allocation, buffer ring/ISR, fichier SD, boucle parallèle, mutation BSP, RNG, cadence monstre ou accès ZIP runtime.

## CI sur le code matériel testé

- [CI PR `44433054831` — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44433054831).
- [CI push `44433047532` — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44433047532).
- Le test C11 `ESP32/test/audio_door_sfx_host.py` compile le vrai propriétaire : staging sans audio, annulation premier frame échoué, deux ouvertures, unicité, fermeture tardive inchangée, fallback non animé ; les oracles historiques SDL-F18/F27/F28 reconstruisent exactement les anciens blobs par inverse mémoire limité. Ce sont des tests host, **pas** des injections d'échec réalisées sur CYD.

## Preuve matérielle — classic CYD ESP32-D0WD-V3 sans PSRAM

Fichier utilisateur `platformio-device-monitor-261119-020828.log` : **1 986 lignes, 257 251 octets, SHA256 `570088d78f287d98994d5304e5d79c8617775957892bd92815c89f09b5f5219c`**. Boot normal 4 Mo flash, PSRAM=0, écran RGB565 logique 160×120/38 400 B, affichage ×2 ; `shapeData=0x0`, `mediaTexels=0x0`.

**Première session NEW GAME / Entrance :** trois ouvertures régulières de la ligne 275 ; respectivement quatre frames RA2B avec première frame `totalUs=114090`, `90255`, `90879`. Dans **chaque** séquence : `[RA2B] FRAME` du frame 1 → `[AUDIOINTENT] PUBLISH resource=5063` → `[DOORSFX] OPEN-START ... phase=first-visible-frame` → `[SFXGEN] START id=5063` et `END` → frames RA2B 2, 3, 4 → `[DOORANIM] COMPLETE`. Un `5063` unique par ouverture, zéro replay en fin d'animation. Trois fermetures ligne 275 : `HOLD` → `[DOORSFX] COMMIT ... 5064` → `RELEASE` → `SFXGEN START/END` → animation complète. Aucun changement de cadence monstre ou de porte observé.

**Seconde session LOAD V11 slot04 :** quatrième ouverture animée (ligne 113), avec `OPEN-START 5063` et `SFXGEN START/END` avant `DOORANIM COMPLETE` ; une quatrième fermeture régulière `5064` (ligne 113), encore `HOLD→COMMIT→RELEASE→START/END→COMPLETE`. Deux `SNAP` non régulières lignes 112 et 111 publient chacun leur propre `5063` post-commit (deux `SFXGEN START/END`, pas un doublement). La capture RA2B s'arrête à 48 images et ne montre donc pas le premier frame de la quatrième ouverture ; l'ordre du hook est attesté par `OPEN-START phase=first-visible-frame` et les logs de l'animation.

**Compteurs globaux exacts :** `DOORANIM ARM=8`, `COMPLETE=8`, `HOLD=4`, `RELEASE=4`, `POST-MONSTER-COMPLETE=4`, `SNAP=2`. `DOORSFX OPEN-START=4` (animé), `DOORSFX COMMIT=6` (4 closes + 2 opens SNAP), `SFXGEN START=26` et `END=26`, `DROP=0`. `RAMBUDGET RESERVE HEADROOM_OK=2` ; en session heap8 `59072` B et plus grand bloc `45044` B sur observations régulières. `RESIDENTRESET empty=1=2` et `SYSEXIT MENU-READY=2` ; sauvegarde V11 **lue**, aucune écriture SAVE neuve démontrée. Aucun panic/Guru Meditation/WDT, `DOORANIM FAILED`, `MOVEEVENT FAILED`, `SFXGEN DROP`.

**Audio hors scope :** 6 STREAM `underflows=0`, cinq FINAL `underruns=0 recoveries=0 gapsOver90us=0`. **29 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`** dans la deuxième session après LOAD, comportement indépendant déjà suivi [issue #377](https://github.com/Kharn27/DoomRPG-RE/issues/377). L'absence de sous-alimentation DAC ne prouve pas l'absence d'effets audio perdus par FIFO ; ce correctif de porte n'essaie pas de résoudre #377.

## Frontière du PASS / suite

**PASS matériel borné sur 5063/5064 et absence de duplication**, avec amélioration audible confirmée par l'utilisateur (« C'st mieux :) »). Le log ne valide pas l'échec injecté de première frame, la transaction CHANGEMAP, toutes les maps ou l'identité du firmware compilé. En particulier un son déjà joué après un premier frame réellement visible ne peut pas être annulé acoustiquement si un frame suivant échoue : c'est un choix local de UX, non une garantie d'atomicité sonore totale.

**Politique après PASS :** gel du code testé, commits suivants **uniquement documentation** ; intégration d'ARCH-RC15 PR #388 (déjà fusionnée indépendamment dans main) à valider en CI avant toute fusion. Le test matériel présent précède cette intégration logicielle ; aucune preuve CYD du firmware combiné n'est revendiquée.
