# AUDIO-LOAD-L1 — Musique au READY du loading, AVANT première image gameplay (PASS CYD borné : Entrance/HIT)

**Base** : [PR #362](https://github.com/Kharn27/DoomRPG-RE/pull/362), branche `agent/audio-loading-l0-route-gap-probe`, fork du vrai `main=1e9c1268c30c1051836be6fb066e6d927064415e`. Ce jalon prolonge les mesures L0 de `MILESTONE_AUDIO_LOADING_L0_PENDING_CYD.md` sans remettre en cause la frontière BSP/flash.

## Contrat

Objectif utilisateur, clarifié après le vrai test L0 (inverted, Entrance, MAPFLASH REUSE HIT) : **ne PAS jouer pendant le chargement**, mais débuter **immédiatement à la fin du chargement, juste avant que le gameplay apparaisse**. Les mesures L0 `maxVisibleGapMs=3508` contre ring `743 ms` suffisent à déconseiller une simple lecture précoce qui manquerait de refill (ce ne sont pas des mesures directes d'underrun).

La transition native possède exactement cette frontière dans `EspNativeGameplaySession_service()` :
1. `SESSION_STAGE_ACTIVE` n'est atteint qu'après BSP, activation du cache, `SMALL-COLD/WARM`, `LARGE-LEARN/WARM`, `RESIDENTGAMEPLAY` et `HEADROOM_OK`.
2. En présence d'un loading actif, `checkpointProgress(100, "READY")` affiche physiquement la carte loading à 100 %, puis `renderCacheWitness("FINAL-READY")` reconstruit hors-écran le framebuffer final.
3. `EspNativeTransitionPresentation_releaseLoading()` rend l'ownership ; `EspNativeGameplayHud_repaint()` restaure le HUD complet dans le framebuffer logique. **L'écran physique montre toujours READY**.
4. **L1** exécute une seule fois `EspNativeSfxProbe_service(true)` **si une piste est sélectionnée et SOUND ON**, avant l'audio, pour terminer les lectures PAK de catalogue 92 WAV (évite que cette opération perturbe les premiers 743 ms de la piste). En SOUND OFF, le scan conserve son déclenchement d'origine après la présentation du jeu : pas de délai ajouté au premier frame.
5. **L1** exécute `EspEntranceMusic_prestartGameplayBeforeFirstFrame(view->targetMapId)`. Map ID=1 ENTRANCE et SOUND ON seulement : le même `EspEntranceMusic_service(true,true)` ouvre `M_STALKS.wav`, alloue l'unique ring 16 KiB, décode et mesure l'horloge DAC/Timer1 dans **la même tâche Arduino**, avec un délai plafonné à 1800 ms, en restant sur l'écran READY. DAC active le flux uniquement après préflight 20900..23200 Hz.
6. Immédiatement ensuite, `Esp32PlatformVideo_present()` présente la première image jouable ; aucun nouveau rendu et aucun pipeline SDL. Si musique absente/WAV invalide, SOUND OFF, map non configurée, mémoire basse, PAK encore ouvert ou timer invalide : **l'image est quand même présentée**. Aucun start tardif si tentative audio échouée.
7. `EspEntranceMusic_stopBeforeLoading()` ferme systématiquement la piste du niveau précédent, y compris son ring/SD/timer, après validation du nouvel écran loading mais **avant** toute lecture BSP, copie SD→flash ou ouverture de PAK. La transition ST_INTRO maintient son propre `INTRO_EXIT_BEFORE_MAP`, appel idempotent en L1.

**Routing :** map ID 1 ENTRANCE → M_STALKS ; toutes les autres maps → `deferred`, pas de musique Entrance jouée accidentellement sur Junction/LOAD. Cela ne généralise pas encore le catalogue musical.

## Budget et intégrité

- **Pas de ring supplémentaire**, pas de PSRAM, de ZIP, de buffer PCM map-wide, d'ISR modifiée, de tâche audio concurrente ni d'appel à `SD.open` depuis callback flash.
- Allocation mémoire de 16 KiB identique à la musique gameplay actuelle, mais **~1,1 seconde plus tôt** ; vérifier les stats de heap au nouveau point de démarrage sur vrai CYD.
- Le loading est terminé avant le préstart : un court **palier 100 %** d'environ une seconde est volontaire. Ce n'est pas un blocage pendant la copie ; l'utilisateur décidera s'il est visuellement acceptable.
- En cas de lecture impossible ou de manque de RAM : fail-closed, image continue normalement, pas de deadlock attendu ; le test physique doit le confirmer.
- Le code L0 continue de logguer `transport=deferred` pendant toute la phase de loading, jusqu'au RELEASE. L1 loggue ensuite `[LOADAUDIO-L1] PRESTART map=1 route=M_STALKS.wav ready=1 elapsedMs=... beforeWorldPresent=1`, puis `[ENGINESESSION] FRESH-VISIBLE ...`.
- L'API est déclenchée uniquement sur les chemins `sessionState.stage==ACTIVE` **avec un écran de loading encore actif** : START et checkpoint LOAD avec owner retenu. Sur un CHANGEMAP qui appelle `EspNativeTransitionPresentation_endLoading()` plus tôt, les cartes non configurées restent différées ; pas de supposition que l'écran est encore caché. Un éventuel future L2 traitera le catalogue par carte et les timings de ces transitions.
- L'inventaire P0/P3 de SFX se fait avant l'audio et reste one-shot. Le service normal dans `main.cpp` reste inchangé et ne doit pas rescanner.

## Protocole CYD strict — `esp32-cyd`, plus `esp-cyd-inverted` facultatif

1. **START avec SOUND ON** : vérifier que le WAV intro /33 se lance avant `INTRO1` et se ferme avant `TRANSITIONLOAD BEGIN`.
2. Pendant le chargement Entrance : `LOADAUDIO-L0 ROUTE ... transport=deferred`, `TRANSITIONLOAD FRAME ... READY 100 %`, `ENGINECACHE FINAL-READY`, `LOADAUDIO-L0 BUDGET ... RELEASE`, `SFXP0 END`, `SFXP3 END`.
3. **Avant le tout premier gameplay visible** : `ENTRANCEMUSIC PLAY ... track=ENTRANCE gainDiv=15`, puis `[LOADAUDIO-L1] PRESTART ... ready=1 beforeWorldPresent=1`, enfin `[ENGINESESSION] FRESH-VISIBLE` et `READY`. Vérifier subjectivement musique synchronisée et maintien READY 100 % acceptable.
4. Après jeu pendant au moins 3 secondes : `underruns=0`, `recoveries=0`, `maxSilentFrames=0`, `SFX` mixés, `shapeData=0x0`, `mediaTexels=0x0`, `heap8` et `largest8` compatibles baseline.
5. **SOUND OFF** : `LOADAUDIO-L1 PRESTART ... ready=0 reason=SOUND_OFF`, premier frame immédiat sans attente de ~1 seconde, pas de timer.
6. Si possible **LOAD depuis menu** vers ENTRANCE, et LOAD/CHANGEMAP vers carte non configurée : vérification `deferred`, aucune piste M_STALKS sur une autre map, et aucune lecture musicale pendant SD→flash.
7. Régression de la barre de progression et du rendu : strictement identique, pas de skip FRAME ou flash de framebuffer.

CI : `ESP32/test/audio_loading_l1_ready_prestart_host.py` valide l'ordre EXACT depuis READY/HUD/SFX/lecteur jusqu'au premier present ; `audio_loading_l0_route_gap_host.py` conserve le routage C11 256 identifiants. Normal, inverted, bringup/trace et tests historiques des TU. **Pas de PASS matériel L1 avant ces logs.**

**Statut initial ci-dessus : protocole avant la capture. Le résultat CYD L1 est détaillé ci-dessous ; ne pas fusionner sans autorisation explicite.**


## Retour matériel du 17 novembre 2026 — PASS borné pour START / Entrance / flash HIT

**Source testée :** commit `2ce72ea0a4b9262aa0f114ae9d68df1a439b2655`, deux CI complets SUCCESS (pull_request `43984597205` et push `43984589319`). Capture utilisateur `platformio-device-monitor-261117-013031.log`, SHA256 `952d8df08dd923eb806a2be66baed926041e29b43cbcdf3fdfcbbcc8bd679008`, 614 lignes Serial, démarrage ESP32-D0WD-V3 4Mo sans PSRAM. Le log n'inclut pas la ligne PlatformIO d'environnement ; `alternate-panel inversion=off` est cohérent avec `esp-cyd-inverted`, **sans prouver formellement quel environnement a été flashé**.

- Intro : `M_IN_CIT.wav` démarre AVANT le premier frame intro (`INTROMUSIC PRESTART firstFrame=NOT_PRESENTED`), finit par `INTRO_EXIT_BEFORE_MAP` avec `underruns=0 recoveries=0 timer=OFF file=CLOSED`. En loading, `LOADAUDIO-L1 STOP_FOR_LOADING priorActive=0` confirme un état déjà fermé (ne prouve pas l'arrêt d'une musique active pendant CHANGEMAP).
- L0 : `ROUTE map=1 track=M_STALKS.wav transport=deferred`; `MAPFLASH REUSE HIT rebuild=no`; `BUDGET elapsedMs=5424 frames=8 maxVisibleGapMs=3493 ringBudgetMs=743 flashCallbacks=0 checkpoints=7 pakClosedCheckpoints=7 playback=deferred`, et `TRANSITIONLOAD RELEASE elapsedMs=5446 progress=100`. `3493ms` mesure des présentations d'UI, **pas** une famine DAC.
- Frontière L1 : `FRAME n=8 stage=READY overall=100` → `ENGINECACHE FINAL-READY` → `TRANSITIONLOAD RELEASE` → `SFXP0 END scanned=9 pcm=9` → `SFXP3 END scanned=83 pcm=83 totalOriginalWav=92` → `ENTRANCEMUSIC PLAY M_STALKS.wav gainDiv=15` → `LOADAUDIO-L1 PRESTART ready=1 elapsedMs=1092 beforeWorldPresent=1 queued=15784 ringBytes=16384 failClosed=0` → `ENGINESESSION FRESH-VISIBLE finalWorldPresent=yes` → `ENGINESESSION READY`. L'ordre est observé sur Serial ; l'absence de perception du palier 100 % reste subjective, non déclarée.
- Audio après entrée : jusqu'à `STREAM elapsedMs=90289 frames=1988289`, `minQueued=11928`, `underflows=0`, `recoveries=0`, `maxSilentFrames=0`, `gapsOver90us=0`, `maxReadUs=4279`; `SFXP1 MIX 5060` deux fois pour deux pickups. Pas de panic, WDT ou backtrace dans la capture.
- RAM : `RAMBUDGET HEADROOM_OK margin8=25028` avant préstart, puis `ALIVE heap8=60708 largest8=45044` constants pendant gameplay, et `shapeData=0x0 mediaTexels=0x0` au boot/résident. Aucun ZIP runtime ni nouvelle tâche audio témoigné.

**Limites strictes :** seul chemin testé = nouveau jeu, SOUND ON, Entrance, flash REUSE HIT. Ni SOUND OFF, ni LOAD depuis sauvegarde, ni passage à une autre carte, ni flash MISS/SD→flash, ni fermeture de la musique du niveau au teardown ne sont enregistrés. La capture finit au bout de 118334ms uptime **en plein gameplay** ; aucun `ENTRANCEMUSIC FINAL` de la piste Entrance à revendiquer. Aucune comparaison visuelle de progression sur l'écran n'a été jointe. Le statut **PASS CYD borné** signifie que le comportement principal L1 et la continuité audio observée fonctionnent sur ce parcours, pas que toute la matrice du protocole est validée. Commit post-test strictement documentaire ; **pas de changement de code sans nouveau test CYD**.
