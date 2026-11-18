# GAME-COMPLETE-A0 — audit de complétude de campagne (18 novembre 2026)

**Base GitHub vérifiée avant branche** : [main b648b33b27dbefe28c7691ac4752456dc4f2e7b2](https://github.com/Kharn27/DoomRPG-RE/commit/b648b33b27dbefe28c7691ac4752456dc4f2e7b2). Branche dédiée : **agent/game-complete-a0-audit**.

**Mise à jour corpus original fourni :** [rapport réel des 13 BSP, 241 ressources et findings F1/F2](GAME_COMPLETE_A0_VERIFIED_CORPUS.md). **241/241 payloads PAK ↔ ZIP byte-exact**, 906 événements, 3102 commandes, 36 opcodes IDs. Deux incompatibilités statiques P0 documentées : V11 Sector 5 (133 entités suivies / cap100) et Reactor→Endgame (CHANGEMAP sans SAVEGAME + loadMapId0). **Aucun flash ou correctif firmware dans A0.**

**Décision A0 : NON DÉMONTRÉ que la campagne entière est terminable.** Ce n'est **pas** une preuve que la campagne est bloquée. A0 n'ajoute aucun opcode, fallback desktop, allocation firmware, ni chargement ZIP au runtime. Il rend les trous de preuve vérifiables.

## 1. Sources, périmètre, provenance

1. Référence comportementale : [src/Game.c](../src/Game.c), [src/Game.h](../src/Game.h) (42 identifiants EV_ définis, 1 à 42) ; les sources desktop ne sont pas liées dans le firmware.
2. Identité des cartes : [esp_map_catalog.c](src/esp_map_catalog.c) et [esp_map_catalog.h](include/esp_map_catalog.h) (13 IDs natifs, pas une liste imaginée).
3. Ressources réelles : [build_asset_pack.py](tools/build_asset_pack.py) définit PAK **DRPGESP2/v2**, index trié par hash FNV, CRC32, payloads non compressés ; [esp_bsp_reader.c](src/esp_bsp_reader.c) définit le parseur sur fenêtres bornées. [esp_map_events.h](include/esp_map_events.h) définit les champs des descripteurs événementiels.
4. Contrats de progression : [esp_map_transition_preflight.c](src/esp_map_transition_preflight.c), [esp_map_change_map_state.c](src/esp_map_change_map_state.c), [esp_native_gameplay_transition.c](src/esp_native_gameplay_transition.c), [esp_native_gameplay_event_chain.c](src/esp_native_gameplay_event_chain.c), [esp_native_gameplay_save.cpp](src/esp_native_gameplay_save.cpp).
5. Preuves matérielles historiques : [PORTING_STATUS.md](PORTING_STATUS.md) et [DOCUMENTATION.md](DOCUMENTATION.md), notamment [PR #249](https://github.com/Kharn27/DoomRPG-RE/pull/249), [PR #253](https://github.com/Kharn27/DoomRPG-RE/pull/253), [PR #266](https://github.com/Kharn27/DoomRPG-RE/pull/266), [PR #373](https://github.com/Kharn27/DoomRPG-RE/pull/373).
6. Recensement des hôtes et workflow : [ESP32/test](test/) et [esp32-cyd.yml](../.github/workflows/esp32-cyd.yml).

**Provenance mise à jour** : les assets binaires ne sont **pas versionnés dans GitHub**, mais l'utilisateur les a ensuite fournis pour examen local. La comparaison indépendante des 241 payloads ZIP/PAK est exacte et les 13 BSP réels (plus menu.bsp) ont été décodés et recensés ; CRC par carte, statistiques, destinations, opcodes et deux findings source+code sont dans [le complément corpus](GAME_COMPLETE_A0_VERIFIED_CORPUS.md). Les trois fichiers restent hors Git pour éviter d'y déposer des contenus commerciaux.

## 2. Inventaire du catalogue et matrice par carte

**Légende** : « CYD PASS ciblé » atteste seulement les actions citées, jamais toute la carte ; « IMPLEMENTED / UNTESTED » désigne le code générique de catalogue/chargement sans preuve de parcours ; « UNKNOWN » désigne un verdict de progression impossible à établir. Les treize fichiers sont présents et structurés dans **le PAK fourni** ; la copie SD de chaque dispositif CYD et son profil mémoire restent à prouver matériellement.

| ID | Ressource réelle du catalogue | Preuve déjà présente | Progression complète / sortie |
| --- | --- | --- | --- |
| 1 | /intro.bsp (Entrance) | **IMPLEMENTED + CYD PASS ciblé** : START, mouvement, dialogues, portes, pickups, combat, mort/EXIT et LOAD V11 sur les routes testées | **PARTIAL** : tout le parcours/embranchement pas couvert |
| 2 | /level01.bsp (Sector 1) | **IMPLEMENTED + CYD PASS ciblé** : chargement V11 slot02 en session HUB (PR #266-B) et entrée directe depuis Junction (capture #253) | **UNKNOWN** : objectifs, scripts et sortie non prouvés |
| 3 | /level02.bsp (Sector 2) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 4 | /level03.bsp (Sector 3) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 5 | /level04.bsp (Sector 4) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 6 | /level05.bsp (Sector 5) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 7 | /level06.bsp (Sector 6) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 8 | /level07.bsp (Sector 7) | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 9 | /junction.bsp (Junction) | **IMPLEMENTED + CYD PASS ciblé** : Entrance→Junction avec stats/ACK, puis Junction→Sector 1, 2 transitions distinctes | **PARTIAL** : sauvegarder/reprendre ici et toutes les routes non prouvées |
| 10 | /junction_destroyed.bsp | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 11 | /items.bsp | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 12 | /reactor.bsp | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** |
| 13 | /endgame.bsp | **IMPLEMENTED / UNTESTED** (route générique/catalogue) | **UNKNOWN** (conclusion jamais prouvée) |

La capture [PR #249](https://github.com/Kharn27/DoomRPG-RE/pull/249) documente **SAVEGAME(27)→CHANGEMAP(2)** depuis Entrance, MISSION COMPLETE, ACK, préflight spawn, commit Junction et marquage de complétion **après** commit. La capture du 6 novembre rapportée dans PORTING_STATUS (section #253) ajoute **Junction→/level01.bsp**, sans bilan. Le log du 8 novembre (#266-B) démontre également le chargement V11 slot02 Sector 1. Ces faits sont des **preuves ciblées sur des SHA anciens**, non un parcours complet sur le HEAD actuel.

## 3. Analyseur reproductible : produire les vraies lignes manquantes

Le nouveau [game_complete_a0_inventory.py](tools/game_complete_a0_inventory.py) est **host-only**, standard library et lecture seule. À partir du PAK natif fourni localement, il extrait pour **chaque carte du vrai catalogue** :

- présence PAK par hash FNV ; offset, taille, CRC et SHA256 ; contrôle de l'index PAK, des bornes et du CRC de chaque BSP ;
- nom/en-tête BSP, gameplay loadMapId, spawn, direction, camera, couleurs, volumes de nodes/lines/sprites/events/bytecodes/strings, estimation du **plan immutable** (ne prédit pas la RAM gameplay réelle) ;
- identifiants de textures de lignes et de sprites **bruts** (pas une classification fantaisiste monstres/objets), fréquence de chaque opcode 1..42 et ID inconnus ;
- tous les événements : tile, état initial, flags, fenêtre de commandes, opcodes et arguments hexadécimaux ;
- candidats de graphe SAVEGAME(27) et CHANGEMAP(2) : event/tile, index de chaîne, cible de string et éventuel ID catalogue, SHOWSTATS, spawnParam ; références de strings UI ;
- alertes : événement non trié/doublé, fenêtre hors borne, spawn invalide, octets résiduels, gameplay loadMapId hors préflight natif ;
- avec ZIP original **optionnel**, comparaison des **241** entrées nommées contre l'index PAK (hash/flags/taille/CRC et payload exact), découverte de BSP hors catalogue et des entrées sans correspondant.

Exécution **sur PC**, à la racine du clone local, avec vos fichiers **locaux** :

    python3 ESP32/tools/game_complete_a0_inventory.py \
      --pak /chemin/DoomRPG-ESP32.pak \
      --zip /chemin/DoomRPG.zip \
      --output /tmp/game-complete-a0-inventory.json --strict

Le ZIP ne sert **qu'à l'audit hôte**. Le firmware continuera à lire le PAK natif. Le JSON (qui inclut des arguments de script et des noms de ressources) reste local : **ne pas committer le JSON issu des assets**. En cas d'absence du ZIP, omettre l'option --zip ; les noms des fichiers hors catalogue ne sont alors pas récupérables depuis un index PAK qui ne stocke que les hashes.

**Important :** les arêtes extraites sont des **candidats structuraux**, pas un graphe de progression prouvé. Le filtre d'événements, les états persistants, les prérequis, les clés, les états de portes et les chaînes de commandes peuvent bloquer l'arête ; la graphie ne doit pas annoncer une carte « accessible » sans cet oracle supplémentaire. Les références de texte sont identifiées par indices et tailles, sans copier les dialogues protégés dans Git.

## 4. Matrice fonctionnelle transversale (preuves exactes)

| Famille | Code actuel / source | Statut A0 et limite |
| --- | --- | --- |
| Catalogue / chargement BSP / PAK | esp_map_catalog, esp_asset_pack, esp_bsp_reader, esp_map_runtime | **IMPLEMENTED + CYD PASS ciblé** sur Entrance/Junction/Sector 1 ; **UNKNOWN** sur les dix autres BSP |
| Filtrage, états, opcode simple | esp_map_event_filter, esp_map_script_state, esp_map_opcode_executor | **IMPLEMENTED + CYD PASS ciblé** pour dialogue→NEXTSTATE Entrance ; **PARTIAL** à l'échelle des opcodes de toute la campagne |
| Interaction et continuation script | esp_native_gameplay_event_chain (journal borné), esp_native_gameplay_dialog, esp_map_ui_intent | **IMPLEMENTED + CYD PASS ciblé** sur scénarios Entrance ; chemins complexes et commandes non admises **FAIL-CLOSED / UNKNOWN selon route** |
| Changement de carte, spawn, stats | esp_native_gameplay_transition, esp_map_transition_preflight, esp_player_spawn_state, esp_map_level_exit_stats | **IMPLEMENTED + CYD PASS ciblé** Entrance→Junction→Sector 1 ; pas de graphe complet |
| Monstres, combat, riposte, XP/drops | esp_native_gameplay_monster_*, esp_native_gameplay_player_resources | **IMPLEMENTED + CYD PASS ciblé** (Malwrath/Imp et quelques kills/drops dans logs) ; tous les types/cartes **UNKNOWN** |
| Portes, ligne mobile, cadenas | esp_map_line_state, esp_native_door_animator, gameplay action/transition | **IMPLEMENTED + CYD PASS ciblé** sur Entrance ; séquences multi-cartes **UNKNOWN** |
| Objets, inventaire, armes/munitions | esp_native_gameplay_pickup, interaction_inventory, player_resources, weapon | **IMPLEMENTED + CYD PASS ciblé** Entrance ; économie d'une campagne entière **UNKNOWN** |
| SAVE/LOAD V11 et restauration | esp_native_gameplay_save.cpp, save reprime, snapshots map/script/monster | **IMPLEMENTED + CYD PASS ciblé** sur slots Entrance et chargement Sector 1 ; toutes cartes, cycles SAVE/LOAD multi-états **UNKNOWN** |
| Mort et reprise | esp_native_gameplay_player_death, startup, save/hub | **IMPLEMENTED + CYD PASS ciblé** sur écran de mort/EXIT ; routes RETRY, LOAD-death et cohérence de reprise **UNKNOWN** |
| Cache SD→flash, RELEASE, reset | esp_asset_pack, resident lifecycle, transition handoff | **IMPLEMENTED + CYD PASS ciblé** sur REUSE HIT et EXIT ; **flash MISS en progression multi-carte : UNKNOWN** |
| Conclusion / fin de campagne | src/Game.c (oracle), endgame map, scène finale | **UNKNOWN**, aucun résultat de fin sur CYD documenté |

**Ne pas réduire cette matrice au code isolé** : un test hôte d'API prouve une propriété de cette API dans ses fixtures, pas un chemin complet avec contenu original, pression RAM et restauration multi-cartes.

## 5. Cartographie des tests existants à réutiliser

| Contrat couvert en hôte | Tests déjà dans ESP32/test | Ne prouve pas |
| --- | --- | --- |
| Spawn et identité pré-commit | map_spawn_preflight_host.c | Entrée dans tous les BSP réels |
| Stats, marquage complétion, idempotence et rollback de rejet | level_exit_completion_host.c | Toute la séquence en vrai gameplay |
| Exclusion du monde pendant MISSION COMPLETE | changemap_stats_present_owner_host.py | Chaque variante d'événement de sortie |
| Préconditions niveaux complétés | level_gate_native_host.c | Toutes les combinaisons de campagne |
| Script et automap rollback | script_startup_automap_host.c | Échec de lecture SD sur vrai CYD |
| Débit transactionnel/WAIT | paid_interaction_host.c | Tous les dialogues/scripts du PAK |
| Magasin et ressources | store_native_host.c | Économie du jeu jusqu'au boss final |
| Monstres, drops et tours | monster_active_sequence_budget_host.c, native_monster_drop_catalog_host.py, native_monster_turn_resource_chain_host.py | Combat complet de chaque type et campagne |
| Chargement, transitions UI et save | test_level_progress.c, main_load_selector_layout_host.py, native_loading_progress_scale_host.c | Matrice SAVE/LOAD×13 cartes×états |
| Outils de lecture d'assets | audio_sfx_p0_pak_inventory_host.py, native_wall_pak_provenance_host.py | Recensement des 13 BSP |

Le nouveau **game_complete_a0_inventory_host.py** teste sur fixtures binaires artificielles (5 cas) les descripteurs, la cible d'une arête, les textes indexés, la parité ZIP/PAK, CRC, troncation, invalidité d'en-tête et fenêtres de bytecodes incohérentes. **La CI teste aussi que les IDs de chargement 12..32 restent admis, que 33 est refusé et que `--strict` signale les destinations et indices texte invalides.** Ces tests ne sont pas un oracle gameplay sur les ressources originales.

## 6. Risques classés par impact vérifiable, non par supposition

1. **P0 — incompatibilités statiques identifiées (F1/F2).** Voir [corpus vérifié](GAME_COMPLETE_A0_VERIFIED_CORPUS.md) : Sector 5 V11 dépasse le snapshot topologie 100 (133 définitions suivies), Reactor porte deux CHANGEMAP sans SAVEGAME et Endgame loadMapId0 est rejeté par un préflight de transition gameplay. Les **13 BSP réels sont désormais inventoriés** ; l'accessibilité et l'exécution restent à prouver. Le préflight [esp_map_transition_preflight.c](src/esp_map_transition_preflight.c) admet un gameplay loadMapId **1..32** (`esp_map_transition_preflight.h`), distinct des 13 IDs du catalogue. L'analyseur suit cette borne pour éviter de fausses alertes sur les cartes spéciales. Les destinations, les scripts à branches et les opcodes rares doivent être censés **avant** de conclure à une incompatibilité.
2. **P0 — sorties multi-commandes et états conditionnels.** [esp_native_gameplay_transition.c](src/esp_native_gameplay_transition.c) admet le chemin SELECT ciblé SAVEGAME→CHANGEMAP avec préfixe porte ; [event_chain.h](include/esp_native_gameplay_event_chain.h) borne les plans à **12 commandes** et refuse ce qu'il ne peut garantir. Un événement de campagne utilisant un autre ordre/opcode n'est **pas nécessairement** exécutable. Vérifier contre les véritables scripts et leur filtre ; ne pas désactiver fail-closed.
3. **P0 — fin de jeu et check complétion.** [src/Game.c](../src/Game.c) utilise EV_CHECK_COMPLETED_LEVEL (39) et traite une fin spéciale ; [level_exit_completion_host.c](test/level_exit_completion_host.c) couvre les bits V11 valides et rejette les dépassements. L'articulation reactor/endgame/flags de complétion est **UNKNOWN**, non déclarée manquante.
4. **P1 — persistance après changements d'état.** Les snapshots V11 existent et des LOAD ont réussi, mais aucun parcours systématique save→transition→reload→replay ne couvre les treize cartes, états de lignes, événements retirés, monstre mort/drop, lieux et clés.
5. **P1 — cache flash et marge RAM.** REUSE HIT et resets sont observés ; le chemin MISS, les arènes/caches de chaque BSP et l'empilement audio+monstres+dialogues sur carte lourde restent à mesurer. **~59 Ko heap8 en gameplay avec audio** est une observation, pas un budget garanti.
6. **P1 — routes de mort/retry.** Des morts et retours EXIT sont observés ; reprendre depuis un état de jeu/death menu et recharger après le basculement de carte exigent une preuve séparée.
7. **Suivi hors périmètre.** Barils létaux [issue #371](https://github.com/Kharn27/DoomRPG-RE/issues/371), audio [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261), RES60 [#263](https://github.com/Kharn27/DoomRPG-RE/issues/263) et wrappers RC9 **ne sont ni corrigés ni refactorés ici**.

## 7. Roadmap GAME-COMPLETE

**A0 (cette PR, host-only)** : inventaire structurel réel, égalité byte-exact ZIP/PAK, types d'entités via entities.db, matrice et findings F1/F2 publiés, tests négatifs sans assets dans la CI. **Limite ouverte** : oracles C natifs sur le contenu réel et preuve matérielle des transitions et sauvegardes ; aucun asset privé en CI.

**A1a — oracles des ressources déjà recensées** : utiliser les vrais BSP localement et comparer les valeurs BSP/CRC/plan mémoire à EspBspReader C en hôte ; tester la route Reactor→Endgame et le snapshot topologique de Sector 5 avec les vrais owners. FAIL si ressource manquante, collision, bytecode hors borne, cible inconnue ; ne conclure « progression bloquée » que pour un cas démontré.

**A1b — oracles de progression script/transition** : construire un corpus d'événements réels par famille et des fixtures de sauvegarde minimales ; tester filtres d'état, prédicats/clefs, SAVEGAME→CHANGEMAP, stats ACK, spawn et chemins d'échec transactionnels. Ne pas franchir un fail-closed sans scénario source + oracle.

**A1c — SAVE/LOAD, mort et stabilité** : pour états représentatifs de chaque région, tests reproductibles d'identité du monde mutable avant/après SAVE/LOAD, contenu des drop/monstres/objets et complétion ; test de répétition EXIT→START/LOAD et état après death-retry. Cadrer les mesures RAM par source réelle.

**A2+ — corrections ciblées uniquement après défaut avéré** : une famille/régression par PR, oracle original + native + mutations adversariales, quatre firmwares en CI, scénario **esp32-cyd** précis si code runtime changé, puis Serial matériel réel et documentation **docs-only** après PASS.

## 8. Gate de clôture et test matériel

**A0 : aucun flash demandé**. La PR ne doit modifier aucun fichier ESP32/src, include, platformio.ini, renderer, audio, PAK runtime ou firmware. Vérifier que seuls outil, test, workflow CI et ce rapport changent. La CI doit exécuter les quatre tests synthétiques sans assets et les builds usuels. Une validation complète de campagne nécessitera ensuite un journal carte par carte avec pré/post fingerprint d'état, chemin d'entrée/sortie, sauvegarde/reprise, heap8/largest8, shapeData/mediaTexels=NULL, absence panic/WDT et retour menu.

**Conclusion :** les données des **13 BSP** sont vérifiées et révèlent deux incompatibilités statiques P0 à tester et corriger séparément. La preuve matérielle fonctionnelle reste limitée à **Entrance, Junction et une entrée Sector 1** : la campagne entière et sa conclusion ne sont pas encore validées.
