# PERF-GAME — feuille de route de la fluidité native

24 novembre 2026 — [Epic #453](https://github.com/Kharn27/DoomRPG-RE/issues/453).
Statut : proposition de conception et backlog, aucune optimisation ni validation matérielle nouvelle dans cette PR. Base documentaire main e84f2f9498fca0c2172af4c0ef03742e1d701b18 après #452.

## User story et philosophie

En tant que joueur sur classic CYD, je veux des tours ennemis rapides même après activation massive, sans perdre la fidélité des décisions IA, des collisions, des animations, de l'audio et des sauvegardes.
**Simuler tout ce qui doit l'être, présenter seulement ce qui est nécessaire.**
Le portage natif reste autoritaire. Le legacy est un oracle comportemental, pas une architecture à réintroduire.

## Point de départ

Le constat matériel #447 (log 261123-235648, SHA256 e74ff1c1c134ca9c9ea510939dc504d1d26cbe8cb58190686d9552157c2558f4) distingue 24 monstres historiquement activés et 7 membres livrés sur les tours 233–249. Le budget ACTIVESEQ_MEMBER_BUDGET=1 force des reprises de service ; le joueur ressent le délai même en se retirant du champ. Ce dernier constat est un témoignage, pas une mesure isolant le coût du rendu. Les anciens timings VIEWANIM ~95ms médiane / ~128ms p95 et MONSTERCOMBAT ~143ms médiane sont des témoins de frame, non des timings de tour. Les 3-goals, attaques, attentes, audio et service gaps restent à mesurer. Ne pas attribuer tout le coût au raycasting, au BSP ou au LCD.

## Architecture cible (progressive, pas big bang)

Simulation autoritaire ordonnée → intentions visuelles compactes → classification de visibilité/dépendances → présentation groupée si compatible.
Le moteur conserve un propriétaire mutable unique, l'ordre d'activation, les décisions individuelles et le RNG original. La « ruche » orchestre les intentions ; elle ne donne pas une IA omnisciente aux ennemis. Les règles de gameplay restent inchangées sauf décision explicite distincte.

Quatre notions différentes :
- Activé : participe à la simulation même hors champ.
- Visible : sa contribution actuelle ou future peut modifier l'image.
- LOS : possibilité de voir/attaquer selon les règles IA, indépendante du champ caméra.
- Audible : effet sonore à traiter selon les règles existantes, sans le supprimer parce que le sprite est invisible.

Un déplacement pur peut éviter un rendu seulement si son état avant/après et son trajet ne modifient aucune contribution visible ni aucun effet associé. Une silhouette sortante doit être effacée ; une entrante doit apparaître. Une occlusion incertaine, activation dépendante du rendu, porte, lumière, projectile, transparence, HUD, automap ou effet joueur impose une barrière/fallback conservateur. Ne pas confondre absence de pixels d'un sprite avec absence de besoin de frame. Les transactions gameplay historiquement liées au rendu doivent être séparées avec garde d'échec et preuve de rollback, sans supprimer une validation qui porte encore une conséquence logique.

### Exemple spatial

Groupes A1/A2/A3, B1/B2, C1/C2, indépendants géographiquement :
| Vague de présentation | Membres |
| --- | --- |
| 1 | A1, B1, C1 |
| 2 | A2, B2, C2 |
| 3 | A3 |

7 étapes visuelles deviennent potentiellement 3, soit 4 étapes évitées sur 7 (57 % théorique). Ce n'est pas un benchmark ni une promesse de gain. Des dépendances peuvent imposer davantage de vagues. La simulation demeure séquentielle dans son ordre original, même si les animations se chevauchent. Origines, cases cibles possibles, chemins, swaps et 3-goals sont à considérer ; les attaques, RNG, santé joueur, scripts, spawn/raise et effets partagés peuvent créer des dépendances non spatiales. Une bitset de 1024 cases = 128 B, utile comme aide de réservation, insuffisante seule. Pas de réordonnancement de décisions ou de commits concurrents sans preuve d'équivalence et accord si gameplay différent.

### Coûts et autres pistes

Modèle illustratif : 10 déplacements dont 2 visibles, 110 ms/frame, une frame par déplacement → 1100 ms de rendu contre 220 ms si 2 frames nécessaires, ou 110 ms si elles peuvent être regroupées. Hors IA, attente, animation, audio et conséquences partagées. Aucun chiffre de gain matériel revendiqué.
Une tranche de service bornée en temps ET actions doit respecter audio/touch/WDT et la reprise. Une liste vivante compacte peut filtrer les morts sans modifier le préfixe historique ni les règles d'activation/raise. La géométrie statique projetée à caméra fixe peut éventuellement être réutilisée avec invalidation stricte (pose, portes, textures, éclairage, map et ressources) sans deuxième framebuffer. Le deuxième cœur n'est qu'une exploration après preuve de goulot CPU : préparation pure sur snapshot borné, aucun partage mutable du monde/RNG ni concurrent SD. Une intention tactile différée unique est une option UX distincte : revalidation, annulation et feedback, aucun tour fantôme.

## Backlog et dépendances

| Jalon | Issue | Livrable et gate |
| --- | --- | --- |
| P1 | #447 | Baseline et oracle réel, métriques appariées, seuils de réussite fixés avant optimisation |
| P2 | #454 | Mouvements invisibles sans frame, classification conservatrice et état/RNG exact |
| P3 | #455 | Budget temporel + limite d'actions, liste vivante ordonnée, reprise sûre |
| P4 | #456 | Intentions bornées, présentation groupée, barrières et fallback |
| P5 | #457 | Ruche / graphe de dépendances / vagues, conditionnel au bénéfice mesuré |
| P6 | #458 | Réutilisation de géométrie statique, conditionnelle au profil et budget mémoire |
| P7 | #459 | Étude multicœur seulement si nécessaire, prototype réversible |
| P8 | #460 | Option UX intention tactile unique, après P1 et accord avant activation |

P1 → P2 → P3 → P4. Revue après P2 et P4 : poursuivre uniquement si le coût résiduel le justifie. P5/P6 peuvent être écartés si les gains suffisent ; P7 n'est pas un objectif obligatoire. P8 peut être étudié séparément après P1.

### Non-duplication / coordination

#442 et #443 : parité IA, oracles et corpus ; #438/#440 : subtype10 ; #439 : triple-goal et collision spéciale. Ne jamais accélérer en désactivant une famille.
#420 : audit police/HUD/Serial, conserver ticket et distinguer I/O et impression ; #340 : sampler et frontières historiques, vérifier état actuel RC24 avant reprise.
#382 : incidents audio et cadence de pump ; #427 : SFX-only indépendant du routage Sector 1 déjà validé ; #261 : chantier audio global. #446/#371 : explosions/rollback distincts ; ne pas fermer sur une preuve de latence.
#263 et PR draft #364 : laboratoire résolution en pause de développement, conserver les acquis et la branche, aucune fermeture ou migration automatique. Revue après P2/P4 sur 160×120 ; reprendre ou arrêter explicitement selon fluidité, netteté et SRAM.
Tir à travers flammes : évolution gameplay distincte, pas une optimisation ni une conséquence automatique de la transparence graphique.

## Protocole commun et critères de sortie

A/B sur même build de référence, checkpoint, parcours, configuration audio, résolution, cache et état RNG contrôlés ; distinguer variantes instrumentées et firmware normal. Scénarios : 0/1/7 vivants, 24 historiques, tous visibles, hors champ, derrière mur, passage champ↔hors champ, zones congestionnées/indépendantes, 3-goals, attaque multiple, portes/scripts, activation en cours de tour, mort/raise et LOAD/EXIT. Ne pas supposer que 24 historiques = 24 vivants.

Mesures agrégées bornées : p50/p95/max turnStart→complete, temps C par service, gaps, busy, input→commit/présentation, taps ignorés, causes/nombre de frames et presents, temps world/sprite/HUD, audio pumps/deadlines/underruns/recoveries/FIFO/drops, heap8/largest/DMA et stack. Limiter la perturbation Serial ; pas de log par pixel/frame systématique. Comparer état final et préfixes de commit (joueur, monstres, positions, topology, activation, événements, RNG et save), pixels aux mêmes phases ; différence intentionnelle d'animation documentée. Définir les objectifs chiffrés après P1, aucun gain inventé.

Invariants : classic ESP32-2432S028R sans PSRAM, 4 Mio flash, production 160×120 RGB565 (38400 B), PAK natif et cache borné, pas de ZIP runtime, shapeData=mediaTexels=NULL, pas de deuxième framebuffer ni grosse allocation par tick, audio et input prioritaires. Pas d'optimisation prématurée de PlatformVideo_present(). Un seul propriétaire mutable. Tests C hôte réels et adversariaux, CI multi-profils/ELF, test normal esp32-cyd sur matériel avec Serial et retour perceptif. Chaque PR part du vrai main SHA, reste bornée, fail-closed, post-PASS docs-only ; fusion seulement autorisée.

Fin de campagne lorsque les seuils issus de P1 sont satisfaits sur les cas représentatifs, sans régression ni dette critique créée. Les explorations non rentables peuvent être clôturées avec justification. Le moteur reste un jeu tour par tour : pas de cible FPS arbitraire.
