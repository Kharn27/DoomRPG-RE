# GAME-COMPLETE-A0 — corpus original vérifié (18 novembre 2026)

Complément du [rapport initial](GAME_COMPLETE_A0_AUDIT.md). Les trois fichiers utilisateur ont été examinés localement en lecture seule. **Aucun binaire ni ressource du jeu ne sont commités dans GitHub.** Ce rapport est un inventaire de données, pas une validation matérielle de la campagne.

## 1. Intégrité : 241/241 ressources exactes

| Archive | SHA256 | Résultat |
| --- | --- | --- |
| DoomRPG.zip | 9e783fd6fae7185c2fd62f5578f3a8e58e7ca775e99780cd738a1030eac7baed | 241 entrées, 14 BSP, ZIP testé |
| DoomRPG-ESP32.pak | cd60f352962531cda0c3b6ffc08a0e02bb5d4a028ac5ab957f063b9c43952464 | PAK DRPGESP2/v2, 241 entrées, 2 457 398 octets |
| doomrpg.zip | b70a6d5fe55101f323a07202d8ea45101b0724c98e67bf3b590258fc4526341d | Archive BREW distincte, cinq entrées dont doomrpg.bar et doomrpg.mod |

PAK ↔ DoomRPG.zip : **241/241** hashes de noms FNV, longueurs, CRC32 et flags concordent ; les **payloads de chaque entrée** ont été comparés octet par octet sans différence. La fidélité est établie à **ce ZIP fourni**, non à toutes les éditions commerciales du jeu.

L'arbre ZIP contient **14 BSP** : les **13 cartes catalogue** plus un fichier spécial menu.bsp (nom interne DoomRPG, loadMapId=0, 53 nœuds, 120 lignes, 44 sprites, 15 événements, 15 commandes OPENLINE/CLOSELINE).

Structure des 13 BSP complète et cohérente avec esp_bsp_reader.c : en-tête 33 B, nœuds/lignes 10 B, sprites 5 B, événements 4 B, commandes 9 B, chaînes length-prefixed, blockmap 256 B, plans 2048 B. Aucune troncature, fenêtre de bytecodes hors borne, tuile événement en double ni donnée finale inexpliquée observée.

## 2. Inventaire intégral du catalogue

Les compteurs ennemis (type 1) et destructibles (type 12) proviennent des **115 définitions de entities.db**, via le décodage des sprites et la règle first-wins de esp_map_sprite_topology.c. « Suivis V11 » = somme de ces deux catégories, pas le nombre de monstres vivants ni la RAM totale.

| ID | Ressource (nom interne) | CRC32 | Nodes | Lines | Sprites | Events | Opcodes | Strings | Ennemis | Destruct. | Suivis V11 |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | intro (Entrance) | 623f34e4 | 223 | 480 | 344 | 93 | 265 | 94 | 30 | 13 | 43 |
| 2 | level01 (Sector 1) | f60dfb2b | 187 | 448 | 278 | 84 | 210 | 55 | 50 | 15 | 65 |
| 3 | level02 (Sector 2) | f39f0256 | 283 | 500 | 377 | 75 | 232 | 58 | 47 | 17 | 64 |
| 4 | level03 (Sector 3) | 08b56a9e | 209 | 524 | 342 | 97 | 254 | 73 | 52 | 27 | 79 |
| 5 | level04 (Sector 4) | 14e47619 | 287 | 572 | 408 | 84 | 265 | 71 | 37 | 54 | 91 |
| 6 | level05 (Sector 5) | 58fbefae | 335 | 592 | 504 | 99 | 364 | 124 | **85** | **48** | **133** |
| 7 | level06 (Sector 6) | 867c5466 | 325 | 638 | 336 | 134 | 388 | 85 | 43 | 22 | 65 |
| 8 | level07 (Sector 7) | 2ab2d682 | 349 | 676 | 354 | 73 | 290 | 39 | 58 | 12 | 70 |
| 9 | junction (Junction) | 4a2c5800 | 77 | 207 | 48 | 66 | 319 | 126 | 0 | 3 | 3 |
| 10 | junction_destroyed (Junction) | 04cf8f8a | 155 | 474 | 215 | 63 | 245 | 60 | 44 | 0 | 44 |
| 11 | items (Items) | 548142ea | 1 | 4 | 179 | 0 | 0 | 0 | 0 | 13 | 13 |
| 12 | reactor (Reactor) | 982c391d | 163 | 527 | 251 | 38 | 270 | 21 | 50 | 5 | 55 |
| 13 | endgame (Monster Cast) | 39999137 | 17 | 28 | 49 | 0 | 0 | 0 | 44 | 2 | 46 |
| **Total** | **13 BSP** | | **2611** | **5670** | **3685** | **906** | **3102** | **806** | | | |

