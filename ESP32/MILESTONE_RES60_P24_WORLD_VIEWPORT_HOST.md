# RES60-P24 — préflight du viewport monde 160/192 dans le renderer permanent (double CI PASS / CYD PENDING)

Branche expérimentale `agent/esp32-res60-p1`, PR #364 **DRAFT**. Base synchronisée : `main bb774e434b75f84d95cf0d6b7b0f108f7e04e6ce`. P23 était validé en double CI, pas en matériel actif. Dernier firmware de preview réellement testé : P21.

## Frontière exacte

P24 ajoute dans le contrat raster permanent `EspNativeRasterWorldViewport`, un descriptor scalaire possédé par l'appelant. `EspNativeRasterContract_prepareWorld` valide **avant toute allocation, PAK et mutation de pixels** le mode explicite, les octets exacts du framebuffer, le pitch RGB565, la taille monde et la capacité du scratch colonnes. Il calcule `worldFirstPixel` à partir de la vraie géométrie, sans fixer en dur le HUD ni créer un autre framebuffer.

- Classic : 160×120 (38 400 octets), monde 160×80 à y=20, origine pixel 3200, pitch 320 octets.
- RES60 : 192×144 (55 296 octets), monde 192×96 à y=24, origine pixel 4608, pitch 384 octets. Admission possible uniquement si la capacité P23 vaut 192.
- Tous les rejets sont **fail-closed**, le descriptor `out` demeure inchangé.

Le **vrai moteur permanent** `esp_native_first_frame.c` utilise le preflight Classic, son origine pour la peinture sol/ciel de fond et `render->pixels`, et rejette la source avant l'ouverture PAK si son pitch ou sa taille sont incohérents. Le reste du raster (BSP/murs/texels) est intact. Le chemin 192 ne s'exécute pas : `renderFrame` appelle toujours explicitement `ESP_NATIVE_RASTER_CLASSIC` ; les gardes 160 du coordinateur gameplay et du propriétaire de plans restent en place, et `setup()/loop()` RES60 ne démarre pas le gameplay.

## Oracles et critères

`ESP32/test/res60_p24_world_viewport_host.py` compile **le header C11 réel** deux fois avec les drapeaux issus de PlatformIO : Classic capacité 160 et RES60 capacité 192. Pour les deux modes, il peint un tableau synthétique RGB565 avec un oracle indépendant, compare le framebuffer complet, les bandes HUD et les canaries, vérifie les coordonnées réelles de chaque ligne, et rejette les tailles, pitchs, hauteurs, largeurs, modes et capacités invalides **sans écriture dans `out`**.

SDL-F24 conserve ses empreintes Git immuables du véritable C via l'inverse strict des six modifications P24 ; la restauration P23 historique continue d'être strictement vérifiée. Les CI des profils normaux et des profils RES60 restent exigées.

**Aucune preuve pixel 192 réelle, BSP texturé 192, plans, sprites, arme, HUD, audio, RAM réelle ou CPU n'est revendiquée.** Le mode 192 n'est toujours pas actif. **En revanche P24 modifie la voie de préflight/fond du renderer Classic : un smoke-test matériel normal `esp32-cyd` est requis avant PASS CYD**, même si les oracles hôtes sont verts. Après CI, prochaine frontière : adapter l'admission du vrai propriétaire des plans et obtenir la parité 160 tout en préparant 192, sans activation gameplay avant que les autres propriétaires soient prêts. Ne pas fusionner #364 dans main.

## Résultat CI du code (24 novembre 2026)

**Code candidat exact `c720362354e3ec13b62da56a29ba55f57b04eef7`**. Deux workflows **SUCCESS, 265 étapes chacun** : [push #45390126890](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45390126890) et [PR #45390138194](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45390138194). L'oracle C11 P24, la reconstruction exacte SDL-F24, la suite historique et les builds PlatformIO classic/inversé/RES60 passent. Le commit de correction `c720362` modifie uniquement l'inverse Python SDL-F24, pas le C du candidat `c87f7b3`. Aucune mesure du vrai CYD, aucun gain CPU/RAM et aucune image 192 réelle ne peuvent en être déduits.

**Protocole matériel recommandé** : flasher le profil normal `esp32-cyd` de cette branche, START→Entrance, plusieurs rotations/mouvements et éventuellement EXIT→MENU→LOAD V11 ; capturer le Serial en entier, surveiller `HEADROOM_OK`, `shapeData/mediaTexels=NULL`, absence de WDT, pixels/HUD et rafraîchissements. Ne pas transférer les FNV P21 RES60 pré-thème #451 au build Classic actuel. Test optionnel `esp32-cyd-res60-inverted` uniquement pour confirmer la non-régression du preview, pas pour attester du gameplay 192. Après PASS matériel seulement : freeze du code, docs-only, CI finale et continuation P25.
