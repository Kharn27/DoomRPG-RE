# PR #246C — retirer les ultimes résidus raster desktop évidents de Render_t (CANDIDAT)

## Lignée et source de vérité

Repo Kharn27/DoomRPG-RE, **main exact** au début de #246 : \`3142432f5b6870c5a8579a5cd293f82e39f0fa8f\`. **Même** branche \`agent/esp32-render-retire-legacy-map-header\`, **même PR #246**. A (en-tête BSP) et B (quatre compteurs BSP desktop) ont passé le vrai classic CYD normal \`esp32-cyd\`. B code de départ à la fermeture : \`a4dd92552eee3121ab6c8d4277fc89e581fa3dfb\` ; commit de documentation PASS B \`c988ff2e0d4d90330a8435d0c7a9f11597ed644e\`. CI B [push](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41528336540) et [PR](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41528344232) SUCCESS.

Le vrai log B du 2 novembre rapporte Render=1240, CORE used=2208, heap8 menu/START prime/LOAD prime/EXIT **153240/112960/112148/158084**, largest8 gameplay/menu **86004/110580**. START intro/Entrance, trois dialogues, portes/CROSS/ressources et EXIT. LOAD slot04 V11 exact, trois ripostes triple projectiles Malwrath204 avec ordonnancement intact avant dégâts, mort/drop/secret et EXIT ; deux RESIDENTRESET released=18008 empty=1, deux menus 0f2c9fc3, shapeData/mediaTexels NULL, HEADROOM_OK. Retour utilisateur « Visuellement RAS ». Aucune mesure causale de nervosité ou test audio/SAVE neuf/endurance.

## Frontière C

- \`src/Render.h\` : quatre membres de raster **uniquement desktop** derrière \`#ifndef DOOMRPG_ESP32\` : \`floorTex\`, \`ceilingTex\` (matériaux chargés par \`Render_beginLoadMapData\` desktop), \`damageBlend\` (effet de sprite/arme desktop), \`animFrameTime\` (horloge desktop BSP/sprites). Le champ inconnu \`unk3\` reste lui-même gardé desktop. Original \`src/Render.c\` et ses opérations gardés inchangés.
- \`ESP32/src/engine_stubs.c\` : constructeur \`EspNativeEngineRoot_initRender\` supprime les écritures \`ceilingTex/floorTex/animFrameTime\`, déjà redondantes avec le \`SDL_memset\`. \`damageBlend\` était sans écriture native. Process root, allocations, audio, palettes et autres initialisations ne bougent pas.
- Test source strict \`native_render_raster_abi_retirement_host.py\` : vérifie déclaration desktop, conservation des champs natifs réellement utilisés et absence de référence de ces quatre champs dans tous les fichiers natifs; l'ABI/guard \`unk3\` historique et les trois assertions de taille sont mis à jour. \`native_render_init_retirement_host.py\` exécute le constructeur exact 4096 fois ; fingerprint source C extrait \`c0b5cbfc\`, OOM, instance préallouée et allocation directe.
- \`_Static_assert(sizeof(Render_t)==1228U)\` sur CYD normal/trace : **−12 B de structure** vs B ; taille mémoire réellement allouée et heap8 à mesurer. \`CORE used=2196 B\` est un calcul attendu, **pas une observation**. Aucun nouveau cache, aucune lecture ZIP, aucune dépendance map-wide.

**Commits code/fixture** : \`272b6c0bb2f764b0938395250dbe865a94f7d730\` ; corrections test \`09631ff87a2a07a28808c9194292e7550c295bf0\`, \`7710aefc6e5dfdcdcf939216c8a9930cb9f2b47b\`. CI push C [#41540422502](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41540422502) **SUCCESS 78/78**, incluant normal/bringup/trace + hôtes. CI après docs à vérifier aussi. Les échecs initiaux de deux CI C portaient sur deux anciennes assertions hôtes (guard \`unk3\` et taille \`1240U\`), corrigées avant ce PASS logiciel.

## Protocole de validation normal CYD

1. Rester sur \`agent/esp32-render-retire-legacy-map-header\`, aucune PR neuve. Vérifier \`git rev-parse HEAD\` et la CI du dernier HEAD C. Depuis \`ESP32/\` : \`pio run -e esp32-cyd -t clean\` puis \`pio run -e esp32-cyd -t upload\`, enregistrer log Serial complet à 115200.
2. Attendu boot : \`Engine structs: Render=1228 Game=0 Canvas=44 Total=2032\`, \`RENDERCORE INIT bytes=1228\`, \`CORE READY used=2196\`, PSRAM0, PAK241, mappings 152/252 et \`shapeData/mediaTexels=NULL\`. **Non encore mesuré.**
3. START intro/Entrance : \`MAPRT arena=14095 FNV=c3882516\`, \`FIRST_FRAME=71ca7465\`, caisse/pickups avec flash pulsé, dialogues88/89/79 et reprise opcode19, porte275 puis fermeture après tour, CROSS, HUB/EXIT.
4. LOAD slot04 V11 : monstres/topologie/position/activation \`beaeaccc/50e16fe3/e2a9d90a/df419482\`, porte113, Malwrath322 11→7→3→0, projectiles204 triples drainés **avant** dégâts, drop/pickup, secret +5XP, HUB/EXIT.
5. Deux \`RESIDENTRESET released=18008 empty=1\`, deux \`SYSEXIT MENU-READY frame=0f2c9fc3\`, HEADROOM_OK et aucun WDT/panic. Comparer heap8 réel et largest8 avec B ci-dessus, mais ne pas déduire un gain de vitesse à partir de trajets non identiques ; recueillir le ressenti visuel/rythme.
6. **C non PASS sans nouveau log sur CYD**. Après PASS, ne faire que la documentation de clôture, contrôler CI post-doc et diff code-free, mettre la même PR #246 review-ready, laisser le merge à l'utilisateur. Diagnostics NATIVEFRAME BOUNDARY-INLINE et x=0x40000000 du projectile persistent hors périmètre.

**Statut actuel : C CI PASS ; matériel non testé, PR #246 Draft.**
