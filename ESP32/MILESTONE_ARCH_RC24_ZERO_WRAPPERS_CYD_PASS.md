> **Intégration post-test, 22 novembre 2026 :** la PR [#417 FIRE-VIS-P1](https://github.com/Kharn27/DoomRPG-RE/pull/417) a été fusionnée dans `main` après la capture RC24. La branche RC24 a incorporé le nouveau `main=3956064a3f553a4ef6d34f24830f7bc4247a166a` au commit **`92a513724c7275f074509dc8e2ef5690030b00f3`**. Les **preuves matérielles de ce rapport restent attribuées au candidat pré-FIRE `9c529dc9...`**, avec les deux correctifs audio inclus, et **ne valident pas la combinaison RC24 + feu additif**. Aucun code de façade RC24 modifié par cette intégration ; elle ajoute le rendu sprite FIRE et les nouveaux tests CI. **Refaire un smoke-test CYD sur ce HEAD combiné avant de fusionner #414**. Une nouvelle CI du HEAD combiné doit également passer. La PR reste draft.

# ARCH-RC24 — FINISH HIM : zéro GNU wrapper DoomRPG production, PASS CYD borné

**Date du test matériel :** 22 novembre 2026. **[PR #414](https://github.com/Kharn27/DoomRPG-RE/pull/414)**. **SHA code testé par contexte utilisateur :** `9c529dc9bdd95b07b298e7ee13e8ae26812ec9f6`. **Base `main` intégrée après merge [#415 AUDIO-LOAD-V11](https://github.com/Kharn27/DoomRPG-RE/pull/415) :** `826780ff72f43819945d5e1e8028935163a5cc1e`. Le Serial ne contient pas de SHA du firmware : attribution par le parcours de flash et la branche fournis dans cette conversation, pas par une empreinte binaire indépendante.

## Frontière de changement

La dernière interposition GNU production `-Wl,--wrap=Esp32PlatformVideo_present` est supprimée. `Esp32PlatformVideo_present` est désormais une **façade native publique C** (dans `esp_native_gameplay_present_gate.c`) qui protège dans cet ordre : suppression ponctuelle → loading actif → attente stats de CHANGEMAP → modal LEVEL UP → action/decorators → feuille physique. Le pont C++ `Esp32PlatformVideo_presentBase()` appelle directement `PlatformVideo_present()`, sans nouveau framebuffer, mémoire carte ni API desktop. Les propriétaires des écrans loading/LEVEL UP, action, HUB et effets conservent leurs voies Base explicites. **0 GNU wrappers DoomRPG en production/inverted/trace ; 3 interpositions uniquement dans bringup diagnostic** (distinctes des wrappers de logging du toolchain). Invariants natifs conservés, `shapeData/mediaTexels=NULL`, backing PAK et SD bornés.

L'intégration `main` contenant la réparation de musique LOAD V11 #415 est un commit de merge sur la branche RC24 ; 46 commits RC24 antérieurs conservés et seul fichier de résolution commun `.github/workflows/esp32-cyd.yml`. Les tests AUDIO-LOAD-V11 et ARCH-RC24 coexistent dans le workflow combiné.

## Double CI complète sur code candidat

- [Push **SUCCESS** #44983817913](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44983817913).
- [PR **SUCCESS** #44983821696](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44983821696).

Les deux sont `completed/success` au SHA `9c529dc9...` ; oracle `native_present_gate_boundary_host.py` utilisant les vrais corps C, **14 mutants** de suppression/owners/fail-closed ; oracle audio #415 utilisant le vrai C et 13 mutants ; compilation de quatre profils CYD et inventaire ELF des interpositions/retirements **SUCCESS** dans les deux workflows. Ne jamais confondre CI avec test physique.

## Vérité matérielle — firmware normal `esp32-cyd`

Log utilisateur : `platformio-device-monitor-261122-052553.log`, **4 150 lignes / 478 175 octets, SHA256 `71110bfef9c23bf3e5552ee07733028b22ad1237990f383b8288adfade4714bb`**. Chip ESP32-D0WD-V3 dual-core 240 MHz, 4 MiB flash, **0 PSRAM**. PAK natif 241 entrées, framebuffer logique 160×120 RGB565, 38 400 B, 2× vers ILI9341.

La capture montre **un START réel**, introduction puis Entrance, progression soutenue, dialogues, portes, pickups et combats, HUB/System → EXIT → menu → **LOAD réel slot04 V11 (5604 B, `restored-exact`)** → Entrance restauré, mouvements, combats/SFX, second EXIT → menu. Mesures vérifiées par comptage des événements log :
- **99 `VIEWANIM END` : 83 mouvements + 16 rotations**, **10 `DOORANIM COMPLETE`**, dont **5 HOLD/RELEASE différés**.
- **24 `PLAYERRES COMMIT`**, **24 miniatures START et END**, **3 dialogues d'aide de première arme**, **16 DIALOG OPEN et 16 CLOSE**, **5 `MONSTERCOMBAT COMMIT`**, **5 `MONSTERRETAL COMMIT`**, **51 `MONSTERACTIVESEQ COMPLETE`**.
- **Deux `RAMBUDGET RESERVE ... HEADROOM_OK`** : marge `heap8=12820 B` au START et `12000 B` au LOAD. `shapeData=0x0 mediaTexels=0x0` plusieurs fois.
- **Deux `RESIDENTRESET released=18008 empty=1`** et **deux `SYSEXIT MENU-READY`**, hash menu stable `7a70d304`. Aucune erreur `Guru Meditation`, `panic`, WDT, `FAIL-HEAP`, `HEAP-POST-REJECT`.
- **Régression sonore #415 vérifiée dans ce build combiné** : `M_STALKS.wav` `PRESTART ready=1 ... failClosed=0` au START puis au LOAD ; post-ring LOAD **`heap8=48568 largest8=31732`**, ring 16 384 B ; **les 5 arrêts de pistes `FINAL` ont `underruns=0 recoveries=0`**, aucun `audibleSfxDropped`, `queueOverflow` ou `sfxRejectedTrack`, et 2 EXIT propres. On relève sur les deux `M_STALKS` `gapsOver90us=1` chacun mais `gapsOver500Ms=0` et aucune rupture comptabilisée. Aucun résultat d'écoute humaine indépendant du retour utilisateur.

### Deux observations de finition, hors changement ARCH-RC24

**SFX première arme : [issue #419](https://github.com/Kharn27/DoomRPG-RE/issues/419).** Au ramassage Shotgun (tile659) : `WEAPON-HELP firstAcquire=yes` → `PUBLISH resource=5060` → `SFXP2 DEFER reason=pack-lease-open` ; son `SFXP1 MIX id=5060` **seulement après `DIALOG CLOSE ... packClosed=yes`**. Ce n'est pas un SFX perdu mais un SFX décalé par l'exclusivité de la ressource SD/PAK. Même schéma lors d'autres premiers pickups d'armes. Corriger en milestone audio séparé et sans relecture concurrente du SD.

**Réactivité/police : [issue #420](https://github.com/Kharn27/DoomRPG-RE/issues/420).** Retour utilisateur : légèrement moins réactif. Comparaison indicative des **48 `RA2B FRAME` instrumentées par log** : avant police SD active (RC23 `platformio-device-monitor-261121-122913.log`) médianes `totalUs=98,51ms`, `hudUs=1,27ms`, `presentUs=39,14ms` ; avec police déjà présente sur AUDIO #415 `platformio-device-monitor-261121-143238.log` : `102,20/2,42/39,49ms` ; RC24 : `101,71/2,40/39,51ms`. Appariement par poses x/y/angle : RC23→RC24 **+3,30ms** de médiane `totalUs` sur 20 poses ; #415 avec police→RC24 **+0,15ms** sur 19 poses. Indice d'un coût déjà présent avant RC24, **pas** A/B isolé et **pas** preuve que la police en est la seule cause ; rendus/cache/actions différents. **880 messages `FONTSD ACTIVE`** dans ce long log ne signifient pas 880 `SD.open` ; `esp_native_font_sd.cpp` garde un petit handle persistant hors suspension PAK et lit des ranges. Ne pas optimiser `PlatformVideo_present()` à l'aveugle.

### Périmètre non couvert

**Aucun CHANGEMAP / WAIT_STATS / LEVEL UP déclenché** dans cette capture ; les filtres de ces owners sont couverts par oracle C/mutants, pas par une preuve pixels matériels dans ce log. Pas de comparaison visuelle pixel-perfect originale, toutes cartes, LOAD répétés ou endurance multi-heures, ni test de faute/OOM forcée. Le dump ne contient pas l'identifiant SHA du binaire flashé. Les observations #419/#420 ne justifient pas de réinjecter un GNU wrapper ni de retoucher la façade RC24.

## Verdict et clôture

**ARCH-RC24 : PASS CYD borné de la suppression du dernier GNU wrapper DoomRPG production**, par scénario joueur vérifié ci-dessus, plus **double CI code combiné SUCCESS** et inventaire des quatre vrais ELF. Depuis ce test, **gel du code RC24** ; commits de clôture **documentation seulement**, diff post-hardware et double CI sur HEAD documentaire à confirmer. La PR #414 peut être marquée Ready for review après ces contrôles, mais **jamais fusionnée dans `main` sans accord explicite**. Les deux pistes #419/#420 suivront leur propre frontière.
