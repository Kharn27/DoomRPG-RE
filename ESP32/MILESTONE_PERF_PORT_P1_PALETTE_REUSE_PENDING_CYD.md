# PERF-PORT-P1 — palettes natives réutilisées (PASS CYD fonctionnel borné ; A/B CPU pending)

**Branche isolée :** `agent/esp32-perf-port-palettes-p1` depuis le vrai GitHub
`main=1588b6983cc5724f7028ac0d2c18f96688aba990` (22 novembre 2026).
Ne pas fusionner sans accord utilisateur. La PR RES60 #364 reste intacte.

## Capture matérielle réelle — 22 novembre 2026 : PASS fonctionnel borné, performance NON prouvée

**Source :** `platformio-device-monitor-261122-092240.log`, **3 535 lignes /
409 445 octets**, SHA-256
`d72b489f2582b1caacd29406511dfea9a7cb25d5f5a6531311a243f20cc0e77d`.
Attribution au candidat `01b3eac5d8ad3daf85b98d27547451cbb809c2a0`
par contexte du flash et tags `[PERFPORT]` ; **SHA firmware non imprimé dans
le Serial**. ESP32-D0WD-V3 à 240 MHz, 4 Mio flash, PSRAM=0,
framebuffer **160×120 RGB565, 38 400 B**, profil panneau
`alternate-panel inversion=off`, environnement normal `esp32-cyd`
annoncé pour ce test.

**Ce qui est réellement validé :** deux sessions Entrance, la première
depuis NEW GAME/prologue et la seconde depuis un LOAD save V11 ; 2
reconstructions du catalogue `textures=33 sprites=45 storage=3120B
fnv=29ffc14a`, MapRuntime `arenaFNV=c3882516` et source CRC
`623f34e4`. **363/363** `[PERFPORT] PALETTE ok=1`, sans
régression observable du résolveur. Chaque frame résout **33 textures**,
`catalogHits=3 pakReads=30` : **9,09 % des requêtes de palettes**
sautent le `EspAssetPack_readRange` logique, soit 1 089 appels
logiques économisés dans cette capture. Ce nombre n'est **pas** un
compteur d'accès flash physiques économisés, puisque le PAK possède
déjà son cache résident.

`resolveUs` parmi les 363 rendus : **médiane 1 209 µs**, minimum
**624 µs**, maximum **815 519 µs** sur le **premier rendu froid**
(avant cache/flash prêts) ; **67 rendus >=5 ms**, avec pics chauds
typiques autour de 10–12 ms. La différence cold/warm est importante,
mais **elle ne démontre absolument pas** un gain de P1 :
aucun firmware `DOOMRPG_PERF_PORT_PALETTE_REUSE=0` équitablement
instrumenté n'a encore été testé sur le même CYD/PAK. Le probe Serial
ajoute également un coût à `worldMicros/totalMicros`.

Exercices réellement couverts : rotations et déplacements interpolés,
sprites et flammes animées, porte ouverte/fermée en plusieurs frames,
dialogues avec continuation script, pickups, attaques de caisses,
SFX et musique, page HUB, EXIT et retour au menu **deux fois**.
Les 2 `[RESIDENTRESET]` vident intégralement la session
(`before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0`).
Deux `[SYSEXIT] MENU-READY` publient le **même**
`frame=7a70d304`. Les invariants `shapeData=0x0` et
`mediaTexels=0x0` restent observés aux jalons session.

**Audio :** les 11 `ENTRANCEMUSIC STREAM` indiquent
`underflows=0 recoveries=0 slowReadsOver20ms=0`; les six
`ENTRANCEMUSIC FINAL` ont `underruns=0 recoveries=0`.
Les six `AUDIOINTENT AUDIT` ne comptent aucun overflow,
`consumerCursorGap` ni SFX audible perdu. Pas de panic, WDT,
Guru Meditation ou Backtrace détecté.