**BSP spéciaux :** items et endgame possèdent loadMapId=0 et aucun événement. Leur structure n'est pas invalide ; ce sont des ressources différentes d'une carte de gameplay ordinaire. Les 44 définitions « ennemis » dans Monster Cast sont des références de sprites de la scène, **pas** une preuve d'ennemis actifs à combattre.

**Volumes :** plan immutable Sector 5 = **18 110 B**, Entrance = 14 095 B. La pression heap8 totale (audio, overlays, caches, monstres) n'est pas déductible de ces seules valeurs.

## 3. Graphe brut de campagne extrait des commandes de chaque BSP

**25 CHANGEMAP (opcode 2), 23 SAVEGAME (opcode 27), 19 couples orientés source→destination.**

| BSP source | Destinations présentes dans les commandes |
| --- | --- |
| Entrance | Junction (1) |
| Junction | Sector 1, 2, 3, 4, 5, 6, 7 (7) |
| Sectors 1 à 6 | Junction (2 sorties distinctes par secteur, 12 commandes) |
| Sector 7 | Junction (1), Junction Destroyed (1) |
| Junction Destroyed | Reactor (1) |
| Reactor | Junction Destroyed (1), Endgame (1) |
| Items et Endgame | aucune |

**Graphe de ressources uniquement.** Les états d'événements, clés, objectifs, filtres, actions intermédiaires et rollbacks peuvent empêcher l'exécution effective d'une arête. Les 2 commandes EV_CHECK_COMPLETED_LEVEL(39) de Junction sont dans les événements 43 (tuile 806) et 54 (tuile 868), chacune suivie d'une bascule de portes.

## 4. Corpus d'opcodes et mécaniques rares

**36 opcode IDs réellement présents** sur les 42 définis par le legacy (total 3102 commandes) : 1,2,3,4,7,8,9,10,11,12,13,15,16,18,19,21,22,23,24,25,26,27,29,30,31,32,33,34,35,36,37,38,39,40,41,42.

Les IDs 5, 6, 14, 17, 20 et 28 ne sont pas présents dans ces treize BSP ; cela ne démontre pas que les comportements sont absents du moteur original.

Principales fréquences : DIALOG(8)=533, CLOSELINE(16)=389, OPENLINE(15)=365, NEXTSTATE(19)=326, CHANGESTATE(11)=291, SHOW(7)=181, FORCEMESSAGE(24)=141, DIALOGNOBACK(26)=130, WAIT(37)=126.

**47 événements dépassent 12 commandes ;** record maximal Sector 7 = 60, Reactor = 55. La capacité ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS=12 borne un **plan filtré**, pas nécessairement tout l'événement : classer ces cas comme **à vérifier** et non comme bugs acquis.

**Corpus rare :** Reactor contient ACTIVE_PORTAL(38) 1 fois, GOTO(1) 45 fois et WAIT(37) 69 fois. Sector 7 possède 15 GOTO, 21 WAIT et transformations de sprites/décors ; Sector 6 comporte 2 TRIGGER(3) et 134 événements. Les scripts complexes de Reactor et Sector 7 deviennent prioritaires pour A1.

## 5. Findings vérifiables : priorité de complétude

### GC-A0-F1 — Sector 5, capture SAVE V11 au-delà de 100 entrées

