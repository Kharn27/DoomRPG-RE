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

**Statut : PENDING hardware. L0 est une instrumentation/stratégie de routage, pas encore le début anticipé audible du niveau.**
