# Bouton UAC générique — 120 / 240

Le bouton est extrait du START UAC existant et nettoyé avec Adobe : plus de lettres, pentagramme, icône ou logement circulaire, seulement le métal et ses bordures. La police Doom RPG sera dessinée **séparément** par le jeu dans un milestone ultérieur.

| Version | Dimensions natives | PNG source | BMP SD |
|---|---|---|---|
| Écran 120 (×2 LCD) | **55 × 20 px** | [PNG 120](sources/menu_generic_120.png) | [BMP 120](../../../sdroot/doomrpg/img/menu_generic_120.bmp) |
| Écran 240 (direct LCD) | **110 × 40 px** | [PNG 240](sources/menu_generic_240.png) | [BMP 240](../../../sdroot/doomrpg/img/menu_generic_240.bmp) |

## Aperçus

**240 natif :**

![Bouton UAC 240 sans texte ni icône](sources/menu_generic_240.png)

**120 natif :**

![Bouton UAC 120 sans texte ni icône](sources/menu_generic_120.png)

La source est un PNG RGBA, alpha conservé. Les BMP sont empaquetés au format identique aux boutons actuels : **BITMAPV4HEADER (108 B), BGRA32, BI_BITFIELDS, alpha straight, bottom-up**. Taille attendue : **17 722 octets (240)** et **4 522 octets (120)**.

Conversion binaire sans retouche graphique et contrôle reproductible :

    python -m pip install Pillow==11.3.0
    python artwork/menu-uac/generic/tools/export_generic.py --check

L'option --write reconstruit les BMP et le manifeste SHA256SUMS.txt. Sources, README, script et empreintes restent dans artwork/menu-uac/generic ; seuls les deux BMP vont dans /doomrpg/img/ sur la SD.

## Mémoire et périmètre

Une seule image générique retenue coûterait **8 800 octets** en RGB565 pour l'écran 240 ou **2 200 octets** pour l'écran 120, indépendamment de START / LOAD / OPTIONS / HELP. Les états inactifs seraient dérivés par assombrissement logiciel, et la police serait dessinée à la volée.

**Ce PR est graphique uniquement.** Il ne remplace aucun des 16 BMP historiques et ne modifie ni le rendu, ni la RAM du firmware. La PR #406 actuelle utilise encore 4 patches actifs × 8 800 = 35 200 octets : l'économie future reste à développer puis mesurer sur le vrai CYD. Les nouveaux BMP sont facultatifs jusqu'à l'intégration.
