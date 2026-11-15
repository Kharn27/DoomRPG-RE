# PR #341 / issue #340 — R-A2b : capture FNV 160×120 sur CYD, **EN ATTENTE TEST MATÉRIEL**

**15 novembre 2026 — candidat source** `f1e7e32ed0a8d7c1db5c28130281a041e63bfed6` sur `agent/esp32-arch-ra-real-span-oracle`, base GitHub `main=839cc5440a22d6c0a0310bd5bdeeb2dbd674a6c1`. Aucun merge. Suite bornée de [l'audit R-A2a des vrais fichiers](ARCH_RA2A_ORIGINAL_ASSETS_VALIDATION.md) : on crée un corpus de poses complètes vérifiable avant toute extraction de sampler (R-B). **Il s'agit de golden FNV de sortie native, pas d'une preuve de parité contre l'image du renderer desktop.**

## Modifications et invariants

- `ESP32/src/esp_native_gameplay_frame.c` : sonde `DOOMRPG_RA2B_CAPTURE=1`, **48 échantillons maximum par processus**, uniquement après **renderComposed réussi, publié, canonique, cardinal, non-preview, sans automap**. Une ligne `[RA2B] FRAME v=1` après le chronométrage de la frame et sa présentation physique. Aucun second framebuffer, aucune écriture de pixel, aucune lecture PAK supplémentaire, aucune mutation d'entité, script, RNG ou sauvegarde.
- `ESP32/src/esp_asset_pack.cpp`, `include/esp_asset_pack.h` : compteur diagnostic 32 bits de temps `sdRangeUs` mesuré autour de la paire **seek + read** sur les *cache misses* SD dans `EspAssetPack_readRange()`, si mode résident actif ; les hits de cache n'ajoutent pas de temps. Le compteur `flashReadUs` utilise la mesure existante autour de `esp_partition_read()` : **ne jamais appeler ces chiffres « temps microSD » quand `backing=flash`**. Les temps SD exclus : index, démarrage, ouverture, sourceProbe ; les temps flash exclus : overhead d'index et de dispatch.
- Le compositeur capture les deltas `physicalReads/physicalBytes/rangeHit/rangeMiss`, les durées monde/sprites/HUD/present/total et les FNV *déjà calculés* `full/viewport/world/hud` ; tous les champs sont visibles dans la même ligne. `full` désigne le framebuffer logique **160×120 RGB565 (38 400 octets)** en ordre machine ESP32, pas l'image TFT resamplée ni les overlays pouvant être ajoutés plus tard par d'autres propriétaires.
- `ESP32/platformio.ini` active temporairement la sonde dans le profil **normal `esp32-cyd`**, qui sert de référence matériel ; les profils trace/bringup héritent des flags mais ne servent pas de référence RAM. Code protégé par `#if` : désactivation ultérieure sans changement d'API gameplay.
- `ESP32/test/ra2b_frame_capture_host.py` : parseur hôte, exige `ENGINESESSION READY`, rejette les images de *priming* précédant READY, exige **au moins 3 poses distinctes**, ordre `seq` strict, format v1 strict, hashes non nuls, durées bornées, cohérence `backing=sd/flash`. Produit un JSON **métadonnées uniquement** après vérification SHA256 du PAK d'origine R-A2a ; `--compare` compare toutes les poses/FNV/comptages, **pas** les temps (non déterministes).
- L'oracle historique `sdl_f27_native_gameplay_header_host.py` restaure **exactement** le C avant F27 après avoir retiré les quatre blocs diagnostiques bien délimités ; aucun affaiblissement des neuf autres sources, aucune régression silencieuse du corps historique.

Le firmware conserve `shapeData==mediaTexels==NULL`, PAK natif uniquement sur SD/partition vérifiée, cache 3 × 2048 octets, framebuffer unique, zéro SDL/ZIP runtime, et les mêmes 45 `--wrap` normaux. Il n'y a **aucune optimisation fonctionnelle ni remplacement de sampler** dans ce commit.

## CI du code candidat (sans contrôle matériel inventé)

- [PR 43734342617](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43734342617) : **SUCCESS 168/168**.
- [push 43734337269](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43734337269) : **SUCCESS 168/168**.
- Les trois firmwares PlatformIO compilent. Normal : **RAM statique 80736 B, flash 861089 B** (la référence F31 précédente était 860177 B ; **+912 B de flash diagnostic**, pas de gain RAM revendiqué).
- Test parseur hôte `--self-test` : 3 poses synthétiques, priming ignoré, hash falsifié détecté, timings variables admis, mauvais backing / séquences corrompues / trop peu de poses refusés. Test indépendant sur l'ancien vrai log `platformio-device-monitor-261115-103040.log` : **échec attendu**, car l'ancien firmware ne contient aucune ligne RA2B ; ce n'est pas un test de panne matérielle.
- Les fichiers privés `DoomRPG.zip` et `DoomRPG-ESP32.pak` **ne sont pas ajoutés à Git**. L'oracle C exact `--pak --zip --expect-original` de R-A2a sur ces uploads reste à exécuter par un poste avec le dépôt accessible.

## Recette stricte de test utilisateur — classic CYD, firmware normal

1. Sur la branche de la PR #341 : compiler/flasher **`pio run -e esp32-cyd -t upload`**. Repartir d'un vrai reboot (le script `ESP32/monitorCyd.sh` capture le boot et enregistre `ESP32/logs/`).
2. **Start Game → Entrance**, après le log `[ENGINESESSION] READY`, effectuer **4 rotations cardinales** et idéalement **1 déplacement/strafe suivi d'une rotation**, en gardant une zone de décor relativement stable, sans dialogue, feedback d'attaque ou automap en cours. Nous cherchons 3+ couples `map,x,y,angle` distincts post-READY. Les frames de préparation sont volontairement ignorées. Aucun scénario de gain RAM n'est revendiqué.
3. Capturer le Serial complet jusqu'à retour menu ; vérifier absence de `NATIVEFRAME FAILED`, panic, WDT, échec de garde, erreur PAK ou rupture de `shapeData/mediaTexels=NULL`. Relever `HEAP8`, `HEADROOM_OK`, nombre de RA2B frames après READY et comparaison backing/latence.
4. Depuis la racine du repo, après le test et avec la **copie locale privée** du PAK :

```sh
python ESP32/test/ra2b_frame_capture_host.py \
  --log ESP32/logs/platformio-device-monitor-XXXXXX.log \
  --pak /chemin/DoomRPG-ESP32.pak \
  --out /tmp/entrance-ra2b-golden.json
```

Pour comparer **plus tard**, sur même assets, même scénario, même thème / état du monde, relancer avec `--compare /tmp/entrance-ra2b-golden.json` et le nouveau log. Le parseur refuse d'ignorer des poses manquantes ou une divergence de hash ; il n'annonce **pas** une équivalence face à `src/Render.c` sur des frames de 160×120.

## Critères du prochain PASS / limites

**EN ATTENTE log Serial matériel.** Les CI prouvent la compilation, les oracles sur données synthétiques et la régression du corps C ; elles **ne prouvent pas** le fonctionnement sur vrai CYD, les mesures I/O réelles, la stabilité d'une animation temporelle, la parité desktop full-frame ni un gain de performances. Après les vraies captures, analyser les poses et coûts ; documenter les résultats uniquement après PASS. **R-B reste bloqué sur le corpus golden validé, la fermeture de l'oracle C original, et l'absence d'erreur sur la cible.**

Ne pas comparer des FNV de frames avec états de monstres, thème/HUD, animations ou effets différents : mêmes coordonnées ne garantissent pas un monde identique.
