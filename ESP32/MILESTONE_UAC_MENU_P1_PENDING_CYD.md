# UAC-MENU-P1 — prévisualisation expérimentale 120 / 240 — **EN ATTENTE CYD**

Branche : `agent/uac-menu-preview`, base GitHub `b1c6660d55f39623c4e24d874b049cd9b452ad47` (main après #399).
Ce document contient **l'architecture et le protocole prévus**, pas une validation sur le vrai CYD. Ne pas convertir en PASS avant logs Serial et photos.

## Ce qui change

- Deux formats BMP V4 BGRA32/BI_BITFIELDS 32 bits, masques R/G/B/A contrôlés, dimensions et taille payload exactes, rangées bottom-up, alpha straight vers RGB565 et clipping borné.
- Lecteur ESP32-native basé sur `SD.open` / `File.seek` / `File.read`. Préflight des **9 assets** nécessaires à chaque taille (cadre, quatre actifs, quatre inactifs), puis ouverture des **5 fichiers de l'état courant** (cadre + 4 boutons), et fermeture explicite sur chaque sortie. Pas de ZIP runtime ni chargement d'un BMP entier.
- Scanlines statiques : `uint16_t[320]` (640 octets) + `uint8_t[158*4]` (632 octets), **1 272 octets BSS** de buffers d'image, indépendants du framebuffer RGB565 160×120 de 38 400 octets. Le préflight SHA256 utilise un chunk borné de 256 octets de pile. Les handles `File` sont temporaires, la RAM exacte du CYD reste à mesurer.
- Affichage : logo original 102 pixels logiques refitté à gauche, en **162 pixels physiques** ; UAC cadre à droite, en **158 pixels physiques**, x=162..319. Fonds indépendants ; aucun chevauchement avec le rail classique 54 px.
- **Mode 120** : cadre `79×120`, boutons `55×20`, pixel exact ×2 nearest-neighbor vers le LCD. **Mode 240** : cadre `158×240`, boutons `110×40`, pixel direct vers LCD **sans downscale en 160×120**. Les deux modes partagent le même rectangle physique et le même rendu de hauteur totale. Le mode 240 est une couche expérimentale sur LCD au-dessus du pipeline de présentation normal ; **le framebuffer gameplay reste toujours en 160×120 x2**, inchangé.
- Dessiner les quatre boutons puis le cadre au-dessus, comme les compositions de `artwork/menu-uac/README.md`. Sélection active unique héritée du menu natif, pas une seconde copie du modèle.
- Toucher le **logo gauche** : cycle `CLASSIC → 120 → 240 → CLASSIC`. Toucher START/LOAD/OPTIONS/HELP à droite : un tap pour sélectionner/activer, un second tap après relâchement pour valider, **même dispatcher existant**. Un changement de mode réinitialise l'armement en attente. LOAD et toutes les transitions abandonnent l'overlay.
- Chaque faute de lecture d'un asset ou préflight invalide repasse à **CLASSIC** et remet à l'écran le framebuffer original. Pas de lecture SD ni modification des actions quand mode classic=0.

## Géométrie physique comparée

| Sélection | Bouton UAC 120 ×2 | Bouton UAC 240 direct | Hitbox physique |
|---|---|---|---|
| START | (186,20) 110×40 | (186,20) 110×40 | x186..295, y20..59 |
| LOAD | (186,74) 110×40 | (186,74) 110×40 | x186..295, y74..113 |
| OPTIONS | (186,128) 110×40 | (186,128) 110×40 | x186..295, y128..167 |
| HELP | (186,182) 110×40 | (186,182) 110×40 | x186..295, y182..221 |

Comparer avec les boutons de l'interface classique : **108×48 pixels physiques**. Le panneau UAC est plus large mais ses boutons sont 8 px moins hauts ; ne pas considérer la tactilité acquise avant essai réel.

## Tests CI, limites

- Test host du vrai parser `ESP32/src/esp_uac_bmp32.c` : BMP V4 valide/invalide, champs incohérents, orientation, alpha 0/128/255, clipping, dimensions 120/240, read-at refusé, hitboxes/indices et fermeture simulée à chaque chemin de préflight.
- Tests CI d'intégrité : les **18 empreintes** du source C++ doivent correspondre aux 18 BMP finals et au manifeste source ; une mutation de pixel de chaque BMP est rejetée par SHA-256. La compilation PlatformIO normale est lancée par la CI sur `agent/**`. Ni la CI ni l'aperçu du README ne prouvent la lisibilité du texte `OPTIONS` sur LCD.
- Chaque activation d'un mode valide les **9 SHA-256 du payload exact**, en plus de la structure BMP : empreintes des 18 fichiers issues de `artwork/menu-uac/SHA256SUMS.txt`, compilées en lecture seule, comparaison en parcours SD par chunks de **256 octets de pile** (`mbedtls_sha256_*`). Une corruption d'image conservant la même taille échoue aussi et active le fallback classic. Le coût de ce scan est limité aux changements de mode ; sa latence réelle reste à mesurer sur le CYD.
- Le rendu est volontairement une **prévisualisation physique réversible** ; il n'implique aucun remplacement du système original ni résolution 240 généralisée au gameplay.

## Protocole à exécuter sur le vrai CYD classic (normal esp32-cyd)

1. Copier les 18 BMP dans **`/doomrpg/img/`** sur la SD (aux côtés de `credits.bmp`) ; conserver `/DoomRPG-ESP32.pak`. Flasher la branche avec `platformio run --environment esp32-cyd --target upload --target monitor`.
2. Photographier le menu **CLASSIC** en éclairage et distance fixes, puis toucher le logo gauche et photographier le mode **120**, avec START actif. Toucher les trois autres boutons une fois chacun et photographier **OPTIONS actif**, en particulier le lettrage ; pas besoin de confirmer si l'on veut rester sur le menu.
3. Toucher le logo une seconde fois, vérifier `[UAC] MODE 240` puis photographier le mode **240 direct**, START et OPTIONS, **sans le confondre avec 120×2**. Toucher encore le logo : retour CLASSIC. Vérifier qu'aucun overlay n'est resté.
4. Depuis UAC, vérifier le double-tap START, LOAD, OPTIONS, HELP et leur **retour** (LOAD sans sauvegarde aussi), puis la reprise normale du gameplay. Vérifier que la tactile UAC ne répond pas dans les interstices ou sur l'ancien rail invisible.
5. Sur une copie de SD, **avant l'allumage**, renommer temporairement un fichier BMP UAC (ou abîmer son en-tête), vérifier `[UAC] PREFLIGHT FAIL` + `FALLBACK` et que le menu classique reste tactile et fonctionnel. Restaurer le fichier ensuite. Ne pas retirer la SD pendant une transaction.
6. Fournir **logs Serial complets, photos CLASSIC / 120 / 240** (zoom OPTIONS, ergonomie des boutons), ressenti lisibilité et latence. Vérifier absence de panic/WDT, transitions, `shapeData=NULL`, `mediaTexels=NULL`, RAM, et qualité visuelle.

**Statut matériel : PENDING.** Aucune affirmation de PASS LCD, tactile, stabilité ou RAM mesurée avant ces preuves.

## Retest suite au CYD — correctif FD et latence (19 novembre 2026, PENDING)
La capture utilisateur prouve l'échec `vfs_fat: open: no free file descriptors` lors de l'ouverture du cinquième BMP pendant la musique, puis le fallback. Après fermeture du flux musical : `PRESENT OK` 120 = 827–830 ms, 240 = 2 535–2 537 ms ; heap8 119796, largest 86004 octets. 240 affiche plus net que 120, le cadre restant visuellement disproportionné (chantier graphique séparé).

Correctif : 2 BMP simultanés maximum (frame et bouton du secteur), batches contigus de 4 lignes physiques lus bottom-up avec décodeur borné, TFT écriture par bandes, **7480B BSS** (contre 1272), sans framebuffer 320×240 ni PSRAM. Mesures `sdMs` / `lcdMs` / `batches` et `buttonOpens`. Même composition et gate d'actions qu'avant ; tests host du véritable décodeur direct/batch et refus SD.

**PENDING CYD :** vérifier absence d'échec FD en musique, temps 120/240, heap8, santé audio sous préflight/dessin, tactilité, retour au classique et stabilité. Aucun PASS matériel revendiqué pour le correctif.

## UAC-P1 / AUDIO-MENU — coupures sur changement de bouton 240 (19 novembre 2026)

Retour matériel utilisateur : en 240 direct, la musique coupe pendant les changements de bouton. Le premier test avait mesuré 2535–2537 ms pour un FULL redraw ; le ring audio fait 16 KiB/22.05kHz, donc au maximum 743 ms sans recharge. Ce constat signale une famine probable de la production audio, **pas** une limite de la résolution 240. La version avec lecture par blocs (commit précédent) n'avait pas encore de mesure matérielle dans les logs reçus.

Correctif expérimental : lors d'une sélection dans le même mode, rafraîchir **uniquement les deux rectangles physiques 110×40** de l'ancien et du nouveau bouton, avec composition identique matte + bouton + frame (test host de parité pixels aux deux échelles). Un changement de mode reste FULL. Ajouter un `EspEntranceMusic_pumpMenuPreview()` **menu-only**, sans action gameplay ni PAK, après chaque transaction TFT terminée et toutes les 4096B du préflight SHA. Aucun pump au milieu d'une transaction TFT ; un seul producteur WAV et la même FIFO 16KiB, aucun second thread. Garder deux BMP simultanés maximum (plus l'unique WAV).

