# GAME-AI-14 A0 — Inventaire global des 14 subtypes (non matériel)

**Issue maîtresse :** [#442](https://github.com/Kharn27/DoomRPG-RE/issues/442). **Base :** `main=d9896fcf5f07ab8d41935565002792b608a41333`.

## Contrat de ce lot

Le test hôte `ESP32/test/game_ai14_matrix_host.py` lit **le vrai code** du legacy
(`CombatEntity.c::monsterAttacks` et `Combat.c::monsterWpInfo`) ainsi
que les tableaux des unités natives effectivement compilées.
Il compare tous les 28 IDs d'armes (14×2), 14 multiplicités de tirs,
14 cadences d'animation ; quatre copies d'armes, trois copies de shots et
un tableau de phases. Pas de simulation de monstre inventée.

Le test signale explicitement les limitations du `main` :
- subtype 10 ignoré par les quatre propriétaires (branche draft #440 à part) ;
- subtype 4/13 : continuation à 3 objectifs, obstacle spécial / issue #439 ;
- subtype 7/8/12/13 : mort létale fail-closed dans le propriétaire Combat ;
- toute famille : attaque après `NO-MOVE` sans chemin encore différée ;
- toute famille : ciblage destructible eType12 non possédé par l'attaque directe joueur ;
- seulement certaines familles de projectiles avec présentation dédiée ;
- RNG ambiance séparé, différent de l'ABI RNG du legacy.

Ces limitations sont **des constats de source, pas des PASS**. Les assertions
exigent la présence des gardes/constats actuels : toute correction future doit
adapter ce lot avec un oracle comportemental réel, plutôt que supprimer un gate
en silence.

## Prochain lot obligatoire

14×(player-cardinal, diagonale, LOS obstruée, cible destructible,
in-range, no-move, close-bias) en exécution C natif avec RNG contrôlé,
entrée/sortie et rollback exact, plus les families spéciales et sauvegardes.
Une tranche à la fois, mais **l'ensemble en tests hôte avant de demander au
joueur un long playtest Sector 1**.

`python3 ESP32/test/game_ai14_matrix_host.py`

Ce milestone ne modifie ni exécutable ni RAM runtime, ni PAK/ZIP, ni V11,
ni `shapeData`/`mediaTexels`. **Aucun test CYD revendiqué.**
