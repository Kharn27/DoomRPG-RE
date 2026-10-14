# Doom RPG ESP32 — guide de travail

Documentation **opérationnelle** du port classic CYD. Pour l'état exact et les limites hardware : [`PORTING_STATUS.md`](PORTING_STATUS.md). Pour le design de long terme : [`ARCHITECTURE.md`](ARCHITECTURE.md).

## Matériel et stockage

- **ESP32-2432S028R classic CYD**, 4 MB flash, sans PSRAM ; écran ILI9341 320×240, touch XPT2046, carte microSD.
- Framebuffer logique 160×120 RGB565 (38 400 octets), présentation nearest-neighbor ×2.
- Fichier runtime obligatoire sur SD : `/DoomRPG-ESP32.pak`.
- `/DoomRPG.zip` n'est utilisé que comme source/référence pour la génération ; **aucune lecture ZIP en jeu**.
- L'audio est hors périmètre de priorité immédiate. Ne pas déplacer la mémoire graphique vers des buffers map-wide.

## Build, flash et logs

À partir de `ESP32/`, environnement normal (celui à tester sur le vrai CYD) :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t upload -t monitor
```

Selon le poste, PlatformIO peut être invoqué avec `platformio` au lieu de `pio`. La cible d'upload configurée dans le projet peut dépendre du port série (historique : `/dev/ttyUSB0`).

`esp32-cyd-bringup` sert aux diagnostics/probes supplémentaires ; **ne pas** tirer de conclusions RAM production à partir de ce profil. La vérité matérielle vient du boot et des interactions réelles, via Serial.

### Diagnostic d'un incident gameplay

`esp32-cyd-trace` hérite de **la production** : même layout, optimisations
release, stockage et propriétaires. Il active TRACE sans définir
`DOOMRPG_ESP32_BRINGUP_PROBES`, sans overlay tactile et sans probes de startup.
Les écritures Serial modifient le timing : ce n'est ni un benchmark ni une
preuve d'identité du firmware normal. Bringup reste un diagnostic plateforme,
avec d'autres layouts et un démarrage différent, pas un simple « mode debug ».

Les anciens profils `esp32-cyd-story-teardown-probe` et
`esp32-cyd-divider-probe` et leurs blocs firmware sont retirés. Leur couverture
utile est exercée par `test/diagnostic_profiles_host.py` : vrais formatter,
story-hand et `DoomCanvas_free`, I/O image/cleanup intro mockés. Pas de preuve
PAK ou d'équivalence heap ESP32 apportée par ce test hôte.

Pour l'incident zombie actuel, `DOOMRPG_ESP32_TRACE_SPRITE=106` dans
`platformio.ini` sélectionne **l'index immuable du sprite de la carte**, pas
son subtype ni son ordre d'activation. Changer uniquement cette valeur pour
un autre incident, sans créer d'environnement par monstre. `[SPRITETRACE]`
observe les données déjà lues par le renderer : position projetée, tile et
flags topology, pose, caméra, preview, admission hidden/BSP/candidate,
near/backface/clip/frame/span failure ou raster avec nombre de pixels.
`result=rasterized pixels=0` n'est pas une preuve d'affichage visible ; un rejet
ou une frame réussie ne suffit pas à diagnostiquer un rollback gameplay.
Les déplacements commis et décisions de chaque membre deviennent également
visibles via les logs TRACE existants. Aucun buffer ni owner persistant ajouté.

Depuis `ESP32/` :

```sh
pio run -e esp32-cyd-trace
pio run -e esp32-cyd-trace -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Fermer tout autre lecteur Serial avant le monitor. Vérifier au boot
`[TRACEPROFILE] production-path sprite=106 ... bringup=no`, puis charger la
sauvegarde existante près du zombie **sans la réécrire**. Capturer la sortie
depuis LOAD, tous les tirs jusqu'à la disparition, puis le premier mouvement
qui le fait réapparaître. Conserver les lignes complètes, pas seulement les
SPRITETRACE. Une sauvegarde est une reproduction, pas la preuve d'une cause.
Après correction éventuelle, refaire le scénario avec `esp32-cyd` normal.
Ce jalon ajoute l'observabilité ; il ne corrige pas encore la disparition.

