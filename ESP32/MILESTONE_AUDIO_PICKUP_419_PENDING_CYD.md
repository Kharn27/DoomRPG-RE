# AUDIO-PICKUP #419 — handoff du 5060 avant dialogue standalone (PENDING CYD)

**Issue :** [#419](https://github.com/Kharn27/DoomRPG-RE/issues/419).
**PR :** [#422](https://github.com/Kharn27/DoomRPG-RE/pull/422).
**Branche :** `agent/audio-pickup-419-modal-handoff`.
**Base exacte de main :** `d6c47f5a2906355b82329ecd21dff5ff44d09abc` (ARCH-RC24 et FIRE-VIS-P2 inclus).
**Statut : CI et test physique à confirmer. Aucun PASS acoustique / Serial de ce correctif à ce stade.**

## Cause réelle documentée (CYD avant #419)

Ramassage de première arme : `[PLAYERRES] WEAPON-HELP ... status=OPEN` ouvrait un
`EspNativeGameplayDialog_beginStandalone` qui maintenait le bail PAK.
L'intention `5060` était publiée **après** ce début de dialogue ; le mixeur
faisait `[SFXP2] DEFER ... pack-lease-open` jusqu'à `[DIALOG] CLOSE`.
La FIFO fonctionnait : aucun SFX perdu comptablement, mais un décalage audible
du moment du pickup.

## Changement borné

1. La transaction de pickup et son rendu de confirmation réussissent **avant**
   toute publication audio de ce nouvel embranchement. Le rollback reste
   strictement silencieux.
2. Le pickup publie une **seule intention 5060**, puis, seulement si le
   dialogue de **première arme** doit s'ouvrir, propose au propriétaire audio
   `EspEntranceMusic_handoffPickupBeforeModal(sequence)`.
3. Le propriétaire audio unique valide la **prochaine** intention FIFO exacte
   (pas de saut ni de réordonnancement), la disponibilité du PAK/flash et de la
   piste, la forme réelle de `5060.wav` (PCM16 mono 8 kHz) et les voix
   disponibles. Les 3 520 frames source du PAK original représentent **9 702**
   frames à 22 050 Hz ; avec les **1 024** frames de garde, le ring de
   **16 384 octets** doit contenir **10 726** frames futures disponibles.
   Cette limite est **recalculée depuis l'en-tête PAK** et non aveuglément
   codée sur la durée historique.
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
par ses tests historiques). Tests incluant 5060 avant modal, FIFO ancienne,
bail occupé, sons désactivés, musique absente, voix occupées, WAV corrompu,
refills bornés, exact budget 10726, ACK unique et non-rejeu.
La restauration inverse stricte des changements #419 est appliquée **uniquement
pour l'oracle Git historique SDL-F27**, sans réécrire ses hashes d'origine.

### Protocole vrai classic CYD / env normal `esp32-cyd`

Après **double CI SUCCESS** sur le HEAD de la PR, flasher ce HEAD normal
sur le CYD 4 MiB sans PSRAM, même microSD et PAK que les essais précédents.

- START → NEW GAME → Entrance ; ramasser **Shotgun première fois** (tile
  observée 659). À l'oreille : cue de pickup au toucher, avant fermeture du
  dialogue. Chercher `[AUDIOINTENT] PUBLISH ...resource=5060`,
  `[SFXP1] MIX ...id=5060`,
  `[AUDIOPICKUP] HANDOFF ...status=MIXED mixedBeforeModal=1 ack=1`,
  **puis** `[DIALOG] OPEN-STANDALONE`. Les messages de journalisation
  `PLAYERRES COMMIT` doivent précéder le handoff.
- Tester une **première Fire Ext** (tile 643 si parcours identique), puis
  toute autre première arme proposée et une reprise d'arme déjà possédée.
  Vérifier aucun SFX en double, dialogue lisible, pickup unique.
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
