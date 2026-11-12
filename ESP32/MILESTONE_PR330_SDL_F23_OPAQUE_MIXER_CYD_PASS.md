# SDL-F23 — Sound.h ne ramène plus SDL.h dans le firmware ESP32 : PASS classic CYD

## Provenance du milestone

- PR https://github.com/Kharn27/DoomRPG-RE/pull/330 ; suivi https://github.com/Kharn27/DoomRPG-RE/issues/269
- Base **réelle** du main après fusion F22 : `b0690500662e3ec05e65eb16617efb374029a818`.
- **Firmware réellement testé sur classic CYD normal `esp32-cyd` :** `86f2f22ecf5ebe3f01cc054682b5a530951d5558`.
- Audio : toujours **intent natif silencieux** ; F23 **ne réintroduit pas** SDL mixer ni FluidSynth à l'exécution.
- Avant, `ESP32/src/engine_stubs.c` n'importait déjà `SDL.h` directement qu'en desktop/oracle F21, mais via `Sound.h -> SDL_mixer.h -> SDL.h`, le firmware récupérait quand même le gros header SDL en transitive. `ESP32/include/SDL_mixer.h` ne requiert réellement qu'une **déclaration opaque `Mix_Chunk`**. F23 conditionne son `#include <SDL.h>` à `!DOOMRPG_ESP32 || DOOMRPG_ESP32_GEOMETRY_ORACLE`.
- **Seul fichier produit changé** : `ESP32/include/SDL_mixer.h`. Historique `SDL_mixer.h` restituable bit-pour-bit sous le blob Git `0f41248cbbb6ecd27e7de86305bb51b479fcc76a`. Aucun changement `src/Sound.h`, `Sound_t`, `SoundChannel_t`, `engine_stubs.c`, canvas, map, RNG, asset, allocation, codec V11, entrée, son ou rendu.
- **Oracle C11 réel** `sdl_f23_opaque_mixer_header_host.py` reconstitue le header pré-F23 exact, compile et exécute les vrais `src/DoomRPG.h`, `src/Sound.h` et shims existants, **avant/après** en natif/geometry-oracle/host ; vérifie `SDL.h` absent après uniquement dans le cas natif et conservé dans les deux autres. Layouts C11 identiques : SoundChannel_t taille **24 B**, alignement **8**, offsets mediaAudioMusic 8, size 16, flags 20, Sound_t taille **304 B**, offset volume **280** (plateforme hôte 64 bits ; ne pas transposer comme mémoire CYD).
- CI pré-matériel [push #43293111129](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43293111129) et [PR #43293193308](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43293193308) **SUCCESS 158/158 chacune**. Builds normaux `esp32-cyd`, `-bringup`, `-trace`, compiledb et ELF ; 0 fonction SDL liée et moteur BMP natif PAK conservé. Tailles **identiques F22** : normal flash/RAM statique 859693/80736 B, bringup 882477/80880, trace 877845/80776. Aucun gain heap8 annoncé.

## Test matériel normal — vérité Serial

- Fichier `platformio-device-monitor-261112-210204.log` ; **1674 lignes, 189168 octets, SHA256 `e619fa7e12ca0acef4a8e2a597bac07906b91c75618d81139400c9e3f06abe3f`**.
- Boot ROM unique `rst:0x1 (POWERON_RESET)`, ESP32-D0WD-V3 rev3 240MHz, flash 4194304 octets, PSRAM 0, SD prête, DoomRPG-ESP32.pak 241 entrées, framebuffer 160x120 RGB565 38400B (320x240 x2). Aucun reboot ultérieur.
- Menu HELP/ABOUT : scroll des 112 lignes jusqu'à page finale, retour MENU_MAIN réussi. START GAME → intro complète → dispose + transition → carte `/intro.bsp` Entrance, source CRC `623f34e4`, map compact arena 14095B ; séquences move/turn, caisse subtype2 transformée en armor shard, dialogues EV_DIALOG (8), EV_DIALOGNOBACK (26), dialogue autonome de Fire Ext, 6 portes animées COMPLETE, 11 pickups miniature complets.
- Premier HUB SYSTEM→EXIT confirmé avec `saveWrite=no`, nettoyage `[RESIDENTRESET] heap8=112552->130560 released=18008 ... empty=1` et `[SYSEXIT] MENU-READY frame=4c01eb2b`.
- MENU_MAIN→LOAD GAME slot04 V11 `5604` octets, restauration map/player/scripts/line states/action-removals/crates/automap/monsters/topology/positions/activation/drops exacte (rapport `NATIVESAVE LOAD`), gameplay RESUME normal avec armement du cache, budget mémoire.
- Après LOAD : arme changée, porte, Malwrath sprite322 combattu à la hache `[MONSTERCOMBAT] COMMIT ... hp=11->0 ... alive=1->0` puis `DEATH-SETTLE` et `DEATH-FRAME`; pas de `MONSTERDROP COMMIT`, le jet est `dropMaterialize=none` dans cette capture. Second HUB SYSTEM ouvert ; **dernières lignes = `CONFIRM-ARM page=system row=EXIT`, pas de second `EXIT-CONFIRMED` ni `SYSEXIT`**.
- 2× `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`, marges heap8 **28092 B** et **27280 B**, largest8 en gameplay **61428 B** ; 2× `ENGINESESSION READY` avec `shapeData=0x0 mediaTexels=0x0`. `[ALIVE]` jusqu'à **154572 ms** avec `heap8=84624 largest8=61428` session active après LOAD.
- Comptage précis : **3× `[DIALOG] OPEN`, 3× `[DIALOG] CLOSE`, 6× `[DOORANIM] COMPLETE`, 11× `[PICKUPMINI] START` et 11× END, 1× `[MONSTERCOMBAT] COMMIT`**, 0× MONSTERRETAL COMMIT, 0× MONSTERDROP COMMIT.
- Aucun reset supplémentaire, Guru Meditation, `panic`, watchdog, `failed`, `fail=SPRITES`, `SETTLE-FAILED` ou `DEATH-REDRAW-FAILED` dans le log. 10 messages `[NATIVEFRAME] BOUNDARY-INLINE` connus hors F23 et maintenus comme signal de diagnostic. Aucune visite OPTIONS, NOTE, rare drop defTile=100, SAVE neuf ni sortie finale du deuxième HUB ; audio matériel non testé.

## Conclusion / gate

PASS hardware sur les chemins effectivement empruntés et CI success pour F23 : frontière de préprocesseur SDL supprimée sans toucher au runtime. Ne pas confondre avec suppression des headers SDL historiques nécessaires au desktop et aux oracles ; ne pas affirmer tous chemins SDL-free. **Après PASS, seuls 3 Markdown de documentation sont ajoutés ; revalider CI puis fusion explicite.** Pour la suite, récupérer le SHA exact de `main` après merge avant toute nouvelle branche `agent/*`.
