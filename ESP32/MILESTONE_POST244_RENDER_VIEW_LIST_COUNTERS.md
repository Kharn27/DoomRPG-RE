# PR #245B — retirer trois slots desktop de liste visible / comptages BSP (PASS réel classic CYD)

## Validation réelle classic CYD — PASS du jalon B (2 novembre 2026)

**Code matériel testé :** `4f81d31665e671283181147a34c00ff13b2602a2` sur la même branche `agent/esp32-render-world-pointers-post244`, firmware **normal** `esp32-cyd` ; le SHA n'est pas embarqué dans les lignes Serial, l'attribution repose sur le commit candidat fourni au testeur. Source hardware `device-monitor-261102-022338.log` (**1519 lignes**), retour utilisateur : **« Nervosité inchangée pour moi pour l'instant »**. CI du code exact : [push #41371923424](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41371923424) et [PR #41371928441](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41371928441) **SUCCESS 78/78 chacune** (tests hôte + firmwares normal/bringup/trace ; aucune compilation locale ou mesure A/B contrôlée revendiquée). Le jalon A reste un **PASS séparé** au SHA `4aa6eb643e68f81820da5f27dd46e6c9be1baff4`.

**Boot observé :** `Engine structs: Render=1296 Game=0 Canvas=44 Total=2100 bytes`, `[RENDERCORE] INIT ... bytes=1296`, `[CORE] READY ... used=2264`, ESP32-D0WD-V3 à 240 MHz / flash 4 Mo / **PSRAM=0**, framebuffer logique 160×120 RGB565 38400 octets. B par rapport à A : **−12 B pour Render_t et CORE** ; cumul #244B→#245B **−36 B** (`1332→1296` et `2300→2264`). Native PAK : 241 entrées, mappings 152/252 et payload 8376 B ; `shapeData=mediaTexels=NULL` sur les frontières documentées. BSP `intro.bsp` CRC `623f34e4`, arène 14095 octets `c3882516`, première frame `71ca7465`.

**Heap8 matériel A→B :** menu après mappings **153184→153196** (+12 B), START prime **112904→112916** (+12 B), LOAD prime **112092→112104** (+12 B), EXIT après chacun des deux sessions **158028→158040** (+12 B). `largest8` gameplay 86004 et menu 110580 B restent **identiques**. Deux réserves `RAMBUDGET RESERVE ... HEADROOM_OK` : marges heap8 **55572** (START) et **54760** (LOAD). Les chiffres sont des snapshots hardware, pas une promesse générale d'allocation.

**Parcours START / Entrance :** prologue, porte 275 avec animation 4 frames et fermeture différée après monster-turn ; deux caisses `sprite127/56`, **7 pickups** (Armor Shards, Bullet Clip, Fire Ext et Sm Medkit), flash blanc **3 pulses 105 ms ON / 65 ms OFF** et extinction ; dialogues **88, 89, 79 (opcode 26), 60**, reprise opcode19 ; **event60** ouvre deux étapes de dialogue et `DIALOGCHAIN RESUME handled=3 show=1 removed=1 topologySnapshot=2408B` sans fail-open, puis deuxième étape. Animations move/turn/strafe, CROSS141/77 ; HUB et sortie sans écriture save. L'automap reçoit les publications de visibilité, mais **aucune ouverture de l'écran automap** n'est documentée par ce journal.

**Parcours LOAD slot04 V11 :** save 5604 B, restaures exacts des états `beaeaccc/50e16fe3/e2a9d90a/df419482` (monstre/topologie/positions/activation), porte113 ; Malwrath322 HP `11→7→3→0` et **3 ripostes triples projectile204**, chaque séquence entière `MALWRATHPROJECTILE CLEAR probe=1/2/3` **avant** `MONSTERRETAL COMMIT`, gameplay RNG préservé jusqu'à la résolution ; mort/drop Sm Medkit, pickup Flak Jacket et secret event15 `+5XP`, HUB et EXIT. **103** témoins `NATIVESPRITE CROSS copies=4 accounting=1` avec clipping/near cohérent. Le log n'établit pas lui-même le nombre réel de lectures PAK de chaque groupe CROSS.

**Teardown / sûreté :** **deux** `RESIDENTRESET heap8=140032→158040 released=18008 empty=1` ; **deux** `SYSEXIT MENU-READY frame=0f2c9fc3 ... saveWrite=no checkpoint=unchanged` ; aucune erreur fatale, panic, watchdog, positive `unsupported` ou écriture SAVE observée. Les **3** `NATIVEFRAME BOUNDARY-INLINE` et l'offset affiché `0x40000000` pour la coordonnée x projectile restent des diagnostics déjà présents sur le jalon A, ni attribués à B ni corrigés ici.

**Performance / ressenti :** 16 `SPRITEORDER.scanUs` médiane **3016,5 µs** (premier scan froid 12400 µs), 30 `VIEWANIM FRAME mode=move` médiane **85979,5 µs**, 20 frames `rotate` médiane **82252,5 µs**, 6 `MONSTERCOMBAT FRAME` moyenne **94997,8 µs**, 10 `PLAYERRES FRAME` médiane **92908 µs**. Sur A, scan médian 3066,5 µs, move médian 88145 µs, combat moyen 95854,5 µs ; **campagnes et scènes non identiques**, donc aucune attribution causale d'accélération à −12 octets. Le ressenti utilisateur reste inchangé.

