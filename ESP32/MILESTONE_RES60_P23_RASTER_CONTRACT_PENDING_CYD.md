# RES60-P23 — premier contrat raster du renderer natif permanent (CI/CYD PENDING)

**Branche `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364).** Main de référence synchronisé au vrai `bb774e434b75f84d95cf0d6b7b0f108f7e04e6ce` (thème #451) par merge non-force `1f4ea4927eabf526b4b499c4986079aad5bcf3a4`. L'ancien FNV final RES60 P21 provenait du thème précédent ; ne pas l'utiliser comme oracle de pixels de `main` actuel sans nouvelle mesure. L'oracle géométrique BSP brut ne dépend pas de la palette.

## Frontière P23

Le P22 a montré que `ESP32/src/esp_native_first_frame.c` est le premier moteur permanent susceptible de produire un viewport 192 colonnes, mais qu'il garde un tampon de `MAX_SCREEN_WIDTH=160` colonnes, et que `esp_native_gameplay_frame.c`, le renderer de plans, les sprites et le HUD sont strictement 160×120. P23 démarre **uniquement le contrat de géométrie du renderer permanent**. Il ne déverrouille aucun autre consommateur.

Nouveau header permanent `ESP32/include/esp_native_raster_contract.h`, **sans heap/BSS, allocation, PAK ni framebuffer** : `EspNativeRasterContract` est un petit descriptor scalaire explicitement possédé par son appelant, pour exactement deux modes :

| Mode | Frame RGB565 | Stride pixels | World | HUD | Frame bytes |
|---|---:|---:|---:|---:|---:|
| `ESP_NATIVE_RASTER_CLASSIC` | 160×120 | 160 | 160×80 @ 0,20 | 20+20 | 38 400 |
| `ESP_NATIVE_RASTER_RES60` | 192×144 | 192 | 192×96 @ 0,24 | 24+24 | 55 296 |

La sélection, la validation et l'adresse d'une ligne `worldRow` se font sans modification de l'image et refusent une taille de buffer, un pitch, une marge ou un mode anormal. La capacité de colonnes `ESP_NATIVE_RASTER_COLUMN_CAPACITY` vaut 160 en production et 192 **uniquement si** `DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1` est activé dans `esp32-cyd-res60*` (prérequis P21/P18/P1 ; 0, 2 ou dépendances manquantes refusées). **Le vrai renderer** `esp_native_first_frame.c` utilise désormais cette capacité pour son `RenderScratch.columnScale`; le coordinateur de frame `esp_native_gameplay_frame.c` utilise la constante de HUD Classic au lieu de dupliquer 20. Les valeurs de production restent strictement 160 et 20.

**Important : P23 n'autorise PAS le rendu de gameplay 192.** Le `setup()/loop()` RES60 continue de s'arrêter avant l'initialisation du jeu ; le coordinateur exige toujours 160×80 @ 0,20, les plans 160×80, les sprites et HUD 160×120. Le chemin `EspNativeFirstFrame_renderGameplayViewport` n'est pas invoqué dans le laboratoire. Augmenter de 128B **la taille possible du scratch de colonnes du renderer expérimental** ne veut pas dire qu'il a été alloué ou exercé sur le CYD. Zéro changement au type `Render_t`, aux textures, aux pixels du preview, à la PAK lease ou au LCD.

## Oracles CI

`ESP32/test/res60_p23_raster_contract_host.py` compile le **vrai header C11** en deux configurations : production (cap 160) et laboratoire P23 (cap 192, vrais flags de PlatformIO). Teste strictement les deux descripteurs, 80/96 lignes et leur pitch, les 20/24 lignes de chacune des deux bandes HUD, tous les 38 400/55 296 octets d'un framebuffer synthétique et les canaries, bornes de colonnes 160/192, rejets de toutes géométries falsifiées, flags P23 isolés, fail-closed compilation, et présence de l'API dans les **sources permanentes**. En complément les suites historiques SDL et les deux builds CYD restent obligatoires. Le test P22 adapte son témoin `MAX_SCREEN_WIDTH` à la nouvelle capacité, sans affaiblir les gardes 160 du coordinateur.

## Protocole

1. Double CI P23 (push + PR), builds normaux/inversés/RES60 ; exigence 0 échec, sans benchmark sur hôte.
2. Si CI PASS, **P23 reste host/compile seulement**, sans test de rendu matériel : la voie active RES60 est encore le preview P21, unchanged côté framebuffer. Les FNV P21 post-thème #451 ne sont pas prédits.
3. Prochaine étape **P24** : exposer réellement la préparation de viewport 192 du renderer monde (sol/plafond et murs source PAK), avec oracles des frames 160 existantes et des limites exactes; n'activer aucun gameplay, sprite, arme, HUD ou audio tant que les propriétaires dépendants ne sont pas prêts.

**Aucune revendication de gameplay, de gain CPU, de mémoire libre ou de conformité pixel-world 192 par P23.** Reste DRAFT ; jamais merger à main sans demande explicite.
