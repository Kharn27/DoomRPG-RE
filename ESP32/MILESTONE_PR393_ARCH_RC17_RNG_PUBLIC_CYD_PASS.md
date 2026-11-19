# ARCH-RC17 — RNG public byte/word sans wrappers : double CI + PASS CYD borné

- PR : [#393](https://github.com/Kharn27/DoomRPG-RE/pull/393)
- Base exacte de `main` au démarrage : `2e8f070cddbd4409c222e9d9c6f4fad4ec7e7193` (après #389)
- HEAD **code** validé (avant toute documentation) : `cc66779aa6856de3a57534d6f4ef2e2d4294ad80`.
- Double CI sur le code : [push #44521012191](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44521012191) **SUCCESS** ; [PR #44521021999](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44521021999) **SUCCESS**, incluant tous les profils et le test ARCH-RC17 C réel/12 mutants.
- Hardware : **classic CYD ESP32-2432S028R, ESP32-D0WD-V3, 4 MB flash, aucune PSRAM**. Capture utilisateur `platformio-device-monitor-261119-140017.log`, **2169 lignes**, **285191 octets**, SHA-256 `cc1d3296f9ce75b5cc33e2554ba98f07222e4b8b7be0cf96cc53b3705d6db480`.
- Verdict : **PASS matériel fonctionnel strictement borné** sur cette capture et retour positif utilisateur. Le Serial **n'imprime pas le SHA du firmware ni l'environnement PlatformIO exact** ; l'attribution de cette capture à ce HEAD code est donc contextuelle, non une preuve binaire indépendante.

## Frontière du changement

ARCH-RC17 supprime les deux interpositions GNU `--wrap=DoomRPG_randNextByte` et `--wrap=DoomRPG_randNextInt`. Les fonctions publiques réelles sont définies dans le propriétaire ESP32 `esp_native_rng_replay_guard.c` ; les corps de tirage sont conservés hors renommage. La structure `Random_t` de 132 octets, les états privés de refill, le `DoomRPG_setRand` native et les protections de lease/probe/rollback restent inchangés. Le moteur desktop sert seulement d'oracle hôte, non de moteur runtime.

Inventaire cible : **14 → 12 wrappers production/inverted/trace**, **17 → 15 bringup** (3 exclusivement diagnostiques). Le test différentiel C exécute le générateur legacy réel à travers 12 000 opérations, 1 024 tirages mixtes byte/word, comparaison des changements de tables/offsets/seed cachée, traversée 126→0 de mot 32 bits et rejeu après rollback sans double refill. Les **12 mutants C** attendus échouent ; les fichiers d'oracle historiques conservent leurs SHA immuables. Un commentaire P1 Codex sur le mutant `word-forget-lease` a été traité par le commit `8552b460c43df656a2d3cde2c3edcdf25dbc35ec` ; la CI finale sur `cc66779` est verte.

## Exercices constatés sur le CYD

1. Boot et nouvelle partie : INTRO puis MAP_INTRO / Entrance, chargement PAK/flash et HUD natif, état `shapeData=0x0 mediaTexels=0x0`. Le premier tour et le rendu sont fonctionnels.
2. Première session : **20 mouvements MOVE commit et 10 rotations TURN** au total sur les deux sessions, **31 VIEWANIM END**, **5 DOORANIM COMPLETE** dont **2 HOLD → RELEASE → POST-MONSTER-COMPLETE**. **16 DIALOG CLOSE** (6 reprises `DIALOGCHAIN RESUME`), **7 ramassages PLAYERRES COMMIT**.
3. Sortie depuis le HUB System : `SYSEXIT MENU-READY`, `RESIDENTRESET ... released=18008 ... empty=1`. Chargement natif **V11 slot04, 5 604 octets**, rétablissement positions/topologie/états/ressources, reprise gameplay visible.
4. Deuxième session : **2 MONSTERCOMBAT COMMIT**, **7 ripostes MONSTERRETAL COMMIT** et **1 riposte létale**, séquence ordonnée jusqu'à `MONSTERACTIVESEQ TERMINAL`; **12 MONSTER3GOAL COMMIT**. `PLAYERDEATH READY`, sélection MAIN puis deuxième `RESIDENTRESET ... empty=1` et `SYSEXIT MENU-READY`.
5. Audio/RAM : **30 SFXGEN START / 30 END**, **10 SFXP1 MIX**. Cinq `AUDIOINTENT AUDIT` affichent `queueOverflow=0`, `consumerCursorGap=0`, `audibleSfxDropped=0` et `pending=0`. Aucun `SFXGEN DROP`, aucun underflow/underrun/recovery non nul dans les checkpoints lus. **2× HEADROOM_OK**, heap8 de session observé autour de 56–60 KiB, largest8 45 044 B. Deux teardowns complets et retour à l'écran principal.
6. Aucun `panic`, Guru Meditation, WDT, FAILED, ou autre erreur fatale identifiable dans la capture. Plusieurs lignes de contrôle `shapeData=0x0 mediaTexels=0x0`.

## Limites précises

- La capture ne possède pas de témoin matériel dédié `RNGGUARD` et ne démontre pas explicitement toutes les frontières byte 127→0, word 126→0, expiration de lease, seed cachée ou rollback de faute injectée. **Ces contraintes sont couvertes par le différentiel C/CI**, pas déclarées validées sur le CYD.
- Ni sauvegarde V11 neuve, ni CHANGEMAP multi-niveaux, ni test inverted, ni panne SD artificielle, ni injection de rollback fautif sur le matériel dans cette capture.
- Le test audio ne mesure pas la perception acoustique : « zéro underflow/overflow mesuré » n'est pas la preuve d'absence d'artefact audible.
- Aucune modification audio, rendu, combat, monde, loot, PAK/ZIP ni mémoire structurelle n'a été faite dans RC17 ; les anomalies éventuelles hors de cette frontière doivent être traitées séparément.

## Discipline de livraison

Tout commit sur la branche après l'analyse de cette capture doit être **docs-only** et laisser le HEAD code `cc66779` intact. Recontrôler la CI finale et les changements avant fusion ; ne jamais présenter un test local firmware non exécuté comme validé. Après fusion de #393, repartir du SHA GitHub réel de `main` pour ARCH-RC18 ou la prochaine frontière cohérente.
