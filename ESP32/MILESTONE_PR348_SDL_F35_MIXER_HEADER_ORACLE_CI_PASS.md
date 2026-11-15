# SDL-F35 — SDL_mixer.h classé comme oracle hôte, CI PASS

**15 novembre 2026 — PR [#348](https://github.com/Kharn27/DoomRPG-RE/pull/348)**, branche `agent/esp32-sdl-f35-mixer-oracle`, depuis le vrai GitHub `main=593631bc67c61a23c10acc04780fefa7966bf358` après fusion SDL-F34 (#346).

## Frontière architecturale

- Uniquement `ESP32/include/SDL_mixer.h` → `ESP32/test/oracles/SDL_mixer.h`, renommage byte-for-byte, **blob Git `59fe49318cb550277f999b5c6307c746eb7f6ad8`, 0 ligne modifiée**.
- `src/Sound.h` **strictement inchangé**. En mode ESP32 native, SDL-F31 a déjà fourni une déclaration opaque `Mix_Chunk`; le firmware n'importe aucun header SDL_mixer.h. En desktop et geometry, la vraie déclaration de test reste incluse avec `-I ESP32/test/oracles`, sans variante de contenu, ni changement de taille/offset du pointeur.
- F23 (`sdl_f23_opaque_mixer_header_host.py`) et F31 (`sdl_f31_native_sound_opaque_header_host.py`) continuent de reconstruire le blob historique avant évolution et compilent/exécutent le vrai C11 dans les trois modes. Le nouveau `sdl_f35_mixer_oracle_host.py` impose l'emplacement, le hash, l'absence d'import dans les sources/headers ESP32 de production et les mêmes layouts C11.
- `ESP32/scripts/build_engine.py` refuse le retour du header sous `ESP32/include` ainsi que la disparition de la copie oracle ; `doomrpg_oracle_retirement_host.py` vérifie ces deux refus avec le vrai pré-script SCons dans un faux projet.
- `ESP32/include/SDL.h` reste présent : plusieurs tests desktop/geometry en dépendent encore. Son éventuel déplacement exige un jalon propre, et n'est **pas** couvert par F35.

## Preuves GitHub

**Code validé : `0b9ab6d01407b86e43293d47ed20c0d886125f88`.**

- [GitHub Actions PR run 43762478877](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43762478877) : **173/173 SUCCESS**.
- [GitHub Actions push run 43762438757](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43762438757) : **173/173 SUCCESS**.
- Suite hôte SDL historique, nouveaux cas de refus, quatre environnements PlatformIO (`esp32-cyd`, `esp-cyd-inverted`, `esp32-cyd-bringup`, `esp32-cyd-trace`), inventaires compiledb et ELF, zéro fonction SDL liée et parité des unités/flags moteur pour les deux builds de production.
- Diff source : aucun fichier `ESP32/src` ni `src/Sound.h` modifié ; `SDL_mixer.h` ne change aucun octet, seules les voies de test/CI/build-gate changent.

## Limites et suite

**CI-only.** Aucun nouveau test de la vraie carte n'est exigé pour ce changement d'organisation hôte. Ne pas inventer de nouveau PASS Serial, gain RAM/flash, modification gameplay/render/audio, de la dalle ou des données PAK. Le runtime ne réintroduit ni SDL ni ZIP. Documentation et milestone ajoutés **après** double CI du code ; seuls des fichiers Markdown changent dans le commit post-validation. Vérifier la CI du HEAD final avant fusion. La PR n'est **pas fusionnée** sans demande de l'utilisateur.

La frontière restante à auditer est `ESP32/include/SDL.h` et ses consommateurs desktop/geometry, sans suppression automatique.
