# ARCH-RC21 — abandon conjoint des wrappers sprite projeté et admission BSP

**Base GitHub exacte après fusion PR #403 :** `88c1e58d85001f6cbb26a7b7bee5fea43a37e7cf`. **Branche :** `agent/esp32-arch-rc21-sprite-visibility-reads`. **Statut : PASS matériel borné vrai classic CYD + double CI code SUCCESS. Fusion autorisée par le testeur le 21 novembre 2026.**

## Architecture et portée

Suppression groupée des deux GNU `--wrap` suivants : `EspMapRuntime_getMapSprite` et `EspNativeBspVisibility_mapSpriteVisible`. **Production 6→4 ; bringup 9→7**, les trois interpositions de diagnostic inchangées. Les quatre wrappers production restants couvrent `EspNativeGameplayFrame_renderTurn`, `EspNativeGameplayHud_view`, `EspPlayerView_commitPreparedMove`, `Esp32PlatformVideo_present`. Aucun changement au format BSP, V11, audio ou framebuffer.

La paire retirée forme une frontière d'appel cohérente :

1. `esp_map_access.c:EspMapRuntime_getMapSpriteBase` décode les **5 octets immuables** depuis l'arena, sans allocation ni overlays. C'est l'ancienne implémentation réelle.
2. `esp_native_gameplay_sprite_overlay.c:EspMapRuntime_getMapSprite` est la **façade publique directe** ; elle conserve **exactement** la transformation crate, le bit consumed, la topologie projetée et le positionnement du monstre vivant/déplacé/cadavre, puis les bits animation FIXED et sort bias pain/death/corpse. Le cas **monsterPosition.tile == topology.tile** est préservé même si le cadavre n'est plus LINKED. Erreur d'accès raw et erreur crate restent fail closed.
3. `esp_native_bsp_visibility.c:EspNativeBspVisibility_mapSpriteVisibleReadOnly` demeure l'admission pure et le cache 2 KiB des sprites immobiles, avec recalcul de feuille pour monstre dont la position publiée a bougé (bug Pinkie corrigé précédemment).
4. `esp_native_bsp_visibility.c:EspNativeBspVisibility_mapSpriteVisibleBase` est la feuille pure explicitement appelée par la nouvelle **façade publique** `esp_native_gameplay_monster_active_sequence.c:EspNativeBspVisibility_mapSpriteVisible`. Après admission uniquement, elle lit la topologie canonique **et le sprite raw Base**, refuse NOT-READY, indices invalides, non-enemy, unlink/dead, ou bit NO_ACTIVATE ; ensuite seulement `EspNativeGameplayMonsterActivation_observeVisible`. La **vue de preview** appelle `ReadOnly` et ne déclenche jamais d'activation.

Le point de couplage ancien était `__real_EspMapRuntime_getMapSprite` dans la fonction de visibilité, utilisé pour ne jamais voir le sprite projeté et ne pas réentrer via les overlays. RC21 remplace strictement ce bypass par `EspMapRuntime_getMapSpriteBase`. Les deux symboles publics restent inchangés pour leurs appelants.

## Protections logicielles

- `ESP32/test/native_sprite_visibility_reads_boundary_host.py` : compilation de **6 vrais corps C**, 14 mutations adversariales (getters raw, crate, consumed, move index/tile, pain/corpse sort, admission pure, activation), oracle des sorties et side effects ; inclut la vraie fonction utilitaire `readLe16` (aucun re-code de ce décodage) ; aucune création monde ni allocation.
- Inventaire `ESP32/test/native_wrap_linkage_inventory_host.py` : symbole GNU retiré à la fois de `platformio.ini` et des quatre vrais ELF, 4+3 attendus. Les oracles RC7 visibilité Pinkie, RC20 topologie et RC19 vues dynamiques restent exécutés dans le CI.
- **Deux workflows SUCCESS sur HEAD final** et quatre compilations `esp-cyd-inverted`, `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace` exigés avant essai.

## Protocole de test — vrai classic CYD sans PSRAM

Sur l'environnement normal `esp32-cyd` : BOOT → NEW GAME/INTRO → Entrance, vérifier sprites projetés après caisse détruite et pickups, ouverture/fermeture de porte, apparition et cri d'un zombie puis attaques (pistolet ou shotgun sont tous deux legacy), mort/cadavre, déplacement d'un monstre près d'une porte (Pinkie si disponible), rotation et strafes, HUD, HUB EXIT ; LOAD V11 slot04 et reproduire un combat puis EXIT. Une seconde passe avec `esp-cyd-inverted` est optionnelle, **non requise** pour la RAM normale.

