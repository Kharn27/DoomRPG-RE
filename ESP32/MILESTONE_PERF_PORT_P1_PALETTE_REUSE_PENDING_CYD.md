# PERF-PORT-P1 — réemploi exact des palettes déjà résidentes (CYD PENDING)

**Branche isolée :** `agent/esp32-perf-port-palettes-p1` depuis le vrai GitHub
`main=1588b6983cc5724f7028ac0d2c18f96688aba990` (22 novembre 2026).
Ne pas fusionner sans accord utilisateur. La PR RES60 #364 reste intacte.

## Audit comparatif réel

| RES60 | Production 160×120 actuelle | Gain plausible | Coût RAM P1 | Risque / décision |
|---|---|---|---|---|
| P8 : texels, catalog/PAK cache | LRU 3×2 048 B dans `esp_native_first_frame.c`, catalogue compact et cache résident PAK ≤19 KiB déjà présents ; résolution de palettes refaite à chaque frame | À mesurer (nombre de palettes réutilisables inconnu) | Pas de cache permanent | **P1** réemploi d'une palette cataloguée par offset exact |
| P9 : wedge avant raycasting | Culling de bounds BSP + clipping dynamique de ligne déjà intégrés (`esp_native_bsp_visibility.c`) | Incertain | — | **NON** sans oracle de clipping/occlusion |
| P10 : matériau projeté visible | En prod `acquireWall()` travaille à la demande, libération totale via `releaseCache()` | Incertain | — | **NON** à la transplantation de la projection |
| P11 : chronométrage | world/sprite/hud/present/total disponibles ; pas la résolution individuelle des palettes | Attribution CPU uniquement | 8 B de scalaires transitoires sous probe | **OUI** : stopwatch de la seule résolution |
| P12 : BSP inventory cache | `EspMapRuntime` déjà chargé pendant toute une session gameplay, pas d'inventory à chaque rotation | Nul pour le cas mesuré par P12 | Inutile | **NON** |

**Cible RAM :** classic CYD, 4 Mio flash, 0 PSRAM, unique framebuffer
160×120 RGB565 38 400 B. Le plus récent test matériel décrit sur main
`HEADROOM_OK margin8=11976`, mais ce n'est PAS une mesure de P1.

## Mécanisme P1 : une seule frontière

`EspNativeGraphicsCatalog_buildFromRuntime` charge à l'admission les
records immuables de 40 B, contenant déjà 16 couleurs **BGR565 brutes**
(`paletteRgb565`) et `paletteSourceOffset`. À chaque frame,
`buildResolvedTextures()` lit `texturesIDs`, calcule la phase des
animations, lit la paire réelle `actualId` (source+palette), puis recharge
les 32 octets de palette.

P1 conserve absolument la résolution, les validations des longueurs,
des offsets et la phase originale. **Uniquement si l'offset palette
résolu, vérifié sur 32 bits, correspond exactement à l'offset 16 bits du
catalogue de la même ressource logique**, le renderer réutilise les 16
mots déjà validés. Il applique **la même conversion**
`sourceBgr565ToRgb565()` qu'avec une lecture PAK. En cas de différence,
l'ancienne lecture de `palettes.bin` reste identique et fail-closed.

Aucun atlas, mapping global, framebuffer, allocation persistante ou
modification de la cache murale 3×2 048 B. Le catalogue est reconstruit
pour une nouvelle carte et les entrées de map vérifiées via taille+CRC
avant le rendu. Le PAK source ne doit pas être modifié en place pendant
une session validée ; la production impose déjà ce contrat.

Macros : `DOOMRPG_PERF_PORT_PALETTE_REUSE=1` par défaut, `=0` pour
l'oracle de contrôle. Dans cette branche, `DOOMRPG_PERF_PORT_PROFILE=1`
est activé dans le profil normal `esp32-cyd`, **temporairement** :
`[PERFPORT] PALETTE ok=1 resolved=N catalogHits=H pakReads=R resolveUs=U cacheBytes=0`.
Deux appels `esp_timer_get_time()` par image, et compteurs ajoutés
uniquement au `FirstFrameWork` transitoire. Le Serial est émis **après**
`resolveUs` mais avant le retour au compositeur, donc il fausse
`worldMicros` de cette branche. Comparer seulement `resolveUs` entre
deux variantes **également instrumentées** (P1 ON/OFF).

## Tests host : corps C réels, ON/OFF

`ESP32/test/perf_port_palette_host.py` extrait le vrai corps
`buildResolvedTextures()` de la production. Deux compilations C11
différentes, mêmes entrées, exigent les mêmes octets/FNV des résolutions
sur : texture palette identique, palette différente (fallback),
phase animée, nouveau contenu de carte/catalogue, tentative d'alias
palette > 65535, échec de lecture palette/mapping, catalogue absent,
header invalide. Les lectures attendues passent de 4 à 3/2 uniquement
lorsque la correspondance est réelle. Aucun résultat host ne prouve
une amélioration CPU sur le matériel.

## Protocole CYD : strictement en attente

Firmware normal : `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor`.
Conserver Serial complet sur ≥10 images, rotations dans les deux sens,
portes animées, combats/pickups, EXIT/MENU et si possible LOAD V11.
Observer `[PERFPORT] PALETTE` et `[TURNFRAME]`, les FNV monde/frame à pose
égale, les compteurs de mur, la RAM libre / largest8 / minHeap8, la
musique et les audits AudioIntent, et `shapeData/mediaTexels=NULL`.

Pour l'A/B, tester **le même code, même firmware normal et même PAK**
avec une seconde compilation `DOOMRPG_PERF_PORT_PALETTE_REUSE=0`.
Ne comparer ni une image à froid et une rotation à chaud, ni des
angles/états du monde différents. Si `catalogHits=0` ou si
`resolveUs` ne s'améliore pas de façon répétable, aucun gain ne sera
annoncé : candidat à abandonner/revert. Aucune confirmation de parité
visuelle, d'audio et de stabilité RAM n'existe encore pour ce code.

**PENDING CYD.** Pas de merge automatique.
