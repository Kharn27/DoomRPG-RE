# SDL-F31 — `Sound.h` opaque sans `SDL_mixer.h` natif : CI PASS / CYD À TESTER

## Provenance et code

- Branche conservée à la demande utilisateur : `agent/esp32-sdl-f30-compiler-header-closure`, [PR #339](https://github.com/Kharn27/DoomRPG-RE/pull/339), [issue #269](https://github.com/Kharn27/DoomRPG-RE/issues/269). **Ne pas fusionner sans nouveau test matériel**.
- Milestone précédent [SDL-F30 : PASS CYD](MILESTONE_PR339_SDL_F30_COMPILER_CLOSURE_CYD_PASS.md), capture `platformio-device-monitor-261114-193843.log` de 1998 lignes et SHA256 `bc6205a052dbd306f03009e81ad9cf2c62cea3e523385fc683eaa7ace8076dab`. Le matériel F30 atteste le garde `engine_stubs.c→Z_Zip.h`, **pas la présente modification F31 de `src/Sound.h`**.
- Un seul changement de production F31 : `src/Sound.h` remplace son inclusion directe inconditionnelle de `SDL_mixer.h` par une déclaration **`typedef struct Mix_Chunk Mix_Chunk;`** en profil `DOOMRPG_ESP32 && !DOOMRPG_ESP32_GEOMETRY_ORACLE`. Pour hôte et oracle géométrique, le header réel `SDL_mixer.h` reste inclus. Aucun corps de fonction, lecteur PAK, UI, buffers, ni backend audio modifié ; `AUDIOINTENT` demeure silencieux.
- Oracle `ESP32/test/sdl_f31_native_sound_opaque_header_host.py` : inverser exactement un guard et récupérer le blob Git CRLF pré-F31 `c66ec813e06e8cb37b682bd08f5749088113ddc7`. Compilation et exécution C11 des vrais `Sound_t` et `SoundChannel_t` avant/après, en native/desktop/geometry, taille/alignement/offsets identiques :
  - `SoundChannel_t` : taille 24, alignement 8, `mediaAudioMusic` offset 8, `size` offset 16, `flags` offset 20 (environnement hôte 64 bits).
  - `Sound_t` : taille 304, alignement 8, `volume` offset 280 (environnement hôte 64 bits). Ce ne sont **pas** des allocations dynamiques ESP32.
- CI du candidat après fusion contrôlée de main `40a36643efec2a49f733377f02dc033c189ebba6` (palettes PR #336) dans la branche, merge commit **`fbb949ca1b0733a998e9c2856494946bbe5dc1c3`**. PR redevenue `mergeable=true` mais conservée en draft jusqu'au CYD.

## Vérités CI logiciel

- [Run PR #43629120395](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43629120395) **SUCCESS : 166/166 étapes** et artefacts firmware produits. Test F31 C11 dans les trois configurations PASS, et tous les anciens tests/oracles encore vérifiés.
- Audit `ESP32/test/sdl_f30_compiler_header_closure_host.py` étendu : **171 vrais `ESP32/src` C/C++ compilés et 171 fichiers GCC `.d` par profil, soit 513 fermetures transitives**. Pour chacun des trois profils, `0 SDL.h`, `0 SDL_Video.h`, `0 SDL_mixer.h`, `0 Z_Zip.h`. Anciens `esp32_sdl.cpp` et `esp32_bmp.cpp` exclus, anciennes fonctions SDL absentes des trois ELF.
- CI compilation normale `esp32-cyd` : **RAM statique 80736 B**, **flash 860177 B** ; bringup : **80880/882953 B** ; trace : **80776/878329 B**. Ces chiffres sont les coûts statiques annoncés par PlatformIO, pas des mesures du heap à l'exécution. Normal identique à F30.

## Frontière matérielle encore ouverte

- **Aucun PASS sur le vrai CYD F31 revendiqué** à la date de rédaction. Le log F30 ne peut pas confirmer la source `Sound.h` plus récente ni l'intégration des thèmes PR #336.
- Tester firmware normal `esp32-cyd` issu de cette branche, pas bringup : POWERON_RESET, 0 PSRAM, SD+PAK, MENU_MAIN HELP/BACK et Options/Theme, START/ST_INTRO, Entrance gameplay, portes/dialogues, un combat si accessible, HUB/EXIT, LOAD V11 et EXIT de retour ; vérifier `HEADROOM_OK`, `RESIDENTRESET empty=1`, `shapeData/mediaTexels=NULL`, absence de panic/WDT. Ne pas demander que l'audio joue : backend sonore toujours différé.
- Après retour Serial exact, seulement documents post-test, vérifier le diff `hardware candidate → docs` docs-only et CI finale, puis proposer merge-ready **sans fusion automatique** sauf autorisation explicite.

## Sens exact du résultat

Ce milestone établit la fermeture des **quatre headers de compatibilité/ZIP ciblés** pour les 171 unités de compilation natives du firmware. Les fichiers SDL historiques, oracles desktop et noms `SDL_` dans les anciens commentaires/fixtures continuent à exister dans le dépôt par conception ; ne pas prétendre que toute trace nominale a disparu.
