# AUDIO-LOAD-L0 — Route native par niveau + probe de cadence loading (PENDING CYD)

Base exacte de ce milestone : `main=1e9c1268c30c1051836be6fb066e6d927064415e`, fusion [#360](https://github.com/Kharn27/DoomRPG-RE/pull/360), après #361. Branche `agent/audio-loading-l0-route-gap-probe`. Issue audio globale [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261).

## Pourquoi un L0 avant la diffusion musicale

Lors du vrai test CYD `platformio-device-monitor-261117-000639.log` :
- `M_IN_CIT.wav` : `PRESTART ready=1 elapsedMs=1092 firstFrame=NOT_PRESENTED`, zéro underrun, fermeture avant BSP.
- Le loading d'Entrance commence APRÈS fermeture intro (`TRANSITIONLOAD BEGIN`) et se termine après **5 402 ms** avec 8 frames ; `MAPFLASH REUSE HIT`. Les opérations BSP, flash et cache priming interviennent pendant ce délai. La musique Entrance démarre ensuite avec son propre préflight d'environ une seconde.
- Le ring de `16384` codes 8 bits à `22050 Hz` = ~743 ms, pas 5,4 s. Le propriétaire `EspNativeGameplaySession_service()` enchaîne plusieurs étapes synchrones dans un seul appel ; le `loop()` peut donc rester bloqué loin de `EspEntranceMusic_service()`, même lorsque la carte est déjà présente en flash.
- Lors d'une flash MISS, les callbacks de copie arrivent alors que les propriétaires PAK/SD/flash peuvent être actifs. Un callback de progression **n'est PAS** automatiquement un point sûr pour lire la musique sur la SD. Aucune deuxième tâche audio ni grand buffer ne doivent être introduits.

La demande est de commencer la musique du niveau dès l'écran de chargement **si la piste est connue**, sinon l'ancien mode **deferred**. Pour le faire sans faux bruit, nous devons connaître les longues périodes sans ravitaillement ainsi que les routes vérifiées.

## Changement L0 borné

- `ESP32/include/esp_audio_loading_route.h` définit une sélection native pure et allocation-free : **map ID 1 / ENTRANCE → `M_STALKS.wav`** ; **toutes les autres maps et tous les IDs invalides → deferred**. Ce n'est **pas** une autodétection du contenu SD, ni le moteur audio complet multi-cartes. Aucune carte ne reçoit par erreur la piste Entrance.
- `esp_native_transition_presentation.c` conserve le rendu et le flux de progression exacts. Il publie seulement :
  - `[LOADAUDIO-L0] ROUTE map=<id> track=M_STALKS.wav|none choice=explicit|unconfigured transport=deferred ... ringBudgetMs=743`.
  - `[LOADAUDIO-L0] BUDGET map=<id> route=... boundary=RELEASE|END|ABORT elapsedMs=... frames=... maxVisibleGapMs=... ringBudgetMs=743 flashCallbacks=... maxFlashCallbackGapMs=... checkpoints=... pakClosedCheckpoints=... playback=deferred`.
- La mesure `maxVisibleGapMs` décrit **l'intervalle maximal entre présentations physiques de loading** (inclut le dernier intervalle jusqu'à RELEASE/END). C'est une borne de diagnostic sur ce que la seule UI voit, **pas** un vrai relevé de famine DAC. `maxFlashCallbackGapMs` mesure les délais entre callbacks flash (sans effectuer de lecture SD dans les callbacks), et `pakClosedCheckpoints` compte les checkpoints où le pack est explicitement fermé ; ce ne sont pas des autorisations globales de SPI.
- **Aucune lecture de musique pendant loading** dans L0 : la route est identifiée mais `transport=deferred` pour toutes. Aucun nouvel audio ring, heap, tâche, ISR, timer ou gestionnaire de SD. Le préstart intro, les SFX et la sortie gameplay sont inchangés. Le code de présentation n'effectue que quelques appels à l'horloge et petits compteurs statiques.

## Tests et conditions d'acceptation

