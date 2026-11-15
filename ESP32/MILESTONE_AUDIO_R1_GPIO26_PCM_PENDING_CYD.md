# AUDIO-R1 — DAC GPIO26 : R1d, comparaison A/B de la reconstruction du signal (PENDING CYD)

**Issue :** [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261). **Base exacte :** `839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1` (vrai main du 15 novembre 2026). Branche : `agent/esp32-audio-r1-gpio26-pcm-analog`.

## Frontière

- Un probe one-shot **sur le firmware normal `esp32-cyd`**, après activation effective de la première session de gameplay ; l'intro/menu et `EspNativeAudioIntent` restent silencieux et inchangés.
- I2S0 master/DAC intégré : PCM 16 kHz, transport **deux slots 16 bits par frame**, APLL obligatoire. **Seul le DAC2 GPIO26** est activé (le nom IDF est `I2S_DAC_CHANNEL_LEFT_EN`), **aucun appel `i2s_set_pin(NULL)`** pour éviter DAC1/GPIO25, qui pilote l'horloge tactile logicielle XPT2046.
- Buffers bornés : 4 × 160 frames DMA dans le pilote, 160 × 2 × 16 bits = **640 octets** sur la pile de la tâche audio core0 de 3072 octets. Table sinus 256 octets read-only. Aucune lecture SD/PAK, aucun mutex concurrent ou bloc WAV pendant R1 ; **le futur streaming SD n'est pas testé**.
- Le DAC reste coupé pendant 8 blocs de warm-up + 80 blocs de mesure de la **cadence réelle de vidange DMA**. Garde [15000, 17000] Hz, échec **fail-closed** ; évite la régression de cadence ~5,5× du précédent prototype.
- Séquence actuelle **R1c**, en reprenant la calibration de **la nouvelle carte** plutôt que les minimas hérités du CYD à réseau de résistances incorrect : **3 s DAC OFF**, **3 s DAC midpoint 0x8000**, **3 s 440 Hz ±8**, **1 s midpoint**, **3 s ±32**, **1 s midpoint**, **3 s ±80**, **2 s midpoint**, **3 s DAC OFF** tout en maintenant le DMA actif puis shutdown. Fondus 200 ms, plafond 80/127 (~63 %) du DAC pleine échelle pour ce jalon seulement ; aucun gain analogique ou réglage persistant. Les premières expériences R1/R1b (±4/8 puis ±4/12/24) avaient un niveau de sécurité très bas et **ne constituent pas la référence de volume optimale du nouveau circuit**.
- Probe limité à la **première session** du boot. Quitter gameplay demande un arrêt audio asynchrone (la tâche coupe le DAC). Pas d'option persistante ou nouveau fichier sur SD.
- Préflight avant installation : heap8 ≥61440B, largest8 ≥24576B, DMA ≥32768B, largestDMA ≥16384B. Après installation et avant création tâche : heap8 ≥49152B, largest8 ≥16384B, DMA ≥16384B, largestDMA ≥8192B. Le probe saute au lieu d'amoindrir le moteur de jeu si l'une des marges manque.

## Tests attendus

1. `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor` sur la **nouvelle CYD** avec HP 8 Ω, **volume prudent, haut-parleur éloigné de l'oreille**. Rien à copier dans `/doomrpg/music/` : les fichiers musicaux ne sont pas utilisés pour R1.
2. Démarrer START/intro/Entrance. Le probe démarre seulement quand `ENGINESESSION` est réellement actif. Pendant les 15 secondes de mesure/écoute, bouger, tourner et utiliser le tactile pour vérifier l'absence d'interférence.
3. Envoyer le log Serial depuis boot et le ressenti séparé sur les étapes `DAC-OFF` initial, `MIDPOINT-GPIO26`, `SINE-440Hz-PEAK8`, `MIDPOINT-GAP-8-32`, `SINE-440Hz-PEAK32`, `MIDPOINT-GAP-32-80`, `SINE-440Hz-PEAK80`, `MIDPOINT-TAIL`, `DAC-OFF-TAIL` : **le grésillement existe-t-il DAC OFF, DAC au point neutre, ou seulement pendant la sinusoïde ?** Arrêter le test si le son est agressif ou saturé.
4. Vérifier les lignes `[AUDIOR1] PREFLIGHT/START/STAGE/PULSE`, cadence réelle proche de 16000, `errors=0`, `DONE` final, `[ALIVE]`, pas de reboot/WDT/panic, puis gameplay, EXIT et retour menu, maintien shapeData/mediaTexels NULL.

**PASS logiciel n'implique pas PASS analogique.** Aucun résultat acoustique ni budget final n'est encore déclaré. Ne pas fusionner cette branche avant retour utilisateur et permission explicite. En cas de PASS, documenter les preuves matérielles ensuite (Markdown only).

