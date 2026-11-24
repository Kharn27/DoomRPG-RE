# RES60-P24 — préflight du viewport monde 160/192 dans le renderer permanent (CI / CYD PENDING)

Branche expérimentale \`agent/esp32-res60-p1\`, PR #364 **DRAFT**. Base synchronisée : \`main bb774e434b75f84d95cf0d6b7b0f108f7e04e6ce\`. P23 était validé en double CI, pas en matériel actif. Dernier firmware de preview réellement testé : P21.

## Frontière exacte

P24 ajoute dans le contrat raster permanent \`EspNativeRasterWorldViewport\`, un descriptor scalaire possédé par l'appelant. \`EspNativeRasterContract_prepareWorld\` valide **avant toute allocation, PAK et mutation de pixels** le mode explicite, les octets exacts du framebuffer, le pitch RGB565, la taille monde et la capacité du scratch colonnes. Il calcule \`worldFirstPixel\` à partir de la vraie géométrie, sans fixer en dur le HUD ni créer un autre framebuffer.

- Classic : 160×120 (38 400 octets), monde 160×80 à y=20, origine pixel 3200, pitch 320 octets.
- RES60 : 192×144 (55 296 octets), monde 192×96 à y=24, origine pixel 4608, pitch 384 octets. Admission possible uniquement si la capacité P23 vaut 192.
- Tous les rejets sont **fail-closed**, le descriptor \`out\` demeure inchangé.

Le **vrai moteur permanent** \`esp_native_first_frame.c\` utilise le preflight Classic, son origine pour la peinture sol/ciel de fond et \`render->pixels\`, et rejette la source avant l'ouverture PAK si son pitch ou sa taille sont incohérents. Le reste du raster (BSP/murs/texels) est intact. Le chemin 192 ne s'exécute pas : \`renderFrame\` appelle toujours explicitement \`ESP_NATIVE_RASTER_CLASSIC\` ; les gardes 160 du coordinateur gameplay et du propriétaire de plans restent en place, et \`setup()/loop()\` RES60 ne démarre pas le gameplay.

## Oracles et critères

\`ESP32/test/res60_p24_world_viewport_host.py\` compile **le header C11 réel** deux fois avec les drapeaux issus de PlatformIO : Classic capacité 160 et RES60 capacité 192. Pour les deux modes, il peint un tableau synthétique RGB565 avec un oracle indépendant, compare le framebuffer complet, les bandes HUD et les canaries, vérifie les coordonnées réelles de chaque ligne, et rejette les tailles, pitchs, hauteurs, largeurs, modes et capacités invalides **sans écriture dans \`out\`**.

SDL-F24 conserve ses empreintes Git immuables du véritable C via l'inverse strict des six modifications P24 ; la restauration P23 historique continue d'être strictement vérifiée. Les CI des profils normaux et des profils RES60 restent exigées.

**Aucune preuve pixel 192 réelle, BSP texturé 192, plans, sprites, arme, HUD, audio, RAM réelle ou CPU n'est revendiquée.** Pas de nouvelle voie 192 active, donc pas de flash CYD P24 nécessaire. Après CI, prochaine frontière : adapter l'admission du vrai propriétaire des plans et obtenir la parité 160 tout en préparant 192, sans activation gameplay avant que les autres propriétaires soient prêts. Ne pas fusionner #364 dans main.