**RAM :** `[RAMBUDGET] RESERVE advisory=HEADROOM_OK` sur les
deux sessions, marges heap8 respectivement **13 240B et 11 976B**,
`largest8=31 732B` au gameplay musical ; à menu inactif après
l'arrêt audio `heap8=115 256B largest8=86 004B` en fin de log,
sans dérive visible. `cacheBytes=0` signifie **pas de cache
permanent P1** ; la sonde ajoute seulement de petits compteurs
transitoires dans `FirstFrameWork`.

**Verdict :** **PASS matériel fonctionnel borné**, **gain CPU et parité
inter-build PENDING**. 3/33 réemplois rendent le gain probable
modeste ; ne pas qualifier ce jalon de « optimisation prouvée »
avant comparaison ON/OFF. Ne pas merger cette PR draft.

---

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

## Protocole CYD : ON validé fonctionnellement, A/B OFF encore en attente

Firmware normal : `cd ESP32 && pio run -e esp32-cyd -t upload -t monitor`.
Conserver Serial complet sur ≥10 images, rotations dans les deux sens,
portes animées, combats/pickups, EXIT/MENU et si possible LOAD V11.
Observer `[PERFPORT] PALETTE` et `[TURNFRAME]`, les FNV monde/frame à pose
égale, les compteurs de mur, la RAM libre / largest8 / minHeap8, la
musique et les audits AudioIntent, et `shapeData/mediaTexels=NULL`.

Pour l'A/B, **deux vrais environnements PlatformIO indépendants**,
sans éditer le source ni modifier les flags à la main :

```sh
cd ESP32

# A : optimisé (ON, normal 160x120, déjà validé fonctionnellement)
pio run -e esp32-cyd -t upload -t monitor

# Quitter le moniteur avec Ctrl+C avant le flash suivant.
# B : témoin (OFF, héritage exact du normal + une seule macro)
pio run -e esp32-cyd-perf-off -t upload -t monitor
```

**Attention :** chaque commande téléverse un nouveau firmware et écrase
le précédent. Pour un test ciblé Sector 1, utiliser **le même slot V11**
et un parcours comparable (rotations dans les deux sens, mouvements,
porte et combat) sur les deux variantes ; conserver deux logs séparés.
Conserver strictement la même carte SD et le même PAK.

**A :** `esp32-cyd` hérite du défaut source
`DOOMRPG_PERF_PORT_PALETTE_REUSE=1` ; en Entrance on a observé
`catalogHits=3 pakReads=30` pour `resolved=33`.
**B :** `esp32-cyd-perf-off` hérite de la totalité des flags
`esp32-cyd` (dont `PERF_PORT_PROFILE=1`, `RA2B_CAPTURE=1`),
ajoute **uniquement** `-D DOOMRPG_PERF_PORT_PALETTE_REUSE=0`
et le chemin local de dépendance TFT_eSPI nécessaire à son build
standalone. Pour Entrance et 33 textures, on **attend**
`catalogHits=0 pakReads=33` ; ce n'est pas une mesure CYD acquise.

La CI compile les deux firmwares, lit le vrai
`compile_commands.json` du profil OFF, vérifie que le renderer C
est bien compilé avec `REUSE=0` et `PROFILE=1`, et publie les deux
firmwares binaires. **Ne pas confondre cet oracle compile avec un test
matériel.**

Comparer prioritairement les distributions de `resolveUs` à phase
et carte comparables, en isolant les rendus froids/chauds. Confronter
`worldUs` avec prudence à cause du Serial. Comparer
`viewport`/`world` FNV uniquement à mêmes position, angle,
animation et mutations de la carte (les animations sont temporelles).
Contrôler `heap8/largest8`, `HEADROOM_OK`, musique et audio.
Si `resolveUs` ne s'améliore pas de façon répétable, aucun gain
CPU ne sera annoncé : candidat à abandonner/revert. La parité
inter-build et la stabilité matérielle du profil OFF restent en attente.

**PASS fonctionnel CYD documenté ; A/B CPU et parité visuelle ON/OFF toujours PENDING.** Pas de merge automatique.