Le jalon depuis main `b941048` retire la copie C desktop Render générée dans
les deux profils. L'owner géométrique natif sert maintenant aussi bringup ;
normal reste bit-identique chargé dans la comparaison locale. DoomRPG.c reste
généré pour ses utilitaires/états réellement liés, et le shim SDL packed reste
fonctionnel. Cartographie ELF et priorités : `CONSOLIDATION_AUDIT.md`.

Pour refaire la cartographie sur **un build courant**, depuis ESP32 :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t compiledb
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --calls
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --require-retired-render
```

Répéter avec bringup et régénérer compiledb pour le profil choisi. Les vieux
Render.o/Render.c de .pio ne prouvent pas une compilation ; SCons/map/ELF sont
croisés. Le CSV b941048 est un snapshot de la base **avant** retrait, pas un
inventaire automatique du HEAD. Un symbole lié ou un call direct ne prouve
jamais un chemin réellement exécuté sur le CYD.

L'ancienne suite graphique `menu.bsp`/ZIP est retirée : son point d'entrée
retournait déjà 0 pour tous les builds ESP32. Le rapport bringup `[MENUBSP]
RETIRED` et `MENU BSP FAIL` reste donc attendu ; il ne teste pas le menu opaque
production. Les diagnostics actifs première frame, CHANGEMAP, opcodes et touch
ne sont pas supprimés avec cette suite historique.

La politique de logs compile-time est définie dans `ESP32/include/doomrpg_log.h` : `DRPG_LOGE/I/D/T`. Normal = INFO, bringup = TRACE. Les messages de succès récurrents sur VIDEO, PAKIO, redraw et tour doivent rester hors du flux INFO ; ne pas masquer les erreurs, transitions, guards et recoveries.

Mouvements de monstres : le normal conserve les résumés BEGIN/COMPLETE,
ORDERED-DISPATCH, les erreurs de mouvement/publication, ainsi que les témoins
attaque-probe/retaliation nécessaires au watcher. Les succès PROBE/ALLOW/COMMIT,
obstacles de pathfinding ordinaires et détails MEMBER/PAUSE/RESUME sont TRACE ;
les mêmes PROBE/prepare/MEMBER restent affichés si rollback/admission/publication
échouent. Utiliser **esp32-cyd** pour juger la latence : bringup garde les logs
exhaustifs. Ce retrait de texte ne change ni le nombre de redraws, ni le RNG,
ni la cadence d'attaque ; pas de gain FPS hardware revendiqué.

Le dernier retour CYD confirme l'absence de gain ressenti et un SAVE V11 slot 3
suivi du retour au jeu/expiration de « Game saved » ; pas de LOAD dans cet
extrait. Le goal 2/3 de sprite 202/subtype 4 reste différé sur une géométrie
special-plane non possédée (premier déplacement déjà commis), code inchangé
depuis la base main. Détails et limites de preuve : `PORTING_STATUS.md`.

Rollback de l'interaction opcode 23 depuis automap : en cas d'échec de
démarrage du script/dialogue, le resident restaure son mode et redessine
l'automap ; depuis WORLD, il redessine le monde. Si le redraw de restauration
échoue, le gameplay se désactive. Le test hôte utilise le vrai routage/mode
resident avec startup/render/assets mockés ; il ne prouve pas un échec I/O
réel sur CYD. Pour un smoke matériel, ouvrir normalement le dialogue depuis
automap ; ne pas retirer/corrompre le PAK pour fabriquer une panne.

## Feedback directionnel des dégâts et réserves post-LOAD

Sur le CYD, un hit monstre non mortel commité affiche le contour rouge
historique de 2 px logiques / 500 ms **et**, pendant cette période lorsque
le compositor publie immédiatement la première impulsion, une bande de
10 px logiques (20 px physiques) dans le côté de provenance réel :
`FRONT` en haut de la vue, `RIGHT` à droite, `BACK` en bas,
`LEFT` à gauche. Deux impulsions de 85 ms avec 45 ms de pause ;
le feedback est distinct du séquenceur IA et ne doit pas retarder
l'attaquant suivant. En INFO, comparer `[MONSTERRETAL] COMMIT`,
`[VIEWFLASH] PAINT/EXPIRE`, `[DAMAGECUE] QUEUE` et
`[DAMAGECUE] ON ... redBorder=visible presented=1`. Le
`concurrent=yes` du log signifie que les deux effets *peuvent* coexister,
pas que chaque impulsion a réellement recouvert le rouge : c'est
`redBorder=visible` qui l'atteste. Le premier hit après déplacement
a déjà montré `redBorder=none` ; cette limite reste ouverte malgré
l'acceptation qualitative du jalon par l'utilisateur.

Pour un tonneau, tester séparément la cible et son explosion.
Un blast **unique** mortel (`chain=1`) peut désormais mettre
`playerHP=0` dans l'API terminale, réserver exactement un octet RNG
de mort après le blast et avant les conséquences monstres, puis armer le
vrai menu de mort une fois la transaction commise. Chercher
`BARRELRADIUS PLAYER-HIT`, `DEATH-RNG-RESERVE`,
`MONSTERRADIUS COMMIT`, `LETHAL-COMMIT`, `PLAYERDEATH ARM/READY`.
Le vrai CYD a validé ce chemin avec deux monstres voisins ; une
**chaîne létale de plusieurs tonneaux** reste explicitement non supportée.

Lorsqu'un monstre né d'un trigger `SHOW` refuse de bouger après
LOAD jusqu'à ce que le joueur se retourne, **ne pas** réactiver tout
le voisinage ou scanner à 360° comme correctif. V9/V11 enregistre déjà
activation + ordre (snapshot 268 B), topologie et positions. Garder
un slot avant le trigger et un autre après apparition/activation,
puis comparer `[MONSTERACT] ACTIVE` avant SAVE,
`[NATIVESAVE] SAVE ... activation=count/FNV`,
`[MONSTERACT] RESTORE ... activeCount/stateFNV` et le premier
`[MONSTERACTIVESEQ] BEGIN/COMPLETE`. Une apparition après rotation
prouve une activation par visibilité BSP, **pas** une perte du snapshot
à elle seule. L'enquête root-cause SAVE/LOAD/SHOW doit rester un
jalon distinct ; détails dans `PORTING_STATUS.md`.

## Playtest libre : captures automatiques des incidents

Pour jouer normalement **sans surveiller le terminal**, depuis `ESP32/` :

```sh
# Python utilisé par PlatformIO a généralement déjà pyserial :
python3 -m pip install pyserial  # seulement si non installé
python3 tools/playtest_watch.py --port /dev/ttyUSB0 --bell
```

Cet outil remplace le moniteur Serial pendant le playtest : **ne pas ouvrir `pio device monitor` simultanément**, car les deux processus se disputeraient le port série. Aucun reflash n'est nécessaire. Les règles explicites reconnaissent les frontières non implémentées ou les anomalies importantes (ex. `[HUB] SELECT-DEFER ... kind=item`, `[RESIDENTGAMEPLAY] SELECT-DEFER`, `[MONSTERMOVE] DEFER`, `[TURNFRAME] DIAG fail=WORLD_RENDER`, `[HUB] CLOSE ... exactHud=NO`). Les simples textes `sound=...-deferred`, `turnAdvance=deferred` et les messages READY ne provoquent **pas** d'alerte.

À chaque incident : une alerte `[WATCH]` et un fichier `playtest-captures/incident-....log` de ~75 lignes précédentes, le déclencheur, puis jusqu'à 20 lignes suivantes. Un cooldown de 45 s par catégorie limite les duplications ; ce n'est **pas** une garantie de capture exhaustive de chaque occurrence. `Ctrl+C` arrête seulement le moniteur PC et écrit `snapshot-....log` contenant les dernières lignes, utile si un problème visuel ne correspond à aucune règle. **Le CYD continue à tourner : aucun arrêt logiciel, watchdog, ou écriture microSD n'est impliqué.** Ne pas confondre capture du contexte et pause du gameplay.

Options : `--echo` réaffiche tous les logs, `--before 120 --after 30` augmente le contexte, `--cooldown 90` réduit les alertes. On peut valider sans matériel avec `python3 tools/playtest_watch.py --self-test`, ou lire un flux enregistré : `python3 tools/playtest_watch.py --stdin < ancien-log.txt`. Les fichiers de capture sont ignorés par Git. Pour une pause du jeu réellement ciblée sur des actions non possédées, ajouter plus tard un mode diagnostic ESP32 **opt-in** avec reprise explicite, jamais un `while(1)` qui déclenche le watchdog.

Cheats : jalon séparé, à auditer par rapport aux commandes réelles du legacy ; ne pas inventer de code secret ni modifier les progressions de sauvegarde dans ce jalon.

## Régression native : projectiles de monstres et tonneaux

Le **sprite logique 204** est une ressource de projectile créée en combat, pas un sprite BSP permanent. Sa paire de mappings est ajoutée *sparsely* au catalogue **avant** le gameplay dès que la topologie contient un Imp (subtype 3) ou un Malwrath/Cacodemon (subtype 6). Ce chemin de présentation est autorisé **uniquement pour l'attaque primaire** de ces deux subtypes : Imp = un projectile, Malwrath = trois. Chaque projectile est un effet monde transitoire, peint aux positions bornées par le renderer, sans ajouter de sprite persistant ni consommer de gameplay RNG. Les autres familles de missiles demeurent hors de ce contrat limité.

Sur le vrai CYD, contrôler `[ENGINESESSION] SPRITES ... monsterProjectile204=primed` si l'une de ces familles est présente ; puis, pour une attaque du Malwrath, `[MONSTERATKVIS] ARM ... projectile=malwrath204-3shots-armed`, les `[MALWRATHPROJECTILE] FRAME ... logical=204 ... presented=1` (trois points par vol) et deux `REARM shot=2/3` / `shot=3/3`. Le `[MONSTERATKVIS] COMPLETE` doit précéder la résolution réelle `[MONSTERRETAL] COMMIT` ou `MISS-COMMIT`; les HP/armure/RNG et le tour restent les propriétaires gameplay existants. Un `projectile=none` sur cette attaque primaire, des `DEFER` de rendu ou un sprite 204 absent ne sont **pas** un PASS. La validation matérielle ciblée du Malwrath est consignée dans `PORTING_STATUS.md`.

Pour les tonneaux, distinguer **visée** (`[ACTIONENGINE] TRACE ... route=BARREL_SUBTYPE1`) et **préflight du rayon d'explosion** (`[BARRELRADIUS] PREFLIGHT` ou `DEFER`). Une ligne liée `type=0` est inerte pour `Game_hurtEntityAt()`, quel que soit son subtype, sans rendre les autres entités destructibles ignorables. Quand plusieurs tonneaux et monstres participent à la chaîne, l'ordre natif est tirage de l'explosion, dégâts/conséquences et RNG des monstres, puis explosion suivante, dans une transaction à rollback borné ; chercher `[MONSTERRADIUS] SEQUENCE-BEGIN`, `[BARRELRADIUS] BLAST` et `[BARRELRADIUS] INTERLEAVED-COMMIT`. **Ne pas qualifier un ancien `DEFER` ou une impression visuelle seule de PASS ciblé** : reproduire le scénario exact et vérifier la consommation d'ammo, les monstres, les drops et l'absence de rollback partiel.

Enfin, si les taps remontent en `[TOUCH]` mais que `[ALIVE] SD=unavailable PAK=unavailable ... MENU=unavailable`, rechercher l'échec du montage SD avant d'incriminer le menu ou les correctifs gameplay. Un firmware qui démarre sans `/DoomRPG-ESP32.pak` ne peut pas initialiser le jeu.

## Validation d'une nouvelle frontière

### Interactions payantes avec dialogue et attente

Le script original ne propose pas de YES/NO : terminer le dialogue d'offre
avance son état, puis une nouvelle interaction contrôle/débite les crédits.
Il est toutefois possible d'interrompre **EV_DIALOG** avec sa carte **BACK**
tactile (en haut à gauche du viewport, au-dessus du texte). Ce tap annule sans
reprise du script ni débit ; ailleurs, le tap reste SELECT (texte/page/suite).
**EV_DIALOGNOBACK**, notamment après paiement, n'a pas cette carte et reste
SELECT-only : aucun remboursement ou nouvelle branche script n'est ajouté.

Le SELECT natif admet maintenant un `EV_REQSTAT` crédits (23, stat 2) suivi
du débit `EV_DECSTAT` correspondant (22), d'un dialogue sans Back (26), des
particules de capture (35, famille 3), d'un son publié comme intent silencieux
(42) et d'une attente (37). Après l'attente : SHOW/HIDE, refresh de vue (36),
note (40) et mutations d'état (11/19/20). Les opérandes et toute cette fermeture
sont préflightés avant le paiement ; SHOW utilise les gardes natives existantes.
Les autres stats, particules et formes de chaîne restent fail-closed.

Fonds insuffisants : texte BSP de l'opcode 23, aucun débit ni continuation.
Paiement : snapshot PlayerState et rollback si l'ouverture du dialogue échoue ;
le débit est commité seulement lorsque le dialogue est présenté. WAIT garde
l'entrée monde fermée, sans sleep ni action différée derrière le timer. Le
timer monotonic démarre après la première frame d'effet réussie, accepte le wrap
32 bits, et reste borné à 10 s. Le fond de l'effet passe par `Frame_composeTurn`
(pas de present/activation/automap/fresh-frame), puis l'effet est ajouté et une
seule frame physique publiée. Ne pas armer PresentGate avant renderTurn : sa
garde refuse cette composition et laisserait le WAIT armé indéfiniment.
Une note est préparée dans le candidate/scratch
NOTE existant et commise après le rendu final ; échec I/O/rendu = rollback du
segment et retry, pas de second paiement. Pas de nouveau fichier moteur ni de
nouveau propriétaire Player/Notebook/topologie/PAK.

**PASS CYD ciblé consigné** : event67/tile675, débit 19→9, effet publié,
HIDE/SHOW/NOTE, activation puis combat/mort du monstre, retour WORLD et aucune
seconde facture dans la trace. Portée précise et réserve d'identité firmware :
PORTING_STATUS. BACK visible est accepté ; son annulation sans débit n'est pas
prouvée par cet extrait. Pour compléter : depuis une sauvegarde pré-paiement,
annuler l'offre via BACK puis réouvrir ; avec <10 crédits, le dialogue de refus
ne doit changer ni crédits ni monde. Vérifier le notebook à l'écran et SAVE/LOAD.
Les logs doivent montrer `SCRIPTWAIT PRESENT` puis `SCRIPTSTAT COMPLETE` ;
`ARM` seul ne vaut pas succès. Garder les logs
`[SCRIPTSTAT]`, `[SCRIPTWAIT]`, `[NOTE]`, et vérifier `shapeData/mediaTexels=0`.
Le timer, le son silencieux et les mocks hôte ne prouvent pas une parité visuelle
ou RNG exhaustive avec le desktop. Ajouter un smoke-test dialogue ordinaire,
HUB/SAVE/LOAD et SHOW/HIDE (les types/formats de sauvegarde restent inchangés).

### Protocole général

1. Lire le vrai SHA `main`, [`PORTING_STATUS.md`](PORTING_STATUS.md) et [`ARCHITECTURE.md`](ARCHITECTURE.md) ; examiner les fonctions legacy concernées dans le repo.
2. Partir **du SHA exact** après chaque merge ; créer une branche `agent/*`. Jamais de merge automatique dans `main`.
3. Choisir **une famille cohérente bornée** ; introduire uniquement le propriétaire natif durable et les guards de parité nécessaires. Les cas non couverts restent **fail-closed**.
4. CI sur `esp32-cyd` ; vérifier symboles, mémoire, allocations, signatures, flux d'appel, invariants. Les tests locals/CI ne sont pas présentés comme tests hardware.
5. Push de la branche, flash du firmware normal par l'utilisateur, collecte des **logs Serial réels**, comparaison avec le comportement attendu. Diagnostiquer, corriger, committer et push sans redemander.
6. Après PASS : geler la frontière du code testé ; commits suivants **docs-only** avant merge-ready. Toute nouvelle modification de code nécessite sa propre validation.
7. Actualiser **seulement** les faits du statut courant dans `PORTING_STATUS.md`. Pas de nouveau fichier `MILESTONE*.md` par micro-changement. Les commits et PR forment l'historique.

### Mini check-list runtime

- Boot normal, SD et PAK prêts, menu stable.
- Identité de la carte et FNV non contradictoires avec l'état attendu.
- `shapeData == NULL`, `mediaTexels == NULL` ; pas de source ZIP runtime.
- Allocations et cache bornés, pas de changement sournois d'owner mutable.
- Tester uniquement les trajets pertinents à la frontière (START, mouvements, rotations, portes, dialogue, HUD, LOAD, monstres, EXIT si concernés). **Ne jamais présenter les autres comme testés.**
- Sur Render, distinguer explicitement appel réel du chemin retiré et simple non-régression d'une session.
- Le résultat d'un test est documenté avec **SHA de code précis** et éventuelles réserves ; ne pas recopier tous les logs.

## Où trouver les spécifications

| Besoin | Référence |
| --- | --- |
| Objectifs/invariants mémoire/architecture native | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Derniers SHA, RAM, FNV, frontières en cours, caveats | [PORTING_STATUS.md](PORTING_STATUS.md) |
| Implémentation exacte | Code source et scripts de génération dans le repo |
| Parité Doom RPG originale | Sources desktop/J2ME retenues dans le repo |
| Historique d'un jalon | Commits/PR GitHub et fichiers historiques au [snapshot du 8 octobre 2026](https://github.com/Kharn27/DoomRPG-RE/tree/60d34d174bed0d4d4e13137f306070b5018c0fa9/ESP32) |

## Présentation du menu de mort

Après la chute et le fondu existants, le menu de mort occupe le framebuffer
entier et reprend la palette industrielle et les polices partagées du HUB et
du bilan de mission : cadre sombre, bandeau `YOU DIED` rouge, sous-titre
`MISSION FAILED`, puis quatre cartes à deux lignes. Aucun asset ni buffer
supplémentaire n'est chargé ; le propriétaire de mort reste inchangé.

`LOAD GAME` ouvre désormais un sélecteur tactile de **10 slots SD** (deux pages de cinq), qui peut charger l'ancien checkpoint via le slot 1. Deux pressions sur un slot lisible confirment LOAD ; les slots vides sont désactivés. En jeu, HUB → SYSTEM → SAVE/LOAD ouvre le même inventaire de slots, avec sauvegarde V11 atomique indépendante par slot. Pagination : gros chevron supérieur **précédent** (actif page 2), inférieur **suivant** (actif page 1) dans une colonne à droite ; numéro de page dans l'en-tête. Style visuel accepté comme provisoire sur CYD, finition Doom à reprendre en fin de port. Le slot AUTO n'est pas implémenté.
`GO TO JUNCTION`, `RETRY SECTOR` et `MAIN MENU` restent visibles et atténués,
avec `NOT AVAILABLE` : leurs backends ne sont pas implémentés par cette refonte.
Le tap unique LOAD, son dispatch différé au service et les autres actions
fail-closed sont conservés. Les cartes font 140×19 pixels logiques, à
`x=10..149`, `y=33..51 / 54..72 / 75..93 / 96..114` ; peinture et hit-test
utilisent les mêmes constantes. Les marges et interlignes ne déclenchent rien.

Ne pas confondre `MAIN MENU` sur cet écran de mort, encore différé, avec
`SYS → EXIT TO MENU` dans le HUB en jeu : ce dernier possède son propre retour
natif fonctionnel. La refonte graphique ne débranche ni ne modifie cette route.

Validation locale : commande du test dans [`test/README`](test/README), build
`esp32-cyd` et inspection du vrai painter avec/sans sauvegarde. Ces preuves
ne remplacent pas le matériel. Sur CYD : mourir avec un checkpoint, vérifier
la lisibilité puis charger et reprendre le gameplay ; répéter sans checkpoint,
vérifier `NO SAVE` et que les options différées ne quittent pas l'écran.

## Contrat d'évolution

L'audit architectural depuis `main 9d203e5` est dans
[`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md). Son inventaire distingue
sources présentes, unités compilées, sections liées et preuves d'exécution.
Premier lot : retrait de neuf anciens markers post-load Junction inaccessibles,
sans fusion d'états actifs. La comparaison des sections allouées des deux ELF
est stricte (octets/adresses/layout), avec `tools/compare_elf_alloc.py` ; les
tests hôte et la CI n'attribuent aucun nouveau PASS gameplay/matériel.

Deuxième lot sur la même branche : retrait de l'ancien renderer/probes de
menu ZIP (16 sources, 13 headers), quatre directives de wrap inutilisées et
allègement du diagnostic fermé. Après les deux lots : **158 sources C/C++,
152 headers include**. Sections allouées et RAM/flash inchangées dans les deux
profils. Trois fixtures SYS/progress/death-menu sont réalignées et intégrées
à la CI ; elles ne valident pas la restauration SD réelle. Les bridges de menu
et caches encore liés/partagés restent explicitement différés dans l'audit.

Ne pas introduire de runtime propre à une map, de gros index pointer-heavy, ni de nouveaux wrappers universels simplement pour remplacer ceux du desktop. Le PAK reste le backing store, `EspMapRuntime` reste immuable, chaque mutation a un propriétaire explicite. Différer l'optimisation de `PlatformVideo_present()` : Doom RPG est turn-based.

La documentation ne doit plus être un second journal de développement : décrire **le fonctionnement actuel**, son protocole de test et ses limites. L'historique exhaustif vit dans Git.