Les logs `[UAC] PRESENT` incluent désormais `paint=FULL/BUTTONS`, `rows`, `sdMs`, `lcdMs`, `audioPumps`, `audioBlocks`. Retester durant le WAV du menu, vérifier qu'aucun `underruns/recoveryStarts` ne progresse et que les boutons vieux/nouveaux ne présentent aucun artefact. Contrôler le mode 120, l'entrée/sortie du mode 240, les 4 touches et le fallback sans SD.

**PENDING matériel.** Aucun résultat de performance ni de son du nouveau code n'est prétendu avant les logs du vrai CYD.


## UAC quatre modes / rebase ARCH-RC19 — 19 novembre 2026 (PENDING CYD)

Rebase du code UAC sur main `f04f16ba73a8f9532f0bffdae8ed459f6711626a`, tests ARCH-RC19 (8 wrappers) conservés. Cycle logo : CLASSIC → authored BMP 120 ×2 → source BMP 240 décimée 2× en virtual-120 et présentée ×2 → BMP 240 direct → CLASSIC. Même logique tactile, mêmes quatre boutons, même composition alpha, patch ancien/nouveau bouton et pump title-music, sans framebuffer supplémentaire.

BMP modifiés localement sur SD : `kStrictSha256=false` pour la phase graphique ; parser BMP V4 BGRA32, dimensions, extent et bornes restent obligatoires, CI conserve les 18 empreintes des BMP versionnés. Rétablir le SHA strict après choix des assets définitifs.

