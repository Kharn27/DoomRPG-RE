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


## UAC MENU HOT-CACHE — 20 novembre 2026 (PENDING CYD)

Retour utilisateur `platformio-device-monitor-261120-002906.log` : **mode 240 direct préféré visuellement**, 120/240→120/240 direct tous présentés `result=OK` ; FULL 505/709/717ms, sélection 198/284/286ms dont SD 174/261/261ms et LCD 4ms pour les deux boutons. Deux activations OPTIONS et retours Back fonctionnels, `shapeData/mediaTexels=NULL`. Musique `MENU_END_ONCE` terminée à ~30s, **avant** premiers taps UAC (~75s) ; aucun underrun/recovery mesuré, mais pas de test audio pendant refresh. Pas de PASS audio-UAC sous musique revendiqué.

Conception : cache transitoire de **8 patches RGB565 complets (4 boutons × inactive/active × 110×40 × 2 = 70 400 B)**, *après* composition du matte/bouton/frame. Huit allocations bornées individuelles `MALLOC_CAP_8BIT` de 8 800 B évitent la fragmentation du plus grand bloc (~65KB avec WAV ouvert) ; garde-fou ≥20 480B de heap8 disponible et fallback SD pour chaque slot indisponible. Les quatre états visibles sont capturés lors du FULL ; les états alternatifs sont précomposés une seule fois en petits batches SD, audio pump coopératif, **sans écriture LCD supplémentaire**. Après échauffement, sélection ancien/inactif + nouveau/actif = deux transferts TFT 110×40 depuis RAM, sans nouveaux descripteurs SD, allocations ni modification framebuffer 160×120.

Le cache est relâché **avant** préflight de changement de mode, retour CLASSIC, START/LOAD/OPTIONS/HELP et tout `EspUacMenuPreview_reset` sur sortie ; log `CACHE FREE ... heap8` pour preuve. Échec de précomposition SD : reste fail-closed et retourne au classic. Le jeu et le ring music ne gardent aucun cache UAC. Tester sur vrai `esp32-cyd` `CACHE ALLOC` 8/8, `CACHE WARM`, `PRESENT paint=BUTTONS cache=HIT sdMs=0`, `CACHE FREE` et mêmes heap8 après sortie, surtout **pendant** le morceau titre. Le premier FULL peut prendre plus de temps, du fait de l'échauffement volontaire ; mesurer `warmMs` séparément.

**PENDING matériel** : pas d'estimation de gain prétendue sur hardware avant test ; vérifier latence tactile, qualité pixel à chaque état, absence d'audio underrun et headroom/FD.


## UAC-P3 — décision graphique 240 direct, logiciel fallback (20 novembre 2026, PENDING nouveau firmware)

Le vrai CYD avec cache RAM a confirmé les trois prévisualisations précédentes : en **240 natif**, 13 changements de sélection consécutifs avec `paint=BUTTONS cache=HIT sdMs=0`, **4 à 10 ms** par paint (4 ms LCD). Audio titres **STREAM 10s, 0 underflows, 0 recoveries, pumpReads=49**, `MENU_END_ONCE 0 underruns / 0 recoveryStarts` ; transitions OPTIONS/Back et cache libéré `heap8 23480->94008` **avant** l'action OPTIONS. Pendant le titre : `heap8=23480`, plus grand bloc `7668B`, ce qui interdit de cumuler un second cache de sous-menu en parallèle.

**Décision utilisateur :** abandonner les modes 120 et 240→120 et le tap logo : démarrage automatique du menu `240 direct` lorsque les neuf BMP `*_240.bmp` sont valides sur SD ; sinon conserver sans changement le menu logiciel/procédural (sélection et double-tap). Le capteur logo gauche n'est plus un sélecteur. Préflight lors de la réinstallation du callback MENU_MAIN, **avant** son premier rendu, et retour au classic via reset sur erreur de lecture. Le cache reste borné au seul écran MENU_MAIN et est libéré avant START/LOAD/OPTIONS/HELP, retour classique et sortie. Le mode 240 conserve les deux boutons `110x40` chargés depuis les 8 slots cache et le fallback SD si manque de RAM.

**Politique future sous-menus :** jamais plusieurs caches UAC simultanés. Reprendre l'unique propriétaire de cache après libération de l'écran précédent ; en cas de RAM insuffisante, cache réduit ou rendu SD/procédural. Ne pas charger les graphismes de sous-menus aujourd'hui : leurs modèles/actionnaires restent natifs et logiciels. Tests matériels requis sur boot direct 240, absence de logo-switch, retour OPTIONS/Back, manque de BMP sur SD, musique, heap8 et retour gameplay. Pas de PASS déclaré avant le prochain firmware sur CYD.

## UAC-P3 — régression d'initialisation (20 novembre 2026, PENDING retest CYD)

