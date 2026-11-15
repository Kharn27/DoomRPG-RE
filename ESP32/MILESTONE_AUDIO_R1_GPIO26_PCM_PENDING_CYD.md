# AUDIO-R1 — DAC GPIO26 : cadence prouvée, calibration acoustique R1b en attente sur CYD

**Issue :** [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261). **Base exacte :** `839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1` (vrai main du 15 novembre 2026). Branche : `agent/esp32-audio-r1-gpio26-pcm-analog`.

## Frontière

- Un probe one-shot **sur le firmware normal `esp32-cyd`**, après activation effective de la première session de gameplay ; l'intro/menu et `EspNativeAudioIntent` restent silencieux et inchangés.
- I2S0 master/DAC intégré : PCM 16 kHz, transport **deux slots 16 bits par frame**, APLL obligatoire. **Seul le DAC2 GPIO26** est activé (le nom IDF est `I2S_DAC_CHANNEL_LEFT_EN`), **aucun appel `i2s_set_pin(NULL)`** pour éviter DAC1/GPIO25, qui pilote l'horloge tactile logicielle XPT2046.
- Buffers bornés : 4 × 160 frames DMA dans le pilote, 160 × 2 × 16 bits = **640 octets** sur la pile de la tâche audio core0 de 3072 octets. Table sinus 256 octets read-only. Aucune lecture SD/PAK, aucun mutex concurrent ou bloc WAV pendant R1 ; **le futur streaming SD n'est pas testé**.
- Le DAC reste coupé pendant 8 blocs de warm-up + 80 blocs de mesure de la **cadence réelle de vidange DMA**. Garde [15000, 17000] Hz, échec **fail-closed** ; évite la régression de cadence ~5,5× du précédent prototype.
- Séquence actuelle R1b après clock PASS : **2 s DAC OFF**, **2 s midpoint 0x8000**, puis **3 s sinus 440 Hz ±4**, **1 s midpoint**, **3 s ±12**, **1 s midpoint**, **3 s ±24**, **1 s midpoint** et arrêt du DAC ; fondus de 100 ms aux limites des sinusoïdes. Paliers limités à 24/127 du DAC pleine échelle. L'étape initiale R1 utilisait 4 s ±4 puis 4 s ±8 et des fondus de 20 ms.
- Probe limité à la **première session** du boot. Quitter gameplay demande un arrêt audio asynchrone (la tâche coupe le DAC). Pas d'option persistante ou nouveau fichier sur SD.
- Préflight avant installation : heap8 ≥61440B, largest8 ≥24576B, DMA ≥32768B, largestDMA ≥16384B. Après installation et avant création tâche : heap8 ≥49152B, largest8 ≥16384B, DMA ≥16384B, largestDMA ≥8192B. Le probe saute au lieu d'amoindrir le moteur de jeu si l'une des marges manque.

## Tests attendus

1. `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor` sur la **nouvelle CYD** avec HP 8 Ω, **volume prudent, haut-parleur éloigné de l'oreille**. Rien à copier dans `/doomrpg/music/` : les fichiers musicaux ne sont pas utilisés pour R1.
2. Démarrer START/intro/Entrance. Le probe démarre seulement quand `ENGINESESSION` est réellement actif. Pendant les 15 secondes de mesure/écoute, bouger, tourner et utiliser le tactile pour vérifier l'absence d'interférence.
3. Envoyer le log Serial depuis boot et le ressenti séparé sur les étapes `DAC-OFF`, `MIDPOINT-GPIO26`, `SINE-440Hz-PEAK4`, `SINE-440Hz-PEAK8`, `MIDPOINT-TAIL` : silence, pop, souffle/grésillement, pureté de la sinusoïde, niveau et réactivité tactile. Arrêter le test si le son est agressif.
4. Vérifier les lignes `[AUDIOR1] PREFLIGHT/START/STAGE/PULSE`, cadence réelle proche de 16000, `errors=0`, `DONE` final, `[ALIVE]`, pas de reboot/WDT/panic, puis gameplay, EXIT et retour menu, maintien shapeData/mediaTexels NULL.

**PASS logiciel n'implique pas PASS analogique.** Aucun résultat acoustique ni budget final n'est encore déclaré. Ne pas fusionner cette branche avant retour utilisateur et permission explicite. En cas de PASS, documenter les preuves matérielles ensuite (Markdown only).

## Retour CYD initial R1 (15 novembre 2026) — PASS numérique seulement

- Serial fourni directement par l'utilisateur : `measuredHz=16001` (16 kHz attendu), `writes=1388`, `errors=0`, `code=0`, `STAGE DONE` ; gameplay Entrance actif, pas de WDT/panic dans l'extrait et `shapeData=mediaTexels=0` au ready.
- RAM `[AUDIOR1]` : avant 84976 B heap8 / 61428 B largest8 ; tâche démarrée 78292 B ; après shutdown 84936 B : **40 B en dessous du préflight**, delta observé, pas une affirmation de fuite permanente.
- Retour d'écoute réel : tonalité entendue ; **légers grésillements parasites entre phases** et **contraste subjectif faible entre ±4 et ±8**. L'origine du grésillement **reste inconnue** (DAC gate/transitoires/quantification/ampli), ne pas l'attribuer au DMA avec le seul log.
- Conclusion stricte : transport numérique I²S + DAC GPIO26 **validé**, qualité analogique / volume / absence de parasites **non validés**. La version R1b revoit uniquement les paliers et les fondus pour distinguer cause et comportement ; nouveau retour matériel requis. **Ne pas merger.**
