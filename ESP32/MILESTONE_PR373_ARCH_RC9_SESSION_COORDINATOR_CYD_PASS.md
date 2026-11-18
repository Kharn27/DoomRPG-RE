# ARCH-RC9 — session coordinator sans linker interposition : PASS CYD borné

**18 novembre 2026 — [PR #373](https://github.com/Kharn27/DoomRPG-RE/pull/373)**. Branche `agent/esp32-arch-rc9-session-coordinator` ; base réelle GitHub `main=fd3917ef496061110dc965753a0f4379d7490a52` après fusion RC8. HEAD code `4172b2db1a0767314711c8f5d76a85be35c82574`, attribué au test par le contexte PR et le test utilisateur ; le log série ne transmet pas de SHA ni de nom d'environnement PlatformIO.

## Frontière architecturale

Deux interpositions d'une même famille retirées de `ESP32/platformio.ini` :
- `-Wl,--wrap=EspNativeGameplaySession_service`
- `-Wl,--wrap=EspNativeGameplaySession_reset`

Les corps de l'ancien wrapper externe dans `esp_native_gameplay_gib_fx.c` sont renommés `EspNativeGameplaySessionCoordinator_service/reset` sans modifier leur ordre d'appels ; seul ce coordinateur compose la chaîne publique. `main.cpp` conserve `EspNativeGameplaySession_canService()` avant l'appel direct au service. Les resets SAVE/LOAD, EXIT, changement de map, START et route death utilisent directement le coordinateur (cinq callsites de reset). La leaf `esp_native_gameplay_player_resources.c` appelle directement `EspNativeGameplaySession_service/reset` raw sans `__real_`, et conserve la composition PlayerResources/ActionEngine/MonsterTurn héritée de RC8. Les deux fonctions `__wrap_EspNativeGameplaySession_*` historiques de `esp_native_gameplay_pickup.c`, devenues dormantes, sont supprimées ; les fonctions pickup encore vivantes restent en place. Aucun nouvel ownership de mémoire, aucune mutation de gameplay, RNG, audio, moteur 3D ou barils introduite.

## Vérification hôte / CI code

- Le test `native_monster_turn_resource_chain_host.py` compile toujours trois **corps C réels** : 5 scénarios et 12 mutants de composition rejetés.
- L'audit `native_session_coordinator_boundary_host.py` scanne **178 modules C/C++** réellement présents ; contrôle un service main, les resets composés, les deux rôles leaf raw et 16/16 mutants (bypass, omission, doublon, récursion, retour de `--wrap`).
- Les oracles SDL-F10/F18/F27 et la chaîne audio adaptent leurs attentes sans changer les comportements historiques. Les quatre vrais ELFs Xtensa affichent **34/34/34** déclarations/liens/calls directs en normal, inverted, trace et **37/37/37** bringup ; trois autres wraps de logging toolchain hors périmètre.
- HEAD code `4172b2db1a0767314711c8f5d76a85be35c82574` : [CI push #44189183108](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44189183108) **SUCCESS** et [CI PR #44189192223](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44189192223) **SUCCESS**.

## Test matériel — log exact

`platformio-device-monitor-261118-115143.log` : **1 933 lignes, 259 225 octets, SHA256 `ab91cf37b5df2ebb3f7221c6265d82e27969524ea53831214626fe58e2fd0f98`**. ESP32-D0WD-V3 rev3, 2 cœurs à 240MHz, flash 4 194 304B, **0 PSRAM**. Framebuffer logique 160×120 RGB565 (**38 400B**), présentation 320×240 x2. Profil LCD observé `inversion=off` ; on ne peut pas en déduire quel environnement a été flashé.

**Session 1 : START neuf / Entrance.** INTRO puis chargement complet ; flash map `REUSE HIT`. Caisses sprite127 et sprite56 transformées en armures, consommables, munitions, arme Fire Ext, dialogue scripts 8/26 avec reprise 19/11, ouverture et fermeture de porte, mouvements axial/strafe/rotations, HUB inventory/system et sortie sans sauvegarde. Turns `MONSTERTURN ORDERED-DISPATCH` avec `activeCount=0`, pas d'ennemi activé sur ce parcours. `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY`.

**Session 2 : LOAD V11 slot04 / Entrance.** Sauvegarde spatiale **5604B**, chaîne runtime restaurée, `MAPFLASH REUSE HIT`. Malwrath sprite322 activé derrière porte : deux ripostes `MONSTERRETAL COMMIT` non létales (HP joueur 34→25→17). Deux attaques directes joueur `MONSTERCOMBAT COMMIT` sur le même Malwrath : PV monstre **11→7→0** (mort et drop). Ordonnanceur à `activeCount=14`, attaques et animations séquencées ; portes et déplacements ensuite. HUB→SYSTEM→EXIT ; retour au menu sans checkpoint écrit. Pas de mort du joueur et donc **pas de retry death-menu** dans cette capture.

**Contrats communs :** deux `ENGINESESSION READY`, deux `TRANSITIONLOAD RELEASE progress=100`, deux `RAMBUDGET HEADROOM_OK`, deux `RESIDENTRESET released=18008 empty=1` et deux `SYSEXIT MENU-READY resident=empty`. `shapeData=0x0` et `mediaTexels=0x0` tout au long des témoins. Heap8 pendant jeu: `ALIVE` 59120–60264B en session 1 et 59772B en session 2, largest8 **45044B** ; ces échantillons ne sont pas une mesure d'un minimum absolu. Musique `M_STALKS.wav` 2 sessions, fins à **0 underrun, 0 recovery, 0 gap ISR >90µs**, maximum observé lecture SD 4231µs puis 4177µs. SFX stream **13 START, 13 END**, aucun `SFXGEN DROP` dans le log ; **21** traces `AUDIOINTENT OVERFLOW drop=oldest`, qui désignent l'éviction de messages d'intention, pas une observation perceptive de SFX manquants. Aucun Guru, abort, WDT, panic ou FAILED observé. Le retour MENU comporte `RESIDENTRESET empty=1` avant la relance musicale menu.

## Verdict et limites

**PASS CYD borné ARCH-RC9** pour START, LOAD, service actif, interruptions dialogues/portes/combat, reset EXIT et reprise de menu, cohérent avec la suppression de deux wrappers. Non validés : retry après mort (même si callsite et mutant hôte sont vérifiés), transition CHANGEMAP réelle, flash MISS, rendu golden, comparaison physique normal/inverted, rollback fautif forcé ou expérience audio subjective. Barils/issue [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371) explicitement exclus.

Tous les commits réalisés **après ce test matériel** pour RC9 sont exclusivement documentaires. Le SHA testé reste celui du code ; toute validation CI documentaire doit être faite sur le HEAD documentaire avant merge. Aucune affirmation de test sur un firmware recompilé après ce HEAD documentaire.
