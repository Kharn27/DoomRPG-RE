# AUDIO-MONSTER-SFX-ALL + dialogue music — PASS CYD borné

Base de travail : PR #365 en draft, branche `agent/audio-menu-m0-dm2ttl`. Le vrai main GitHub a avancé indépendamment après ARCH-RC5 : aucun merge automatique de cette branche.

## Défauts matériels identifiés

Capture CYD `platformio-device-monitor-261117-022205.log` :
- Musique Entrance pendant dialogues : `underflows=192871 recoveries=3 maxSilentFrames=133242` (~6.0 s de silence). Le dialogue garde un bail logique PAK pour sa police ; le producteur SD était bloqué malgré la présence du PAK en flash interne.
- Avant LOAD, un tir sur caisse lance `[SFXP1] MIX id=5046`. Après LOAD, deux tirs sur le Malwrath de mType 6 publient `attackSound=5046-deferred`, sans mélange SFX. Trois salves du Malwrath portent `sound=deferred`.
- La musique d'accueil préstart avant le premier `MAINOPAQUE READY` est, elle, confirmée sur M1 ; cette preuve n'est pas une validation de l'ensemble audio.

## Correctif technique proposé dans la PR

- **Pas de cas monstre individuel** : le tableau des 14 familles × 12 événements sonores est transcrit **à l'identique** depuis `src/EntityMonster.c` (168 IDs, 0 = silence légitime). Un oracle compare chaque valeur au legacy.
- Attaque de chaque famille : `mType + alternateAttack`, émission sur le rythme de présentation `pose+150ms`, après frame réussie, aucune nouvelle consommation RNG gameplay. Les 3 tirs d'un Malwrath passent le même mécanisme que les autres.
- Tir du joueur sur monstre, douleur et mort : publication après **commit** sans son en cas de rollback ; les effets de douleur/mort utilisent les choix déjà calculés par le moteur natif, avec cadence RNG conservée.
- Le propriétaire `EspNativeAudioIntent` conserve une FIFO statique de **16 événements** pour que tir+douleur, ou plusieurs tirs de monstres, ne s'écrasent pas. Overflow explicite ; aucun malloc, taille de l'ancien dernier-intent inchangée.
- Lecteur **3 voix PCM16 génériques** conservées en BSS. Toute piste PAK mono 8k PCM16 valide (famille 5042..5138) est resamplée vers le ring DAC 22050 par morceaux ≤8192 frames, même si son WAV dure plus que les 16K frames du ring musical. Lecture PAK en flash, pas d'ISR I/O, pas de buffer wave map-wide.
- Le producteur musical SD n'est plus arrêté simplement parce qu'un dialogue garde une **lease logique PAK flash** : il vérifie le backing réel `physicalUsesMapFlash` et l'absence de source probe SD. Les prêts sur PAK SD continuent de bloquer le producteur. Aucune double tâche SD ou accès ZIP.
- Pas de modification des combats et projectiles : hooks exclusivement audio post-commit/post-presentation. Les déclencheurs sight/idle gardent la sémantique native existante ; aucune nouvelle mutation RNG n'est fabriquée pour ces événements.

## Test CYD obligatoire

Environnement normal `esp-cyd-inverted` ou `esp32-cyd`. Vérifier SOUND ON, M_DM2TTL avant premier affichage et une fois seulement, puis :
1. Entrance, dialogue scientifique de plusieurs secondes : la musique **ne doit pas couper**, et les prochains STREAM doivent afficher `underflows=0 recoveries=0` si le backing est bien flash. Tester un dialogue long, sans cliquer immédiatement.
2. Flingue contre caisse puis Malwrath : les deux doivent produire le son 5046, avec une éventuelle douleur 5099.
3. Charger la sauvegarde Malwrath, observer ses trois coups : `[MONSTERSFX] ATTACK ... id=5072` et `[SFXGEN] START id=5072`. Attaquer à son tour ; vérifier les sons `SFXP1` et `SFXGEN` sur la même session.
4. Quitter/revenir : aucune fuite de ring/PAK/SD/Timer, `shapeData=0x0 mediaTexels=0x0`. Surveiller `SFXGEN DROP`, `FAIL-TIMER`, panic/WDT et heap8.
5. D'autres monstres sont servis par les **mêmes 14 familles**, sans code ajouté au fil des rencontres ; validation statique du corpus ne remplace pas une écoute matérielle représentative.

**PENDING CI et PENDING nouveau test CYD. Ne pas fusionner sur le test précédent de MENU-M1.**


## Validation finale CYD — 17/11/2026 (PASS borné)

Log réel `platformio-device-monitor-261117-221926.log` : 1683 lignes, 238313 octets, SHA256 `b0892983d52d6fba51f4fe455fad883fd2ae8c1a02b9f6465d14ca70fbf8ab83`. Branche audio effectivement testée `033617a3691b9384c1ec20acedf092783c9b644a`, issue du rebase ARCH-RC7 ; puis intégration sans perte sur BOOT-LEGALS main `51208686828b5887db54c00d2cc378010b1ad1ac` dans `30dd0f4f68bd79cd5ae16be78fe7b0795b460390`. Cette dernière combinaison nécessite CI (pas de second test physique spécifique du rebase).

Scénario : menu titre avec `M_DM2TTL.wav` (préstart avant première présentation), LOAD valide slot04, Entrance, Malwrath/Pinky/monstres supplémentaires, tirs, ripostes, effets de douleur/mort, retour menu. Sous `M_STALKS.wav` : FINAL 2454411 frames, ~111,3s, `underruns=0 recoveries=0 gapsOver90us=0 maxSilentFrames=0`, max SD read 4328µs, `catchUpBursts=12 maxPublishGapMs=530 gapsOver500Ms=1`, ring minimum STREAM 12589 frames. Contraste avec test 19:13 sur ancien code : `underflows=21159 recoveries=3` ; nouveau rattrapage validé en charge dans ce scénario.

Effets : **34** `[SFXGEN] START`, **34** `[SFXGEN] END`, **0** `[SFXGEN] DROP` ; un `[SFXP1] STREAM-FALLBACK id=5046 ... loss=no`. Malwrath `SIGHT id=5097`, Pinky `SIGHT id=5093`, 1 `SIGHT-HOLD`, six `MONSTERCOMBAT COMMIT` incluant plusieurs douleurs et morts. **0 `PAIN-HOLD` constaté**, donc sa garde 250ms n'est pas matériellement prouvée malgré les dommages non létaux. **35 `AUDIOINTENT OVERFLOW`**, sans DROP générique correspondant ; garder le diagnostic FIFO distinct de l'audio effectivement joué. L'utilisateur reteste à l'oreille et indique que tous les sons semblent présents et le résultat est satisfaisant.

Mémoire/teardown : `shapeData=0x0 mediaTexels=0x0`, `RAMBUDGET HEADROOM_OK`, heap8 observé ~59808B en gameplay, `SYSEXIT MENU-READY resident=empty`, `ENTRANCEMUSIC FINAL timer=OFF file=CLOSED`, aucun crash/panic/WDT. **PASS matériel borné** de la continuité musique + SFX de ce parcours. Non couvert : tous les monstres/cartes, écoute des 16 secondes du menu jusqu'à `END_ONCE`, pause prolongée en dialogue, PAIN-HOLD effectif ou nouveau firmware combiné BOOT-LEGALS. Les mentions PENDING précédentes décrivent les essais antérieurs et sont remplacées, pour ce scénario, par le PASS ci-dessus.
