# HELP-NAV-P1 — Écran d'aide plein cadre et navigation par demi-écran

**Statut : PASS visuel CYD du 22 novembre 2026 ; double CI PASS ; fusion explicitement autorisée.**
Le rendu final sans cadre ni voile est approuvé par l'utilisateur sur le vrai CYD ; aucun nouveau log Serial de cette finition n'a été joint.

**Source de référence :** `main=c1a8c2820e5f7a75d8097e873905ec8e11fb53f7` au départ,
`ESP32/MILESTONE_PR332_HELP_VISUAL_CYD_PASS.md` pour la fidélité existante
des illustrations. Branche `agent/esp32-help-fullscreen-halfscreen-nav`.

## Contrat

Conserver les **16 pages de 7 entrées** du guide original, le vrai fond étoilé
`c.bmp`, le thème actif, les séquences de respiration et les trois miniatures
tactiles, HUD et armes réellement issues du PAK. Aucun changement de gameplay,
audio, source SD de police, sauvegarde ou menu principal.

- **Retour** : petit `X` rouge vif issu du même atlas texte natif `a.bmp`,
  position `x4,y1`, **sans cadre ni plaque de fond**. Le rectangle de
  touche, invisible mais pratique, reste `x0..17,y0..14` (logique).
- **Page précédente** : tout appui ailleurs en moitié **gauche** (`x0..79`).
- **Page suivante** : tout appui en moitié **droite** (`x80..159`).
  Aux extrêmes : rester à 1/16 ou 16/16 sans redessiner.
  Le garde FNV/owner et le retour natif MENU_MAIN sont identiques.
- **Chevrons** : à gauche et à droite, x2..7 / x152..157, y57..67 ;
  RGB565 aux couleurs du thème en cours, teinté faiblement lorsque
  la direction n'est plus disponible. Placés hors des visuels x8..151.
- **Espace récupéré** : disparition des trois plaques BACK/UP/DOWN et
  du pied de page y100..119. Le fond `c.bmp` animé est maintenant affiché
  **tel quel, sans voile noir translucide**, jusqu'au bord de l'écran ;
  les sept lignes textuelles ont une cadence verticale
  de **14 px** (fin de la dernière ligne approximativement y109).
  Le cache 144×60 RGB565 du sélecteur (**17 280 B** inchangés)
  s'étire au rendu vers 144×76, de y39 à y114, sans nouvelle allocation.
  Miniatures HUD, scène, TURN et boutons WPN restent à leur taille/source
  native, avec leurs bornes de peinture inchangées.

## Propriété mémoire / échecs

`EspNativeHelpNav_hit` / `EspNativeHelpNav_scroll` sont des helpers
purement statiques (C11/C++ compatibles), sans heap, sans I/O, pas de
mutation directe du monde. Le renderer réutilise le framebuffer
**160×120×RGB565**, le PAK native et les caches mini existants.
`shapeData` et `mediaTexels` restent NULL.

Les contrôles touch physiques invalides sont rejetés avant la division par 2.
Le `X` déclenche exclusivement le chemin de retour existant, précédé de
`PlatformInput_setTapCallback(NULL)` et `helpStop()`. Si le FNV ou
le menu owner n'est plus conforme, le fail-closed et le recovery d'origine
restent en place. Aucune action sur le monde depuis HELP.

## Tests

`ESP32/test/native_main_help_host.py` : le catalogue 112/7, le thème,
la barre native, les aperçus PAK et les positions visuelles, plus les
nouveaux éléments de navigation. `native_help_fullscreen_nav_host.py`
**compile les vrais corps C** du fond étoilé, des chevrons et de la grille, et
teste les **19 200** coordonnées logiques, y compris le X prioritaire,
les bornes première/dernière page, les deux couleurs de chevrons et les pixels
extérieurs aux rails. Il confirme aussi que les **19 200 pixels RGB565**
issue des blits `c.bmp` restent strictement inchangés après présentation,
sur trois phases de défilement (dont wrap), et que le painter du cadre X
est absent. La CI lance les oracles historiques en complément.

## Test matériel demandé (firmware normal `esp32-cyd`)

