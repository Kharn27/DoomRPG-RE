# Audit de consolidation architecturale ESP32

Base vérifiée : `9d203e525a4b8736fadfd34aaf693423bff7f602` (main après PR #208).
Audit du 13 octobre 2026. Classic CYD, sans PSRAM. Ce rapport n'attribue aucun
nouveau PASS matériel et ne clôt aucune anomalie gameplay du statut courant.

## Conclusion

La fragmentation existe, mais le nombre de fichiers n'est pas le bon objectif.
Le port contient à la fois des propriétaires natifs justifiés, du scaffolding
inaccessible, des compatibilités ABI nécessaires et des adapters fonctionnels
encore nommés « probe ». Les fusionner en bloc rendrait l'architecture pire.

Premier jalon retenu : retirer **la fermeture historique post-load Junction**,
neuf sources et neuf headers sans consommateur extérieur ni section allouée
dans les deux profils. Pas de fusion d'états, pas de nouveau module production,
pas de modification d'un fichier de gameplay conservé.

Suite sur la même branche depuis `9cde9d5` : **lot 2 graphique ZIP** et remise
en état des trois fixtures hôte. Le compteur courant devient **158 sources /
152 headers include**, contre 183/174 à la base. Les inventaires CSV restent
des snapshots du main initial ; les preuves du nouveau lot figurent ci-dessous.

## Méthode et portée des preuves

Les trois documents de référence, `platformio.ini` et les 1 190 lignes de
`scripts/build_engine.py` sont relus sur cette base exacte. Les builds normal et
bringup sont reconstruits avant toute suppression. L'inventaire est croisé avec :

1. `pio run -e <profil> -t compiledb` : unités **admises par SCons**, pas les
   vieux objets qui peuvent subsister dans `.pio`.
2. `xtensa-esp32-elf-nm -S --defined-only` sur chaque objet et ELF final : les
   macros de renommage des wrappers sont ainsi prises en compte.
3. `firmware.map` : attribution des sections d'entrée aux objets, seulement
   quand elles ont une adresse de mémoire chargée et une taille non nulle.
   Les sections à adresse zéro/discarded et le debug ne prouvent pas du linkage.
4. Includes et références de symboles dans sources, headers, tests et build :
   absence ELF seule n'autorise **pas** à retirer un header, un guard ou un ABI.
5. Comparaison stricte avant/après des sections ELF non vides `SHF_ALLOC`, y
   compris adresses, flags, alignement, taille et contenu ; NOBITS par layout.

Le CSV [architecture_inventory_9d203e5.csv](architecture_inventory_9d203e5.csv)
donne une ligne pour chacune des **183 sources de départ**. Il reste un snapshot
de cette base, pas un compteur automatique du HEAD après retrait. Les octets
de sections d'entrée ne sont pas une mesure parfaite de flash (padding/linker).
La colonne de données statiques décrit **l'objet**, pas la RAM finale : elle
peut inclure des tables et des symboles éliminés. Les classifications sont une
orientation d'audit, non un relevé d'exécution de chaque fonction.

Le second CSV [architecture_headers_9d203e5.csv](architecture_headers_9d203e5.csv)
recense les **174 headers** et leurs includers lexicaux à cette même base, y
compris tests et références desktop. Un include source ne prouve pas une
inclusion préprocessée active ni un appel lié ; plusieurs contrats de frames
survivent alors que leurs anciennes fonctions ne sont plus liées. Les neuf
headers du lot choisi n'ont d'includers qu'à l'intérieur de leur fermeture.

Un symbole lié n'est pas nécessairement appelé. Une branche testée sur CYD ne
prouve pas l'exécution de toutes ses compatibilités. Les logs matériels restent
l'autorité d'exécution ; aucun nouveau trajet CYD n'a été observé ici.

## Inventaire chiffré à la base

| Mesure | Production | Bringup |
| --- | ---: | ---: |
| Sources C/C++ présentes dans `ESP32/src` | 183 | 183 |
| Sources directement compilées dans ce dossier | 182 | 182 |
| Copies générées compilées | 3 | 3 |
| Sources originales desktop directement compilées | 0 | 0 |
| Sources du dossier apportant des sections allouées | 147 | 150 |
| Directives projet `--wrap` | 48 | 55 |
| Wrappers projet effectivement définis dans l'ELF | 46 | 49 |
| RAM statique, relevé PlatformIO | 47 680 B | 47 832 B |
| Flash, relevé PlatformIO | 802 633 B | 820 313 B |

Les 183 sources = 172 C + 11 C++. Les **174 headers** de `ESP32/include`
représentent 10 986 lignes ; huit headers supplémentaires vivent dans `src`.
Les sources représentent 81 806 lignes ; l'attribution par sections identifie
**146 sources communes** (71 940 lignes), quatre spécifiques bringup et une
spécifique production. `nm` montre aussi trois wrappers de logs SDK dans chaque
ELF ; ils ne sont pas comptés dans les wrappers projet du tableau.

Les **32 sources sans contribution allouée dans les deux ELF** se décomposent :
template SDL exclu (1), ABI de rejet à conserver (1), formatter d'un profil
diagnostic distinct (1), fermeture Junction retirée (9), autres candidats à
examiner (20). Ce ne sont **pas 32 modules morts à supprimer**.

Deux précisions de build essentielles :

- `esp32_sdl.cpp` est un template exclu de la compilation directe. Le supprimer
  casse le générateur ; sa copie packed/zero-copy est compilée et liée.
- Les copies générées sont `DoomRPG.c`, `Render.c`, `esp32_sdl.cpp`. Aucun des
  21 TU originaux desktop n'est directement compilé. Les 240 commandes de
  chaque compilation incluent aussi framework/libs ; ne pas les annoncer
  comme 240 modules du moteur.

## Cartographie des responsabilités à conserver

| Domaine | Propriétaires / contrats représentatifs | Frontière à préserver |
| --- | --- | --- |
| Storage/assets | `esp_asset_pack`, BSP reader, indexed BMP, graphics catalog | PAK SD/raw flash, leases, cache borné, pas de texels map-wide |
| Monde immuable et overlays | runtime, map/script/line/texture/automap state, sprite topology | identité de l'arena, snapshot/restauration, reset en ordre inverse |
| Joueur | PlayerState, PlayerView, fresh-map progress, spawn/facing/tile owners | valeurs persistantes vs pose vs effets post-spawn ; ne pas fusionner les lifetimes |
| Transactions d'action | dispatch, move events, event chain, action engine, destructible/store | préflight, journal lazy, rollback, publication seulement après réussite |
| Combat/monstres | state, activation, positions, movement/publish, active sequence, turn, retaliation | ordre actif, coup anticipé vs commité, RPG RNG vs effet visuel |
| Présentation | frame, first frame, dynamic lines, sprite/plane renderer, HUD, FX, menus/modal | framebuffer partagé, gates, durée d'animation, pas de mutation de monde par preview |
| Save/transition | save V11/multislot, route, reprime, transition/handoff | atomicité SD, formats V1–V10, teardown différé, pas de références de carte après reset |
| Plateforme/compatibilité | SDL packed, core stubs, Game/Canvas bridge, render startup, menu storage | shells minimaux/ABI explicites ; pas de restauration du moteur desktop |

La séparation immuable/mutable, le joueur vs la vue, le planificateur vs la
publication, la présentation d'attaque vs les dégâts et les parsers vs les
propriétaires sont **bonnes**. Leurs petits fichiers ne sont pas une anomalie.
92 unités avec une contribution production contiennent des données statiques
dans leur objet ; ce chiffre n'est pas « 92 propriétaires indépendants ».

## Là où la complexité est réelle

- **Wrappers superposés et noms trompeurs.** Treize aliases `#define __wrap_*`
  dans les headers transforment des anciens wrappers en feuilles privées.
  L'ordre public est partiellement caché par le préprocesseur/linker. La chaîne
  session est notamment FX → ressources → pickup/session, avec les monstres,
  rendu d'attaque, retaliation, mouvement et handoff autour de cette séquence.
  Remplacer progressivement par des noms/calls explicites est préférable à
  fusionner leurs états. L'ordre service et l'ordre reset doivent être audités
  **séparément** ; ils ne sont pas interchangeables.
- **« Probe » ne signifie pas diagnostic.** Sur 13 sources avec ce mot, deux
  sont des adapters production (`esp_asset_pack_perf_probe` : staging raw PAK,
  `monster_movement_probe` : réservation RNG/publication). Trois autres sont
  effectivement propres à bringup, huit n'apportent pas de section dans ces
  deux profils. Ne pas couper les deux adapters fonctionnels.
- **Petits modules avec vraie politique.** `HubActionGate` (38 lignes) empêche
  le feedback monde de restaurer des pixels sous le HUB/Automap ; mouvement
  activation (48 lignes) impose admission puis capture de publication ; save
  reprime (58 lignes) restaure deux propriétaires HUD. Ne pas supprimer ces
  comportements sous prétexte qu'ils sont courts.
- **Gros owners trop couplés.** ActionEngine 3 973 lignes, save 3 860, resident
  gameplay 2 385, move events 2 202 : la dette n'est pas seulement la quantité
  de fichiers. Resident inclut 64 headers, save 47, session 33. Clarifier API
  publiques/privées, composition et scratch partagé doit précéder une nouvelle
  découpe. Ne pas extraire un sous-owner rollback en changeant son lifetime.
- **Compatibilités dormantes mais utiles.** `esp_legacy_render_reject.c` ne
  contribue pas aux ELF observés, mais le générateur vérifie ses 14 endpoints
  et il rejette un futur appel interdit. Ce n'est pas un module mort ordinaire.
- **Diagnostic historique devenu peu atteignable.** Le pipeline de 15 sources
  graphiques ancien représente 6 043 lignes sans contribution dans les deux
  profils. Certaines déclarations de frames restent utilisées par les anciens
  caches/gates : retirer les headers en masse serait incorrect.
- **Génération devenue catalogue de migration.** Les 1 190 lignes du générateur
  mêlent gardes de dérive, slicing de Render, patch de DoomRPG et transformation
  SDL. Beaucoup de gardes sont une protection réelle, pas une dette à supprimer
  avec leur code. La répétition peut devenir déclarative, avec tests de copies
  générées identiques pour chaque profil.
- **Docs en décalage.** ARCHITECTURE §6/7/10/13 conserve des descriptions de
  pickups/CHANGEMAP/password/ZIP/Player désormais anciennes ; PORTING_STATUS
  contient plusieurs anciens « pending/merge à faire » suivis de PASS plus
  récents. La date/le SHA de chaque preuve doivent rester explicites. Ces textes
  ne sont pas un graphe d'appel et ne justifient pas de retirer un owner actif.

## Inventaire priorisé des suites

Les lots ne s'additionnent pas aveuglément ; certains partagent des contrats.

| Priorité / lot | Taille constatée | Bénéfice réel | Risque et preuve nécessaire |
| --- | --- | --- | --- |
| **P0 choisi : post-load Junction** | 9 C + 9 H, 1 691 lignes, 45 exports | supprimer une fausse architecture de démarrage et ses marqueurs map-specific | faible après fermeture et comparaison alloc stricte ; deux builds, CI, pas de PASS CYD exigé si code chargé identique |
| P1 : graphe documentaire courant | 3 documents structurants | une autorité actuelle, éliminer les contradictions qui conduisent aux faux retraits | docs-only ; confronter chaque claim aux code/Serial, ne pas effacer les limites |
| P1 : fixtures hors CI devenues obsolètes | 3 tests : SYS touch, level progress, death menu | retrouver une régression exploitable de ces frontières | déjà non compilables sur la base : mocks multislot/completion absents ; les réaligner sans modifier le firmware, puis CI |
| P1 : anciens diagnostics graphiques | 15 C, 6 043 lignes candidates | sortir un second renderer/scaffolding hors du moteur ou supprimer une fermeture devenue inutile | vérifier profils ciblés, typedefs de frames/caches, endpoints de probes ; pas de suppression globale proposée ici |
| P2 : directives de wrap sans endpoint vivant | 2 normales, 6 bringup (dont les 2 communes) | retirer de faux branchements de build, préciser les vrais adapters | vérifier appels `__real_*`, futures probes et les deux ELF avant/après ; no-op chargé requis |
| P2 : helpers map inutilisés et modèle de sortie parallèle | 4 C : key gate, password, line topology query, player exit | réduire des abstractions parallèles et contrats obsolètes | les types/constantes peuvent rester utilisés ; comparer avec action/password/transition actifs et tests d'événements avant retrait |
| P2 : aliases/chaînes session-action | 13 aliases header ; 48/55 directives globales | rendre service/reset et gate de présentation lisibles sans linker magique | élevé : ordre RNG, dégâts après idle, rollback et LOAD ; tranche d'une seule chaîne, tests hôte et PASS CYD ciblé si code chargé change |
| P3 : adapters fonctionnels mal nommés | 3 sources identifiées | noms reflétant raw-PAK, renderer context, mouvement/RNG | renommer sans supprimer comportement ; prototypes/flags/symboles à recenser |
| P3 : API pre-game | 7 C, 3 287 lignes, headers dispersés | contrats publics vs privés, éviter la duplication de checks | conserver model/help parser, callbacks et état de sélection distincts ; START/LOAD/Back/HUD boundary tests |
| P3 : générateur | 1 Python, 1 190 lignes | règles/CRC lisibles, moins de copier-coller de transformation | comparaison des sources générées + deux profils ; garder le fail-closed et le contrôle des 21 originaux |
| À conserver : parsers et petits states actifs | 16 sources ≤80 lignes au total, pas toutes candidates | localiser l'autorité et les transactions | aucune fusion proposée sur le seul critère de taille |

## Premier jalon : fermeture retirée

```text
post_load_givemap -> weapon_select -> initial_save_intent -> flag_cleanup
 -> event_particle_cleanup -> view_invalidation -> playing_transition
 -> idle_time -> native_playing_service
```

Les chemins `esp_post_load_*.{c,h}` et `esp_native_playing_service_state.{c,h}`
sont retirés (18 fichiers). Leur dernière version reste récupérable dans Git
à la base de cet audit. Chaque marker compare la même Junction historique
(`sourceBytes=21051`, CRC `4a2c5800`, cible 9 et FNV épinglés). La fin du ladder
annonce encore gameplay/rendu « deferred » : elle n'est pas le service résident
générique actuel. Les 45 exports sont absents **avant et après**, les neuf
statics représentent 108 B dans les objets mais **0 B dans les deux ELF**.

Recherche complète : aucun include ni appel externe à cette famille dans le
code du dépôt, les tests ou les scripts de build ; seulement des références
internes. Pas de wrapper, callback enregistré ni format de sauvegarde qui lui
donne une racine. Les dépendances **sortantes** vers runtime/map/automap ne
rendent pas les markers accessibles en sens inverse.

Les owners actifs `EspNativeStartup`, `EspNativeGameplaySession`, PlayerView,
fresh-map, HUD refresh/clear, transitions/handoff, V11, RNG et renderer sont
inchangés. Les vrais owners post-spawn ne sont pas assimilés au vieux ladder.
Le script de génération et les flags de firmware ne sont pas modifiés.

Après : **174 sources** présentes, **173 directement compilées** + 3 générées,
**165 headers** dans include ; toujours huit headers locaux dans src.
Ce gain est de compréhension/build et d'API retirées, **pas de RAM/flash**.

## Vérification et niveau d'acceptation

- Builds normal/bringup locaux avant et après : PASS.
- Frontière de code commitée/poussée :
  `1bd1120550040d0b7060f537bc13783b64af7a5b`, branche
  `agent/esp32-consolidate-postload-junction`, parent exact `9d203e5`.
  [CI 38385323228](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38385323228)
  **SUCCESS** : tests hôte, deux builds, absence des exports retirés. La CI ne
  refait pas le comparatif byte-à-byte avec la base locale ; ces deux preuves
  sont distinctes. Clôture documentaire uniquement après ce résultat.
- Les **neuf sections allouées de chaque ELF sont strictement identiques**, y
  compris `.flash.appdesc`, `.flash.text`, `.flash.rodata`, IRAM, DRAM et BSS.
  Comparateur sans masque ni exception : `tools/compare_elf_alloc.py`.
- Les adresses et tailles sont inchangées ; RAM/flash restent celles du tableau.
- Le `.bin` n'est pas annoncé byte-identique : l'ELF perd du debug et l'image
  contient son hash ainsi qu'un digest. Comparaison binaire : toutes les
  différences sont bornées au SHA ELF (`0xb0..0xcf`), au checksum image et au
  digest final ; aucun autre octet ne diffère dans les deux images. Les digests
  sont vérifiés. Cela ne change pas le code/data fonctionnel chargé.
- Tests du comparateur : changement de debug accepté ; code, descriptor,
  adresse, BSS ou fichier tronqué rejetés. Les sept tests C actuels du workflow,
  le syntax-check action et l'autotest du watcher passent ; CI ajoute le test
  du comparateur et l'absence des exports retirés dans les deux ELF.
- Trois fixtures historiques hors CI ne compilent déjà plus avec le code de
  base, dont les fichiers concernés sont inchangés dans ce lot :
  `test_sys_touch.c` manque les mocks selector/touchSlot, `test_level_progress.c`
  les completionCheckpoint/restoreCompletionCheckpoint, `test_death_menu.c`
  les mainSelector Begin/Ready/Finish/Tap. Pas de PASS inventé pour ces fixtures,
  ni de correctif de gameplay dissimulé dans ce retrait.
- **Pas de nouveau PASS matériel demandé pour ce lot** : aucun octet chargé,
  adresse ni état alloué n'est modifié dans la comparaison contrôlée. Ce n'est
  pas un nouveau PASS de gameplay, de parité RNG ou de rendu. Toute suite qui
  change une frontière production redevient soumise au test CYD ciblé.

Reproduction sur un même poste/toolchain : construire la base dans un worktree
séparé, garder ses ELF, puis construire le candidat. Éviter de comparer les
ELF de profils différents ou les sorties de toolchains différentes.

```sh
python3 ESP32/test/elf_alloc_compare_host.py
python3 ESP32/tools/compare_elf_alloc.py ESP32/.pio/build/esp32-cyd/firmware.elf \
  --baseline /chemin/base-production.elf \
  --forbid-prefix EspPostLoad --forbid-prefix EspNativePlayingService
python3 ESP32/tools/compare_elf_alloc.py ESP32/.pio/build/esp32-cyd-bringup/firmware.elf \
  --baseline /chemin/base-bringup.elf \
  --forbid-prefix EspPostLoad --forbid-prefix EspNativePlayingService
```

## Deuxième jalon : ancien pipeline graphique ZIP retiré

Branche conservée : `agent/esp32-consolidate-postload-junction`, depuis le head
documentaire `9cde9d5`, sans rebase implicite ni changement de base main.

Le lot retire les quinze sources candidates du renderer/diagnostic ancien :

```text
menu_bsp_structure_probe / map_runtime_structure_probe
 -> resource_memory_plan_probe / native_asset_pack_probe
 -> native_bitshape_loader / native_sprite_texel_probe / native_palette
 -> native_graphics_resource_manager
 -> native_sprite_render_consumer / native_wall_render_consumer
 -> native_projected_wall_bridge / native_projected_wall_probe
 -> native_projected_sprite_renderer
 -> native_menu_wall_frame_probe / native_menu_sprite_frame_probe
```

Le seizième retrait est `esp_render_mapping_reload_guard.c`, wrapper dormant de
`Render_beginLoadMap`. **16 C + 13 H = 6 455 lignes supprimées**. Le diagnostic
`menu_bsp_probe.c` passe de 238 à 11 lignes : son implémentation ESP32 conserve
exactement le log RETIRED et le retour 0 ; le fallback non-ESP32 à ZIP/longjmp
est retiré. Ce dossier n'est pas une cible desktop. Les sources desktop de
référence dans `src/` et le générateur sont inchangés. Tout retrait est
récupérable dans Git au parent du lot.

### Fermeture et états vérifiés

- Ces 16 objets étaient compilés, mais leurs **31 exports** et leurs sections
  allouées étaient absents des deux ELF avant retrait. Leur orchestration venait
  du fallback `#else` de `DoomRPG_probeMenuBspHeader`, déjà exclu sur ESP32.
  `main.cpp` bringup continue d'appeler uniquement le stub qui retourne 0 ; le
  menu normal continue de construire/peindre le dashboard directement.
- Includes et symboles sont recherchés dans tout le code du dépôt, les tests et
  scripts. Les références externes aux anciens loaders proviennent seulement
  des méthodes LRU dormantes ; aucun begin/acquire/end/getStats de ces caches
  n'est lié. Aucun callback, RNG, format save ou owner transactionnel ne rend
  l'ancien pipeline accessible. Les ABI Render/DoomCanvas conservés ne sont
  pas supprimés sous prétexte que leurs anciens wrappers sont morts.
- Quatre directives sont retirées : `EspNativeGraphics_loadSpriteFrame`,
  `EspNativeGraphics_releaseSpriteFrame`, `Render_beginLoadMap` et
  `DoomCanvas_updateLoadingBar`. Les deux premières étaient communes aux
  profils ; les deux suivantes uniquement bringup. Compteurs wrap : **46/51**,
  contre 48/55 à la base ; wrappers projet effectivement liés : toujours 46/49.
- Les **guards cache-inactif restent conservés** : seuls les deux `isActive()`
  sont liés, mais ils gardent les BSS historiques sprite **312 B** et wall
  **176 B**. Ce sont 488 B **réellement présents**, non un gain fictif. Les
  corps des caches et `native_graphics_resource_manager.h` restent inchangés
  (sauf commentaire du contrat) pour préserver leurs layouts et les gates.
  Les fonctions de chargement anciennes n'ont plus de backend : elles ne sont
  pas une API disponible et ne doivent pas être réactivées. Le linker échouerait
  si leurs références non résolues devenaient vivantes.

### Retrait volontairement différé : les deux bridges du painter

Une tentative de retrait des deux wrappers scène→dashboard à l'intérieur de
`native_main_menu_touch_layout.c` a changé l'inlining de GCC dans
`DoomRPG_esp32PaintMenuDashboardCards` et `DoomRPG_esp32RepaintOpaqueMainMenu` :
déplacement des helpers/clones, **24 B de Flash en moins**, malgré l'absence des
wrappers dans les ELF. Le comparateur strict a correctement **REFUSÉ** cette
version. Les tailles rodata étaient inchangées ; ce n'est pas une preuve de
retrait de simples chaînes ni un no-op machine.

Le fichier du painter a donc été rétabli à l'identique au parent, avec ses deux
headers de bridge et leurs directives bringup. Seuls les commentaires de ces
headers clarifient leur dormance. **L'ELF final retrouve l'identité stricte**.
Ce retrait se fera dans un jalon séparé, avec tests ciblés MENU_MAIN/Options Back/
LOAD échec/START sur CYD si le code lié change, pas en ajoutant des constantes
de remplissage ou en désactivant les gardes du comparateur.

### Consolidation des régressions hôte

Les trois fixtures auparavant non compilables sont réparées **sans changement
de leur code production testé**, puis ajoutées au workflow CYD :

- SYS touch : mocks slotSelector/touchSlot, carte de hitboxes de l'overlay,
  aucun déplacement du curseur EXIT sous le picker, dispatch SELECT seulement
  au résultat 2, consommation des taps d'armement/navigation.
- Level progress : transport des deux bytes de completion dans le suffixe
  existant, validation map/complete avant appel de l'owner, rejet de l'owner
  sans remplacement du progress, anciens zéro réservés. Temps/moves/XP,
  wrap et saturation restent testés. **Le validateur des bits completion est
  mocké** : on ne lui attribue pas un PASS par ce test.
- Death menu : ouvrir LOAD ne restaure pas automatiquement une partie ; choix
  explicite, BACK avec repaint, admission failed/unready, restore failure puis
  retry et finish du selector. Les tests gardent timing, palette/fonts,
  no-save/deferred routes et guards framebuffer/present. Le picker et le
  backend SD sont mockés : ni pagination réelle ni atomicité SD ne sont validées.

### Mesures et acceptation du lot 2

| Mesure courante | Normal | Bringup |
| --- | ---: | ---: |
| Sources C/C++ présentes | 158 | 158 |
| Sources du dossier directement compilées | 157 | 157 |
| Copies générées / original desktop directement compilé | 3 / 0 | 3 / 0 |
| Commandes totales, framework/libs inclus | 215 | 215 |
| Sources contribuant aux sections allouées | 147 | 150 |
| Headers include / headers locaux src | 152 / 8 | 152 / 8 |
| RAM statique PlatformIO | 47 680 B | 47 832 B |
| Flash PlatformIO | 802 633 B | 820 313 B |

Deux builds locaux PASS. Les neuf sections allouées, layout/adresses/bytes et
les ensembles de symboles définis sont strictement identiques à `9cde9d5`
dans chaque profil. Les `.bin` ont la même taille (802 992 / 820 672 B) ; seules
les zones SHA ELF, checksum image et SHA image diffèrent, digests vérifiés.
Les dix tests C du workflow, l'autotest watcher, le comparateur Python et le
garde des exports passent localement. Les guards CI interdisent maintenant
aussi les namespaces/endpoints retirés du pipeline graphique dans les deux ELF.
Les trois fixtures réparées passent aussi localement sous ASan/UBSan ; le
LeakSanitizer est désactivé car incompatible avec le ptrace de cet environnement.
Ce contrôle mémoire hôte ne prouve pas les allocations/RAM sur le CYD.

**CI distante du nouveau lot : à vérifier après push.** Aucun nouveau PASS
matériel demandé : aucune section chargée ni adresse ne change dans le lot
final. Les contraintes PAK, `shapeData == NULL`, `mediaTexels == NULL`, RNG,
rollback, sauvegarde et transaction de publication demeurent inchangées.

Suites prioritaires mises à jour : les trois fixtures et les 15 diagnostics
candidats sont traités. Restent les deux bridges avec effet compiler, les
gardes/layouts LRU (488 B), les quatre helpers/modèle de sortie parallèles,
la documentation historique restante et les aliases de composition actifs.
Ne pas fusionner leurs lifetimes ; refaire audit et preuve à chaque frontière.
