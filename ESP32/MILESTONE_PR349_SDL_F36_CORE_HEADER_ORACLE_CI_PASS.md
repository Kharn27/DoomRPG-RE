# SDL-F36 — SDL.h seul dernier header SDL de compatibilité, oracle hôte CI PASS

**15 novembre 2026 — [PR #349](https://github.com/Kharn27/DoomRPG-RE/pull/349)** sur `agent/esp32-sdl-f36-sdl-header-host-oracle`, base vérifiée après fusion #348 : `main=3fb287dc759cc6bd6d121b18a4a3f4d8fb8f9355`.

## Frontière et comportement inchangé

- Renommage Git **0 ligne modifiée** : `ESP32/include/SDL.h` → `ESP32/test/oracles/SDL.h`, Git blob strict `da1040d4fa36f70cafe1141f0d6e573741a3d575`.
- F35 avait déjà sorti `SDL_mixer.h` ; désormais aucun `SDL.h` / `SDL_mixer.h` ne reste dans `ESP32/include`.
- Aucune source `ESP32/src`, source desktop `src`, configuration `platformio.ini`, `Sound.h`, tableau de textures, framebuffer, gamma LCD, PAK, gameplay, audio, ni profil de dalle modifié.
- Pré-script `ESP32/scripts/build_engine.py` fail-closed pour retour `SDL.h` en production ou perte de l'oracle. `doomrpg_oracle_retirement_host.py` exécute les deux erreurs avec le vrai pré-script.
- Probes historiques SDL-A, F4-F31/F35, géométrie, Render/Root/RNG/Canvas gardent leurs tests d'ABI C/C++ exacts et chargent `test/oracles` explicitement. Les anciennes sources hôte peuvent encore importer `<SDL.h>` dans leurs modes non natifs : **le header n'a pas été supprimé comme référence**.
- Nouveau `sdl_f36_sdl_header_oracle_host.py` : blob entier exact et taille/compatibilité des types SDL_Rect, SDL_Texture, SDL_Color, SDL_Surface, SDL_PixelFormat et SDL_Palette en C11 desktop/geometry. Les tests de vrais fichiers `.d` F30 restent l'autorité sur l'absence d'import SDL lors de la compilation Xtensa.
- En CI, les commandes `cc` host-only ont un chemin `-IESP32/test/oracles`. Aucun ajout de ce chemin à la compilation firmware PlatformIO. Cette distinction évite qu'un import natif accidental soit masqué par un chemin d'oracle partagé.

## Preuves de validation du code

**Candidat final testé : `629c7562215319c88c5b17e7147b43c995cbd101`.**

- [Workflow PR 43765337965](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43765337965) : **175/175 SUCCESS**.
- [Workflow push 43765333693](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43765333693) : **174/174 SUCCESS**.
- Quatre profils : `esp32-cyd`, `esp-cyd-inverted`, `esp32-cyd-bringup`, `esp32-cyd-trace` ; parité stricte des moteurs release, compiledb/.d native SDL-free, audits ELF et oracles historiques.
- Premiers échecs CI F36 liés à la relocalisation de l'include dans des probes hôte de RNG, monstres et démarrage ; corrigés sur cette branche avant le double PASS. Aucun changement correctif firmware.

## Séparation matérielle et suites

Ce milestone isole une dépendance **de développement et de test seulement** : pas de nouveau flash ni PASS Serial matériel ; aucun bénéfice de RAM/flash ni changement de comportement natif revendiqué. Un test optique des panneaux/gammas n'est pas pertinent ici.

Après ce double PASS, seul un commit **Markdown-only** ajoute ce rapport, `PORTING_STATUS.md`, `DOCUMENTATION.md` et `test/oracles/README.md`. Revalider les deux CI au HEAD documentaire avant merge. **Ne pas fusionner sans approbation explicite**.

Le nettoyage SDL suivant ne doit pas effacer aveuglément des déclarations desktop : rechercher les frontières legacy restantes avec une preuve de consommateurs réels, ou clore l'effort SDL si seules des références de comportement subsistent.
