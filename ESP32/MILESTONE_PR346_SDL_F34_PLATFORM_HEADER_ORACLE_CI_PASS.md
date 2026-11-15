# SDL-F34 — header d'attachement SDL sorti des includes de production, CI PASS

**15 novembre 2026.** [PR #346](https://github.com/Kharn27/DoomRPG-RE/pull/346) sur `agent/esp32-sdl-f34-platform-header-oracle`, depuis le vrai GitHub `main=84390da3937913db64ecc18876ac328c0f534765`, fusion vérifiée de #345.

## Frontière

- Déplacement **octet-pour-octet** `ESP32/include/esp32_sdl_platform.h` → `ESP32/test/oracles/esp32_sdl_platform.h` (Git rename **0 ligne modifiée**, blob `6d89897ebbcccaba4dd7e041520fc7776bae6142`).
- Ce header déclarait uniquement `Esp32Sdl_attachDisplay(TFT_eSPI*)`, fonction de l'ancien shim SDL-F33 `ESP32/test/oracles/esp32_sdl.cpp` ; ce shim demeure byte-identique, ne revient pas en production et est **encore compilé et exécuté** par `sdl_shim_permanent_host.py`.
- `ESP32/scripts/build_engine.py` refuse explicitement tout header de même nom sous `ESP32/include` et exige sa présence dans `ESP32/test/oracles` ; `doomrpg_oracle_retirement_host.py` exécute le vrai script sur un projet adversarial avec les deux fautes, chacune devant être rejetée.
- `sdl_f3_uncompiled_shim_host.py` vérifie désormais le blob complet inchangé, son absence des includes firmware et l'absence d'import de ce header dans toutes les sources `ESP32/src` et `ESP32/include`. `sdl_dead_demo_pattern_host.py` vérifie toujours la déclaration historique via l'oracle.
- Les headers `ESP32/include/SDL.h` et `ESP32/include/SDL_mixer.h` **restent** disponibles ; leurs tests de compatibilité desktop/geometry (`sdl_f23_opaque_mixer_header_host.py` et `sdl_f31_native_sound_opaque_header_host.py`) utilisent de vrais modes C11 et ne justifient pas une suppression prématurée. Pas de suppression SDL massive.

## Preuves exactes

**Commit de code `597798ac4e283d79fffb47d033167c06be8b0b0e`**. [GitHub Actions run 43759889292](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43759889292) : **172/172 étapes SUCCESS**, aucun échec : tests hôte SDL historiques, exécution réelle du shim C++, faux SCons adversarial, quatre builds PlatformIO (`esp32-cyd`, `esp-cyd-inverted`, bringup, trace), comparaison des deux moteurs C/C++/defines et audits compiledb/ELF.

**Aucun fichier source natif compilé n'a été modifié**, aucune logique de rendu, PAK/SD, gameplay, automap, audio, input, transition, save, RNG, gamma LCD, PWM ou allocations natives. Le framebuffer reste 160×120 RGB565, le moteur commun des deux CYD est préservé et les invariants `shapeData==NULL` / `mediaTexels==NULL` ne sont pas modifiés.

Ceci est une validation **CI-only** d'une frontière source. Aucun changement du binaire utile, gain RAM/flash, nouveau PASS colorimétrique ni test matériel revendiqué. Pas de flash nécessaire pour F34 seul.

## Clôture

Les commits **après** ce PASS code ne doivent modifier que les fichiers Markdown (ce milestone, `PORTING_STATUS.md`, `DOCUMENTATION.md`). Vérifier leur diff et le succès de la CI HEAD final avant de déclarer la PR merge-ready. Le merge sur `main` reste à la discrétion de l'utilisateur.

**Piste suivante, à auditer depuis le futur main :** les derniers include paths SDL.h/SDL_mixer.h et leur ABI en mode desktop/geometry. Ne jamais déplacer ces headers sans préserver les tests C11 exacts.
