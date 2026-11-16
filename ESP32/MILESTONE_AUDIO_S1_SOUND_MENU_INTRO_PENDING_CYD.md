# AUDIO-S1/I1 — SOUND ON/OFF, DAC idle dès MENU et musique M_IN_CIT pendant ST_INTRO (CYD PENDING)

**16 novembre 2026**, [issue audio #261](https://github.com/Kharn27/DoomRPG-RE/issues/261). Après fusion autorisée de [PR #358](https://github.com/Kharn27/DoomRPG-RE/pull/358) (main exact `d51afd5b72b98ecd549ffd9908b2f2e62c011bf4`), une seule branche nouvelle `agent/audio-sound-menu-toggle-idle-dac`, préparée pour le **firmware normal esp32-cyd**, **aucun PASS matériel sur cette branche à ce stade**.

## Frontière S1 : vrai sous-menu SOUND

Avant : Options -> SOUND ouvrait directement les 5 cartes de volume (BACK/25/50/75/100), runtime uniquement. Après : **Options -> SOUND -> BACK / ON (ou OFF selon état actuel) / VOLUME -> BACK / 25 / 50 / 75 / 100**.

- La carte ON/OFF affiche l'**état courant**, avec la même sélection en **deux taps relâchés** que la page Options ; sa confirmation bascule ON ↔ OFF. BACK retourne à Options. La carte VOLUME ouvre **le vrai écran 5 cartes préexistant**, avec les mêmes 25/50/75/100 et deux taps ; son BACK et sa confirmation retournent désormais à **SOUND**, non directement à Options.
- UI bornée à la colonne de droite, même framebuffer 160x120, même modèle natif `MENU_MAIN_OPTIONS`, aucun nouvel objet desktop, aucun framebuffer ni texture.
- Nouveau `EspNativeAudioIntent_isSoundEnabled/setSoundEnabled` : **ON par défaut au boot**, **OFF coupe complètement le lecteur de musique**, indépendant de `musicVolumePercent` qui garde sa valeur sélectionnée. Ces deux réglages sont **session-only** ; la config native SD V3 (12 B) ne change pas, le prochain reboot remet SOUND ON et music 100 %.
- Laisser DAC2/GPIO26 **ON au point numérique 128** dès `setup()`, sans timer ni ring ni lecture SD : `[AUDIOIDLE] ON dacPad=ON code=128 timer=OFF buffer=0`. Quand SOUND OFF, passer au milieu puis désactiver DAC2 : `[AUDIOIDLE] OFF dacPad=OFF`. Ce maintien de pad ne fait **pas** démarrer plus tôt la musique ; il évite seulement d'activer tardivement l'étage de sortie. Pendant la lecture, l'ISR 22050Hz reste seule propriétaire du DAC.

## Frontière I1 : morceau d'intro natif

- Lorsque la vraie clock `ST_INTRO` est active, le lecteur 22.05kHz choisit `/doomrpg/music/opl/M_IN_CIT.wav` (fallback `/M_IN_CIT.wav`) au lieu de `/doomrpg/music/opl/M_STALKS.wav` (fallback `/M_STALKS.wav`).
- **Le fichier M_IN_CIT n'est PAS dans Git/PAK** : le placer sur la microSD à `/doomrpg/music/opl/M_IN_CIT.wav`. Format exigé exactement **WAV IMA-ADPCM mono 22050Hz**, RIFF+fmt+fact+data, blockAlign <=1024 (généralement 1024) et samplesPerBlock correspondants, vérifié par le même parseur natif fail-closed que M_STALKS. S'il manque ou a un format inconnu : `[ENTRANCEMUSIC] SILENT reason=missing_file|invalid_wav`, **pas** de crash, pas de lecture ZIP.
- Le lecteur réutilise **strictement le même** ring 16384B, les mêmes buffers bornés, la même ISR DAC2, les mêmes garde-mémoire et clock/preflight mesurée. Il n'y a **qu'une seule piste à la fois**. Les intentions SFX PAK sont désactivées pendant le morceau de l'intro.
- **Important frontière SPI/RAM** : après le tap `[INTROIN] FINAL-CONTINUE` (clock déjà parkée), en tout début de l'itération suivante **avant** `Esp32IntroClock_service()`, `EspEntranceMusic_stopIntroAtHandoff()` libère le timer, la SD `File` et les 16KiB avant `Esp32IntroClock_park("intro-exit-ready")`, et donc **avant** le chargement de `/intro.bsp`, la préparation de la flash map, le cache et la transition. Log `[INTROMUSIC] HANDOFF musicClosed=yes mapLoaderMayStart=yes`.
- Après l'intro, un vrai `M_STALKS.wav` distinct est rouvert à l'entrée dans Entrance, avec le même preflight timer 1s. **La musique ne démarre pas encore pendant l'écran de chargement** : cette option serait un jalon distinct nécessitant une preuve de budget RAM et d'ownership du lecteur lors des phases flash / BSP. Ne pas la mélanger avec S1/I1.
- Le DAC maintenu ON ne retire pas les ~1-2 s de startup musical post-load : clock calibration, priming SD, et scheduling sont conservés.

## Non-régressions et validation

- Aucun changement de jeu, géométrie, rendu/animation, ZIP runtime, native config V3, musique R2e gainDiv=14, gain SFX G1, compteur `SFXGAIN`, ring 16KiB, timer, ISR, occupation SD/PAK pendant gameplay ni deux builds normal/inverted (même code).
- Tests hôtes C11 : SOUND 0/1, invalid-state fail-closed, volume retenu OFF puis ON, reset d'intentions indépendant. Tests statiques : UI SOUND 3 cartes + 5 cartes, peinture de rail, pas de nouveau menu legacy, DAC point silencieux sans timer, WAV intro exact, libération pré-BSP, séparation SFX/intro, API existante de M_STALKS inchangée. CI sur 4 profils.
- **Test CYD normal à effectuer** : depuis MENU_MAIN, Options -> SOUND -> vérifier état ON -> double-tap OFF -> revenir en arrière -> double-tap ON ; ouvrir VOLUME -> choisir 50 % -> vérifier retour à SOUND puis Options, EXIT ; vérifier `[AUDIOSWITCH]`, `[AUDIOIDLE]`, `[OPTIONSSOUND]` et cohérence volume au démarrage du jeu. Avec `M_IN_CIT.wav` conforme sur SD, Start Game : morceau en ST_INTRO, `[ENTRANCEMUSIC] PLAY ... M_IN_CIT.wav`, `FINAL reason=INTRO_EXIT_BEFORE_MAP`, `[INTROMUSIC] HANDOFF`, puis `[BSPREAD]`, `[MAPFLASH]` et enfin `PLAY ... M_STALKS.wav`. Vérifier `underruns=0`, heap8/largest8 et le gameplay normal avec SFX. Faire aussi une séquence Sound OFF -> Start Game -> intro silencieuse -> Entrance silencieuse. Sur `esp32-cyd-inverted`, même logique audio doit compiler/être identique.
- Si la musique de l'intro est absente : le test des gardes/menu reste possible, mais l'audio I1 n'est pas matériellement validé. Si le démarrage pendant les animations de l'intro perd des frames ou la RAM, rollback et isoler un milestone de service.

**Statut : PENDING test réel sur CYD. Ne PAS fusionner avant le retour matériel.**

### Audit architectural SDL-F8 préservé

Le vrai `native_intro_input.c` et les autres unités `native_intro_*.c` sont restés byte-exacts à leur SHA d'avant ce milestone. Le premier essai d'appel direct depuis `native_intro_input.c` a été **retiré** après détection du garde `sdl_f8_native_intro_no_sdl_host.py`. Le raccord définitif est uniquement dans `ESP32/src/main.cpp` : `EspEntranceMusic_stopIntroAtHandoff()` **avant** `Esp32IntroClock_service()`. Le nettoyage ne s'exécute que si la piste intro a réellement été sélectionnée **et** `Esp32IntroClock_isActive()==0`; donc jamais pendant la narration encore active. La clock parkée autorise ensuite le début de la procédure d'initialisation BSP sans musique ni ring. L'audit SDL-F2 des 14 appels `present()` est préservé via `presentSoundUiFrame()` partagé entre les deux écrans SOUND.

## AUDIO-I2 — volume spécifique de M_IN_CIT, après retour CYD (PENDING)

Retour matériel du 16 novembre 2026 : `platformio-device-monitor-261116-231505.log` contient le premier vrai démarrage `M_IN_CIT.wav` sous ST_INTRO, WAV IMA-ADPCM mono 22050 Hz, `frames=6176066`, `dataBytes=3098624`, `PLAY gainDiv=14`. Après ~19.1 s de lecture, `SIGNAL at=FINAL` relève `pcm16MinMax=-2915..7443`, `softKneeFrames=124957` pour `producerFrames=436774` (≈28,6 % des frames produites ; ratio indicatif car premier bloc peut être partiellement ignoré par le flux), `dacWrittenMinMax=18..239`, **underruns=0**, **gapsOver90us=0**, `recoveries=0`. Le ressenti sur le CYD est **une forte saturation de l'intro**, tandis que la piste Entrance `M_STALKS` utilise également `/14` mais présente `pcm16MinMax=-1668..1584`, `softKneeFrames=138`, `dacWrittenMinMax=23..232`, **underruns=0**. Cela n'indique pas un hard-clip du DAC 8 bits ; le limiteur soft-knee est simplement beaucoup plus sollicité et le niveau peut dépasser la tenue de l'étage de sortie.

**Correctif ciblé I2 sur cette même branche PR #360 :**
- `M_IN_CIT.wav` est maintenant décodé avec `kIntroGainDiv=32` en lieu de 14 : facteur linéaire `14/32=0,4375`, soit environ **−7,2 dB** **avant le soft knee**, ce qui réduit les échantillons comprimés, au lieu d'atténuer un signal déjà écrasé. Un éventuel écrêtage *à l'intérieur du fichier WAV* ne peut pas être réparé par ce gain.
- **`M_STALKS.wav` reste exactement `kEntranceGainDiv=14`**, ainsi que son ABI d'appel `EspEntranceAdpcm_decodeBlockObserved`, son fade 200 ms, son amplitude max DAC et sa plage 16..240 ; le menu SOUND/volume reste indépendant ; le gain SFX P2/G1 reste inchangé. L'ISR ne change pas.
- Les lignes `BLOCK0` et `PLAY` exposent maintenant `track=INTRO|ENTRANCE gainDiv=32|14`. Le compteur `softKneeFrames` mesure le seuil réellement appliqué pour chaque piste (et non systématiquement /14).
- Test C11 `audio_i2_intro_only_gain_host.py` : PCM16 synthétique de valeur 3000, sortie ancienne /14 = 238, nouvelle intro /32 = 221, soft-knee 9 -> 0, min/max PCM toujours 3000 et comportement d'ancienne API bit-identique. CI sur les deux builds et hôtes.

**À revalider sur le vrai CYD :** écouter `M_IN_CIT` au volume SOUND=100 %, lire `[ENTRANCEMUSIC] PLAY ... track=INTRO gainDiv=32`, comparer `softKneeFrames` et plages DAC de `SIGNAL at=STREAM/FINAL`, confirmer sortie pré-BSP, musique Entrance `track=ENTRANCE gainDiv=14`, SFX inchangés et 0 underruns. **Le fonctionnement de l'intro et son handoff a été observé matériellement ; le correctif de saturation I2 lui-même est PENDING CYD.**


## Retour matériel I2 : gain d'intro /32 amélioré, transport PASS (16 novembre 2026)

Le log normal CYD `platformio-device-monitor-261116-234711.log` démontre :
- `[AUDIOIDLE] ON dacPad=ON code=128` au démarrage ; `[INTRO1] READY` avant `[ENTRANCEMUSIC] OPEN` : **retard visuel encore observé**, c'est la raison du nouveau jalon I3.
- `[ENTRANCEMUSIC] PLAY ... M_IN_CIT.wav gainDiv=32 track=INTRO`, `pcm16MinMax=-2915..7443`, `softKneeFrames=26847` sur `producerFrames=624546` (~4,3 %, contre ~28,6 % à /14 lors d'un précédent parcours de durée différente), `dacWrittenMinMax=37..238` ; `underruns=0`, `maxSilentFrames=0`, `gapsOver90us=0`, `recoveries=0`. Retour auditif utilisateur : « L'intro c'est mieux » — amélioration **confirmée**, pas de déclaration de correction parfaite de toute saturation analogique.
- `[ENTRANCEMUSIC] FINAL reason=INTRO_EXIT_BEFORE_MAP` avec `timer=OFF`, `file=CLOSED`, `[INTROMUSIC] HANDOFF` **avant** `INTRODISP`, `TRANSITIONLOAD BEGIN`, `BSPREAD`.
- `[ENTRANCEMUSIC] PLAY ... M_STALKS.wav gainDiv=14 track=ENTRANCE`, `underruns=0`, `heap8=60744`, `largest8=45044`, `shapeData=0x0`, `mediaTexels=0x0` ; écran de transition huit frames et cache normal, mais **la musique Entrance commence encore APRÈS le début du loading**. Ce changement de timing n'était pas dans I2.
- Le log ne teste pas les cartes SOUND ON/OFF/VOLUME. **S1 reste PENDING** même si l'initialisation DAC au boot apparaît.

## AUDIO-I3 — départ de la piste avant la première image d'intro (CYD PENDING)

Le nouvel appel `EspEntranceMusic_prestartIntroBeforeFirstFrame()` est situé dans le propriétaire natif `DoomRPG_esp32ActivateMainMenuStart()` **après** `Esp32StoryFit_prepare()` mais **avant** `DoomRPG_esp32RenderFirstIntroFrame()` et son `DoomRPG_flushGraphics()`. L'audio utilise toujours le WAV IMA mono22.05, seul ring 16 KiB, timer/DAC2 préflight matériel inchangé, `/32` spécifique intro. Le flux est amorcé dans la même tâche Arduino pendant une fenêtre bornée à 1800 ms, par `EspEntranceMusic_service(false,true)` et `delay(1)` ; l'ISR ne change pas. Une seule `File` SD ; garde `EspAssetPack_isOpen()==0`, aucun usage du ZIP, pas d'autre propriétaire de SPI réentrant. La fenêtre `introPrestartWindow` identifie explicitement ST_INTRO avant que sa clock visuelle ne soit armée. `[INTROMUSIC] PRESTART ready=1 ... firstFrame=NOT_PRESENTED` doit désormais précéder `[INTRO1] Begin/READY` et `[INTROCLK] ARMED`.

Si SOUND est OFF, si le PAK est ouvert, si mémoire/WAV absent ou invalide, ou si la calibration dépasse le timeout : **l'image s'affiche quand même**, aucun démarrage tardif non voulu n'est initié dans ST_INTRO (latch fail-closed). Sur le vrai CYD, écouter si le passage menu → écran intro semble plus naturel ; vérifier 0 underruns/FRAME allocation 0 et `heap8` vs la baseline I2.

Conséquence explicite : jusqu'à ~1,8 s de préflight peuvent maintenant s'écouler **avant** la première image de l'intro, pendant que le menu reste affiché. C'est un échange intentionnel à valider à l'oreille et à l'œil ; si gênant, choisir ensuite une transition visuelle dédiée sans démarrer de deuxième ring ni altérer le scheduler natif.

Les fichiers `native_intro_input.c`, `native_intro_first_frame.c` et autres F8 restent bit-identiques ; `native_main_menu_start_action.c` conserve son oracle historique F29 via inversion exacte de **l'unique include et appel I3**. Nouveau test `audio_i3_intro_prestart_host.py` et CI multi-profils.

## Pourquoi la musique pendant loading n'est pas activée avec I3

Le loading/transition **n'est pas** ST_INTRO : il peut exécuter des accès SD→flash, effacer/copier/valider la map flash, lire du PAK et primer le cache. Le ring de 16 384 échantillons représente seulement ~743 ms à 22050 Hz. Le vrai chargement Entrance a duré ~5,48 s avec `TRANSITIONLOAD`, `BSPREAD`, `MAPFLASH` et `ENGINECACHE`. Précharger M_STALKS puis démarrer son ISR *avant le loading* sans service producteur sûr pendant les opérations SD/flash provoquerait des underruns et/ou de la contention SPI. Allouer un buffer de 5,5 s (~121 KiB) violerait les contraintes sans PSRAM. Ne PAS maquiller ça par un ring augmenté, ni accepter des sous-alimentations.

**Jalon séparé à concevoir :** musique par map explicitement choisie, les autres **deferred** ; identification de la route START/LOAD/MAP-TRANSITION et préflight source, petites phases de chargement bornées avec SD owner unique et service de refill uniquement quand le PAK/flash a relâché SPI, débit/horizons prouvés sur flash REUSE et MISS. En cas d'absence de piste sélectionnée, d'ON/OFF OFF, de budget insuffisant ou de longue phase non coopérative : comportement actuel deferred, sans underrun. Aucun `M_STALKS` pendant loading activé dans I3.

**Statut I3 : PENDING CYD ; ne pas fusionner PR #360 avant essai matériel du préstart et du sous-menu SOUND.**
