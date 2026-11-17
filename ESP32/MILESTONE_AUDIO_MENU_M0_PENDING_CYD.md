# AUDIO-MENU-M1 — titre avant premier menu, sans boucle ni redémarrage (PENDING CYD)

Base officielle : `main=174a22fed5a5d254fdeb6a9f576be8fd8d955953` après PR #363. Même branche PR #365 : `agent/audio-menu-m0-dm2ttl`. Corrige le prototype M0 d'après retour utilisateur sur vrai `esp-cyd-inverted`.

## Symptômes M0 observés et source matérielle

Le log Serial fourni par l'utilisateur démontre que MENU_MAIN était présenté et prêt avant `[MENUMUSIC] ROUTE` et `PLAY track=MENU`. Le WAV existant a été reconnu : `frames=355134 durationMs=16105 blockAlign=1024 samplesPerBlock=2041`. À ~5.4 s de lecture, HELP confirmait `FINAL reason=MENU_ACTION_BEFORE_PAK`, puis le retour HELP relançait le morceau ; idem sur LOAD/retour sélecteur. Tous les segments de titre visibles ont `underruns=0 recoveries=0` ; absence de preuve de lecture intégrale puisqu'interrompue avant 16s. Le retour utilisateur demande explicitement le comportement opposé sur ces trois points.

## Contrat M1 corrigé

- `M_DM2TTL.wav` : un seul lancement pendant la création du menu d'accueil, **avant le premier present physique**. Préstart synchrone borné 1800ms sous SOUND ON/SD, guard de clock R2f conservé (22050Hz), un lecteur SD, un ring 16384 octets, Timer TG1T1 DAC2 inchangés.
- Rejoue **une fois seulement** sur chaque visite fraîche du titre (boot ou retour effectif depuis gameplay). Pas de boucle : à la fin des 355134 frames, pas de `seek(dataOffset)`, stop à ≤256 frames restantes (~11ms de marge de service avant underrun) : `[MENUMUSIC] END_ONCE`, `FINAL reason=MENU_END_ONCE`. Tentative enregistrée jusqu'à sortie du titre, donc pas de redémarrage à la fin de la musique.
- HELP, OPTIONS, sous-menu SOUND/VIDEO, écran LOAD, choix et BACK des slots : **même flux et position musicale**, aucune fermeture et aucune remise à zéro. Ne pas la relancer en revenant sur MENU_MAIN, même si le morceau s'est terminé.
- START réellement confirmé : fermer et libérer le lecteur **avant** le lancement de l'intro M_IN_CIT et ses accès PAK. LOAD : ne fermer **qu'après** confirmation d'un slot sauvegardé valide et **avant** libération du menu/lecture du checkpoint, jamais lors du simple clic sur LOAD ou sur BACK du sélecteur.
- SOUND OFF coupe toujours le son et libère le lecteur. Si l'utilisateur remet SOUND ON, il demande explicitement la reprise du morceau depuis le début ; ce n'est pas un redémarrage automatique lié au sous-menu.
- Fichier SD : `/doomrpg/music/opl/M_DM2TTL.wav`, fallback `/M_DM2TTL.wav`. Format IMA-ADPCM mono 22050 Hz, bloc ≤1024 bytes, confirmé par log utilisateur. Aucun WAV dans Git/PAK. PAK ouvert : le service principal saute toutes les lectures SD et aucune lecture n'est faite depuis ISR. Missing/invalid WAV/heap : fail closed sans late-start.
- Stabilité des autres parcours : intro avant ST_INTRO, fin intro avant map, Entrance M_STALKS seulement après READY avant premier gameplay. Aucune allocation persistante en plus du ring existant, aucun buffer/PSRAM/world/SDL.
- Historique SDL : restauration stricte des insertions M1 dans les oracles F17/F19/F29, puis même SHA Git historique immuable. CI du code et vrais ELF à valider.

## Test matériel requis

Flasher le firmware normal `esp32-cyd` ou inverted (mentionner explicitement). Sur boot : `[MENUMUSIC] PRESTART ready=1 beforeMenuPresent=1` doit apparaître AVANT `[MAINOPAQUE] READY`. Écouter **au moins 20 secondes sans toucher**, vérifier **une seule** occurrence `ROUTE / PLAY track=MENU` et `END_ONCE`, sans `loops>0`. Ensuite HELP/BACK, OPTIONS/BACK, LOAD/slot/BACK : pas de `FINAL` ni nouveau `ROUTE` en changeant de page avant fin de piste. Confirmer START : titre fermé avant intro, puis Entrance après loading READY. Si sauvegarde valide, vérifier LOAD confirmé et fermeture avant restore; SOUND OFF doit rester silencieux. Surveiller heap8/largest8, underflows/recoveries, CPU watchdog/panic. Faire un retour sur absence de clic/pause lors des accès SD des sous-menus.

**PENDING CI et PENDING validation CYD de M1.** Ne pas fusionner sur le seul test M0 ni sur CI ; fusion sur accord explicite.
