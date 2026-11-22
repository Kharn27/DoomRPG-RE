# HELP-NAV-P1 — Écran d'aide plein cadre et navigation par demi-écran

**Statut : PENDING CYD** — la CI et les tests host ne remplacent pas le toucher
et la lecture visuelle sur l'ESP32-2432S028R classic CYD.

**Source de référence :** `main=c1a8c2820e5f7a75d8097e873905ec8e11fb53f7` au départ,
`ESP32/MILESTONE_PR332_HELP_VISUAL_CYD_PASS.md` pour la fidélité existante
des illustrations. Branche `agent/esp32-help-fullscreen-halfscreen-nav`.

## Contrat

Conserver les **16 pages de 7 entrées** du guide original, le vrai fond étoilé
`c.bmp`, le thème actif, les séquences de respiration et les trois miniatures
tactiles, HUD et armes réellement issues du PAK. Aucun changement de gameplay,
audio, source SD de police, sauvegarde ou menu principal.

- **Retour** : petit `X` rouge vif issu du même atlas texte natif `a.bmp`,
  position `x4,y1`; fin cadre rouge RGB565 `x1..16,y0..13`.
  Hitbox tactile prioritaire `x0..17,y0..14` (logique).
- **Page précédente** : tout appui ailleurs en moitié **gauche** (`x0..79`).
- **Page suivante** : tout appui en moitié **droite** (`x80..159`).
  Aux extrêmes : rester à 1/16 ou 16/16 sans redessiner.
  Le garde FNV/owner et le retour natif MENU_MAIN sont identiques.
- **Chevrons** : à gauche et à droite, x2..7 / x152..157, y57..67 ;
  RGB565 aux couleurs du thème en cours, teinté faiblement lorsque
  la direction n'est plus disponible. Placés hors des visuels x8..151.
- **Espace récupéré** : disparition des trois plaques BACK/UP/DOWN et
  du pied de page y100..119. La zone de lecture assombrie s'étend
  désormais à y116, les sept lignes textuelles ont une cadence verticale
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
**compile les vrais corps C** des chevrons, du cadre et de la grille, et
teste les **19 200** coordonnées logiques, y compris le X prioritaire,
les bornes première/dernière page, les trois palettes et les pixels
extérieurs aux rails. La CI lance les oracles historiques en complément.

## Test matériel demandé (firmware normal `esp32-cyd`)

1. Ouvrir HELP. Vérifier `X` rouge encadré + `HELP / CYD` + `1/16`,
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

Aucun résultat visuel, tactile ou RAM de cette variante n'est inventé
avant la capture réelle.
