# SDL-F9 — Render startup SDL-free, PASS matériel classic CYD — 11 novembre 2026

## Provenance
- PR [#309](https://github.com/Kharn27/DoomRPG-RE/pull/309), famille SDL [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269).
- Base de code intégrée testée (déclarée par le flash utilisateur) : `761b52aa24483bdc137d3cf0ca34f80bb782ecdd`, issue du vrai `main=ed45aae1c0617af7b81cc2cb31da820edb2334cb` après fusion Root-J3 #307. **Le firmware ne diffuse pas son SHA en Serial** : attribution au candidat flashé, pas d'identification binaire indépendante.
- CI exacte après réintégration : [push #43058624052](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43058624052) SUCCESS et [PR #43058636937](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43058636937) SUCCESS ; 3 profils PlatformIO, hôtes et audits ELF.
- Firmware normal de CI : **859313 B flash / 80736 B RAM statique** (−168 / −32 B vs F8 859481/80768), pas de mesure runtime/A-B ni de gain CPU ou heap8 attribué.
- Matériel : `device-monitor-261111-174631.log`, **1684 lignes / 189980 octets**, un boot POWERON_RESET du classic CYD ESP32-D0WD-V3 240MHz, 4MB flash, **PSRAM=0**, `env:esp32-cyd`.

## Modifications vérifiées
- `ESP32/src/render_startup_bridge.c` : plus d'import `<SDL.h>` / `SDL_Video.h` ni de lecture `sdlVideo.rendererW/H`. Géométrie fixe issue de `platform_video_config.h` 160×120 ; contrôle statique de la cohérence du canvas.
- Retrait des alias `SDL_malloc/free` de ce seul TU au profit des routines libc identiques ; aucune ressource/owner/framebuffer supplémentaire.
- Vérification des gardes du vrai framebuffer partagé : pointeur, capacité **38400 B**, pitch **320 B**, table trigonométrique **1024 B**, palette **3280 entrées / 6560 B**. Le test SDL-F9 restaure précisément le corps avant modification : FNV `bd5d5bed`. Les oracles historiques Root-J2/J3/Render restent actifs.

## Observations Serial réelles
- SD montée **240MiB**, PAK indexé **241 entrées**, `PRERENDER READY` (4156B), `RENDERSTART READY`, `RENDERSTART result=1`, écran physique 320×240 / logique 160×120 RGB565.
- OPTIONS→VIDEO→THEME (2→0 persisté), BRIGHTNESS (75→100 persisté), retour MENU ; START→INTRO (50ms, 7 touches final continue) → `INTRODISP READY` récupérant **34080 B** → /intro.bsp / Entrance → gameplay. Boucle normale de mouvement/tour/dialogue, deux caisses transformées, ramassages, ouverture et fermeture différée des portes.
- **6 dialogues OPEN (dont 1 standalone) / 6 CLOSE**, **4 DOORANIM COMPLETE**, **11 PICKUPMINI START / 10 END observés** ; la miniature restante est suivie d'un démontage de session, sa complétion individuelle n'est pas visible.
- **2× MONSTERRETAL COMMIT** et **2× MALWRATHPROJECTILE CLEAR** après animation, gameplay restauré ; chargement du checkpoint **V11 slot04, 5604 B**, session `RESUME-VISIBLE` puis EXIT.
- **2× `HEADROOM_OK`** : marges heap8 **28268 puis 27456 B**, marges DMA **69228 puis 68416 B**. **2× `RESIDENTRESET released=18008 empty=1`**, **2× `SYSEXIT MENU-READY frame=1945fafa`**, **13×** invariants `shapeData=0x0 mediaTexels=0x0`. Menu HELP retesté après deuxième sortie (scroll jusqu'à 42 puis Back).
- **0** panic/Guru/abort/WDT observé ; **8 `NATIVEFRAME BOUNDARY-INLINE`**, diagnostic déjà connu et non fatal. Aucun signe de régression de démarrage Render ni de retour menu.

## Limites et suite
PASS matériel **ciblé** ; pas de test OOM, de tous les actifs d'images, de longue endurance ou de preuve binaire SHA. Après ce PASS, **geler le code testé** : commits PR post-test uniquement de documentation. Fusionner une fois CI du commit documentaire réussie et protections de branche respectées.

Suite SDL : auditer sur **le vrai `main` après fusion**, choisir une nouvelle frontière réellement compilée et bornée, sans retirer `SDL.h` ou l'ABI desktop/oracle en masse et sans toucher au firmware audio en cours d'expérimentation.
