# Post-#245 — retirer le miroir d'en-tête BSP du shell Render (CANDIDAT)

## Base et périmètre

**Source de vérité :** `main` GitHub après le merge de #245, SHA exact `3142432f5b6870c5a8579a5cd293f82e39f0fa8f`. **Branche :** `agent/esp32-render-retire-legacy-map-header`. **Premier code candidat :** `23f57b9758c47990abc1b5df4347ccc6ba0d7daa`. La PR #245 (code B `4f81d31665e671283181147a34c00ff13b2602a2`) reste le dernier **PASS CYD matériel** de référence avant ce jalon ; son boot `Render=1296`, `CORE used=2264`, PSRAM zéro et sa nervosité inchangée ne prouvent rien du nouveau code.

## Propriété / comportement desktop et runtime natif

`src/Render.c::Render_beginLoadMap` conservait depuis l'architecture desktop/J2ME les champs `Render_t::mapName[MAPNAMESTRLEN]` (16 octets), `mapNameID`, `loadMapID`, `introColor`, `mapSpawnIndex`, `mapSpawnDir` et `mapCameraSpawnIndex`. L'ancien `Render_init` écrivait `mapCameraSpawnIndex=0` et le chargement desktop utilisait `loadMapID` pour le contenu sprite. Aucun de ces membres ne porte le monde ESP32 actuel : le port a un reader `EspBspReader`, une arène compacte `EspMapRuntime` et l'état de spawn et de caméra du joueur natifs.

**Frontière choisie :** supprimer seulement leur duplication dans la structure `Render_t` **normale et trace** ESP32 ; garder tous les membres dans le desktop et `DOOMRPG_ESP32_BRINGUP_PROBES`, sans toucher au parseur original. `src/Render.h` pose deux guards de préprocesseur autour de `mapName` et du groupe de six scalaires, en respectant `unk1` desktop. Le constructeur natif ne référence plus `mapCameraSpawnIndex`. Aucun changement au gameplay/rendering, moteur sprite #244A/B, cache, SD/PAK, save, audio, `PlatformVideo_present()`, `shapeData/mediaTexels` ou pipeline d'allocations. Les compteurs raster natifs, le curseur, la palette et les buffers indispensables restent inchangés.

## Contrats logiciels vérifiables

- `_Static_assert(sizeof(Render_t) == 1256U)` sur firmware normal, soit **−40 B attendus** vs #245B, alignement 32 bits compris. `CORE READY used=2224` est une **prévision**, pas une mesure.
- Le guard `native_render_raster_abi_retirement_host.py` vérifie l'existence des anciens membres desktop et des nouveaux guards desktop/bringup, recherche tout accès `render/rootRender/r` depuis `ESP32/src` et `ESP32/include` et préserve les tests historiques.
- Le test réel extrait `EspNativeEngineRoot_initRender` (constructeur + 4096 cycles + allocation/OOM) et accepte son nouvel FNV **`1e6db3c5`** après retrait de l'affectation morte ; il refuse le retour du champ supprimé.
- Tout échec de tests ou de firmware est corrigé **sur la même branche**. Les trois builds `esp32-cyd`, `esp32-cyd-bringup` et trace doivent rester verts. N'affirmer aucune compilation locale ni PASS hardware tant que non prouvés.

## Protocole hardware — pas encore exécuté

1. Récupérer la branche `agent/esp32-render-retire-legacy-map-header`, effectuer `pio run -e esp32-cyd -t clean`, puis `pio run -e esp32-cyd -t upload` et monitor Serial complet. **Normal** uniquement pour les chiffres RAM ; le bringup modifie les allocations.
2. Au boot, vérifier `Engine structs: Render=1256 Game=0 Canvas=44 Total=2060`, `[RENDERCORE] INIT ... bytes=1256`, `[CORE] READY ... used=2224`, ESP32-D0WD-V3 240 MHz, 4 MB flash, PSRAM zéro, PAK 241, mappings 152 textures/252 sprites, et `shapeData=mediaTexels=0x0` au START/LOAD/EXIT.
3. START intro → Entrance : première frame FNV `71ca7465`, arène `14095/c3882516` ; caisses, pickups/flash pulsé, dialogues, porte275 animée, sprites CROSS avec `copies=4 accounting=1`, HUB et EXIT. Explorer event60 si accessible, sans l'exiger d'un parcours court.
4. LOAD slot04 V11 : exact monster/topologie/positions/activation `beaeaccc/50e16fe3/e2a9d90a/df419482`, porte113, Malwrath322 HP11→7→3→0, ripostes projectile204 **drainées avant COMMIT**, drop/pickup, secret +5XP, EXIT. Vérifier **2×** `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=0f2c9fc3 saveWrite=no`, deux `RAMBUDGET ... HEADROOM_OK`, pas de panic/WDT ni unsupported positif.
5. Comparer les heap8 aux références #245B menu **153196**, START prime **112916**, LOAD prime **112104**, EXIT **158040** et le largest8 gameplay **86004** / menu **110580** ; noter les valeurs réelles (pas de bénéfice déduit par simple différence `sizeof`). Ressenti **nervosité** prioritaire ; relever `SPRITEORDER.scanUs`, `VIEWANIM FRAME totalUs` et `MONSTERCOMBAT FRAME totalUs`, sans prétendre à un A/B causal entre parcours non identiques.
6. Si le vrai CYD confirme le code précis, **geler le code**, ajouter seulement les mesures et limites réelles à la documentation, vérifier diff docs-only et les CI post-doc, puis rendre l'unique PR Ready for review. **L'utilisateur merge lui-même.**

**Statut actuel : CANDIDAT, CI et test matériel non encore acquis.** Trois diagnostics `NATIVEFRAME BOUNDARY-INLINE` et l'offset x affiché des projectiles sont des témoins préexistants ; ne pas attribuer un éventuel diagnostic à ce retrait sans démonstration. Audio, ouverture automap, toute carte, SAVE neuf et endurance restent hors du PASS ciblé.