Logs attendus : `MONSTERACTIVESEQ COMPLETE` / activation cohérente, `DOORANIM COMPLETE` et HOLD/RELEASE, pas de `VIEWANIM FALLBACK` ou `DOORANIM FAILED`, `HEADROOM_OK`, `shapeData=0x0 mediaTexels=0x0`, `RESIDENTRESET empty=1`, `SYSEXIT MENU-READY`, audits AudioIntent zéro overflow/drop et aucune panic/WDT. Les cas injection de faute et mutations host ne seront jamais présentés comme observés sur le CYD.

## Validation matérielle réelle — 21 novembre 2026

**Source :** `platformio-device-monitor-261121-100048.log` ; **2 180 lignes, 287 798 octets, SHA256 `6be1918aee22418e1f101b84bca0707efe49fee88c9b8a9d56bf0973a3cf945d`**. Cible observée : `ESP32-D0WD-V3`, 240 MHz, flash 4 Mo, PSRAM 0 ; framebuffer 160×120 RGB565 x2 sur ILI9341. La trace ne contient ni SHA de firmware ni nom d'environnement PlatformIO ; l'attribution à RC21 est établie **par contexte d'essai fourni**, pas par preuve binaire dans le Serial.

**Parcours réellement couvert :** menu Options/Video/Theme/Back → NEW GAME, ST_INTRO, Entrance (`/intro.bsp`) → caisses, pickups, dialogues, porte 275 animée (ouverture et fermeture différée), HUD/System EXIT sans sauvegarde → LOAD V11 `slot04` (5 604 B) avec restauration spatiale exacte `V9-SPATIAL-STAGE exact=yes` (monstres 30, topology 43, position et activation) → reprise Entrance → activation BSP du sprite ennemi 322 (`MONSTERACT ACTIVE`) → 3 commits de combat (HP 11→7→3→0), 2 ripostes complètes avec animation, `death4→corpse2` et `MONSTERDROP COMMIT` (slot 2) → retour propre au menu.

**Compteurs stricts de la capture :** 56 `VIEWANIM END` ; 6 `DOORANIM COMPLETE`, 3 `HOLD`, 3 `RELEASE` et 3 `POST-MONSTER-COMPLETE` ; 3 `DIALOG CLOSE` ; 33 `MONSTERACTIVESEQ COMPLETE` ; 3 `MONSTERCOMBAT COMMIT` (un décès), 2 `MONSTERRETAL COMMIT`, 1 `MONSTERDROP COMMIT` ; 2 `RESIDENTRESET empty=1` suivis de 2 `SYSEXIT MENU-READY`. Pas de `VIEWANIM FALLBACK` ni `DOORANIM FAILED`, crash, panic ou WDT dans la trace.

**Mémoire et audio :** 2 `RAMBUDGET RESERVE advisory=HEADROOM_OK` ; `margin8=17092` puis `16276` octets, `largest8Target=16384` satisfait ; `shapeData=0x0 mediaTexels=0x0` dans les frontières de startup, jeu, retour menu. Cinq `AUDIOINTENT AUDIT` : `queueOverflow=consumerCursorGap=audibleSfxDropped=pending=0`. Sur les flux et finales, `underruns=underflows=recoveries=recoveryStarts=maxSilentFrames=gapsOver500Ms=0`. **Une mesure cumulée `gapsOver90us=1`** sur le premier flux Entrance (sans underrun) ; la seconde session reste à 0.

**Portée et non-preuves :** l'activation par visibilité est observée pour l'ennemi 322, mais la trace ne montre **aucun `MONSTERMOVE COMMIT` / `MONSTERPOS COMMIT`** ; pas de test réel du Pinkie traversant une porte, ni de preuve visuelle image-par-image du corps/cadavre déplacé. Le cache de feuille BSP du monstre déplacé est protégé par l'oracle RC7, les accès raw/projection et leurs 14 mutants par RC21. Pas de campagne exhaustive, d'injection d'échec ou de contrôle SHA firmware dans Serial. Le résultat matériel est donc un **PASS borné sans anomalie RC21 bloquante démontrée**, et non une preuve de toutes les branches.

**CI code final avant hardware :** commit `d7ff4ce874f5bd00407f97695f651d19215d37be`, [push #44639511612](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44639511612) et [PR #44639515680](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44639515680), tous deux **SUCCESS**, quatre ELF Xtensa, 4+3 wrappers, six corps C réels + 14 mutants et oracles SDL historiques. **Tous les changements après la capture matérielle doivent rester docs-only ; revérifier double CI sur HEAD final avant fusion.**

**Décision :** fusion autorisée par l'utilisateur le 21 novembre 2026, sous réserve de la vérification finale des deux CI documentaires et de la base actuelle de `main`.
