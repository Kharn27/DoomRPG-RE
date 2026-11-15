# AUDIO-R1 — preuve analogique PCM GPIO26 sur la nouvelle CYD (EN ATTENTE du test réel)

**Issue :** [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261). **Base exacte :** `839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1` (vrai main du 15 novembre 2026). Branche : `agent/esp32-audio-r1-gpio26-pcm-analog`.

## Frontière

- Un probe one-shot **sur le firmware normal `esp32-cyd`**, après activation effective de la première session de gameplay ; l'intro/menu et `EspNativeAudioIntent` restent silencieux et inchangés.
- I2S0 master/DAC intégré : PCM 16 kHz, transport **deux slots 16 bits par frame**, APLL obligatoire. **Seul le DAC2 GPIO26** est activé (le nom IDF est `I2S_DAC_CHANNEL_LEFT_EN`), **aucun appel `i2s_set_pin(NULL)`** pour éviter DAC1/GPIO25, qui pilote l'horloge tactile logicielle XPT2046.
- Buffers bornés : 4 × 160 frames DMA dans le pilote, 160 × 2 × 16 bits = **640 octets** sur la pile de la tâche audio core0 de 3072 octets. Table sinus 256 octets read-only. Aucune lecture SD/PAK, aucun mutex concurrent ou bloc WAV pendant R1 ; **le futur streaming SD n'est pas testé**.
- Le DAC reste coupé pendant 8 blocs de warm-up + 80 blocs de mesure de la **cadence réelle de vidange DMA**. Garde [15000, 17000] Hz, échec **fail-closed** ; évite la régression de cadence ~5,5× du précédent prototype.
- Séquence après clock PASS : **2 s DAC OFF**, puis **2 s DAC GPIO26 ON au midpoint 0x8000** (observer pop/craquement), **4 s sinus 440 Hz à ±4 codes DAC**, **4 s au même 440 Hz à ±8 codes**, **1 s midpoint**, puis DAC OFF + libération du pilote ; fondus de 20 ms aux limites de chaque sinusoïde.
- Probe limité à la **première session** du boot. Quitter gameplay demande un arrêt audio asynchrone (la tâche coupe le DAC). Pas d'option persistante ou nouveau fichier sur SD.
- Préflight avant installation : heap8 ≥61440B, largest8 ≥24576B, DMA ≥32768B, largestDMA ≥16384B. Après installation et avant création tâche : heap8 ≥49152B, largest8 ≥16384B, DMA ≥16384B, largestDMA ≥8192B. Le probe saute au lieu d'amoindrir le moteur de jeu si l'une des marges manque.

## Tests attendus

1. `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor` sur la **nouvelle CYD** avec HP 8 Ω, **volume prudent, haut-parleur éloigné de l'oreille**. Rien à copier dans `/doomrpg/music/` : les fichiers musicaux ne sont pas utilisés pour R1.
2. Démarrer START/intro/Entrance. Le probe démarre seulement quand `ENGINESESSION` est réellement actif. Pendant les 15 secondes de mesure/écoute, bouger, tourner et utiliser le tactile pour vérifier l'absence d'interférence.
3. Envoyer le log Serial depuis boot et le ressenti séparé sur les étapes `DAC-OFF`, `MIDPOINT-GPIO26`, `SINE-440Hz-PEAK4`, `SINE-440Hz-PEAK8`, `MIDPOINT-TAIL` : silence, pop, souffle/grésillement, pureté de la sinusoïde, niveau et réactivité tactile. Arrêter le test si le son est agressif.
4. Vérifier les lignes `[AUDIOR1] PREFLIGHT/START/STAGE/PULSE`, cadence réelle proche de 16000, `errors=0`, `DONE` final, `[ALIVE]`, pas de reboot/WDT/panic, puis gameplay, EXIT et retour menu, maintien shapeData/mediaTexels NULL.

**PASS logiciel n'implique pas PASS analogique.** Aucun résultat acoustique ni budget final n'est encore déclaré. Ne pas fusionner cette branche avant retour utilisateur et permission explicite. En cas de PASS, documenter les preuves matérielles ensuite (Markdown only).
