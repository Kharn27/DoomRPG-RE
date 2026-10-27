# Doom RPG ESP32 — état actuel du port

## PR #234 / jalon A — culling BSP à scratch privé, PASS classic CYD (27 octobre 2026)

**Code du jalon matériellement éprouvé : `6f278e4fa7919a4e90da4914d530abeb470c851d`** sur branche `agent/esp32-bsp-cull-private-scratch`, créée depuis le `main=47d30a74f830720590527b6edc6459d97cf66f6d` (merge #232). [PR #234](https://github.com/Kharn27/DoomRPG-RE/pull/234) ; [CI code #40410357224](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40410357224) **SUCCESS** : différentiel géométrique C (10 000 cas), gardes de 23 ABI Render et builds `esp32-cyd` normal, bringup, trace. Les logs Serial fournis ne contiennent **pas de fingerprint embarqué du SHA** ; l'attribution au SHA vient de la branche/code préparé et du parcours de validation demandé, pas d'une auto-identification binaire.

**Frontière A :** `EspNativeRenderGeometry_cullBounds(view, x1,y1,x2,y2)` ne reçoit plus de `Line_t` / `struct Line_s* scratch` ni de copie de `Render_t::tmpLine`. Le seul `Line_t localLine={0}` naît dans le culler ; ses x/y sont écrits avant le clip et les z issus de l'ancien `tmpLine` n'influençaient pas le booléen de culling. `esp_native_first_frame.c::walkNodes` et `esp_native_bsp_visibility.c::walkNode` lui transmettent les bornes `EspMapNode` avec casts `(short)`. Les colonnes d'occlusion restent live et consultées sans mutation ; les transforms/frustum/projection, porte z animée, plancher/plafond, `Render_t` restant, `shapeData/mediaTexels=NULL`, RNG, gameplay, PAK et SAVE ne changent pas. Le comparateur hôte conserve l'oracle desktop pour la décision et exige `native.tmpLine` inchangé ; l'oracle peut muter son propre `tmpLine` sans incidence sur la décision.

**Vrai CYD, parcours START → INTRO → Entrance → EXIT :** menu Start armé, player fresh `e745fce9`, intro `ade0195d`, dispose `recovered=34056`, BSP `/intro.bsp` CRC `623f34e4`, arène `14095/c3882516`, **FIRST_FRAME `71ca7465`** (8 murs, 4430 px), `RESIDENTGAMEPLAY READY`. Avance/rotation/strafe, caisse127 TRANSFORM en Armor Shard puis ramassage, armure `0→4→8→12`, dialogue88 opcode8 avec reprise opcode19 `stateMutation=1`, porte275 ouverte 4 frames et navigation après ouverture ; EXIT sans SAVE, `DIALOGCHAIN OWNER-RELEASE recovered=1144`, `RESIDENTRESET released=18008 empty=1`, menu FNV `0f2c9fc3`, heap8/largest8 `159872/110580`.

**Vrai CYD, LOAD slot04 V11 → Malwrath → EXIT :** `5604B`, trente monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation restaurée 13 puis 14 après porte113. Clipping sprite324 copies4, dessins `4→2→0`, quatre frames porte113 + fermeture HOLD/RELEASE post-tour. Malwrath322 : **deux salves de trois projectiles204**, drainage intégral avant `MONSTERRETAL COMMIT`, joueur HP `34→27→21`, armure `23→16→10`. Deux coups shotgun : Malwrath HP `11→7→0`, mort `death4→corpse2`, Armor Shard drop au tile285 et ramassé ; Bullet Clips sprite324 ramassées, Flak Jacket, secret portes112/111 +5 XP. EXIT sans SAVE : `RESIDENTRESET released=18008 empty=1`, menu `0f2c9fc3`, heap8/largest8 `159872/110580` stable. Gameplay `113936/86004` hors journal ; START avec journal `112792/86004`. `shapeData=mediaTexels=NULL` aux frontières diagnostiquées.

**Observations et limites :** aucun `WORLD_POST fail`, panic ou `RNG_REPLAY_DIVERGED` visible dans ces deux extraits. `NATIVEFRAME BOUNDARY-INLINE` et abscisse `MALWRATHPROJECTILE` supérieure de `0x40000000` à la position `NATIVESPRITE TRANSIENT` restent **préexistants, non corrigés et non qualifiés comme inoffensifs**. Déplacements `~103–126ms` par preview, rotation deux frames `~219–227ms`, sensations encore « un poil molles » : pas de démonstration A/B ni de modification de cadence ou `PlatformVideo_present()`. Pas de parité exhaustive toutes cartes, SAVE neuf, test d'endurance ou fingerprint du firmware. **Gel du code du jalon A au SHA `6f278e4f` : fermeture par documentation uniquement ; le prochain jalon B sera un nouveau changement de code sur la MÊME branche, avec sa propre CI et son test CYD ; merge exclusivement utilisateur.**

---


## PR #232 — traversée BSP depuis EspMapNode sans Node_t desktop, PASS ciblé CYD (26 octobre 2026)

**Deuxième campagne Serial CYD START/EXIT + LOAD slot04/EXIT (26 octobre 2026, après PASS initial, mêmes frontières #232) :** première frame Entrance toujours `71ca7465` / 8 murs / 4430 px, intro frame `ade0195d` et disposal `34056B`, arène `14095/c3882516`. Caisse127 BREAK_REMOVE, caisse56 TRANSFORM puis Armor Shard au tile867 ; pickups armure `0→4→8→12`. Dialogues 88 et 89 `EV_DIALOG(8)` suivis par `EV_NEXTSTATE(19)` (`stateMutation=1`), porte275 ouverte et refermée après séquence HOLD/RELEASE. LOAD V11 slot04 `5604B`, `MONSTERSTATE=beaeaccc`, topo `50e16fe3`, pos `e2a9d90a`, activations `13→14`, porte113 ouverte/fermée, sprite324 quatre copies (`draws=4→2→0`). Malwrath322 : shotgun HP `11→8→4→0`, deux ripostes de 3 projectiles204 entièrement drainées avant `MONSTERRETAL COMMIT` (`playerHP 34→30→25`, armure `23→19→14`), drop Health Vial puis pickup, ammo sprite324 ramassée. Chaque EXIT : `RESIDENTRESET 141864→159872 released=18008 empty=1`, menu FNV `0f2c9fc3`, `heap8/largest8=159872/110580`; gameplay `113936/86004`, `shapeData=mediaTexels=NULL`, SAVE non écrit. Aucun `WORLD_POST fail`, panic ou divergence RNG dans cette série. `NATIVEFRAME BOUNDARY-INLINE` et coordonnée x de `MALWRATHPROJECTILE` restent des traces déjà connues, non corrigées ici. Cette deuxième série n'a pas reproduit le secret +5XP (couvert dans le PASS initial). L'extrait Serial ne contient pas de fingerprint du binaire : il confirme la campagne #232, **sans prouver indépendamment un SHA du firmware**. Aucun changement de code après les PASS ; pas d'affirmation de pixel-parité exhaustive ni d'amélioration de performance.

**Code exact flashé et exécuté sur le vrai classic CYD sans PSRAM (firmware normal `esp32-cyd`) : `a6bbb6b44d55ce58148482114c57ec68ba3d178d`**, branche `agent/esp32-bsp-native-node-direct`, base exacte du vrai `main=3f6875ee7f2d3572eff867e2426c5072dc0e267c` après merge #231 ; [PR #232](https://github.com/Kharn27/DoomRPG-RE/pull/232). [CI du code #40312993500](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40312993500) **SUCCESS** : gardiens d'ABI/tests hôtes et builds `esp32-cyd` normal/bringup/trace. **Aucun test matériel n'est revendiqué au-delà des deux parcours Serial ci-dessous.**

**Contrat natif :** `esp_native_first_frame.c::walkNodes` et `esp_native_bsp_visibility.c::walkNode` consomment directement le `EspMapNode compact` immutable retourné par `EspMapRuntime_getNode` ; plus de `Node_t` desktop temporaire ni de `memset`/copie de champs `args1/args2` dans ces traversées. Les quatre bornes sont passées au `EspNativeRenderGeometry_cullBounds` avec des casts explicites `(short)` identiques à l'ancien élargissement signé 16 bits. L'ordre de récursion/front-to-back, `args1/args2`, le scratch local dérivé de `render->tmpLine`, la lecture live de `columnScale`, les portes z animées et les autres références au petit `Render_t` restent inchangés. Garde hôte ajouté à `native_render_raster_abi_retirement_host.py`. Aucun nouveau backing ZIP, allocation, entité/world mutation ou framebuffer ; `shapeData` et `mediaTexels` restent NULL.

**START → INTRO → Entrance → EXIT, preuve matérielle :** intro `ade0195d`, libération intro `recovered=34056`, `/intro.bsp` CRC `623f34e4`, arène `14095/c3882516`, `FIRST_FRAME=71ca7465` (8 murs/4430 pixels), `RESIDENTGAMEPLAY READY`. Déplacements/rotations, caisse127 `BREAK_REMOVE`, caisse56 `TRANSFORM`, Armor Shards `0→4→8`, dialogue événement82 opcode8 puis continuation opcode19 `stateMutation=1`, porte275 ouverture quatre frames et fermeture différée HOLD/RELEASE post-tour monstre. EXIT sans SAVE, `DIALOGCHAIN OWNER-RELEASE recovered=1144`, `RESIDENTRESET released=18008 empty=1`, menu `0f2c9fc3`, `heap8=159872/largest8=110580` stable sur deux témoins ALIVE.

**LOAD slot04 V11 → Malwrath → EXIT :** `5604 B`, restauration exacte de 30 monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `13→14` après ouverture porte113 (quatre frames). Deux ripostes Malwrath322 à **trois projectiles204 chacune** correctement drainées avant chaque `MONSTERRETAL COMMIT` : HP joueur `34→30→25`, armure `23→19→14`; shotgun HP monstre `11→8→4→0` avec mort `death4→corpse2`, drop Health Vial créé puis ramassé, secret portes112/111 +5XP. Sprite324 `copies=4`, `draws 4→2→0` lors du rapprochement ; porte113 fermeture HOLD/RELEASE quatre frames. EXIT sans SAVE, `RESIDENTRESET released=18008 empty=1`, MENU `0f2c9fc3` et `heap8=159872/largest8=110580` stables. `heap8=113936/largest8=86004` pendant les deux sessions hors journal dialogue ; `shapeData=mediaTexels=NULL`.

**Limites non modifiées :** pas de `WORLD_POST fail`, panic ni divergence RNG apparents dans les logs transmis. `NATIVEFRAME BOUNDARY-INLINE` préexistant se reproduit sans autre faute ; `MALWRATHPROJECTILE` affiche encore un x décalé de `0x40000000` face à `NATIVESPRITE TRANSIENT`, observation de journal distincte, pas une preuve de corruption. Cadence peu nerveuse à investiguer séparément : MOVE preview `115–141ms` environ, TURN à deux previews `~249–256ms`, LARGE-WARM START `123.435ms` / LOAD `101.106ms`, sans A/B contrôlé ni attribution causale à #232. Aucune preuve all-maps, SAVE neuf, pixel-parité exhaustive, test d'endurance ou évaluation visuelle explicite dans ce message. **Geler le code matériel à `a6bbb6b4` : commits suivants documentation uniquement sur #232 ; vérifier CI post-doc avant ready-for-review ; merge exclusivement utilisateur.**

---

## PR #231 / 2e jalon — culling BSP indépendant de Render_t, PASS CYD (26 octobre 2026)

**Code exact testé matériellement en `esp32-cyd` : `efad8a2ffe018bb8319298e29b20065845c5360e`**, branche `agent/esp32-retire-last-render-clip-abi`, base `main=4e9efc4b064ce65eb311fbaa4fedd4b4f84632a6`. CI [push #40236074792](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40236074792) et [PR #40236078052](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40236078052) **SUCCESS**, différentiel hôte 10 000 cas, 23 ABI Render et builds normal/bringup/trace. Ce PASS est **postérieur et distinct** du premier PASS clipping `1c1a715e8c23f562f53f301b02449e8ada8b09ce` documenté ci-dessous.

**API native validée :** `EspNativeRenderGeometry_cullBounds` ne reçoit plus `Render_t` ni `Node_t` : vue `EspNativeRenderCullView` capturée une fois par traversal, colonne d'occlusion `const int*` consultée en direct et scratch de ligne local (copié depuis `render->tmpLine` avant chaque cull). Le culler ne touche plus le vrai `tmpLine`. Deux consommateurs : `esp_native_first_frame.c` et `esp_native_bsp_visibility.c`, les deux ordres récursifs couverts. Clipping interne pur, adaptation de z des portes animées seulement via l'adaptateur explicite. Les autres usages du graphe Render n'ont pas été retirés.

**START / INTRO / Entrance / EXIT :** `FIRST_FRAME=71ca7465`, BSP CRC `623f34e4`, arène `14095/c3882516`, MOVE/TURN/STRAFE, caisse127, pickups armure 0→4→8→12, dialogue88 BACK-cancel, porte275 4 frames et HOLD/RELEASE différé ; EXIT `RESIDENTRESET released=18008 empty=1`, MENU `0f2c9fc3`. **LOAD slot04 / Malwrath / EXIT :** monstres30 `beaeaccc`, topologie `50e16fe3`, position `e2a9d90a` restaurés ; porte113 4 frames, activation monstre322 13→14, clipping sprite324 `draws 4→2→0`, quatre ripostes Malwrath à trois projectiles204 chacune résolues après animation, shotgun HP monstre `11→8→5→0`, drop ramassé, secret +5XP, sortie `empty=1`, même MENU FNV. Utilisateur : « pas vu de glitch ou autre ».

**RAM / limites :** `shapeData=mediaTexels=NULL`, `heap8=113936/largest8=86004` en gameplay, MENU `159872/110580` après EXIT, sans erreur `WORLD_POST`, panique ou anomalie RNG visible. Fluidité jugée encore peu nerveuse : previews MOVE ~102–128ms, TURN ~219–234ms, LARGE-WARM START 117.846ms / LOAD 81.884ms (sans A/B contrôlé). `NATIVEFRAME BOUNDARY-INLINE` et abscisse loggée des projectiles restent à auditer séparément. Ne pas inférer all-maps, pixel parity exhaustive, SAVE neuf ou gain de performance. **Code hardware figé au SHA `efad8a2f` ; commits suivants docs-only, CI post-doc, merge exclusivement par l'utilisateur.**

---

## PR #231 — dernière ABI Render_clipLine retirée : PASS matériel CYD (25 octobre 2026)

**Code exact testé en firmware normal `esp32-cyd` : `1c1a715e8c23f562f53f301b02449e8ada8b09ce`**, branche `agent/esp32-retire-last-render-clip-abi` créée à partir du vrai `main=4e9efc4b064ce65eb311fbaa4fedd4b4f84632a6` (merge #230). [PR #231](https://github.com/Kharn27/DoomRPG-RE/pull/231). CI **SUCCESS** sur ce même code, [push #40215912165](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40215912165) et [PR #40215950660](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40215950660) : tests hôte, différentiel géométrique sur 10 000 cas, garde des **23 ABI Render retirées**, builds PlatformIO normal/bringup/trace. Les SHAs intermédiaires en échec CI, avant correctifs du comparateur de types et du garde header, ne sont **pas** des firmwares validés.

**Frontière native permanente :** `Render_clipLine(Render_t*,Line_t*)` ne fait plus partie de l'ABI ESP32 et son ancien `--wrap=Render_clipLine` est retiré ; le prototype reste uniquement desktop, et le corps de `src/Render.c` reste oracle inchangé. `EspNativeRenderGeometry_clipLine(struct Line_s*)` effectue en place les six chemins de clipping frustum 16.16 avec interpolation x/y/z, ordre/aliasing des sommets hérités, sans `Render_t` en argument. `EspNativeDynamicLineRender_clipLine` préserve **explicitement** le traitement `z` transitoire des portes animées, les comparaisons de `builtZ/desiredZ`, et le diagnostic fail-closed de géométrie ; tous les points de clipping murs, profondeur BSP et sprites y passent. À l'inverse, le clipping de culling bounds, appelé à l'intérieur du même fichier C et jamais intercepté par le wrapper linker historique, reste **pur**. Ne jamais déplacer l'adaptation de porte dans le clipper pur ou modifier son ordre d'appel sans nouveau jalon. Aucune réintroduction de graphes legacy/ZIP runtime/`shapeData`/`mediaTexels`, framebuffer, allocation, RNG ou mutation gameplay.

**Preuve physique START → INTRO → Entrance → EXIT :** menu Start armé `fec57f30`, PlayerState fresh `e745fce9`, premier écran intro `ade0195d`, dispose `recovered=34056`. `/intro.bsp` CRC `623f34e4`, arène `14095/c3882516` ; `ENGINESESSION FIRST_FRAME=71ca7465` (8 murs, 4430 pixels) puis `RESIDENTGAMEPLAY READY`. PASS_TURN, mouvements/rotations/strafes y compris preview, caisse127 retirée puis caisse56 transformée, deux Armor Shards armure `0→4→8`, dialogue88 opcode8 affiché et continuation opcode19 `stateMutation=1`, porte275 ouverte en **4 frames**, fermée après HOLD/RELEASE à la fin du tour monstre. Aucun `WORLD_POST fail`, panic ou divergence RNG signalés. EXIT sans SAVE : journal dialogue libéré `recovered=1144`, `RESIDENTRESET released=18008 empty=1`, MENU `0f2c9fc3`, heap8 menu `159872`, largest8 `110580`.

**Preuve physique LOAD V11 slot04 → Malwrath → EXIT :** checkpoint `5604 B`, mêmes CRC/arène ; restauration exacte des **30 monstres** `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `13→14` après ouverture porte113. `NATIVESPRITE CROSS` sprite324 quatre copies avec `draws=4→2→0` et clipping explicite, selon proximité ; porte113 ouverture **4 frames**, fermeture HOLD/RELEASE **4 frames** post-monster-turn. Shotgun trois attaques sur Malwrath322 : HP monstre `11→8→4→0`, mort `death4→corpse2`, drop Health Vial matérialisé/ramassé, trois projectiles204 par riposte sur deux ripostes complètes, résolution `MONSTERRETAL COMMIT` après vidage projectile : HP joueur `34→30→25`, armure `23→19→14`; preflight `rngRollback=yes`, commit RNG seulement au bon moment. Secret portes112/111 +5 XP, autres pickups et mouvements, EXIT sans SAVE `RESIDENTRESET released=18008 empty=1` puis MENU `0f2c9fc3`. Heap8 en gameplay `113936/largest8 86004` stable sur les `ALIVE` sans journal ; START descend temporairement à `112792` avec propriétaire dialogue. `shapeData=mediaTexels=NULL` aux frontières instrumentées.

**Limites / suivi :** verdict utilisateur **« visuellement c'est ok, pas de régression »** sur ces deux parcours, pas une parité de pixels prouvée pour toute la carte ou toutes les maps, ni SAVE neuf, ni test exhaustif des variations des portes. `NATIVEFRAME BOUNDARY-INLINE` est préexistant et observé pendant START sans fail. Décalage préexistant des abscisses `MALWRATHPROJECTILE` de `0x40000000` par rapport à `NATIVESPRITE TRANSIENT` encore visible, **non corrigé** et ne démontre pas un bug de trajectoire. **Nervosité :** l'utilisateur juge le jeu jouable mais pas encore assez nerveux ; MOVE preview `~103–116ms`, TURN total `~216–232ms`, LARGE-WARM `92.299ms` (START) / `83.002ms` (LOAD), chiffres par parcours non contrôlés, sans établir de gain/perte causal par ce jalon. Optimisation uniquement après profilage séparé PAK/cache/sprite/input ; ne pas toucher `PlatformVideo_present()` ou au flash dégâts par intuition. **Code de `1c1a715e` gelé après PASS, commits suivants documentation seulement ; CI post-doc requise ; merge par l'utilisateur, jamais automatique.**

---

## PR #230 / Rendu unifié des dégâts — PASS visuel 4 côtés sur CYD (25 octobre 2026)

**Code exact flashé en firmware normal `esp32-cyd` et accepté visuellement par l'utilisateur : `27453985033a830f41bf2b516d33fa252c1426fc`.** Branche `agent/esp32-damage-cue-border-independence`, base exacte `main=fc75b87cdb3edfaeae974acd8a0141fc47b55457`, [PR #230](https://github.com/Kharn27/DoomRPG-RE/pull/230). CI sur le code réellement testé : [push #40208925698](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40208925698) et [PR #40208929067](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40208929067) **SUCCESS**, avec tests C hôte, comparaison d'ABI et builds PlatformIO normal/bringup/trace. Verdict utilisateur : « le rendu que je voulais est parfaitement obtenu », avec réserve explicite sur la nervosité générale. Cette preuve **remplace la conception finale décrite dans le sous-jalon #230 précédent ci-dessous** : l'ancienne bande autonome/FIFO et son extinction indépendante ont été retirées, elles ne décrivent PAS le code final testé.

**Architecture validée :** un seul `viewportFlash` propriétaire du contour opaque rouge RGB565 `b800` de **2px / 944 pixels** et de l'extension directionnelle intérieure translucide **10px** (épaisseur côté totale **12px logiques**, RGB565 blend 50 % avec `fa20`). L'horloge unique de **500ms** commande conjointement trois impulsions demandées **105ms ON / 65ms OFF**, puis restitution commune au plus tard à l'expiration ; les flashes blancs de pickup restent continus pendant 500ms. Aucun FIFO de direction, scheduler/timestamp indépendant, tâche FreeRTOS, seconde animation directionnelle, allocation heap, PAK/SD I/O ou framebuffer supplémentaire. Snapshot statique limité à **1560 texels/3120 B** pour l'extension + contour existant. Les tests hôtes couvrent les quatre côtés, les phases ON/OFF, pixels restaurés, changement d'attaque, retry des présentations, fresh world redraw ON/OFF, entrée AUTOMAP/touch/menu, et la continuité des flashes blancs. Les durées réelles de chaque ON/OFF ne sont **pas** mesurées indépendamment sur CYD par le Serial normal ; le retour visuel confirme néanmoins le clignotement synchronisé.

**Preuve Serial physique LOAD → combat → EXIT :** slot04 V11 `5604B`, carte1 `/intro.bsp` CRC `623f34e4`, arène `14095B/c3882516`, monstres 30 `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation 13→14 ; choix shotgun, déplacement tile281→282, Health Vial 33→34, porte113 quatre frames, Malwrath sprite322 **4 ripostes réelles**, trois tirs projectile204 par riposte. `DAMAGEBORDER DIRECTION/PAINT` en ordre **FRONT probe1 / RIGHT probe2 / BACK probe3 / LEFT probe4** ; dans les quatre cas `innerAlpha=50pct`, `thickness=12`, `additionalPresent=no`, `VIEWFLASH PAINT blink=3x105ms-on/65ms-off` et `EXPIRE elapsedMs=503/502/502/500 targetMs=500 restored=one-border-including-direction`. `MONSTERRETAL COMMIT` joueur HP **34→28→23→17→7**, armure **23→17→12→6→0**, avec `rngRollback=yes` sur les préflights, puis commit et tournants cohérents. Les FNV framebuffer des frames de riposte varient selon pose et contexte comme attendu ; ne pas revendiquer une parité générale de rendu sur toutes les cartes.

**Fin de session et RAM :** `RESIDENTRESET ... released=18008 ... empty=1` ; EXIT confirmé sans écriture SAVE, menu `frame=0f2c9fc3`, `heap8=159872 largest8=110580`. Gameplay `heap8=113936 largest8=86004` stable dans les témoins `ALIVE`; `shapeData=mediaTexels=0` au ready et cleanup. Aucune faute de RNG, crash ou résidu visuel mentionné dans le parcours. Ne pas attribuer de preuve à START/INTRO pour **ce** SHA (essais précédents uniquement), ni à sauvegarde neuve, gauche sur d'autres monstres ou toutes cartes.

**Observation à suivre distinctement — cadence ressentie trop lente :** même checkpoint sur l'essai de code précédent `59624eeb`, LARGE-WARM `64.370ms` (sprite `28.087ms`) et mouvement `90.149ms`, rotation témoin `185.563ms` ; maintenant LARGE-WARM `83.508ms` (sprite `46.843ms`), mouvement `102.668ms`, rotation `219.432ms`. Comparaison NON contrôlée entre runs, mais temps sprite accrus dès le chargement avant tout clignotement : le rendu de dégâts n'est pas une cause démontrée. Ne pas toucher prématurément `PlatformVideo_present()` ni altérer le nouveau flash visuellement validé pour « cacher » la régression ; mesurer PAK/cache/sprite et la cadence input/render dans un jalon borné séparé. Les anciens logs `MALWRATHPROJECTILE x=10737436xx` et `NATIVEFRAME BOUNDARY-INLINE` restent des sujets préexistants hors périmètre. **Code visuel gelé au SHA testé : seulement documentation post-PASS. Performance/nervosité à suivre explicitement, merge utilisateur uniquement.**

---

## PR #230 — séparation pixel-exclusive des signaux de dégâts, PASS CYD ciblé (25 octobre 2026)

**Code firmware normal `esp32-cyd` validé visuellement par l'utilisateur sur le vrai classic CYD sans PSRAM : `b1e7733ca81e0dbdf8e40d4586071243426c8a75`.** Branche `agent/esp32-damage-cue-border-independence`, base du vrai `main=fc75b87cdb3edfaeae974acd8a0141fc47b55457` (merge #229), [PR #230](https://github.com/Kharn27/DoomRPG-RE/pull/230). CI du SHA de code : [push #40139413687](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40139413687) et [PR #40139418203](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40139418203) **SUCCESS** (tests host et builds normal/bringup/trace). Le tout premier essai au SHA `f2a3c35c83f6d2af0f48526cdc9119d61c0cb708` était **FAIL visuel matériel** : un fragment du cadre rouge restait après son extinction. La correction du mélange des couleurs ne suffisait pas : la bande et le cadre possédaient encore des pixels communs. **Ne pas considérer ce premier SHA comme validé.**

**Frontière finale :** dans `esp_native_gameplay_action_engine.c`, le cadre rouge conserve ses deux pixels de périmètre du viewport logique 160×80 (944 pixels) et la bande directionnelle conserve 10 pixels d'épaisseur mais occupe seulement l'intérieur : FRONT y22..31 / x2..157, BACK y88..97 / x2..157, LEFT x2..11 / y22..97, RIGHT x148..157 / y22..97. Leurs ensembles de pixels sont disjoints, y compris aux coins. Suppression de la compensation précédente par lecture du snapshot du cadre ; chaque bande continue de restaurer uniquement ses propres pixels. Le flash du périmètre peut maintenant expirer indépendamment d'une bande encore allumée ou de la file des dégâts. Pas de nouveau framebuffer, de BSS ajouté, d'allocation, de lecture PAK, de modification RNG/monster turn/HP/SAVE ni de retard de gameplay. Les tests hôte gardent FRONT/RIGHT/BACK/LEFT, deux impulsions, coexistence de plusieurs impacts, les deux ordres de restauration, et surtout **extinction du cadre alors que la bande frontale est toujours allumée** avec restauration exacte sans résidu.

**Serial matériel utilisateur — LOAD V11 slot04 → Entrance → Malwrath (FRONT puis RIGHT) :** checkpoint `5604 B`, carte1 `/intro.bsp`, BSP CRC `623f34e4`, arène `14095 B / c3882516` ; restauration des 30 monstres et positions `beaeaccc/e2a9d90a`, topologie `50e16fe3`, activation 13→14, session visible. Health Vial 33→34/34, sélection shotgun depuis HUB, porte113 ouverte en quatre frames, Malwrath sprite322 et animation à trois tirs projectile204, puis deux `MONSTERRETAL COMMIT` : **HP joueur 34→29 puis 29→23, armure 23→18 puis 18→12**. Première direction `[DAMAGECUE] QUEUE/ON side=FRONT probe=1 redBorder=visible`, `DONE onMs=122/143 gapMs=79 delayed=no`, `[VIEWFLASH] EXPIRE elapsedMs=503 targetMs=500`. Deuxième direction `RIGHT probe=2`, `DONE onMs=123/148 gapMs=79 delayed=no`, `VIEWFLASH EXPIRE elapsedMs=504 targetMs=500`. Les périodes observées sont supérieures aux 85/45 ms nominaux du fait de la cadence de service ; **ne pas les présenter comme des 85/45 ms exacts**. L'utilisateur confirme « Pour moi c'est ok visuellement ». Le cadre de 500 ms et les bandes finissent proprement dans ce parcours, **sans fragment rouge restant constaté**.

**Invariants et limites :** `heap8=113768` / `largest8=86004` sur les `ALIVE` pendant gameplay avant/après les impacts, `shapeData=mediaTexels=0` à `ENGINESESSION READY`; aucun crash, faute d'invariant ou divergence RNG apparente dans l'extrait. Le log projectile x `10737436xx` face aux transients x `18xx/17xx` reste une anomalie de représentation **préexistante et non corrigée**, sans preuve de trajectoire incorrecte. Le test présent couvre visuellement **FRONT et RIGHT uniquement**, pas LEFT/BACK sur hardware, pas de SAVE/EXIT dans l'extrait, pas de preuve toutes cartes/pixels/endurance. **Geler le code à `b1e7733c` ; après ce PASS, seulement commits docs et CI post-doc. PR merge-ready après vérification, merge par l'utilisateur uniquement.**

---

## PR #229 / Jalon J — interpolation de clipping privée, PASS CYD (24 octobre 2026)

**Code firmware `esp32-cyd` effectivement testé sur le classic CYD sans PSRAM : `70e8af2b66357936eb6e6df39ad98b794e59b949`.** Branche `agent/esp32-native-point-transform`, base main `d249bb5ab6002e93b498bf12459162b4f502f7c5`, [PR #229](https://github.com/Kharn27/DoomRPG-RE/pull/229). CI de ce code : [push #40133269693](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40133269693) et [PR #40133272703](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40133272703) **SUCCESS** (host différentiel, guards, builds normal/bringup/trace). `Render_clipVertex(Render_t*,Vertex_t*,Line_t*,int,int)` retiré de l'ABI ESP32, déclaration `src/Render.h` rendue desktop-only, implémentation remplacée par `static clipVertex(Vertex_t*,Line_t*,int,int)` appelée par les six branches de `Render_clipLine` ; arithmétique `FixedDiv/FixedMul` x/y/z et aliasing des sommets inchangés, desktop `Render.c` toujours oracle. **21 ABI Render retirées, 2 primitives legacy encore exportées** (`Render_cullBoundingBox`, `Render_clipLine`). Ce changement n'affecte ni l'injection explicite des plans du jalon I, ni PAK/cache, RNG/gameplay, UI/animation ou sauvegarde.

**START → INTRO → Entrance → EXIT sur le vrai CYD : PASS.** Intro FNV `ade0195d`, intro dispose `+34056B`, source BSP CRC `623f34e4`, arène `14095B/c3882516`, FIRST_FRAME `71ca7465` (8 murs, 4430 pixels). Cache SMALL-COLD/SMALL-WARM/LARGE-LEARN/LARGE-WARM/FINAL-READY terminé, `TRANSITIONLOAD RELEASE progress=100`, `ENGINESESSION FRESH-VISIBLE` puis `RESIDENTGAMEPLAY READY`. Déplacements 904→872→840→839→838→837→836→835, rotations 64→0→64→128→64→0→192, strafe 835→834, exposition de sommets/clipping ; caisse127 RNG first254 `BREAK_REMOVE` (pas de piège), deux Armor Shards armure 0→4→8, porte275 ouverte puis close HOLD/RELEASE après monster turn, dialogue89 opcode8 ouvert puis annulé `resume=no stateMutation=no`, HUB et EXIT. `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=0f2c9fc3`.

**LOAD V11 slot04 → Malwrath → EXIT sur le vrai CYD : PASS.** Slot 5604B, 30 monstres restaurés FNV `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation 13→14 et source map1 `c3882516`; première session visible et `RESIDENTGAMEPLAY READY`. Sélection shotgun via HUB weapons (weapon2→3), pickup Health Vial 33→34/34, porte113 quatre frames, sprite324 CROSS copies4 avec `draws=4→2→0` / `clip=0→2→4` selon proximité, Malwrath322 trois ripostes complètes à trois tirs projectile204, `MONSTERRETAL COMMIT` HP joueur 34→30→26→20 et armure23→19→15→9. Trois tirs shotgun ennemi `hp11→8→5→0`, mort visual `death4→corpse2`, `dropMaterialize=none` sur ce run (ne pas inventer de drop), bonus Bullet Clips +10, Flak Jacket armure9→23, secret portes112/111 +5XP. Porte113 fermeture différée HOLD/RELEASE/POST-MONSTER-COMPLETE. EXIT sans SAVE, `RESIDENTRESET empty=1`, MENU `0f2c9fc3`.

**Mémoire et invariants :** heap8 gameplay `113768`, largest8 `86004` stable ; menu `159704`, largest8 `110580` après les deux EXIT ; journal dialogue START `112624`, libéré au teardown. `shapeData=0` et `mediaTexels=0` au démarrage, session et cleanup ; zéro défaillance `WORLD_POST`, crash ou divergence RNG visible dans ces logs. Limite de preuve : ces itinéraires ne couvrent pas toutes les cartes/segments/effets visuels, et la seule equality FNV globale contrôlée ici est la première scène à pose d'origine.

**Observations indépendantes préexistantes toujours ouvertes :** `NATIVEFRAME BOUNDARY-INLINE` observé à certaines rotations sans FAIL ; `MALWRATHPROJECTILE FRAME pos=10737436xx` contre `NATIVESPRITE TRANSIENT pos=18xx` (différence `0x40000000` inexpliquée dans les journaux, pas une preuve de trajectoire fausse) ; `DAMAGECUE DONE delayed=yes` pour probe1 `onMs=321/309 gapMs=440`, probes2/3 `delayed=no`. Rien de ceci n'est corrigé, contourné ni dissimulé dans J. **Geler le code de J au SHA testé ; uniquement docs pour clôturer PR #229, merge utilisateur uniquement.**

---

## PR #229 / Jalon I — reset colonnes explicite + plans texturés : PASS matériel (24 octobre 2026)

**Firmware normal `esp32-cyd` testé sur le classic CYD : code `3fe4011cb4b9c2adb1a292f2cbfc718364b66681`**, branche `agent/esp32-native-point-transform` issue de main `d249bb5ab6002e93b498bf12459162b4f502f7c5`. CI corrective [push 40090695244](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40090695244) et [PR 40090699598](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40090699598) SUCCESS. `Render_initColumnScale` retiré de l'ABI ESP32 (20 Render ABI retirées, 3 primitives géométriques encore actives), `EspNativeRenderGeometry_resetColumns` borné, **plans floor/ceiling injectés explicitement** par `EspNativeWorldPlane_renderAtColumnReset` après reset et avant BSP. Ancien `--wrap=Render_initColumnScale` supprimé ; `Render_cullBoundingBox` wrapper préservé. Test différentiel 10k cas + largeurs 0..160 et garde de l'ordre de rendu.

**Incident du SHA précédent `d0e102a7ce8b1bb68688522bd3ebcbc549064d4c` : FAIL CYD réel** : `FIRST_FRAME eb943db2` au lieu de `71ca7465`, `TURNFRAME fail=WORLD_POST planes=no`, bloqué au chargement. Cause vérifiée : ancienne fonction traversait un wrapper qui peignait aussi les plans ; reset direct supprimait l'effet de bord. Correction ciblée ; jamais compter le SHA défaillant comme validé.

**START → INTRO → Entrance → EXIT, SHA corrigé :** intro premier FNV `ade0195d`, dispose +34056B, source `/intro.bsp` CRC `623f34e4`, runtime 14095B FNV `c3882516`, première frame `71ca7465`, SMALL-COLD/SMALL-WARM/LARGE-LEARN/READY et `RESIDENTGAMEPLAY READY`. Mouvements, rotations, strafe, caisse127 `first=131 TRANSFORM` en shard, 3 pickups d'armure 0→4→8→12, deux dialogues event88 avec reprise opcode19 puis 11, porte275 animée et HUB EXIT sans SAVE. `RESIDENTRESET released=18008 empty=1`, retour menu `0f2c9fc3`.

**LOAD V11 slot04 → Malwrath → EXIT :** 5604B, restauration des 30 monstres FNV `beaeaccc`, topo `50e16fe3`, positions `e2a9d90a`, activation13→14 ; porte113 et sprite324 multi-copies, Malwrath322 deux ripostes (3 projectiles204 chacune) puis shotgun `hp11→7→0`, coup critique, drop dynamique `Armor Shard` ramassé au tile285, fermeture différée de la porte, Flak Jacket puis secret +5XP (portes112/111) ; EXIT sans SAVE, même reset et menu. Gameplay heap8 `113768`/largest `86004` avant dialogue, `112624` avec journal ; menu `159704`/largest `110580` stable. `shapeData=mediaTexels=NULL`. Aucun crash ou échec d'invariant observable dans les extraits.

**Observations non corrigées dans I :** journal `MALWRATHPROJECTILE pos=10737436xx` vs renderer transient `18xx` (différence `0x40000000` inexpliquée, ne pas masquer), `DAMAGECUE` peut être retardé (`onMs 120/448 gap258` lorsque le rendu combat se poursuit), sans preuve de mutation de trajectoire/combat ; suivi isolé. Validité limitée aux parcours testés ; pas de garantie de pixel parity dans toutes les cartes. Code I **gelé après PASS** ; commits documentaires seuls avant prochaine frontière et pas de merge automatique.

---

## PR #229 / Jalon H — transformation 2D native : PASS CYD (24 octobre 2026)

**SHA de code physique testé : `54b4f8db43c297a2e9b6a6c6d1fd9992c62842d6`** en environnement normal `esp32-cyd`, branche `agent/esp32-native-point-transform` sur main mergé `d249bb5ab6002e93b498bf12459162b4f502f7c5`. CI [push 40088929517](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40088929517) et [PR 40088986033](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40088986033) SUCCESS. `Render_transform2DVerts(Render_t*,Vertex_t*)` devient desktop-only ; nouvelle `EspNativeRenderGeometry_transformPoint` avec six scalaires caméra et pointeurs x/y explicites, calcul entier et ordre legacy identiques, z non muté. BSP, culling et sprites consommant la nouvelle API. Host: 10 000 cas contre corps desktop, quatre orientations et coefficients fractionnaires. **19 ABI retirées, quatre primitives Render géométriques restantes** ; PAK, framebuffer, RNG, gameplay, animations, sauvegardes et allocations inchangés.

**START → INTRO → Entrance → EXIT, vrai CYD :** INTRO premier FNV `ade0195d`, dispose `+34056B`, source `/intro.bsp` CRC `623f34e4`, arena `14095B FNV c3882516`, première scène `71ca7465` avec 8 murs. Déplacements, rotations, strafe, caisse127 `first=254 BREAK_REMOVE`, dialogue event88 opcode8 puis reprise opcode19 `stateMutation=1`, deux Armor Shards `0→4→8`, porte275 ouverte en quatre frames puis fermée HOLD/RELEASE après tour. EXIT `RESIDENTRESET released=18008 empty=1`, menu `0f2c9fc3`, `heap8=159704 largest8=110580`; gameplay `113768/86004` puis journal dialogue `112624/86004`.

**LOAD V11 slot04 → combat → EXIT, vrai CYD :** checkpoint 5604B map1, 48 pickups consommés, 10 portes ouvertes, 30 monstres et états/topologie/positions exacts (`beaeaccc/50e16fe3/e2a9d90a`), 13→14 actifs, restaurés sans RNG. Porte113, sprite324 multi-copies 4, Malwrath sprite322 trois ripostes complètes à trois projectiles204 (attente/retaliation après animation), HP joueur `34→30→26→20`, armure `23→19→15→9`. Trois tirs de shotgun `hp monstre 11→8→5→0`, mort/corpse; **dropMaterialize=none** sur cet essai (ne pas prétendre un drop). Pickup santé, puis cartouches. EXIT sans SAVE, résident `empty=1`, retour menu `0f2c9fc3`, RAM gameplay `113768/86004` stable avant dialogue, puis `112624/86004`, menu `159704/110580`; `shapeData=mediaTexels=0`.

**Observations distinctes :** utilisateur ressent gameplay « redevenu plus nerveux » ; previews avance ~93–107 ms et rotations ~189–208 ms selon scène, **sans A/B contrôlé ni preuve de gain causal**. `MALWRATHPROJECTILE` affiche un x `10737436xx` là où `NATIVESPRITE TRANSIENT` affiche un x 16-bit normal, comme précédemment : audit de journal séparé ; pas d'attribution à H. `DAMAGECUE DONE delayed=yes` sur deux effets et `NATIVEFRAME BOUNDARY-INLINE` hérités. Aucun piège de caisse testé ; ne pas modifier sa probabilité legacy 2/256. Aucun crash, dérive de heap ou divergence RNG visible sur ces parcours ; pas de preuve toutes cartes/endurance/pixel parity. **Code H gelé** au SHA ci-dessus ; commits post-test docs-only avant nouveau jalon, sans merge automatique.

---

## PR #228 / Milestone G — PASS CYD (24 octobre 2026)

Code hardware testé : `0484c7c2a4cd3dcf120dc4daec1f4a4ec460c43a` (`esp32-cyd`). START, INTRO, Entrance, dialogue opcode19, portes et pickups, LOAD V11 slot04, Malwrath trois tirs 204, drop, EXIT x2 : PASS. FNV introduction `ade0195d`, première frame `71ca7465`, menu `0f2c9fc3`, arena `c3882516`, reset `empty=1`, heap8 menu `159704`, gameplay `113768`; les deux CI push/PR sont SUCCESS. `Render_projectVertex` est desktop-only ; 18 ABI Render retirées, 5 primitives restantes. Caisse piégée : origine et port utilisent `first<2` sur octet RNG, 2/256 de chance ; tirages observés 254 (vide), 102 (transformée). Aucun piège effectivement testé. Entrée rejetée pendant combat (`COMBAT-INPUT-BLOCK`) et impression de latence : suivi séparé. Code gelé, docs-only après le test.

---

## PR #228 / Milestone F — occlusion colonnes projetées native, sans `Render_t` : PASS matériel ciblé (24 octobre 2026)

**Code firmware testé en environnement normal `esp32-cyd` sur le vrai classic CYD sans PSRAM : `bd34c15e2e9c290deaebe5219ac27660b36ac512`.** Branche `agent/esp32-native-column-occlusion`, [PR #228](https://github.com/Kharn27/DoomRPG-RE/pull/228) issue du vrai `main=c2e3198d1574e411d99066c80258b5fc3e0ea709` après merge #227. CI de ce code [push #40082658226](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40082658226) et [PR #40082706235](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40082706235) **SUCCESS** : différentiel hôte, gardes d'ABI, firmwares normal/bringup/trace compilés. Le test physique ci-dessous provient **exclusivement des deux logs Serial utilisateur** ; aucun build ou flash local n'est prétendu.

**Frontière désengagée / contrat permanent :** l'ancien `Render_occludeClippedLine(Render_t*, Line_t*)` devient desktop-only dans `src/Render.h` ; son corps `src/Render.c` reste l'oracle original inchangé. Les deux consommateurs BSP natifs (`esp_native_first_frame.c`, `esp_native_bsp_visibility.c`) appellent `EspNativeRenderGeometry_occludeProjectedSpan(int* columnScale, int x1, int x2)`. Seul le tableau de colonnes fourni par l'appelant est muté ; bornes projetées 16.16, arrondi `(x+0xffff)>>16` et segment semi-ouvert conservés, aucune allocation ou nouvelle carte/buffer. `esp_render_geometry_primitives.c` héberge l'opération permanente ; `render_geometry_host.py/.c` vérifie la parité hôte contre la vraie fonction desktop (10 000 cas, quatre directions, segments projetés fractionnaires/bornés) et `native_render_raster_abi_retirement_host.py` surveille maintenant **17 ABI Render retirées**, laissant **six** primitives `Render_*` actives (clipping, projection, transform, culling, initialisation). **Aucun changement à `Render_t`, framebuffer, raster PAK, événements, gameplay, RNG, caches ou animations.**

**Serial 1 — START → INTRO → Entrance → interactions → EXIT :** menu initial armé `fec57f30`, nouvelle partie `PlayerState=52 B/FNV e745fce9`, INTRO initiale `ade0195d`, disposal `recovered=34056`, fichier `/intro.bsp` `21823 B/CRC 623f34e4`, arène `14095 B/FNV c3882516`, première frame du monde `71ca7465`, `FRESH-VISIBLE`, `shapeData=mediaTexels=NULL`. `heap8=113768/largest8=86004` au début de partie ; déplacements et animations, deux Armor Shards `0→4→8`, porte 275 `DOORANIM COMPLETE frames=4`. Dialogue event88/opcode8 ouvert puis **annulé**, `resume=no stateMutation=no turnAdvance=no` (pas de continuation opcode19 testée ici). Après allocation de journal dialogue `1128 B`, `heap8=112624` observé, puis libération `DIALOGCHAIN OWNER-RELEASE recovered=1144 activeAtTeardown=0`; `RESIDENTRESET released=18008 empty=1`. Retour menu `0f2c9fc3`, `heap8=159704/largest8=110580` stable sur deux `ALIVE` ; `saveWrite=no`.

**Serial 2 — LOAD V11 slot07 → porte → deux tirs/attaque ennemie → EXIT :** sauvegarde V11 `5604 B` carte1, restauration `81` ressources consommées, `20` portes ouvertes/`1` verrouillée, `4` caisses transformées, `30` monstres état `8acca67c`, topologie `ebca87dd`, positions `24bbd504`, activation `6698ed80` (22 actifs), sans replay legacy et `MONSTERSTATE RESTORE rngCalls=0`. `RESUME-VISIBLE` et `heap8=113768/largest8=86004` stables sur plusieurs `ALIVE`. Porte ligne160 `DOORANIM COMPLETE frames=4`, activation sprites6/7 `22→24`. Riposte sprite7 après `MONSTERATKVIS COMPLETE` : joueur HP `30→27`, armure `10→8`, probe en rollback exact puis RNG commis. Tir pistolet 1 : sprite7 HP `6→3`/armure `4→2`/munitions `38→37`; tir 2 : sprite6 HP `8→2`/armure `2→0`/munitions `37→36`, `MONSTERCOMBAT COMMIT rollback=closed`, HITFX `gameplayRng=untouched`. Attaque ultérieure du sprite6 **manquée** : `MONSTERRETAL MISS-COMMIT`, HP/armure joueur inchangés, RNG de gameplay engagé. Second EXIT `RESIDENTRESET released=18008 empty=1`, menu `0f2c9fc3`, `heap8=159704/largest8=110580`, pas de SAVE écrit.

**Performance perçue / point à auditer séparément :** l'utilisateur signale un gameplay « un peu moins nerveux ». Les `VIEWANIM` transmis mesurent ~102–132 ms par preview d'avance, ~201–218 ms pour deux frames de strafe, ~220–225 ms pour deux previews de rotation ; les animations d'attaque monstre conservent `phaseMs=500`. Un `COMBAT-INPUT-BLOCK tap=5 queued=no` est visible pendant un tour monstre. Ces mesures ne sont **pas** un A/B contrôlé avant/après ; aucune régression causale de cette extraction pure d'occlusion n'est établie. Ne pas changer la cadence ou le contrat d'input dans la clôture docs-only de ce jalon : audit distinct si poursuivi.

**Verdict : PASS matériel ciblé de l'occlusion native et non-régression fonctionnelle observée** sur ces parcours. Aucun panic, reset, divergence RNG ou dérive heap visible dans les fenêtres fournies. `NATIVEFRAME BOUNDARY-INLINE` reste le fallback BSS antérieur. Pas de nouveau SAVE, autres cartes, longue endurance, parité pixel exhaustive, ni benchmark comparatif contrôlé. Code `bd34c15e2e9c290deaebe5219ac27660b36ac512` **gelé**, modifications ultérieures exclusivement documentaires et CI post-doc à vérifier avant merge-ready ; ne pas merger `main` automatiquement.

---

## PR #227 / Milestone E — 11 ABI desktop Render span-mode masquées sur ESP32 : PASS matériel CYD (24 octobre 2026)

**Firmware normal `esp32-cyd` réellement vérifié sur classic CYD sans PSRAM, SHA de code exact : `40e2d2c2a74a0362617d15daf2d49526e9f54cf2`.** Même branche `agent/esp32-retire-solid-bg-render-abi`, même [PR #227](https://github.com/Kharn27/DoomRPG-RE/pull/227), base main `89b7fa27732cababdbe83f8d89ba1ccf649bae4a`. Ce SHA succède au **PASS matériel du Milestone D** sur `1c480682a3a2fb5e31e87c1b83bcfb7b12ff8c6a`, déjà documenté ci-dessous, sans repartir de main. CI du nouveau code : [push #40022218390](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40022218390) et [PR #40022225349](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40022225349) **SUCCESS**, tests host d'ABI/différentiel et trois firmwares `esp32-cyd`, bringup, trace compilés. Les deux essais ci-dessous proviennent des **logs Serial du vrai CYD transmis par l'utilisateur**, non d'un test local supposé.

**Retrait exact / contrat permanent :** `Render_getSpanMode` et les dix `Render_SpanMode0..9` ne sont plus des prototypes visibles sous `DOOMRPG_ESP32` dans `src/Render.h` (garde `#ifndef DOOMRPG_ESP32`). Leurs implémentations originales **restent intactes dans `src/Render.c` desktop** pour référence/oracle ; aucun nouveau reject/provider natif, aucun changement aux calculs ni au propriétaire du rendu PAK. `ESP32/test/native_render_raster_abi_retirement_host.py` protège désormais **16 ABI retirées** : les 5 anciennes raster/background et ces 11 span-mode (desktop oracle conservé, absence de référence dans tous les `ESP32/src` et `ESP32/include`). `render_geometry_host.py/.c` garde les **7 primitives de géométrie réellement actives**, différentiel 10 000 cas seedés/quatre orientations. `Render_t`, géométrie BSP, framebuffer, plans natifs, caches, gameplay, RNG et allocations sont inchangés. **Ce jalon n'est PAS encore la suppression des 7 API de géométrie Render_t actives.**

**Scénario CYD 1 — MENU_MAIN → START → INTRO → Entrance → porte/dialogues → EXIT :**
- Menu première sélection `fec57f30`, heap8 `154860/largest 110580` ; nettoyage main `+8440 B` ; joueur fresh `PlayerState` FNV `e745fce9`, 52 B, `legacyPlayer=NULL`. Premier frame INTRO `ade0195d`, heap8 `129244/largest 98292` sans allocation par frame. Taps REVEAL/MORE/CONTINUE/SKIP-ANIM/FINAL-CONTINUE, `INTRODISP recovered=34056` et framebuffer préservé pendant dispose ; `shapeData=mediaTexels=NULL`. Les FNV de frames intermédiaires et la durée de l'intro varient selon l'input/clock, **pas des invariants du test**.
- `/intro.bsp` 21823 B CRC `623f34e4`, 223 nodes/480 lines/344 sprites/93 events, arena `14095 B` FNV `c3882516`, première frame monde `71ca7465`, `FRESH-VISIBLE`, `RAMBUDGET advisory=HEADROOM_OK`. Gameplay `heap8=113768/largest8=86004` stable aux `ALIVE` 17, 23, 28 s. Déplacements 904→872→840→839→838, **deux** Armor Shards (`0→4→8`) ; porte 275 ouverte par SELECT, puis fermeture mouvement `HOLD` (collision immédiatement fermée), `RELEASE after-monster-turn`, `POST-MONSTER-COMPLETE`, pas de corruption de rendu visible dans le log.
- `DIALOG` event82/opcode8 string52 : ouverture, pages, fermeture et `DIALOGCHAIN RESUME` offset1/opcode19 `stateMutation=1`. Nouvelle sélection event82 string53/offset3 puis MENU `DIALOG-CANCEL resume=no stateMutation=no turnAdvance=no`. HUB SYSTEM puis EXIT à double confirmation, `saveWrite=no`. Journal dialogue `activeAtTeardown=1` mais `OWNER-RELEASE recovered=1144` et `owner=none` ; `RESIDENTRESET released=18008 empty=1`. Menu `0f2c9fc3`, heap8 `159704/largest8=110580` stable aux `ALIVE` 38 et 43 s.

**Scénario CYD 2 — LOAD V11 → porte/2 tirs/2 ripostes → EXIT :**
- Slot07, `5604 B`, version11, carte1 position `1440,608` angle64, source CRC `623f34e4`, arène `c3882516`. Restaurations exactes rapportées : `81` ressources consommées, lines `open20/locked1`, `4` caisses transformées, `30` monstres FNV `8acca67c`, topologie `ebca87dd`, positions `24bbd504`, activation `6698ed80` avec **22** actifs, drops serial5/next5. `legacyReplay=0/0/0`, restore monster RNG `rngCalls=0`; `RESUME-VISIBLE`, `shapeData=mediaTexels=NULL`. Gameplay `heap8=113768/largest8=86004` stable aux `ALIVE` 79, 85, 90, 95 et 100 s, avec `HEADROOM_OK`.
- Arme `1→2` sans tour ; porte ligne160 ouverte avec 4 frames. Déplacements/visibilité déclenchent les sprites6/7, activation **22→24**, séquence ordonnée active, `MONSTERMOVERNG COMMIT rngCalls=1` exactement quand le déplacement est commis. Les inputs durant les turns/animations sont bloqués (`COMBAT-INPUT-BLOCK`, pas de queue différée).
- **Monstre sprite7** : `MONSTERTURN MEMBER-ATTACK-PROBE` ne mute ni player ni RNG (`rngRollback=yes playerExact=yes`), `MONSTERATKVIS COMPLETE` avant `MONSTERRETAL COMMIT` ; joueur `HP 30→27`, armure `10→8`, RNG engagé `5705f48f→6681375c`, feedback LEFT, flash rouge.
- **Tir 1** pistolet → sprite7 : `MONSTERCOMBAT COMMIT` `HP 6→3`, armure `4→2`, munitions `38→37`, `rollback=closed`, `HITFX gameplayRng=untouched`, `monsterFNV 8acca67c→ee10e72b`.
- **Tir 2** pistolet → sprite6 : `MONSTERCOMBAT COMMIT` `HP 8→1`, armure `2→0`, munitions `37→36`, `rollback=closed`, `monsterFNV ee10e72b→54f20ae8`. Après mouvement déjà commis, `MONSTERPOSTMOVE ATTACK-PROBE` est en rollback exact ; animation puis `MONSTERRETAL COMMIT` : joueur `HP 27→25`, armure `8→6`, `rng a1aa5180→0415c46e`, feedback FRONT / flash rouge, `sameTurn=yes`.
- EXIT double confirmation sans sauvegarde, `RESIDENTRESET released=18008 empty=1`, MENU FNV `0f2c9fc3`, heap8 `159704/largest8=110580` stable aux `ALIVE` 105 et 110 s.

**Verdict Milestone E : PASS matériel ciblé sur SHA `40e2d2c2`** ; aucune erreur, panic/reboot, divergence RNG ou fuite heap8 visible dans les deux fenêtres. `NATIVEFRAME BOUNDARY-INLINE` est un fallback BSS déjà connu, pas une nouveauté. Écarts de frames de cache et d'animations intermédiaires selon la session ne signifient pas divergence. **Non testé :** nouveau SAVE, progression sur une autre carte, teardown fatal, endurance longue, parité pixel exhaustive. **Gel du code validé ; commits suivants exclusivement documentaires ; vérifier CI post-docs et passage en Ready for review avant merge par l'utilisateur.**

---

## PR #227 / Milestone D — Retrait de Render_renderFloorAndCeilingSolidBG : PASS matériel classic CYD (23 octobre 2026)

**SHA de code exact vérifié sur la branche et flashé pour les deux parcours Serial transmis : `1c480682a3a2fb5e31e87c1b83bcfb7b12ff8c6a`** (normal `esp32-cyd`), [PR #227](https://github.com/Kharn27/DoomRPG-RE/pull/227) sur `agent/esp32-retire-solid-bg-render-abi`, issue du **main mergé `89b7fa27732cababdbe83f8d89ba1ccf649bae4a`**. [CI push #40020264072](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40020264072) **SUCCESS**, [CI PR #40020315456](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40020315456) **SUCCESS** ; tests host de géométrie différentielle et d'ABI raster verts et trois firmwares (normal, bringup, trace) compilés. **Aucun build local ou test autre que les logs Serial fournis n'est revendiqué.**

**Frontière retirée :** `Render_renderFloorAndCeilingSolidBG(Render_t*)`, fonction de remplissage framebuffer par bandes qui restait présente mais **non appelée** dans le propriétaire ESP32 `ESP32/src/esp_render_geometry_primitives.c`, n'est plus définie sur CYD ni exposée via `src/Render.h` sous `DOOMRPG_ESP32`. La déclaration reste desktop-only et `src/Render.c` garde son corps original. Le test `ESP32/test/native_render_raster_abi_retirement_host.py` surveille désormais cette cinquième ABI raster/background retirée, toutes les sources/headers ESP32 et le desktop oracle ; `render_geometry_host.py/.c` continuent de comparer **les sept fonctions de géométrie réellement actives** au desktop sur 10 000 cas seedés et quatre orientations. Les calculs de clipping/culling/projection/occlusion et leurs appels restent inchangés, tout comme `Render_t`, le pipeline plans PAK, BSP, caches, RNG, UI, gameplay et framebuffer. **Ce jalon n'est pas encore une migration des sept ABI géométriques actives hors de `Render_t`.**

**Parcours matériel START → INTRO → Entrance → dialogue → EXIT :**
- Menu initial sélection armée `fec57f30` ; START heap8 `154860`, largest8 `110580`, cleanup mappings récupère `8440 B`, PlayerState fresh FNV `e745fce9`. Intro initiale `ade0195d`, final Continue, `INTRODISP recovered=34056`, FNV framebuffer préservée durant dispose ; `shapeData=mediaTexels=NULL`.
- BSP `/intro.bsp` `21823 B` CRC `623f34e4`, arène native `14095 B` FNV `c3882516`, 223 nodes / 480 lines / 344 sprites / 93 events, première frame monde `71ca7465`, `FRESH-VISIBLE`, `RAMBUDGET HEADROOM_OK`. Heap8 gameplay `113768` / largest8 `86004`, stable aux `ALIVE` 24 et 29 secondes environ ; déplacements 904→872→840→839 et Armor Shard armure `0→4`.
- Dialogue `SELECT event82/opcode8` depuis tile807 : ouverture, 2 pages, fermeture, `DIALOGCHAIN RESUME event82 opcode19 stateMutation=1` ; état et rendu natifs. HUB System EXIT double confirmation, `saveWrite=no`. Le journal `DIALOGCHAIN` `1128 B` peut rester actif au teardown (`activeAtTeardown=1`) : `recovered=1144` et owner libéré, pas une preuve de fuite ou de rollback fatal exact. `RESIDENTRESET released=18008 empty=1`, MENU `0f2c9fc3`, heap8 `159704` / largest8 `110580` à `ALIVE` 44 s.

**Parcours matériel LOAD V11 → porte → déplacement/combat → EXIT :**
- Slot07 `5604 B` V11 : restauration map1 pos `1440,608` angle64 ; arène toujours `c3882516`, `81` consommés, portes `open20/locked1`, `4` caisses transformées, `30` monstres `8acca67c`, topologie `ebca87dd`, positions `24bbd504`, activation `6698ed80` avec `22` actifs, drops `serial5 next5`, zéro replay legacy/RNG ; `RESUME-VISIBLE` et `shapeData/mediaTexels=NULL`.
- Heap8 gameplay `113768`, largest8 `86004` stable aux `ALIVE` 92, 98, 103, 108 et 113 s. Porte ligne160 `DOOR_OK`, animation 4 frames ; activations 22→24 (sprites 6/7), ordre des turns préservé et mouvement RNG `COMMIT rngCalls=1`. Riposte du monstre sprite7 seulement après `MONSTERATKVIS COMPLETE` : joueur HP `30→27`, armure `10→8`, feedback rouge/directionnel. Attaque pistolet `weapon=2`, cible sprite7 : mob HP `6→3`, armure `4→2`, balles `38→37`, `MONSTERCOMBAT COMMIT rollback=closed` ; HITFX `gameplayRng=untouched`.
- Second EXIT depuis le LOAD, toujours `saveWrite=no` ; `RESIDENTRESET released=18008 empty=1`, MENU FNV `0f2c9fc3`, heap8 `159704` / largest8 `110580` stables aux `ALIVE` 118 et 123 s.

**Verdict : PASS matériel ciblé des deux parcours sur le SHA de code `1c480682`** ; aucune erreur, panic, reboot, dérive de heap8 visible, divergence RNG ni retour de géométrie desktop observés. `NATIVEFRAME BOUNDARY-INLINE` reste le fallback BSS préexistant ; les FNV de frames chaudes et la cadence tactile de fin d'intro ne sont pas des invariants entre essais. **Ne couvre pas** un nouvel enregistrement SAVE, autres cartes, reboot prolongé, fatal teardown ou une preuve exhaustive de parité pixel par pixel. **Code hardware gelé : clôture docs-only, vérifier CI docs-only avant merge-ready ; merge laissé à l'utilisateur.**

---

## PR #226 / Milestone C — Retrait final des 4 ABI raster Render et de leur TU : PASS matériel CYD (23 octobre 2026)

**SHA exact du firmware normal `esp32-cyd` testé sur vrai ESP32-2432S028R : `71244d1ca94a56c1eb01cecf75c192de95a9c9dc`**, [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226) sur `agent/esp32-retire-legacy-plane-test-abi`, base de la PR `main=a7bfffca4f13abd4cbdf5844615ab5559930fb30`. Ce SHA **succède** aux deux PASS de la même PR : `ba758095` (3 plane `*_Test`) et `10ff0b4b` (7 BSP/map/event ABI). CI du code C : [push #39844070050 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39844070050), [PR #39844080187 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39844080187), les trois tests host de retrait ABI PASS, trois firmwares compilés. Budgets flash/RAM statiques inchangés : **normal 825 497/49 536 B**, bringup **846 301/49 688 B**, trace **842 265/49 584 B**. Ne pas confondre avec le heap8 runtime.

**Frontière effectivement retirée** : `Render_render`, `Render_renderFloorAndCeilingBG`, `Render_drawplane`, `Render_spanPlane` n'ont plus de déclaration ESP32 ni de provider/reject. L'unité source **`ESP32/src/esp_legacy_render_reject.c` est supprimée** (réduction réelle du nombre de TUs). Leurs prototypes dans `src/Render.h` sont protégés par `#ifndef DOOMRPG_ESP32`, les corps d'origine dans `src/Render.c` restent l'oracle desktop non compilé sur le CYD. Le script `ESP32/scripts/build_engine.py` interdit désormais le retour de l'unité supprimée ; `ESP32/test/native_render_raster_abi_retirement_host.py` contrôle déclarations desktop-only, absence d'appels/références natives et maintien des vraies primitives géométriques `esp_render_geometry_primitives.c` et des plans PAK `esp_native_plane_renderer.c`. Les deux anciens tests ABI ont été adaptés. **Aucune modification du BSP/PAK propriétaire, des caches, du moteur natif, du gameplay ni du framebuffer.**

**Témoins BOOT→START→INTRO→Entrance→EXIT sur le vrai CYD** :
- Boot ESP32-D0WD-V3 240 MHz, flash **4 MB**, **PSRAM 0**, SD 240 MiB, PAK **241 entrées** ; logical framebuffer **160×120 RGB565/38 400 B**, physique 320×240 ×2 ; CORE **Render=1532, Game=0, Canvas=44**, `DOOMCANVASBRIDGE exports=0`, mappings `8392` dont payload `8376`. MENU_MAIN FNV **`0f2c9fc3`**, heap8 **154860 / largest 110580**, START arm `fec57f30`.
- Nouvelle partie PlayerState `e745fce9`, première intro FNV **`ade0195d`**, interactions finales correctes, `INTRODISP recovered=34056` et FNV fin `b93d13d3→b93d13d3` (la valeur de fin varie avec la cadence des taps, ce n'est **pas** un invariant). Intro libérée avant la carte ; `shapeData=mediaTexels=NULL`.
- `/intro.bsp` **21823 B** CRC **`623f34e4`**, MAPRT **14095 B / FNV `c3882516`**, 223 nodes/480 lines/344 sprites/93 events, frame monde initiale **`71ca7465`**, `FRESH-VISIBLE` et `RAMBUDGET HEADROOM_OK`. Heap8 gameplay **113768 / largest 86004** stable aux `ALIVE` 72, 77, 82 s.
- Deux Armor Shards tiles 839 puis 838, armure **0→4→8**, Player FNV `e745fce9→548397a5→363261d1`; porte **275** SELECT `DOOR_OK`, animation **4 frames**, déplacement et fermeture `HOLD→RELEASE→POST-MONSTER-COMPLETE`. Rotations et strafe/marche natifs ; `NATIVEFRAME BOUNDARY-INLINE` est le même fallback BSS connu des jalons antérieurs.
- Déplacement vers tile 738 et dialogue **opcode26/event79/cmd0** : `DIALOG OPEN back=0`, pages, `CLOSE mode=resume`, puis `DIALOGCHAIN RESUME` **opcode19** `stateMutation=1`, `rollbackLease=closed`, et pickup `Got Sm Medkit` (`totalConsumed=3`). HUB System EXIT sans SAVE (`saveWrite=no`). `DIALOGCHAIN OWNER-RELEASE journal=1128 activeAtTeardown=1 recovered=1144` : journal de continuation encore marqué actif au reset, mais bien libéré ; ce log à lui seul **ne démontre ni fuite ni rollback exact lors d'un fatal teardown**. `RESIDENTRESET heap8=141696→159704 released=18008 empty=1` ; MENU FNV **`0f2c9fc3`**, heap8 **159704/largest 110580** stable aux `ALIVE` 97 et 102 s.

**Témoins second parcours : vrai LOAD V11, combat, puis second EXIT depuis la session chargée** :
- Slot07 sauvegarde spatiale **5604 B**, `NATIVESAVE LOAD version=11` map1 pos **1440,608 angle64**, CRC/arène inchangés, **81 consommés**, lines `open20/locked1`, **4 crates transformées**, script et automap restaurés, **30 monstres** `monsterFNV=8acca67c`, topology `ebca87dd`, positions `24bbd504`, activation `6698ed80` (activeCount **22**), drops `serial5 next5` ; reprise `RESUME-VISIBLE`, `shapeData=mediaTexels=NULL`, aucun replay legacy/RNG.
- Heap8 **113768/largest 86004** après reprise, puis `ALIVE` 143, 148, 153, 158, 163, 168, 173, 178, 183 s : stable y compris pendant mouvement, combat et HUB. Strafe/marche, porte **160** `DOOR_OK` animée 4 frames ; activation sprites 6/7 **22→24**, tours ordonnés, `MONSTERMOVERNG COMMIT rngCalls=1` ; `COMBAT-INPUT-BLOCK` pendant l'action monster-turn sans action indue.
- Weapon switch `1→2→3→2` sans tour, pistol `weapon=2`, attaque sur sprite7 : `hitLoops=1 totalDamage=3 armorDamage=2`, mob HP **6→3**, armor **4→2**, ammo **38→37**, `MONSTERCOMBAT COMMIT rollback=closed`, HITFX sang en overlay RNG visuel local, aucun RNG gameplay du HITFX. Riposte du monstre sprite7 après animation `MONSTERATKVIS COMPLETE` puis `MONSTERRETAL COMMIT` : joueur HP **30→27**, armure **10→8**, RNG du probe restauré avant engagement, feedback rouge/directionnel.
- Deuxième HUB EXIT `saveWrite=no` : `RESIDENTRESET released=18008 empty=1`, FNV MENU **`0f2c9fc3`**, heap8 **159704/largest 110580** encore à `ALIVE` 188 s. **Le second EXIT après LOAD était hors périmètre du précédent jalon B et est maintenant matériellement observé PASS.**

**Verdict** : **PASS matériel ciblé pour le SHA `71244d1c`**, aucun panic/reboot, FAIL, divergence RNG ou retour du rendu desktop observé. Les variations FNV de frames chaudes ou le `NATIVEFRAME BOUNDARY-INLINE` préexistant ne sont pas une régression prouvée. Cette preuve ne valide pas l'écriture d'un nouveau SAVE ni les autres cartes ou fatal teardown. **Geler le code testé, enchaîner seulement des commits docs-only puis CI ; l'utilisateur effectuera le merge.**

---

## PR #226 — BSP/map ABI retired: CYD hardware PASS (22 octobre 2026)

Firmware normal `esp32-cyd` testé, code SHA `10ff0b4b7b75095f7e12fa1d0fc0bdf15c18d4df`, branche `agent/esp32-retire-legacy-plane-test-abi`. [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226). [CI push SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39688241930) et [CI PR SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39688246727). Builds identiques au précédent code : normal RAM 49 536 B / flash 825 497 B, bringup 49 688 / 846 301 B, trace 49 584 / 842 265 B.

Sept anciennes entrées ESP32 supprimées : `Render_relinkSprite`, `Render_renderBSPNoclip`, `Render_findEventIndex`, `Render_beginLoadMap`, `Render_beginLoadMapData`, `Render_renderBSP`, `Render_walkNode`. Prototypes `Render.h` desktop-only, vrais corps desktop conservés ; CI `native_render_bsp_abi_retirement_host.py` PASS. Quatre stubs raster non concernés restent inchangés.

**Serial START/EXIT** : PSRAM 0, framebuffer 38400 B, PAK 241, MENU_MAIN `0f2c9fc3`, intro `ade0195d`, `INTRODISP recovered=34056`, /intro.bsp CRC `623f34e4`, arena 14095/FNV `c3882516`, FIRST_FRAME `71ca7465`. Deux Armor Shards armure 0→4→8, porte 275 ouverte et fermée en 4 frames, dialogue opcode8/event89 annulé sans mutation. EXIT `saveWrite=no`, `DIALOGCHAIN activeAtTeardown=0`, `RESIDENTRESET released=18008 empty=1`, retour MENU_MAIN `0f2c9fc3`, heap8 après EXIT `159704` largest `110580` stable.

**Serial LOAD/combat** : sauvegarde V11 slot 07, 5604 B, restauration map, overlays et états 30 monstres ; activations 22→24, porte 160, mouvement, deux ripostes après animation HP `30→27→24`, armure `10→8→6`. Extincteur `weapon=1` sans touche `No effect!`, ammo `12→11`. Heap8 `113768`, largest `86004` stable durant LOAD et combat jusqu'à 100s. `shapeData/mediaTexels=NULL`, `NATIVEFRAME BOUNDARY-INLINE` préexistant ; aucun crash ni divergence RNG visible.

**Périmètre** : PASS sur START/EXIT et LOAD/combat, mais second EXIT après LOAD, nouvelle SAVE, autres maps et fatal teardown non testés. SHA de code gelé ; seuls documents modifiés après PASS. Aucun merge dans main.

---

## PR #226 — Retrait des trois entrées Render planes *_Test : PASS matériel CYD (22 octobre 2026)

**Code exactement présenté au test matériel, firmware normal `esp32-cyd` : `ba758095e82e594c232c410f737e005ccae518fc`**, branche `agent/esp32-retire-legacy-plane-test-abi`, [PR #226](https://github.com/Kharn27/DoomRPG-RE/pull/226), créée depuis le **main mergé** `a7bfffca4f13abd4cbdf5844615ab5559930fb30` (merge PR #225). [CI push #39686642437 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39686642437) et [CI PR #39686742624 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39686742624), trois builds/contrôles d'exports verts. Budgets statiques inchangés : **normal RAM 49 536 B / flash 825 497 B**, **bringup 49 688 / 846 301 B**, **trace 49 584 / 842 265 B**. Ces nombres ne sont pas les mesures du heap pendant le jeu.

**Frontière retirée sans remplacer le rendu** : `Render_renderFloorAndCeilingBG_Test`, `Render_drawPlane_Test`, `Render_spanPlane_Test`, auparavant de simples stubs de refus côté production `ESP32/src/esp_legacy_render_reject.c`, n'ont désormais **ni provider ni déclaration ESP32**. Leurs prototypes dans `src/Render.h` sont sous `#ifndef DOOMRPG_ESP32` ; leurs vrais corps dans `src/Render.c` restent l'oracle desktop non compilé sur CYD. L'ancien pipeline `mediaTexels` monolithique est interdit ; le rendu natif des plans BSP, son PAK, les géométries/événements, le framebuffer partagé, le RNG, les inputs et le gameplay n'ont pas été modifiés. Nouveau test `ESP32/test/native_render_plane_test_retirement_host.py`, intégré à la CI : absence de ces trois noms dans tous les sources/headers natifs (commentaires et chaînes ignorés), préservation des corps desktop et prototypes conditionnels. **Ce test host a PASS** ; la validation du comportement provient des logs Serial ci-dessous.

**Témoins matériel transmis pour ce jalon** :
- **BOOT/PAK** : vrai classic CYD ESP32-D0WD-V3, 4 MB flash, **PSRAM=0**, écran physique 320×240, logique **160×120 RGB565 / 38 400 B** partagé, SD **240 MiB**, PAK **241 entrées**. `Render=1532 / Game=0 / Canvas=44`, `DOOMCANVASBRIDGE exports=0`. Palette **3280 entrées / 6560 B**, `RENDERSTART result=1 used=6576 heap8=163300`. Mappings directs `mappings.bin=8392`, payload **8376 B**, `EspLegacyMappings_load result=1`, `heap8=154860 largest8=110580`, scratch=framebuffer.
- **MENU/INTRO** : MENU_MAIN `frame=0f2c9fc3`, carte START armée `fec57f30`, `MAINMENU Runtime cleanup gained=8440` et champs `shapeData/mediaTexels/nodes/lines/mapSprites=NULL`. Player fresh `FNV=e745fce9`. Première frame intro `ade0195d`; séquence tactile jusqu'à `FINAL-CONTINUE` ; `INTRODISP recovered=34056`, FNV du framebuffer conservé `c0700aa8->c0700aa8`.
- **Entrance** : `/intro.bsp` **21823 B / CRC `623f34e4`**, `MAPRT arenaBytes=14095 arenaFNV=c3882516`, première frame `71ca7465`, `MAPFLASH REUSE HIT`, `FRESH-VISIBLE/READY`. `RAMBUDGET HEADROOM_OK`, `shapeData=0 mediaTexels=0`. Navigation, rotations `64→128→64→0→64`, trajet `904→872→840→839→838→837→836` et après la porte jusqu'à `738`. Deux Armor Shards ramassés `armor 0→4→8`, porte **ligne 275**, `DOOR_OK`, animation d'ouverture **4 frames** puis fermeture différée `HOLD→RELEASE→POST-MONSTER-COMPLETE` sans échec visible.
- **Dialogue natif** : déplacement `770→738`, `MOVEEVENT WORLD-READY opcode26/event79`, `DIALOG OPEN` (113 octets / 7 lignes / no-back) ; touch `FASTFORWARD→PAGE→FASTFORWARD→CLOSE`, `packClosed=yes`, reprise `DIALOGCHAIN RESUME state=1 mutation=1` puis ramassage Sm Medkit. **Cela augmente le périmètre observé au-delà du parcours minimum**, sans constituer un test des autres familles de scripts.
- **Mémoire/EXIT** : relevés `ALIVE` gameplay **heap8=113768 largest8=86004** à 13, 18 et 23 s (dialogue ensuite : `112624` durant l'owner lazy). HUB Inventory → System → EXIT double confirmation, `saveWrite=no`; `DIALOGCHAIN OWNER-RELEASE journal=1128 ... activeAtTeardown=1 ... recovered=1144 owner=none` ; `RESIDENTRESET heap8=141696->159704 released=18008 empty=1`, puis `MAINMENU Runtime cleanup gained=0`. `SYSEXIT MENU-READY frame=0f2c9fc3 session=off resident=empty`; `ALIVE heap8=159704 largest8=110580` identique à 33, 38, 43, 48 s.

**Interprétation stricte** : `activeAtTeardown=1` est le marqueur du journal de rollback encore actif au moment de `EspNativeGameplayEventChain_reset()`, **pas** celui d'une fenêtre dialog toujours ouverte ; la destruction de session l'abandonne et libère l'owner, conformément au code. Pas de preuve de fuite dans les fenêtres fournies, mais pas de certification des branches de rollback après échec de présentation. `NATIVEFRAME BOUNDARY-INLINE` est un fallback préexistant ; RNG/FNV des frames de cache non canoniques ne sont pas des invariants cross-boot. **PASS matériel ciblé BOOT→START→INTRO→Entrance→pickups/porte/dialogue→EXIT**, pas de LOAD V11, SAVE écrit, combat, reboot, plusieurs sessions, ni fatal `DoomRPG_FreeAppData`. **Le SHA de code `ba758095` est gelé ; clôture uniquement docs-only après PASS, pas de merge automatique.**

---

## PR #225 — Direct PAK mappings : PASS matériel CYD (22 octobre 2026)

**SHA de code exactement flashé et validé sur le classic CYD, environnement normal `esp32-cyd` : `d284329b937204452ceb9d5f2d7b26a37fc99f72`**, branche `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225) ; base `main` vérifiée `61bf1519730af5d81aeef5faeeba31bed210bfac`. [CI push #39682532690 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39682532690), [CI PR #39682540173 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39682540173). Builds : **`esp32-cyd` 49 536 B RAM statique / 825 497 B flash**, `esp32-cyd-bringup` **49 688 / 846 301 B**, `esp32-cyd-trace` **49 584 / 842 265 B**. Les chiffres build ne sont pas les mesures du heap en exécution.

**Frontière technique retirée** : `Render_loadMappings(Render_t*)` était seulement un adaptateur booléen vers le chargeur déjà natif `EspLegacyMappings_load(struct Render_s*)`. CONFIGMAP appelle désormais directement `EspLegacyMappings_load(render) ? true : false`. ABI `Render_loadMappings` déclarée uniquement côté desktop (`#ifndef DOOMRPG_ESP32`), sans provider ni appel native. **Le corps du parseur natif n'a pas changé**, FNV source **`da45ea1f`**, quatre tableaux persistants, vérification de l'en-tête, lecture PAK bornée dans le framebuffer partagé comme scratch, échec fail-closed et contrat de démarrage inchangés. Nouveau test `ESP32/test/native_render_mappings_retirement_host.py` : **4096 cycles** du véritable corps C du CONFIGMAP startup avec succès/idempotence et branches invalides, plus absence de l'ABI dans les sources ESP32. Tests existants keybind et mocks build adaptés aux nouveaux symboles sans modifier le gameplay. Deux runs CI intermédiaires rouges étaient causés par assertions historiques et faux `project_src_dir` du host ; la **CI finale pour ce SHA est verte**.

**Preuves série reçues sur ce SHA, firmware normal** :
- **BOOT** : RGB565 logique `160x120`, framebuffer partagé **38 400 B**, `PSRAM=0` sur classic CYD ; carte SD montée **240 MiB**, PAK natif **241 entrées**, palette `3280` entrées / **6560 B**, `RENDERSTART used=6576 heap8=163300`. CORE `Game=0`, `DOOMCANVASBRIDGE exports=0`.
- **Mappings directs** : `[MAPPINGS] -> EspLegacyMappings_load() heap8=163300 largest8=110580` ; `mappings.bin bytes=8392 backing=pak`, en-tête **592 / 1300 / 152 / 252**, `Plan payload=8376B largestAlloc=5200B` ; `[MAPPINGS] INSTALLED payload=8376 heap8=154860 largest8=110580 scratch=framebuffer noInflatedHeap=yes` ; `EspLegacyMappings_load result=1 used=8440` puis `CONFIGMAP READY mappingPayload=8376`. Ainsi ni payload ni allocations des mappings n'ont changé.
- **MENU/START/INTRO** : MENU_MAIN `frame=0f2c9fc3`, initial `ALIVE heap8=154860 largest8=110580` stable à 5 et 10 s ; `MAINSTART framebufferFNV=fec57f30`, `shapeData/mediaTexels=NULL`; nettoyage mappings `154860->163300 gained=8440`; `PLAYERSTATE FNV=e745fce9`; première intro `ade0195d`, progression tactile `REVEAL→MORE→CONTINUE→SKIP-ANIM→REVEAL→FINAL-CONTINUE`, `INTRODISP recovered=34056` et FNV de framebuffer inchangé pendant le dispose.
- **Entrance** : `/intro.bsp` **21823 B**, CRC `623f34e4`, `MAPRT arenaBytes=14095 arenaFNV=c3882516` ; `FIRST_FRAME=71ca7465` ; `FRESH-VISIBLE/READY` avec `shapeData=0 mediaTexels=0`, `RAMBUDGET advisory=HEADROOM_OK`. Navigation `904→872→840`, rotation `64→128`, avance `840→839→838`, ramassage des deux Armor Shards (`armor 0→4→8`), porte **ligne 275** `DOOR_OK` et `DOORANIM frames=4`, puis franchissement `838→837→836`. `ALIVE` en jeu `heap8=113768 largest8=86004` aux échantillons transmis.
- **HUB/EXIT** : navigation Inventory puis System, entrée AUTOMAP en HUB `IGNORED` sans mutation/turn, EXIT à double confirmation sans écriture (`saveWrite=no`) ; `RESIDENTRESET heap8=141696->159704 released=18008 empty=1`, `MAINMENU cleanup gained=0`, retour MENU_MAIN `finalFNV=0f2c9fc3`, `ALIVE heap8=159704 largest8=110580`.

**Limites du PASS** : chemin ciblé **BOOT→START→INTRO→FRESH GAMEPLAY→PICKUPS/DOOR→EXIT**. Pas de LOAD/SAVE nouveau, combat, fatal `DoomRPG_FreeAppData`, ni preuve exhaustive d'absence de fuite sur sessions multiples. Les fingerprints des cadres de warm cache et l'état de RNG varient normalement avec contexte/graine ; ne pas leur attribuer des invariants non définis. `NATIVEFRAME BOUNDARY-INLINE` est un fallback déjà connu. **Code `d284329b` gelé après test ; clôture exclusivement docs-only, PR merge-ready après CI de documentation ; aucun merge automatique.**

---

## PR #225 — Suppression ABI Render_setGrayPalettes : PASS matériel CYD (21 octobre 2026)

**Code normal `esp32-cyd` exactement testé : `3acd358e15a13430e8aa991f2797da73d75ac91f`**, sur `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225) ; base main inchangée `61bf1519730af5d81aeef5faeeba31bed210bfac`. [CI push #39677462114 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39677462114), [CI PR #39677472767 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39677472767). Budgets statiques : **normal RAM 49 536 B / flash 825 509 B**, bringup **49 688 / 846 317 B**, trace **49 584 / 842 261 B**.

**Retrait** : `Render_setGrayPalettes(Render_t*)` n'est plus défini ni déclaré côté ESP32 ; la fonction desktop de référence subsiste sous `#ifndef DOOMRPG_ESP32`. Aucun appelant natif dans `ESP32/src` ni `ESP32/include` selon test de régression dédié (sources C/C++ et headers, commentaires/chaînes exclus). Fonction legacy supprimée sans remplacement : elle mutait directement palettes et, côté desktop, plans de texture, contraire à l'ownership compact du port. FNV du corps legacy de référence **`6ff4291e`** préservé dans le test ; aucun nouveau chemin grayscale, allocations natives, map-wide textures, ZIP runtime, RNG ou gameplay.

**Serial exact reçu pour ce SHA** :
- **BOOT normal** ESP32-D0WD-V3, 4 MB flash, **PSRAM 0**, SD 240 MiB, PAK **241 entrées**, `palettes.bin` **6564 B**, palette `entries=3280 bytes=6560`, framebuffer partagé `160x120 RGB565` **38 400 B**, `RENDERSTART result=1 used=6576 heap8=169876->163300`, mappings PAK **8376 B** ; `Game=0`, `DOOMCANVASBRIDGE exports=0`.
- **MENU_MAIN** `frame=0f2c9fc3`, `ALIVE heap8=154860 largest8=110580` constant aux relevés 5–45 s puis 180 s ; `MAINSTART` armé `fec57f30`, `[MAINMENU] Runtime cleanup heap8=154860->163300 gained=8440`, `shapeData/mediaTexels/nodes/lines/mapSprites/mappings=NULL`.
- **INTRO** FNV première frame **`ade0195d`** ; touche semantic reveal/more/continue, `FINAL-TEXT-PRESENTED` puis `FINAL-CONTINUE` ; `INTRODISP recovered=34056`, framebuffer inchangé pendant dispose.
- **MAP_INTRO** `/intro.bsp` CRC **`623f34e4`**, arena **14 095 B / FNV `c3882516`**, `FIRST_FRAME=71ca7465`, `MAPFLASH REUSE HIT`, `FRESH-VISIBLE/READY` ; déplacement `904→872→840`, rotation `64→128`, MOVE `840→839→838` ; `PLAYERRES` armure **0→4→8**. `ACTIONENGINE` SELECT `NOTHING_TO_USE` fail-closed sans mutation, puis porte **ligne 275** `DOORANIM frames=4`, `DOOR_OK`. Les `ALIVE` gameplay transmis donnent `heap8=113768 largest8=86004`, invariant `shapeData=0 mediaTexels=0` aux frontières.
- **EXIT** HUB System double confirmation, `saveWrite=no`, `RESIDENTRESET heap8=141696->159704 released=18008 empty=1`, cleanup MENU `gained=0`, FNV menu final **`0f2c9fc3`** ; `ALIVE heap8=159704 largest8=110580` répétées après EXIT.

**Interprétation prudente** : le `LARGE-LEARN-HOT` durant la préparation du cache et les FNV variables des frames cache intermédiaires ne démontrent pas de régression : le chargement et la présentation finaux aboutissent ; la première frame de référence et MENU_MAIN gardent leurs FNV historiques. Le `NATIVEFRAME BOUNDARY-INLINE` est un fallback préexistant. Le FNV RNG change naturellement selon la graine de boot ; pas de preuve de divergence. **PASS de non-régression BOOT→START→intro→Entrée→pickups/porte→SYS EXIT**, pas de nouveau LOAD/SAVE, combat, fatal `DoomRPG_FreeAppData`, ni garantie générale sur toutes les cartes/couleurs. Le SHA `3acd358e` est gelé comme frontière hardware ; documentation post-test **docs-only**, tout changement code ultérieur doit repasser CI et CYD. Pas de merge dans main.

---

## PR #225 — Retrait Render_loadPalettes ABI : PASS matériel BOOT → START → EXIT (21 octobre 2026)

**Code exactement flashé/testé sur classic CYD, `esp32-cyd` normal : `2080e250a0c60d81d3304988d8c89ea9fd328f7b`** ; branche `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), main de base `61bf1519730af5d81aeef5faeeba31bed210bfac`. [CI push #39675709558 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39675709558), [CI PR #39675715983 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39675715983). Builds normal **49 536 B RAM / 825 517 B flash**, bringup **49 688 / 846 321 B**, trace **49 584 / 842 269 B**. 4 096 cycles du vrai C loader palettes avec remplacement, couleur RGB565 et delta mémoire, corps historique normalisé FNV **`3573ad2c`** ; test initialiseur Render préservé FNV **`f6343a76`**.

**Frontière retirée** : `Render_loadPalettes(Render_t*)` n'est plus exposé côté ESP32. Son corps exact est devenu `static EspNativeRenderStartup_loadPalettes(Render_t*)` dans `render_startup_bridge.c`, et `EspNativeRenderStartup_initialize` appelle ce propriétaire privé. Déclaration desktop gardée sous `#ifndef DOOMRPG_ESP32` ; conversion via `Render_make565RGB` inchangée. Pas de modification de la source native PAK, du `PlatformVideo` partagé, des arènes, du RNG ou du gameplay.

**Preuves Serial sur le vrai CYD** : 
- **Boot complet** : ESP32-D0WD-V3, 4MB flash, **PSRAM=0** ; SD 240MiB, PAK indexé **241 entrées**, `/sintable.bin=1024 B`, `/palettes.bin=6564 B` par PAK ; `[RENDER] palettes loaded: entries=3280 bytes=6560`. Shared framebuffer **160×120 RGB565 / 38 400 B**, `ptr=0x3ffca054`, pitch=320 ; `RENDERSTART result=1 used=6576 heap8=169876->163300` ; mappings **8376 B** puis `heap8=154860`. `DOOMCANVASBRIDGE exports=0`, `Game=0`, objets desktop lourds absents.
- **Menu stable** : `MAINBOOT READY`, FNV final **`0f2c9fc3`**, `[ALIVE] heap8=154860 largest8=110580` à 5s, 10s, 120s, 125s, sans dérive observée. L'armement du bouton START préserve la frame attendue `fec57f30`.
- **START / intro** : `[MAINMENU] Runtime cleanup heap8=154860->163300 gained=8440`, tous les champs legacy `shapeData/mediaTexels/nodes/lines/mapSprites/mappings=NULL` ; player FNV `e745fce9`; intro première frame FNV **`ade0195d`**, `FINAL-TEXT-PRESENTED` → `FINAL-CONTINUE`, `INTRODISP recovered=34056` et FNV inchangé entre début et fin de disposal (la valeur de frame avant disposal dépend de l'horloge et des taps).
- **MAP_INTRO Entrance** : `/intro.bsp` CRC **`623f34e4`**, arena **14095 B / `c3882516`**, `ENGINESESSION FIRST_FRAME frame=71ca7465`, `MAPFLASH REUSE HIT`, `FRESH-VISIBLE/READY`. MOVE **904→872→840**, TURN **64→128**, MOVE **840→839→838**, `PLAYERRES COMMIT` armure **0→4→8** ; HUD 30/30 ; `shapeData=mediaTexels=0` aux frontières. `NATIVEFRAME BOUNDARY-INLINE` ponctuel déjà connu, pas un incident créé par ce jalon.
- **SYS EXIT** : HUB System, confirmation double `saveWrite=no`, `RESIDENTRESET heap8=141696->159704 released=18008 empty=1`, `[MAINMENU] Runtime cleanup gained=0` (slots déjà libérés) ; frame MENU_MAIN identique **`0f2c9fc3`** ; `[ALIVE] heap8=159704 largest8=110580` après retour. Pendant gameplay, `ALIVE heap8=113768 largest8=86004` ; stabilité sur les relevés fournis.

**Limites de la preuve** : PASS matériel de non-régression **palette startup + affichage menu/intro/monde + pickups + EXIT**. Le test n'inclut **pas de LOAD après EXIT**, de SAVE écrit, de combat, de reboot après sauvegarde ni d'appel du destructeur fatal `DoomRPG_FreeAppData`. Ne pas extrapoler cette validation aux autres jalons de code. SHA `2080e250` **gelé après PASS** ; seuls des commits documentaires sont permis pour sa clôture, un nouveau changement de code nécessitera sa propre CI et son test matériel. Pas de merge automatique.

---

## PR #225 — Render_freeRuntime 1→0 : PASS matériel CYD avec deux EXIT (21 octobre 2026)

**Dernier code effectivement validé sur le classic CYD, environnement normal `esp32-cyd` : `3b551a6d0c17f28b392fe04675f5f428b4d545a8`**, sur [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), branche `agent/esp32-retire-render-free-abi` issue de main `61bf1519730af5d81aeef5faeeba31bed210bfac`. [CI push #39650761704 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39650761704), [CI PR #39650771777 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39650771777). Budgets statiques inchangés : **normal 49 536 B RAM / 825 529 B flash**, bringup **49 688/846 341 B**, trace **49 584/842 281 B**.

**Frontière native et correction d'ownership** : l'ancienne ABI `Render_freeRuntime` ne doit plus être liée côté ESP32 ; le corps de cleanup des seuls champs compatibilité `Render` (ordre des frees/NULL inchangé) est désormais `EspNativeRenderRuntime_releaseCompatibility(Render_t*)` dans `render_startup_bridge.c`, déclarée côté ESP32 dans `esp_render_startup_bridge.h`. Deux appelants **réels** sont conservés : `DoomRPG_esp32ReleaseMainMenuMemory` à START et `EspNativeEngineRoot_releaseRender` au teardown fatal. La première tentative strict-root (`4b6c93d`) a été rejetée par CI parce qu'elle ignorait START, puis corrigée. Tests C : **4096 cycles du vrai nettoyage START** et gardes d'échec, **4096 cycles du vrai destructeur** (framebuffers shared/owned, full/partial, freePtr), empreintes normalisées `Render_freeRuntime=8042d34c` et `Render_free=7e988863`. Les allocations natives de map, le framebuffer PlatformVideo et les overlays restent indépendants. Aucun retour aux ZIP, gros `shapeData` ou `mediaTexels`.

**Preuves Serial de ce nouveau SHA (ne pas confondre avec le PASS antérieur de `dfd77e6`)** :
- **START** : MENU_MAIN sélectionné `fec57f30`; `[MAINMENU] Runtime cleanup ... heap8=154860->163300 gained=8440 largest8=110580->110580`, `nodes/lines/mapSprites/mappings/shapeData/mediaTexels=0x0`. L'intro `ST_INTRO` présente FNV `ade0195d`, séquence complète jusqu'à `FINAL-CONTINUE`; `INTRODISP recovered=34056` sans modifier le framebuffer retenu.
- **Entrée nouvelle partie** : `/intro.bsp` CRC `623f34e4`; `MAPRT arenaBytes=14095 arenaFNV=c3882516`, `MAPFLASH REUSE HIT`, `FRESH-VISIBLE/READY`, `shapeData=0 mediaTexels=0`. MOVE `904→872→840`, TURN `64→128`, MOVE `840→839→838`; Armor Shards `0→4→8` avec mutations et messages locaux bornés.
- **Premier EXIT** : HUB System EXIT double-confirmé `saveWrite=no`, `RESIDENTRESET heap8=141696->159704 released=18008 empty=1`, cleanup MAINMENU `gained=0`, retour MENU_MAIN FNV `0f2c9fc3`; `ALIVE heap8=159704 largest8=110580`.
- **LOAD réel après EXIT** : `SAVESLOTS MAIN-OPEN`, page 2, slot07 confirmé, SAVE **V11 5604 B** ; `NATIVESAVE LOAD world=...restored-exact` (resources, scripts, lignes, transformations, automap, 30 monstres, topology, position, activation, drops), `MONSTERSTATE RESTORE rngCalls=0`, `MONSTERACT RESTORE activeCount=22`, `MAPFLASH REUSE HIT`, `RESUME-VISIBLE/READY`. Un relevé `ALIVE heap8=138888` intervient **pendant** la reprise, avant la réallocation du cache ; ne pas le comparer à un heap steady-state.
- **Gameplay sauvegardé** : PASS_TURN, MOVE `310→278→246`, strafe `246→245`, SELECT porte160 (`DOORANIM` 4 frames, `DOOR_OK`) ; MOVE `245→213→181`, activation sprites6/7 **22→24**. `MONSTERATKVIS COMPLETE` avant `MONSTERRETAL COMMIT sprite7` : joueur HP **30→27**, armure **10→8**, RNG rollback pendant probe puis commit, `DAMAGECUE DONE delayed=no` et `VIEWFLASH EXPIRE`. Pas de tir du joueur dans ces **nouveaux** logs (ce tir était vérifié sur le SHA antérieur).
- **Deuxième EXIT** : HUB System EXIT double-confirmé sans écriture SAVE, `RESIDENTRESET released=18008 empty=1`, `[MAINMENU] Runtime cleanup ... gained=0`, retour FNV `0f2c9fc3`; `ALIVE heap8=159704 largest8=110580`. Durant jeu frais et LOAD : `ALIVE heap=179012 heap8=113768 largest8=86004` répété, sans dérive observée sur les fenêtres transmises.

**Portée de validation** : **PASS matériel de non-régression des deux appelants en pratique pour START** et des transitions normal START/EXIT/LOAD/EXIT, plus combat ; l'appel à la fonction de cleanup depuis le **teardown fatal** n'est pas exécuté matériellement, seulement testé en vrai C hôte. `SYS EXIT` correspond au reset de session et au cleanup des slots, pas à `DoomRPG_FreeAppData`. Pas de SAVE neuf, reboot, mort joueur, fatal teardown, mesure de fuite hors fenêtres, ni vérification visuelle pixel indépendante. Le code `3b551a6` est **gelé après hardware PASS** ; commits de clôture documentaire **docs-only**. Aucune modification du code ne peut hériter automatiquement de ce PASS.

---

## PR #225 — Retrait ABI Render_free : PASS hardware non-régression / destructor testé en C (21 octobre 2026)

**Code exactement testé sur classic CYD, profil normal `esp32-cyd` : `dfd77e63629a0a457ffe693d28db140cf30d2240`**, branche `agent/esp32-retire-render-free-abi`, [PR #225](https://github.com/Kharn27/DoomRPG-RE/pull/225), créée depuis **main exact** `61bf1519730af5d81aeef5faeeba31bed210bfac` (merge PR #224). [CI push #39554989115 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39554989115) et [CI PR #39555113606 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39555113606). Builds normal, bringup, trace et audits/tests hôte réussis. Budgets statiques : **normal 49 536 B RAM / 825 529 B flash**, bringup **49 688/846 341 B**, trace **49 584/842 281 B**.

**Frontière retirée** : `Render_free` n'est plus exportée ni déclarée pour ESP32. Le corps de destruction, conservé hors nom et `static`, appartient désormais à `engine_stubs.c` sous `EspNativeEngineRoot_releaseRender(Render_t*, boolean)` et n'est appelé que par `DoomRPG_FreeAppData` (teardown fatal root). Ordre préservé : détacher le framebuffer si alias du `PlatformVideo` partagé, `Render_freeRuntime` (slots de compatibilité hérités), libérer plafond/sol/échelle/palettes, puis framebuffer possédé, texture éventuelle et `freePtr`. **Le framebuffer partagé 160×120×2 B n'est jamais libéré par Render**. Le `Render_freeRuntime` reste une ABI et un owner séparé, à ne pas retirer sans nouveau jalon. `src/Render.h` conserve la déclaration desktop sous `#ifndef DOOMRPG_ESP32`. Aucun changement de gameplay, map, RNG, PAK, sauvegarde, rendu, timing ou input. Le libellé Serial `[RENDERFREE] ... owner=esp-native-render-startup` est historique, pas une preuve d'ownership du bridge.

**Preuves de parité hôte CI** : empreinte FNV-1a du corps `Render_free` normalisé **`7e988863`** ; **4096 cycles** du vrai C extrait de production avec full/partial state, framebuffer partagé/possédé, `freePtr` faux/vrai, absence de free du backing PlatformVideo et compteurs de nettoyage ; test root 4096 cycles, test startup 4096 plus échecs fail-closed. Le test startup a été réancré à `EspRenderStartupBridge_start` car il dépendait autrefois du commentaire précédant `Render_free`. Premier commit `3d7b4c8f` avait une CI rouge **test-only** sur l'ancienne ancre ; correction `dfd77e6`, puis deux CI vertes. Les tests hôte n'attestent pas d'un appel fatal réel sur le CYD.

**Preuves Serial réellement transmises sur le firmware normal** :
- **START/intro** : menu armé `fec57f30`, `MAINSTART shapeData=0 mediaTexels=0`, joueur neuf `HP30/30 armor0`; première intro FNV `ade0195d`, séquence jusqu'à `FINAL-TEXT-PRESENTED` puis `FINAL-CONTINUE`, `INTRODISP recovered=34056`, framebuffer stable à l'évacuation.
- **MAP_INTRO Entrance** : `/intro.bsp` CRC `623f34e4`, arena compacte **14 095 B / FNV `c3882516`**, `MAPFLASH REUSE HIT`, `ENGINESESSION FRESH-VISIBLE/READY`, `shapeData=mediaTexels=0`. MOVE `904→872`, rotation `64→128`, strafe `872→840`, MOVE `840→839→838`; deux Armor Shards `armor 0→4→8`, état joueur confirmé par `PLAYERRES COMMIT`.
- **SYS EXIT** : HUB/System/EXIT double-confirmé, `saveWrite=no`, `RESIDENTRESET released=18008 empty=1`; menu revenu FNV **`0f2c9fc3`**, `heap8=159704 largest8=110580` stable aux relevés transmis. Ceci est un **reset de session**, pas un appel à `Render_free`.
- **Premier LOAD après EXIT** : `SAVESLOTS MAIN-OPEN`, page 2 slot07, V11 **5604 B** ; `world=...restored-exact`, monster-state/positions/topology/activation et overlays rechargés ; `MONSTERSTATE RESTORE rngCalls=0` ; `MAPFLASH REUSE HIT`, `RESUME-VISIBLE/READY`, `shapeData=mediaTexels=0`. Monstres initiaux **30**, activés **22**.
- **Gameplay V11** : MOVE `310→278`, STRAFE `278→277`, MOVE `277→245`, SELECT porte160 avec animation 4 frames puis MOVE `245→213→181`, activation sprites 6/7 (**22→24**). `MONSTERRETAL COMMIT sprite7` : HP **30→27**, armure **10→8**, RNG commit après animation ; `DAMAGECUE DONE delayed=no`. Pistol sur sprite7 : HP **6→3**, armure **4→2**, munitions **38→37**, ordre de tour observé.
- **Mémoire observée** : en gameplay neuf et après LOAD `ALIVE heap=179012 heap8=113768 largest8=86004`, répétée dans les relevés fournis, sans variation. `NATIVEFRAME BOUNDARY-INLINE` ponctuel = fallback borné connu, pas un nouveau crash. Un `COMBAT-INPUT-BLOCK queued=no` durant animation est la politique préexistante, non corrigée ici.

**Portée exacte** : **PASS matériel de non-régression** sur START, intro, resident map, mouvement, pickups, EXIT, LOAD V11, porte et combat ; **PASS hôte du vrai corps de destructeur**, mais **aucune exécution fatale de `DoomRPG_FreeAppData → EspNativeEngineRoot_releaseRender` démontrée sur matériel**, donc ne pas revendiquer cette voie hardware. Aucun SAVE nouveau, reboot, mort joueur ni couverture universelle de fuites ; aucune preuve pixel visuelle indépendante sur la première scène LOAD. Code `dfd77e6` gelé après ces logs ; clôture post-test **docs-only**, pas de merge automatique.

---

## PR #224 — review P2 initial-WORLD lease : validation matérielle CYD ciblée (20 octobre 2026)

**Dernier code de production soumis au test CYD normal `esp32-cyd` : `f15c412c757c40eceacd353eec3c3439d33462c2`**, sur [PR #224](https://github.com/Kharn27/DoomRPG-RE/pull/224), depuis `main=6b1d0c44e90414dc95c8650fbe6ad048c5a70ae1`. [CI push #39463831546 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463831546), [CI PR #39463839288 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39463839288) : vrais tests C de composition ActionEngine, 4096 cycles du garde extrait EXIT→LOAD→BACK et première présentation WORLD, plus tests 4096 startup/fail-closed, audits ELF et builds normal/bringup/trace. Budgets statiques : **normal 49 536 B RAM / 825 581 B flash**, bringup **49 688/846 385 B**, trace **49 584/842 329 B**. Le code de cette version diffère du dernier test `0686740c` : la review P2 était fondée et le correctif exigeait donc un **nouveau** test matériel, fourni ensuite.

**Revue P2 validée et résolue** : après `EspNativeGameplayFacingLabel_refresh("SESSION-ARM")`, la première présentation du monde intervient avant `gameplayState.active=1`. Le garde antérieur `!isActive()` bloquait ainsi la peinture du label sur cette image initiale. `EspNativeResidentGameplay_ownsWorldPresent()` autorise désormais le compositeur soit en jeu actif, soit pendant un lease synchrone `firstWorldPresentArmed` strictement autour du premier `Esp32PlatformVideo_present()`. Le lease est annulé sur succès **et** échec ; `active`, horloge de jeu et entrée tactile ne s'arment qu'après présentation réussie. Sur MENU_MAIN, INTRO, LOAD/SAVESLOTS et écrans de transition, le garde refuse toujours le compositeur HUD (et conserve le `facing.dirty` pour le prochain WORLD). [Discussion P2 répondue et résolue](https://github.com/Kharn27/DoomRPG-RE/pull/224#discussion_r4322799482). Pas de changements dans RNG, calculs de combat, sauvegarde ou carte.

**Vrai test CYD transmis pour ce SHA, incluant un nouveau jeu et un LOAD V11 après EXIT :**
- **START / entrée** : MENU FNV `fec57f30` une fois armé ; premier ST_INTRO FNV `ade0195d`, séquence complète, `INTRODISP recovered=34056`. `/intro.bsp` CRC `623f34e4`, arena `14095 B` FNV `c3882516`; `MAPFLASH REUSE HIT`, empreintes vérifiées, buildUs=0 ; `SESSION-ARM`, `RESIDENTGAMEPLAY READY entry=fresh-first-frame` et `ENGINESESSION FRESH-VISIBLE` après `GAMEPLAYHUD REPAINT`. Le Serial démontre la séquence de présentation et l'activation, **pas le contenu pixel exact du label sans observation visuelle indépendante**. `shapeData/mediaTexels=0`.
- **Gameplay frais / EXIT** : mouvement `904→872` (`previewUs=108009`), HUB/System/EXIT confirmé sans écriture SAVE, `RESIDENTRESET released=18008 empty=1`; MENU_MAIN FNV `0f2c9fc3`, `heap8=159704 largest8=110580`.
- **Premier LOAD après EXIT** : `SAVESLOTS MAIN-OPEN slots=10 pages=2`, puis slot04 confirmé, SAVE V11 `5604 B`. Le bug historique de barre grise avait déjà été **visuellement confirmé corrigé** par l'utilisateur sur `0686740c`; sur le **nouveau** SHA, les logs prouvent l'ouverture sans anomalie signalée, sans inclure de capture/empreinte du haut d'écran.
- **LOAD restauré** : `NATIVESAVE LOAD ... version=11 ... world=...restored-exact`, 30 monstres, 13 activés, état joueur `HP33/34 armor23/23`, position `1632,544`, `MONSTERSTATE RESTORE rngCalls=0`, `MONSTERPOS RESTORE`, `RESIDENTGAMEPLAY READY entry=checkpoint-resume`, `ENGINESESSION RESUME-VISIBLE/READY`. `MAPFLASH REUSE HIT`, pas de reconstruction. `heap8=113768 largest8=86004` constants dans les échantillons observés, aussi avant EXIT en jeu neuf.
- **Vrai gameplay V11 / attaque** : PASS_TURN, MOVE `281→282`, Health Vial `HP33→34`, SELECT porte113 avec animation 4 frames, activation sprite322 `activeCount 13→14`, projetiles Malwrath 3 salves × 3 frames ; `MONSTERATKVIS COMPLETE`, `MONSTERRETAL COMMIT sprite322 HP34→28 armor23→17 rngRollback=closed`, `DAMAGECUE DONE delayed=no`, `VIEWFLASH EXPIRE`. `heap8=113768 largest8=86004` reste stable aux relevés ~2344, 2349, 2360, 2365, 2370 s. Pas de conclusion extrapolée sur les autres familles d'attaque.
- **Bruit identifié, hors périmètre PR #224** : `[NATIVESPRITE] CROSS` est un `printf` inconditionnel dans `esp_native_sprite_renderer.c:1367`; `TRANSIENT` aussi aux branches cull/draw (autour des lignes 1099, 1116, 1137), répété à chaque frame. Ces impressions détaillées devront passer en `DRPG_LOGT` dans un milestone ultérieur pour assainir `esp32-cyd` normal, tout en préservant `UNSUPPORTED/OVERFLOW`, les vrais compteurs de rendu et la possibilité de les activer avec `esp32-cyd-trace`. Aucun comportement de rendu ou diagnostic n'est modifié ici.

**Frontière de validation** : PASS de parcours hardware et combat ciblés + CI. Les pixels précis du label initial ne sont pas vérifiables par Serial seul ; pas de SAVE neuf, mort joueur, reboot ni fatal teardown. Ancien SHA `0686740c` et nouveau `f15c412c` sont documentés séparément pour ne pas mélanger leurs preuves. Clôture documentaire uniquement après le code testé ; **ne pas modifier de code sans nouveau test CYD**, ni merger automatiquement.

---

## PR #224 — Render_startup privé + correctif HUD sur premier LOAD après EXIT : PASS CYD ciblé (20 octobre 2026)

**Code normal `esp32-cyd` testé sur classic CYD : `0686740c1361068f3c96d05eab95e4713b37aa49`** ([PR #224](https://github.com/Kharn27/DoomRPG-RE/pull/224), `agent/esp32-retire-render-startup-abi`, base exacte main fusionné `6b1d0c44e90414dc95c8650fbe6ad048c5a70ae1`). [CI push #39343404155 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39343404155), [CI PR #39343408609 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39343408609), tests hôte C (4096 cycles startup + cinq chemins d'échec, 4096 cycles non-world EXIT/LOAD/BACK avec échec physical present, vrai compositeur de dégâts avec cas facing.dirty), trois builds normal/bringup/trace et audits ELF. **RAM statique normale 49 536 B / flash utilisé CI 825 537 B**, bringup **49 688/846 333 B**, trace **49 584/842 281 B**. Le log de flash local indique `Wrote 825920 bytes ... Hash ... verified` : longueur du firmware écrit distincte de l'occupation `Flash used` calculée par PlatformIO.

**Frontière Render native permanente** : `Render_startup` ne reste plus exporté ni déclaré pour ESP32 ; son unique implémentation native devient `static EspNativeRenderStartup_initialize` dans `esp_native_render_startup` propriétaire (fichier `render_startup_bridge.c`), appelée exclusivement par `EspRenderStartupBridge_start`. Le corps C est conservé à l'identique hors nom/static, FNV normalisé **`f6343a76`** ; lecture PAK de `sintable.bin`, `palettes.bin`, pitch 320, framebuffer unique RGB565 **38 400 B**, erreurs fail-closed et allocations sont inchangés. Les lignes Serial `[RENDERSTART] OWNER api=Render_startup` / `Render_startup result=1` restent des **libellés historiques**, pas des symboles ABI.

**Bug UI reproduit avant ce correctif, puis résolu sur le CYD :** après `SYS EXIT`, le **premier** affichage du sélecteur MENU_MAIN `Load Game` montrait la barre supérieure grise du gameplay ; BACK puis LOAD supprimait le défaut. Cause vérifiée dans le code : `EspNativeResidentGameplay_reset` réinitialise `EspNativeGameplayFacingLabel` avec `facing.dirty=1`. Le sélecteur efface pourtant les **160×120** pixels, mais le wrapper ActionEngine présentait ensuite `paintFeedback(NONE)` et retuilait `k.bmp` sur les 20 pixels supérieurs (et consommait `dirty`). Le correctif final `98f748b0`, suivi de commits **test-only** `39d075ac` et `0686740c`, fait court-circuiter la composition HUD à `__real_Esp32PlatformVideo_present()` tant que `EspNativeResidentGameplay_isActive()==0`. Les écrans MENU_MAIN, INTRO, SAVESLOTS et transition ne sont plus possédés par le feedback gameplay ; monde actif, animations, RNG et tours ne changent pas. **Le propriétaire d'un écran, non le peintre LOAD, porte la correction.**

**Preuves utilisateur sur le firmware normal final (code gelé `0686740c`) :**
- **Boot** : ESP32-D0WD-V3 240 MHz, flash 4 MB, PSRAM=0 ; Canvas 44 B, Game=0, Render 1532 B, `[RENDERCORE] SETUP ...160x80@0,20 arrays=1280B`, `[RENDERSTART] Render_startup result=1 used=6576`, palettes 3280 entrées, mappings 8376 B, `[LAYOUT] READY`; MENU_MAIN FNV **`0f2c9fc3`**, `heap8=154860 largest8=110580` répétables à 5/10/15 s.
- **START → Entrance** : intro premier framebuffer **`ade0195d`**, `FINAL-TEXT-PRESENTED` puis `FINAL-CONTINUE`, `INTRODISP recovered=34056` ; `/intro.bsp` CRC **`623f34e4`**, runtime compact **14 095 B / FNV `c3882516`**, `shapeData=0` et `mediaTexels=0`. Première exécution FLASH MAP `REUSE MISS cachedMap=2` puis copie interne complète **`buildUs=14665621`** (~14,67 s), empreintes index/payload vérifiées ; c'est une reconstruction ponctuelle du cache flash, pas une latence par action du gameplay. Entrée gameplay `[ENGINESESSION] FRESH-VISIBLE/READY`. RAM en gameplay `heap=179012 heap8=113768 largest8=86004` stable sur plusieurs `ALIVE`.
- **World / EXIT** : mouvements `904→872→840→839`, rotation `64→128`, ramassage Armor Shard `0→4`, HUB→System→EXIT confirmé `saveWrite=no`. `[RESIDENTRESET] ... released=18008 ... empty=1` et retour MENU_MAIN FNV canonique **`0f2c9fc3`**, `heap8=159704 largest8=110580`.
- **Scénario régression exactement défaillant précédemment** : *sur le premier* MENU_MAIN LOAD, `[MENUTOUCH] CONFIRM-PASS item=1` → `[SAVESLOTS] MAIN-OPEN slots=10 pages=2`, le testeur confirme **« plus de barre grise supérieur.. niquel »**, heap menu constant `heap8=159704 largest8=110580` aux relevés ~89/94 s. **PASS matériel visuel ciblé.**

**Précision de portée** : ce **nouveau** SHA n'a pas encore de trace BACK→LOAD, OPTIONS, chargement effectif d'un slot V11 ni combats ; ces trajets ont été éprouvés **sur le premier code `82b08e4d2faeab01f6289b2906ea3bd09c4e8fa1` avant la correction de composition**, pas sur `0686740c`. Ne pas fusionner les observations de SHA. Pas de SAVE nouveau, mort joueur, reboot ni fatal teardown. `NATIVEFRAME BOUNDARY-INLINE` sur rotation est un fallback borné déjà connu. L'utilisateur perçoit toujours ~300–400 ms de temps mort hors combat depuis les animations : **ce jalon ne résout pas la réactivité**, suivi distinct d'input/service. Le SHA n'est pas imprimé dans les logs ; l'identité du firmware repose sur le flash communiqué et le HEAD de branche. **Code gelé après PASS, clôture documentaire exclusivement ; pas de merge automatique.**

---

## PR #223 — retrait de Render_setup : PASS matériel CYD ciblé (20 octobre 2026)

**Nouveau dernier code matériellement éprouvé sur `esp32-cyd` normal : `eb93bf2fefe3aa48cd87b933648d3a84c78f9743`**, branche `agent/esp32-retire-final-canvas-free-abi`, [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223), base main exacte `59e5cd90c9360d1b3a08d83d33cd28ed0732e5b1`. [CI push #39338723652 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39338723652), [CI PR #39338727944 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39338727944) : tests host C/Python, builds normal/bringup/trace et audits ELF. Budget statique firmware normal **49 536 B RAM / 825 525 B flash** (−56 B contre `Render_init` déjà retiré), bringup **49 688/846 333 B**, trace **49 584/842 285 B**. Le log de flash local indique une écriture vérifiée de 825 904 B (taille du binaire chargé, distincte de l'attribution `Flash used` CI).

**Changement uniquement de frontière** : l'appel et le symbole ESP32 `Render_setup` ont disparu ; son corps fonctionnel, conservé sans modification hors `static` + nouveau nom, vit dans `engine_stubs.c` en `EspNativeEngineLayout_setupRender`, appelé une fois par `EspNativeEngineLayout_startupCanvas`. Fingerprint corps normalisé **`ec9d1151`** ; test du vrai C hôte sur **4096** cycles dimensions 160×80 paires/impaires, réinitialisation et **trois** échecs d'allocation. Les trois tableaux largeur160 (`ceilingColor`, `floorColor`, `columnScale`) conservent **1280 B** utiles ; hauteur impaire arrondie comme avant, frees/erreurs préservés. Le fingerprint source Canvas layout normalisé reste **`9166e429`**. Aucun changement `Render_startup`, palettes, mappings, RAM du framebuffer, PAK, RNG, gameplay, sauvegardes, moteur de rendu.

**Preuves Serial sur classic CYD ESP32-D0WD-V3, 4 MB flash / PSRAM 0 :**
- **Boot/layout** : `[RENDERCORE] INIT ... bytes=1532`, `[CORE] Render used=1548`, Canvas `exports=0/nativeText=1/bytes=44`, `Game used=0`, `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=1280B`, `[LAYOUT] READY ...`, `[RENDERSTART] Render_startup result=1`, framebuffer logique `160x120 RGB565/38400B` ; menu FNV **`0f2c9fc3`**, `heap8=154860 largest8=110580` identiques à 5, 10, 15 et 40 secondes.
- **START/intro/fresh** : première frame intro FNV **`ade0195d`**, FINAL-CONTINUE, disposer **34 056 B**, `/intro.bsp` CRC **`623f34e4`** ; arena compact **14 095 B, FNV `c3882516`** ; `ENGINESESSION FRESH-VISIBLE/READY`, zéro `shapeData/mediaTexels`. Mouvements `904→872→840→839→838`, pickup Armor Shard `0→4→8`, porte275 ouverte/animée, HUD/HUB OK. `ALIVE heap8=113768 largest8=86004` à 53, 59, 64 s.
- **EXIT** : HUB SYS confirme `saveWrite=no`, `[RESIDENTRESET] ... released=18008 ... empty=1` ; retour menu frame **`0f2c9fc3`**, `heap8=159704 largest8=110580` aux relevés à 69/74 s.
- **LOAD réel** : page2, slot07 **V11 / 5 604 B** ; `NATIVESAVE LOAD ... world=...restored-exact` (ressources, scripts, lignes/textures, suppressions, crates, automap, 30 monstres, topo/positions/activation/drops). `MONSTERSTATE RESTORE rngCalls=0` ; `RESUME-VISIBLE`, `shapeData/mediaTexels=0`. `MONSTERACT` **22→24** après porte160 ; déambulation, STRAFE, porte held-closed pendant le tour monstre, puis `DOORANIM POST-MONSTER-COMPLETE`.
- **Combats réels** : sprite7 `MONSTERRETAL COMMIT HP30→27 armor10→8` ; sprite6 plusieurs ripostes `MISS-COMMIT` avec RNG avancé ; Pistol sprite6 **hp8→2→0**, munitions 38→37→36 et GIBFX ; Pistol sprite7 **hp6→3**, puis Axe `weapon=0` **hp3→0**, `MONSTERDROP COMMIT slot5 type6 subtype1 visible=1`, GIBFX. Sprite7 riposte ensuite `HP27→24 armor12→10`. `ALIVE heap=179012 heap8=113768 largest8=86004` stables sur les échantillons 104, 109, 114, 119, 124, 129, 134, 139 et 144 s après la reprise.
- **Observation utilisateur / fluidité (non causée ni corrigée par ce retrait d'ABI)** : sensation de perte de nervosité depuis les animations, taps répétés. Les logs attestent `[RESIDENTGAMEPLAY] COMBAT-INPUT-BLOCK tap=8/9 ... queued=no` pendant une animation d'attaque monstre, `[MONSTERATKVIS] phaseMs=500`, frames de déplacement axial ~104–127 ms et rotations ×2 ~220–252 ms ; `[DAMAGECUE] DONE ... delayed=yes` avec gap 608 ms puis 349 ms sur certaines cues. Les taps bloqués en combat sont **explicitement abandonnés**, pas conservés pour exécution. Un `MENUTOUCH MISS hit=-1` est un toucher hors carte, à distinguer. La stabilité des ressources ne garantit pas la réactivité. **Suivi requis ultérieur** : analyser la politique d'admission pendant les animations/retaliations et définir, si sémantiquement sûr, un éventuel petit tampon borné (pas de double-tour/replay de tir, pas de mutation ni de RNG avant la clôture du tour). Ne pas masquer ces pertes en supprimant les logs ni optimiser `PlatformVideo_present` prématurément.

**Couverture/limites** : ces logs ne testent ni SAVE nouveau, ni mort joueur, ni reboot, ni teardown fatal du root, ni EXIT après LOAD ; pas de preuve universelle d'absence de fuite. Le SHA n'est pas imprimé en Serial ; attribution par le firmware utilisateur testé. `NATIVEFRAME BOUNDARY-INLINE` deux fois pendant START est un fallback borné déjà antérieur, non échec du setup. **Clôture du code `eb93bf2` = PASS matériel ciblé, code gelé, documentation seulement ; ne pas propager ce PASS à un code ultérieur.** Aucun merge automatique.

---

## PR #223 — retrait du constructeur Render_init : PASS CYD (20 octobre 2026)

**Dernier code réellement validé sur le classic CYD, profil normal `esp32-cyd`** : `19f52749b18cf60cc14ecbacd2a32e4756aac2b1`, sur `agent/esp32-retire-final-canvas-free-abi`, [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223), base exacte de `main` `59e5cd90c9360d1b3a08d83d33cd28ed0732e5b1`. Commit initial code `f97d16eac8895b95f84c5096529cfe6d7071ca8f`, correctif **uniquement de borne du test Canvas** `19f52749`. [CI push #39336850874 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39336850874) et [CI PR #39336855926 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39336855926) : host C/Python, trois builds, audit ELF et compilations. **RAM statique normal 49 536 B / flash 825 581 B** (−28 B par rapport au jalon Canvas-only 825 609 B) ; bringup **49 688/846 393 B**, trace **49 584/842 333 B**.

**Frontière et parité** : `Render_init` n'est plus un appel/export ESP32 ; son corps exact, initialisation incluant NULL/stockage fourni/OOM et zéro complet du `Render_t`, est déplacé sous le root natif `EspNativeEngineRoot_initRender` (`static` dans `engine_stubs.c`). FNV source normalisée **`884791fe`** ; vrai corps compilé en test host 4096 cycles d'allocation et de stockage fourni et simulation OOM. Le constructeur Canvas conserve le FNV **`b621a796`**. Les fonctions `Render_setup`, `Render_startup`, `Render_free`, le framebuffer partagé, palettes, mappings et rendu n'ont pas changé. `Render_t=1532 B` normal, `Game=0`, `DoomCanvas=44 B`.

**Preuves Serial du nouveau SHA** :
- **Boot et menu** : `[DOOMCANVASBRIDGE] INIT exports=0 nativeText=1 desktopTU=no bytes=44`, `Render_init` reste une **chaîne diagnostique**, pas un appel au symbole retiré ; `[RENDERCORE] INIT ... bytes=1532` / `[CORE] Render used=1548`; `CORE READY ... heap used=2500`, HUD `160x80@0,20`, framebuffer logique `160x120`, `Render_startup result=1`. Menu FNV **`0f2c9fc3`** et `heap8=154860 largest8=110580` stables aux ALIVE 5/10/55/60 secondes.
- **START→intro→Entrance** : `MAINSTART shapeData=0x0 mediaTexels=0x0`, PlayerState neuf 52 B, intro première frame **`ade0195d`**, `FINAL-TEXT-PRESENTED`/`FINAL-CONTINUE`, `INTRODISP recovered=34056` avec intro `heap8=129244 largest8=98292` ; `/intro.bsp` CRC **`623f34e4`**, native arena **14 095 B / FNV `c3882516`** ; `ENGINESESSION FRESH-VISIBLE/READY`, `shapeData/mediaTexels=0`. Moves `904→872→840→839→838`, deux Armor Shards `armor=0→4→8`, rotation et HUB. ALIVE fresh 71 530/76 533/81 536 ms : **`heap8=113768 largest8=86004`** constants.
- **EXIT / LOAD exact** : HUB SYS → EXIT confirmé `saveWrite=no`, `RESIDENTRESET released=18008 empty=1` ; menu FNV restauré **`0f2c9fc3`**, `heap8=159704 largest8=110580`. MENU_MAIN Load page 2 → slot07, **V11 / 5 604 B** valide ; `NATIVESAVE LOAD ... world=...restored-exact` avec ressources/scripts/lignes/crates/automap/états monstres/topologie/positions/activation/drops ; `MAINLOAD state=3 intro=skipped`, 30 monstres, 22 actifs, `MONSTERSTATE RESTORE rngCalls=0`, `ENGINESESSION RESUME-VISIBLE/READY`, `shapeData/mediaTexels=0`. ALIVE à 128 087, 133 091, 138 129, 143 131, 148 736, 153 810, 160 009, 165 105 et 170 204 ms : **`heap=179012 heap8=113768 largest8=86004`** invariants.
- **Gameplay repris réel** : MOVE `310→278→246`, STRAFE `246→245`, ouverture porte160 (`DOORANIM COMPLETE`), activation ennemis **22→24**, mouvement `245→213→181→180`, fermeture de porte tenue durant riposte puis `DOORANIM POST-MONSTER-COMPLETE`. Player pickup Armor Shard `10→14`. Monstre sprite7 `MONSTERRETAL MISS-COMMIT` (RNG validé, joueur inchangé), sprite6 `MONSTERRETAL COMMIT` deux fois `HP30→28→26`, `armor14→12→10`, `DAMAGECUE DONE`. Deux tirs Pistol contre sprite6 : `hp8→2→0`, `ammo38→37→36`, mort et `GIBFX` peint puis expiré ; tir contre sprite7 `hp6→3`, `ammo36→35` puis riposte esquivée. `MONSTERACTIVESEQ COMPLETE` jusqu'au tour 10, pas d'échec gameplay.
- **Limites** : le `[NATIVEFRAME] BOUNDARY-INLINE` ponctuel après une rotation est un fallback borné antérieur, pas une panne liée au constructeur. Ce test n'a pas exercé SAVE neuf, mort joueur, fatal root cleanup, reboot ni sortie après LOAD ; heap stable sur les fenêtres observées, pas preuve d'absence universelle de fuite ; aucun SHA inscrit dans Serial, attribution fondée sur le firmware flashé déclaré.

**Clôture** : **PASS physique ciblé du code `19f52749b18cf60cc14ecbacd2a32e4756aac2b1`** ; ce SHA doit rester gelé, clôture documentaire uniquement. La poursuite sur cette branche est autorisée mais constitue un **nouveau jalon de code**, avec sa propre CI puis un nouveau PASS matériel obligatoire. Ne pas merger main automatiquement.

---

## PR #223 — Canvas final 1→0, destructeur native-root : PASS CYD ciblé (20 octobre 2026)

**Dernier code physiquement validé : `201232031e595563a42ca1e0d0f34d0252b1ab07`**, branche `agent/esp32-retire-final-canvas-free-abi`, [PR #223](https://github.com/Kharn27/DoomRPG-RE/pull/223) depuis **main fusionné exact `59e5cd90c9360d1b3a08d83d33cd28ed0732e5b1`**. CI [push #39334948960 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39334948960) et [PR #39334955182 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39334955182) : tests hôte C/Python (destructeur réel **4096 cycles NULL/full**, ordre story → intro → font → SDL_free, FNV du glyph blitter `4c4a9a93` et init `b621a796`), builds `esp32-cyd`, bringup, trace, audits ELF. **Normal RAM statique 49 536 B, flash 825 609 B** (−20 B vs dernier code #222) ; bringup **49 688/846 409 B**, trace **49 584/842 357 B**. Ce sont des résultats CI, distincts des relevés heap Serial.

**Frontière native** : dernier appel `DoomCanvas_free(..., true)` retiré du root ; `EspNativeEngineRoot_releaseCanvas` private/static de `engine_stubs.c` conserve exactement **StoryFit → IntroState → DoomRPG_freeImage(font) → SDL_free(Canvas)**. Ancien `esp_legacy_doomcanvas_bridge.c` renommé/dégraissé en `esp_native_canvas_text.c` : texte const-safe et blitter privé inchangés, sans allocation persistante. `src/DoomCanvas.h` n'expose plus `DoomCanvas_free` sous `DOOMRPG_ESP32`. **Zéro export desktop Canvas de cycle de vie** (init, setState, startup, free, drawString1 ; le no-op bringup historique `DoomCanvas_updateLoadingBar` est séparé, non gameplay). Aucun changement PAK, RNG, framebuffer, save ou mutations gameplay.

**Serial classic CYD fourni après flash** :
- **Boot/menus** : ESP32-D0WD-V3/4 MB/no PSRAM, `[DOOMCANVASBRIDGE] INIT exports=0 nativeText=1 desktopTU=no bytes=44`, `CORE READY stages=5 allocatedObjects=4 heap used=2500`, Render 1532/Game 0/Canvas 44 ; framebuffer 160×120 + HUD view 160×80@0,20, `MENU_MAIN finalFNV=0f2c9fc3`, `heap8=154860 largest8=110580`. Options `0acffb35`, Help/About 83 lignes, pages `5f22cf6b/d0788359/9213df95` puis retours menu exacts `0f2c9fc3`, sans rechargement BSP.
- **START/intro** : `MAINSTART shapeData=0 mediaTexels=0`, PlayerState neuf 52 B, `ST_INTRO=9`, premier FNV `ade0195d`, intro active `heap8=129244 largest8=98292` (ALIVE à 10 525 ms), `FINAL-TEXT-PRESENTED` puis `FINAL-CONTINUE`. `INTRODISP READY ... recovered=34056`, `INTROFIT HAND-RELEASE`, `INTROSTATE RELEASE`, pas de load BSP dans disposer. `/intro.bsp` CRC `623f34e4`, compact arena **14 095 B** FNV `c3882516`, `NATIVEBOOT READY` ; `ENGINESESSION FRESH-VISIBLE/READY`, world + HUD présentés, `shapeData/mediaTexels=0`. ALIVE fresh à **19 674 ms** : `heap8=113768 largest8=86004`.
- **EXIT et LOAD réel** : depuis gameplay neuf, HUB INV → SYSTEM → EXIT double-confirmé (`saveWrite=no`), `RESIDENTRESET released=18008 empty=1`, retour `MENU_MAIN FNV=0f2c9fc3`, ALIVE à 44 928 ms `heap8=159704 largest8=110580`. MENU_MAIN LOAD page 2, slot07 existant V11 **5604 B**, `result=valid`. `NATIVESAVE LOAD ... resources/script/lines/action/crates/automap/monsters/topology/positions/activation/drops ... restored-exact`, `MAINLOAD READY intro=skipped state=3`, 30 monstres, 22 actifs et `MONSTERSTATE RESTORE rngCalls=0`. `ENGINESESSION RESUME-VISIBLE/READY`, `shapeData/mediaTexels=0`. ALIVE à **55 098, 60 638, 65 672, 70 826, 75 858 ms** : **`heap=179012 heap8=113768 largest8=86004` identiques**, malgré mouvements/portes/combats dans l'intervalle.
- **Gameplay après restauration** : MOVE `310→278→246`, STRAFE `246→245`, SELECT porte 160 (animation 4 frames), MOVE `245→213→181`, activation visible **22→24**. `MONSTERATKVIS COMPLETE` avant `MONSTERRETAL COMMIT` sprite7 `HP30→27 armor10→8 damageTotal=5`, puis `DAMAGECUE DONE`. Tir Pistol sprite7 `hp6→3 armor4→2 ammo38→37`; tir sprite6 `hp8→2 armor2→0 ammo37→36`, riposte sprite6 `HP27→25 armor8→6 damageTotal=4` ; dernier tir sprite6 `hp2→0` et `GIBFX` visible, `ammo36→35` ; `MONSTERACTIVESEQ COMPLETE` jusqu'au tour 9. Les effets sanguins s'exécutent sans consommer le RNG gameplay.

**Limites strictes** : aucun SAVE neuf, aucune mort ou reboot de firmware, aucun chemin fatal `DoomRPG_FreeAppData` réellement déclenché ; la fonction native de libération root est prouvée par **test hôte**, pas par une fatal cleanup matérielle. Un `[DAMAGECUE] TIMEOUT probe=2` survient après un COMMIT de riposte valide pendant une séquence de combat chargée : l'intention visuelle est abandonnée selon la politique bornée `combat=untouched sequencer=never-blocked`; ne pas l'étiqueter comme échec de combat ni comme `DONE`. Pas de garantie d'absence universelle de fuite : seulement la fenêtre `ALIVE` fournie. Pas de SHA imprimé par Serial ; attribution fondée sur le flash communiqué.

**Décision** : **PASS matériel ciblé** pour la frontière Canvas ABI 1→0 et les chemins START → INTRO → Entrance → EXIT → LOAD V11 → combat. Geler ce **SHA de code** ; commits ultérieurs de clôture **docs-only**. L'utilisateur autorise la poursuite d'un jalon *distinct* sur la même branche, sans transférer ce PASS au code suivant. Ne pas merger `main` par l'agent.

---

## PR #222 — Canvas constructeur natif, ABI 2→1 : PASS matériel, prêt pour revue (20 octobre 2026)

**Dernier code matériellement validé sur le classic CYD** : `adb075773c8698fd2cd6fc8a913e07aa00412c21`, firmware **normal `esp32-cyd`**, branche `agent/esp32-retire-canvas-state-abi`, [PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222) basée sur `main 8b89a46151c1645ae77493b27e4f6925df6d99d1`. [CI push code SUCCESS #39332204874](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39332204874) et [CI PR code SUCCESS #39332212609](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39332212609) : host C/Python, builds normal/bringup/trace, audits ELF et retrait des TUs desktop. **RAM statique normal 49 536 B / flash 825 629 B**, contre 49 536 / 825 649 B sur le jalon 3→2 ; bringup **49 688 / 846 429 B**, trace **49 584 / 842 381 B**. Le SHA firmware n'est pas imprimé sur Serial ; lien au SHA fondé sur la version de branche flashée/testée par l'utilisateur. Les sections 3→2, 4→3 et #221 ci-dessous sont des preuves historiques de leurs SHAs, pas des évaluations de code postérieur.

**Frontière retirée** : `DoomCanvas_init` n'est plus un symbole lié pour ESP32. `EspNativeEngineRoot_initCanvas`, fonction `static` de `ESP32/src/engine_stubs.c`, conserve l'ordre `SDL_malloc` → `SDL_memset` → initialisation de `doomRpg`, `imgFont.imgBitmap=NULL`, `fontColor=0xffffffff`, `renderFloorCeilingTextures=true`. Corps normalisé par test hôte FNV **`b621a796`** (seuls nom du symbole et nombre d'exports dans le log changent). Constantes de layout 44 B et `_Static_assert` restent dans l'en-tête natif existant `engine_metrics.h`. Le bridge `esp_legacy_doomcanvas_bridge.c` conserve **un seul export de compatibilité Canvas : `DoomCanvas_free`** ; il garde également un raster texte natif privé, ce n'est pas encore un fichier entièrement retiré. Aucun nouveau TU, buffer persistant ou accès ZIP runtime ; `shapeData/mediaTexels` demeurent nuls.

**Preuves réelles fournies par Serial** :
- **Boot / allocation** ESP32-D0WD-V3 4 MB sans PSRAM : `[DOOMCANVASBRIDGE] INIT exports=1 ... bytes=44`, `[CORE] DoomCanvas used=60` (inclut overhead d'allocation), `[CORE] READY stages=5 allocatedObjects=4 heap used=2500`, `[LAYOUT] READY ... view=160x80@0,20` / 1 280 B tableaux Render. Menu FNV canonique **`0f2c9fc3`** ; `heap8=154860 largest8=110580` inchangés aux ALIVE 5/10/15 s.
- **START / intro / chargement natif** : `MAINSTART ... state=2 skipIntro=0 startupMap=1 shapeData=0x0 mediaTexels=0x0`, `PLAYERSTATE READY 52B`, intention audio STOP_ALL avant publication resource5039, première frame intro **`ade0195d`**, `heap8=129244 largest8=98292` stable durant l'intro, `INTROIN FINAL-CONTINUE` puis `INTRODISP READY recovered=34056`, pas de map load dans le disposer. `/intro.bsp` source CRC **`623f34e4`**, compact arena **14 095 B**, FNV **`c3882516`**, `NATIVEBOOT READY`, `ENGINESESSION FRESH-VISIBLE/READY`, `shapeData/mediaTexels=0x0`.
- **Gameplay neuf** : mouvements `904→872→840→839→838` avec 2 ramassages Armor Shard (`armor 0→4→8`), rotation sans tour, feedback HUD, `NATIVEFRAME BOUNDARY-INLINE` isolé connu (garde borné, pas d'échec). ALIVE `uptime=52664/57719/62721/72725 ms` : **`heap8=113768 largest8=86004`** constants.
- **EXIT / reprise V11 effective** : HUB SYS → EXIT confirmé, `saveWrite=no`, `RESIDENTRESET ... released=18008 ... empty=1` ; menu `0f2c9fc3` retrouvé ; `heap8=159704 largest8=110580`. MENU_MAIN LOAD page 2 → slot07, sauvegarde **V11 5 604 B** `result=valid` ; `NATIVESAVE LOAD ... world=...restored-exact` (ressources, script, lignes, entités, topologie, positions, activation, drops) ; `MAINLOAD READY intro=skipped state=3` ; `ENGINESESSION RESUME-VISIBLE/READY`, 30 monstres restaurés, 22 actifs, `rngCalls=0` durant RESTORE ; `shapeData/mediaTexels=0x0`. ALIVE `uptime=85973/90973/96487/101907/106908/112674 ms` : **`heap8=113768 largest8=86004`** constants.
- **Gameplay après LOAD et combat** : MOVE/STRAFE/ROTATE, arme changée, portes 152/160 ouvertes (152 ensuite refermée après tour et animation différée), activation des monstres **22→24** ; tir réel `sprite=7` `MONSTERCOMBAT COMMIT hp=6→3 armor=4→2 ammo=38→37`, sang `HITFX` et fermeture du tour ; riposte `MONSTERATKVIS COMPLETE` avant `MONSTERRETAL COMMIT hp joueur=30→27 armor=10→8 damageTotal=5` ; `DAMAGECUE DONE` et flash expiré, sans RNG modifié par le rendu. Ce log démontre une **attaque et une riposte réellement commitées** après reprise V11.
- **Limites** : ni SAVE d'un nouveau slot, ni mort, ni sortie après la reprise dans ces extraits ; pas de preuve générale d'absence de fuite ni comparaison exhaustive visuelle. Le `NATIVEFRAME BOUNDARY-INLINE` n'est pas une nouvelle régression. Aucun crash ni fail-closed non attendu observé dans le parcours transmis.

**Clôture / politique de merge** : le code **`adb075773c8698fd2cd6fc8a913e07aa00412c21` est gelé après PASS matériel**. Jusqu'à la fusion par l'utilisateur, **docs-only** (`PORTING_STATUS.md`, `DOCUMENTATION.md`, mise à jour description PR) et vérification CI. PR prête à être soumise à revue après CI documentaire verte, mais **ne jamais fusionner `main` par l'agent**. Le retrait du dernier export `DoomCanvas_free` est un jalon *futur* et n'appartient pas au code validé ici.

---

## PR #222 — retrait du startup Canvas, ABI 3→2 : PASS ciblé sur classic CYD (19 octobre 2026)

**Dernier SHA de code éprouvé par l'utilisateur sur le vrai classic CYD** : `5c76e7f73e019200c3296a5567891b21170093df`, branche `agent/esp32-retire-canvas-state-abi` ([PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222)), basée sur `main 8b89a46151c1645ae77493b27e4f6925df6d99d1`. [CI push #39330881971 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39330881971), [CI PR #39330886161 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39330886161) avec tests hôtes, normal/bringup/trace et audits. **Normal RAM 49 536 B, Flash 825 649 B** (RAM inchangée, −4 B de flash contre le code 4→3 `ebba9c6`), bringup **49 688 / 846 457 B**, trace **49 584 / 842 401 B**. Le SHA firmware n'apparaît pas dans le Serial ; la corrélation est celle de la branche flashée/testée par l'utilisateur. Ce SHA est dorénavant la dernière preuve matérielle, les sections 4→3 et #221 ci-dessous restent historiques.

**Retrait native owner** : `DoomCanvas_startup` ne fait plus partie du lien/ABI CYD : `esp_legacy_doomcanvas_bridge.c` ne publie que `DoomCanvas_init/free` (3→2). La fonction interne `EspNativeEngineLayout_startupCanvas` du propriétaire existant `engine_stubs.c` conserve à sa place exacte `Render_setup`, `startupMap=1`, `skipIntro=false`, chargement `a.bmp`, logs de layout. Le test host fixe le corps complet normalisé FNV `9166e429` et refuse les appels à l'ancienne ABI ; FNV texte `b4296a8f`, blitter `4c4a9a93` inchangés. Aucun nouveau buffer, module, ZIP runtime ni mutation du monde.

**Témoins CYD fournis** :
- Boot ESP32-D0WD-V3 sans PSRAM : `INIT exports=2 nativeText=1 desktopTU=no bytes=44`, `Render=1532 Game=0 Canvas=44`, `[LAYOUT] Begin DoomCanvas_startup`, `[RENDERCORE] SETUP view=160x80@0,20 arrays=1280B`, `[DOOMCANVASBRIDGE] STARTUP ... startupMap=1`, `LAYOUT READY` avec `heap8 180632→174020` (allocation de police, comme auparavant). Menu FNV **`0f2c9fc3`**, `heap8=154860 largest8=110580`. Réglage luminosité **25 %** relu depuis la configuration SD après redémarrage : `NATIVECONFIG LOAD ... brightness=25`.
- START direct : `MAINSTART` FNV attendu `fec57f30`, `state=2→9`, intention audio `STOP_ALL seq=2` puis musique `PUBLISH seq=3 resource=5039` (numéros de séquence dépendants du parcours, ordre préservé) ; première frame intro **`ade0195d`**, intro active stable `heap8=129244 largest8=98292` sur les échantillons 10/15 secondes ; histoire jusqu'au `FINAL-CONTINUE`. `INTRODISP READY` restitue exactement **34 056 B**, sans vieux chargement ZIP.
- `/intro.bsp` Entrance CRC **`623f34e4`**, compact arena **14 095 B** FNV **`c3882516`** ; `NATIVEBOOT READY`, `ENGINESESSION FRESH-VISIBLE/READY`. Les deux pointeurs `shapeData=0x0 mediaTexels=0x0` restent nuls. Jeu neuf `heap8=113768 largest8=86004` à **25 819 ms**, déplacements `904→872→840`, dialogue event **88** natif `EV_DIALOG(8)` suivi des reprises `EV_NEXTSTATE(19)` puis `EV_CHANGESTATE(11)` ; `DIALOGCHAIN RESUME` et mutations natives sans erreur.
- SYS → EXIT confirmé `saveWrite=no`, `RESIDENTRESET released=18008 empty=1` et menu **`0f2c9fc3`** ; chargement effectif slot **07**, V11 **5 604 B**, `world=...restored-exact`, état `3` / `ST_PLAYING`, sans intro. Reprise de 30 monstres, 22 actifs ; positions/topologie/activation restaurées, `MONSTERSTATE RESTORE rngCalls=0`. Frame/HUD chargés, `shapeData/mediaTexels=0x0`, `heap8=113568 largest8=86004` identiques à 168356, 173359, 178572, 184091 ms.
- Gameplay repris : MOVE/STRAFE/weapon SELECT/porte puis activation de **24** monstres. Une attaque réelle `sprite=7` a produit `MONSTERATKVIS ARM→COMPLETE` puis `MONSTERRETAL COMMIT playerHP=30→27 armor=10→8 damageTotal=5`, feedback visuel deux pulses `DAMAGECUE DONE`. Tir Pistol contre le même sprite : `MONSTERCOMBAT ROLL hitLoops=0` puis `COMMIT ammo 38→37 hp=6→6`, monstre turn livré. Vérifie l'ordonnancement combat effectif **après chargement**, et non seulement l'initialisation.
- **Limites** : aucune mort ni cycle SAVEnouveau observé sur ce SHA, pas de preuve d'absence de fuite universelle ; aucun nouveau screenshot indépendant. Le rapport `[ALIVE] heap8=112624` durant dialogue reflète un propriétaire paresseux temporaire, pas une baisse permanente démontrée. Aucun crash ou fatal dans les extraits. La séquence `MONSTERRETAL` est bien un COMMIT réel.

**Clôture du jalon 3→2** : code `5c76e7f73e019200c3296a5567891b21170093df` gelé après PASS matériel ; commit documentaire seulement, puis prochain jalon code séparé sur **la même branche**, avec un nouveau SHA et sa propre CI + validation physique. Ne jamais qualifier le nouveau code de PASS avant Serial CYD, ni fusionner `main` automatiquement.

---

## PR #222 — retrait de l'ABI d'état Canvas : PASS ciblé sur classic CYD (19 octobre 2026)

**Dernier SHA de code éprouvé physiquement** : `ebba9c6586ecb8919937f8067442faac08e2a2f8`, branche `agent/esp32-retire-canvas-state-abi`, [PR #222](https://github.com/Kharn27/DoomRPG-RE/pull/222) issue de `main 8b89a46151c1645ae77493b27e4f6925df6d99d1` après fusion #221. Firmware **normal `esp32-cyd`** : [CI push #39328930441 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39328930441) et [CI PR #39328938740 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39328938740), hôtes + 3 builds et audits. RAM statique normale **49 536 B** (identique #221), Flash **825 653 B** (**+8 B**), bringup **49 688 / 846 457 B**, trace **49 584 / 842 405 B**. Ce SHA remplace l'étiquette « dernier code matériellement testé » de #221 dans les sections plus anciennes ; leur preuve historique reste attachée à leurs propres SHAs. Le SHA firmware **n'est pas imprimé** par le journal Serial, l'association repose sur le flash de branche communiqué.

**Frontière supprimée** : le pont `esp_legacy_doomcanvas_bridge.c` ne publie désormais que `DoomCanvas_init/free/startup` (**Canvas exports 4→3**, `DoomCanvas_setState` retiré du build ESP32). La transition native `EspNativeMainMenu_transitionCanvasState` appartient à l'owner existant `native_main_menu_model.c`. Ses quatre appelants réels (activation MENU tactile, entrée modèle MENU, START → INTRO, LOAD → PLAYING) utilisent cette API ; les états hors MENU/INTRO/PLAYING restent rejetés. Le corps de transition + début d'intro est inchangé hors renommage, test hôte FNV source **`2f165f3f`** ; texte `b4296a8f`, blitter `4c4a9a93` inchangés. Pas de nouvelle TU source, pas de mémoire persistante, pas de conversion ZIP runtime, pas d'activation des états desktop hérités.

**Preuves matérielles sur ce code (extraits du 19 octobre)** :
- **Boot** classic ESP32-D0WD-V3, 4 MB, `PSRAM=0`; `[DOOMCANVASBRIDGE] INIT exports=3 nativeText=1 desktopTU=no bytes=44` ; `Render=1532 Game=0 Canvas=44` ; menu tactile état `0→2` / `ST_MENU`. Frame menu canonique **`0f2c9fc3`** ; `heap8=154860 largest8=110580` à 5 et 10 secondes.
- **HELP/OPTIONS** : Help parse 83 lignes, PAINT `0..7=5f22cf6b`, scroll `8..15=d0788359`, `16..23=9213df95`, `24..31=b0191189`, retour BACK canonique `0f2c9fc3`. Options `0acffb35`, luminosité persistée `100→25` via `NATIVECONFIG SAVE result=committed`, retour `OPTIONBACK FAST ... expected=0f2c9fc3 runtime=0f2c9fc3`. `shapeData/mediaTexels=0x0` sur Options et retour. Ne pas confondre le plateau temporaire `heap8=153436` observé pendant HELP avec une fuite après retour.
- **START → INTRO** : `MAINSTART` état `2→9` / `ST_INTRO`, séquence audio `STOP_ALL seq=6` puis `PUBLISH seq=7 resource=5039 flags=11` ; `INTROSTATE READY` (96 B), première frame `ade0195d`; taps/story/FINAL-CONTINUE puis disposer `INTRODISP READY ... recovered=34056` sans carte ZIP/legacy. Chargement Entrance `/intro.bsp` CRC `623f34e4`, compact arena **14 095 B** FNV **`c3882516`** ; `NATIVEBOOT READY`, `ENGINESESSION READY`, `shapeData=0x0 mediaTexels=0x0`.
- **Gameplay neuf + EXIT** : mouvements `904→872→840→839→838`, deux Armor Shards `armor 0→4→8`, porte 275 ouverte via quatre frames, tour SELECT_DOOR dispatché. Deux ALIVE gameplay `uptime=280112,285451` : **`heap8=113568 largest8=86004` identiques**. HUB SYS → EXIT confirmé, `saveWrite=no`, `RESIDENTRESET released=18008 empty=1`, menu FNV `0f2c9fc3`, `shapeData/mediaTexels=0x0`.
- **LOAD réel V11 + reprise** : après EXIT, MENU_MAIN LOAD → page 2 → slot07 armé/confirmé, fichier **5 604 B** `result=valid`, puis `NATIVESAVE LOAD ... version=11 map=1 pos=1440,608 angle=64 ... world=...restored-exact`. `MAINLOAD READY ... intro=skipped state=3` / `ST_PLAYING`, STOP_ALL audio `seq=8`. Restaurations de 30 monstres, dont **22 actifs**, position et topologie; `ENGINESESSION RESUME-VISIBLE`, `READY` ; MOVE/STRAFE et `MONSTERACTIVESEQ COMPLETE` réels, puis `AUTOMAP-OPEN`, mouvement en carte et `WORLD-COMPOSITOR` sans recouvrir la carte. ALIVE `uptime=316979,321983` : **`heap8=113568 largest8=86004` identiques**. Le journal établit l'avancement du séquenceur mais **pas** une attaque/dégât de monstre dans cet extrait.
- `[NATIVEFRAME] BOUNDARY-INLINE` isolé lors d'une rotation : récupération inline bornée déjà connue, sans `RETRY` fatal ni `CRASH` dans l'extrait. Pas de preuve générale d'absence de bug/fuite ni de test de mort dans ce nouvel extrait.

**Clôture** : PASS matériel **ciblé** sur les chemins concernés par l'ABI, confirmé par le log utilisateur (pas un SHA firmware Serial). Geler le code `ebba9c6586ecb8919937f8067442faac08e2a2f8` ; poursuivre uniquement par commits **docs-only** et CI documentaire, puis revue avant merge **humain**. `main` ne doit pas être fusionné par l'agent.

---

## PR #221 — PASS_TURN sur automap et mort depuis la carte : PASS CYD ciblé (19 octobre 2026)

**Dernier SHA de code éprouvé sur le vrai classic CYD, firmware normal `esp32-cyd` :** `c7faa293a702d2a259e51faa8a17b174fe896e32`, branche `agent/esp32-retire-desktop-canvas-text-abi`, [PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221). Base GitHub `main 90cb0d054e9ec234690c27935e68a19cc805a464`, non modifiée. **CI du même code :** [push #39324723379 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39324723379) et [PR #39324731800 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39324731800) ; tests C du vrai exécuteur `pass_turn_automap_host`, contrôle de routage, autres hosts, builds `esp32-cyd`, bringup/trace et audits ELF. **Statique normal : RAM 49 536 B, Flash 825 645 B** ; bringup 49 688 / 846 469 B, trace 49 584 / 842 409 B.

**Cause et frontière native validée :** avant ce correctif, `PASS_TURN` sur la carte était routé vers `AUTOMAP-IGNORE` : aucune avance de tour malgré les taps. Le `switch` résident partage maintenant le même exécuteur `EspNativeGameplayPassTurn_execute` qu'en vue `WORLD` : traitement de la case courante type 10/11, puis **une** demande de tour au propriétaire `MonsterTurn`, sans quitter l'automap. La présentation reste exclusivement à la carte ; `Turn passed.`, HUD 3D et flash 3D ne peuvent repeindre son écran. Un danger de sol non létal sur `PASS_TURN` peut publier son intention `AUTOMAPDAMAGE` uniquement **après** acceptation du tour ; si celui-ci échoue, rollback de PlayerState et aucune alerte. Le danger létal délègue à `PlayerDeath`. Les branches hazard et rollback sont couvertes côté host ; la session matérielle ci-dessous n'a touché aucun type 10/11.

**Témoins CYD du code `c7faa29` :**
- Séquence `AUTOMAP-OPEN` puis `PASS_TURN` **seq=14, 20, 21, 22, 23, 26, 27, 28, 29** : `[PASSTURN] REQUEST ... monsterTurn=requested`, `[RESIDENTGAMEPLAY] AUTOMAP-PASS status=0 turn=requested-or-lethal map=retained`, `[MONSTERTURN] ORDERED-DISPATCH reason=PASS_TURN` ; `activeCount=24`, `MONSTERACTIVESEQ BEGIN/COMPLETE`. Le joueur confirme « Pour moi c'est clean ». Les `PASS_TURN` n'étaient donc pas des actions UI décoratives ; les ennemis agissent réellement sur l'automap.
- Coups réels en `PASS_TURN` : `sprite=6`, attaques réussies et esquives (`MISS-COMMIT`), final idle visuel avant `COMMIT`. Exemple `seq=20` / `probe=5` : `HP 23→21`, armure `8→6`, `damageTotal=4`, `AUTOMAPDAMAGE ARM`; les phases rouge/noir puis `DONE` redonnent la carte canonique `ccdb8916`. Les probes suivants portent `damageTotal=4/6`, sans contamination de HUD 3D rapportée. La présentation `WORLD-COMPOSITOR` garde le FNV de carte en dehors des impulsions.
- Non-régressions annexes visibles : déplacement en automap `tile 181→180` avec fermeture de porte `DOORANIM AUTOMAP-COMPLETE` ; prise d'un `Armor Shard` `armor 8→12`, `PLAYERRES COMMIT` puis tour monstre normal ; fermeture `AUTOMAP-CLOSE` et `GAMEPLAYHUD REPAINT health=25/34 armor=10/23 weapon=2 ammo=38`, sans avancée de tour au seul changement d'écran.
- Coup **létal en automap** : `PASS_TURN seq=29`, `probe=12`, `HP 3→0`, `[PLAYERDEATH] ARM ... hudPixels=0` (aucun HUD 3D à peindre pendant la carte), puis `[MONSTERRETAL] LETHAL-COMMIT ... deathOwner=armed`, `[MONSTERACTIVESEQ] TERMINAL ... remaining=discarded`. `[PLAYERDEATH] PHASE elapsedMs=869 fall=complete`, puis `[PLAYERDEATH] READY elapsedMs=3010 phase=death-menu-ready frames=50 input=death-menu load=available`. `[NATIVESAVE] READABLE-SPATIAL ... slot01 ... result=valid` prouve la lisibilité du slot, **pas** l'exécution d'un chargement. Les frames de chute ne sont pas observées une par une.
- Plusieurs `[ALIVE]` entre `uptime=31909` et `82531 ms`, incluant attaques, changements de contexte et mort : `heap=179012 heap8=113768 largest8=86004` constants sur **50,622 secondes** ; SD/PAK/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENU `ready`. C'est une stabilité observée, pas une preuve universelle d'absence de fuite.

**Limites :** aucun `HAZARDPASS` matériel puisque chaque `PASS_TURN` indique `tileTouch=none type10/11=absent` ; rollback et danger létal dans le nouveau test C, pas ce log. Aucun CRIT ni SAVE/LOAD effectif après la mort, pas de capture pixel indépendante ou impression du SHA dans les logs. Les invariants `shapeData == NULL` / `mediaTexels == NULL` ne sont pas réimprimés par cet extrait. **Code `c7faa29` gelé après PASS** : seules des éditions documentaires sont permises avant fusion par l'utilisateur. La section `bb4668b` ci-dessous reste un PASS matériel antérieur, **pas** le dernier SHA testé.

---

## PR #221 — automap, rendu exclusif et dégâts visibles : PASS CYD (19 octobre 2026)

**Code réellement testé sur classic CYD, firmware normal `esp32-cyd` :** `bb4668b25be487aa80b85f8f3ac5f3871a006acc`, branche `agent/esp32-retire-desktop-canvas-text-abi` ([PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221)). Base `main 90cb0d054e9ec234690c27935e68a19cc805a464` non modifiée. **CI du même code verte** : [push #39301322358](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39301322358), [PR #39301350182](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39301350182) ; tests host dont `automap_damage_host`, `damage_cue_timing_host` et `script_startup_automap_host`, builds normaux/bringup/trace et audits ELF. RAM statique **49 536 B** (soit +16 B contre `7b5a6e3`), Flash **825 129 B** ; bringup RAM/Flash **49 688 / 845 949 B**, trace **49 584 / 841 897 B**.

**Contrat natif figé :** AUTOMAP possède seule la présentation sur l'écran 160×120 ; les rendus 3D de déplacements/attaques/animation de monstre sont calculés mais ne se publient pas par-dessus la carte. L'ouverture/fermeture bascule le propriétaire d'écran au bon moment ; à la fermeture, HUD complet live (arme/munitions/HP/armure) puis world redraw ; rollback vers carte si ce redraw échoue. Les feedbacks 3D (barre haut, border/flash, indicateur de direction) restent interdits sur l'automap et leurs snapshots ne sont pas restaurés sur un autre propriétaire. Une **nouvelle intention visuelle AUTOMAP**, publiée seulement après `MONSTERRETAL COMMIT` réellement non létal, fait deux impulsions du fond noir en rouge sombre puis revient au noir, avec texte `N DAMAGE!` ou `CRIT N DAMAGE!` en haut, **N = santé + armure** selon le texte legacy. Durée maximale **900 ms**, expiration par repaint de carte sans `uncover` ni mutation gameplay, petit état borné (+16 B), aucun framebuffer/texel map-wide supplémentaire ; PAK reste backing store.

**Témoins matériels du nouveau code `bb4668b` :**
- La session suit `AUTOMAP-OPEN seq=7` puis `STRAFE_RIGHT` en contexte `AUTOMAP`, `MONSTERACTIVESEQ` avec `activeCount=24`, et `AUTOMAP-FRAME reason=WORLD-COMPOSITOR`, sans bascule 3D rapportée par l'utilisateur.
- Premier coup réel : `MONSTERTURN MEMBER-ATTACK-PROBE` puis `[MONSTERRETAL] COMMIT probe=1 playerHP=30->27 armor=10->8 message="5 damage!" damageTotal=5`. `[AUTOMAPDAMAGE] ARM total=5 critical=0 durationMs=900`; le vrai CYD présente les **phases 0, 1, 2, 3** avec FNV **`e4e48586` rouge → `dbed1196` noir → `e4e48586` rouge → `dbed1196` noir**, puis `[AUTOMAPDAMAGE] DONE` suivi du framebuffer canonique `7c0cfbc6`. Le message et le clignotement ont été expressément acceptés visuellement par l'utilisateur (« c'est nickel… j'adore »).
- Deuxième coup après `AUTOMAP-OPEN seq=11`, `STRAFE_RIGHT` et animation de porte : `MONSTERRETAL COMMIT probe=2 playerHP=27->25 armor=8->6 damageTotal=4` ; `ARM total=4`, les phases **2 et 3** sont publiées dans ce log (FNV `e1986f76` / `c1166ae6`) puis `DONE` et carte de base `ed365836`. Les phases 0/1 ne sont pas attestées pour le deuxième coup : le premier repaint après la COMMIT survient à une phase ultérieure.
- `AUTOMAP-CLOSE hudPixels=7522 worldRedraw=yes mode=world`, `REPAINT health=27/34 armor=8/23 weapon=1 ammo=12 angle=128` ; aucune `AUTOMAP-FRAME WORLD-COMPOSITOR` intermédiaire après le repaint HUD. Le joueur confirme que la transition écran/carte et l'arme conservée fonctionnent. Le feedback visuel de monstre en 3D reste supprimé **uniquement** tant que l'automap possède l'écran.
- `[ALIVE]` **27 477, 32 495, 37 498, 42 560, 47 561 ms** : `heap=179012 heap8=113768 largest8=86004` constants dans la fenêtre d'environ 20 s, SD/PAK et autres services `ready`. Ce n'est pas une mesure de fuite sur toutes les cartes.

**Portée/limites** : ce PASS couvre deux coups non létaux, les pulses/texte, l'expiration, les mouvements et la fermeture d'automap, portes et HUD. Il ne démontre pas un coup critique, deux dégâts arrivant avant expiration, un hit létal avec animation de mort *sur ce dernier SHA*, ni un screenshot/fingerprint indépendant de chaque pixel affiché. Le host vérifie aussi CRIT, le wrap d'horloge et les bornes. Les protections `shapeData == NULL` / `mediaTexels == NULL` restent invariants du moteur ; leur impression n'apparaît pas dans ces extraits. Les tests antérieurs sur la mort/LOAD restent attachés à leurs SHAs précis. Aucun résultat hardware inventé.

**Statut** : plus de correction de code nécessaire d'après ce PASS ; commit de clôture **docs-only** puis CI, et PR `Ready for review` (merge uniquement par l'utilisateur). La section historique ci-dessous décrit `813987e`, correctif post-review antérieur, **qui n'est plus le dernier SHA testé**.

---

## PR #221 — régression post-review P1/P2 : PASS matériel CYD ciblé (19 octobre 2026)

**Dernier code réellement testé par l'utilisateur sur le classic CYD normal :** `813987e02b09c558364fba0d89b6ae4b62ca7656`, sur `agent/esp32-retire-desktop-canvas-text-abi` ([PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221)). Base GitHub `main 90cb0d054e9ec234690c27935e68a19cc805a464`, non modifiée. **CI du code** : [push #39251963619 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39251963619), [PR #39251973504 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39251973504) ; tests hôte, builds normaux/bringup/trace et audits compile/ELF. Statique normal RAM **49 520 B** (inchangée), Flash **823 197 B** ; bringup RAM/Flash **49 672 / 843 925 B** ; trace **49 568 / 839 857 B**.

### Deux findings Codex fondés, corrigés, threads GitHub résolus

- **P1 ([commentaire 4313645128](https://github.com/Kharn27/DoomRPG-RE/pull/221#discussion_r4313645128)) — death sans arme** : `EspNativeGameplayPlayerState_enterDeath()` transforme réellement `HP=0, weapon=0, weapons=0` avant les frames FALL. `EspNativeGameplayHud_snapshotLive()` accepte **uniquement** cet état sans arme canonique avec `weaponsPresent=0`, de sorte que la couche arme 1re personne est sautée par le renderer natif sans faire échouer `renderTurn`. `weapons=0` avec HP positif et les armes non possédées restent fail-closed ; ne jamais donner une arme fantôme à un joueur mort. Test hôte C du vrai snapshot répété sur plusieurs frames, modèle retained / PlayerState non mutés.
- **P2 ([commentaire 4313645176](https://github.com/Kharn27/DoomRPG-RE/pull/221#discussion_r4313645176)) — feedback bouton WEAPON_MENU** : `drawActionGlyph()` fournit désormais un glyphe vectoriel **W** jaune pour la zone HUD x100..159/y100..119. Test hôte C avec **vrai compositor de feedback**, framebuffer 160×120, journal d'éditions borné, durée 120 ms et **restauration exacte RGB565/FNV**. Pas d'allocation ni nouvelle mémoire permanente. Les deux fils de review ont reçu une réponse et ont été résolus sur GitHub.

### Témoins matériels réels fournis après les corrections

1. **Axe → Fire Ext** : tap `WEAPON_MENU zone=13 logical=139,111` puis `HUB-OPEN seq=21 page=weapons source=hud-ammo-compass` ; `HUBWGRID equipped=0 selected=0`. Tap `SELECT zone=5 logical=58,46` : deux `[GAMEPLAYHUD] REPAINT health=27/34 armor=8/23 weapon=1 ammo=12 angle=128`, `[HUB] CLOSE n=3 weapon=0->1 ... hudRepaint=yes hudBottom=3ec7786a expectedBottom=3ec7786a exactBottom=yes`, `[HUBWEAPON] GRID-SELECT-CLOSE target=1`, `[RESIDENTGAMEPLAY] HUB-CLOSE seq=22 turnAdvance=no`.
2. **Fire Ext → Shotgun après combat** : nouveau tap `WEAPON_MENU zone=13 logical=138,109` ; `HUBWGRID equipped=1 selected=1` ; `HUB-OPEN seq=25`. `SELECT logical=139,47` → deux `REPAINT health=25/34 armor=6/23 weapon=3 ammo=20 angle=128`, `HUB CLOSE n=4 weapon=1->3 hudRepaint=yes hudBottom=e6ba2d3c expectedBottom=e6ba2d3c exactBottom=yes`, `GRID-SELECT-CLOSE target=3`, `HUB-CLOSE seq=26 turnAdvance=no`. Aucune tentative `REBASE-FAILED` ou `GRID-CLOSE-ROLLBACK` dans l'extrait.
3. **Mort réelle, correctif P1 éprouvé** : `PASS_TURN seq=32`, monstre `sprite=7` inflige un coup létal `playerHP=5->0 lethal=deferred-player-death`, puis `[GAMEPLAYHUD] REPAINT health=0/34 armor=0/23 weapon=0 ammo=12 angle=64`. `[PLAYERDEATH] ARM seq=11 tile=149 playerFNV=b56537cc->71a6dde8 hp=0 weapons=000f->0000 weapon=3->0 ... viewZ=36 ... menu=deferred`, `[MONSTERRETAL] LETHAL-COMMIT ... deathOwner=armed`. `[PLAYERDEATH] PHASE seq=11 elapsedMs=836 fall=complete viewZ=6 fade=begin`, puis `[PLAYERDEATH] READY seq=11 tile=149 elapsedMs=3001 phase=death-menu-ready viewZ=6 fade=0 frames=50 input=death-menu load=available` ; le slot 01 est reconnu `[NATIVESAVE] READABLE-SPATIAL ... bytes=5604 ... result=valid`. **Le chemin létal→armement→chute achevée→fondu→menu fonctionne sur le vrai CYD** ; aucune coupure du service de mort dans le log reçu. Les frames individuelles FALL (niveau de log `DRPG_LOGD`) ne sont pas imprimées ici ; `frames=50` est le compteur de séquence, pas une preuve isolée de chaque frame.
4. **Routage et feedback P2** : `QUEUE action=WEAPON_MENU` puis ouverture directe du HUB à deux reprises, sans `[TOUCHFEEDBACK] SKIP` dans l'extrait sur le dernier code (contrairement au code avant fix). Cela valide l'admission du feedback et du geste ; le Serial normal ne montre ni `[TOUCHFEEDBACK] FLASH` (niveau de log) ni capture du glyphe jaune, dont la réalisation/restauration pixel-exacte est prouvée par test C, **pas par un screenshot matériel**.
5. **Mémoire sur cette session** : `[ALIVE]` de **207 285 ms** à **257 871 ms**, y compris combats, deux HUB et mort/menu : `heap=179028 heap8=113784 largest8=86004` sans variation sur les **50,586 s observées**. Tous les services SD/PAK/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENU sont `ready`. Pas de preuve d'absence universelle de fuite ; seulement cette fenêtre matériellement mesurée.

**Limites** : le code ne journalise pas son propre SHA firmware ; l'association au commit repose sur le flash déclaré. Pas de capture pixel du glyphe, de tests de cas grisé/BACK/sélection de la même arme ni de reprise de sauvegarde depuis le menu de mort dans cet extrait. Les invariants `shapeData/mediaTexels == NULL` sont protégés par les contrats de rendu/audits ; pas de ligne d'impression explicite fournie dans ce segment. Les preuves précédentes Canvas et sélections Pistol→Shotgun→Axe restent attachées respectivement aux SHAs `f9d8061` et `0482f13`, pas transférées artificiellement.

**Clôture** : corrections P1/P2 de `813987e` désormais **PASS matériel ciblé**. Après ce PASS, uniquement **documentation**, CI du commit documentaire, puis PR `Ready for review` si verte ; aucun merge de `main` sans ordre de l'utilisateur.

---

## PR #221 — jalon B : navigation armes via HUD, PASS CYD ciblé (19 octobre 2026)

**Branche** : `agent/esp32-retire-desktop-canvas-text-abi`, [PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221), base `main 90cb0d054e9ec234690c27935e68a19cc805a464`. **Dernier commit de code testé sur CYD** : `0482f13c2f62b9fc79e650a0b80bfc9695d55bd4`. [CI push SUCCESS #39246653544](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39246653544), [CI PR SUCCESS #39246661282](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39246661282) : tests hôte (dont le vrai helper C de snapshot HUD live Pistol→Shotgun), normal/bringup/trace, audits compilés/ELF. **Statique normal** RAM **49 520 B**, Flash **823 037 B** ; bringup **49 672 / 843 769 B** ; trace **49 568 / 839 693 B**.

### Contrat natif livré

- Tap HUD munitions/boussole **x100..159/y100..119** en pixels logiques 160×120 (zone 13, action `WEAPON_MENU`), quatre petits repères ambre. Accès direct à la **grille native 4×3 de douze armes** ; mêmes ressources du PAK, scratch statique borné 2 690 B, aucune nouvelle copie de sprites map-wide. La grille démarre sur l'arme courante ; les armes non possédées sont grises et refusées. Les trois familiers 9–11 occupent les trois cases originales, leurs icônes sont absentes du corpus source ; ne pas inventer d'assets.
- Le HUB classique ne comporte désormais que **INV / STAT / SYS** ; WPN a disparu des trois onglets et de leur cycle. La grille d'armes autonome possède une carte BACK. Choisir une arme possédée, y compris déjà équipée, ferme immédiatement le menu. Pas d'avance de tour, d'appel au moteur desktop ni de mutation world/entity.
- **Incident matériel réellement observé sur le code antérieur `45a549a5`** : sélection valide `weapon=2→3`, mais le double rendu de restauration divergeait (`[HUBHUD] REBASE-FAILED firstBottom=4ad75e77 secondBottom=f1d77c03`), fermeture `NOT_READY` puis `GRID-CLOSE-ROLLBACK`. La sélection joueur était annulée ; ne jamais étiqueter ce code intermédiaire PASS.
- **Correctif de cause racine** : `EspNativeGameplayHud_snapshotLive` dérive sur la pile l'état d'affichage à partir du `EspNativeGameplayPlayerState` courant, en reprenant HP/armure, arme, type/quantité de munitions via la table existante `EspNativeGameplayWeaponControl`, et angle de vue actuel ; aucune mutation du HUD immutable conservé à l'entrée. Le HUD se reconstruit une seule fois par peinture complète, sans deuxième overlay boussole potentiellement non idempotent. Le rendu de l'arme en vue subjective consulte le même instantané actuel, pas l'arme chargée historiquement. L'exigence **deux peintures avec FNV de bas de HUD strictement égaux reste active**, fail-closed si divergence, rollback sur échec ; aucun simple assouplissement de comparaison.

### Témoins CYD du code final `0482f13`

**Deux sélections réellement exécutées sur firmware normal**, même session, et pas seulement des tests hôte :

1. Tap `WEAPON_MENU zone=13 logical=130,110` → `HUB-OPEN page=weapons source=hud-ammo-compass`, grille `equipped=2 selected=2`. Tap `SELECT logical=145,44` : deux `[GAMEPLAYHUD] REPAINT health=39/39 armor=26/26 weapon=3 ammo=22 angle=128`. `[HUB] CLOSE n=1 weapon=2->3 playerFNV=d17395e1->f5760d24 exact=yes hudRepaint=yes hudBottom=f1d77c03 expectedBottom=f1d77c03 exactBottom=yes` ; `[HUBWEAPON] GRID-SELECT-CLOSE target=3`, `[RESIDENTGAMEPLAY] HUB-CLOSE seq=2 ... turnAdvance=no`.
2. Réouverture `WEAPON_MENU zone=13 logical=133,108` : grille `equipped=3 selected=3` confirmant persistance de Shotgun. Tap `SELECT logical=20,41` : deux `REPAINT ... weapon=0 ammo=14 angle=128` ; `[HUB] CLOSE n=2 weapon=3->0 playerFNV=f5760d24->856ea10f exact=yes hudRepaint=yes hudBottom=966e71ad expectedBottom=966e71ad exactBottom=yes` ; `GRID-SELECT-CLOSE target=0`, `HUB-CLOSE seq=4 ... turnAdvance=no`. `ammo=14` est le compteur interne rapporté au repaint pour l'arme 0 ; ce champ ne prouve pas à lui seul la typographie visible.
3. `[ALIVE]` aux **80 261, 85 262, 90 361 et 95 362 ms** : `heap=179028`, `heap8=113784`, `largest8=86004` constants sur les ~15 s observées ; SD/PAK/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENU tous `ready`. Aucun `REBASE-FAILED`, `GRID-CLOSE-ROLLBACK` ni `HUB-RECOVER` dans cette séquence.
4. `[TOUCHFEEDBACK] SKIP ... overlay-unavailable inputQueued=yes fatal=no` survient sur les deux taps du HUD : **l'overlay visuel de feedback n'est pas disponible**, mais le routage et l'ouverture fonctionnent ; non bloquant pour la fonctionnalité validée. `exactHud=topbar-pending` pendant CLOSE est attendu : le bas du HUD est exact et la barre haute est recomposée avec le redraw monde.

**Portée exacte du PASS** : routage HUD→grille, état de sélection Pistol→Shotgun→Axe, réouverture après première sélection, fermeture instantanée, réconciliation pixel-exacte du bas HUD, absence de tour et stabilité RAM observée. Le log ne contient ni screenshot/témoin explicite du sprite d'arme en vue subjective, ni test de sélection d'une case grise, BACK, sélection de la même arme, cycle INV/STAT/SYS, combat/SAVE/LOAD/EXIT ou assertion `shapeData/mediaTexels` à ce nouveau SHA ; ces routes bénéficient des tests/contrats de code mais **ne sont pas proclamées matériellement retestées ici**. Le firmware n'imprime pas de SHA embarqué ; son association au commit est celle du flash déclaré. La preuve du jalon A Canvas reste attachée au SHA `f9d8061` antérieur et n'est pas réinventée.

**Clôture historique avant reviews** : `0482f13` était le dernier code approuvé **à cette étape**. Les deux corrections de review et leurs nouveaux tests matériels sont documentés dans la première section ; ne plus traiter cette ancienne clôture comme le HEAD final.

---

## PR #221 — retrait de l'ABI texte Canvas desktop : PASS CYD ciblé (19 octobre 2026)

**Base `main` réelle** : `90cb0d054e9ec234690c27935e68a19cc805a464` (merge #220).
**Branche** : `agent/esp32-retire-desktop-canvas-text-abi`, [PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221).
**Dernier SHA de code testé sur CYD** : `f9d8061fd342e77d4c83271075c077a470b0207a` ; [CI push SUCCESS #39181089435](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39181089435) et [CI PR SUCCESS #39181183706](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39181183706).
**Jalon A clos sur le code cité** : `DoomCanvas_drawString1(char*)` est retiré de la surface ESP32 (Canvas **5 → 4 exports** : init/free/setState/startup). `EspNativeCanvas_drawText(const char*)` conserve le corps raster exact (FNV `b4296a8f`) et le blitter glyphes privé inchangé (FNV `4c4a9a93`). Les casts de chaînes constantes du menu disparaissent ; l'en-tête desktop garde son API hors ESP32. La CI rejette le symbole legacy et vérifie les trois builds (normal/bringup/trace) et audits ELF. RAM statique normal **49 520 B**, Flash **822 329 B** (−28 B par rapport au #220 en CI) ; diagnostics bringup **49 672 / 843 113 B**, trace **49 568 / 839 061 B**.

**Témoins matériels réels fournis par l'utilisateur**, firmware normal déclaré sur la branche :
- Help/About : **83 lignes**, FNV des offsets `0/8/16/24/32` = `5f22cf6b/d0788359/9213df95/b0191189/4c944ee5`; page 40 `943f0b77` puis retours 32 et 24 aux mêmes FNV ; BACK vers menu `0f2c9fc3`.
- Options : framebuffer `0acffb35` ; presets luminosité **75 → 100 → 25 → 100** sauvegardés nativement dans `/DoomRPG-ESP32.cfg`, BACK enfant et BACK parent ; retour exact au menu `0f2c9fc3`. `shapeData=0 mediaTexels=0` aux frontières.
- START : intro packed PAK, FNV première image `ade0195d`, progression complète de sept taps ; `INTRODISP` récupère **34 056 B** ; `/intro.bsp` CRC `623f34e4`, arena **14 095 B** FNV `c3882516`, `ENGINESESSION READY`, invariants `shapeData=0 mediaTexels=0`.
- Deux `[ALIVE]` en jeu aux uptimes **38 669 ms** et **43 714 ms** : `heap8=113584`, `largest8=86004`, stables sur cette fenêtre de ~5 s. Le log fourni commence au menu (pas d'en-tête boot), donc `exports=4` est prouvé côté code/CI, **pas indépendamment imprimé par le CYD** ; aucun SHA firmware embarqué.

**Limites** : PASS de navigation/texte/startup uniquement ; pas de LOAD, SAVE, combat, EXIT ou parcours multi-cartes à ce SHA. Les tests hôte et CI ne sont pas des mesures de pixels matérielles exhaustives. **Ce PASS ne valide aucun changement de code ultérieur** : le jalon UI armes dispose désormais de son propre PASS distinct, documenté en tête de ce fichier. Ne pas merger sans demande explicite.

---

## PR #220 — animation native des sprites et des voyants de porte (19 octobre 2026)

**Branche** : `agent/esp32-native-ambient-sprite-animation`, [PR #220](https://github.com/Kharn27/DoomRPG-RE/pull/220).
**Base main vérifiée** : `16155aec8e741151c5f057e95c7fe395a15521e8`.
**Dernier SHA de code (post-review, CYD à revalider)** : `5b0439f801532cf070d8dae57f13a7345d0eea29`.
**CI du dernier code** : [SUCCESS #39179286818](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39179286818), **56 étapes** (tests hôte, builds normal/bringup/trace, audits compilés/ELF). Le commit docs-only post-correctifs `f05214e6f05a377f1131da1392a9567ce6c2f3b4` a également passé la [CI #39179545535](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39179545535).
**Statique CI normal** : RAM **49 520 B** ; Flash **822 357 B**.
Bringup : **49 672 / 843 141 B** ; trace : **49 568 / 839 109 B**.
Le **PASS matériel ciblé antérieur** porte sur le code `3b8c2135bc2f4307eaa0deb633f7314c93ea1c27` ([CI verte #39176840460](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39176840460)).
Le commit initial de clôture `09125e4706343c377c1094dd4bb7cf0da0e86e16` était docs-only ; les **deux findings P2 apparus ensuite** ont exigé le correctif de code `5b0439f8` et une nouvelle validation. Celle-ci a été reçue sous forme d'un **PASS de régression ciblé** sur le vrai CYD (voir ci-dessous). Les commits suivants sont docs-only. La branche peut revenir en revue, le merge restant du ressort de l'utilisateur.

### Périmètre livré et ownership

- Sprites natifs non-fixes animés depuis les frames du PAK sans état mutable de carte ni random gameplay : éléments de décor/armor shards 4 phases / 250 ms ; humains 2 phases / 6 s ; monstres ordinaires au repos 0/3 / 4 s ; **sous-type 4 (Phantom / Lost Soul / Nightmare) au repos 0/3 / 250 ms** conformément à `src/Render.c`. Les animations explicites d'attaque, douleur et mort gardent leur propriétaire et leur `FIXED_ANIM`.
- Les previews d'ambiance passent par le même adaptateur natif des lignes dynamiques que MOVE/TURN. Cela respecte les portes ouvertes et le remplacement de texture 9 (rouge/verrouillée) par 10 (verte/déverrouillée), sans jamais réécrire l'état d'unlock à la présentation. Cette correction a éliminé la cause du retour visuel au rouge et de l'échec `[AMBIENT] FAIL` après ouverture relevés sur une révision précédente.
- Les voyants de porte 9/10 utilisent uniquement les **quatre frames physiques PAK** de leur texture, indexées par l'horloge monotone absolue, sans réinitialisation au déplacement. Rouge : `0,0,1,2,3,3,2,1` à **360 ms/pas**, cycle de respiration **2 880 ms** ; vert : `0,1,2,3` à **144 ms/pas**, cycle **576 ms**. Le dernier raster expose masque de voyants réellement dessinés et horloge échantillonnée ; l'idle ne redessine que lorsque la phase affichée diffère, indépendamment des rafraîchissements MOVE/TURN. Aucun timer ou texel map-wide.
- Rendu seulement si besoin en phase idle, sans tour de monstre, sans activation, sans nouvelle lecture ZIP, `/DoomRPG-ESP32.pak` restant l'unique backing store ; `shapeData==NULL`, `mediaTexels==NULL` préservés. **P2 corrigée :** les candidats d'animation ne sont armés qu'après des pixels réellement écrits par le raster de sprites (y compris les copies CROSS et les glows), jamais simplement à l'admission BSP. Les sprites cachés/culled/entièrement masqués par les murs ne doivent pas entretenir des redessins idle. Cadence supplémentaire à 250 ms uniquement pour les sous-types 4 visibles. Le RNG est gardé exact, rollback et suspension d'ambiance en cas d'échec, sans masquer un échec de rendu normal.
- **21 unités C desktop exclues**, Game/Canvas/Render natifs du jalon précédent inchangés. Le legacy reste oracle de format/comportement, et non nouveau fournisseur runtime.

### Témoins CYD réels du 19 octobre 2026 — PASS ciblé

L'utilisateur a confirmé des **armor shards qui clignotent** et une **flamme animée derrière une grille à distance** au début du jalon ; puis la **conservation effective du vert après EV_UNLOCK** sur les révisions correctives ; finalement, sur la révision de code **`3b8c2135bc2f4307eaa0deb633f7314c93ea1c27`**, **« Pour moi le rouge c'est ok »** valide subjectivement la respiration lente. Il n'a pas encore donné un verdict visuel explicite détaillé sur la nouvelle cadence verte *dans ce dernier extrait* (cadence verte laissée identique à la révision antérieure).

Le dernier extrait normal `esp32-cyd` contient `[ENGINESESSION] READY map=1`, `shapeData=0x0 mediaTexels=0x0`, les `[PLAYERRES]` armor shards / medkit / bullet clip, `[VIEWANIM]` MOVE et TURN, les événements/défilements de dialogue, `[DOORANIM] ARM/HOLD/RELEASE/COMPLETE` sur la ligne 275, sans échec de rendu ni `[AMBIENT] FAIL` **dans la portion fournie**. Les trois mesures au cours de la session évoluent avec des propriétaires légitimes :
`heap8=113784` (147 s, début du segment), `112640` (~162 s), puis `110216` (fin de dialogues et ~177–182 s), `largest8=86004` constant. Cela prouve la stabilité sur ces fenêtres observées, **pas** une absence universelle de fuite.
Le firmware ne journalise pas son SHA, les logs n'en prouvent pas directement l'identité binaire indépendante. Le test n'est ni un run complet du jeu, ni une revalidation SAVE/LOAD/combats/EXIT à ce SHA. La visualisation de la cadence rouge et les chemins listés sont seuls acceptés matériellement ici.

**Observation non bloquante à isoler plus tard** : `[NATIVESPRITE] CROSS index=77 ...` est très répétitif lors des rafraîchissements idle (aucune dégradation mémoire visible ici). Envisager un lot distinct de réduction de bruit des logs sans modifier le gameplay ni le code de ce jalon après PASS.
La rencontre « zombie shotgun » sur l'essai précédent avait une attaque alternative `weapon=3` et les coups du joueur `weapon=0` (hache) ; les tirages critique/manqués sont compatibles avec les calculs legacy dans les logs. Aucun nerf ni correctif RNG engagé dans la présente PR.

### Régression CYD du correctif post-review — PASS ciblé

**Retour utilisateur** : « Ça m'a l'air ok :) », après flash et navigation sur la branche corrigée (dernier code `5b0439f8`). Extrait du firmware normal : mouvements `FORWARD` et `STRAFE_RIGHT`, `TURN_RIGHT`, entrée en dialogue opcode 26, pages et fermeture du dialogue, ramassages medkit/munitions/Fire Ext, HUD/animations de flash. Aucun `[AMBIENT] FAIL` ni incident rendu rapporté dans cet extrait ; pas de lecture abusive ZIP ni d'indicateur rouge régressé signalés. Les mesures `[ALIVE]` de `uptime=228293` à `268626 ms` sont toutes `heap8=112640 largest8=86004`, stabilité observée sur environ 40 s.

**Limites du PASS** : les tours `[MONSTERACTIVESEQ]` montrent `activeCount=0` dans l'extrait ; aucune rencontre Phantom/Lost Soul/Nightmare et aucune mesure de fréquence idle ne prouve directement le nouveau chemin rapide du sous-type 4 ou l'absence de toute recomposition invisible. Leur correction s'appuie sur l'implémentation revue + tests hôte + CI, pas sur une observation physique spécifique de cette famille. Le dernier log n'établit pas non plus le SHA exact du binaire, ni une couverture SAVE/LOAD/tous niveaux.

**Statut** : **PASS de régression matériel ciblé + CI complète verte**, deux P2 corrigées et commentées dans leurs discussions GitHub. **PR #220 prête pour revue / merge par l'utilisateur**, sans extrapolation aux familles non exposées. Aucun correctif de code postérieur à ce dernier PASS.
 Les journaux historiques du désengagement desktop restent plus bas, datés de la PR #219.

---

## PR #219 — désengagement desktop C/Game/Canvas : état courant (18 octobre 2026)

**Branche** : `agent/esp32-retire-generated-doomrpg-oracle`, [PR #219](https://github.com/Kharn27/DoomRPG-RE/pull/219).
**Base main** : `6390a6d42e50a058a05ebc0f4483cb02894f065f`.
**Dernier SHA de code** : `a59554ed524502d02dbab69149777a94050b0a26`,
[CI SUCCESS #39162920720](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39162920720)
(**55 étapes**, tests host, builds normal/bringup/trace et audits ELF).
Les commits ultérieurs de clôture sont **documentation uniquement**.
Les témoins, FNV et métriques historiques des lots A-D sont préservés dans
[le journal #219 archivé](archive/PR219_DESKTOP_RETIREMENT_MILESTONES.md).

### Frontière de production actuellement retenue

- **21 unités C desktop exclues** ; aucune génération ni compilation de
  `DoomRPG.c` desktop. Le seul source adapté généré au build reste
  `esp32_sdl.cpp`, shim SDL packed indexed 1/4/8 bpp ; backing store natif
  **`/DoomRPG-ESP32.pak`**, et **aucun accès ZIP runtime**.
- **Game** : pas de `Game_t` alloué, ni de fournisseur/export `Game_init`,
  `Game_activate` ou `Game_unloadMapData`. Le champ `doomRpg->game`
  est encore présent pour compatibilité de layout, mais doit rester
  **NULL** sur les chemins boot/intro/maps/session/teardown. Métrique
  `Game=0` ; le journal de teardown confirme le reset des vrais owners.
- **Canvas** : cinq ABI externes seulement :
  `DoomCanvas_init`, `DoomCanvas_free`,
  `DoomCanvas_drawString1`, `DoomCanvas_setState`,
  `DoomCanvas_startup`. L'ancien
  `DoomCanvas_drawImageSpecial` est maintenant un blitter glyphes
  privé `static EspDoomCanvas_drawImageSpecial`, **corps inchangé**
  (FNV source `4c4a9a93`) ; la CI interdit l'ancien symbole ELF.
- **Diagnostics et const** : le boot annonce désormais
  `[DOOMCANVASBRIDGE] INIT exports=5` dans le source.
  Le libellé intro `Loading...` utilise un buffer local `char[]`
  pour respecter la signature `DoomCanvas_drawString1(char*)`.
  Trois warnings du même type subsistent historiquement sur les
  littéraux `BACK`, `UP`, `DOWN` dans
  `native_main_menu_actions.c` ; **ils ne bloquent pas** la CI et ne
  doivent pas être dissimulés par un cast. Prochain cleanup à traiter
  par API de texte const-correcte, dans un jalon distinct.
- **Taille CI dernier SHA code (normal)** : RAM statique **49 488 B**,
  Flash **820 997 B** ; bringup **49 640 / 841 617 B**, trace
  **49 536 / 837 613 B**. Aucune preuve d'identité ELF bit-à-bit
  n'est revendiquée, ni égalité obligatoire avec les jauges locales
  PlatformIO d'un autre environnement.

### Témoins matériels et limites, normal esp32-cyd

**PASS matériel des lots A–D**, puis **acceptation CYD ciblée du dernier
lot E** d'après les logs transmis par l'utilisateur le 18 octobre 2026.
Le dernier extrait **ne fournit pas l'en-tête boot**, donc ne prouve pas
directement la nouvelle ligne `exports=5` ; en revanche, la CI compile
le nouveau code et son test source confirme la chaîne.

- **MENU_MAIN et texte** : Help/About parse **83 lignes**, pages
  `0→8→16→24→32`, avec FNV `5f22cf6b`,
  `d0788359`, `9213df95`, `b0191189`, `4c944ee5`.
  BACK rétablit exactement le dashboard FNV **`0f2c9fc3`**.
- **Options** : modèle et panneau FNV **`0acffb35`**, bouton BACK
  fonctionnel, nouveau retour au dashboard `0f2c9fc3` ;
  brightness 75 % dans les anciens logs boot de la branche.
- **START / intro** : le nettoyage menu libère **8 440 B**,
  première image FNV **`ade0195d`**, texte de l'intro achevé puis
  navigation validée (six touches dans cet extrait : premier texte
  terminé naturellement, pas par tap). `[INTRODISP] READY`
  libère **34 056 B** (`heap8=129292→163348`) avec ressources
  relâchées exactement, sans moteur ZIP/desktop.
- **Entrance** : `/intro.bsp`, CRC **`623f34e4`**, arena
  **14 095 B** / FNV `c3882516`, `FIRST_FRAME`,
  `FRESH-VISIBLE`, `RESIDENTGAMEPLAY READY`, monstres
  natifs initialisés.
- **RAM stable dans les deux [ALIVE] finaux** (uptime ~211 s et
  ~216 s) : `heap8=113816`, `largest8=86004`,
  `shapeData=0x0`, `mediaTexels=0x0`.
  Le jeu a tourné normalement pendant l'observation.
- **Non exercés dans le dernier extrait E** : LOAD checkpoint,
  combat, retour EXIT, nouvelle sauvegarde, corruption SD,
  audio audible, autres cartes ; **ces scénarios ont des
  témoins distincts sur les lots antérieurs**, pas sur ce
  dernier code. Le log ne fournit pas de SHA firmware imprimé :
  on associe le PASS au flash déclaré de la branche,
  pas à un fingerprint binaire indépendant.

**Bilan de clôture** : lot E validé matériellement **sur les chemins
de texte/intro/Entrance observés** ; le firmware possède désormais
**Game=0** et **Canvas ABI=5**. Les invariants mémoire et
l'architecture native PAK sont tenus sur ce périmètre.
PR #219 préparée pour revue/merge **par l'utilisateur seulement**,
sans modification de code après le SHA validé.

---

## PR #218 — clôture matérielle : luminosité persistante, Config v23 et keybindings desktop retirés (18 octobre 2026)

**Base GitHub vérifiée** : `main 8a9c28940b03ab1ee10d5cdc720a26b6f08cbb4e`.
**Branche** : `agent/esp32-native-backlight-options`,
[PR #218](https://github.com/Kharn27/DoomRPG-RE/pull/218).
**Dernier SHA de code validé matériellement** :
`3c36f7544e537ec96f77c7fa077b66636cd6db0f`.
CI de ce SHA : [SUCCESS #39127502174](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39127502174)
et [SUCCESS #39127499576](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39127499576) ;
tests hôte, builds normaux `esp32-cyd` + bringup + trace, audit ELF/compiledb sur les trois profils.
**Validation CYD normal du 18 octobre 2026** : l'utilisateur a flashé et exécuté
ce SHA, fourni les traces de boot, Options/Luminosité, LOAD slot09
puis déplacements/armes/combats/montée de niveau. **PASS sur le périmètre
observé, sans prétendre à une couverture exhaustive du jeu**.
Seul l'utilisateur effectue le merge.

### Frontière de code native / desktop

- `src/Game.c`, `src/Render.c`, `src/DoomCanvas.c` et le
  `src/DoomRPG.c` desktop ne sont pas des TU compilées par le CYD ;
  les adaptateurs permanents nécessaires restent bornés.
- Le pont `esp_legacy_game_bridge.c` possède **3** exports
  (`Game_init`, `Game_unloadMapData`, `Game_activate`) ;
  **`Game_loadConfig` est retiré**, comme les lecteurs historiques
  `File_readByte`/`File_readInt` (ancien TU `esp_doomrpg_config_read.c`
  supprimé). `[CONFIGMAP]` n'ouvre plus `Config` v23.
- `keyMapping`, `keyMappingTemp`, `keyMappingDefault`
  (**12 × 44 = 528 octets chacun**) et
  `DoomRPG_setDefaultBinds` ont été supprimés de
  `esp_legacy_config_mappings_startup.c` et de l'init core. Leur
  ancien décodage PC clavier/souris/manette
  `DoomRPG_getEventKey`/`DoomRPG_setBind` n'est pas lié :
  la surface `src/DoomRPG.c/.h` reste **référence desktop intacte**.
  `EspLegacyConfigMappingsStartup_start` et
  `Render_loadMappings` continuent à charger `mappings.bin`
  depuis le PAK natif ; aucune dépendance keybinding n'a été
  réintroduite. L'audit
  `--require-retired-doomrpg-keybinds` vérifie l'absence des 6
  symboles et la présence de l'owner PAK mappings dans les ELF.
  Tests : `desktop_keybind_retirement_host.py` remplace le
  test de parité d'un état désormais inexistant.
- **Gain démontré**, profil `esp32-cyd` :
  RAM statique `51072 -> 49488` octets (**1584 libérés**),
  flash `821881 -> 821285` octets (**596 libérés**).
  La preuve hardware correspond aussi :
  après mappings, heap8 `153308 -> 154892` ;
  après une écriture SD, `153108 -> 154692` ;
  `largest8=110580` avant/après. Les allocations ponctuelles SD
  de 200 B sont observées séparément, **pas attribuées au
  retrait keybinding ni qualifiées de fuite**.

### Configuration native et sous-menu vidéo

- Backlight classic CYD GPIO21 actif haut, LEDC 5000 Hz/8 bits,
  canal 7, valeurs **25/50/75/100 %** (jamais 0 %).
- Dans Options, carte `LUMINOSITE` ; deux taps relâchés pour
  ouvrir le sous-menu, puis **un tap** sur un preset, BACK
  enfant vers Options et Back parent vers MENU_MAIN.
  Logo PAK étiré **102 × 120** à `x=0` intact, rail droit repeint
  sans rerender du monde, Input et Sound encore différés.
- Nouveau fichier SD `/DoomRPG-ESP32.cfg`, **12 octets**,
  magic `DRCF`, version 1, pourcentage, réservés zéro et
  FNV-1a LE32. Pas de conversion de `Config` v23 ni de
  dépendance aux `.sav` ou au ZIP runtime. Ouverture
  **après montage SD et avant affichage du menu**.
  Premier boot sans fichier = défaut 100 %, aucune création
  de fichier avant changement. Écriture vérifiée dans `.tmp`,
  sauvegarde `.bak`, promotion puis relecture ; backup valide
  récupérable après primaire invalide. Échec d'écriture
  **avant PWM**, pas de faux commit visuel.
- Hardware initial de la sous-fonction avant retrait keybindings :
  `[NATIVECONFIG] LOAD ... first-run-missing percent=100 write=no`,
  `[NATIVECONFIG] SAVE ... percent=50 bytes=12`,
  `[BACKLIGHT] SET percent=50 duty=128/255`, **débranchement
  et rebranchement**, `LOAD source=primary result=valid ...
  brightness=50` ; valeur conservée visuellement.
- Hardware du SHA final : boot
  `[NATIVECONFIG] LOAD source=primary result=valid
  version=1 bytes=12 brightness=100 write=no`, puis
  `SAVE result=committed percent=75 bytes=12`,
  `[BACKLIGHT] SET percent=75 duty=191/255`,
  `[OPTIONSBRIGHTNESS] SELECT percent=100->75 changed=yes`,
  navigation enfant/parent correcte. `heap8=154692` après
  écriture, puis stable durant la session observée.
- Tests `native_config_host.py` + `native_config_codec_host.c` :
  magic/version/checksum, tailles tronquées, quatre valeurs,
  refus d'SD absente, premier démarrage sans fichier,
  relances, corruption, backup et rollback avec mocks SD
  (ces tests ne remplacent pas le test matériel).

### Preuve gameplay après désengagement

- `[CORE] ... exports=3`, `[CONFIGMAP] Native CYD config
  restored earlier; legacy Config v23 skipped`,
  `[MAPPINGS] INSTALLED payload=8376 heap8=154892`,
  menu `framebufferFNV=0f2c9fc3`, logo
  `05aa37e1`, `shapeData=mediaTexels=0x0`.
- LOAD `/DoomRPG-ESP32-slot09.sav` : **v11, 5604 B**,
  `/intro.bsp`/Entrance, `arenaFNV=c3882516`.
  Ressources, états scripts/lignes/caisses, topologie,
  positions et activation restaurés depuis le checkpoint ;
  `MONSTERSTATE ... enemies=30`,
  `MONSTERACT ... activeCount=12`,
  `V9-SPATIAL-STAGE ... exact=yes`,
  `[ENGINESESSION] RESUME-VISIBLE` puis `READY`.
  Aucun moteur desktop Entity n'a été instancié.
- Touch réel : `TURN_LEFT` `128->192`, `TURN_RIGHT`
  `192->128`, `STRAFE_LEFT` `209->241`, `STRAFE_RIGHT`
  `241->242`, `NEXT_WEAPON` `0->1->2`.
  Tour de monstres ordonné sur mouvements.
  `SELECT` sur sprite 242 : `MONSTERCOMBAT` commit
  `hp=6->1`, attaque monstre et
  `[MONSTERRETAL] COMMIT ... playerHP=30->28 armor=20->18`
  après animation ; deuxième attaque tue ce sprite,
  déclenche `[LEVELUP] PRESENT/CLOSE level=1->2`
  avec fermeture par tap. Sprite 241 ensuite tué
  (critical, drop visible, XP), puis `FORWARD 241->209`.
  Cela démontre la continuité des touch actions natives,
  pas une nouvelle validation exhaustive de l'IA.
- **RAM en jeu observée** : `[ALIVE] heap8=113592
  largest8=86004` stable à travers les commandes
  envoyées ; `[RAMBUDGET] ... HEADROOM_OK` ;
  `shapeData==NULL`, `mediaTexels==NULL`.
  Aucun crash, assert, RNG replay diverged ou
  rollback fail relevé **dans cette trace fournie**.
- Les logs de probes qui impriment encore
  `Game_loadConfig still NOT executed` sont une
  **mention textuelle historique** d'une API désormais
  non liée, pas une invocation de la fonction. Leur
  nettoyage est cosmétique, hors de ce checkpoint docs-only.

**État** : jalon **merge-ready** pour la partie native
config/backlight + retrait keybinding, à condition que la
review GitHub n'ouvre pas de nouveau problème de code.
Aucun commit de production nécessaire après ce PASS ;
les commits suivants de clôture sont **docs-only**.
Ne pas merger dans `main` sans demande explicite.
Au prochain jalon, après confirmation du merge,
relire le **vrai main** et repartir de son SHA exact.

---

## PR #217 — correctif de code review P2, écran Options (16 octobre 2026)

**Correctif post-clôture demandé par la review GitHub** :
`e4d0d03844ebfbd431318b8b1e0c9f2936c5d961`
(2 commits de code/test après le HEAD documentaire `909e7e37`).
Le reviewer a identifié une vraie régression : dans
`native_main_menu_options_action.c`, `paintOptionsBounded()`
centrait toujours le logo 102×120 sur x=29 alors que
`MENU_MAIN` le pose en x=0 et que le rail droit débute x=102.
Les 29 pixels de droite du logo pouvaient être écrasés et
une bande noire apparaissait à gauche. Options partage
désormais **exactement** la position du MENU_MAIN via
`DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X`. Le test
`native_ui_doom_polish_host.py` compare les quatre paramètres
`logoDst` des deux renderers et interdit le recentrage.
Les actions, hitboxes, PAK, RAM, gameplay et RNG sont inchangés.

**Validations** : la CI de `7c866696` et celle de la clôture
documentaire `909e7e37` étaient SUCCESS (51 étapes chacune).
Le code review-fix **n'a pas encore été flashé et photographié
sur le CYD** : n'attribuer aucune validation matérielle nouvelle
au SHA `e4d0d038`. La revue/CI du nouveau SHA sont distinctes.
Le présent correctif *déplace* la dernière frontière de code ;
la clôture docs-only ci-dessous doit être lue comme le checkpoint
historique précédant cette review. Le merge de `main` reste
exclusivement à l'utilisateur.

---

## PR #217 — clôture de branche : audio-intent natif + interfaces DOOM (16 octobre 2026)

**Source GitHub** : `main 4434e47a36b70826c401fc17a8b24d371789098c`,
branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
**Dernier SHA de code (firmware à merger)** :
`7c866696f4ce1d263b4a514544d7f83c06146d27`.
[CI de ce SHA : SUCCESS, 51 étapes](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38877111031)
(test hôte, builds `esp32-cyd` normal, bringup et trace, compiledb/ELF).
Ce SHA **n'est pas** le SHA du premier PASS audio :
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc`.

**Couture audio initiale**, conservée depuis le premier PASS CYD :
`esp_legacy_doomcanvas_bridge.c` n'appelle plus `Sound_playSound`
ou `Sound_stopSounds` lors de l'entrée/sortie d'intro ;
`EspNativeAudioIntent_publish(5039, 11, 5)` et
`EspNativeAudioIntent_stopAll()` utilisent l'owner natif existant
de **8 B**. `STOP_ALL` est `resourceId=0, flags=0x80`.
Le backend reste **silencieux**, sans file FIFO garantissant
plusieurs intentions. L'audio audible, le stop à la sortie gameplay
et la politique d'interruption restent **hors périmètre**.

**Reprise UI native sur cette même branche, validée en revue
fonctionnelle par l'utilisateur et autorisée à merger :**

- **MENU_MAIN** : logo PAK original, peint par le chemin SDL natif
  existant, maintenant **étiré volontairement** de la source 108×74
  vers le rectangle logique `x=0, y=0, w=102, h=120` :
  bord haut à zéro, pleine hauteur 160×120, proportions non conservées,
  **aucun cadre** sur la zone gauche. Il ne faut ni réactiver le
  cadre des itérations précédentes ni rétablir l'ajustement proportionnel.
  La zone boutons reste seulement à droite : rail `x=102..159`,
  quatre cartes `x=104..157`, chacune 54×20 logiques,
  premières positions `y=13,38,63,88`, pas de 25.
  Focus/armement rouge DOOM, état initial visuellement neutre,
  **premier tap pour armer, second tap après relâchement pour confirmer**.
  `native_main_menu_160x120_layout.h` centralise les rectangles
  pour painter, hitboxes, Options→Back et retour LOAD `NO SAVE`.
  Repaint des cartes borné au rail ; même dispatch/actions.
- **PASSWORD / terminal** : habillage industriel RGB565 plutôt que
  monochrome, header à deux lignes nettes, suppression des deux
  fragments rouges décoratifs dans le champ de code, touches
  en relief et DEL/VALID différenciés. Seuls les glyphes **DEL**
  et **VALID** sont descendus de 3 px ; les zones 3×4 restent
  identiques. Curseur rectangulaire 5×9 à la prochaine position
  de saisie, clignotement modal toutes les **450 ms**, changement
  de 5×9 pixels du framebuffer sans relire la police/PAK ni
  exécuter de tour ; l'appel à `present()` reste un présent
  normal, **ne pas prétendre à un transfert SPI partiel**.
- Gardes dans `ESP32/test/native_ui_doom_polish_host.py` :
  vraie géométrie C et non-recouvrement logo/rail, dessin sans
  cadre, états menu, chemins NO SAVE et Options Back, en-tête
  terminal, DEL/VALID, curseur et retry cosmétique. CI sans
  régression sur les 3 profils firmware.

**Preuves et limites** : le **premier PASS CYD audio** est établi
par les logs de la section historique ci-dessous (intro → Entrance
→ EXIT, `shapeData=mediaTexels=NULL`). Les captures/retours
CYD des itérations UI antérieures ont motivé les changements ;
le dernier accord utilisateur clôt le choix visuel. **Aucun
nouveau log Serial ni capture explicitement rattaché au SHA
`7c866696` n'est fourni ici** : le rendu pleine hauteur final
n'a pas de mesure matérielle indépendante à déclarer. La CI
atteste compilation/tests, pas pixels physiques ni audio audible.
L'anomalie supposée « zombie sans tour » correspond, dans le
scénario fourni, à un `LEVELUP` après mort du chien :
`MONSTERTURN SKIP levelup=active`, comportement expliqué par
`Player_nextLevel` → dialogue legacy et par le retour à
`ST_PLAYING` sans nouvel `Game_advanceTurn`. Aucun correctif
AI/RNG n'a été introduit ; les cas sans LEVELUP restent à
diagnostiquer séparément.

**Frontières permanentes préservées :** écran 160×120 RGB565 ×2,
pas de nouveau framebuffer ni de cache de texels global, données
runtime depuis PAK, `shapeData == NULL`,
`mediaTexels == NULL`, pas de nouveau code desktop lié,
aucune modification des sauvegardes, RNG ou combats dans cette
passe UI. **À partir du code `7c866696`, clôture docs-only** ;
tout changement de firmware ultérieur exigerait un autre test CYD.
PR prête pour décision de merge par l'utilisateur,
**aucun merge de `main` par l'agent**.

---

## PR #217 — Canvas se désengage de l'ABI audio desktop : CI PASS, CYD PASS ciblé (16 octobre 2026)

**Base main GitHub post-merge #216 :** `4434e47a36b70826c401fc17a8b24d371789098c`.
Branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
**SHA exact du code testé par l'utilisateur en firmware normal `esp32-cyd` :**
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc`.
La [CI push #38824713309](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38824713309)
est **SUCCESS** (test hôte, build normal/bringup/trace, gardes
compiledb/ELF). Il s'agit d'un nouveau jalon autonome, et non d'un
retest du scheduler de combat #216.

**Frontière retirée :** dans `esp_legacy_doomcanvas_bridge.c`,
l'entrée `ST_INTRO` publie maintenant l'intention native
`EspNativeAudioIntent_publish(5039, LOOP|STOPSOUNDS|ISMUSIC, 5)`
sans `Sound_playSound`. Les deux sites de l'ancien `Sound_stopSounds`
(transition quittant le menu et rollback d'intro) publient une
commande `EspNativeAudioIntent_stopAll()`. Cette commande emploie
le record 8 B préexistant, avec `resourceId=0` et flag
`ESP_NATIVE_AUDIO_STOP_ALL_FLAG=0x80` ; la séquence reste ordonnée.
Le test `ESP32/test/native_audio_handoff_host.py` compile/exécute
le vrai owner, prouve le layout de 8 B, PLAY→STOP→PLAY, reset et refus
d'un PLAY de ressource zéro ; il interdit statiquement les appels
Sound depuis le pont. **Il n'y a toujours ni backend sonore physique
ni file garantissant de conserver plusieurs intentions : `lastIntent`
est un seul enregistrement.** Les stubs `Sound_*` résiduels ne sont
pas retirés dans cette PR.

**Preuve CYD sur les logs utilisateur :** `START GAME`, préparation
de l'intro PAK (4 BMP, 33 236 B de source), `[AUDIOINTENT] STOP_ALL
seq=5` puis `PUBLISH seq=6 resource=5039 flags=11 priority=5
backend=silent`. Sept taps conduisent de l'intro à `FINAL-CONTINUE`,
puis `INTRODISP READY` libère 34 056 B depuis `heap8=124200`
jusqu'à `158256`, sans mutation du framebuffer conservé.
`/intro.bsp` est chargé avec `sourceCRC=623f34e4` et
`arenaFNV=c3882516`, `ENGINESESSION FRESH-VISIBLE` et
`RESIDENTGAMEPLAY READY` atteints, `shapeData=mediaTexels=NULL`.
En jeu, les témoins `[ALIVE]` donnent `heap8=112576,
largest8=86004` constants. HUB→SYS→EXIT à double sélection :
`saveWrite=no`, `[RESIDENTRESET] released=18008 empty=1`,
retour MENU_MAIN avec `heap8=158256 largest8=110580`
et frame `522dc605`, sans échec signalé. Le menu HUB et l'EXIT
n'avancent pas le tour. Ces chiffres sont les mesures de ce
parcours, pas un benchmark comparatif avec #216.

**Limites matérielles/observations ouvertes :** aucun événement
`STOP_ALL` n'est visible **au retour gameplay→menu** dans cette
trace ; ce chemin natif n'atteste donc pas l'appel à
`DoomCanvas_setState(ST_MENU)`. Ne pas déclarer l'arrêt audio EXIT
physiquement validé. Aucune lecture audio, conservation FIFO des
plusieurs intents, parité auditive, SAVE→LOAD avec ce firmware,
combat/RNG ou validation visuelle pixel à pixel ne sont prouvés par
ces logs. La dette de cadence DAMAGECUE de #216 reste indépendante.
**Après ce premier SHA audio testé, trois commits de documentation audio étaient docs-only ; les itérations visuelles ultérieures constituent un second lot de code explicitement borné et tracé ci-dessus.**
Ne pas merger dans `main` automatiquement ; nouveau code = nouveau
jalon + nouveau flash CYD.

---

## PR #216 — racine DoomRPG C native et retour d'impact coopératif : CI PASS, acceptation CYD ciblée (16 octobre 2026)

**Base GitHub réelle** : `main 0ca15d756c2c6a64a5b4bc6b3e2b0ec89d83b7bd` ;
branche `agent/esp32-own-config-keybinds`, [PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216).
**Dernier commit de code effectivement chargé et joué sur le classic CYD normal
`esp32-cyd` : `3361cdd1d4208f69c9d9eb12c41207b9016f1ac4`.**
Ce nouveau témoignage complète le lot Config (déjà clôturé ci-dessous).
La racine process native est validée pour le boot/reprise et ce parcours de combat
sur la vraie carte ; il ne contient **pas** de test d'EXIT jusqu'au menu,
de lecture positive de `Config`, ni de cycle nouveau SAVE→LOAD avec le firmware
présent. Ne pas attribuer ces preuves absentes à ce SHA.

**Ownership natif / retraite desktop :** `engine_stubs.c` détient désormais
`doomRpg`, `DoomRPG_freeMemory`, `DoomRPG_FreeAppData` et
`DoomRPG_Error`. L'outillage peut encore *générer* une copie C pour
vérification, mais elle n'est compilée ni liée dans aucun des trois profils :
`DoomRPG.c`, `Game.c`, `Render.c` et `DoomCanvas.c` desktop
sont tous exclus du firmware. Le test hôte de cleanup exerce
4 096 configurations et les gardes compiledb/ELF vérifient la propriété
native/process et l'absence de définitions en double. Cela ne retire
pas tous les adaptateurs historiques ni tous les comportements legacy.

**Symptôme et résolution des retours de dégâts :** sur l'Imp/monstre distant
`sprite=157 subtype=0`, le rectangle rouge apparaissait trop tôt, puis la
bande directionnelle clignotait tardivement. La première impulsion tenait
**784 ms** au lieu de 85 ms nominaux en présence d'un long traitement
synchrone. L'owner de feedback mesure maintenant les temps physiques
`onMs/gapMs`. Le `DAMAGE` non létal groupe le texte, le contour 2 px et
la première impulsion dans **une même présentation physique après COMMIT**,
avec début de lease mesuré au présent. Les ripostes létales et dommages
pièges/tonneaux conservent leurs chemins distincts ; échec de présent,
FIFO pleine, barrières tactiles/PAK, timeout et restauration bornée restent
testés. Aucun changement au calcul de dégâts/RNG/SAVE.

**Frontière d'animation native :** la pose finale immobile doit être
**présentée avec succès** avant le COMMIT, mais elle ne bloque plus 500 ms
de plus. `MONSTERATKVIS STEP phase=final-idle phaseMs=0`, puis
`COMPLETE finalIdleHoldMs=0` et `MONSTERRETAL COMMIT`. Pour les trois
tirs, les intervalles intermédiaires sont conservés ; le dernier projectile
204 doit être effacé physiquement, avec retry en cas d'échec, avant
résolution. Touch snapshot et RNG guard restent fail-closed.

**Séquenceur coopératif :** `EspNativeGameplayMonsterActiveSequence_service`
conserve préfixe d'activation, ordinal, compteur privé et pause sur probe
de riposte ; il traite au plus **un membre vivant complet par appel** puis
rend la main au service UI. La mutation d'un membre n'est jamais interrompue.
`EspNativeGameplayMonsterActiveSequence_isBusy()` ferme le toucher et
l'admission d'inputs en attente jusqu'à fin du tour. Pas de délai ajouté,
de nouvelle tâche FreeRTOS ou allocation. Les tests hôtes de budget
sont des tests de contrat/politique ; ils ne prouvent pas à eux seuls
l'ordonnancement réel. Le vrai CYD atteste néanmoins
`DAMAGECUE ON` **avant** `MONSTERACTIVESEQ COMPLETE` pour
`SELECT_DOOR` et `PASS_TURN`, contrairement à l'ancien blocage.

**Témoignage matériel utilisateur sur le code `3361cdd1` :**
boot gameplay et cache prêts, porte `line=396` ouverte par événement 22,
dix monstres actifs, sprite 157 armé et riposte réelle :
probe 1 `HP 30→26`, armure `20→17`, RNG
`2c65f990→1ceab6c3`, texte `7 damage!` ; probe 2
`HP 26→22`, armure `17→15`, texte `6 damage!`,
`rngRollback=yes` en préflight, `playerExact=yes`,
`rngExact=yes` lors de l'animation. Les `[ALIVE]` maintiennent
`heap8=112320`, `largest8=86004` avant/après, sans incident de
collision, RNG ou de présentation signalé. Retour visuel utilisateur :
« C'est vraiment pas mal là », **acceptation ciblée de l'amélioration**,
pas preuve d'une animation parfaitement régulière.

**Anomalie résiduelle ouverte — NE PAS masquer :** sous cette charge
coopérative, deux `[DAMAGECUE] DONE` donnent
`onMs=272/120 gapMs=281 delayed=yes` et
`onMs=321/120 gapMs=237 delayed=yes`, pour un nominal
`85/85` et `gap=45`. Les flashs expirent physiquement à
`675` et `681 ms` pour `targetMs=500` ; ce n'est ni
`delayed=no`, ni une stabilité de cadence validée. La
préemption cooperative entre **membres** n'offre pas de deadline UI
lorsqu'un membre, un rendu ou une présentation bloque la boucle ;
résoudre ce point dans un **jalon séparé de scheduling/presentation**,
sans toucher gratuitement à la parité RNG, aux tours ou aux visuels
déjà validés. Les tables `monsterPhaseMs` (200–500 ms),
cadences intermédiaires des trois tirs, lancement projectile 150 ms
et étapes 65 ms restent à auditer par famille, pas à remplacer par zéro.

**CI sur SHA code exact :**
[push 38822809348 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822809348),
[PR 38822814390 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822814390).
Tests de vrai compositeur `damage_cue_timing_host.c`,
machine d'états réelle `monster_attack_visual_timeline_host.c`,
contrat budget `monster_active_sequence_budget_host.c`,
config/root/clock/RNG et audits ELF ; build
**normal RAM statique 50 984 B / Flash 809 997 B**,
trace **51 032/826 605 B**, bringup **51 136/830 649 B**.
Ces chiffres statiques ne sont pas le heap mesuré sur le CYD.

**Décision de clôture :** ce lot fonctionnel est accepté par l'utilisateur
pour revue/merge **avec latence d'overlay connue et documentée**, non
comme correction exhaustive des timings. Les commits après ce témoin
matériel doivent être exclusivement **docs-only** ; nouvelle correction
du scheduler/timing, et éventuellement vérification dédiée d'EXIT/LOAD,
sur branche/milestone séparé avec nouveau PASS physique. Ne jamais merger
`main` automatiquement.

---


## PR #216 — Config key bindings natifs, CI + CYD PASS de non-régression (15 octobre 2026)

Base réelle : `main 0ca15d756c2c6a64a5b4bc6b3e2b0ec89d83b7bd`.
Branche `agent/esp32-own-config-keybinds` ; **code firmware matériel
testé `8436de9caa83c8b3efd1d3d39fbc340c5293c89f`** ;
commit `7f943508395b375514889c7895072dcd86df12de`
corrige **uniquement le garde CI ELF** (tentative COMMON de GCC Xtensa
dans l'objet → BSS finale), sans modifier le firmware.
[PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216).

Trois variables à vie process `keyMapping[12]`, `keyMappingTemp[12]`
et `keyMappingDefault[12]` (3 × 528 B) ainsi que
`DoomRPG_setDefaultBinds` sont sorties du `DoomRPG.c` généré.
L'owner permanent existant `esp_legacy_config_mappings_startup.c`
garde le tableau et le corps de reset **strictement identiques**
au legacy. Le générateur pinne les CRC `640ab977` (table) et
`5c83b22a` (fonction), vérifie corps/valeurs et retire leurs
définitions du généré. Aucun nouveau TU de production, aucune
nouvelle allocation, aucun changement de format de sauvegarde,
RNG, input tactile, world ou render.

**CI hôte + 3 profils PASS** :
[push #38747810890](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747810890)
et [PR #38747821709](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747821709).
Test compilé : 8 192 séquences de mutation/configuration et reset,
copie des 2 jeux de binds, parité différentielle et 1 584 B de
storage, pas la lecture d'un fichier `Config` réel.
Audit compiledb/ELF trois profils : deux tableaux BSS et un
tableau DATA, une fonction reset, toutes dans l'owner natif ;
zéro doublon/résidu généré. RAM statique/Flash :
normal **50 976/809 517 B**, bringup **51 120/829 997 B**,
trace **51 016/825 953 B**, égales aux budgets du précédent
jalon. Ce n'est pas une mesure de RAM dynamique CYD.

**CYD normal — PASS matériel ciblé de non-régression sur les
logs utilisateur** : boot `[CONFIGMAP] READY`, menu
`frame=522dc605`, `Config file present=no` puis
`loadConfig: (unable to open file)` attendu. Depuis HUB,
LOAD slot07 V11 /intro.bsp : CRC `623f34e4`,
arena `c3882516`, 30 monstres `8acca67c`,
topologie `ebca87dd`, positions `24bbd504`,
activation `6698ed80` (22 actifs), RNG au restore = 0.
Mouvements axiaux + strafe + rotation, porte160
ouverte puis fermée après le tour, activation des sprites
6/7, attaque/riposte, `rngRollback=yes` et
`rngExact=yes`, effets sang/gib, ramassage d'Armor Shard,
changement d'armes ; aucun échec nouveau remonté.
EXIT par double sélection, `saveWrite=no`,
`[RESIDENTRESET] released=18008 empty=1`, retour au menu
`522dc605`. `shapeData=mediaTexels=0x0` au retour.
Snapshots ALIVE gameplay `heap8=112328 largest8=86004`
stables, puis retour menu `158264/110580`.
Ces logs prouvent les frames `presented=1`, **pas**
une validation visuelle exhaustive de tous les pixels.

**Limites** : `Config` absent sur la SD ; pas de validation
matérielle de la lecture positive des binds configurés.
La parité des defaults/reset est logicielle.
Ce PASS valide le **premier lot de code**, pas les prochains
commits firmware sur la même branche. La clôture présente est
**docs-only après hardware PASS** ; toute nouvelle frontière
de code nécessite une **nouvelle validation CYD**. Pas de merge
automatique.


## Décorations murales visibles en oblique — CI + CYD PASS (15 octobre 2026)

Après le PASS `BOUNDARY-INLINE` (code `a71ed3c4`), l'utilisateur
a signalé une **régression visuelle restante** dans la dernière
pièce d'Entrance : des motifs sur un mur rouge disparaissaient selon
l'angle d'observation et réapparaissaient de face. Ce symptôme a
bloqué la PR #215 et conduit à un second correctif distinct,
**code firmware matériel testé `a088b485f3891d515537ae2961a27e0d0a40e5eb`**.

Comparaison avec le vrai `src/Render.c` : les sprites marqués
`0x00800000` sont décalés visuellement d'une unité devant leur
mur suivant les flags directionnels (N/S/E/O). Le renderer native
négligeait ce décalage et utilisait un tri de profondeur générique ;
la priorité legacy des sprites muraux (`INT32_MAX`) et le cas
spécial des IDs 184..190 (`sortZ=0`) manquaient.
`ESP32/include/esp_native_sprite_visual_legacy.h` contient ces
règles de projection en fonctions pures ; le code natif les emploie
pour le tri, le placement à l'écran et la recherche de feuille BSP
(`esp_native_sprite_renderer.c`,
`esp_native_bsp_visibility.c`).
**Aucune modification** des positions du BSP immutable, du
save/checkpoint, de la topologie des monstres, des assets ni du RNG ;
pas de réintroduction de `shapeData` ou `mediaTexels`.

**CI code exact** :
[push #38734975334 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734975334),
[PR #38734982820 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734982820).
`ESP32/test/wall_sprite_visual_host.py` compare les règles avec
le legacy et compile leur implémentation permanente : **25 920
cas**, quatre directions, priorités murales et IDs 184..190.
Les trois firmwares compilent : profil normal RAM statique
**50 976 B**, Flash **809 517 B** ; bringup **51 120/829 997 B** ;
trace **51 016/825 953 B**.

**CYD normal — PASS visuel rapporté par l'utilisateur :**
« les murs sont bien dessinés et leurs motifs ne disparaissent pas »,
y compris dans les vues obliques de la pièce initialement en défaut.
Trace fournie : déplacements/strafe/rotations avec preview
`rendered=yes presented=1`, ouverture de la porte 160 et
fermeture différée après le tour des monstres, activation des
sprites 6 et 7, combat et ripostes différées, FX/gibs,
drop dynamique puis collecte de munition et d'armure,
`rngRollback=yes` dans les probes et `rngExact=yes`
sur les animations d'attaque. `heap8=112328` et
`largest8=86004` constants sur les lignes `ALIVE`
observées. Pas de `NATIVEFRAME FAILED` ni anomalie nouvelle
rapportée dans cette trace.

**Portée exacte :** restitution visuelle du mur problématique
validée par l'utilisateur, pas une comparaison pixel à pixel
de toutes les cartes contre le desktop. Le premier PASS
`BOUNDARY-INLINE` reste attribué au code `a71ed3c4` ;
les nouveaux logs ne reproduisent pas explicitement le cas
packed-index 2048. Le second PASS concerne le nouveau code
`a088b485f3891d515537ae2961a27e0d0a40e5eb` et son rendu visuel. Après ce test,
**commit(s) exclusivement docs-only**, sans modification de
firmware ; PR #215 merge-ready après deux checks verts.



## Renderer mural compact : frontiere 2048 resolue inline — CI + PASS materiel cible (15 octobre 2026)

**Base post-merge PR #214 :** `main c5384c7664f062a64b0e7ed3c928b2dcde8cd89b`.
Branche `agent/esp32-wall-boundary-inline` ; **firmware reel teste :
`a71ed3c4cb0b12e2a0ad6ab2e06292637bbb39d5`** ; [PR #215](https://github.com/Kharn27/DoomRPG-RE/pull/215).
Le sampler d'`esp_native_first_frame.c` peut encore produire un indice
2048 pour une texture murale 64x64 / 2048 B. La lecture contigue
historique utilisait un octet de la texture successeur, pas un clamp
sur 2047. Avant : `SPAN_OOB -> LEGACY_GUARD -> RETRY -> RECOVERED`,
avec abandon puis second rendu complet. Maintenant, quand
`packedIndex == 2048`, `prepareLegacyWallGuard(work, source)`
retrouve **le meme octet exact** dans mappings.bin/wtexels.bin
pendant la lease PAK deja ouverte, puis le span continue **sans
relancer le rendu**. Ownership BSS strict : 4 gardes au maximum,
16 B chacun, soit 64 B ; indice >2048, mapping absent/incoherent,
PACK ferme ou gardes epuisees : `SPAN_OOB` fail-closed. La fonction
de rendu n'a plus sa boucle de retry. Aucun `mediaTexels` / gros
buffer monolithique reinstalle.

**CI hote + compilations :** [push #38728936980 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38728936980),
[PR #38729192919 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38729192919).
`ESP32/test/native_wall_boundary_inline_host.py` extrait et compile
les vraies fonctions `prepareLegacyWallGuard` et
`sampleWallSpan` ; 2000 cas deterministes de nibble/pixel,
octet successeur reference 68 depuis offset 32768, source actual40
offset 20480 ; reuse sans lecture PAK supplementaire, violations
>=2049, garde pleine, mapping invalide et PACK ferme restent
bloquees sans peindre. Profil normal statique RAM **50976 B**,
Flash **809333 B** ; bringup **51120 / 829809 B** ; trace
**51016 / 825773 B**. Il s'agit d'un test de span, non de
parite de l'image complete.

**Preuve materielle utilisateur, firmware normal CYD :
PASS cible pour le cas reproduit.** Menu Load Game, slot07
V11 5604 B ; `/intro.bsp` CRC `623f34e4` et arena
`c3882516` ; restore exact monstres `8acca67c`,
topologie `ebca87dd`, positions `24bbd504`,
activation `6698ed80` (22 actifs). Trajet
`STRAFE_LEFT tile=310->309`, puis second passage
`STRAFE_RIGHT tile=308->309` : marqueurs recurrents
`[NATIVEFRAME] BOUNDARY-INLINE logical=15 actual=40
source=20480 successorActual=68 successorSource=32768 byte=11
owner=BSS bytes=16`. Aucun `RETRY`, `RECOVERED`,
`SPAN_OOB` non recupere ni `NATIVEFRAME FAILED` dans
les logs communiques. Animation porte line160 ouverte/fermee
(y compris fermeture differee apres tour), mouvement/rotation,
combat ennemis sprites6/7, riposte differee,
`rngRollback=yes`, `rngExact=yes`, critiques/gib,
drop `MONSTERDROP COMMIT`, pickup et PASS_TURN fonctionnels.
`[ALIVE] heap8=112328 largest8=86004` stables pendant
la session ; `shapeData=0x0 mediaTexels=0x0`.
Scratch rendu transitoire observe 2980 B contre 2960 B dans
le jalon precedent, sans baisse de la RAM libre residente observee.

**Limites :** le PASS concerne la frontiere effective index2048,
pas une preuve de parite pixel pour toutes les vues/textures,
ni un benchmark permettant d'affirmer un gain de FPS ou de temps.
Les incidents historiques figurant dans les milestones precedents
restent de vrais temoins du probleme *avant ce correctif* ;
ne pas les requalifier retrospectivement. Post-test : commits
documentation uniquement ; aucune nouvelle modification firmware
sans revalidation CYD. Jalon pret pour revue et merge manuel.



## Lecteurs de configuration binaires natifs — CI + CYD PASS de non-régression (15 octobre 2026)

Suite sur la **même branche** après le PASS process clock/RNG.
Code firmware testé : `eaa915aacf5171dbd118758ab33bb402f9f9edee`.
Les corps legacy **strictement inchangés** `File_readByte` et
`File_readInt` ont quitté la copie générée de `DoomRPG.c`
pour `ESP32/src/esp_doomrpg_config_read.c`. La comparaison
source CRC + corps est exigée par le générateur. Aucun changement
de l'ABI SDL_RWops, du `char` signé, du little-endian `SDL_SwapLE32`,
de `Game_loadConfig`, du RNG ni du format SAVE.
Le fichier source desktop reste la référence.

[CI #38717722550 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38717722550) :
hôte différentiel **4 096 appels, 8 194 octets consommés**,
audit ELF 2 définitions propriétaires, 0 dans `DoomRPG.o` généré,
builds normal, trace et bringup. Profil normal :
**50 976 B RAM statique, 809 561 B Flash** ;
bringup **51 120/830 089 B**, trace **51 016/826 041 B**.

**CYD réel normal : PASS d'intégration et de non-régression.**
Boot ESP32-D0WD-V3 sans PSRAM, PAK 241 entrées, startup complet,
CONFIGMAP READY et menu `522dc605`. LOAD depuis HUB du slot07
V11 (5 604 B) avec runtime arena `c3882516` et restauration
exacte des empreintes monstres `8acca67c`, topologie
`ebca87dd`, positions `24bbd504`, activation `6698ed80`
(22 actifs) sans consommation RNG au restore.
Session gameplay prête, `shapeData=mediaTexels=NULL`, mouvements,
ouverture de porte 160, activation monstres 6 et 7,
attaques/ripostes/commits morts, `rngRollback=yes` et
`rngExact=yes`. `heap8=112328`, `largest8=86004`
stables pendant les actions observées, pas de reboot ni anomalie
nouvelle dans les logs communiqués.

**Limite matérielle explicite :** `[CONFIG] Config file present=no`
et `loadConfig: (unable to open file)`. Ces deux fonctions
**n'ont pas été exercées par un vrai fichier Config sur CYD**
dans cette trace ; leur comportement de lecture est validé par
l'exécution hôte comparative, les définitions/liaisons par ELF,
et le matériel ne prouve ici que la non-régression en mode
configuration absente. Ne pas écrire « Config présent validé ».

Reste ouvert indépendamment :
`NATIVEFRAME LEGACY_GUARD -> SPAN_OOB -> RECOVERED`,
notamment ligne 161 pendant déplacement ; le cache mural
récupère par retry, sa cause n'a pas été corrigée.
**Clôture du lot de lecteurs : modifications post-test docs-only ;
aucun nouveau firmware. Branche prête pour merge après revue,
sans merge automatique dans main.**


## Extraction du RNG process natif : PASS matériel ciblé (15 octobre 2026)

Code firmware testé `a7968169a0af2e2ad93322214547089102b2d292`
sur `agent/esp32-extract-doomrpg-process-clock`, base main
`e99286b9dc1c3a120f24f76ed3741ee22cc6be0c`.
Les corps originaux `Random_SetSeed`, `Random_Get`,
`Random_getRand`, `DoomRPG_setRand` sont dans
`esp_doomrpg_process_rng.c`. Etat statique `_seed/resetRand` unique
et process-lifetime ; les wrappers RNG byte/int et replay guard
restent inchangés. Contrat source CRC/égalité des corps et audit ELF
sur les trois profils, zéro définition restante dans le généré.

[CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38714139429) :
test hôte différentiel 12 000 opérations par scenario sur 4 ticks
initiaux, tests reservation/rollback/replay byte et word, builds
normaux/trace/bringup. Normal : RAM statique 50976 B, Flash 809565 B.

**CYD firmware normal PASS de non-régression** : MENU_MAIN LOAD
slot07 V11, restauration exacte MAP_INTRO CRC `623f34e4`,
monstres `8acca67c`, topologie `ebca87dd`, positions `24bbd504`,
activation `6698ed80` (22 actifs), `rngCalls=0` au restore.
Plusieurs centaines d'actions, combat et riposte, PASS_TURN, portes,
pickups, SAVE slot06, evenement SHOW-batch 74, fin Entrance
secrets 6/6 et monstres 30/30. Transition reelle vers Junction,
CRC `4a2c5800`, map flash rebuild verified, arena `bc432a0f`;
`shapeData=mediaTexels=NULL`. Heap8 112328 en debut de
session, 109552 en fin d'Entrance, 121716 en Junction ;
largest8=86004 sur ces snapshots.

**Temoin reel RNG limite de table** :
`MONSTERMOVERNG ARM next=127->0 prepared=1 reservation=persistent`
puis `MONSTERMOVERNG COMMIT reservation=consumed-by-live-move
randomLive=advanced-exactly`. Autre combat : trois coups de
sprite286 subtype2, `rngExact=yes` durant animation, resolution
differee apres final-idle. Aucun marqueur RNG divergence ou mismatch
dans les extraits fournis. Ceci ne demontre pas toutes les
sequences RNG possibles ; aucune mesure de perf revendiquee.

Les nombreux `NATIVEFRAME LEGACY_GUARD / SPAN_OOB / RECOVERED`
restent un incident de rendu **ouvert** ; il ne faut pas confondre
recovery et correction. La cloture apres ce PASS ne modifie que
la documentation. Les commits de code suivants doivent etre
valides a nouveau physiquement.


## DoomRPG process clock — CI + vrai CYD PASS ciblé (2026-10-15)

Base après merge PR #213 : `main e99286b9dc1c3a120f24f76ed3741ee22cc6be0c`.
Branche `agent/esp32-extract-doomrpg-process-clock`, code testé
`d757502976f3eaaa37e5bf28bc8dd63c7e9c002a`. Deux fonctions
**réellement liées** `DoomRPG_GetTimeMS` / `DoomRPG_GetUpTimeMS`
et leur `static Uint32 basetime=0` sont déplacées du `DoomRPG.c`
généré vers `src/esp_doomrpg_process_clock.c`. Corps **inchangés**
(CRC source, égalité native/legacy), état **process-lifetime**, sentinelle
`basetime==0`, ticks SDL et soustraction Uint32 conservés. Le
générateur retire les corps + le global de la copie ; le source desktop
de référence reste inchangé. **Aucun reset session / LOAD** ni
couplage au RNG; ce n'est pas une correction de la cadence de jeu.

[CI SUCCESS #38711767983](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38711767983)
sur le **SHA code testé**, trois profils + host différentiel (10 000
étapes mixed-order, zéro, overflow) + audit compiledb/ELF : deux
définitions dans l'owner permanent, zéro dans le généré et un seul
`basetime` en BSS native. RAM statique / Flash rapportées par CI :
normal **50 976 / 809 565 B**, trace **51 016 / 826 041 B**,
bringup **51 120 / 830 089 B**. Pas de gain Flash/heap/FPS revendiqué.

**Validation physique utilisateur, firmware normal : PASS ciblé.**
MENU_MAIN → START, intro texte/animation/skip/final, disposal
`recovered=34056` ; MAP_INTRO source CRC `623f34e4`, runtime
`c3882516`, first frame `71ca7465`, cache cold `a9b263f5` /
warm `20c09fe4`. LOAD V11 slot07 à `1632,992 / angle192` :
monstres `966ac091`, topologie `14ce1a4e`, positions
`eb9553d2`, activation `aca2058f` restitués exactement,
`activeCount=21`. Test tactile réel : rotations et mouvements
axial/strafe, portes 102/113/152 avec fermeture différée,
dialogues paginés events 33/19 et resume opcode 19/11, pickup
Health Vial, monstre `sprite=5 subtype4` activé à l'ouverture,
attaque trois coups et COMMIT de dégâts/riposte, extincteur
sur sprite5 HP 5→0, XP/gib, HUD/HUB puis double confirmation SYS EXIT.
Le SAVE existant n'a **pas** été réécrit (`saveWrite=no`).
Sur les snapshots `[ALIVE]` : heap8 en gameplay fresh
`112328`, après reprise/dialogues `111184`, largest8
`86004`; après EXIT : `[RESIDENTRESET] released=18008 empty=1`,
`MENU_MAIN frame=522dc605`, heap8 `158064` largest8 `110580`.
`shapeData=0x0 mediaTexels=0x0` maintenus.

**Limites :** ni mesure de temps absolu/overflow à 49,7 jours sur
matériel, ni preuve d'égalité des pixels/latences ; logs ne donnent
pas de hash de l'image effectivement flashée. Anomalie préexistante
`[NATIVEFRAME] LEGACY_GUARD→SPAN_OOB→RECOVERED` observée plusieurs
fois : **ouverte**, récupération bornée ≠ correction. L'incident
historique de monstres apparus par trigger après LOAD n'est pas
résolu par ce test. Cette clôture est **docs-only** après PASS ;
la suite RNG sera un **autre commit de code** et exigera un **nouveau
PASS matériel** avant toute déclaration merge-ready.


## DoomRPG — extraction de 7 primitives SDL/clip de la TU générée : CYD PASS ciblé (15 octobre 2026)

Base exacte `main 3dde62507cee7cc335423617759502c217d67651`; branche
`agent/esp32-extract-doomrpg-presentation-sdl`; **dernier commit de code
testé `1840009295909088f97d94b806320585ef832e41`**.
Selon l'inventaire **des symboles réellement liés** du firmware, les
sept définitions `DoomRPG_setClipFalse`, `DoomRPG_setClipTrue`,
`DoomRPG_setColor`, `DoomRPG_setFontColor`,
`DoomRPG_fillRect`, `DoomRPG_drawLine` et
`DoomRPG_flushGraphics` sont sorties du `DoomRPG.c` généré
pour rejoindre `ESP32/src/esp_doomrpg_presentation.c`.
`drawRect` et `drawCircle` ne faisaient **pas** partie de ces
sept symboles liés et n'ont pas été extraits. Le générateur compare
CRC/corps adaptés et échoue sur dérive ; audit ELF : **7 définitions
natives, 0 dans DoomRPG.o généré, 0 unité desktop d'origine liée**.
SDL/clip/couleur/rendu et framebuffer logique partagé **160×120 RGB565**
conservés ; aucun framebuffer secondaire, retour ZIP, `shapeData` ni
`mediaTexels`. Le `DoomRPG.c` généré reste compilé pour le reste
de son périmètre : son retrait total **n'est pas** revendiqué.

**CI du code : SUCCESS**, [run 38693375415](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38693375415),
avec test hôte [`doomrpg_presentation_host.py`](test/doomrpg_presentation_host.py)
(1 600 opérations mixtes, équivalence des corps, raster/clip/RGB565
simulés, sentinelles, couleur/font/present) et trois profils
PlatformIO + audits compiledb/ELF. Mesures **RAM statique / Flash** :
normal `esp32-cyd` **50 976 / 809 565 B** ; trace **51 024 /
826 041 B** ; bringup **51 128 / 830 089 B**.
Ce sont des métriques CI, sans gain FPS/latence revendiqué ; le
raster hôte simulé n'est pas une mesure de la dalle physique.

**Validation utilisateur sur le vrai CYD, firmware normal : PASS ciblé
de non-régression visible**, avec mouvement et rotation animés,
dialogues paginés/continuation, portes animées et événement mixte
`event=74` (six SHOW), ramassage de la clé jaune, HUD/HUB,
combat chiens/zombies/Troop, attaques et ripostes multi-animations,
chaîne de trois barils `283→276+303` et SAVE/LOAD V11.
Après ouverture de la porte `line=87`, une activation
`[MONSTERACT] ACTIVE sprite=10 ... activeCount=21` est observée.
Sauvegarde `slot07` **dos au monstre**, `pos=1632,992 angle=192` :
`monsters=30/966ac091`,
`topology=43/14ce1a4e`, `positions=30/eb9553d2`,
`activation=21/aca2058f`. LOAD restitue ces **quatre FNV exactement**,
`[MONSTERACT] RESTORE activeCount=21 orderCount=21
stateFNV=aca2058f`, puis sans tourner la caméra,
premier `PASS_TURN` : `sprite=10` attaque depuis **BACK** ;
`HP 34→31, armure 20→18`, `DAMAGECUE side=BACK`.
La sauvegarde ne prouve pas à elle seule que `sprite=10` a été
**créé** par SHOW : la ligne d'activation indique
`source=bsp-render-visible`. Cet essai valide la persistance de
l'activation de **ce** monstre, sans résoudre le précédent incident
spécifique à des monstres effectivement apparus par trigger.
`shapeData=0x0 mediaTexels=0x0` après LOAD, `heap8=112128`
et `largest8=86004` stables pendant les tours montrés.

**Restent ouverts, hors périmètre** : cause des répétitions
`[NATIVEFRAME] LEGACY_GUARD → SPAN_OOB → RECOVERED`
(vues pendant ce playtest ; recovery bornée, pas correction) ;
investigation de l'ancienne activation post-trigger après LOAD ;
rendu, son et autres fonctions desktop encore différés ;
preuve de parité pixel-à-pixel sur TFT ou gain de performance non établie.
Les modifications post-PASS de ce jalon sont **documentation uniquement**.


## Consolidation DoomRPG : propriétaire natif images / PAK — CI et CYD PASS ciblé (15 octobre 2026)

Suite **sur la même branche** `agent/esp32-extract-doomrpg-pure-primitives`,
depuis le `main bb5b599fbc56c39bd5ddf4a48bc4bd2cfb7cad0c` ;
lot 1 déjà accepté (six primitives mathématiques/binaires), lot 2
code **`2d09b91b8eedfd9083da08eede01d5399cdf88d3`**, correction de la
fixture hôte **`e648f2fc89c9047f3d73d47122c7bd963a63db2e`**.
Les trois fonctions **réellement liées**
`DoomRPG_createImage`, `DoomRPG_freeImage` et
`DoomRPG_fileOpenRead` résident maintenant dans
`src/esp_doomrpg_image_resources.c` ; le générateur élimine
**leurs trois définitions** du `DoomRPG.c` hérité. Il impose des CRC
des corps après adaptation ZIP→PAK / BMP→indexed et l'égalité textuelle
exacte des méthodes de l'owner permanent. `src/DoomRPG.c` desktop reste
une spécification intacte ; **le runtime n'utilise pas le ZIP**.
Aucune modification intentionnelle de comportement ou de lifetime :
palette quantifiée RGB565, transparence, ownership texture/surface,
`imageMemory`, allocations bornées par asset, lecture alloc PAK et
retour de buffer restent les mêmes. Berserk, dessin/clip SDL, RNG,
horloges, structs et restauration du monde non concernés.

La suite CI du **HEAD `e648f2fc`** est **SUCCESS** :
[push 38648524728](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648524728),
[PR 38648529981](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648529981).
Le premier run de la nouvelle fixture échouait **au lien hôte** faute
de stub `SDL_GetError` ; correction dans la fixture seule, aucune
correction firmware nécessaire. Le test hôte valide l'owner réel avec
I/O PAK, BMP et SDL **mockés** (palette, masque, conversion, dimensions,
`imageMemory`, libération et path slash). Les tests existants,
les trois builds et les gardes ELF/compiledb PASS. Mesures CI
**RAM statique / Flash** : normal **50 976 / 809 565 B**, trace
**51 024 / 826 041 B**, bringup **51 128 / 830 089 B**.
Par rapport au lot 1 : RAM statique inchangée, Flash normale -4 B ;
ne pas extrapoler à un gain FPS, une parité ELF globale ou un gain heap.
Le message `Wrote 809936 bytes` d'esptool mesure l'image écrite,
**pas** la colonne Flash utilisée de PlatformIO.

**Test matériel de l'utilisateur — PASS ciblé sur la nouvelle tête de
branche (sans SHA binaire imprimé par le firmware).**
Boot : CYD ESP32-D0WD-V3, **PSRAM 0**, framebuffer logique
160×120 RGB565/38 400 B, ILI9341 ×2, SD montée, **PAK 241 entrées** ;
`[BMP] decode 144x72 4-bpp ... out=5184` puis
`[SDL] Adopt packed indexed texture`, et image `j.bmp`
`108x74 4-bpp out=3996` avec
`[MENUSTORAGE] STARTUP READY`.
`[MAINOPAQUE] READY`, `logoFNV=0ac1f9c6` (conforme à la capture
précédente). Tactile MAIN→LOAD, slot 07 confirmé :
`[NATIVESAVE] LOAD ... version=11 bytes=5604 map=1`,
`/intro.bsp` CRC `623f34e4`, états native scripts/lignes,
topologie, positions et activation annoncés restaurés ;
`[ENGINESESSION] RESUME-VISIBLE` puis READY,
`shapeData=0x0 mediaTexels=0x0`. Déplacement affiché
`tile=552->520` (commit unique), porte jammed `line=413`
ouverte ; monstre `sprite=106` activé depuis rendu BSP, frappé
au pistolet jusqu'à **HP 7→5→3→0**, death settle, drop, corpse,
contre-attaques **joueur 27→23→18→13**, avec direction
`FRONT` et `redBorder=visible`, fermeture des probes/RNG.
À session stable : `heap8=112328 largest8=86004`,
`HEADROOM_OK`. Aucune anomalie fatale signalée.

**Limites d'acceptation :** la capture ne prouve pas
`DoomRPG_freeImage` sur sortie/teardown, ni un SAVE écrit puis relu,
ni le comportement de tous les assets, ni un benchmark de fluidité.
La variation `[CORERNG] tableFNV` entre boots provient de seeds
différents ; ce lot n'a pas modifié la cadence RNG.
L'incident historique du monstre invisible ou dormant après LOAD
reste **ouvert**. Le présent commit de clôture est **docs-only** :
aucun binaire changé depuis le PASS matériel. Après cette preuve,
la PR #212 peut passer **ready for review** ; merge effectué par
l'utilisateur seulement. La suite exige de relire `main` après merge
avant toute nouvelle branche.



## Consolidation : six primitives DoomRPG transférées en natif — CI PASS / CYD PASS ciblé (2026-10-15)

Branche `agent/esp32-extract-doomrpg-pure-primitives` créée depuis le
**main exact `bb5b599fbc56c39bd5ddf4a48bc4bd2cfb7cad0c`** après
PR #211. Frontière de code `9fec89d17ab028062477421277422d448e810679`.
Premier lot P1 après le retrait de Render.c : six corps réellement liés
`DoomRPG_FixedMul`, `DoomRPG_FixedDiv`, `DoomRPG_shortAt`,
`DoomRPG_shortAtNext`, `DoomRPG_intAt`, `DoomRPG_intAtNext`
passent dans l'owner permanent sans état
`src/esp_doomrpg_primitives.c`. Le générateur supprime leurs six définitions
de la copie `DoomRPG.c` et contrôle leurs signatures, CRC de référence et
équivalence textuelle stricte de chaque corps. Le vrai `src/DoomRPG.c`
desktop reste intact/test-only. Aucun changement de comportement volontaire,
de RNG, horloge, PAK, framebuffer, SAVE V11, activation ou gameplay.

Test hôte ajouté au workflow : comparaison des vrais corps natifs contre
six corps desktop isolés, **60 000 vecteurs seeded**, encodage
little-endian/offsets et frontières de signes/saturation (sans couvrir
toutes les combinaisons 32 bits ni prouver un ELF bit-identique).
La **CI [38645127794](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38645127794)
est SUCCESS** sur le SHA de code, comprenant tests hôte, builds normal,
trace et bringup et gardes compiledb/ELF. Mesures du runner GitHub Actions
sur le SHA de code (RAM statique / Flash) : normal **50 976 / 809 569 B**,
trace **51 024 / 826 025 B**, bringup **51 128 / 830 073 B**.
Par rapport aux derniers nombres publiés avant ce lot
(50 976 / 809 593 ; 51 024 / 826 041 ; 51 128 / 830 089),
RAM statique inchangée, Flash -24/-16/-16 B. Ce n'est **pas**
une comparaison stricte de neuf sections ALLOC ni un gain FPS/heap revendiqué.

**Test matériel fourni par l'utilisateur (normal demandé, logs Serial sans
fingerprint SHA firmware) : PASS ciblé boot / LOAD / déplacement / combat.**
Boot : ESP32-D0WD-V3 240 MHz, 4 MB, **PSRAM=0**, TFT 320×240 /
framebuffer logique 160×120 RGB565 38 400 B, SD montée, PAK indexé
241 entrées, menu opaque READY. LOAD du slot 07, SAVE **V11**, map 1
`/intro.bsp` (CRC `623f34e4`), restauration annoncée exacte des
overlays ressources/scripts/lignes/topologie/positions/activation,
`ENGINESESSION READY` et `shapeData=0x0 mediaTexels=0x0`.
Un mouvement vers l'avant est présenté et commité une fois
(`tile=552->520`), une porte jammed-door est ouverte par attaque
(`line=413`), le sprite 106 est activé lors du rendu puis reçoit trois
COMMIT de combat (**HP 7→5→3→0**), mort/cadavre présentés.
La retaliation committée fait **HP joueur 27→23** et montre
`DAMAGECUE side=FRONT redBorder=visible`, `VIEWFLASH EXPIRE`
à 504 ms. Session prête : `heap8=112328 largest8=86004`,
`RAMBUDGET HEADROOM_OK`. Aucun fatal, rollback fermé signalant un
échec, ni divergence RNG dans la capture.

**Limites :** aucun checksum de binaire flashé, de comparaison hardware
avant/après, de parcours save *écrit* puis relu dans cette capture, ni de
parité complète du renderer/simulation. L'incident zombie invisible ou
activation post-LOAD reste **ouvert** : la capture atteste activation
du sprite 106 dans ce parcours, pas la résolution de l'autre reproduction.
Le commit de clôture suivant est **docs-only** ; aucun firmware différent
n'est introduit. PR [#212](https://github.com/Kharn27/DoomRPG-RE/pull/212)
présentable au merge après ce PASS ; jamais de merge automatique.

Suite : migrer de façon séparée images / PAK / adaptateurs SDL liés, puis
RNG, clocks, config et racines avec leurs lifetimes explicites. Ne pas
regrouper les 1 600 B d'états historiques de `DoomRPG.o` ni retirer
un wrapper gameplay sous prétexte de réduire le nombre de TU.



## Correctif : synchronisation rectangle rouge / première impulsion directionnelle — CI PASS / acceptation matérielle ciblée (2026-10-14)

Suite sur la même branche depuis `ef2447e`. Capture `814a5034...` : attaques
ranged de sprite 157, probes 4/5/8, `VIEWFLASH EXPIRE` après 1108/818/567 ms
puis `DAMAGECUE ON redBorder=none`. Probes melee 6/7 de sprite 159 : ON avec
`redBorder=visible`. Ce n'est pas une règle de rendu selon la distance :
le rectangle est déjà présenté lors de la retaliation, puis la direction
est queued post-COMMIT, les membres restants reprennent avant son service.
L'expiration du rectangle était en outre évaluée avant l'admission de la file.

Correctif dans l'owner action-feedback existant : l'enqueue post-COMMIT tente
immédiatement le présent de la première impulsion via le compositor existant ;
le service direction passe avant expiry, et la base attend aussi une file non
vide avant restauration. FIFO, barrières UI, retry/timeout et LIFO conservés.
Pas de nouveau owner/buffer, de boucle d'attente ou de redraw monde ajouté.
Un présent physique overlay borné peut être supplémentaire ; aucun gain FPS
revendiqué. Les durées nominales peuvent s'allonger sous charge. Aucun code
retaliation, AI, dégâts, RNG, SAVE V11, PAK ou layout propriétaire modifié.

Test `damage_cue_timing_host.c` : le vrai service UI et compositor, dont le
premier essai sur ancien code échoue faute de première impulsion immédiate.
Après correction : stall 818 ms, barrières touch/level-up/PAK/transaction,
FIFO/overflow, présent fail→retry/timeout, quatre côtés, clock wrap et nouveau
world-redraw entre impulsions ; restauration des pixels/snapshots et guards
exacte. Assets/horloge/présent et autres propriétaires externes mockés : pas de
preuve de transaction retaliation complète ni de timing/SPI sur le CYD.
La nouvelle suite passe également sous ASan/UBSan (LSan off sous ptrace).
Les **22 étapes hôte du workflow** et les gardes ELF/compiledb des trois
profils passent localement.

Builds locaux PASS : normal **50 976 / 809 593 B RAM/Flash**, trace
**51 024 / 826 041 B**, bringup **51 128 / 830 089 B**. RAM inchangée.
**Le normal chargé change pour ce correctif** (+20 B Flash) ; les preuves
d'identité ALLOC des lots de consolidation ci-dessous restent historiques.
CI code **`c81c2d3f2a4a504504a2e3d6a2aa950a94fc5f95`** :
[push 38559202786](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38559202786)
et [PR 38559211874](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38559211874),
**SUCCESS**. Le commit documentaire suivant ne modifie pas le firmware.
**Acceptation matérielle qualitative donnée par l'utilisateur** après ce
correctif : « pas réussi à reproduire, disons que c'est bon ». Ce retour clôt
le besoin de retest demandé pour la désynchronisation rectangle/direction.
Frontière de code de la branche : `c81c2d3`, head présenté pour essai `24d5901`
(docs-only). Aucun nouveau log, SHA de boot ni profil effectivement flashé
fourni : ne pas prétendre à une mesure exacte de concurrence ni à une
validation exhaustive ranged/multi-monstres/melee/MOVE/weapon-cycle/rollback.
Les tests logiciels détaillés ci-dessus restent une preuve distincte.
La disparition du zombie reste séparément ouverte/non reproduite ; cette
acceptation ne la clôture pas. Aucun nouveau firmware dans ce lot documentaire.

## Diagnostics gameplay : profil production TRACE et retrait de deux probes — CI PASS (2026-10-14)

Suite sur `agent/esp32-retire-generated-render`, parent `d8bf3997`.
Trois profils désormais : normal, trace et bringup. `esp32-cyd-trace` hérite
du normal en release, TRACE + filtre `DOOMRPG_ESP32_TRACE_SPRITE=106`, sans
bringup/overlay. Les logs observent uniquement les lectures existantes du
renderer ; pas d'owner/buffer/lecture gameplay supplémentaire ajouté. Les
traces MOVE/MEMBER/RNG existantes complètent admission et raster par sprite.
Les profils story-teardown/divider et **79 lignes** conditionnelles de startup
sont retirés ; leur couverture utile passe côté hôte, avec I/O image et
cleanup intro mockés, sans preuve hardware de heap/PAK.

Builds locaux des trois profils PASS. Normal **50 976 / 809 573 B RAM/Flash**,
bringup **51 128 / 830 061 B** : leurs **neuf sections SHF_ALLOC strictement
identiques** à celles du parent dans la comparaison contrôlée locale.
Trace **51 024 / 826 013 B**, soit +48 B RAM statique et +16 440 B Flash
par rapport au normal : logs TRACE existants + instrumentation, pas un nouveau
buffer graphique. Ne pas attribuer à ce profil de gain de performance :
Serial et l'expiration des leases changent le timing observé.

`test/diagnostic_profiles_host.py` valide séparation de profils, filtre d'un
sprite, admission hidden/BSP/candidate sur le vrai renderer, counts de lectures
identiques et chemin preview/live conservé ; vrais formatter/story/free testés.
Les essais admission ne couvrent pas le raster complet ni la reproduction CYD.
La CI construit les trois profils et vérifie les mêmes gardes d'exports et
d'admission de Render.c. Procédure matérielle : `DOCUMENTATION.md`.
Vérification locale : **21 étapes hôte du workflow PASS**, nouvelle suite
également sous ASan/UBSan (LSan off sous ptrace), gardes ELF/compiledb PASS
sur les trois profils. CI code **`87fdacce3df9dddf08665d9d9a3373ba62d22d25`** :
[push 38548186622](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38548186622)
et [PR 38548193657](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38548193657),
**SUCCESS**. Le commit documentaire suivant ne change pas le code firmware.

**Incident zombie ouvert, pas de correctif ni PASS matériel revendiqué.**
Capture `7abee51e...` : sprite 106, tirs seq 111–113, HP 7→5→3→1,
armor 4→3→2→0, alive 1→1 et rollback fermé. Aucun échec de rendu signalé.
Le tour 60 livre un membre sans attack-probe ; les déplacements réussis sont
masqués en INFO. La disparition puis réapparition au MOVE peut relever de la
position ou du rendu : cette trace ne permet pas de trancher. Charger la
sauvegarde conservée près du cas, tracer tirs/disparition/premier mouvement,
puis valider une éventuelle réparation sur le profil normal. SAVE V11, RNG,
`shapeData == NULL`, `mediaTexels == NULL` et backing PAK restent inchangés.

## Consolidation : retrait de Render.c généré — CI PASS (2026-10-14)

Nouvelle branche `agent/esp32-retire-generated-render` depuis main exact
`b9410482b67af3158c2095aa7eefaa26c3d0980d`. Audit **ELF renouvelé** dans
[`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md), avec CSV symboles de cette
base : Render.o normal **0 B lié**, bringup **7 fonctions / 776 B** ;
DoomRPG.o conserve **27 fonctions / 7 données nommées**, dont RNG et temps.
Ne pas supprimer celui-ci ni regrouper ses globals sans leurs contrats.

Premier lot : Render.c n'est plus généré/compilé dans aucun des deux profils ;
les sept géométries bringup passent au fichier natif existant. Build script
1 190→568 lignes ; 158 sources/152 headers conservés, aucun nouvel owner
production. SCons 215→214 commandes, 160→159 unités moteur, trois→deux copies
générées (DoomRPG.c et shim SDL packed). Desktop original src/Render.c intact,
référence des tests hôte seulement. Les ABI/helpers natifs et wraps
transactionnels restent ; les features desktop non possédées restent deferred.

Normal **50 976 B RAM / 809 573 B Flash**, neuf sections allouées **strictement
identiques** au main de cette base, bytes/adresses/flags/alignement/layout,
app descriptor compris. Ce n'est pas une identité de tout le .bin/digest ELF.
Bringup **51 128 / 830 061 B**, RAM inchangée,
−44 B Flash par rapport aux 830 105 B du main reconstruit. Pas de gain de
fluidité ni RAM production inventé ; pas d'identité du binaire bringup annoncée.

Tests nouveaux PASS local : attribution ELF/map/homonymes ; différentiel des
sept primitives contre référence, 10 000 cas seeded/quatre cardinaux, guards
colonnes et solid BG aux deux profils ; ASan/UBSan (LSan off sous ptrace).
La garde compiledb/map rejette l'ancienne base et accepte le retrait : elle
distingue les artefacts .pio résiduels des véritables sources admises.
Builds normal/bringup, **20 étapes hôte**, garde des exports retirés et garde
source/admission Render PASS local (Python local = python3). **CI distante
PASS** sur le code `0a1dd49e171760f61aaf01cd9e340525da75971c` :
[push 38541821965](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38541821965)
et [PR 38542095599](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38542095599),
suite hôte, les deux builds et les deux gardes, dont compiledb/map régénérés
aux deux profils. [PR #211](https://github.com/Kharn27/DoomRPG-RE/pull/211)
ouverte, pas mergée. Clôture suivante docs-only. La comparaison des neuf
sections avec le main est locale, distincte de cette CI.
Les références desktop ne sont pas des inputs
firmware ; quatre tests parser/admission couvrent aussi l'objet résiduel et
la compilation database vide refusée.

Pas de reflash/PASS CYD demandé pour le normal chargé bit-identique. Aucun
nouveau PASS physique bringup, Render ABI, SD, RNG ou parité globale attribué.
Les anomalies post-LOAD/AI/feedback décrites ci-dessous restent ouvertes,
shapeData/mediaTexels NULL et backing `/DoomRPG-ESP32.pak` préservés. Aucune
modification du gameplay, de ses RNG/timers/journaux ou du format SAVE V11.
Suite : retirer DoomRPG.c par familles/lifetimes (utilitaires, assets/SDL, puis
clock/RNG/config/core), pas copier son monolithe dans un autre fichier.

## Feedback dégâts directionnels + tonneau mortel — clôture CYD (2026-10-14)

**PR #209** sur `agent/esp32-directional-damage-feedback`, base `main`
`65e9e8c5ab199895b0ebbc31088761193576d5b4`. Dernier SHA de code
fonctionnel `6763c9f43f7b8e50a7224748bf9f4e7d4e99698e`; le commit
`56ca2f16d54b8de936c14bb9ae9d72e66097aa2c` annule intégralement
une exploration d'activation post-LOAD (même arbre Git que le dernier
SHA de code fonctionnel). **Aucun scan à 360°, aucune réactivation forcée
au LOAD, aucune modification du format SAVE** dans cette PR.

**Feedback d'attaque — accepté qualitativement par l'utilisateur sur
le vrai CYD après sa dernière passe** (confirmation « C'est good »,
sans nouveau log Serial détaillé dans cette confirmation). Après un
`MONSTERRETAL COMMIT` réel et non létal, un contour rouge 2 px logiques
sur le viewport (500 ms) coexiste avec une bande latérale directionnelle
10 px logiques / 20 physiques, RGB565 mélangé à 50 %. Direction calculée
depuis la position du monstre ayant effectivement commité ses dégâts et
l'orientation du joueur ; `FRONT/RIGHT/BACK/LEFT`. Deux impulsions
de 85 ms séparées par 45 ms, FIFO bornée 8. Aucune barrière de gameplay
ajoutée entre les monstres : le séquenceur reprend sur résolution réelle
`lastResolvedProbe`, pas sur expiration du feedback. Aucune mutation
supplémentaire de RNG/HP/armure/tour, heap, PAK, shapeData ou mediaTexels.

**Réserve observée dans les logs matériels précédant l'acceptation :**
lors de la première attaque immédiatement après une chaîne de déplacements,
`[VIEWFLASH] EXPIRE elapsedMs=803 targetMs=500` précède
`[DAMAGECUE] ON probe=1 ... side=LEFT redBorder=none`. Le premier
clignotement directionnel n'est donc *pas démontré synchrone* avec le rouge
dans cette trace. À l'attaque suivante,
`[DAMAGECUE] ON probe=2 ... side=RIGHT redBorder=visible` prouve bien
la superposition sur la présentation réelle ; autres coups
`probe=3/4 redBorder=visible`. **Acceptation de jalon ne signifie pas
que le décalage après mouvement est corrigé à 100 %.** À isoler si
reproduit, sans réintroduire d'attente du tour de monstre.

**Tonneau mortel — PASS matériel ciblé** sur le log utilisateur :
`BARREL sprite=89`, un seul blast (`chain=1`), le rayon applique
`PLAYER-HIT component=19 hp=15->0 armor=0->0`, réserve le RNG de mort
(`DEATH-RNG-RESERVE value=148`) après celui du blast et avant les
conséquences sur les monstres ; sprites 7 et 8 meurent, 12 XP crédités,
puis `BARRELRADIUS LETHAL-COMMIT ... deathOwner=armed rollback=closed`,
`PLAYERDEATH ARM` et `PLAYERDEATH READY ... death-menu-ready`. Le
checkpoint SD apparaît lisible (`READABLE-SPATIAL result=valid`).
Ce test valide **la mort sur un seul tonneau avec monstres voisins**, pas
la famille létale multi-tonneaux, toujours fail-closed. Un impact
non létal peut émettre son intent directionnel ; le cas mortel laisse
priorité à la présentation de mort. La vraie transition ne réintroduit
pas `applyDamageNonlethal` pour forcer HP=0.

**CI** : push [38476870335](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38476870335)
et PR [38476878889](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38476878889)
**PASS** sur `56ca2f16` (arbre de code inchangé depuis `6763c9f4`) :
fixture 16/16 directions, compositing rouge+direction et restauration
LIFO, death-menu/RNG réservé, barrel terminal/rollback, suite hôte et
builds `esp32-cyd` normal et bringup. Dernières tailles remontées
pour le SHA fonctionnel `6763c9f4` : **50 976 B RAM statique,
809 557 B flash** en normal, **51 128 B / 830 089 B** en bringup.
Ces preuves logicielles ne remplacent pas le test matériel ci-dessus.

**Anomalie ouverte, hors PR : activation des monstres apparus par trigger
après SAVE/LOAD.** Sur map 2, juste après LOAD à
`tile=486 pos=416,992 angle=128`, quatre `PASS_TURN`
consécutifs voient `activeCount=19 delivered=0`. Après deux
`TURN_LEFT`, le rendu BSP déclenche
`MONSTERACT ACTIVE sprite=7 subtype=4 tile=460` et
`sprite=8 tile=521` ; `activeCount=21`, et les deux avancent puis
attaquent. Selon l'utilisateur, ils sont apparus après un ordinateur /
déverrouillage de porte et avaient déjà été vus/activés avant SAVE.
**Cause non tranchée** : instant exact de capture de l'ordre d'activation,
restauration du snapshot V9/V11 (268 B, FNV), possible reset ultérieur,
ou sémantique SHOW/activation BSP. Sur reproduction sauvegardée avant et
après le trigger, comparer `[MONSTERACT] ACTIVE`,
`[NATIVESAVE] SAVE ... activation=<count>/<FNV>`,
`[MONSTERACT] RESTORE ... activeCount=... stateFNV=...`, puis
`MONSTERACTIVESEQ BEGIN`. **Ne pas élargir les monstres actifs par
heuristique de vue arrière avant d'avoir établi la cause en amont.**
Le code de l'IA et de SAVE/LOAD est inchangé pour cette anomalie.

## Revue PR #210 : rollback du démarrage script depuis automap (2026-10-13)

P2 confirmé : SELECT opcode 23 ferme l'automap avant d'exécuter l'interaction.
Si le démarrage retourne EXEC_FAILED/INVALID (par exemple ouverture/premier
present du dialogue refusé), le journal script restaure son segment mais le
resident redessinait seulement le monde, perdant le contexte automap.
Le resident rétablit maintenant son bit RESIDENT_MODE_AUTOMAP puis appelle
`renderAutomapCurrent`, comme les chemins dialogue/password/store adjacents.
Depuis WORLD, le rollback conserve son redraw monde. Si cette restauration
échoue, `disableGameplay` reste la frontière fail-closed ; un démarrage PAUSED
réussi garde la présentation modale, sans rouvrir l'automap prématurément.
Aucun owner/TU/header production ajouté, ni changement de crédit, RNG, WAIT,
sauvegarde ou cadence ; backing PAK et invariants NULL inchangés par ce lot.

Test hôte `script_startup_automap_host.c` : vrai resident SELECT, closeAutomap,
mode et routage de restauration ; script/action, HUD, visibilité, raster et
present sont mockés. EXEC_FAILED/INVALID depuis automap et WORLD, bits voisins,
échec de restauration/fermeture et pause réussie couverts. **Contre-épreuve :**
l'ancien code échoue sur la première assertion de conservation d'automap.
Fixture corrigé et ASan/UBSan PASS local (LeakSanitizer désactivé sous ptrace).
Les **17 étapes hôte** et la garde des exports retirés PASS local ; sur ce
poste, commandes Python exécutées avec `python3` (la CI configure `python`).
Builds normal/bringup PASS : RAM/Flash **47 680 / 805 717 B** et
**47 832 / 825 905 B** (+68 / +44 B Flash, RAM statique inchangée).
Le redraw automap réutilise son scratch de visibilité transitoire existant ;
aucune nouvelle réserve persistante. **CI distante PASS** sur le code
`363828a50557f77f8e47c164c80154b0cc193aba` : 17 étapes hôte, les deux builds
et garde des exports retirés
([push 38446227208](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38446227208),
[PR 38446235549](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38446235549)).
La clôture documentaire suivante ne modifie pas le firmware.
Les preuves CYD ci-dessous précèdent ce changement de code :
aucun PASS matériel de l'échec font/PAK/present n'est attribué à ce correctif.
Ne pas corrompre le PAK pour provoquer l'échec ; un smoke depuis automap peut
vérifier l'ouverture normale du dialogue sans valider l'injection d'erreur.

## Dernier playtest CYD : sauvegarde et limites persistantes (2026-10-13)

Extrait utilisateur après le lot de diagnostics `3910a7c` / doc `5959de5`,
sans SHA de boot : parcours continu jusqu'au SAVE slot 3 puis retour WORLD,
sans crash visible dans l'extrait. Deux explosions interleavées sont commises
(`seq=116`, `blasts=2`, `monsterHits=2`, `rollback=closed`), avec 12 XP et
consommation de munition ; level-up 4→5, extinction d'un feu et MOVE/TURN
continuent. Ce sont les scénarios observés, pas une parité globale du moteur.

`NATIVESAVE SAVE` : `/sd/DoomRPG-ESP32-slot03.sav`, V11, 5 604 B,
`atomic=temp+backup+rename`, `playerFNV=916e606d` conservé à la fermeture du
HUB ; `SAVE-CLOSE result=success feedback="Game saved"`, expiration après
1 203 ms avec `restored=topbar-only`. **SAVE exercé ; LOAD non exercé** dans
cette trace, ni restauration visuelle d'un label d'objet démontrée.

**Limite AI ouverte, pas une régression du lot :** sprite 202/subtype 4
diffère neuf fois le goal 2/3 (`calcPath-special-plane-corpus-needed`, mask
f387). Le premier mouvement reste commis, la continuation n'est pas publiée
(`priorMovesRemainCommitted=yes`), avec alternance des tiles 242/274.
`esp_native_gameplay_monster_three_goal_turn.c` est inchangé depuis la base
main `9d203e525a4b8736fadfd34aaf693423bff7f602`. Ce fail-closed n'est pas une
validation du comportement complet de cette famille ; correction à isoler
dans un futur jalon géométrie/AI, sans élargir le nettoyage actuel.

**Performance :** l'utilisateur ne constate pas d'accélération particulière.
Les frames intermédiaires tracées restent de l'ordre de 100 ms ; ce n'est pas
une comparaison contrôlée avant/après. Le lot réduit les logs/Flash, pas le
nombre de rendus. Aucun gain FPS/latence matériel revendiqué, aucun son ajouté.
Les SPAN_OOB récupérés et les diagnostics sprite restants ne sont pas déclarés
corrigés par cette consolidation.

## Consolidation des diagnostics de mouvement — logiciel testé (2026-10-13)

Même branche, parent documentaire `e12abcd`. Premier lot performance borné :
**19 sites** de diagnostic dans les six owners mouvement/tour existants ;
16 détails succès/collision deviennent TRACE-only, 3 témoins mixtes restent
visibles en INFO sur échec (prepare refusé, rollback position/RNG non exact,
publication de membre échouée). DEFER/FATAL/ROLLBACK, réservations RNG et les
couples attaque-probe/retaliation du watcher restent visibles. BEGIN/COMPLETE
et ORDERED-DISPATCH conservent le résumé de tour. États, ordre, tirages, frames,
timers/cadences et formats save inchangés ; aucun owner fusionné/supprimé,
toujours 158/152.

Motivation chiffrée sur la capture Troop/zombie fournie : **37 lignes / 8 060 B
de succès** retirables sur 16 833 B (~47,9 %). Équivalent UART 115200 8N1 :
699,7 ms de ligne, **pas** un gain CPU/FPS mesuré ni une garantie d'autant de
latence gagnée (buffers/ordonnancement non mesurés). Le coût des redraws monde
par monstre et des animations demeure ; ne pas raccourcir l'idle final pour
accélérer. Trois redraws successifs affichent le même FNV `7aa4ab0f` dans
la trace : indice de pixels inchangés, pas une preuve d'invisibilité ni un
motif pour sauter un rendu d'après son hash. La géométrie doit être préflightée.

Builds normal/bringup locaux PASS ; RAM/Flash **47 680 / 805 649 B** et
**47 832 / 825 861 B** (normal **−2 692 B Flash**, RAM statique inchangée,
aucune nouvelle allocation gameplay).
Les **neuf sections allouées bringup sont byte-identiques** au parent avec le
même toolchain ; aucune identité binaire annoncée pour INFO. Tests : syntaxe
et préprocessing des six vrais TU aux niveaux INFO/TRACE, 16 sites exclus en
normal, trois guards conservés, capture/admission réelles à deux niveaux avec
leurs dépendances mockées ; ASan/UBSan du gate PASS local (LeakSanitizer désactivé
sous ptrace). **CI distante PASS** sur le code
`3910a7cc7e0cc48eeda848c49698463f6837b057` : les 16 étapes hôte, les deux
builds et la garde des exports retirés
([run 38437582969](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38437582969)).

Pas de nouvelle frontière gameplay dans ce lot ; pas de nouveau PASS matériel
attribué. Pour juger le gain ressenti, jouer avec **esp32-cyd normal**, pas
bringup qui conserve tout le texte. Si la latence reste forte, prioriser les
rendus hors champ après preuve d'activation/automap/overlay/rollback, puis la
réservation audio ; aucun backend sonore ni buffer audio ajouté ici.

## Correction WAIT + Back tactile — PASS CYD ciblé (2026-10-13)

Retour matériel sur `fca7d04` : débit 20→10 et dialogue 26 ouverts, puis
`SCRIPTWAIT ARM` sans aucun `PRESENT/COMPLETE` pendant plus de 30 s. Le CYD
reste vivant (`ALIVE`, touch), entrée monde fermée ; **pas de PASS matériel**.
Le suffixe HIDE/SHOW qui remplace le civil n'a pas encore été exécuté dans
cette trace : ce n'est pas une preuve d'un crash de monster spawn.

Cause : le WAIT armait PresentGate avant renderTurn(), alors que le vrai
compositor refuse une gate pré-armée. Le mock de frame hôte précédent avait
masqué cette incompatibilité. Correction dans les owners existants :
`Frame_composeTurn()` compose une pose cardinale sans present, activation
monstre/automap ni fresh-frame publication ; la même adaptation des lignes
dynamiques est scoped comme les previews, sans consommer d'animation/move event.
Le WAIT ajoute les particules puis publie une seule frame physique. La garde
de renderTurn reste intacte ; timer/retry/rollback/note/débit inchangés.

Pas de YES/NO dans ce script original : REQSTAT vérifie, DECSTAT débite.
Le dialogue EV_DIALOG précédent autorise toutefois **Back**, devenu
inaccessible lorsque tous les taps natifs étaient convertis en SELECT.
Une carte BACK visible (viewport x=16..59, y=22..42, hors HUD et texte) rétablit
MENU/cancel ; tous les autres taps gardent SELECT. EV_DIALOGNOBACK après
paiement reste non annulable. Pas de branche de script inventée ni de refund.

Tests hôte : vrai Frame/PresentGate/dynamic-line adapter intégrés au replay
payant ; l'ancien WAIT échoue effectivement dans ce fixture, le nouveau passe.
Quatre angles cardinaux, portes ouvertes/texture déverrouillée, absence de
publish intermédiaire/activation, échec render/present et retries couverts.
Un troisième test exerce le vrai dialog paint/tap/cancel et la fonte HUB ;
PAK/font-atlas et physical present restent mockés. Suite locale : **13 tests C
et 2 tests Python PASS**, ASan/UBSan PASS pour les fixtures WAIT et BACK
(LeakSanitizer désactivé sous ptrace). Builds normal/bringup et garde des
exports retirés PASS. RAM/Flash **47 680 / 808 341 B** et **47 832 / 825 861 B** :
RAM statique/journal lazy inchangés, Flash +248 / +268 B depuis `fca7d04`.
**CI distante PASS** sur le code
`7220f17f97bd334f5348c3eb79cbcf97a61e6249` : suite hôte, les deux builds et
garde des exports retirés
([run 38407334363](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38407334363)).
Retour CYD fourni après `e12abcd` (frontière code correspondante `7220f17`,
SHA de boot absent de l'extrait) : **PASS ciblé** paiement 19→9, dialogue
opcode26 `back=0`, `SCRIPTWAIT PRESENT`, HIDE/SHOW/NOTE, activation sprite9
subtype3 puis COMPLETE et retour WORLD. L'interaction suivante a `NO_ELIGIBLE`
pour le script, sans second débit ; le combat natif tue ce monstre (HP9→0,
armure5→0), consomme une munition et applique 7 XP/drop. MOVE/TURN et une porte
fermée après le tour continuent. NOTE staged 71→104 et commit annoncé par
COMPLETE ; consultation visuelle du notebook non prouvée. BACK généralisé aux
EV_DIALOG est accepté par l'utilisateur ; cancellation et stabilité des
crédits/states non tracées dans cet extrait.

**Non couverts par cette preuve** : refus <10 crédits, SAVE/LOAD, SHA/invariants
NULL du boot et parité visuelle/RNG globale. Garder le test fonds insuffisants
et le smoke SAVE/LOAD pour compléter la frontière. Les SPAN_OOB intermédiaires
sont récupérés (`RECOVERED`), pas assimilés à un crash ni déclarés corrigés.
Aucun flash/merge effectué par l'agent.

## Interaction payante REQSTAT/WAIT — logiciel testé, CYD pending (2026-10-13)

Même branche `agent/esp32-consolidate-postload-junction`, parent `0423fde`.
Trace fournie : event 67/tile 675, SELECT refusé avec `unsupported=23`, après
les dialogues et la progression d'état normaux. `001d0a02` demande 10 crédits,
`00000a02` les débite, puis la fermeture inclut HIDE/state/dialogue 26, effet
capture 35, son 42, WAIT 350 ms et SHOW/HIDE/REFRESH/NOTE/state. Le filtre
`00080000` est le masque **état 4**, pas un numéro d'état 8.

Correction native bornée : préflight de la fermeture avant débit ; rollback
PlayerState/topologie/states/removed si l'ouverture du dialogue échoue ; commit
au dialogue visible. Après clôture, WAIT non bloquant avec entrée monde fermée,
particules orange RGB565 via un painter stateless/local visual RNG, son comme
intent **silencieux**, reprise du suffixe après 350 ms depuis sa première frame
réussie. Note staged dans l'owner existant, commit après rendu final ; failure
I/O/rendu = rollback et retry du suffixe sans rejouer le paiement. Unsupported
stats, autres particules, dialogues cancelables et formes de scripts hors ce
contrat restent fail-closed. SHOW est préflighté conservativement contre la
topologie courante, sans writes ni tirage RNG.

Les tests hôte rejouent les 23 commandes de la trace avec les vrais filtre,
event walker, opcode-state executor, journal/continuation et capture painter.
Player, dialog, PAK, notebook et topology sont mockés dans ce test ; un second
test exerce le vrai kernel SHOW preview/commit/rollback. Cas couverts : <10
crédits, erreur d'ouverture puis retry, double SELECT sans double débit,
attente depuis première frame publiée, échec rendu et note I/O, restauration
exacte du suffixe, NOTE unique, wrap uptime et guards framebuffer/HUD.
ASan/UBSan PASS local (LeakSanitizer incompatible avec ptrace, désactivé).
CI ajoute ces deux tests aux dix tests C existants. **CI distante PASS** sur
`99c9f6153bbbcb2f7259f842ff02f3cbdbe34e48` : les douze tests C, les deux
tests Python, les deux builds et la garde des exports retirés passent
([run 38403169340](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38403169340)).
**Aucun PASS matériel attribué.**

RAM statique cible inchangée ; journal lazy **1 020 → 1 128 B (+108 B heap)**,
mesuré par le type DWARF de l'ELF Xtensa. Le propriétaire NOTE/scratch existe
déjà et reste lazy ; cette route peut l'acquérir. Aucun nouveau fichier moteur
ou header production (158/152), ni source desktop/shapeData/mediaTexels rétabli.
Builds locaux normal/bringup PASS : RAM/Flash **47 680 / 808 093 B** et
**47 832 / 825 593 B**. Flash +5 460 / +5 280 B par rapport au parent ; ce
n'est pas un retrait no-op.

**PASS CYD demandé avant merge** : interaction avec ≥10 crédits → dialogue
→ effet/WAIT → gameplay/note ; crédit débité une fois, état avancé, retap sans
nouvelle facture. Autre essai avec <10 crédits → refus sans mutation. Capturer
`SCRIPTSTAT CHECK/DEBIT/PAUSE/COMPLETE`, `SCRIPTWAIT ARM/PRESENT` et l'état des
crédits/note après reprise. Smoke-test dialogue habituel et HUB/SAVE/LOAD.
PAK, NULL shapes/texels et sauvegarde V11 doivent rester invariants. La parité
pixel-perfect des particules, tous les scripts/stats et l'audio audible restent
hors de cette validation. Aucun flash ni merge main effectué par l'agent.

## Consolidation — lot 2 graphique ZIP + fixtures hôte (2026-10-13)

Même branche `agent/esp32-consolidate-postload-junction`, parent `9cde9d5`,
base main inchangée `9d203e525a4b8736fadfd34aaf693423bff7f602`.
Retrait du renderer/diagnostic de menu ZIP : **16 sources / 13 headers**,
6 455 lignes, 31 exports déjà absents des deux ELF ; diagnostic fermé ramené
de 238 à 11 lignes, quatre directives de wrap sans endpoint chargé retirées.
Après les deux lots : **158 sources C/C++, 152 headers include**, 157 unités
du dossier compilées + 3 copies générées ; aucun original desktop compilé.
Détail, fermeture et suites : [`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md).

Builds locaux normal/bringup PASS, neuf sections allouées strictement identiques
à `9cde9d5` dans chaque profil, mêmes ensembles de symboles. RAM/flash : normal
47 680 / 802 633 B, bringup 47 832 / 820 313 B. `.bin` de même taille, seuls
SHA ELF/checksum/digest image diffèrent, digests vérifiés. Pas de gain RAM
revendiqué. Les deux gardes LRU et leurs 488 B BSS restent conservés.

Les deux bridges scène→dashboard du fichier painter sont **différés** : leur
retrait tentatif changeait l'inlining de deux fonctions actives et 24 B de
Flash. Comparateur strict en échec, tentative annulée ; painter final inchangé.
La suite ZIP n'était déjà pas accessible en bringup (stub RETIRED retourne 0,
`MENU BSP FAIL` conservé). Pas de suppression des vrais diagnostics actifs.

Les trois fixtures hôte SYS touch / level progress / death menu sont réalignées
sur les APIs slot/completion et intégrées à la CI. Les dix tests C, watcher,
comparateur Python et garde des exports passent localement. Slot picker,
restore SD et validateur completion demeurent mockés dans ces fixtures : pas
de PASS matériel/SD/parité globale inventé. Frontière de code du lot :
`8dfd82e00e20305132977b324f8314f83bc6939f`. [CI 38390131286](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38390131286)
**SUCCESS** : tests hôte, deux builds et garde des exports retirés. Le comparatif
ELF avec le parent est local, distinct de cette CI. Clôture suivante docs-only.

Pas de reflash/PASS CYD exigé pour ce lot final : code/data/layout chargés
inchangés. RNG, rollback, save V11, rendu natif et backing PAK sont conservés.
Aucune nouvelle correction des anomalies de gameplay documentées ci-dessous.

## Consolidation architecturale — lot post-load Junction (2026-10-13)

Base auditée exacte : `9d203e525a4b8736fadfd34aaf693423bff7f602`, après le merge
de la PR #208. Rapport et priorités :
[`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md), inventaire des 183 sources
de base dans le CSV associé. Le premier lot retire neuf sources et neuf headers
du ladder historique Junction, 1 691 lignes et 45 exports sans consommateur
extérieur ; aucune section allouée dans normal/bringup, déjà avant retrait.
Après : 174 sources / 165 headers include, sans nouveau propriétaire production.

Vérification locale : builds normal et bringup PASS ; les neuf sections non
vides `SHF_ALLOC` de chaque ELF sont strictement identiques avant/après,
adresses/flags/alignement/taille/octet par octet compris (NOBITS par layout).
RAM/flash utiles inchangées : normal 47 680 / 802 633 B, bringup 47 832 /
820 313 B. Pas de gain RAM revendiqué pour les 108 B de markers des objets
éliminés ; ils étaient déjà absents des ELF. Tests comparateur et garde CI
d'absence d'exports ajoutés. Frontière de code :
`1bd1120550040d0b7060f537bc13783b64af7a5b` sur
`agent/esp32-consolidate-postload-junction`. CI distante
[38385323228](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38385323228) :
**SUCCESS**, tests hôte, builds normal/bringup et garde des exports retirés.
La clôture suivante est docs-only ; aucun nouveau firmware n'est modifié.

À la frontière du lot 1, les tests C/outil du workflow passaient localement,
mais trois fixtures hors CI (SYS touch, level progress, death menu) étaient
désalignées avec les APIs de la base : mocks manquants, pas un PASS. Cette
dette est réparée dans le lot 2 ci-dessus, sans modifier le code firmware testé.

Aucun nouveau trajet exécuté ni PASS matériel revendiqué. Aucun octet chargé
ne change dans cette comparaison contrôlée : pas de reflash/PASS CYD exigé pour
ce lot. RNG, rollback, save V11, PAK et invariants renderer restent hors mutation.
Les preuves/limites matérielles de PR #208 et des autres jalons restent celles
ci-dessous ; ce retrait ne résout aucune de leurs anomalies ouvertes.

## PR #208 — vendeur natif : PASS CYD après les deux correctifs de code review (2026-10-13)

**Témoin sur le vrai classic CYD, firmware normal `esp32-cyd`, frontière de code `f2d85321e6c286289d1021ad6735396aa3987677` + commit documentaire `91635c6d1aaddfe1e064e755b417afe78a1d67b5`.** Le testeur confirme « Allez c'est ok » après un test physique du vendeur `vendor=0`. CI de la frontière : [run 38379283255](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38379283255), **SUCCESS** (normal, bringup, tests hôte). Ce jalon fait suite aux deux commentaires P2 de la PR #208, tous deux corrigés, répondus et résolus.

**Preuves Serial du hit-test store :** `[STORE] OPEN event=0 cmd=0 opcode=33 vendor=0 entries=11 ... credits=12` ; les taps en dehors des cartes, dont `logical=156,74`, `157,75`, `1,75`, reviennent avec `[RESIDENTGAMEPLAY] STORE-TAP ... result=0 worldAction=no queued=no`. Deux validations réelles à l'intérieur des cartes (`logical=124,74` puis `131,76`) déclenchent **exactement deux** `[STORE] PURCHASE` pour `10X BULLETS` au prix de 5 : `credits=12->7`, puis `7->2`, `mutation=player-only rollback=closed turn=no rng=untouched`. Pas de `PURCHASE` causé par les taps hors carte dans ce passage. Les tests hôte vérifient en outre les marges `x=0,11,148,159` et les extrémités inclusives `x=12,147`. Cette preuve porte sur les interactions observées ; elle ne prétend pas tester tous les vendeurs ni toutes les positions de pixels sur matériel.

**Consolidation sans nouveau changement de gameplay :** le correctif `54a88b96` a aligné les rectangles tactiles du vendeur sur le dessin (prompt, YES, NO, notice) ; `da002138` ajoute `/playtest-captures/` au `.gitignore` local, tout en gardant six captures historiques suivies par Git ; `f2d85321` ajoute le test hôte. Le commit `91635c6d` est documentaire. **Après cette validation, seuls des commits docs-only sont admissibles avant le merge** sans nouveau test matériel. PR #208 considérée *merge-ready* à cette frontière, sous réserve de la revue GitHub habituelle ; le propriétaire du repo reste responsable du merge.

## Playtest CYD — Malwrath projectile 204 : PASS matériel ciblé ; barils : acceptation qualitative (2026-10-13)

**Frontière de code matériellement exercée :** `agent/esp32-playtest-incident-capture` à `81fb0361ebe21dfe0769522769730847d5101618` (dernier commit de code/CI avant consolidation documentaire), sur le **vrai classic CYD** sans PSRAM, environnement normal `esp32-cyd`. Confirmation utilisateur : « pour moi c'est ok ». [GitHub Actions 38363323599](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38363323599) **SUCCESS** : tests hôte, build normal et bringup. Le merge de la branche reste à faire par l'utilisateur, pas par l'agent.

**Malwrath (subtype 6, `sprite=75`, arme 15 primaire) — PASS de présentation du projectile et de la séquence d'attaque.** Le legacy `Combat_launchProjectile()` choisit le sprite logique **204** pour l'arme 15 hors subtype 10 ; `CombatEntity.monsterAttacks` attribue cette arme au Malwrath primaire, avec **trois** tirs selon `monsterWpInfo`. Le port n'armait jusque-là `204` que pour l'Imp (subtype 3, déjà PASS). Nouveau contrat compact `esp_native_monster_projectile204_contract.h`, prime sparse du catalogue au chargement si la carte contient subtype 3 **ou 6**, et présentation transitoire `NATIVESPRITE` de trois trajectoires à trois positions sur les trois poses successives. Les autres variantes restent hors de cette extension. Aucun `shapeData` / `mediaTexels` map-wide, aucune mutation du RNG gameplay ou du monde par ce projectile.

**Témoin matériel transmis :** sur `probe=1` puis `probe=2`, `[MONSTERATKVIS] ARM ... subtype=6 ... projectile=malwrath204-3shots-armed`, `[MALWRATHPROJECTILE] FRAME ... step=1/3,2/3,3/3 logical=204 ... presented=1 gameplayRng=untouched`, deux `REARM shot=2/3` et `shot=3/3` par attaque, `[MONSTERATKVIS] COMPLETE ... loops=3 ...` **avant** `[MONSTERRETAL] COMMIT`. Exemple `probe=1` : trois hits, `playerHP=44->32`, `armor=33->21`, `rngCalls=7`, `message="Crit! 24 damage!"` ; `probe=2` : `hitLoops=2`, `playerHP=32->28`, `armor=21->17`, `rngCalls=7`, `message="8 damage!"`. La suite démontre aussi l'attaque du joueur, la mort native du Malwrath (`alive=1->0`), la matérialisation d'un drop (`MONSTERDROP COMMIT`), et le pickup (`Got Sm Medkit`). Cela **ne prouve pas** une parité graphique exhaustive, le son, ni la parité du RNG pour toutes les familles.

**Tonneau à distance / explosions : correctifs livrés, mais PASS ciblé non revendiqué.** Les premiers essais ont démontré la visée du tonneau `sprite=186 tile=241 distance=4` à travers grille et table, puis des refus de préflight distincts : ligne liée `288 type=0 subtype=1` (`BARRELRADIUS DEFER`) et monstre `sprite=202 type=1 subtype=4` dans les voisins d'une chaîne. Les correctifs de la branche autorisent les lignes `type=0` inertes (cf. `Entity_initspawn` / `Game_hurtEntityAt`) et prennent en charge une transaction multi-vagues bornée qui entrelace explosion, impacts de monstres et consommation RNG, avec rollback des états du joueur, monstres et drops ; les familles non possédées restent *fail-closed*. CI verte pour cette logique. Le testeur dit considérer ce conglomérat corrigé, **mais n'a pas retrouvé le tonneau précis ni fourni de `[BARRELRADIUS] BLAST` / `INTERLEAVED-COMMIT` pour ce scénario après correctif**. Consigner l'acceptation pratique sans la présenter comme un PASS de non-régression matériel spécifique. Les derniers logs n'incluent pas cette reproduction.

**Bornes toujours ouvertes :** les précédentes observations `[NATIVEFRAME] LEGACY_GUARD ... RECOVERED` indiquent un chemin de récupération de rendu distinct ; elles ne démontrent pas la disparition de la cause `SPAN_OOB`. Les autres alertes documentées ci-dessous (RNG d'autres branches, collision près de la clé jaune, HUD après switch, etc.) conservent leur statut de validation propre. Le **PASS Malwrath** est attaché précisément au code `81fb0361` ; les deux commits de consolidation `b0dff7a6` et `6c20e2fb` étaient docs-only. **Depuis, la revue PR #208 a nécessité un nouveau correctif de code**, sans rapport avec les projectiles : `54a88b96` borne les taps des cartes du store à leurs rectangles réellement peints, `da002138` ignore les nouvelles captures locales sans dé-suivre les témoins Git existants et `f2d85321` ajoute la régression hôte sur marges et bords. La **nouvelle frontière de code `f2d85321` a désormais un PASS matériel ciblé pour les marges et les achats du store** (voir ci-dessus). Les logs de cette session valident des marges représentatives et deux vrais achats, non le balayage exhaustif de chaque pixel du rectangle ; les bords exacts sont testés par le test hôte. Aucun autre comportement gameplay n'a été intentionnellement modifié par la revue.


## Playtest CYD : attaques à distance des Imps — PASS matériel (2026-10-11)

**Confirmation explicite du testeur sur le vrai classic CYD :** « Les attaques à distance des Imps sont validées ». Les correctifs sur la branche `agent/esp32-playtest-incident-capture` relatifs au sprite de projectile Imp 204 (catalogue sparse `7cb7c00`, composition additive `14f02ad`, instrumentation `362dc91`, test hôte `5210621` et CI `a7fccdc`) sont donc associés à un **PASS matériel de cette interaction**. Ce retour ne contient pas de nouvelles empreintes Serial détaillées et ne constitue pas une validation générale de la parité graphique des projectiles, du RNG de tous les monstres ni de l'audio. Ne pas étendre ce PASS à ces autres familles. Le correctif HUD postérieur `e02b090b` reste séparément en attente de son test CYD.

## Playtest 04:29–04:45 : trois anomalies matériel observées, correctifs à tester (2026-10-10)

**Source matérielle :** six fichiers utilisateur sur `agent/esp32-playtest-incident-capture:ESP32/playtest-captures/` (5 incidents et 1 snapshot), code d'origine des captures `955d739a3dace4d961a554f02bef1bf94c7d1e4f`. `main` demeure `d07a4b0a59c527b1a3792f1c2c38b0f0010affc1`. Aucun nouveau `agent/*`, aucun merge `main`. Plusieurs SAVE V11 sur SD facilitent la reproduction.

**Collision près de la clé jaune : défaut confirmé dans les traces et code.** `snapshot-20261010-044501-465022.log` : `tile=665->697 blocker=286 type=1` répété en FORWARD, puis `tile=665->633 blocker=286 type=1` en BACK. C'est **le même monstre lié à la case de départ**, et non une preuve d'échec du pickup de clé. `esp_native_gameplay_collision.c` trace source+destination et refusait toute fuite lorsqu'un ennemi rejoint la case du joueur. Parallèlement, `esp_native_gameplay_monster_movement.c:cardinalTraceClear` ne simulait pas la sentinelle `flags & 0x100` du `Game_trace()` legacy : un monstre pouvait entrer sur la case du joueur. Correctifs `70301cd9` et `680a33bd` : reconstitution stricte de la sentinelle *uniquement sur les tracés de mouvement avec 0x100*, et échappement des ennemi(s) de **source** seulement lorsque le joueur partage déjà leur case ; les murs/lignes et les ennemis de destination restent bloquants. Sur échappement : `[PLAYEROVERLAP] ESCAPE` (anomalie visible). Le watcher détecte également deux `MOVE-BLOCKED` avec la **même** source et le **même** sprite mais des destinations différentes et déclenche `PLAYER_STUCK_COLLISION` sans cooldown (commits `690286d6`, `c988dec2`). Le bug « clé jaune » n'est **pas encore validé comme résolu sur CYD**. Ne pas qualifier un simple `NO_EVENT` de pickup réussi.

**Renderer : autre SPAN_OOB, distinct des portes ouvertes.** Captures `incident-20261010-044024-406340-00004-FIRSTFRAME_UNRECOVERED.log` et `...00005-WORLD_RENDER_FALLBACK.log` : angle intermédiaire 149, vue `1440,224,36`, première limite compact wall `actual=68 source=32768`, puis seconde `actual=112 source=65536 line=185`. La récupération initiale ne conservait qu'un octet successeur **d'une seule texture**, donc échouait à la deuxième texture hors bornes dans la même frame. Correctif `7e416590` : jusqu'à **4 octets successeurs distincts** de mappings PAK validés, BSS bornée, retry des renders depuis scratch restauré ; aucun débordement autorisé, même limite `WALL_PACKED_BYTES`, échec final `RECOVERY-FAILED` explicite. Aucun framebuffer ni texels map-wide ajouté. À confirmer sur matériel avec angle 149 et rotation depuis `tile=118` après LOAD.

**RNG : première attaque souvent seule à diverger, pas une anomalie masquée.** `incident-20261010-042939-174153-00001-RNG_REPLAY_DIVERGED.log` : `sprite=220 producerProbe=1 firstRandHit=223 firstRandDamage=227`, suivi de `MONSTERRETAL COMMIT probe=1 firstRandHit=149 firstRandDamage=22`, alors que FNV pré-RNG identique `d2cffe4e`; 3 HP et 2 armure soustraits dans les deux cas. Le motif **suggère** une limite de table RNG qui ne concerne pas chaque attaque : `DoomRPG_setRand` modifie un état global caché hors `Random_t`, le garde natif avait une lease de replay de **1000 ms**, or animation d'attaque et idle peuvent dépasser ce délai. Correctifs `97b4f4a1`, `7cb85a5d`, `aa053fdb` : petite transaction native `beginAttackProbe/endAttackProbe` **dans la famille immédiate des monstres actifs**, réserve le post-refill exact jusqu'à la véritable lecture à la frontière, même après 1 s d'animation ; restaurer `Random_t` strictement après le probe, ne changer ni RNG généré ni combat math. Ne prétend pas couvrir d'autres familles de probe (postmove/3-goal) ni prouver la parité complète sans logs hardware. Le watcher continue à signaler `RNG_REPLAY_DIVERGED` **sans cooldown**, et alerte désormais `RNG_GUARD_FAILURE` pour les erreurs internes de réservation (`da1a772d`), également sans cooldown. Il reste possible de manquer un événement si Serial perd des lignes ou si le monitor commence tard ; ne jamais prendre l'absence d'alerte pour preuve d'intégrité.

**Validation CI code HEAD :** `2cab1aae55b30d88c3e24a1633f0d7ae376fa34b`, [CI 38019176441](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38019176441) : autotest de la capture Python, build normal `esp32-cyd` et bringup **SUCCESS**. Les réservations RNG sont maintenant explicitement libérées dans `EspNativeGameplaySession_reset()` pour les handoffs et LOAD (commits `ea4566d8`, `aa1aecd5`, `2cab1aae`). Seul un test CYD peut valider la parité RNG du premier combat et la réparation du passage près de la clé jaune.

**Validation restante :** firmware normal `esp32-cyd` à flasher et tester sur vrai classic CYD. Demander les lignes `[PLAYEROVERLAP]`, `[MONSTERMOVE] TRACE-BLOCK ... player-sentinel`, `[NATIVEFRAME] RECOVERED ... guards=...`, `[RNGGUARD] ATTACK-PROBE-*`, `[MONSTERTURN] MEMBER-ATTACK-PROBE` / `[MONSTERRETAL] COMMIT`, et `[WATCH]`. Sauvegardes et données PAK préexistantes intactes. Les hits d'un monstre ne doivent pas être modifiés hors résolution RNG attendue. Aucun résultat matériel inventé.

## Playtest CYD : captures utilisateur, correction des previews et RNG à traiter (2026-10-10)

**RNG incident coverage follow-up (2026-10-10, logiciel CI PASS, hardware à refaire) :** `RNG_REPLAY_DIVERGED` et `RNG_REPLAY_UNVERIFIED` contournent définitivement le cooldown du watcher (même avec `--cooldown` grand) ; les fichiers ont un numéro unique et ne s'écrasent pas entre attaques rapprochées. Les familles `MONSTERRETAL COMMIT`, `MISS-COMMIT`, `LETHAL-COMMIT` sont prises en compte, ainsi que le garde natif `REPLAY-DIVERGED`. Le firmware ajoute à `LETHAL-COMMIT` les champs `firstRandHit/firstCalcHit/firstCritLimit/firstRandDamage` **à des fins de logs uniquement**, sans aucune modification de RNG/dégâts. Sur ancien firmware, un `LETHAL-COMMIT` apparié sans `firstRandHit` déclenche explicitement `RNG_REPLAY_UNVERIFIED`, jamais un PASS implicite. Le mécanisme ne peut pas garantir la détection si les lignes Serial manquent, si la session du watcher commence après le probe ou si un couple n'est pas journalisé ; la correction permanente exige un propriétaire RNG transactionnel probe/commit. [CI 38016897988](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38016897988) : autotest Python, `esp32-cyd` et bringup **SUCCESS** sur le head du code `c9f3e8b7`. Ne pas attribuer de PASS hardware avant reflash et logs CYD.


**Branche de travail conservée :** `agent/esp32-playtest-incident-capture`, basée sur `main d07a4b0a59c527b1a3792f1c2c38b0f0010affc1`. L'utilisateur a poussé ~30 captures Serial réelles dans `ESP32/playtest-captures/` (à conserver comme preuves, non destinées à la production). Le travail inventaire/Notebook de la branche secondaire est **rapatrié en un commit** `925e60c3ba49408f1046eb661be07b506fc852fe` sur cette branche. La branche inventaire secondaire ne doit plus recevoir de commits.

**Animations : cause précise retrouvée dans le code.** Sur la partie vierge, avant l'ouverture de la porte 275, les previews MOVE/ROTATE publient `[VIEWANIM] FRAME ... rendered=yes presented=1` avec 1 ou 2 intermédiaires. Après `SELECT_DOOR` (`03:33:12`), `[TURNFRAME] DIAG fail=WORLD_RENDER` et `[VIEWANIM] FALLBACK ... intermediates=0` apparaissent dès le déplacement suivant. Or `esp_native_first_frame.c:renderFrame()` refuse `EspMapLineState_view()->openCount != 0` (garde de bringup), et seul le frame cardinal avait `dynamicFrameActive` via `__wrap_EspNativeGameplayFrame_renderTurn`. Les previews `renderVisualPose` et `renderVisualMove` contournaient cet adaptateur, d'où les rejets après portes ouvertes, indépendamment des sauvegardes. Correctif `44aecacc` + flags linker `86f70d38` : wrappers render-only utilisant **la même projection dynamique des lignes et textures** que le rendu cardinal, sans consommer les frames d'animation des portes ni publier des événements/tours. [CI 38014897652](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38014897652) **SUCCESS** sur `esp32-cyd` et bringup. **Hardware non encore validé** : exiger une ouverture de porte, puis MOVE/ROTATE avec `[VIEWANIM] FRAME` et non `FALLBACK`; garder les protections de rendu et les invariants mémoire.

**Difficulté : un défaut réel de parité RNG potentielle apparaît dans les logs, mais n'est pas encore corrigé.** Capture `incident-20261010-033406-440372-WORLD_BACKEND_DEFER.log` : `MEMBER-ATTACK-PROBE ... producerProbe=1 sprite=220 firstRandHit=177 crit=0 ... rng=ba3b467a`, puis `MONSTERRETAL COMMIT probe=1 sprite=220 firstRandHit=5 crit=1 ... playerHP=30->23 armor=8->1 message="Crit! 14 damage!"` avec le **même FNV RNG de départ**. La fréquence/difficulté n'a pas été modifiée par les commits multislot/capture/inventaire, mais l'issue anticipée de ce coup diffère de l'issue appliquée. Piste technique de haute confiance à prouver : `DoomRPG_randNextByte` appelle `DoomRPG_setRand` à la frontière des 128 octets ; `Random_getRand` utilise `_seed` et `resetRand` statiques **hors** du `Random_t` sauvegardé/restauré par le probe. Un refill pendant le probe change donc un état global qui n'est pas annulé par `doomRpg->random = randomBefore`; le commit peut obtenir un autre jet. NE PAS affirmer que l'état du RNG est exact aux limites de refill avant la correction. Nécessite un jalon RNG distinct avec preuve matériel/logicielle de parité probe→commit et fail-closed. Le watcher PC contient désormais une catégorie `RNG_REPLAY_DIVERGED` qui compare `producerProbe/probe, sprite, firstRandHit, crit` et capture automatiquement l'écart ; validation CI à confirmer pour le SHA du watcher.

**Statut :** consolidation de branche effectuée, builds CI réussis pour les wrappers, pas encore PASS hardware des animations, du soin ou du Notebook ; aucune écriture dans `main` et aucun merge automatique.

## Outil PC de capture playtest — autotest et CI PASS, CYD non instrumenté (2026-10-10)

Branche `agent/esp32-playtest-incident-capture` créée depuis `main` exact `d07a4b0a59c527b1a3792f1c2c38b0f0010affc1`. Outil Python autonome `ESP32/tools/playtest_watch.py` : lit UART via pyserial ou stdin, filtre par préfixes d'incidents réellement actionnables plutôt que chaque mot `deferred`, conserve un contexte hôte borné de 75 lignes avant/20 après, capture immédiatement sur disque, limite à un événement par classe par 45 s, option alerte sonore, snapshot des dernières lignes à Ctrl+C. Aucune modification du firmware, aucun buffer embarqué, aucune écriture SD, aucun freeze/watchdog. Commande : depuis `ESP32/`, `python3 tools/playtest_watch.py --port /dev/ttyUSB0 --bell`, **sans lancer simultanément le moniteur PlatformIO**. Logs sous `ESP32/playtest-captures/` ignorés par git.

Code de l'outil `10d4c42494d83d0daebb5a4c55d9c3f46dab38aa` ; workflow test ajouté `5b40bc05`, documentation et ignore ajoutés ensuite. [CI 38012812860](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38012812860) **SUCCESS** : `py_compile` + `--self-test` du filtre et des extraits, builds normaux `esp32-cyd` et diagnostic `esp32-cyd-bringup` SUCCESS. Pas de validation du capteur sur un vrai flux Serial ou de comparaison d'effets gameplay : **outil hôte, pas un jalon firmware CYD**. Les règles explicites peuvent manquer de nouvelles familles ; conserver le contexte entier sur Ctrl+C ou étendre la classification sur preuve. Cheats et pause native des actions demeurent deux sujets séparés.

## Multi-save PR #207 — P1 death-menu CYD PASS, P2 accepted (2026-10-10)

Code head testé : `0824b4c4adc84e0604e9a0ca74075def23f4f91d`; CI GitHub Actions du head : **SUCCESS**. Le code est inchangé après cette validation, les commits ultérieurs de clôture sont documentaires uniquement.

**P1 (death-menu LOAD)** — **PASS sur CYD normal** : attaque létale après `PASS_TURN`, `[PLAYERDEATH] READY ... load=available`, tap LOAD, `[SAVESLOTS] MAIN-OPEN slots=10 pages=2` puis `[DEATHMENU] SLOT-OPEN ... choice=required`. Pagination `MAIN-PAGE page=2` puis `page=1`, retour `[DEATHMENU] SLOT-BACK`, réouverture LOAD, `MAIN-ARM slot=1`, `MAIN-CONFIRM slot=1`, enfin `[DEATHMENU] DISPATCH ... slot=confirmed`. Le checkpoint V11 du slot 1 est restauré à `pos=1184,352 angle=0`, joueur `34/34`, armure `11/23`, `RESIDENTRESET released=18008 empty=1`, `ENGINESESSION RESUME-VISIBLE`, `shapeData/mediaTexels` inchangés. Aucun chargement automatique du premier slot n'a eu lieu à la première pression LOAD.

**P2 (hidden main selector on restore failure)** — correction défensive revue et **acceptée par l'utilisateur sans reproduction matérielle du cas d'échec tardif**. Le chemin de récupération réussie `MENU_MAIN` appelle `EspNativeGameplaySave_mainSelectorFinish()`; pas de test hardware d'injection d'erreur et aucun checkpoint SD n'a été altéré artificiellement.

**Réserves hors périmètre** : polish final des menus, autres slots et coupure électrique non exhaustivement testés, fallbacks de rendu `WORLD_RENDER`, anomalie `exactHud=NO`, tours monstres différés. Pas de slot AUTO.

## Native SD multi-save — CYD gameplay PASS, navigation accepted (2026-10-08)

**Baseline:** `main` `4554ccd68f7928bd8918ffaf8711d82179f5e587`; branch `agent/esp32-native-save-multislot`, code head `c4f1322509237031bd048c57bb7798036271e06c`. [CI 37835789142](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37835789142): **SUCCESS**. Firmware normal `esp32-cyd`, classic CYD sans PSRAM.

**Hardware evidence:** manual SAVE slot 2 `/DoomRPG-ESP32-slot02.sav` V11 5604 bytes, position 544,1760; after moving and collecting two armor shards, LOAD slot 2 restored exactly position 544,1760, armor 0/20, consumed resources 0, monster state/topology/position/activation. LOAD slot 1 also restored a **different earlier checkpoint** position 1184,352, 36 consumed resources, 12 dead monsters, HP 34/34, armor 11/23; legacy single-save file `/DoomRPG-ESP32.sav` was detected as readable at selection entry. MAPRT `c3882516`, `shapeData=0x0`, `mediaTexels=0x0`, heap8 resident after load 117880/largest8 86004. Session reset logged `released=18008 empty=1`. No save version change, no legacy deletion.

**UI review:** MENU_MAIN LOAD now opens its own ten-slot picker; HUB SAVE/LOAD uses five rows per page. Latest revision separates **previous** (upper) and **next** (lower) chevrons on a right-side column, with disabled states, page number in header, and removes the decorative red vertical stripe. User reviewed the display and called it *better*, accepting it for this milestone with additional Doom-style visual polish **deferred**. This is a visual/acceptance report, **not** a complete new serial trace of both page directions or slots 6..10.

### Complément CYD : slot 8, deuxième page et restauration (2026-10-08)

Traces du firmware normal, après le code `c4f13225` : HUB → SYSTEM → SAVE → `[SAVESLOTS] PAGE page=2 via=right-column` → `ARM slot=8` → `CONFIRM slot=8`. `[NATIVESAVE] SAVE path=/sd/DoomRPG-ESP32-slot08.sav version=11 bytes=5604 pos=1248,352 angle=0 resources=37/43B ... atomic=temp+backup+rename`. Après déplacement supplémentaire et ramassage d'armure, HUB → SYSTEM → LOAD → page 2 → `ARM slot=8` → `CONFIRM slot=8` ; `LOAD path=/sd/DoomRPG-ESP32-slot08.sav version=11 bytes=5604 pos=1248,352 angle=0 playerFNV=eeda091c`, `RESTORE consumed=37`, armor `15/23`, monsters/topology/positions/activation restaurés. La progression plus tardive avait atteint armor `19/23` et n'a donc pas remplacé le slot 8. `[RESIDENTRESET] released=18008 empty=1`, `[ENGINESESSION] RESUME-VISIBLE`, `heap8=117880 largest8=86004`. Ceci valide sur hardware le chemin **page 1→2**, le SAVE/LOAD du slot 8 et l'indépendance temporelle du checkpoint. Le chemin de retour page 2→1 n'est pas attesté par ces traces.

**Anomalies ouvertes, indépendantes du multi-save :** en haut de carte Entrance chargée depuis la progression avancée, `[TURNFRAME] DIAG fail=WORLD_RENDER` et `[VIEWANIM] FALLBACK` se répètent (mouvement et rotations) ; le mouvement logique est commité une fois et le rendu cardinal final continue, mais l'animation intermédiaire est perdue. À analyser dans un jalon render dédié, sans masquer ces fallbacks. Un `[HUB] CLOSE ... exactHud=NO` est aussi observé juste après SAVE slot 8, alors que `hudBottom ... exactBottom=yes` et le gameplay reprend. Investiguer ce décalage du témoin HUD séparément ; ne pas le déclarer PASS visuel exact. `[MONSTERMOVE] DEFER cause=active-order-not-owned` avec 12 actifs reste une dette de gameplay connue, non causée ici par le système de slots.

**Still to test separately:** page 1→2→1 on both selectors with actual hardware logs, slots 6, 7, 9, 10, overwrite confirmation, empty-slot fail-closed, MENU_MAIN BACK, and power-loss atomicity. `AUTO` remains explicitly out of scope pending a genuine original-game trigger audit. No merge has yet been reported for this branch.

# Doom RPG ESP32 — état actuel du port

## Prochain jalon — multi-save SD : 10 slots, AUTO conditionnel (2026-10-08)

Branche `agent/esp32-native-save-multislot`, base `main` `4554ccd68f7928bd8918ffaf8711d82179f5e587`. **Périmètre corrigé avec l'utilisateur : exactement 10 slots manuels au maximum, et pas 20.** AUTO est **conditionnel**, non promis tant que les déclencheurs et la sémantique originaux n'ont pas été audités. Legacy `Game.c` possède `EV_SAVEGAME` (27), qui capture un itinéraire/point d'arrivée, ainsi que `Game_saveState` avec fichiers `Player`/`Player2`; cela ne prouve pas un slot AUTO supplémentaire dans l'interface originale. Ne pas transformer EV_SAVEGAME en autosave arbitraire. Le runtime actuel écrit un unique V11 5604 B sous `/DoomRPG-ESP32.sav` sur la microSD, avec `.tmp`/`.bak` ; partition flash interne raw PAK n'est pas le lieu de stockage. V1..V10 restent lisibles. Préserver l'ancien checkpoint et conserver la compatibilité en lecture. Toute écriture doit être atomique par slot et ne jamais toucher à un autre fichier ; pas de cache des dix sauvegardes en RAM. HUB SAVE/LOAD et MENU_MAIN LOAD doivent sélectionner la même source explicite, avec pagination tactile 160x120 et confirmation d'écrasement. Séparer le stockage borné et l'UI en jalons hardware-testables, et vérifier les chemins LOAD/death/changement de carte avant implémentation d'AUTO.


## Render solid background — REAL-CYD NON-REGRESSION PASS (2026-10-08)

Tested code SHA `0503b08b7bd89a7a50706f9d18cda51fd75679e0`; normal esp32-cyd CI [37790502396](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37790502396) **SUCCESS**. `Render_renderFloorAndCeilingSolidBG` is now defined by the permanent `esp_render_geometry_primitives.c`; generator keeps desktop/bringup source and checks exact legacy function parity. No extra allocator, backing store, or mutable world ownership.

Real classic CYD boot: Render/Game/Canvas = 1532/4/44 B; `shapeData=0x0 mediaTexels=0x0`, MAPRT `c3882516`, first gameplay frame `71ca7465`, cache SMALL-COLD `a9b263f5` and SMALL/LARGE-WARM `20c09fe4`. FORWARD midpoint, both TURN previews, crate transform and Armor Shard pickup, event 88 dialogue/opcode-19 resume, HUB pages and double-confirm SYS EXIT all worked. `[RESIDENTRESET] released=18008 ... empty=1`; final MENU_MAIN `522dc605`, heap8 `164184`, largest8 `110580`, `saveWrite=no checkpoint=unchanged`. These traces prove the **session non-regression only**; no direct invocation of the migrated solid BG function was instrumented, no LOAD or live-monster test, and no pixel-level equivalence proof beyond reported FNVs. Future Render changes need a separate CI and CYD PASS.


> Source de vérité : **main GitHub + code + logs Serial du vrai classic CYD**. État de référence au 8 octobre 2026, `main` : [`60d34d174bed0d4d4e13137f306070b5018c0fa9`](https://github.com/Kharn27/DoomRPG-RE/commit/60d34d174bed0d4d4e13137f306070b5018c0fa9). Les SHA ci-dessous décrivent des frontières testées, pas nécessairement des mesures du commit documentaire courant.

## Render shapeData helpers — real-CYD PASS, merge checkpoint (2026-10-08)

Firmware code `f013172b4164b488b4a46a9dd9af72a22e64e232`, followed by documentation-only `4692f2b781272ba21b430e37c7bf1d8d3da0154d`. GitHub normal/PR CI [37809328500](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37809328500) and [37809337192](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37809337192) **SUCCESS**. Normal `esp32-cyd` on physical classic CYD: 4 MB flash, no PSRAM; Render/Game/Canvas = 1532/4/44 B; framebuffer 160x120 RGB565 = 38400 B. Idle main menu `heap8=159340 largest8=110580`, displayed `522dc605`. Full START→intro→Entrance→MOVE/TURN→crate transform→armor pickup→dialogue event 88 with opcode-19 continuation→HUB SYSTEM→double-confirm EXIT→menu. MAPRT arena `c3882516`; first frame `71ca7465`; cache SMALL-COLD `a9b263f5`, SMALL-WARM/LARGE-WARM `20c09fe4`; runtime gameplay idle `heap8=118288 largest8=86004`. Intro disposal reclaimed 34056 B. Exit `[RESIDENTRESET] released=18008 empty=1`; menu `heap8=164184 largest8=110580`, `[SYSEXIT] session=off resident=empty saveWrite=no checkpoint=unchanged`. `shapeData=0x0 mediaTexels=0x0` remain invariant. No crash or divergence observed.

**Scope:** validates non-regression after normal-production exclusion of legacy `Render_getSTexelOffsets` and `Render_getSTexelBufferSize`, **not** execution of those helpers or exhaustive native texture/sprite parity. No save/load cycle, combat with active monsters, or death was exercised in this final test. The ledger's Berserk tint, deferred visuals and earlier WORLD_RENDER preview fallback issues remain OPEN. Post-test change is documentation-only; branch can be merged by the user after checks.

## ShapeData Render_getSTexel helpers — pending CI/hardware (2026-10-08)

Legacy `Render_getSTexelOffsets` and `Render_getSTexelBufferSize` read `render->shapeData` directly. Their known legacy consumer is `Render_loadTexels`, which allocates map-wide `mediaTexels` and is incompatible with native PAK / compact cache ownership. Generator now retains exact originals for desktop and bringup but excludes both helper definitions from normal ESP32; unexpected linked dependencies fail closed. This removal does NOT certify original texture/sprite feature parity; those remain the responsibility of native decoders. New hardware test and CI required.

## Render_draw2DSprite ABI cut — CYD non-regression PASS (2026-10-08)

Code `580fac411ec3ba19514c112e080f12d7308f4664` (followed by docs-only `90659246638b070ad6a6a91e541a12562020dad7`), normal/PR CI [37805150632](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37805150632) and [37805154165](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37805154165) **SUCCESS**. Real ESP32-2432S028R hardware: 160x120 RGB565 38400B, no PSRAM; `shapeData=0x0 mediaTexels=0x0` at entry, map handoff, and EXIT. MENU_MAIN `522dc605`, MAPRT `c3882516`, first gameplay frame `71ca7465`, cold `a9b263f5`, warm `20c09fe4`. MOVE/TURN, blocked BACK, crate transform, armor pickup, dialog event 88 opcode 8→19, HUB system double-confirm EXIT all work; `[RESIDENTRESET] released=18008 empty=1`, menu heap8 164184 / largest8 110580, `saveWrite=no checkpoint=unchanged`. No crash or abnormal heap decline in this scenario.

**Scope**: real-CYD *non-regression* PASS for excluding legacy `Render_draw2DSprite` and preserving native visuals. Does **not** establish full feature parity of all weapon/overlay flash effects; their parity remains OPEN. No SAVE/LOAD, death, or monster-combat test here. Following documentation commit is docs-only.

## Legacy Render_draw2DSprite retirement — awaiting CI and hardware (2026-10-08)

The old Render 2D sprite path indexes `render->shapeData` and is incompatible with the ESP32 `shapeData == NULL` invariant. Audit found its source definition in `src/Render.c`, no other reference in that file, and production desktop DoomCanvas is already retired. The generator now keeps original desktop/bringup source but excludes the legacy routine from normal production compilation; native sprite and weapon rendering remain unchanged. This only retires an unsafe compatibility ABI, **not** a statement that every original weapon flash/overlay behavior has parity. Require fresh CI and CYD normal-env non-regression before closing the increment.

## Render Berserk ABI cut — CYD non-regression PASS (2026-10-08)

Hardware test of code `b57b0d910c2ca05058bc70387e0306e66e15b2f7` (with subsequent docs-only `6d2026617666c04f495ef54c5b13f748801c75e0`). Normal/PR CI [37803814898](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37803814898), [37803820112](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37803820112): **SUCCESS**. Real classic CYD: intro disposal recovers 34056 B; `shapeData=0x0 mediaTexels=0x0`; MAPRT `c3882516`; FIRST_FRAME `71ca7465`; cache COLD `a9b263f5`, WARM `20c09fe4`. MOVE/TURN, crate, dialog 82 with opcode 19 resume, HUB→SYSTEM→double-confirm EXIT completed. Resident cleanup `released=18008 empty=1`; menu FNV `522dc605`, heap8 `164184`, largest8 `110580`, `saveWrite=no`. **PASS is non-regression only**: Berserk item/effect was not tested, and its red-tint behavioral parity remains OPEN. No LOAD, monsters, or death exercised in this run.

## Render Berserk desktop ABI retirement — pending CYD validation (2026-10-08)

Source audit: `Render_setBerserkColor` reads/modifies framebuffer pixels and drives legacy `SDL_UpdateTexture` / `SDL_RenderCopy`; known original call sites are in `src/DoomCanvas.c`, a retired production TU. The normal firmware must not retain this desktop presentation API: generator now guards the original body as desktop/bringup-only and checks body drift, so any remaining production caller would be a linker error rather than silent reactivation. **This does not implement or certify original Berserk behavior.** The red-tint behavior remains a functional parity item (deferred) until native ownership, trigger and CYD visual proof are established. No gameplay, death fade, framebuffer size, or sprite renderer is changed by this cut.

## Render PR #205 — CYD PASS après correction des guards (2026-10-08)

**Firmware testé** : code `291f99fcdf2e46536d5f941f13e033b16338639c` (suivi du commit documentaire `a70a5835e3f02a64cb60893fd1c8497ccaa95d2d`, aucun changement code). CI du head pré-test : [37801938309](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37801938309) et [37801946460](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37801946460), **SUCCESS**. La correction P2 rend désormais `Render_renderFloorAndCeilingSolidBG` compilable sous le guard de production ; l'algorithme n'a pas changé. La nouvelle compilation et le parcours matériel passent, **sans preuve d'appel direct à cet ABI**.

Log Serial sur classic CYD : Render/Game/Canvas `1532/4/44 B`, framebuffer partagé `38400 B`, `shapeData=0x0` et `mediaTexels=0x0`. Menu initial et final `522dc605`; MAPRT `c3882516`; première frame `71ca7465`; cache SMALL-COLD `a9b263f5`, SMALL-WARM/LARGE-WARM `20c09fe4`. Parcours réel : START → intro → Entrance → MOVE/TURN → caisse transformée, pickup armor, dialogue événement 88 avec opcode 19, HUB → SYS EXIT double confirmation → menu. `[RESIDENTRESET] released=18008 empty=1` ; menu final `heap8=164184 largest8=110580`, `saveWrite=no checkpoint=unchanged`. Aucune régression sur ce parcours. Non couvert par ce test : LOAD, monstres actifs et mort, appel positif de la fonction solid BG. La compatibilité fade legacy reste retirée, son comportement de mort est possédé par `fadeViewport()`.

**Conclusion :** PASS de compilation normale/PR et PASS matériel de non-régression, sur le code ci-dessus ; documenté sans nouvelle modification de firmware. Une preuve fonctionnelle du solid BG exigerait un appel explicite distinct ; ne pas confondre linkage et utilisation.

## Code review PR #205 (2026-10-08)

- **P1**: newline perdu par le stripping du témoin fade : corrigé au SHA `d145acf45601aa2d35050fc8f82b798fd250f5fb`, CI normale et PR vertes.
- **P2**: `Render_renderFloorAndCeilingSolidBG` était initialement placé par erreur sous `#if FIXED_VERSION != 1`, donc exclu du firmware normal. Corrigé au SHA `291f99fcdf2e46536d5f941f13e033b16338639c` : corps original inchangé, désormais placé dans le guard production `DOOMRPG_ESP32 && !DOOMRPG_ESP32_BRINGUP_PROBES`. **Les anciens PASS matériels n'éprouvaient pas ce symbole** ; attendre CI et nouveau test CYD pour ce correctif.

## Render fade ABI retirement — implementation awaiting new hardware test

Legacy call census: `src/DoomCanvas.c` invokes `Render_fadeScreen` in its desktop animation paths (lines 824, 1556); the production ESP32 `DoomCanvas.c` translation unit is retired and its live death animation calls `fadeViewport()` in `ESP32/src/esp_native_gameplay_player_death.c`. Source and the previous real-CYD death log confirm native death fade, but not ABI invocation. The latest increment **removes the production `Render_fadeScreen` export entirely**. Desktop/bringup retain original code; a newly introduced normal firmware caller must fail at link time, not silently revive legacy framebuffer semantics. The preceding native fade implementation was a temporary migration, not needed for equivalent game behavior. Pending CI and real-CYD non-regression of this *new* commit, do **not** call it tested.

## Render fade call witness — real-CYD death path (2026-10-08)

Tested firmware head `d145acf45601aa2d35050fc8f82b798fd250f5fb`, normal esp32-cyd CI [37796616566](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37796616566) and PR job [37796635096](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37796635096) both **SUCCESS**. The one-shot `[RENDERFADE] ENTRY` witness is present in the native ABI implementation; **no such entry appears in the provided death-session Serial excerpt**. The trace does show `[PLAYERDEATH] ARM`, `PHASE elapsedMs=807 fade=begin`, `READY elapsedMs=3027 fade=0 frames=51 input=death-menu`, after ordered monster attack and lethal player state commit. Therefore the **native death fade is hardware-exercised**, but no positive runtime call to legacy-named `Render_fadeScreen` is established. Do not claim that its migrated body is functionally exercised; assess whether the ABI can be retired after confirming its complete caller census.

This run also covers monster crit/hit/miss, active move, hazard touch/pass, door close held across monster turn, and stable observed idle heap8=118288 B / largest8=86004 B. `[TURNFRAME] DIAG fail=WORLD_RENDER` preview fallbacks recur, including moves around 1376,384/448 and strafe at 1397,480; this known gap remains **OPEN**, not a Render fade regression attribution. No full LOAD-after-death coverage in supplied excerpt. Hardware results apply to this exact code SHA; following commit is documentary only.

## Render fade — REAL-CYD death-sequence PASS (2026-10-08)

Hardware-tested firmware code SHA `2e541ab86824099fe2e6e9b73095826154cd9aa1`; normal CI [37793287250](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37793287250) **SUCCESS**. Real CYD: boot / MAPRT `c3882516`, first frame `71ca7465`, cache `a9b263f5` / `20c09fe4`, MOVE/TURN, crate, pickups, dialogs 88+79, door 275, active monster movement and attack, and lethal PASS_TURN all proceed. Player death logs `[PLAYERDEATH] ARM ... fadeMs=750..3000`, `PHASE elapsedMs=804 ... fade=begin`, then `READY elapsedMs=3013 phase=death-menu-ready fade=0 frames=51`. During initial stable gameplay, heap8 remains 118288 B and largest8 86004 B; death-menu heap8 114828 B, largest8 86004 B. No crash observed. **This proves the native death fade path, NOT direct invocation or pixel parity of the newly owned legacy-named `Render_fadeScreen`**. The exact call path still requires an invocation witness. The old `[TURNFRAME] ... fail=WORLD_RENDER` midpoint fallbacks, `[NATIVEFRAME] LEGACY_GUARD→RETRY→RECOVERED` and lack of LOAD testing are not silently cleared by this PASS.

## Functional parity ledger — original Doom RPG vs native ESP32

Status vocabulary: **native-validated** = original behavior reproduced and exercised on hardware; **native-partial** = live behavior with recorded gaps; **disconnected/deferred** = legacy call removed/stubbed but user-visible behavior still to reproduce; **compat-only** = old ABI retired because a separate native owner replaces it; **unverified** = original function exists/migrated but runtime invocation not yet shown. Never use a linker/build PASS alone as behavior parity.

| Original family / behavior | Native state / owner | Remaining proof or missing feature |
| --- | --- | --- |
| World BSP visibility and rendering | native-partial: compact runtime + native wall/plane/sprite renderer | Packed-wall guard recovery and some VIEWANIM WORLD_RENDER fallbacks still occur; regression-path investigation pending |
| Screen floor/ceiling solid fill (`Render_renderFloorAndCeilingSolidBG`) | **unverified invocation**, implementation migrated unchanged | First-session regression PASS SHA `0503b08b`; direct-call witness not recorded |
| Framebuffer fade (`Render_fadeScreen`) | **compat-only, retired from production linkage**; no native ABI export | Death fade **native-validated** in SHA `2e541ab8`; prove whether this specific ABI is actually called |
| Player death fall + fade + death menu | native-partial: `PLAYERDEATH` | Fall/fade/death menu real-CYD PASS; legacy shake and death sound intentionally deferred; test LOAD/RETRY from death menu separately |
| Monster activation, movement, retaliation, attack animation | native-partial: `MONSTERACT/MOVELIVE/RETAL/ATKVIS` | **Imp ranged attacks: CYD PASS (2026-10-11)**; generic projectile/attack sound/message/pain face/shake and movement interpolation parity still open |
| Save/load original world semantics | native-partial: save V11 and read-compatible V1–V10 | LOAD animation fallbacks, loaded live-monster lifecycle and older save coverage remain |
| Original sound/music | disconnected/deferred: `AUDIOINTENT` silent backend | Implement native playback independently; don't revive legacy Sound object |
| Legacy plane-test functions / BSP traversal / map loads | compat-only: production `esp_legacy_render_reject.c`; normal owner native | Reject stubs are not validated positive invocations; any missing original effect must get a native owner, not a fallback |
| Legacy `Render_draw2DSprite` / `shapeData` decoding | **production legacy ABI retired**; native weapon/sprite assets read packed PAK | Weapon/HUB visuals are native; **remaining original overlay/effect parity OPEN**, do not restore map-wide `shapeData` |
| Legacy berserk postprocess (`Render_setBerserkColor`) | **disconnected/deferred**: desktop reference retained; ABI absent in both CYD profiles after generated Render retirement | **Berserk red-tint behavior parity remains OPEN**. Audit original trigger, timing and native HUD/framebuffer owner before claiming reproduction; never silently re-enable legacy SDL texture writes |

Update this ledger whenever a legacy function is unlinked or a corresponding native behavior becomes hardware-validated; historic milestones live in Git, not in 123 separate files.

## Règles fondamentales

- Cible : ESP32-2432S028R classic CYD, ESP32-D0WD-V3 à 240 MHz, flash 4 MB, **0 PSRAM** ; ILI9341 320×240, XPT2046, microSD.
- Framebuffer partagé : **160×120 RGB565 (38 400 B)** ; présentation nearest-neighbor ×2.
- Source des assets runtime : **`/DoomRPG-ESP32.pak`** sur SD. `/DoomRPG.zip` est un input de génération/référence, **pas** un backing store runtime.
- `shapeData == NULL`, `mediaTexels == NULL`. Ne pas réintroduire de décompression carte entière, de tables de texels globales, ni de graphes d'entités desktop.
- Architecture pérenne : catalogues/parsers natifs → `EspMapRuntime` immuable compact → petits états mutables possédés → moteur événementiel → gameplay et rendu natifs.
- Desktop/J2ME = **spécification comportementale**, jamais l'architecture mémoire imposée au CYD.
- Le redraw doit suivre les événements, et non une boucle vidéo fixe optimisée prématurément.
- Lire aussi [`ARCHITECTURE.md`](ARCHITECTURE.md) (contrat durable) et [`DOCUMENTATION.md`](DOCUMENTATION.md) (utilisation/build).

## Frontière hardware actuellement validée

| Élément | Dernier témoin CYD observé |
| --- | --- |
| Build normal | `esp32-cyd` ; dernier CI production P1 : [37775947093](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37775947093) SUCCESS |
| SHA code hardware P1 | `cccd26d1e3560d1948db3004805d4c8d1a8317a2` |
| Structs Render / Game / Canvas | 1532 / 4 / 44 B |
| Main menu framebuffer FNV | `522dc605` |
| MAP_INTRO / native arena FNV | `c3882516` |
| Première frame gameplay FNV | `71ca7465` |
| Sortie SYS, session libérée | `[RESIDENTRESET] released=18008 empty=1`, journal dialogue : 1036 B |
| Heap8 après retour au menu (échantillon P1) | 164184 B ; largest8 110580 B |
| État PAK / mémoire | invariants shapeData/mediaTexels NULL aux frontières observées |

Ces chiffres **ne sont pas** une promesse universelle pour toutes les cartes ou séquences d'interaction. Les FNV du cache renderer ont changé lors du rétablissement de `FIXED_VERSION=1` : SMALL-COLD `d4151456→a9b263f5`, SMALL-WARM/LARGE-WARM `efb3a31b→20c09fe4`. Il n'y a **pas** de preuve d'équivalence pixel à pixel pour ces caches. La graine RNG varie entre boots.

## Frontières natives et propriétaires actuels

### Storage et cartes
- Backing PAK indexé et accès bornés SD/offsets ; catalogues natifs.
- `EspMapRuntime` compact immuable, accessors sémantiques sans allocations ; `EspMapState`, scripts, lignes/textures, automap et topologie sprite possédés explicitement.
- Identité carte via catalogue ; une nouvelle BSP **ne doit pas** nécessiter un renderer ou un moteur dédié par niveau.
- Les scripts/événements mutent des états explicites, avec rejet des cas non supportés et rollback là où requis.

### Gameplay et UI
- Intro et premier rendu, menu principal tactile, Help/Options, START new game, LOAD checkpoint natif, HUD/HUB, interactions, portes, pickups, armes, ennemis, mort joueur, changement de carte et checkpoint natif ont des chemins ESP32-native.
- Sauvegardes : le format V9 inclut la topologie et l'état spatial monstres/destructibles ; la compatibilité V8 est ciblée (ne rejoue pas arbitrairement les scripts).
- Les parties gameplay vivantes ont leurs propres propriétaires : actions joueur, mouvement/attaque des monstres, état du monde, rendu, feedback et save/load. Ne pas rétablir les structures desktop comme autorité.
- Détail des invariants de modules : [`ARCHITECTURE.md`](ARCHITECTURE.md).

### Menu de mort — candidat graphique, hardware à confirmer

- Refonte limitée à la présentation : palette industrielle et polices HUB
  partagées, titre rouge, quatre cartes alignées avec indications d'état.
- LOAD actif en ambre avec checkpoint ; sinon `NO SAVE`. Junction/Retry/Main
  restent fail-closed et affichent `NOT AVAILABLE`. Aucun nouveau backend.
- `MAIN MENU` du menu de mort est distinct du retour `SYS → EXIT TO MENU`
  fonctionnel du HUB. Ce dernier est inchangé par la refonte graphique.
- Chute, fondu, admission à 3000 ms et dispatch LOAD conservés ; pas de nouvel
  asset, framebuffer ou état permanent. Propriétaire de mort : toujours 28 B.
- Build local `esp32-cyd` : PASS, RAM statique 45056 B / flash 774713 B.
  Test host `test_death_menu.c` : PASS, timing, rendu, hitboxes, guards et routes
  avec backends mockés ; aperçu du vrai painter inspecté avec/sans sauvegarde.
- Validation CYD du nouvel aspect et du LOAD après mort encore à faire, selon
  le protocole de [`DOCUMENTATION.md`](DOCUMENTATION.md#présentation-du-menu-de-mort).

### Render — frontière de désengagement en cours
- `Render_t` 1532 B ; aucun retour des gros membres historiques.
- Le normal `esp32-cyd` a les propriétaires permanents `render_startup_bridge.c`, `esp_render_geometry_primitives.c` et `esp_legacy_render_reject.c` pour les familles déjà migrées.
- `Render_initColumnScale`, `Render_cullBoundingBox`, `Render_transform2DVerts`, `Render_clipLine`, `Render_clipVertex`, `Render_projectVertex`, `Render_occludeClippedLine` sont natifs ; calcul fixed-point `FIXED_VERSION=1` restauré pour la parité legacy.
- `Render_findEventIndex`, `Render_relinkSprite`, `Render_renderBSPNoclip` et trois fonctions plane-test sont retirés du production legacy, via des compatibilités **fail-closed** là où nécessaire. `Render_freeRuntime` a un propriétaire natif.
- Depuis le jalon b941048, Render.c n'est plus généré/compilé dans aucun profil CYD : la cartographie a prouvé zéro code lié en normal et seulement sept primitives géométriques bringup, désormais fournies par l'owner natif existant.
- Les références desktop restent hors firmware pour les tests différentiels ; les builds normal/bringup passent. Le normal chargé est strictement identique dans la comparaison locale ; le binaire bringup change, sans PASS matériel revendiqué.
- Les validations de retrait sans appel effectif sont des **PASS de non-régression**, pas des preuves que les stubs ont été invoqués.
- `SYS EXIT` éprouve le reset résident natif ; il ne prouve **pas** l'exécution directe de `Render_free` / `Render_freeRuntime`.

## Limites et risques à ne pas effacer

- Le chemin **LOAD → aperçu d'animation de déplacement** possède encore un fallback connu ; la séquence monstres actifs après LOAD demande une validation dédiée.
- Les derniers tests Render/P1 fraîche session **ne couvrent ni LOAD, ni attaque/mouvement de monstres vivants**.
- En cache packed-wall, le chemin `[NATIVEFRAME] LEGACY_GUARD → RETRY → RECOVERED` a été observé ; comportement de récupération bornée, pas preuve de correction complète.
- Le dernier test HUB a rapporté `exactHud=NO` pour bandes haut+bas combinées, `exactBottom=yes` ; recomposition de la barre supérieure reportée au redraw.
- Une réussite CI ne remplace **jamais** un PASS Serial du vrai CYD. `esp32-cyd-bringup` n'est pas la référence de RAM production.

## Prochaine direction

Poursuivre le **désengagement de Render** par **famille de fonctions** : auditer les appels réels, comparer au comportement legacy, assigner un propriétaire permanent, fail-closed pour l'incomplet, instrumenter un probe borné, CI normal et preuve matérielle avant d'étendre la frontière. **Aucune nouvelle famille Render ne doit être activée en bloc.**

## Source de l'historique

Les anciens 123 rapports `MILESTONE*.md` ont été supprimés du HEAD pour éviter duplication et fausses informations courantes. Ils restent dans Git au [commit de base documentaire `60d34d174bed0d4d4e13137f306070b5018c0fa9`](https://github.com/Kharn27/DoomRPG-RE/tree/60d34d174bed0d4d4e13137f306070b5018c0fa9/ESP32) et dans les PR/commits antérieurs. Le présent fichier décrit **ce qui est valide maintenant**, non l'ordre des travaux.
