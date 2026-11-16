# AUDIO-SFX-P1 — premiers SFX PAK PCM16 audibles mélangés dans R2e (CYD PENDING)

**16 novembre 2026.** Branche `agent/audio-sfx-p1-first-pak-mixed-sfx` **strictement empilée** sur [AUDIO-R2f #356](https://github.com/Kharn27/DoomRPG-RE/pull/356), lui-même sur [AUDIO-SFX-P0 #352](https://github.com/Kharn27/DoomRPG-RE/pull/352). **Pas de merge dans main** avant décision utilisateur. Deux résultats matériels déjà établis : neuf sondages PCM16 mono 8kHz PAK P0 et race d'horloge R2e (résolue en code R2f, non encore testée sur CYD).

## Une frontière très bornée

Premier effet natif **réel** depuis le backing store immuable `/DoomRPG-ESP32.pak` :

- **5046.wav** — son de tir/impact caisse depuis **commit réussi** de l'attaque native/crate (uniquement `crateWeapon->resourceId==5046` et outcome valide).
- **5060.wav** — son générique pickup après **commit** de la collecte native des ressources.
- Leurs formats/tailles exacts viennent du test P0 sur CYD : RIFF PCM mono 16 bits **8000Hz**, données à l'offset 44 ; 5046 = 4160 frames (~520ms), 5060 = 3520 frames (~440ms).
- Le gameplay publie un petit `EspNativeAudioIntent` **sans ouvrir le PAK** ; le service musique le lit ensuite dans la même tâche Arduino **après les ressources/rendus**.
- Le service n'accède aux sons que si `phase==kPlay`, PAK *vérifié dans le flash*, résident, et aucun bail déjà ouvert ; lit le RIFF puis le PCM par segments de **512 octets**, jamais les fichiers WAV complets, aucune lecture ZIP/SD pour les SFX.
- Resampling **nearest-neighbor 8000→22050Hz** entier dans la **tâche principale**, mélange saturé du PCM16 réduit (SFX/512) dans les **échantillons musicaux 8 bits déjà préparés**. La source music de R2e reste inchangée, ring 16 384B, horloge 22,05kHz, soft-knee et gainDiv14, DAC2 GPIO26 et le **code ISR ne sont pas modifiés**.
- Un effet doit tenir entièrement dans le ring en attente ; départ à **rd+1024 frames (~46ms)** au lieu de `wr` (ancienne latence 743ms), marge de collision ISR >=128 frames vérifiée régulièrement. Aucun second canal, aucun changement de sample rate matériel, aucune tâche, timer, heap ou buffer WAV plein. Une seule voix sonore : si un effet précédent est encore en cours, drop journalisé.
- **Fail-closed** pour formats autres que PCM16/mono/8k, IDs hors `{5046,5060}`, son trop long, buffer insuffisant, lecture flash impossible, pack indisponible ; jamais tenter les MIDI de musique legacy.
- Deux entrées SFX supplémentaires peuvent être sautées si rapprochées sous une voix occupée. Les autres effets legacy (portes, explosions 5061 de 1,7s, monstres, armes plus complexes) sont délibérément hors P1.
- Logs `[SFXP1] MIX/PARTIAL/DROP` enregistrent l'ID, format, nombre de samples, sources, latence, lectures, volume implicite et profil d'erreur.
- Host test C11 `audio_sfx_p1_pcm_overlay_host.py` montre la taille exacte et resampling pour 5046/5060, les refus pour explosion 5061, saturation DAC 16..240, original PAK, ordre commit→intention et absence de modification de l'ISR.

**Risques bornés et diagnostiquables :** le mixage réécrit des DAC8 déjà préparés juste devant le consommateur. Une image gameplay très lente peut rattraper la marge de 46ms ; le service interrompt alors le mixage avec `PARTIAL reason=reader-caught-up`, sans bloquer la musique. Les tests host prouvent les calculs/invariants, mais **la qualité audible, le délai exact et l'absence de glitches simultanés doivent être validés sur le CYD**. SFX P1 ne joue qu'avec la musique R2e déjà en phase PLAY (pas d'effets en menu/prologue, ni si fichier musical manquant).

## Test sur vrai CYD normal

Flasher la branche P1 via `esp32-cyd` (pas bringup), garder PAK et WAV SD intacts. Démarrer vite et **bouger pendant la calibration** pour valider R2f `CLOCK ... sampleMode=uncapped-interval` suivi de `PLAY`. Depuis Entrance, **tirer sur une caisse avec le pistolet**, puis ramasser l'objet obtenu, attendre environ une seconde entre les deux pour permettre d'entendre séparément 5046 et 5060. Chercher `[AUDIOINTENT] PUBLISH resource=5046/5060` et `[SFXP1] MIX id=...` (ou `DROP/PARTIAL` pour diagnostic). Vérifier le ressenti SFX, musique simultanée, `[ENTRANCEMUSIC] STREAM underruns=0`, RAM, `FINAL timer=OFF dacPad=OFF file=CLOSED`, re-Start Game après EXIT. Envoyer log complet et constat auditif (niveau des FX par rapport à musique, bruit/latence).

**CYD PENDING :** pas de PASS hardware inventé, pas de fusion sans ordre explicite ; prochain P2 étendra le catalogue seulement après mesure des vrais SFX.
