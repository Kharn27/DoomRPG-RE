# ARCH-RC4 — retrait de l'interposition readRange PAK, observation TRACE native explicite : PASS CYD

**17 novembre 2026, PR [#363](https://github.com/Kharn27/DoomRPG-RE/pull/363)**. Base exacte du vrai `main` après fusion RC3 et #360 : `1e9c1268c30c1051836be6fb066e6d927064415e`. Branche `agent/esp32-arch-rc4-read-range-observer`. Issue [#340](https://github.com/Kharn27/DoomRPG-RE/issues/340), jalon précédent [RC3](MILESTONE_PR361_ARCH_RC3_RESIDENT_END_HOOK_CYD_PASS.md).

## Frontière et comportement

La sélection est issue de l'audit des propriétaires survivants : `EspAssetPack_residentBegin` contient la préparation de la flash et les chemins d'échec/présentation du chargement ; `Esp32PlatformVideo_present` compose plusieurs états modaux/rendu ; les risques sont plus élevés. RC4 retire **seulement** `-Wl,--wrap=EspAssetPack_readRange` et `__wrap_EspAssetPack_readRange` / `__real_EspAssetPack_readRange`. Le corps de lecture d'origine est déplacé, hormis son nom, **à l'identique** dans `static readRangeCore()` ; `EspAssetPack_readRange()` appelle explicitement `EspAssetPackIoProbe_beforeReadRange()` puis le core puis `EspAssetPackIoProbe_afterReadRange(start)`. Ces deux petits hooks C sont déclarés dans `esp_asset_pack_io_probe.h` et implémentés par l'observateur déjà propriétaire des compteurs. En TRACE : baseline physique avant lecture, chronométrage d'une requête logique complète (y compris erreurs, refus d'ouverture, zéro octet), compteur/somme/max et flush au seuil de 64. INFO : aucune mesure ni invocation chronométrique dans le chemin public, résultat PAK inchangé. Les propriétaires SD/source-probe/flash/cache/validation sont inchangés. Pas de nouveau buffer, allocation, lecture ZIP runtime, renderer, gameplay, LCD, audio ou modification de l'API de présentation.

## Vérification hôte, CI et quatre ELFs réels

**Commit définitif de code et de tests matériellement flashé : `912678056082d72bbd3291230b1f6427ad4a0221`** (les commits précédents de la branche ont corrigé des fixtures adversariales ; seul ce HEAD est retenu). L'oracle `ESP32/test/native_read_range_boundary_host.py` extrait le **vrai C++** des deux hooks, de l'API publique et du core, teste INFO et TRACE (17 chemins de lecture, vidage de seuil 64), et rejette **11 mutations adversariales**. Il ne simule pas un ESP32 réel. Le test d'inventaire empêche la réintroduction des interpositions retirées, et compare le moteur des deux builds release.

Double CI sur le **même HEAD de code** : [push 43984410493](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43984410493) **SUCCESS** et [PR 43984421724](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43984421724) **SUCCESS**. Les journaux CI confirment chacun les véritables ELFs **déclarés / liés / témoins directs** :
- `esp32-cyd` : **42/42/42**
- `esp-cyd-inverted` : **42/42/42**
- `esp32-cyd-bringup` : **45/45/45**
- `esp32-cyd-trace` : **42/42/42**

Trois wrappers de logging toolchain dans chaque ELF, indépendants des interpositions Doom RPG. Témoin direct lié ne signifie pas preuve d'exécution matérielle.

## Test réel utilisateur — PASS RC4 (scope explicitement limité)

Le **vrai classic CYD ESP32-D0WD-V3 240 MHz / flash 4 MB / PSRAM 0 B** a démarré sur **`esp-cyd-inverted`** (ligne `[VIDEO] ... alternate-panel inversion=off`, qui désigne la polarité du profil alternatif). Capture `platformio-device-monitor-261117-014629.log`, **2043 lignes**, SHA-256 **`caf4e52784f66d02744422531d851156c5d044aeb5c7c8dcd6b79159ff0b687e`**. Le Serial n'imprime pas le SHA firmware cryptographique ; l'attribution au HEAD testé repose sur la branche/CI fournie et le scénario utilisateur.

- Démarrage `MENU_MAIN` -> nouvelle partie -> intro puis `Entrance /intro.bsp` ; PAK natif indexé à **241 entrées**, runtime compact `arenaBytes=14095`, carte `sourceCRC=623f34e4`. Transition avec `[MAPFLASH] REUSE HIT` et `ARM resident=1` ; `[TRANSITIONLOAD] RELEASE ... progress=100` ; `[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0`. Gestes réellement effectués : déplacements, rotations, un dialogue 8 et 26 (reprise opcode 19), destruction/transformation de caisses, acquisition d'armure/medkit/munitions/Fire Ext, animations de porte, HUB/SYSTEM puis EXIT sans sauvegarde.
- **1ère fermeture :** `[RESIDENTRESET] heap8=88228->106236 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1` ; `[SYSEXIT] MENU-READY ... resident=empty saveWrite=no checkpoint=unchanged`.
- LOAD confirmé du slot `/DoomRPG-ESP32-slot04.sav`, **V11**, **5604 B**, restauré sur Entrance ; deuxième `[MAPFLASH] REUSE HIT`, nouveau `ARM resident=1`, transition `progress=100`, `[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0`. **2ème fermeture strictement identique** `[RESIDENTRESET] heap8=88228->106236 released=18008 ... empty=1`, puis second `[SYSEXIT] MENU-READY ... resident=empty`.
- Une seule carte physique et deux cycles sur Entrance : les logs ne démontrent ni changement de niveau, ni réécriture de slot map-flash, ni refus de lecture PAK en réel, ni instrumentation TRACE exécutée sur matériel. La nouvelle observation `readRange` étant TRACE-only, son exactitude/les échecs restent prouvés **par les tests hôte**, pas par ce log INFO.
- Audio **non modifié par RC4**, mais observation non régressive : intro `FINAL frames=255315 underruns=0 recoveries=0 gapsOver90us=0` ; Entrance sessions `FINAL frames=957170` puis `847596`, **0 underrun / 0 recovery / 0 gap>90µs** sur les deux, DAC/timer/fichier **OFF/OFF/CLOSED** à chaque fin. `heap8After=127076` et `largest8=86004` strictement identiques après les deux exits. Heap8 minimum parmi les lignes `[ALIVE]` en gameplay **59600 B** première session, **60260 B** deuxième (échantillonnage, non preuve de minimum absolu). Deux `[RAMBUDGET] RESERVE ... HEADROOM_OK`.
- Aucune occurrence de Guru Meditation, panic, assertion fatale, WDT ni shapeData/mediaTexels non NULL dans la capture. Le sampler R-B **n'est pas autorisé** sans golden desktop original full-frame.

## Clôture et merge

**PASS matériel RC4 dans le scope ci-dessus.** Après le HEAD testé, seuls ce milestone et des paragraphes de `PORTING_STATUS.md` / `DOCUMENTATION.md` doivent changer. Vérifier le diff post-test docs-only et la **double CI SUCCESS sur le HEAD documentaire** avant fusion autorisée (l'utilisateur a explicitement demandé « si c'est ok tu fusionnes et on continue »). Après fusion, relire le `main` GitHub et son SHA exact avant toute branche RC5.
