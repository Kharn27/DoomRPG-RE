# SDL-F33 — ancien shim SDL déplacé en oracle hôte, CI PASS

**15 novembre 2026.** [PR #345](https://github.com/Kharn27/DoomRPG-RE/pull/345), branche `agent/esp32-sdl-f33-shim-host-oracle-isolation`. Point de départ GitHub réel : `main=c69844364118a56b30503065aeed95fbfea70a27`, fusion de la PR #344.

## Frontière de code immuable

- `ESP32/src/esp32_sdl.cpp` est déplacé vers `ESP32/test/oracles/esp32_sdl.cpp` ; Git détecte un **renommage pur (0 insertion / 0 suppression)**. Ancien et nouveau blob Git : `b8385250706b8b5396c3f3808e5084ad1f318425`.
- SDL-F3 avait déjà exclu ce shim des firmware ELF. Ici, on retire sa présence trompeuse dans `ESP32/src`, mais on **garde le code de référence byte-for-byte pour l'exécuter en hôte**.
- `ESP32/scripts/build_engine.py` exige dorénavant l'absence de `ESP32/src/esp32_sdl.cpp` et l'existence de `ESP32/test/oracles/esp32_sdl.cpp`. `doomrpg_oracle_retirement_host.py` vérifie les deux refus adversariaux (shim ressuscité dans `src`, oracle hôte manquant) via l'exécution du vrai pré-script SCons.
- Les tests SDL-A/B/C/D/E/F et `native_main_help_host.py` lisent désormais l'oracle dans son nouveau répertoire ; `sdl_shim_permanent_host.py` continue de **compiler et exécuter la vraie implémentation C++** avec les stubs Arduino/TFT. Le garde SDL-F3 épingle le Git blob exact et la localisation.
- `ESP32/platformio.ini` garde explicitement `-<esp32_sdl.cpp>` et `-<esp32_bmp.cpp>` comme barrières fail-closed. Les headers `ESP32/include/SDL.h` et `ESP32/include/esp32_sdl_platform.h` demeurent nécessaires aux anciens oracles hôte ; leur éventuel déplacement est un autre jalon.

## Preuves CI, pas de surinterprétation

**Code vérifié au SHA `713ee11b2224e0d2d5d086c0de162b722f5d3dd1`** par [GitHub Actions run 43758021712](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43758021712) : **172 étapes SUCCESS / 172**, aucun échec. Cela inclut les vrais tests historiques SDL, l'oracle C++ shim, le test du pré-script SCons, quatre builds PlatformIO (`esp-cyd-inverted`, `esp32-cyd`, bringup et trace), la parité des unités/defines C/C++ du moteur commun, et les contrôles ELF/compiledb.

Cette modification touche **zéro corps de fonction d'une unité de production compilée**. Aucun changement gameplay, renderer, framebuffer 160×120 RGB565, écran, configuration LCD gamma, niveau PWM, input, SD/PAK, ZIP, audio, save ou RNG. Pas de nouveau `shapeData`, `mediaTexels`, allocation map-wide ou PSRAM. On ne revendique **aucun gain RAM/flash ni nouveau PASS hardware** : le shim était déjà absent des ELFs. Un nouveau flash du CYD est donc sans intérêt pour cette seule séparation de fichiers.

## Après la preuve

Commits suivants : documentation Markdown uniquement, sans mutation des fichiers de production ou des oracles hôte. CI du HEAD documentaire à confirmer avant clôture ; ne jamais présenter une vérification en cours comme PASS. PR à laisser non fusionnée, en attente de la décision utilisateur.

Prochaine direction : audit séparé des **headers SDL host-only** puis réduction bornée des reliquats desktop, en préservant les vrais tests comportementaux.
