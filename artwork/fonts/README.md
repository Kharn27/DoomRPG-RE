# Polices Doom alternatives — CYD

Variantes de planches BMP destinées aux essais de thème, **sans remplacer** les ressources existantes ni modifier le moteur. La PR #407 ajoute aussi `sdroot/doomrpg/img/a_doom.bmp` (144 × 72), distinct de cette famille `larger_font`.

## Fichiers à copier sur SD

| Fichier | Dimensions | Encodage | Taille | Usage |
| --- | --- | --- | ---: | --- |
| `larger_font_doom.bmp` | 208 × 102 | BMP indexé 4 bpp, palette de 16 couleurs | 10 726 o | **Essai prioritaire**, chrome et orange |
| `larger_font_doom_compat3.bmp` | 208 × 102 | BMP indexé 4 bpp, palette de 3 couleurs | 10 676 o | Variante conservatrice, palette/poids alignés sur `larger_font.bmp` |
| `larger_font_doom_24bit.bmp` | 208 × 102 | BMP RGB 24 bpp | 63 702 o | Fidélité visuelle, compatibilité du décodeur à vérifier |

Les 96 glyphes ont été repositionnés par rapport à `larger_font.bmp` (208 × 102) ; fond rose magenta. Les dimensions identiques **ne garantissent pas** à elles seules la prise en charge runtime : palette, couleur transparente, chargement et ordre des glyphes doivent être confirmés sur le vrai CYD.

![Aperçu agrandi du thème Doom](previews/larger_font_doom_zoom.png)

## Surcharge native sur SD (branche `agent/font-sd-overrides-pak-fallback`)

La livraison contient maintenant **directement** les deux fichiers sous les noms exacts attendus par le runtime :

| Planche demandée | Fichier prêt à copier sur la SD | Format |
| --- | --- | --- |
| `a.bmp` | `sdroot/doomrpg/img/a.bmp` → `/doomrpg/img/a.bmp` | 144 × 72, 4 bits |
| `larger_font.bmp` | `sdroot/doomrpg/img/larger_font.bmp` → `/doomrpg/img/larger_font.bmp` | 208 × 102, 4 bits |

Il suffit de **synchroniser le dossier `sdroot/doomrpg/img` vers la carte SD**. Les fichiers `a_doom.bmp`, `larger_font_doom.bmp` et les autres variantes sont gardés comme copies de référence. Le fichier `larger_font_doom_24bit.bmp` demeure une référence visuelle, **pas** un fichier runtime.

Le lecteur natif accepte les BMP indexés non compressés **1, 4 ou 8 bits**. Un fichier absent, incompatible, aux mauvaises dimensions ou corrompu lors du premier décodage entraîne un fallback sur les polices d'origine de `/DoomRPG-ESP32.pak`. **Aucune lecture ZIP runtime.**

Le Canvas décode une texture indexée compacte une seule fois ; HUD/dialogues/HUB lisent les plages glyphes à la demande. Les handles sont suspendus avant toute ouverture physique SD du PAK ; lorsqu'un PAK est physiquement ouvert sur SD, les glyphes sont lus depuis le PAK pour éviter une concurrence sur le lecteur. En mode PAK flash-backed, la surcharge SD est disponible. Les fichiers changés pendant une session ne sont pas rechargés à chaud : **redémarrer** après copie.

Logs attendus (à confirmer sur vrai CYD) :

```text
[FONTSD] ACTIVE name=a.bmp source=SD ...
[FONTSD] ACTIVE name=larger_font.bmp source=SD ...
[FONTSD] FALLBACK name=... reason=missing-or-size backing=PAK
[FONTSD] FALLBACK name=... reason=invalid-or-io backing=PAK
```

**PENDING CYD** : tester absence des deux fichiers, présence des deux, un seul manquant, RAM et audio lors des dialogues/HUD/HUB.

## Essai matériel

Conserver une copie de `larger_font.bmp` et ne substituer que le fichier de la variante choisie sous ce nom **là où le moteur lit réellement cet asset**. Les fichiers de variantes sont intentionnellement ajoutés à `sdroot/doomrpg/img/` sous des noms séparés ; aucun mécanisme de sélection automatique n'est ajouté ici. Contrôler sur l'écran réel : caractères (majuscules/minuscules, ponctuation), alignement, transparence magenta et lecture du BMP par le décodeur.

**État : PENDING CYD**. Aucun PASS matériel ni changement firmware revendiqué. Éviter de merger pour une validation visuelle non observée si un gate matériel est exigé.

## SHA-256

```text
82cfce34ba230b56d4a152310f9df0b104782e308fde3c1c88c19fdd9334d2a9  larger_font_doom.bmp
283df2f765238a46694384972ce76b0c34ff6f17a4ad0a183b4a572f3ee6f669  larger_font_doom_compat3.bmp
cfbf9773633908368a47ce3cabd2a84f6a503b251c21db4adf53b1ca2c902721  larger_font_doom_24bit.bmp
f0c91cbbd830145d8e62a1aaa8f21fe6b9dad16d75c61c9cf1ea4d5e300e5c14  previews/larger_font_doom_zoom.png
```
