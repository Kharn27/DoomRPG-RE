# PR #257 — baseline HUD canonique et fermetures HUB exactes : PASS CYD ciblé (7 novembre 2026)

## Source et frontière

- [PR #257](https://github.com/Kharn27/DoomRPG-RE/pull/257) ; branche unique `agent/esp32-hub-hud-exact-restore` créée depuis le **véritable `main=70da56596a3828545b30da497b10754b606e05fc`** (merge #253). Pas de merge par l'agent.
- **Dernier SHA fonctionnel et testé** : `c0ef902fbe774b96a5cccab92114325a27669087`. Trois fichiers fonctionnels/CI concernés : `ESP32/src/esp_native_gameplay_hub.c`, `ESP32/test/hub_hud_baseline_host.py`, `.github/workflows/esp32-cyd.yml`. Après ce SHA, **documentation seulement**, aucun rebase/modification code.
- CI de ce SHA : [push #42363410335](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42363410335) et [PR #42363456856](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42363456856), **SUCCESS**, incluant test hôte compilant les vraies fonctions d'ouverture/capture HUD, et firmwares `esp32-cyd`, bringup, trace.
- Preuve matérielle : extrait Serial posté par l'utilisateur le **7 novembre 2026**, issu du vrai classic CYD en firmware de test. Ce retour n'est **pas un log intégral** ; les résultats se limitent au segment fourni (`uptime` environ 106749–142378 ms).

## Cause précise de la régression #253 et correctif

Sur #253, le HUD inférieur préexistant capturé à l'entrée du HUB pouvait être périmé par rapport au HUD **recomposé depuis le PlayerState courant** : lors de la fermeture navigatoire sans mutation, le hash du repaint actuel différait de la baseline ancienne, `exactBottom=NO`, et provoquait `HUB-RECOVER status=NOT_READY` avec restauration monde de secours.

#257 appelle `repaintGameplayHud()` **avant** `menuOverlayCapture()`, puis capture la baseline canonique. Un log `[HUBHUD] BASELINE-PRIME displayed=... canonical=...` n'est émis que si les bas HUD différaient. Le contrôle de fermeture `restoredProtected == exactBottomTarget` et le double-repaint strict après mutation (arme/consommable) sont **inchangés**. Aucune allocation, second framebuffer ni mutation de monde/tour/RNG ; **une passe HUD et ses accès PAK sont ajoutés à chaque ouverture**, donc coût matériel de latence non mesuré directement.

## Observations strictes de l'extrait Serial

1. **Prime avérée**, HUD périmé corrigé avant ouverture : `displayed=096d67e1 canonical=7476e485` (WEAPONS), `displayed=90427115 canonical=911a0d6d` (INVENTORY), `displayed=83af7893 canonical=69343d07` (INVENTORY après consommation). Les autres ouvertures réussissent également avec `[GAMEPLAYHUD] REPAINT`, sans prime si baseline déjà canonique.
2. **Cinq fermetures visibles, 5/5 `exactBottom=yes`** : `[HUB] CLOSE n=2` (WEAPONS 1→3, `sessionMutation=weapon-only`), `n=3` (STATUS après INVENTORY→STATUS→SYSTEM→STATUS, `sessionMutation=no`, **`exactHud=yes`**), `n=4` (INVENTORY, medkit HP `30→34`, `sessionMutation=consumable`), `n=5` (INVENTORY après navigation et sélection key non prise en charge, `sessionMutation=no`, **`exactHud=yes`**), `n=6` (WEAPONS 3→0, `sessionMutation=weapon-only`). Toutes reportent `menuUnderlayRestore=exact`, `hudRepaint=yes`, `packClosed=yes`.
3. **Aucun `HUB-RECOVER status=NOT_READY` dans l'extrait**. Les fermetures navigatoires produisent `[RESIDENTGAMEPLAY] HUB-CLOSE`, `[HUBACTIONGATE] RESUME` ; après sélection d'arme, `[HUBWEAPON] GRID-SELECT-CLOSE`; après objet, `[HUBITEM] COMMIT` suivi d'un `PASS_TURN` prévu. Le monde et les déplacements/portes/combat de tour continuent après fermeture.
4. **Intégrité de la donnée et des entrées** : les métadonnées `playerFNV` restent stables sur fermetures navigatoires, changent légitimement lors de sélection d'arme ou utilisation du medkit. HUB haut `INV/STAT/SYS`, touches, BACK/SYSTEM, et écran WEAPONS 12/12 fonctionnent dans la trace. Les armes `Hellhound/Cerberus/Demon Wolf` restent `icon=missing` (hors périmètre #257, déjà visibles dans le catalogue) : ne pas compter comme assets réparés.
5. **RAM observée, sans dérive dans ce segment** : plusieurs `[ALIVE]` identiques, `heap=176520`, `heap8=111276`, `largest8=86004`, `PAK=ready`. Le repaint reporte `reads=72 bytes=8072` dans le contexte affiché. Pas de panic, watchdog ni reboot anormal visibles dans l'extrait. Les champs `shapeData/mediaTexels` ne sont **pas affichés dans ce segment** ; leur maintien relève des invariants du projet et d'autres tests, pas de cette preuve spécifique.
6. **Hors périmètre distinct** : répétitions `[NATIVEFRAME] BOUNDARY-INLINE` présentes après gameplay, aucun lien causal établi avec le patch et aucun changement correspondant. Pas d'A/B chronométré des ouvertures, de preuve de la qualité des pixels du TFT au-delà des empreintes framebuffer, ni de vérification exhaustive de tous les BSP, SAV ou morts.

## Verdict et suite

**PASS matériel ciblé pour la régression HUD #257** : les deux scénarios historiquement défaillants (fermetures navigation sans mutation) sont maintenant `exactBottom=yes` et `exactHud=yes` ; les scénarios avec mutation arme et consommable sont `exactBottom=yes` avec `exactHud=topbar-pending`, ce dernier étant attendu jusqu'à recomposition de la barre supérieure. Le fallback pour une vraie erreur reste fail-closed.

**Code gelé à `c0ef902f` ; tout commit post-test doit rester documentaire.** Réviser la PR, vérifier la CI du HEAD documentaire puis la rendre Ready for review. Fusion par l'utilisateur exclusivement. Conserver la réserve historique de #253 et préciser qu'elle est **traitée/validée dans la portée de #257**, pas effacer l'observation initiale.
