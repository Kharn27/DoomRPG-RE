# AUDIO-SFX-P2 — Extincteur complet et gain SFX par famille (CYD PENDING)

**16 novembre 2026.** En réponse au test matériel de P1 sur classic CYD, log `platformio-device-monitor-261116-121845.log` : dix SFX P1 audibles 5046/5060, zéro underrun musical, un DROP 5060 lors d'un dialogue, mais deux FIRE-COMMIT sans intention 5045. Le volume SFX restait faible à l'oreille.

- Le gameplay publie 5045 uniquement après un vrai FIRE-COMMIT ; aucun PAK ouvert dans le moteur gameplay.
- 5045.wav original : PCM16 mono 8000Hz, 6400 échantillons, 800ms. Le resampling 22050Hz produit **17640** échantillons, soit plus qu'un ring DAC 16384B. Une voix bornée continue donc le mixage par segments au fil des avancées du writer, sans charger le WAV entier ni modifier DAC2, timer, ISR ou source musicale SD. Test P2 C11 et CI intégrés, mais **validation matérielle encore PENDING**.
- Le diviseur de gain PCM SFX devient **320 pour 5046 (pistolet), 128 pour 5060 (pickup), 256 pour 5045 (extincteur)**, clamp DAC 16..240. Musique R2e inchangée. Comparaison auditive et saturation à vérifier.
- Une intention bloquée par un PAK temporairement ouvert (dialogue d'aide à la première acquisition d'arme) est reportée à la fermeture, avec `[SFXP2] DEFER`, plutôt que jetée immédiatement. Une seule intention récente, toujours fail-closed si source non validée.
- Exclusion des effets longs non supportés (dont 5061), sans allouer de heap, ring supplémentaire, tâche, SD SFX ni runtime ZIP.

**Test CYD requis sur `esp32-cyd`** : vérifier pistolet, ramassages et 2 feux à l'extincteur. Pour 5045 chercher `[SFXP2] CHUNK` puis `[SFXP2] MIX id=5045 totalMixed=17640`, sans `PARTIAL`; écouter l'équilibre et la saturation. Vérifier `[ENTRANCEMUSIC] STREAM underruns=0` et `FINAL reason=SESSION_EXIT`. Contrôler la fermeture du dialogue Fire Ext après un `DEFER id=5060` et la reprise `MIX id=5060`.