1. Ouvrir HELP. Vérifier **`X` rouge sans cadre**, `HELP / CYD` et `1/16`,
   les deux chevrons aux extrémités, aucun bouton inférieur.
2. Appuyer **droite** à différentes hauteurs pour 2/16…16/16, puis
   **gauche** pour revenir ; tester x79/x80 et les pages extrêmes.
3. Tester le **X** : retour MENU_MAIN dès le premier appui, pas page -1.
   Revenir dans HELP plusieurs fois et confirmer l'absence de touches
   héritées ou de fuites du petit état.
4. Inspecter pages 5 (TOP HUD), 6–9 (illustrations 3×3 et commandes),
   10 (BOTTOM HUD) et 11 (WEAPON SELECT désormais 144×76), ainsi
   que le dernier texte après extension. Idéalement parcourir les trois
   thèmes CLASSIC/INFERNO/PHOSPHORE.
5. Capturer `[MAINHELP] READY/PAINT/PAGE-PREV/PAGE-NEXT`, sorties,
   éventuels `FAILED`, et les budgets mémoire du firmware normal.

Le log matériel précédent couvre la navigation et le retour par le X ;
la finition visuelle finale est attestée par le retour utilisateur, sans
nouvelle mesure RAM ni nouveau log du firmware final.

## Premier log réel : version avant retrait du cadre/voile

Capture de référence **`platformio-device-monitor-261122-184104.log`**,
390 lignes sur firmware `esp32-cyd` (état P1 **avant cette retouche**) :
- Deux ouvertures HELP, **17 PAGE-NEXT** et **5 PAGE-PREV**, passage par la page 11
  WEAPON SELECT (`WEAPON-PREVIEW ... bytes=17280 packClosed=1`), et **un
  retour via X** (`[MAINHELP] READY back=native` puis
  `[MAINBACK] READY ... touch=rearmed`).
- Les garde-fous `[MAINHELP] PAINT ... nav=halves+header-x` restent conformes.
  Aucun `[MAINHELP] FAILED` ni crash/panic dans la capture. Navigation aux
  bornes 1/16 et 16/16 **non attestée** : la capture atteint au maximum
  `scroll=77` sur les 105 possibles.
- La capture contient un `MENU_END_ONCE` audio avec **6 underruns et 1 recovery**
  à la terminaison de la musique du menu : observation hors périmètre HELP,
  ne pas la transformer en promesse de zéro underrun.

Le testeur a demandé ensuite de retirer **le cadre rouge du X** et **le
voile noir translucide du texte**. Le nouveau code garde le X natif et son
hitbox prioritaire, la police `a.bmp`, les chevrons et la pagination,
mais supprime `helpPaintExitFrame` et la boucle de dimming RGB565
dans `helpDrawStarfield`. Le framebuffer reçoit ainsi les pixels exacts du
blit de `c.bmp` ; **pas de cache ou d'allocation ajoutés**. La variante sans cadre ni voile est **approuvée visuellement** par le testeur
sur son CYD le 22 novembre : « Ca claque ! Merci. Tu peux fusionner :) ».
Aucun log Serial complémentaire n'a été transmis avec ce verdict ; ne pas
extrapoler à un test d'endurance, aux trois thèmes ni à la page 16/16.

## Décision finale de validation

L'utilisateur approuve visuellement la version définitive **sans cadre du X
et sans assombrissement du fond c.bmp** et demande explicitement sa fusion.
Ce PASS visuel, combiné à la capture précédente pour les parcours
page suivante/précédente et retour au menu, et à la **double CI SUCCESS**
au commit de code `750e10d768991dd027acc55237a0373bb68db328`,
permet la fusion de PR #435. Tests de code, historique SDL, build firmware
normal `esp32-cyd`, profil inversé et test hôte pixel-perfect PASS.

- Push : https://github.com/Kharn27/DoomRPG-RE/actions/runs/45051473237
- PR : https://github.com/Kharn27/DoomRPG-RE/actions/runs/45051469143
- Aucun changement de code après le PASS utilisateur : cette mise à jour
  documentaire est **docs-only** et ne redéfinit pas les mesures matérielles.
