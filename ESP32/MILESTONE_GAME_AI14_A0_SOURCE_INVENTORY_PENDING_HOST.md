# GAME-AI-14 A0/A1 — Socle permanent de tests host (#442)

Base synchronisée : `69c70eb98bc12b830e5239df1edc2a7f73a42263` ; ancien candidat : `3aab838a6aff0806c26f22170f57c75533cdc7d2`. PR #443.

- A0 : 14/14 métadonnées legacy (28 armes, 14 multiplicités, 14 cadences), comparées aux tables natives. Les marqueurs historiques de lacunes sont des observations non bloquantes ; leur disparition ne prouve pas une correction.
- A1 : 312 cas (13 subtypes génériques × 2 variantes × 12 cas) exécutent les vrais corps C de sélection, compte prévisionnel RNG et masques. Tous les 14 masques sont testés. Ajouts : cardinal éloigné, RNG ranged éloigné, diagonale. Subtype 10 explicitement hors oracle générique (#438) ; son comportement manquant n'est plus une assertion obligatoire.
- Les stubs de LOS/topologie ne prouvent ni collision réelle, ni RNG effectif/refill, ni tour complet. Les types/constantes du harnais sont encore reproduits localement. Les tests de source ne sont pas des preuves comportementales.
- A2–A6 restent ouverts dans #442 : morts spéciales, raise, triple-goal, projectiles, sauvegardes et synthèse CYD. Ne pas conclure 14/14 gameplay PASS.
- Aucun changement de production, PAK, RAM runtime ou sauvegarde ; pas de test matériel spécifique demandé. CI sur la nouvelle base requise avant fusion.

Exécution : `python3 ESP32/test/game_ai14_matrix_host.py` et `python3 ESP32/test/game_ai14_candidate_rng_host.py`.
