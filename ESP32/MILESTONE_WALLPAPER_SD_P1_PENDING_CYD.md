# WALL-SD-P1 — correction après premier test CYD : PENDING RETEST

**Branche :** `agent/esp32-wallpaper-sd-overrides`, [PR #434](https://github.com/Kharn27/DoomRPG-RE/pull/434) (DRAFT).
**Base main vérifiée :** `a40d96eceb94844551a3a3fcd8036d0497d79d93`.
**Source matérielle :** `platformio-device-monitor-261122-175956.log`,
544 lignes, SHA256 `b9d1ba5ca3fa98b0a8d96dc702b7fb993d700e7bcc9ee3c2d54cb3d36f912cac`.
Le candidat testé était la première version `c8/e8`, HEAD
`56979e6d9b061ecb834012bc521f811e26a08ff8` **par contexte**
(le Serial ne transporte pas de SHA firmware).

## Premier essai : échec matériel, cause vérifiée

- `[WALLSD] ACTIVE` pour les deux fichiers BMP8.
- Menu `M_DM2TTL.wav` : `PLAY` et lecture DAC confirmée.
- Intro `M_IN_CIT.wav` : `WAV_OK` suivi de `HEAP-POST-REJECT
  before=66540 after=45764 minAfter=47104` puis `FAIL-HEAP`.
- Entrée map1 `M_STALKS.wav` : `WAV_OK`, puis
  `HEAP-POST-REJECT before=65596 after=44820 minAfter=47104`,
  `FAIL-HEAP`. Ce ne sont pas des WAV absents ou mal encodés.
- Image `e.bmp` : fond rose non uniforme d'après le testeur ;
  audit exact de la palette BMP : 7 527 pixels `#FF00FF`, mais
  3 077 pixels `(255,4,255)` et d'autres nuances proches
  non reconnues par la clé exacte du renderer.

## Correctif candidat (non encore matériellement validé)

1. `c.bmp` passe de BMP8 25 654 B à **BMP4 12 406 B** avec
   palette quantifiée RGB565 16 entrées. Coût RAM de c désormais
   identique au c BMP4 du PAK (hors structures), soit **13 248 B
   récupérés** par rapport au précédent test.
2. `e.bmp` conserve le BMP8, mais **12 022 pixels** sont maintenant
   uniformément indexés à 0 / `#FF00FF`, y compris les 4 495 pixels
   roses parasites détectés par classification couleur.
   Il conserve 4 362 pixels opaques ; le masque normal du moteur
   n'est pas modifié.
3. Le cache de ranges PAK *en gameplay* passe de 19 à 15 KiB et
   de 288 à 256 entrées, soit **4 480 B de capacité/metadata**
   retranchés de l'owner. Lectures flash bornées en cas de miss,
   sans changement de format ni du framebuffer.
4. Les gardes audio restent exactement à **61 440 B avant ring** et
   **47 104 B / largest16 384 B après ring**. Aucun faux
   `PLAY` ni baisse du seuil de sécurité.
5. Projection illustrative sur les mesures de ce premier log,
   en supposant le reste invariant : intro `45764+13248=59012`
   B post-ring, gameplay `44820+4480=49300` B.
   **Ce n'est pas une mesure du correctif ni une garantie de PASS**.

Les override SD restent individuels, les PAK BMP4 inchangés comme
replis, `shapeData=NULL`, `mediaTexels=NULL`; pas de PSRAM, ZIP
runtime, buffer image pleine taille ou second reader.

## Retest impératif du firmware normal esp32-cyd

1. Recopier les **deux fichiers corrigés** `c.bmp` et `e.bmp`
   sous `/doomrpg/img/` de la microSD et recompiler/reflasher le
   HEAD corrigé (pour obtenir le nouveau cache L1).
2. Contrôler `[WALLSD] ACTIVE c.bmp indexed=4`,
   `e.bmp indexed=8`, les planètes, la disparition des confettis roses.
3. Contrôler `[ENTRANCEMUSIC] PRIMING` puis `PLAY` pour
   `M_IN_CIT.wav` et `M_STALKS.wav`, puis
   `[INTROMUSIC] PRESTART ready=1`,
   `[LOADAUDIO-L1] PRESTART ... ready=1`. Aucun
   `HEAP-POST-REJECT`, aucune coupure, pas de WDT/panic.
4. Relever `heap8`, `largest8`, `[RAMBUDGET]`, cache hit/miss
   et fluidité tactile en gameplay. Tester ensuite LOAD si possible.
5. Conserver la PR DRAFT jusqu'au nouveau log matériel :
   un CI vert ne vaut pas un PASS CYD.

**Statut : première version FAIL matériel ; correctif à retester.**
