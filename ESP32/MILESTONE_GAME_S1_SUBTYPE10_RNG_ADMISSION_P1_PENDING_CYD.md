# GAME-S1 subtype10 P1 — RNG/admission et comportement sans réanimation (EN ATTENTE DE CYD)

Issue [#438](https://github.com/Kharn27/DoomRPG-RE/issues/438). Base `main=d9896fcf5f07ab8d41935565002792b608a41333` (22 novembre 2026).

## Oracle legacy

`src/Entity.c::Entity_aiThink` réalise, uniquement si le monstre est immobile sur sa destination :
1. efface le flag « raising » de l'attaquant ;
2. tire **exactement un octet RNG** : `<84` ouvre la recherche d'une cible, `>=84` n'inspecte pas la liste ;
3. parcourt `inactiveMonsters` dans l'ordre circulaire natif au legacy, avec distance au carré non nulle et au plus `0x4000`, cadavre marqué `0x1000000`, non réservé `0x2000000` et absence d'occupant selon le masque **16263** ;
4. sur candidat, réserve attaquant et cible, fait `Entity_attack` avec arme **18**, puis `Combat_monsterSeq` joue la séquence et vérifie de nouveau l'occupation au stade final avant de restaurer vie/armure et l'activation ;
5. sans candidat, inverse **une fois** `0x800000` et poursuit la visée/mouvement/attaque normale.

Aucun tirage supplémentaire n'a lieu pendant l'itération des cadavres. L'action de réanimation et son propre sonore **5124** ne sont pas équivalents à un soin ou à une attaque directe immédiate.

## Contrat P1 borné

Le natif inspecte l'octet RNG **sans l'avancer** en préflight, avec réservation de refill via le propriétaire `RngReplayGuard`. Si un candidat potentiel reste admissible, aucune tentative de réanimation partielle : `RAISE-DEFER`, RNG logique **inchangé**, pas de mutation de cible, membre omis explicitement. La liste circulaire inactive, les bits réservés, la fin d'animation, le link/activate et l'update V11 doivent avoir un propriétaire natif avant d'ouvrir ce chemin.

Si aucun candidat n'est trouvé : un seul tirage réel (<84 ou >=84), alternance du flag d'attaque persistée dans le petit owner `EspNativeGameplayMonsterState` (FNV et snapshot V11 inchangés), puis traitement existant `MonsterTurn` + `Movement` + `Retaliation` + `AttackVisual`. Les cas `MEMBER_INVALID` restaurent RNG et alternance ; les échecs extraordinaires de commit/refill doivent être testés avant gel.

La recherche P1 de cadavres est conservatrice : les cadavres reconstruits depuis SAVE et les états death4 peuvent être classés comme candidats potentiels et *déférés* plutôt que considérés sûrement non réanimables. Le modèle de liste inactive n'est pas simulé en triant les sprites. Pas de ZIP runtime, heap map-wide, réintroduction du desktop ou modification de `shapeData`/`mediaTexels`.

### P1 correction : chaines d'attaque ET de déplacement

L'audit croisé a détecté que l'implémentation initiale de P1 conservait deux
exclusions `subtype10` silencieuses dans `MonsterMovement` et
`MonsterMovementPublish`, malgré le retour dans le séquenceur. **Ne jamais
assimiler un build vert à une IA rétablie.** Les deux exclusions sont désormais
retirées pour les tours préalablement admis sans réanimation ; les masques
originaux `subtype=10` restent appliqués. L'oracle hôte interdit le retour
de ces deux gardes et les tests SDL-F26 continuent à restaurer les blobs
historiques stricts par delta réversible. La validation réelle de la
transaction par le CYD reste nécessaire.

## Validation

`python3 ESP32/test/monster_subtype10_admission_host.py` : oracles C purs + audit du véritable code de raccordement. CI et build `esp32-cyd` à vérifier avant test matériel. **Aucun PASS CYD n'est revendiqué ; P1 n'achève PAS l'issue #438.**

Tests physiques attendus dans Sector 1 : sprite149 vivant sans cadavre, turns et alternance, attaque/mouvement, consommation de l'octet spécial, puis cadavre dans les deux cases (RAISE-DEFER strict), SAVE V11 et LOAD, traces `MONSTERSUB10`, `MONSTERTURN`, `MONSTERRETAL`, `RAMBUDGET`, audit de `shapeData/mediaTexels=NULL`.

P2 pour clôturer #438 : propriétaire compact de liste inactive (ordre et sauvegarde), flags de réservation et de non-réanimation, attaque 18 complète/animation/audio 5124, vérification d'occupation *à la résolution*, restauration HP/armure et topologie, test oracle de plusieurs cadavres, rolls aux frontières du RNG, LOAD et rollback intégral.