- `ESP32/test/audio_loading_l0_route_gap_host.py` compile en C11 la route : ID 1 choisi, les 255 autres IDs refusés, et contrôle les contrats de mesure/deferred dans le propriétaire loading, sans appel audio ou SD.
- CI `esp32-cyd` et `esp32-cyd-inverted` + oracles habituels.
- **Test matériel normal `esp32-cyd` :** (1) START, intro sonore puis loading Entrance déjà en flash (REUSE HIT), (2) si possible un LOAD/transition vers une carte différente qui provoque `MAPFLASH` MISS et copie SD→flash ; (3) comparer `[LOADAUDIO-L0] ROUTE/BUDGET`, `TRANSITIONLOAD RELEASE`, `MAPFLASH REUSE` / stade flash, `heap8` et changement visuel. La musique doit **rester deferred pendant loading** sur ce jalon ; vérification du niveau précédent/suivant et des contrôles SOUND.
- Ne jamais affirmer la sécurité d'une pompe audio uniquement sur des gaps de progress visibles. Le prochain jalon sera un **protocole réel de coopération avec le propriétaire PAK/flash**, music-start only if selected et si les tests HIT/MISS prouvent que le ring peut être rempli assez souvent.

## Retour matériel — 17 novembre 2026, premier essai (cache HIT)

L'utilisateur a fourni le log console complet de compilation, upload et session, sur la cible **`esp-cyd-inverted`** (et non le firmware normal `esp32-cyd`). Build réussi : RAM statique `84200/327680` (25,7 %), flash `889229/1310720` (67,8 %), flash vérifiée et démarrage CYD. Le message `alternate-panel inversion=off` est attendu par le profil inverted, qui ajuste volontairement l'inversion LCD. Le warning TFT_eSPI `TOUCH_CS pin not defined` n'affecte pas le tactile soft XPT2046 utilisé par le port ; `native_main_menu_options_back.c` émet encore un warning C `implicit declaration of function Esp32PlatformVideo_present` car `platform_video_c_bridge.h` n'est inclus que sous `DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY` ; **dette préexistante, hors L0**, non bloquante pour cet essai, à corriger dans une PR dédiée.

**Observations du test :**
- `[ENTRANCEMUSIC] PLAY ... M_IN_CIT.wav ... gainDiv=33`, `[INTROMUSIC] PRESTART ready=1 ... firstFrame=NOT_PRESENTED`, `underruns=0` et `softKneeFrames=0` jusqu'au `FINAL`; fermeture `INTRO_EXIT_BEFORE_MAP` avant `TRANSITIONLOAD BEGIN`, `timer=OFF dacPad=OFF file=CLOSED`.
- `[LOADAUDIO-L0] ROUTE map=1 track=M_STALKS.wav choice=explicit transport=deferred ... ringBudgetMs=743`.
- `[MAPFLASH] REUSE HIT ... rebuild=no`. Aucune copie SD→flash pendant ce cas.
- `[LOADAUDIO-L0] BUDGET map=1 route=M_STALKS.wav boundary=RELEASE elapsedMs=5496 frames=8 maxVisibleGapMs=3508 ringBudgetMs=743 flashCallbacks=0 maxFlashCallbackGapMs=0 checkpoints=7 pakClosedCheckpoints=7 playback=deferred`. `[TRANSITIONLOAD] RELEASE ... elapsedMs=5518 progress=100`. **Attention : 3508 ms est le maximum entre présentations du loading, pas une famine audio directement mesurée**, mais > 743 ms ; aucun fondement pour un simple start avant loading avec le ring actuel.
- `[ENGINESESSION] READY`, `shapeData=0x0 mediaTexels=0x0`, `[RAMBUDGET] ... margin8=25028 ... HEADROOM_OK`. Entrée audio gameplay `M_STALKS.wav` avec `gainDiv=15` : `SIGNAL at=3s ... softKneeFrames=31 underruns=0 recoveryStarts=0 maxSilentFrames=0` et `heap8≈60720 largest8=45044`.

**Conclusion bornée :** PASS matériel **inverted / Entrance / flash REUSE HIT** pour le routage explicitement deferred et les diagnostics sans panne visible. Pas de preuve hardware normale `esp32-cyd`, pas de flash MISS/copier SD→flash, et pas de PASS d'audio pendant le loading puisque ce code ne l'essaie pas. Avis utilisateur : éviter de dégrader l'existant ; **ne pas forcer le démarrage anticipé sans modèle coopératif sûr**. Cet essai ne justifie ni nouveau ring, ni lecture SD depuis les callbacks flash.

**Statut : CI SUCCESS + essai CYD inverted/HIT validé ; les autres chemins restent non éprouvés. PR en draft tant que la portée matérielle voulue n'est pas arrêtée.**
