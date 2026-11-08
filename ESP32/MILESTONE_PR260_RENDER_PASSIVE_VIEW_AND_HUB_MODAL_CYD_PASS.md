# PR #260-C/D/E — Render_t=160 et frontière modal HUB : PASS matériel classic CYD

**8 novembre 2026**, branche unique `agent/esp32-render-retire-desktop-screen-origin`, [PR #260](https://github.com/Kharn27/DoomRPG-RE/pull/260). **Dernier commit de code éprouvé sur le CYD : `ab1c51fdf369e7a4b84788abc5a79c5f62fcce5c`** ; précédé du correctif async `e4c17cc1b9be8263e3f8ae0fa8dc81aa3199c70b` et du retrait ABI `a5eb604d5f285fc9a8f74a5bcbc4ea033b0d54b8`. Suite de la synchronisation avec `main=fcdce5cdef9bea63dbd52059fe4d66affccdbe83` (PR #262, changements LOAD GAME indépendants), les commits ultérieurs ne modifient pas le code propre à cette PR.

## Frontière source et invariants

Le retirement natif-only de `Render_t::viewAngle` et `Render_t::spanMode` conserve les champs de l'oracle desktop/géométrie et le **vrai angle** de `EspPlayerViewState`. `Render_t` est passé de **168 à 160 octets** (et **184 à 160** sur l'ensemble de la PR #260), sans affecter la structure logique 160×120 RGB565, le viewport 160×80 à (0,20), le PAK natif, les mécanismes de combat ni l'allocateur. `shapeData == NULL` et `mediaTexels == NULL` restent des invariants.

L'incident remonté en combat Malwrath à l'ouverture de la grille était un conflit de propriétaire d'affichage, **pas un panic CPU démontré** par le Serial. Après `[HUB] OPEN page=weapons`, `servicePlayerPainExpiry()` pouvait rendre le monde pendant que le HUB détenait le framebuffer. Le correctif `e4c17cc` garde le HUB comme barrière pour les cinq rendus asynchrones `serviceHitExpiry`, `servicePlayerPainShake`, `servicePlayerPainExpiry`, `serviceGibAnimation`, `serviceExpiry`. Les expirations restent dues jusqu'à reprise du monde ; aucune mutation gameplay n'est supprimée.

Le test matériel suivant (`device-monitor-261108-024933.log`) a révélé une seconde fuite : `[HUBWGRID] FRAME` puis `[PLAYERIMPACT] PAINT probe=1 sprite=184 frame=2/2` avant `[HUB] FRAME`. Le correctif `ab1c51fd` emprunte **`__real_Esp32PlatformVideo_present()`** dès que le HUB possède l'écran, avant les décorateurs impact/HIT/GIB/sang et le feedback monde. Ce chemin ne rajoute aucun framebuffer, allocations, temporisation de saisie, accès ZIP ou mutation RNG.

## Validation CI

Pour `ab1c51fdf369e7a4b84788abc5a79c5f62fcce5c`, [CI push #42436230988](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42436230988) et [CI PR #42436234682](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42436234682) : **SUCCESS**, 98 étapes réussies chacune, y compris compilation `esp32-cyd` normale, bringup, trace et tests hôte de la présentation modal→monde→modal avec les fonctions de production. Ces CI ne remplacent pas un test visuel sur le CYD.

## Validation matérielle sur le véritable CYD

**Confirmation explicite utilisateur : « Impeccable tu vas pouvoir merger ces belles modifs ».** Le Serial collé avec cette acceptation contient un LOAD de `/DoomRPG-ESP32-slot04.sav` vers Entrance, le combat réel face au Malwrath, trois salves de projectiles, impacts natifs `sprite=184` et ouvertures/fermetures de la grille pendant et après la fenêtre de dégâts.

- **Premier parcours :** `[PLAYERPAINFX] EXPIRE ... frame=15807524`, `[HUBWGRID] FRAME weapons=12/12`, `[HUB] FRAME paint=1 page=weapons frame=7c1aa8ce`, `[HUB] CLOSE n=1 ... exact=yes exactHud=yes exactBottom=yes`. Aucun `PLAYERIMPACT PAINT` entre `HUBWGRID FRAME` et `HUB FRAME`.
- **Parcours pendant l'effet de dégâts :** après `[PLAYERIMPACT] PAINT probe=2 ... frame=0/2`, `[PLAYERPAINFX] FIRST-PRESENT ... visibleWindowMs=500`, `[RESIDENTGAMEPLAY] QUEUE ... action=WEAPON_MENU`, `[HUBWGRID] FRAME weapons=12/12`, `[HUB] FRAME paint=2 page=weapons frame=dbc44f56`, `[HUB] OPEN n=2`. Pas de `[PLAYERIMPACT] PAINT` entre la grille et son `HUB FRAME`; validation visuelle déclarée par l'utilisateur. `[HUB] CLOSE n=2 ... exact=yes exactHud=yes exactBottom=yes`, suivi de `[PLAYERPAINFX] EXPIRE ... frame=de4b0bad` après reprise du monde.
- **Autres mutations :** `[WEAPONCONTROL] COMMIT ... weapon=1->2` sans tour avancé, coups sur Malwrath et dégâts reçus, puis menu inventory→system→EXIT : `[RESIDENTRESET] ... empty=1`, `[SYSEXIT] MENU-READY ... session=off resident=empty`. Les préconditions d'impact n'interfèrent plus avec le HUB.
- **Mémoire :** `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`, `heap8=109780` à l'initialisation puis `heap8=109740` / `largest8=86004` stable pendant le combat, `shapeData=0x0`, `mediaTexels=0x0` sur `ENGINESESSION READY`. Aucune séquence panic/WDT n'est montrée dans cet extrait de Serial.

**Limites de preuve :** la capture finale est un extrait collé, et non un log série intégral ni un contrôle d'endurance exhaustif. La disparition de la trace est validée par l'acceptation visuelle de l'utilisateur, appuyée par les marqueurs de composition ; les autres cartes, types d'impact et thèmes restent hors périmètre de ce test. Les lignes de mesures RAM viennent du firmware normal `esp32-cyd` déclaré par le testeur ; les builds CI ne prouvent pas la RAM physique.

## Clôture

Milestone fonctionnel matériellement **PASS**, prêt à merger après commits **docs-only** et dernière vérification CI du HEAD réconcilié. La branche a été synchronisée avec le `main` actuel par commit à deux parents `2de4519da6d2ebb798ddcbdfcf67fafcade492f6` sans forcer ni écraser le travail concurrent. Aucune fusion vers `main` ne doit être prétendue avant retour explicite de l'API GitHub.