Le vrai CYD montre [MAINOPAQUE] FAILED heap changed heap8=94452->23480 largest8=65524->7668. Le callback MENU_MAIN allouait 70400 B de cache avant la vérification sans allocation du compositeur logiciel, causant [MAINBOOT] FAILED, l'arrêt anticipé du WAV MENU_ACTION_BEFORE_PAK à seulement 6737 frames, et l'échec de OPTIONS->Back (même anomalie heap8=114860->44332). Malgré 17 changements de boutons UAC en 4-5 ms depuis RAM, ce firmware ne valide PAS le démarrage/retour menu.

Fix sur la même PR #406 et même branche: EspUacMenuPreview_activate() n'effectue plus de préflight SD ou allocation. Il pose activationPending seulement. La toute première présentation physique, après la vérification des allocations dans DoomRPG_esp32MainMenuTouchActivate, réalise le préflight des neuf BMP 240 puis alloue les slots de cache, ou revient fail-closed au menu logiciel classique si fichier invalide. Le reset annule activationPending. Test host compilant et exécutant les vrais corps C++ de l'activation différée, de la résolution, du reset et du fallback avec compteurs SD/heap simulés.

Idée utilisateur pour la suite, non implémentée: deux modèles de boutons 240 vierges (inactif / actif), puis libellés START/LOAD/OPTIONS/HELP et textes des sous-menus rasterisés par le moteur natif. Deux templates RGB565 demanderaient 17600 B de cache contre 70400 B pour les huit patches individuels. Cela conserve le look 240 si les vrais templates sans texte sont fournis. Ne PAS réutiliser les BMP actuels avec libellés incrustés comme modèles vierges; la typographie/art doivent rester corrects. Ne pas ajouter de cache de sous-menu au cache principal.

Test attendu: [MAINOPAQUE] READY, [MAINBOOT] READY, [UAC] MODE 240 default=yes, audio STREAM underflows=0 sous interactions, OPTIONS->Back sans FAILED, CACHE FREE avant transition, et fallback logiciel en retirant volontairement menu_frame_240.bmp sur copie SD. PENDING validation CYD.

## UAC-P4 — bright-only + inactive dim5/8, 240 natif, sans nouvel asset (20 novembre 2026, PENDING CYD)

Le log CYD `platformio-device-monitor-261120-011635.log` a validé le correctif d'activation différée: `[MAINOPAQUE] READY`, `[MAINBOOT] READY`, OPTIONS->Back READY (1790 ms), START->INTRO, jeu et retour au menu, `CACHE FREE bytes=70400` avant toutes transitions, musique menu et intro `underruns=0 recoveries=0`. Touches dans menu 5-7 ms / `cache=HIT sdMs=0`, mais FIRST FULL `ms=798` et `warmMs=640` supplémentaires, puis mêmes ~801+636ms au retour OPTIONS et 797+641ms au retour du jeu. Source du ressenti de lenteur: réinitialisation SD/deuxième état, PAS le double-tap en cache.

Décision exploratoire, sur la même PR #406 sans changer les BMP de la SD: conserver `menu_frame_240.bmp` et seulement les 4 `menu_{start,load,options,help}_active_240.bmp` avec leurs lettres originales incrustées. Pour chaque position, le raster RGB565 final BRIGHT est capturé avant assombrissement; l'état INACTIVE est calculé avec `r5/g6/b5 := floor(5*channel/8)` lors de FULL ou du scanout TFT. Deux fenêtres physiques 110x40 par sélection, pas de SD ni d'allocation sur toucher, un seul buffer de ligne stack 220B. Passer de 8 allocations de 8800B à 4 allocations (35200B), avec le même garde-fou heap8 de 20KiB. Le FULL capture les 4 bright directement; aucun warmup des quatre états alternatifs ne devrait désormais être nécessaire (`CACHE WARM filled=0 ms≈0`). 5 BMP seulement au préflight, les anciens `*_inactive_240.bmp` restent sur SD mais ne sont pas requis par cette branche, et le fallback menu logiciel reste activé si source invalide.

C'est une étape de comparaison matérielle avant les deux assets potentiellement définitifs: un SEUL bouton 240 générique vierge + police native/atlas + assombrissement logiciel, qui pourrait limiter les templates RAM à un patch de 8800B (plus petits glyphes/scratch), mais exige de choisir le modèle de texte et de vérifier que les couches alpha/frame n'imposent pas de données par emplacement. L'ancienne image source 120 et la décimation des 240 sont une piste de comparaison qualité, pas le choix production par défaut.

Test CYD normal `esp32-cyd`: `[UAC] CACHE ALLOC slots=4/4 bytes=35200`, `CACHE WARM filled=0`, `PRESENT paint=FULL` et `PRESENT paint=BUTTONS cache=HIT sdMs=0`, aucune coupure SFX/OPL, couleurs/contraste/texte et bordure inspectés visuellement, OPTIONS->Back et gameplay->menu sans latences de >1s si elles provenaient du warmup. Vérifier exact `heap8` récupéré et `MAINOPAQUE READY`, `MAINBOOT READY` après première entrée et retours. PENDING CYD ; pas de PASS runtime depuis CI.
