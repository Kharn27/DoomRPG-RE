# RES60-P8 — échantillon mur PAK 4bpp + cache de rotation (PASS CYD borné, contrôles MENU/visuel en attente)

> État courant, 19 novembre 2026 : **PASS matériel borné du chargement de texture et du cache de rotation**, amélioration **~1,868s → ~0,463s** prouvée sur vrai CYD. **MENU→START cache cold** et photo de la vignette non validés. Le protocole et les mentions « CYD PENDING » historiques ci-dessous décrivent le candidat *avant le test* ; leur portée est remplacée par le résultat matériel ci-dessous.

## RES60-P8 — échantillon 4bpp + cache de rotation : **PASS matériel CYD borné ; MENU/reset et fidélité visuelle encore à vérifier** (19 novembre 2026)

[PR expérimentale #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), [milestone P8](MILESTONE_RES60_P8_WALL_TEXEL_PENDING_CYD.md). **Deux essais utilisateur sur le vrai ESP32-2432S028R classic CYD sans PSRAM** : `platformio-device-monitor-261119-214142.log` pour P8 avant correctif (101 lignes), puis capture Serial intégralement collée dans la conversation après correction (pas de SHA firmware dans les logs ; attribution par les tags `cache=MISS/HIT`). Environnement expérimental `esp32-cyd-res60-inverted` d'après contexte, PAK SD natif `/DoomRPG-ESP32.pak`.

**Régression mesurée avant correction :** raycast + préparation `≈1 810 ms` et frame `≈1 868 ms` par rotation contre P7 `≈393 / 440 ms`. Le scanout LCD reste `≈36,9 ms` ; le catalogue `mappings.bin/palettes.bin` était reconstruit et `wtexels.bin` relu à chaque orientation, sans modification de texture. Le vrai sample `logical=87, catalog=33, sourceNibble=36864, catalogFNV=29ffc14a, texelFNV=e78943b7, sampleFNV=5503bc26` était stable.

**Correctif de code testé :** HEAD `40ab78b394849f5498a9906bd2725a96fbe22a32` ; cache borné utilisant le **même buffer source 2048B**, palette 32B et petits scalaires, protégé par `sourceCrc32+arenaFNV1a+sourceBytes`, invalidé à START et MENU réussi. Double CI code [push 44612746837](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44612746837) et [PR 44612762923](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44612762923) : **SUCCESS 230/230 chacune**. Le vrai test C11 force zéro lecture PAK/catalogue pour HIT, invalide les sources changées et vérifie reset.

**Nouveau test CYD utilisateur après correctif :** première image `[RES60P8] SWATCH cache=MISS bytes=2052 pixels=2304 sampleFNV=5503bc26`, `[RES60P7] FRAME totalUs=1825313` (~1,825 s). Puis **cinq rotations RIGHT** successives `0→3→2→1→0→3` : à chaque fois `cache=HIT bytes=0`, `sampleFNV=5503bc26`, `texelFNV=e78943b7`, `catalogFNV=29ffc14a` ; frames respectivement `462929, 462461, 462790, 463115, 463036 µs`, moyenne **462866 µs** ; raster `404741..405398 µs`. **Rotation ~4,0× plus rapide et -75 % de temps**, très proche de P7 à ~440 ms. Les 4 `rawP3FNV` restent déterministes : N64 `ed9b68c4`, E0 `48ae8442`, S192 `2697640d`, W128 `f43a2791`. `/intro.bsp crc32=623f34e4`, arena `14095B / FNV=c3882516`. Heap8 stable après chaque rotation `136884`, plus gros bloc `86004`, minimum enregistré `109892`, `mapRuntime=RELEASED pak=closed`. Pas de panic/WDT dans les traces, utilisateur : « Ça marche mieux c'est clair » puis « Ca marche mieux oui ».

**Frontière de PASS :** le Serial prouve lecture native PAK froide, cache chaud sans lecture de texels, rotation et RAM stable ; **aucune photographie ni jugement sur fidélité colorimétrique de la vignette** n'a été communiqué. Le log corrigé s'arrête dans la vue 3D : **retour MENU puis deuxième START/cold MISS non exercés sur matériel** (ces routes sont couvertes par le test C11). Aucun mur texturé en projection, sprite, HUD final, entité ou gameplay 60 % ; `worldTextureSamples=0`. Le ~1,825 s au premier START reste une dette pilote de chargement à étudier séparément, pas un problème de scanout.

---


**Repo** `Kharn27/DoomRPG-RE`, **PR draft #364**, branche `agent/esp32-res60-p1`. Date : 19 novembre 2026. Ceci suit les PASS matériels P6/P7; **P8 n'est pas encore un rendu texturé des murs**.

## Périmètre et source de vérité

P8 ne modifie ni la caméra, ni les 480 segments BSP, ni les touches P7, ni le framebuffer unique 192×144 RGB565 (55 296B), ni le build production 160×120. Une fois le P3 raycast **flat shaded** terminé, `EspRes60TextureProbe_render` lit le **catalogue graphique natif existant** (`mappings.bin` + `palettes.bin` du `/DoomRPG-ESP32.pak`) construit à partir des ressources réellement requises par `EspMapRuntime`. Il choisit déterministiquement la première ligne BSP ordinaire dont la texture est cataloguée et dont l'offset 4bpp source est pair (pour éviter les frontières packed-nibble non encore autorisées).

Il ouvre `wtexels.bin` sous une courte lease PAK, valide strictement le préfixe longueur 4B et un unique intervalle `sourceOffset/2 + 4` de **2048B** (64×64 pixels à 4 bits), puis referme PAK et catalogue. Le décodage sur un petit buffer BSS **2048B** respecte **nibble bas pour pixel pair, nibble haut pour pixel impair**, et conversion historique **BGR565 brut → RGB565 framebuffer** (comme `EspNativeFirstFrame`). Une seule vignette 48×48 sans double-buffer est écrite **à x144,y24**, coin supérieur droit de la vue 192×96. Les autres pixels du monde restent le rayon P3. Aucun ZIP runtime, aucune allocation map-wide, aucune structure Render_t supplémentaire ; aucune donnée source persistante après le rendu.

Le **FNV `rawP3FNV`** est pris avant P8, donc la valeur historique au spawn reste `ed9b68c4` (PAK non modifié). La vignette change le `fnv` final P7 et ne doit pas être interprétée comme un FNV de textured world. Nouvelle ligne `[RES60P8] SWATCH source=PAK map=/intro.bsp file=wtexels.bin logical=... catalog=... sourceNibble=... catalogFNV=... texelFNV=... sampleFNV=... bytes=2052 inset=48x48 pixels=2304 framebuffer=192x144 worldTextureSamples=0`. Pas de sample UV sur les segments, ni textures projetées, sprites, entités, gameplay, HUD final ou audio.

## Contrats stricts

`DOOMRPG_RES60_P8_TEXEL=1` uniquement pour `esp32-cyd-res60` et `esp32-cyd-res60-inverted` (hérité), requiert P7/P3; symbole absent en production 160×120. Le catalogue ne peut être actif à l'admission, le PAK doit être fermé, la carte compacte déjà chargée. Les dépendances sont relâchées avant `PlatformVideo_present()` ; en cas d'IO/ref manquante, offset impair, longueur PAK invalide ou ressource absente, refuser la nouvelle image **sans la présenter**, sans avancer la rotation. Le painter valide tous les pointeurs/tailles avant le premier pixel; BSS plafonnée à 2KiB. `EspRes60TextureProbe_render` refuse de fonctionner hors de la fenêtre de vie de la carte.

Test `python ESP32/test/res60_p8_texel_host.py` : compile vrai propriétaire P8 C11, vérifie les nibble pairs/impairs, l'inversion R/B historique, lease PAK et catalogue, refus de fichiers invalides/lecture tronquée/offset impair, pixels extérieurs intacts, canaries, FNV stable après second rendu, garde production. La CI contient aussi builds Xtensa normaux/inverted/RES60, tests P1–P7 et parité AUDIO/SDL. Un succès CI n'est pas une validation matérielle P8.

## Test sur vrai classic CYD

```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```

`/DoomRPG-ESP32.pak` inchangé sur microSD. Depuis menu, START → relâcher → START. Attendre `[RES60P8] SWATCH ... bytes=2052 pixels=2304`, `[RES60P7] RASTER ... angle=64 rawP3FNV=ed9b68c4`, `FRAME ... mapRuntime=RELEASED pak=closed`. **Vérifier visuellement le petit carré coloré/texturé en haut à droite de la vue 3D** ; le reste des murs reste volontairement en couleurs plates. Tourner à droite puis à gauche, vérifier swatch stable (même ID/source FNV), P3 rawFNV reproductible, MENU → P6 FNV `c093a13b` et heap8/minHeap8 sans dérive. Vérifier les touches rejetées hors boutons. Joindre log reset→menu→START→rotations→MENU et, si possible, photo.

**Gate P8 : CYD PENDING.** Une fois prouvé, prochain milestone : calage UV/projection de véritables textures 64×64 sur segments BSP 192×96 avec cache borné et test d'oracle desktop, sans transformer le moteur entier en 160→192 global.
