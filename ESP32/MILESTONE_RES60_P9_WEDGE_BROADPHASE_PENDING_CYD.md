# RES60-P9 — index BSP conservateur par cône cardinal (PASS CYD borné)

**Dépôt** `Kharn27/DoomRPG-RE`; [PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), branche `agent/esp32-res60-p1` ; base rebasée `main=88c1e58d85001f6cbb26a7b7bee5fea43a37e7cf` le 20 novembre 2026. **PASS matériel CYD le 21 novembre 2026** (test utilisateur, attribution par les tags plutôt que par SHA firmware intégré au Serial).

## Résultat matériel vérifié — 21 novembre 2026

Capture utilisateur `platformio-device-monitor-261121-103834.log` (19 946 octets, SHA256 `818914f285c7f84b724cdf2ffc2c7c382b9545062fd57b2df51661ec157bee18`). Vrai CYD D0WD-V3 à 240 MHz, flash 4 Mio, PSRAM=0. `192x144` RGB565 55 296 B / nearest 5:3. START double-tap, quatre RIGHT, trois LEFT et retour MENU. `MAP_INTRO` : CRC `623f34e4`, arène 14 095 B, FNV `c3882516`, map libérée/PAK fermé sur les huit frames.

* Angle 64 : candidats 346/480, rayTests 66 432/92 160, `rawP3FNV=ed9b68c4`. Frame chaude raster 367 146 µs ; premier START froid raster 1 720 360 µs (non représentatif de l'index seul).
* Angle 0 : candidats 120, rayTests 23 040, `rawP3FNV=48ae8442`, raster 276 616/276 627 µs.
* Angle 192 : candidats 8, rayTests 1 536, `rawP3FNV=2697640d`, raster 232 138/232 031 µs.
* Angle 128 : candidats 41, rayTests 7 872, `rawP3FNV=f43a2791`, raster 245 292/245 227 µs.

Pour chaque orientation, `SWATCH sampleFNV=5503bc26`, 1 MISS (`2052` octets) et 7 HIT (`0` octet). Dernières frames `heap8=135852`, `largest8=86004`, `minHeap8=108860`, aucune erreur/panic/WDT ; retour MENU `fnv=c093a13b`. Temps de présentation ~36,9 ms sur toutes les images. Les données établissent un **PASS du broadphase et de l'absence de dérive observée**, pas la fidélité colorimétrique, le gameplay, les sprites ou des textures projetées sur les murs. Après capture, ne modifier que les docs dans ce jalon.

## Raison et frontière

Après le PASS CYD du cache de texels P8 (~1 868 ms → ~463 ms par rotation), le raycaster P3 continue **192 × 480 = 92 160** tests d'intersection par image. P9 accélère uniquement la pré-sélection de segments immuables avant les intersections. Le portage 160×120 reste inchangé ; P7 quatre angles, P8 swatch `logical=87 / sampleFNV=5503bc26`, menu UAC en production, pas de gameplay 60 %, pas de redraw en boucle.

Le projet continue à inventorier `/intro.bsp` depuis `/DoomRPG-ESP32.pak`, charge l'arène compacte de 14 095B, réalise la preflight complète de toutes les lignes puis libère la carte. **Une seule table statique `uint16_t wedgeIndices[512]` = 1024B BSS** dans le firmware RES60 P9, utilisée uniquement pendant un appel synchrone du projecteur. Ni index map-wide permanent, ni tableau `EspRes60PreviewLine[480]`, ni framebuffer supplémentaire, ni malloc.

Pour chaque segment, on transforme ses deux extrémités en coordonnées (`forward,lateral`) selon la direction source originale Doom (0 E, 64 N, 128 W, 192 S). Les 192 rayons du modèle P3 sont contenus dans le cône convexe défini par `forward>0` et `-191*forward <= 192*lateral <= 191*forward`. Un segment est exclu uniquement si ses deux extrémités sont entièrement **hors du même demi-plan** (derrière la caméra ou strictement au-delà du bord gauche/droit). Toute intersection potentiellement visible est conservée, les égalités aux frontières restent admises ; les indices restent dans l'ordre original BSP, donc même priorité pour les profondeurs ex aequo. `rayHit`, profondeur, palette plate, colonnes et FNV ne changent pas.

**Limites et sécurité** : source `lineCount<=512` (`MAP_INTRO=480`), lecture/validation de **toutes** les lignes avant le moindre pixel, `wedgeCandidates+wedgeRejected=lineCount`, stats `rayTests=192*wedgeCandidates`. En cas de map trop grande, source invalide ou reader échoué : fail closed, pas d'image présentée. Ce prototype n'est pas un BSP traversal ni un renderer 3D permanent, et n'apporte pas encore de UV/texture 3D/sprites.

## CI et preuves attendues

`ESP32/test/res60_p9_wedge_host.py` compile exactement **deux versions du même `esp_res60_world_preview.c`**, P9 désactivé (oracle P3) et P9 activé (nouvel index), puis compare 256 images sur quatre angles avec cadres de murs et segments originaux synthétiques (diagonales, zéros, spéciaux, bords). Comparaison **octet par octet des 55 296B** + canaries, FNV, distance/segment central, intersections, pixels de mur, compteurs d'admission, rejet source malformée et borne >512 ; non-recours à P9 dans le profil jouable. Compilations normales/inverted/RES60 dans la CI. Vérifier le statut exact après les runs.

## Protocole hardware P9, panneau alternate

```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```

Carte SD : `/DoomRPG-ESP32.pak` natif inchangé. Deux appuis START avec relâchement, puis RIGHT au moins quatre fois, LEFT, MENU. Contrôler `[RES60P9] WEDGE lines=480 candidates=... culled=... rayTests=... legacyRayTests=92160` et comparer `FRAME totalUs` au **~463 ms** de P8 avant P9. Les FNV géométriques exacts doivent être **angle64=`ed9b68c4`, 0=`48ae8442`, 192=`2697640d`, 128=`f43a2791`** ; la swatch réelle garde `sampleFNV=5503bc26`, `cache=HIT bytes=0` après START. Consigner `heap8`, `minHeap8`, `mapRuntime=RELEASED`, `pak=closed`. Vérifier que menu P6 revient sans corruption. Photo appréciée pour vérifier la vignette, mais un retour visuel n'a pas encore été fourni.

**Validation visée** : pixel-oracle inchangé + baisse des `rayTests` + gain temporel mesuré sur vrai CYD, sans régression RAM. Ne jamais déduire le gain de la seule CI, ne pas fusionner avant test matériel.
