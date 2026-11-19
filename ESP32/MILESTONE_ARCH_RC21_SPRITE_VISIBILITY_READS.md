# ARCH-RC21 — abandon conjoint des wrappers sprite projeté et admission BSP

**Base GitHub exacte après fusion PR #403 :** `88c1e58d85001f6cbb26a7b7bee5fea43a37e7cf`. **Branche :** `agent/esp32-arch-rc21-sprite-visibility-reads`. **Statut : implémenté, CI/flash vrai CYD non encore validés. Ne pas merger avant PASS utilisateur.**

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

**Ne pas fusionner `main` avant retour utilisateur PASS + documentation post-test docs-only + double CI finale et merge explicite.**
