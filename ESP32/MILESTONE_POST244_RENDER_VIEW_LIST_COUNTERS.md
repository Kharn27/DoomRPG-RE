# PR #245B — retirer trois slots desktop de liste visible / comptages BSP (candidat CYD)

## Relation exacte au PASS précédent

**Même branche :** `agent/esp32-render-world-pointers-post244` ; **même PR #245 (remise en Draft)**, aucun nouveau workflow de merge. Le jalon A, retrait des six pointeurs desktop, a été **PASS matériel** au code `4aa6eb643e68f81820da5f27dd46e6c9be1baff4`, documenté au commit `24e60c1129ce5ac1f1c4327a7b369cf6d39c7c29` ; voir `ESP32/MILESTONE_POST244_RENDER_WORLD_POINTERS.md`. Le code B n'est **pas** couvert par ce hardware PASS.

## Comportement legacy exact / propriétaires

Dans le moteur desktop `src/Render.c` : `Render_beginLoadMapData` fixe `render->numSprites = render->numMapSprites + MAX_CUSTOM_SPRITES + MAX_DROP_SPRITES` pour allouer son tableau monolithique de `Sprite_t`. Le parseur BSP garde `numTileEvents` comme cardinalité du tableau `tileEvents`. Le raster `Render_renderBSP` réinitialise puis reconstruit une linked-list triée `viewSprites` de `Sprite_t` afin de dessiner les sprites visibles.

Le vrai ESP32 exécute à la place `EspMapRuntime`, `EspMapSpriteTopology`, les overlays natifs et le pipeline d'admission BSP de #244A/#244B. Ces trois membres de `Render_t` ne sont lus ni par le moteur natif normal/trace, ni par ses caches, ni par sa séquence de restauration. **Aucune transposition des structures desktop et aucune nouvelle allocation.**

## Changement strict et contrat

Dans `src/Render.h`, placer **uniquement** `int numSprites`, `int numTileEvents`, `Sprite_t* viewSprites` derrière `#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_BRINGUP_PROBES)`. Desktop et bringup gardent l'ABI de référence. Les champs **actifs** `lineCount/lineRasterCount/nodeCount/nodeRasterCount/spriteCount/spriteRasterCount` ne sont PAS retirés : `esp_native_first_frame.c` et `esp_native_sprite_renderer.c` les mettent à jour et sauvegardent/restaurent les snapshots transactionnels. `numLines`, `spanMode`, `columnScale`, `skipCull` et tous les mappings/palette/live framebuffer restent aussi intacts.

Le guard hôte `native_render_raster_abi_retirement_host.py` valide les trois déclarations préprocesseur et leur usage desktop, scanne les sources et headers natifs pour rejeter toute réintroduction, et garde les assertions précédentes. L'assert ABI normal dans `render_startup_bridge.c` passe de **1308 à 1296 B** (−12 B attendus). Ainsi `CORE used` devrait passer de **2276 à 2264 B**, à confirmer par vrai boot. Pas de changement aux contrôleurs, aux timings, au sprite renderer, au gameplay ni à `PlatformVideo_present`.

## Gate — matériel non encore testé

1. CI du **SHA code B** : tests hôte originaux et garde complétée, builds normal/bringup/trace, suivre 78/78 ; aucun test local prétendu.
2. Flash **normal** `esp32-cyd` depuis **cette branche** : boot `Render=1296 Game=0 Canvas=44 Total=2100`, `RENDERCORE bytes=1296`, `CORE used=2264`, `shapeData=mediaTexels=NULL`, PSRAM=0. Heap8 aux points de comparaison A menu/START/LOAD/EXIT `153184/112904/112092/158028` : observer et consigner les deltas réels, ne pas prédire l'arrondi allocateur.
3. START / Entrance intro, visible CROSS, déplacement/rotation, deux caisses/pickups et flash pulsé, dialogue, porte et HUB EXIT ; vérifier `c3882516`, première frame `71ca7465`, sortie `released=18008 empty=1` / menu `0f2c9fc3`. Fluidité **subjective** vs A et #244B.
4. LOAD slot04 V11 Malwrath : porte 113, copies CROSS `copies=4 accounting=1`, attaque triple projectile204, frappe joueur/mort/drop et EXIT ; vérifier `HEADROOM_OK`, pas de crash/WDT/unsupported.
5. Seulement après PASS : source B gelée, commits docs-only, CI post-doc, repasser la **même PR #245** en Ready for review ; **jamais merger main sans l'utilisateur**.

**Statut : CANDIDAT CODE B. Le PASS hardware A documenté n'est pas une preuve de l'ABI B.**
