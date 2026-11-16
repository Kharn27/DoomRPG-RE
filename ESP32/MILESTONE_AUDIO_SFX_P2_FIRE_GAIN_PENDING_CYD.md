# AUDIO-SFX-P2 — Extincteur complet et gain SFX par famille (PASS CYD)

**16 novembre 2026.** En réponse au test matériel de P1 sur classic CYD, log `platformio-device-monitor-261116-121845.log` : dix SFX P1 audibles 5046/5060, zéro underrun musical, un DROP 5060 lors d'un dialogue, mais deux FIRE-COMMIT sans intention 5045. Le volume SFX restait faible à l'oreille.

- Le gameplay publie 5045 uniquement après un vrai FIRE-COMMIT ; aucun PAK ouvert dans le moteur gameplay.
- 5045.wav original : PCM16 mono 8000Hz, 6400 échantillons, 800ms. Le resampling 22050Hz produit **17640** échantillons, soit plus qu'un ring DAC 16384B. Une voix bornée continue donc le mixage par segments au fil des avancées du writer, sans charger le WAV entier ni modifier DAC2, timer, ISR ou source musicale SD. Test P2 C11 et CI intégrés, et **validation matérielle P2 PASS** sur `eb0ab2f1cbc20a8e50ea762bbca9c79e479049f0` avec le log Serial utilisateur décrit ci-dessous.
- Le diviseur de gain PCM SFX devient **320 pour 5046 (pistolet), 128 pour 5060 (pickup), 256 pour 5045 (extincteur)**, clamp DAC 16..240. Musique R2e inchangée. Comparaison auditive et saturation à vérifier.
- Une intention bloquée par un PAK temporairement ouvert (dialogue d'aide à la première acquisition d'arme) est reportée à la fermeture, avec `[SFXP2] DEFER`, plutôt que jetée immédiatement. Une seule intention récente, toujours fail-closed si source non validée.
- Exclusion des effets longs non supportés (dont 5061), sans allouer de heap, ring supplémentaire, tâche, SD SFX ni runtime ZIP.

**Test CYD requis sur `esp32-cyd`** : vérifier pistolet, ramassages et 2 feux à l'extincteur. Pour 5045 chercher `[SFXP2] CHUNK` puis `[SFXP2] MIX id=5045 totalMixed=17640`, sans `PARTIAL`; écouter l'équilibre et la saturation. Vérifier `[ENTRANCEMUSIC] STREAM underruns=0` et `FINAL reason=SESSION_EXIT`. Contrôler la fermeture du dialogue Fire Ext après un `DEFER id=5060` et la reprise `MIX id=5060`.

## Retour matériel réel — PASS P2, limites explicites

Firmware normal `esp32-cyd`, source exacte `eb0ab2f1cbc20a8e50ea762bbca9c79e479049f0`; double CI code **SUCCESS** : [PR](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43860394116) et [push](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43860383855). Log Serial intégral collé par l'utilisateur dans la conversation le **16/11/2026**, retour auditif explicite : « J'ai le son de l'extincteur ».

- `[ACTIONENGINE] FIRE-COMMIT seq=91/95/99` suivi de `[AUDIOINTENT] PUBLISH ... resource=5045` pour les trois feux ; trois `[SFXP2] MIX id=5045 outputFrames=17640 totalMixed=17640 gainDiv=256`, mixage incrémental via plusieurs `CHUNK` sans `PARTIAL` observé. Borne flash peak par chunk `mixUs` ~9 936 µs initial.
- Ramassages `Got Fire Ext` et `Got Axe` : `[SFXP2] DEFER id=5060 reason=pack-lease-open` pendant dialogues PAK puis `[DIALOG] CLOSE ... packClosed=yes` et `[SFXP1] MIX id=5060` ; ordre rétabli et pas de `DROP` dans la séquence fournie.
- Musique jouée concurremment, `[ENTRANCEMUSIC] FINAL reason=SESSION_EXIT frames=1829217 underruns=0 recoveries=0 gapsOver90us=0 maxSdReadUs=4298` ; `timer=OFF dacPad=OFF file=CLOSED`. Durant partie, `heap8` min 57180 B, `largest8=45044`; après session `heap8After=127080`. Invariants legacy `shapeData=0x0 mediaTexels=0x0` maintenus lors du teardown.
- Encore **non validés** : le niveau subjectif final de chaque effet par rapport à la musique, les effets des ennemis, le son `5081` des attaques encaissées, plusieurs voix simultanées et les longs effets. `5081` est déjà publié par la couche gameplay, mais reste rejeté implicitement par la liste fermée P2. Ce n'est pas un défaut DAC/horloge.

**Conclusion : PASS hardware P2 pour extincteur complet, PAK defer→pickup et transport musique coexistants.** Cette mise à jour est uniquement documentaire après validation, aucun changement du firmware code testé. Après fusion, repartir du SHA réel de `main` sur la seule branche de travail de [l'issue audio globale #261](https://github.com/Kharn27/DoomRPG-RE/issues/261).
