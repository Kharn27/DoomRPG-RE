# PR #341 — deux builds CYD, **un seul moteur ESP32** (15 novembre 2026)

## Garantie matérielle / logicielle avant fusion

Les profils de production sont **`esp32-cyd`** et **`esp-cyd-inverted`**. Le second déclare `extends = env:esp32-cyd` puis reprend explicitement `${env:esp32-cyd.build_flags}`. Il ajoute exclusivement le sélecteur `DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1` et un chemin d'inclusion de bibliothèque TFT_eSPI correspondant à son environnement de compilation autonome. Le même `ESP32/src` est la **source unique** de gameplay, événements, rendu logique, framebuffer 160×120 RGB565, PAK SD / partition brute, toucher, sauvegarde et audio.

La différence de sortie est confinée à `ESP32/src/platform_video.cpp::applyCydInvertedGammaProfile()` :
- normal : **INV ON**, historique **E1 dans E0+E1** (calibration du premier CYD, ne pas changer sans test matériel) ;
- inverted : **INV OFF**, gamma candidate **paire E0/E1 ILI9341_DRIVER publiée par TFT_eSPI** (pas de traitement des pixels). Le code sélectionne son panneau à la compilation et réapplique la configuration après changement d'orientation.
- La variante inverse n'a **ni moteur séparé, ni sources héritées compilées à part, ni PAK différent**. Les binaires ne doivent pas être identiques : les commandes LCD diffèrent volontairement.

## Nouveau garde-fou CI durable

`ESP32/test/cyd_dual_build_core_parity_host.py` compare les *vraies* bases `compile_commands.json` produites successivement par PlatformIO pour les deux profils de production :

1. Énumère tous les vrais `ESP32/src` compilés et exige le **même ensemble de C/C++ translation units**.
2. Pour chacune, compare les définitions préprocesseur `-D`, et n'autorise que `DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION` et la métadonnée d'environnement `PIOENV` à différer.
3. Refuse une unité moteur manquante, un `#define` gameplay supplémentaire, l'absence de sélecteur LCD ou une base de compilation anormalement petite.
4. Le test synthétique `--self-test` vérifie explicitement ces refus, et le test réel utilise les compilations de firmware faites dans la même CI.
5. L'ancien test `cyd_alternate_panel_inversion_host.py` vérifie les **octets réels de programmation du panneau** dans les deux configurations ; l'environnement inversé compile de manière autonome en premier, puis le normal. Tous les artefacts firmware figurent dans le même workflow GitHub Actions.

**Résultat candidat source `9eed5bdff3a519ec7a52573abadae2bf10e4f64f`** : exactement **171 unités C/C++ natives identiques** et mêmes définitions hors LCD, les deux CI [PR](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43746978622) et [push](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43746974168) **172/172 SUCCESS** chacune. Aucun changement gameplay ni gamma dans ce milestone de garantie ; seulement test hôte et workflow CI.

## Décision de fusion / suite

Le vrai CYD normal a validé R-A2b sur le matériel (voir [milestone R-A2b](MILESTONE_PR341_ARCH_RA2B_CYD_PASS_AND_PANEL_VARIANT.md)). L'utilisateur préfère visuellement le profil inverted sur les comparaisons, mais **il faut rattacher sans ambiguïté ce jugement au firmware/gamma exacte du dernier HEAD** : les branches ont connu plusieurs essais E0/E1, dont un essai plus récent que le premier comparatif photographique. Ne pas inventer de PASS optique de la gamma `ILI9341_DRIVER` sans preuve de version ou nouveau test sur le second CYD. La CI prouve le comportement du code et la séparation des builds, pas l'exactitude chromatique.

**La correction du rendu historique `esp32-cyd` sera un milestone/PR distinct, à partir du SHA exact de `main` après fusion explicite** : modifier exclusivement les registres gamma et/ou INV du premier panneau, tester les deux CYD à luminosité égale, vérifier les FNV du framebuffer inchangés, et ne jamais modifier les fonctions de rendu ni le contenu PAK pour compenser une dalle. Aucun `merge` sans demande explicite.

Le probe `DOOMRPG_RA2B_CAPTURE=1` demeure temporairement actif dans les deux profils ; sa retraite avant une release générale doit se faire avec son propre contrôle de non-régression. Les deux profils continueront à hériter de toute correction moteur automatiquement, sans création de nouveau projet.