Le log `platformio-device-monitor-261119-231018.log` : `PREFLIGHT FAIL menu_frame_120.bmp` sur fichiers SD modifiés, aucun rendu du nouveau mode. Ancien log `261119-224913.log` : 240 ~800ms en redraw complet avec sous-alimentation audio ; patch partiel + pump encore en attente de preuve hardware. Retester 3 visuels, cycle, sons, actions, `paint=BUTTONS`, `underruns=0`, `recoveries=0`, heap8, échec sans asset. **Ne pas fusionner sans PASS matériel.**


## UAC BMP preflight forensic diagnostics — 20 novembre 2026 (PENDING CYD)

Nouveau retour matériel utilisateur après changements de BMP : `[UAC] PREFLIGHT FAIL /doomrpg/img/menu_frame_120.bmp` répété ; aucun `MODE` ni `PRESENT` UAC, tandis que la musique MENU remonte `underruns=0 recoveryStarts=0` et `underflows=0`. Ce log n'est **pas** une preuve de validation de l'UI/UAC/audio sous dessin. Le `main` fusionné (#400) attend un **cadre 59×120 / 118×240** (fichiers V4 BGRA32 28 442 / 113 402 octets), à distinguer de la première génération 79×120 / 158×240. Boutons 55×20 / 110×40 inchangés. SHA volontairement désactivé pour la phase graphique.

Nouvelle instrumentation **échec uniquement**, sans changer le décodage ni le refus strict : `[UAC] BMP REJECT reason=sd-open|header-read|dimensions|dib-header-not-V4|pixel-depth|compression|channel-masks|pixel-offset|file-size|image-size` + dimensions, tailles, DIB/bit-depth, masques. Réutilise le même handle File, lit au plus 122B de l'en-tête BMP **uniquement si le parseur refuse**. Aucun changement de mémoire persistante, d'accès SD en succès, d'ISR audio ou de compositor.

**PENDING matériel :** test normal `esp32-cyd` avec SD d'images modifiées ; collecter le premier `[UAC] BMP REJECT` et corriger l'export à la source. Le fallback menu classique doit rester opérationnel. Ne pas déclarer le menu validé avant son affichage réel et les mesures de son/latence.