- Données : level05.bsp CRC 58fbefae, **85 ennemis + 48 destructibles = 133** entrées suivies. Décodage identique à esp_map_sprite_topology.c et entities.db.
- Contrat natif : ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_MAX=**100** dans include/esp_map_sprite_topology.h.
- Code actuel : EspMapSpriteTopology_snapshotMonsters parcourt **toutes** les définitions de type enemy ou destructible, y compris mortes/masquées, puis retourne 0 à partir de la 101e, avec SNAPSHOT-FAILED reason=capacity.
- SAVE V11 dans esp_native_gameplay_save.cpp appelle ce snapshot obligatoirement, refuse l'enregistrement sur faux et imprime V11-CAPTURE-FAILED stage=monster-topology.
- **Statut : FAIL-CLOSED structurel démontré** pour le checkpoint V11 Sector 5 ; **aucun test CYD** de ce parcours. Les 85 monstres seuls sont sous leur capacité séparée de 100. Prochaine preuve A1 : exécuter le vrai propriétaire snapshot en hôte et vérifier une sauvegarde contrôlée avant de choisir une représentation compacte compatible V11.

### GC-A0-F2 — Reactor→Endgame hors contrat de transition native

- Données : reactor.bsp événement **23**, tuile **658**, commandes CHANGEMAP vers **/endgame.bsp** puis OPENLINE, SHOWSTATS=1. Les **deux** CHANGEMAP du Reactor sont des commandes sans SAVEGAME.
- Données cible : endgame.bsp CRC 39999137, **loadMapId=0** et pas d'événement ; nom Monster Cast.
- Contrat natif : esp_native_gameplay_transition.c exige sur la route SELECT filtrée **SAVEGAME puis CHANGEMAP**, éventuellement suivi de portes, alors que l'événement Reactor ne contient pas SAVEGAME. De plus esp_map_transition_preflight.c exige un gameplay loadMapId **1..32**, ce qui exclut Endgame=0.
- Oracle legacy : Game_changeMap dans src/Game.c sélectionne **MENU_MAP_STATS_OVERALL** lorsque SHOWSTATS cible MAP_END_GAME, au lieu de traiter cela comme une transition gameplay ordinaire.
- **Statut : deux incompatibilités statiques de contrats** ; la branche SELECT actuelle ne sait pas accepter ce motif, et son préflight gameplay ne saurait accepter la cible finale. L'invocation exacte dans un playtest reste **UNKNOWN**. Il faudra traiter la fin comme un milestone spécifique, sans réintroduire Game desktop.

### GC-A0-R3 — scripts longs/conditionnels : UNKNOWN

47 événements >12 commandes et forte concentration de GOTO/WAIT sur Sector 7/Reactor. Réutiliser les véritables filtres et API natives pour déterminer les suffixes éligibles, pas débloquer massivement les opcodes.

### GC-A0-R4 — budgets et reprises hors Entrance : UNKNOWN

Sector 5 compte 504 sprites ; Sector 6, 638 lignes et 134 événements ; Sector 7, 676 lignes. Aucun profil heap8 avec audio actif sur ces parcours n'a été généré ici. Vérifier LOAD V11 et états retirés, portes, monstres, mort, flash MISS/REUSE, EXIT et nettoyage.

## 6. Roadmap recalée et contrôle de portée

**A0** : inventaire réel + rapport, tests hôte de fichiers artificiels, zéro changement du firmware et **aucun flash**. Le code de l'analyseur distingue désormais les BSP spéciaux de la plage de loadMapId du préflight, compte les entités via entities.db et compare tous les payloads ZIP/PAK.

**A1a** : figer des témoins structuraux (CRC + arêtes + corpus) contre parsers C natifs réels. **A1b** : oracle événement réel Reactor 23 + branche MENU_MAP_STATS_OVERALL. **A1c** : oracle compilé du snapshot V11 Sector 5 avec 133 définitions. **A2+** : corrections **séparées**, bornées, CI quatre firmwares, flash esp32-cyd et Serial réel avant déclaration de PASS matériel.

Hors scope conservé : wrappers RC9, RES60/263, audio/261, barils/371. **Aucun asset commercial commité : les trois SHA256, CRC BSP et outils suffisent à la traçabilité.**