## Retour CYD initial R1 (15 novembre 2026) — PASS numérique seulement

- Serial fourni directement par l'utilisateur : `measuredHz=16001` (16 kHz attendu), `writes=1388`, `errors=0`, `code=0`, `STAGE DONE` ; gameplay Entrance actif, pas de WDT/panic dans l'extrait et `shapeData=mediaTexels=0` au ready.
- RAM `[AUDIOR1]` : avant 84976 B heap8 / 61428 B largest8 ; tâche démarrée 78292 B ; après shutdown 84936 B : **40 B en dessous du préflight**, delta observé, pas une affirmation de fuite permanente.
- Retour d'écoute réel : tonalité entendue ; **légers grésillements parasites entre phases** et **contraste subjectif faible entre ±4 et ±8**. L'origine du grésillement **reste inconnue** (DAC gate/transitoires/quantification/ampli), ne pas l'attribuer au DMA avec le seul log.
- Conclusion stricte : transport numérique I²S + DAC GPIO26 **validé**, qualité analogique / volume / absence de parasites **non validés**. La version R1b revoit uniquement les paliers et les fondus pour distinguer cause et comportement ; nouveau retour matériel requis. **Ne pas merger.**

## Retour réel HP 8 Ω sur R1b / décision R1c (15 novembre 2026)

- L'utilisateur confirme que les parasites enregistrés dans sa vidéo **proviennent réellement du haut-parleur** et non du micro du téléphone. Le premier petit HP ovale livré avec ESP32-S3 n'a pas d'impédance connue.
- Remplacement par le **HP connu 8 Ω** sur R1b : tonalité **beaucoup plus faible**, grésillements à **niveau similaire** ; parasites donc relativement plus saillants. **Pas de mesure instrumentée du bruit** et **aucune preuve d'identité de niveau absolu** ; ne pas conclure trop vite à une cause électrique unique.
- Point important de l'utilisateur : les niveaux DAC ±4/8 puis ±4/12/24 avaient été choisis par prudence pour **l'ancien CYD avec mauvais réseau R7/R8 à 0 Ω** ; ce paramétrage ne doit pas être reconduit par inertie pour le **nouveau CYD au réseau apparent 47k/22k/47k/4.7k**. La R1c réévalue l'amplitude et isole DAC-OFF/midpoint : traitement **diagnostique** et non preuve de qualité analogique.
- Pour interpréter R1c : bruit en DAC OFF et midpoint suggère amplification/bruit/power hors signal utile mais ne prouve pas l'origine ; bruit uniquement en mode DAC ON ou corrélé aux touches implique une autre piste. Un test acoustique en aveugle du log n'est pas suffisant. Périmètre CI **software seulement**, **test réel R1c à venir** ; ne pas fusionner.

## AUDIO-R1d : bruit présent uniquement pendant les sinusoïdes (15 novembre 2026)

- **Observation discriminante R1c fournie par l'utilisateur** : les grésillements sont **audibles uniquement pendant les tonalités 440 Hz** ; les périodes DAC OFF et MIDPOINT (DAC ON à 0x8000) n'ont pas de parasite audible comparable. Ceci exclut l'hypothèse d'un simple bruit de fond permanent dans ces conditions, **mais ne distingue pas encore** quantification 8 bits, non-linéarité analogique, I²S underrun non observé, saturation ou autres phénomènes corrélés au signal. R1c à ±8/32/80 est légèrement plus fort, mais toujours grésillant avec HP 8 Ω.
- **R1d** : contrôle strict même DAC2/GPIO26, I²S0+APLL 16 kHz, gains/ram/gpio25 identiques ; premier A/B 440 Hz à ±64 codes DAC, ancien générateur LUT+troncature **BASE** 3 s, séparé par 1 s midpoint, puis **SHAPED** 3 s au **même niveau et fréquence**, interpolation linéaire Q16 de la table 256 + rétroaction d'erreur de quantification 1er ordre, aucun flottant/dither pseudo-aléatoire. Second A/B identique à 220 Hz pour contrôler l'effet de la fréquence. Fondus 200 ms. Fenêtres DAC-OFF 3 s initiale/finale, DAC midpoint 2 s en début/fin ; indépendamment de la musique, pas de lecture SD/PAK.
- **Limite** : ce générateur ne change pas la résolution physique du DAC 8 bits et peut ne pas améliorer les grésillements. S'il ne change rien, rechercher principalement autre défaut analogique/synchronisation ; un rendu audible amélioré suggérerait une composante due à la forme/quantification du signal, **sans prouver** que la cause est unique. Les 16 kHz sont la cadence de production, pas une mesure de qualité analogique. **CI et nouveau test réel R1d requis. Ne pas fusionner.**
