# ARCH-RC20 — lectures topologiques publiques sans GNU --wrap — PASS CYD borné

**Branche :** `agent/esp32-arch-rc20-topology-reads` — **base `main` vérifiée après PR #398 : `f04f16ba73a8f9532f0bffdae8ed459f6711626a`**. Date : 19 novembre 2026. **Statut : PASS matériel borné sur HEAD code `971adebd2797fb6a67f2fe886f2c00514fd83f6b`, double CI de code SUCCESS ; revalidation de l'intégration main + CI documentaire requise avant merge-ready.**

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

## Double validation logicielle du code (avant test CYD)

- Code/firmware HEAD `971adebd2797fb6a67f2fe886f2c00514fd83f6b`.
- [CI push #44630600220](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44630600220) et [CI PR #44630615163](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44630615163) : **SUCCESS** chacun ; 4 profils (esp-cyd-inverted, esp32-cyd, esp32-cyd-bringup, esp32-cyd-trace), inventaire ELF 6 normal + 3 supplémentaires bringup ; oracles 9 vrais corps C/14 mutants et anciennes empreintes historiques valides.
- Premier CI rouge : `esp_native_gameplay_crate_state.c` consommait encore `__real_EspMapSpriteTopology_getEntity`. Migration **exclusive** de `CrateState_ensure()` vers `EspMapSpriteTopology_getEntityBase`, donc jamais de récursion crate → public projeté → crate ; test de non-régression ajouté. Le double CI final sur le HEAD ci-dessus est vert.

## Capture réelle classic CYD — 19 novembre 2026

**Source matérielle :** `platformio-device-monitor-261119-235308.log`, **2810 lignes / 368057 octets**, SHA256 **`716a9cb9f1b60b1a5bd75c46e31c38375ef35cf3ae650c8d8101bd3bba1ab050`**. ESP32-D0WD-V3, 240 MHz, 4 Mo flash, **0 PSRAM** et framebuffer 160×120 RGB565 (38400 B), écran 320×240. Attribution au HEAD code ci-dessus par contexte de test : **le Serial ne donne ni SHA Git ni identifiant d'environnement PlatformIO**. Environnement demandé au testeur : `esp32-cyd` normal.

**Parcours observé :** BOOT → NEW GAME / ST_INTRO → Entrance, caisse détruite, actions de gameplay, pickups, dialogues SELECT et MOVE, événement state + mutations SHOW/HIDE/UNLOCK, portes, zombie et combat → sortie HUB → LOAD V11 `/DoomRPG-ESP32-slot04.sav` 5604 B → restauration ressources/overlay/topologie/monstres/positions/activation/V11 et combattant Malwrath (3 tirs par rafale), drops, pickup → HUB EXIT → MENU.

- **79 `VIEWANIM END`**, **10 `DOORANIM COMPLETE`**, **5 paires `HOLD/RELEASE`** + 5 `POST-MONSTER-COMPLETE`, sons 5063/5064 diffusés après première présentation de porte. Aucun `DOORANIM FAILED`, `VIEWANIM FALLBACK`.
- **8 `DIALOG CLOSE`**, **40 `MONSTERACTIVESEQ COMPLETE`**, **3 `MONSTERRETAL COMMIT`**, **5 `MONSTERCOMBAT COMMIT`**, dont 2 décès avec traitements gib/cadavre, et **2 `MONSTERDROP COMMIT`**.
- **2 `HEADROOM_OK`** : `margin8=24440 B` puis `23180 B`; `largest8Target=16384 B`; **2 `RESIDENTRESET empty=1 released=18008`**, puis **2 `SYSEXIT MENU-READY resident=empty`**. `shapeData=0x0 mediaTexels=0x0` à plusieurs frontières. Aucune ligne Guru Meditation, panic, WDT, stack corruption ou erreur FAILED observée.
- **6 `AUDIOINTENT AUDIT`** : `queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 pending=0`; en fin de deuxième session `consumed=65 handoffDiscarded=6`. Les 6 `ENTRANCEMUSIC FINAL` : `underruns=0 recoveries=0 maxSilentFrames=0 gapsOver90us=0`. **Une mesure `gapsOver500Ms=1`** sur le second handoff M_STALKS (`maxPublishGapMs=514`) ; ne pas la masquer en affirmant que tous les gaps étaient nuls. Aucun sous-débordement effectif.

## Diagnostic sonore du zombie : exact legacy, pas un bug RC20

L'utilisateur a signalé un son de fusil à pompe, différent de celui du pistolet qu'il s'attendait à entendre. Le log prouve **sprite 220 / subtype=0 / mType=0 / alternateAttack=1 / weapon=3**, puis **3 `MONSTERSFX ATTACK` id=5047**, et les `SFXGEN START/END id=5047`. Ceci correspond **exactement** à la référence du dépôt :

- `src/EntityMonster.c` : famille Zombie Pvt/Lt/Cpt, colonne `Attack1=5046`, colonne `Attack2=5047` ;
- `src/CombatEntity.c` : `monsterAttacks[0]=2` (pistolet), `monsterAttacks[1]=3` (fusil à pompe) ;
- `src/Entity.c` : sélection initiale par tirage RNG bit `0x800000` ; `src/Combat.c` utilise ce bit pour choisir l'arme, puis colonne sonore 4 ou 5. `esp_native_gameplay_monster_state.c` stocke le bit alternatif et `esp_native_gameplay_monster_attack_visual.c` lit le tableau natif copié du legacy.

**Verdict : comportement original volontaire** ; aucun correctif audio, aucune issue de régression créée sur cette seule observation. Ceci ne démontre pas les autres cas sonores/monstres non joués.

## Verdict

**PASS matériel borné RC20.** Le test étaye la continuité de gameplay, rendu, topologie projetée, combats, chargement V11, effets visuels, flux audio, portes et libération mémoire sur la carte réelle. L'utilisateur confirme un jeu apparemment correct, mais le test ne prouve pas les 14 mutants sur matériel, les entrées invalides forcées, toutes les cartes ou les variantes de panneau. Le firmware testé précède l'intégration des travaux parallèles UAC si `main` évolue ; ne pas attribuer cette capture à un binaire ultérieur. Après cette capture, les changements RC20 sont **documentation uniquement**. Procéder à la vérification du vrai main, intégration non destructive et double CI avant de déclarer merge-ready. **Ne pas merger sur main sans demande explicite.**