**Décision : PASS CYD ciblé comportement + mémoire + ressenti pour B au SHA `4f81d31665e671283181147a34c00ff13b2602a2`. Source B gelée. Tout commit post-test DOIT modifier uniquement la documentation, suivi d'une CI post-doc verte et d'un contrôle du diff avant de rendre #245 Ready for review. Merge exclusivement utilisateur ; aucune nouvelle PR.**

## Relation exacte au PASS précédent

**Même branche :** `agent/esp32-render-world-pointers-post244` ; **même PR #245 (remise en Draft)**, aucun nouveau workflow de merge. Le jalon A, retrait des six pointeurs desktop, a été **PASS matériel** au code `4aa6eb643e68f81820da5f27dd46e6c9be1baff4`, documenté au commit `24e60c1129ce5ac1f1c4327a7b369cf6d39c7c29` ; voir `ESP32/MILESTONE_POST244_RENDER_WORLD_POINTERS.md`. Le code B dispose désormais de **sa propre validation hardware indépendante**, décrite ci-dessus.

## Comportement legacy exact / propriétaires

Dans le moteur desktop `src/Render.c` : `Render_beginLoadMapData` fixe `render->numSprites = render->numMapSprites + MAX_CUSTOM_SPRITES + MAX_DROP_SPRITES` pour allouer son tableau monolithique de `Sprite_t`. Le parseur BSP garde `numTileEvents` comme cardinalité du tableau `tileEvents`. Le raster `Render_renderBSP` réinitialise puis reconstruit une linked-list triée `viewSprites` de `Sprite_t` afin de dessiner les sprites visibles.

Le vrai ESP32 exécute à la place `EspMapRuntime`, `EspMapSpriteTopology`, les overlays natifs et le pipeline d'admission BSP de #244A/#244B. Ces trois membres de `Render_t` ne sont lus ni par le moteur natif normal/trace, ni par ses caches, ni par sa séquence de restauration. **Aucune transposition des structures desktop et aucune nouvelle allocation.**

## Changement strict et contrat

Dans `src/Render.h`, placer **uniquement** `int numSprites`, `int numTileEvents`, `Sprite_t* viewSprites` derrière `#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_BRINGUP_PROBES)`. Desktop et bringup gardent l'ABI de référence. Les champs **actifs** `lineCount/lineRasterCount/nodeCount/nodeRasterCount/spriteCount/spriteRasterCount` ne sont PAS retirés : `esp_native_first_frame.c` et `esp_native_sprite_renderer.c` les mettent à jour et sauvegardent/restaurent les snapshots transactionnels. `numLines`, `spanMode`, `columnScale`, `skipCull` et tous les mappings/palette/live framebuffer restent aussi intacts.

Le guard hôte `native_render_raster_abi_retirement_host.py` valide les trois déclarations préprocesseur et leur usage desktop, scanne les sources et headers natifs pour rejeter toute réintroduction, et garde les assertions précédentes. L'assert ABI normal dans `render_startup_bridge.c` passe de **1308 à 1296 B** (−12 B confirmés sur vrai CYD). Ainsi `CORE used` passe de **2276 à 2264 B**, confirmé sur vrai boot. Pas de changement aux contrôleurs, aux timings, au sprite renderer, au gameplay ni à `PlatformVideo_present`.

## Protocole initial (historique, réalisé sauf ouverture de l'automap)

1. CI du **SHA code B** : tests hôte originaux et garde complétée, builds normal/bringup/trace, suivre 78/78 ; aucun test local prétendu.
2. Flash **normal** `esp32-cyd` depuis **cette branche** : boot `Render=1296 Game=0 Canvas=44 Total=2100`, `RENDERCORE bytes=1296`, `CORE used=2264`, `shapeData=mediaTexels=NULL`, PSRAM=0. Heap8 aux points de comparaison A menu/START/LOAD/EXIT `153184/112904/112092/158028` : observer et consigner les deltas réels, ne pas prédire l'arrondi allocateur.
3. START / Entrance intro, visible CROSS, déplacement/rotation, deux caisses/pickups et flash pulsé, dialogue, porte et HUB EXIT ; vérifier `c3882516`, première frame `71ca7465`, sortie `released=18008 empty=1` / menu `0f2c9fc3`. Fluidité **subjective** vs A et #244B.
4. LOAD slot04 V11 Malwrath : porte 113, copies CROSS `copies=4 accounting=1`, attaque triple projectile204, frappe joueur/mort/drop et EXIT ; vérifier `HEADROOM_OK`, pas de crash/WDT/unsupported.
5. Seulement après PASS : source B gelée, commits docs-only, CI post-doc, repasser la **même PR #245** en Ready for review ; **jamais merger main sans l'utilisateur**.

**Statut B : PASS matériel ciblé au SHA code B ci-dessus ; code gelé, clôture documentaire uniquement. Le PASS hardware A reste distinct et n'est pas utilisé pour revendiquer ce résultat B.**
