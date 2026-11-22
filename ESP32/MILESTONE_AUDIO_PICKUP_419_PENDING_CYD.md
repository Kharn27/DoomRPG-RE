# AUDIO-PICKUP #419 — handoff et SFX d'arme original 5057 avant dialogue (PENDING CYD)

**Issue :** [#419](https://github.com/Kharn27/DoomRPG-RE/issues/419).
**PR :** [#422](https://github.com/Kharn27/DoomRPG-RE/pull/422).
**Branche :** `agent/audio-pickup-419-modal-handoff`.
**Base exacte de main :** `d6c47f5a2906355b82329ecd21dff5ff44d09abc` (ARCH-RC24 et FIRE-VIS-P2 inclus).
**Statut : correction de fidélité 5057 à tester sur vrai CYD ; pas encore de PASS de cette version.**
Le premier candidat 5060 (SHA `733d2c17fb4304fd9bc97c635b6ab97a35005861`) a passé la double CI et une lecture de log CYD, mais est **fonctionnellement incorrect** pour les armes à feu.

## Cause réelle documentée (CYD avant #419)

Ramassage de première arme : `[PLAYERRES] WEAPON-HELP ... status=OPEN` ouvrait un
`EspNativeGameplayDialog_beginStandalone` qui maintenait le bail PAK.
L'intention `5060` était publiée **après** ce début de dialogue ; le mixeur
faisait `[SFXP2] DEFER ... pack-lease-open` jusqu'à `[DIALOG] CLOSE`.
La FIFO fonctionnait : aucun SFX perdu comptablement, mais un décalage audible
du moment du pickup.

### Validation matérielle intermédiaire et découverte (22 novembre 2026)

Log exact `platformio-device-monitor-261122-082805.log` fourni par
l'utilisateur sur la PR #422 : LOAD V11 Entrance, première Axe **tile 655**,
premier Shotgun **tile 659**. Les deux sont `[AUDIOPICKUP] HANDOFF
status=MIXED mixedBeforeModal=1 ack=1` avant `[DIALOG] OPEN-STANDALONE` ;
`SFXP1 MIX id=5060 outputFrames=9702 mixed=9702` dans les deux cas,
`underruns=0`, `AUDIOINTENT AUDIT queueOverflow=0
consumerCursorGap=0 audibleSfxDropped=0`. La latence est donc corrigée
dans ce parcours, et le joueur confirme l'entendre avant le dialogue.

**Mais la fidélité de l'identité sonore est incorrecte.** La fonction
historique `src/Entity.c::Entity_touched` (`eType=5`) fixe `sound=5060`
uniquement pour Axe (subtype 0) et Fire Extinguisher (1) ; pour les armes
à feu Shotgun (3), Chaingun (4), Super Shotgun (5), Plasma (6),
Rocket Launcher (7), BFG (8), elle fixe **`sound=5057`**, y compris
les prises de doublon. Le premier candidat #422 avait publié 5060 pour
toutes les ressources ; les 5060 du Shotgun sont une **preuve de régression
de fidélité** malgré le PASS technique de handoff.

Le correctif suivant ajoute le choix original `EspAudioPickupCue_weapon`
et ouvre le chemin PCM borné 5057 **avant** la modale, sans revenir
au SFX générique ou à la lecture PAK pendant le bail. Une nouvelle
validation complète CI + CYD est impérative.

## Changement borné

1. La transaction de pickup et son rendu de confirmation réussissent **avant**
   toute publication audio de ce nouvel embranchement. Le rollback reste
   strictement silencieux.
2. Le pickup publie **une seule intention**, choisie par le type d'objet et
   le sous-type d'arme, après succès de la transaction. `5060` reste
   le cue des objets ordinaires, munitions, Axe et Fire Ext ; `5057` est
   la sonorité originale des armes à feu, **même en doublon**. Uniquement
   si un dialogue de **première acquisition** doit s'ouvrir, le propriétaire
   reçoit `EspEntranceMusic_handoffPickupBeforeModal(sequence)`.
3. Le propriétaire audio valide la **prochaine** intention FIFO exacte
   (pas de saut ni de réordonnancement), la disponibilité PAK/flash et de la
   piste, puis l'en-tête `5057.wav` **ou** `5060.wav` (PCM16 mono 8 kHz).
   La ressource originale 5060 contient 3 520 frames source, soit **9 702**
   frames DAC à 22 050 Hz, **10 726** avec garde. La ressource originale
   5057 contient 4 320 frames source, soit **11 907** frames DAC,
   **12 931** avec garde 1 024. Les deux tiennent dans le ring
   **16 384 octets**. Les seuils réels sont **recalculés depuis le PAK**
   et non figés dans la logique.
4. Si nécessaire, au maximum **quatre** blocs de musique ADPCM sont publiés
   via le lecteur SD et ring **existants**, dans la même tâche Arduino. Puis
   `mixPendingSfxOne` mélange une seule intention, l'ACKant par son propriétaire
   habituel. Le dialogue ouvre le bail PAK **seulement ensuite**.
5. En cas de bail PAK déjà ouvert, musique absente, FIFO plus ancienne, voix
   occupée, WAV invalide, ring insuffisant ou échec SD, le handoff ne force
   aucun nouvel accès concurrent ni aucune lecture illégitime : la FIFO est
   laissée au propriétaire ordinaire après la fermeture de la modale (sauf
   fermeture complète de piste, qui applique son contrat existant de discard).
   Une lecture partielle reste un échec comptabilisé, jamais rejouée deux fois.

**Aucun** second framebuffer, cache PCM, nouveau `File`, allocation heap,
nouveau thread/timer/ISR, SDL ou runtime ZIP. `shapeData` et
`mediaTexels` doivent demeurer NULL. La cadence ambiante FIRE-VIS-P2
375 ms est hors périmètre.

## Oracle hôte et CI

`ESP32/test/audio_pickup_modal_handoff_host.py` extrait et compile **le
véritable corps C++** du handoff, plus le propriétaire FIFO
`ESP32/src/esp_native_audio_intent.c`, les vrais parsers WAV/PCM et la garde
native. Les périphériques PAK/SD/ring et le corps du mixeur préexistant sont
remplacés par de petits témoins déterministes (le mixeur normal reste couvert
par ses tests historiques). Le test vérifie la distinction exacte du
`src/Entity.c` original, la fonction C pure `EspAudioPickupCue_weapon`
(0/1 → 5060, 2..11 → 5057), les deux WAV avant modale, FIFO ancienne,
bail occupé, sons désactivés, musique absente, voix occupées, WAV corrompu,
refills bornés, les budgets **10726 / 12931**, ACK unique et non-rejeu.
La restauration inverse stricte des changements #419 est appliquée **uniquement
pour l'oracle Git historique SDL-F27**, sans réécrire ses hashes d'origine.

### Protocole vrai classic CYD / env normal `esp32-cyd`

Après **double CI SUCCESS** sur le HEAD de la PR, flasher ce HEAD normal
sur le CYD 4 MiB sans PSRAM, même microSD et PAK que les essais précédents.

- START → NEW GAME **ou** LOAD V11 Entrance avec arme encore à prendre ;
  ramasser le **Shotgun première fois** (tile observée **659**).
  À l'oreille : le cue mécanique d'arme original, immédiatement et
  **avant la fermeture du dialogue**. Logs attendus :
  `[PLAYERRES] COMMIT ...action=weapon` →
  `[AUDIOINTENT] PUBLISH ...resource=5057` →
  `[SFXP1] MIX id=5057 ...sampleFrames=4320 outputFrames=11907 mixed=11907` →
  `[AUDIOPICKUP] HANDOFF ...id=5057 status=MIXED mixedBeforeModal=1 ack=1` →
  `[DIALOG] OPEN-STANDALONE`. **Aucun 5060** pour ce pickup Shotgun.
- Vérifier le premier ramassage de la **Hache** (observée tile **655**)
  et du **Fire Ext** (tile **643** si parcours identique) : cue **5060**
  toujours pré-modal. Tester un pickup de munitions : cue **5060**.
  Si possible essayer une acquisition d'arme à feu déjà possédée :
  **5057** sans nouveau dialogue. Vérifier zéro doublon sonore,
  dialogues lisibles, mutation de pickup unique.
- Tester l'absence/le refus d'un WAV ou de musique en regardant le statut
  `DEFER` ; aucune lecture PAK concurrente ni crash. Une cible volontairement
  indisponible est hors du PASS acoustique principal.
- Faire un gameplay/combat/portes, EXIT → MENU → LOAD V11 si accessible.
  Consigner `AUDIOINTENT AUDIT queueOverflow/consumerCursorGap/
  audibleSfxDropped`, `ENTRANCEMUSIC FINAL underruns/recoveries`,
  `RAMBUDGET HEADROOM_OK`, `RESIDENTRESET empty=1`,
  `shapeData=0x0 mediaTexels=0x0`, absence WDT/panic.
- Fournir le **log Serial complet et le ressenti audio**. Le SHA firmware
  n'étant pas historiquement sérialisé, l'attribution reste fondée sur la
  branche/HEAD annoncés et non sur une preuve cryptographique du binaire.

**Gates de fusion :** double CI finale + PASS du vrai CYD + code gelé
post-test + addendum documentaire (docs-only) + accord explicite de merge.
