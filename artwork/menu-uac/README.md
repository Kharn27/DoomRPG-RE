# Atelier graphique du menu UAC — PR #399

**Voir les compositions et les 16 boutons directement dans GitHub.**
Ces images sont assemblées depuis les **18 vrais BMP finaux** du dépôt.
Aucune retouche ou reconstitution manuelle des boutons dans les preuves.

## Cadre 120 px — quatre boutons et quatre sélections

Cadre **79 × 120**, boutons **55 × 20**. Sur la composition suivante,
START est actif ; LOAD, OPTIONS et HELP sont inactifs.

![Cadre UAC natif 79x120 avec START actif, LOAD OPTIONS HELP inactifs](previews/composition_120_start.png)

**Preuve de composition des quatre états de sélection :**

![Preuve du cadre 120px avec quatre montages START LOAD OPTIONS HELP actifs tour à tour](previews/proof_120_selections.png)

Chaque colonne assemble le cadre et ses quatre boutons avec **un seul
bouton actif**. La planche 120 est agrandie **×2 nearest-neighbor**
pour rester lisible. Le calcul de composition se fait à **79 × 120 natifs**.

## Cadre 240 px — quatre boutons et quatre sélections

Cadre **158 × 240**, boutons **110 × 40**. START est actif dans la
composition de référence :

![Cadre UAC natif 158x240 avec START actif, LOAD OPTIONS HELP inactifs](previews/composition_240_start.png)

**Preuve de composition des quatre états de sélection à taille native :**

![Preuve du cadre 240px avec quatre montages START LOAD OPTIONS HELP actifs tour à tour](previews/proof_240_selections.png)

## Catalogue visuel des 16 boutons

Les images ci-dessous sont les vrais boutons BMP alpha **composés sur un
fond sombre d'aperçu** : aucun changement aux fichiers finaux. Les versions
120 sont seulement agrandies ×2 nearest-neighbor pour pouvoir comparer
les deux résolutions côte à côte.

| Bouton | 120 actif | 120 inactif | 240 actif | 240 inactif |
|:--|:--:|:--:|:--:|:--:|
| **START** | ![START actif 120](previews/button_start_active_120.png) | ![START inactif 120](previews/button_start_inactive_120.png) | ![START actif 240](previews/button_start_active_240.png) | ![START inactif 240](previews/button_start_inactive_240.png) |
| **LOAD** | ![LOAD actif 120](previews/button_load_active_120.png) | ![LOAD inactif 120](previews/button_load_inactive_120.png) | ![LOAD actif 240](previews/button_load_active_240.png) | ![LOAD inactif 240](previews/button_load_inactive_240.png) |
| **OPTIONS** | ![OPTIONS actif 120](previews/button_options_active_120.png) | ![OPTIONS inactif 120](previews/button_options_inactive_120.png) | ![OPTIONS actif 240](previews/button_options_active_240.png) | ![OPTIONS inactif 240](previews/button_options_inactive_240.png) |
| **HELP** | ![HELP actif 120](previews/button_help_active_120.png) | ![HELP inactif 120](previews/button_help_inactive_120.png) | ![HELP actif 240](previews/button_help_active_240.png) | ![HELP inactif 240](previews/button_help_inactive_240.png) |

Le texte exact du graphisme est **OPTIONS** au pluriel.
La composition d'origine de l'utilisateur (1024 × 572) est disponible
[chez Adobe](https://at.adobe.com/Y8K1xtmZWe8uhufl). Détourage du
damier et préparation des états avec Adobe ; ces preuves sont
ensuite régénérées **exclusivement à partir des BMP du dépôt Git**.

## Organisation : SD propre, atelier séparé

| Dossier | Rôle | À copier sur la SD ? |
|:--|:--|:--:|
| [sdroot/doomrpg/img/](../../sdroot/doomrpg/img/) | **19 BMP finaux** : 18 du menu + credits.bmp | **Oui, pour les images utiles au runtime** |
| artwork/menu-uac/ | Ce README, les aperçus PNG, les preuves de montage, le script et les empreintes | **Non** |
| [ESP32/SD_BOOT_CREDITS.md](../../ESP32/SD_BOOT_CREDITS.md) | Instructions de chargement du BMP des crédits | **Non** |

Le chemin SD cible est **/doomrpg/img/** : le répertoire nommé
sdroot n'est **pas** lui-même à copier comme un dossier sur la carte.
Les images de menu sont actuellement des assets **non utilisés par le
firmware** ; les copier n'active pas leur affichage. Aucun fichier
Markdown, Python ou PNG de travail ne reste dans sdroot/doomrpg/img/.

## Preuve de composition et reproductibilité

Les points d'ancrage sont **locaux au cadre** (x, y). Les quatre
boutons sont dessinés **avant** le cadre, de sorte que les rails et
reflets métalliques passent par-dessus leurs bordures.

| Cadre | Dimensions des boutons | START | LOAD | OPTIONS | HELP |
|:--|:--|:--|:--|:--|:--|
| 120 | 55 × 20 | (12, 10) | (12, 37) | (12, 64) | (12, 91) |
| 240 | 110 × 40 | (24, 20) | (24, 74) | (24, 128) | (24, 182) |

**Ordre des couches :** fond sombre simulé → quatre BMP du menu
avec transparence → BMP métallique du cadre avec transparence.
Les planches prouvent donc la composition en logiciel des images
**existantes**, pour les quatre états de sélection et les deux tailles.
Elles ne constituent **pas** une preuve matérielle sur le CYD et les
positions restent à valider sur l'appareil.

Reproduction sur ordinateur depuis la racine Git (Python 3.12 et
Pillow 11.3.0) :

    python -m pip install Pillow==11.3.0
    python artwork/menu-uac/tools/render_previews.py --write
    python artwork/menu-uac/tools/render_previews.py --check

L'[outil de composition](tools/render_previews.py) contrôle les
**18 SHA-256 d'origine** stockés dans
[SHA256SUMS.txt](SHA256SUMS.txt), les dimensions, les masques de
transparence BMP V4 32 bits BI_BITFIELDS et la géométrie des découpes.
Il génère **16 aperçus de boutons**, **2 compositions START** et
**2 planches de preuves avec les quatre sélections**, soit 20 PNG.
La vérification reconstruit les 20 PNG **octet pour octet**.

### Traçabilité

Les notices d'origine et leurs métriques sont archivées hors de sdroot :

- [Cadres : notes et SHA-256](notes/MENU_FRAMES.md)
- [START : notes et SHA-256](notes/START_BUTTONS.md)
- [LOAD, OPTIONS, HELP : notes et SHA-256](notes/MENU_BUTTONS_COMPLEMENT.md)

Actions GitHub qui ont généré et vérifié les BMP :
[cadres — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44565988982),
[START — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44569358998),
[LOAD/OPTIONS/HELP — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44572105162).
Preuve de composition dérivée de ces BMP :
[CI générateur — SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44575761589).

**Limites :** aucune implémentation BMP32 alpha dans le runtime,
aucun changement au menu, au rendu, à la gestion des touches ou au
firmware ESP32. Le décodeur SD par lignes (sans grosse allocation),
l'intégration menu et le test matériel normal esp32-cyd restent
des jalons séparés.
