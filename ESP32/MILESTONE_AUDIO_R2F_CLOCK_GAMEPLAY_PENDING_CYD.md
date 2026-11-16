# AUDIO-R2f — horloge de préflight robuste aux actions gameplay (CYD PENDING)

**16 novembre 2026.** Branche `agent/audio-r2f-clock-preflight-gameplay-safe` enchaînée **explicitement** sur [AUDIO-SFX-P0 PR #352](https://github.com/Kharn27/DoomRPG-RE/pull/352), commit source P0 validé `c7562863470f44fc040850b949fca2d6f429bfea` (la branche P0 reste NON fusionnée). Base de branche et contrat P0 intacts.

## Nouveau défaut matériel reproductible

Log exact `platformio-device-monitor-261116-015104.log` (deux parties sans redémarrage) :
- Première partie : l'utilisateur déplace le joueur pendant le préflight audio. Après `[ENTRANCEMUSIC] PREFLIGHT ... ringQueued=12246`, mouvement rendu (`[VIEWANIM] FRAME`, `[RA2B] FRAME`), puis `CLOCK measuredHz=18332 ticks=22050 elapsedUs=1202801 guard=20900..23200` et `FINAL reason=FAIL-CLOCK frames=0 timer=OFF dacPad=OFF file=CLOSED`. Le comportement *fail-closed* de R2e est correct pour une vraie horloge hors plage, **mais cette mesure est fausse** : le compteur ISR était plafonné à `kRate=22050` et la boucle principale n'a lu l'horloge que 202801µs plus tard. Le **dénominateur évoluait**, pas le numérateur.
- Deuxième partie : l'utilisateur attend pendant le préflight et trouve `CLOCK measuredHz=21950 ticks=22050 elapsedUs=1004531` puis `PLAY`, `STREAM ... underruns=0`, `FINAL ... frames=385506 (~17,5s) underruns=0 ... file=CLOSED timer=OFF`. Le WAV IMA n'est pas en cause.
- À P0 `[SFXP0] ... playback=no` signifie **aucun SFX audible, par conception**, même si les neuf WAV originaux PCM16 mono 8 kHz sont disponibles dans le PAK. Les marqueurs gameplay `sound=5046-deferred`, `sound=door-deferred`, `sound=deferred` sont attendus ; AUDIO-SFX-P1 activera le premier effet réel APRÈS correction de la musique.

## Correction minimale, sans autre changement d'audio

- L'ISR IRAM `onAudioTick` compte **chaque** tick de calibration, sans s'arrêter à 22050 : une seule incrémentation 32 bits bornée à UINT32_MAX, **sans lire d'horloge ni de SPI dans l'ISR**.
- Le service Arduino met en pause le timer, prend le **compteur de ticks ET le temps écoulé sur la même fenêtre**, et calcule `hz=ticks*1000000/elapsedUs` via `esp_audio_clock_guard.h` (C11 pur, pas d'allocation). Après déplacement pendant 200ms la calibration reste ~22,05kHz si le timer est sain. Une vraie horloge lente/rapide reste refusée sur **20900..23200Hz** ; timer pause erreur, zéro, overflow et timeout 1500ms restent fail-closed.
- **Aucune modification** à PCM, gainDiv14, soft-knee, ring16KiB, DAC2 GPIO26, IMA-ADPCM SD, allocations, autre profil CYD, ISR kPlay, ou rendu/gameplay. Aucun SFX encore audible à R2f ; le jalon suivant P1 est le mixage depuis PAK.
- Test host C11 `ESP32/test/audio_r2f_clock_preflight_host.py` (contrefactuel 18332 vs vraie mesure 22050, retard 200ms, fréquences hors plage, zéro/overflow), plus CI de tous les profils.

## Test réel nécessaire avant merge

Sur le CYD classic sans PSRAM, firmware normal `esp32-cyd`, démarrer Entrance et **avancer/tourner immédiatement** pendant la seconde de calibration. Attendu `[ENTRANCEMUSIC] CLOCK ... sampleMode=uncapped-interval` ~22050 avec `ticks` possiblement >22050, **puis** `PLAY` et des `STREAM/SIGNAL` avec `underruns=0`. Quitter HUB/EXIT, tester un deuxième Start Game sans attendre ; PAK/SFXP0 restent intacts, la musique devrait toujours démarrer. Envoyer le log Serial complet. Aucun PASS matériel n'est présumé.

**GATE : CYD PENDING ; CI à vérifier.** Après PASS, documenter strictement puis demander la fusion de P0 et R2f dans l'ordre ; ne jamais fusionner main automatiquement sans ordre.
