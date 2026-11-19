# ARCH-RC20 — lecteurs topologiques publics sans GNU --wrap (en validation)

**Branche :** `agent/esp32-arch-rc20-topology-reads` — **base `main` vérifiée après PR #398 : `f04f16ba73a8f9532f0bffdae8ed459f6711626a`**. Date : 19 novembre 2026. **Statut : NON VALIDÉ SUR CYD / NON MERGE-READY.**

## Périmètre

Retrait groupé de deux interpositions production :

- `-Wl,--wrap=EspMapSpriteTopology_getEntity` ;
- `-Wl,--wrap=EspMapSpriteTopology_getVisualState`.

Inventaire **8 → 6** wrappers de production, **11 → 9** bringup (3 extras inchangés). `EspNativeGameplayFrame_renderTurn` reste le propriétaire transactionnel des vues dynamiques/portes. Les interpositions `EspMapRuntime_getMapSprite`, `EspNativeGameplayHud_view`, `EspPlayerView_commitPreparedMove`, `EspNativeBspVisibility_mapSpriteVisible` et `Esp32PlatformVideo_present` restent présentes.

**Pipeline de lecture garanti** :

1. `EspMapSpriteTopology_getEntityBase/getVisualStateBase` : exactes données canoniques compactes de l'overlay topologique, sans allocation ni mutation.
2. `EspNativeGameplayActionEngine_getEntity/getVisualState` : mêmes effets de removal + crate que le précédent `__wrap` privé, sans double `__real`.
3. `EspNativeGameplayProjectionOverlay_getEntity/getVisualState` : mêmes deux applications de crate (dont une répétition héritée/contractuelle), consommés/masqués et animation attaque.
4. `EspMapSpriteTopology_getEntity/getVisualState` : façade publique détenue par MonsterCombat ; mêmes corps que les deux anciens `__wrap` publics, y compris absent/not-ready, gib/corpse/pain, `syncOwner()`, vivant/mort, lien et ordre.

Le rendu sprites, les traces de combat et les autres consommateurs appellent les **noms publics inchangés**. Le raw ne doit jamais traverser la chaîne publique, afin de ne pas récursiver via ActionEngine. Aucune mutation BSP, aucune adaptation monde nouvelle, aucune structure supplémentaire dans la session.

## Oracles logiciels

- `ESP32/test/native_topology_reads_boundary_host.py` : compose **9 vrais corps C** (2 raw, 3 ActionEngine, 2 ProjectionOverlay, 2 MonsterCombat), couvre les états visuels/entités, ordre d'application, deux crates, suppression, pickup, douleur, cadavre, gib, source malformée, index hors plage, NULL et arrêt sur erreur ; **14 mutants adversariaux**, qui doivent tous échouer. Nécessite un compilateur hôte `cc`.
- `ESP32/test/native_projection_read_boundary_host.py` : oracle RC7 précédent inchangé sur le comportement (17 vérifications / 13 mutants) mais branché sur les feuilles `Base` et les noms privés directs.
- `ESP32/test/arch_rc20_history.py` : inverse stricte RC20 **seulement** pour les empreintes historiques SDL-F25/F27, sans restauration d'alias GNU dans le firmware.
- `ESP32/test/native_wrap_linkage_inventory_host.py` et 6 oracles de frontières antérieurs : nouvel inventaire exact 6+3. Quatre ELF natifs à prouver, et deux workflows GitHub SUCCESS sur le HEAD final.

## Contrat de test CYD

Après double CI vert, flasher **`esp32-cyd` normal** sur le classic CYD sans PSRAM. Parcours conseillé : BOOT → NEW GAME (INTRO) → Entrance, ouvrir puis refermer porte avec HOLD/RELEASE, pickup, dialogue SELECT et ENTER, action caisse, combattre un zombie/imp, observer mort/cadavre/animation, HUB EXIT → LOAD slot V11 → gameplay/monstres restaurés → EXIT. Inclure si possible un monstre mobile dans une porte (regression Pinkie). Ne pas utiliser `esp32-cyd-bringup` comme mesure RAM de production.

Contrôler `HEADROOM_OK`, `shapeData=NULL`, `mediaTexels=NULL`, absence de WDT/panic, `RESIDENTRESET empty=1`, `AUDIOINTENT AUDIT`, porte COMPLETE et `SYSEXIT MENU-READY`. Les résultats matériel/visuels appartiennent au Serial + observations du testeur, jamais extrapolés des tests host. En cas de CI rouge, corriger sur cette branche avant test.
