# HUD-FACE-L1 — planche originale larger HUD faces : A/B CYD (PENDING)

## Base et périmètre

- Base GitHub `main` au démarrage : `4571acb48091b7909a37127535eebccdf1005dce` (22 novembre 2026).
- Branche : `agent/esp32-hud-larger-faces-ab`.
- Spécification comportement : `src/Hud.c::Hud_startup` : normal `l.bmp`, large `larger HUD faces.bmp`. Les **9** états de `Hud_drawBottomBar` restent définis par santé, pickup et impact.
- Le chemin ESP32 existant `ESP32/src/esp_native_gameplay_hud.c` chargeait uniquement `l.bmp` depuis `DoomRPG-ESP32.pak`. Les BMP sources ne sont pas suivis comme fichiers isolés dans `sdroot/doomrpg/img`; `ESP32/tools/build_asset_pack.py` copie **toutes** les entrées du ZIP original dans le PAK v2.
- **Aucune vérification de présence, dimensions, format ni contenu visuel du vrai `larger HUD faces.bmp`** : les assets originaux privés ne sont pas accessibles à ce jalon hors matériel. Ne pas annoncer le mode actif sans le log CYD.

## Candidat de code

- Environnement normal `esp32-cyd` : essaie `larger HUD faces.bmp` dans le PAK existant, avec validation BMP indexed sans décompression complète ; repli sûr sur `l.bmp` absent, corrompu ou hors limites.
- Environnement témoin `esp32-cyd-hud-original` : hérite du normal et définit seulement `DOOMRPG_HUD_LARGER_FACE_EXPERIMENT=0`. Le code suit alors exclusivement `l.bmp`.
- Un **seul** état 0..8 est composé à la fois. S'il tient dans 18×20 pixels logiques, les pixels originaux sont copiés 1:1. Sinon, un nearest-neighbor avec préservation des proportions ramène le visage dans 18×20, au centre de l'emplacement x82..99, y100..119, sans chevauchement du bloc munitions. Le visage témoin `l.bmp` garde sa présentation exacte historique.
- Le scaler lit au plus 32×32 pixels de sortie et une rangée source à la fois via le **scratch existant de 1 KiB**, palette 256 couleurs déjà allouée dans le descripteur temporaire. Aucun atlas RGB565 retenu, deuxième framebuffer, nouvelle allocation heap, source ZIP runtime, ni nouveau propriétaire SD.
- Lecture du visage entier au format original **non** nécessaire. Le changement n'affecte pas `shapeData`, `mediaTexels`, BSP, entités, RNG, comportement des 9 états, HUD supérieur, animation combat/audio ou politique de repaint.
- Un témoin `[HUDFACE]` s'imprime au plus une fois **par état de visage et par session**, avec `mode=larger|classic`, dimensions source/affichées, `scaled`, `packReads`, `bytes`, heap8 avant/après et largest8. Le témoin ne constitue pas une mesure d'allocation statique (celle-ci se compare par les ELFs/CI).

## Contrôles avant matériel

- Hôte : `cc -O2 -std=c11 -Wall -Wextra -Werror -IESP32/include ESP32/src/esp_native_indexed_bmp.c ESP32/test/hud_face_scaled_bmp_host.c -o /tmp/doomrpg-hud-face && /tmp/doomrpg-hud-face`.
- Oracle : vraie fonction de décodage BMP indexed, 9×4 lignes synthétiques bottom-up, frame n°8, échantillonnage, nombre de lignes/lus/octet, transparence magenta, bornes, index palette invalide fail-closed.
- CI : ajouter le test au workflow existant et compiler le firmware normal. Une CI verte prouve la compilation et les oracles **mais pas** les dimensions du vrai PAK ni le rendu physique.

## Test réel requis — classique CYD sans PSRAM

1. Flasher `pio run -e esp32-cyd -t upload` depuis `ESP32/` avec le **PAK original inchangé** sur microSD ; capturer le Serial 115200 jusqu'à Entrance / mouvement / un impact. Vérifier impérativement `[HUDFACE] mode=larger`. Si `mode=classic`, la tentative a replié : remonter le log, ne pas déclarer succès visuel.
2. Photographier/évaluer au HUD 160×120 nearest x2 la netteté, les bords et le centrage du visage, ainsi que les chiffres santé/armure/munitions. Tester blessure directionnelle et ramassage si possible. Contrôler `shapeData=mediaTexels=NULL`, absence de WDT et `HEADROOM_OK`.
3. Pour A/B RAM et réactivité, flasher le témoin `pio run -e esp32-cyd-hud-original -t upload` **sans changer la SD**, jouer le même parcours, capturer les mêmes `[HUDFACE]`, `HEADROOM` et `RAM`. Comparer `heap8After/largest8`, volumes PAK par repaint et durée ressentie, en distinguant les fluctuations dues au jeu, à l'audio et à l'ordonnancement. Répéter si l'écart est faible.
4. Ne marquer `PASS` et ne documenter les résultats physiques qu'après réception de logs et retour visuel. Après PASS, docs-only et CI finale ; pas de merge vers `main` sans demande explicite.

## Limites avant test

Le format de `larger HUD faces.bmp` dans le vrai PAK n'a pas été inspecté et la qualité après réduction demeure subjective. Le coût SRAM permanent attendu est quasi nul (aucun nouveau pixel buffer), **mais son delta réel et sa performance ne sont pas revendiqués**. Si l'asset est 24 bpp, compressé ou dépasse les gardes du parseur indexé, le code conservera `l.bmp` au lieu de risquer un crash.
