# Root-H — retrait du pointeur retour Render → DoomRPG : PASS sur classic CYD

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/295
- **Candidat code testé** : `07b2e055894ecd6ecec4e831ba22f6325cca594c` (attribution par parcours de flash utilisateur ; SHA firmware non imprimé dans ce log).
- **Base exacte** après fusion SDL-F2 #296 : `acdb4751963dc2cc68af457f776d895517b2fe0e`.
- **CI** : https://github.com/Kharn27/DoomRPG-RE/actions/runs/43011962445 (push) et https://github.com/Kharn27/DoomRPG-RE/actions/runs/43011970302 (PR) — **SUCCESS** tous deux.
- **Serial réel** : `device-monitor-261111-122807.log`, 1668 lignes, 200775 octets. Capture débutant dans OPTIONS à `uptime=10006 ms`, sans début de boot.
- **Environnement** : `esp32-cyd`, classic CYD sans PSRAM (cible du jalon ; ce log partiel n'inclut pas la ligne PSRAM).

## Frontière de code

`src/Render.h` conserve `Render_t::doomRpg` uniquement dans les builds desktop/oracle ; les profils ESP32 normaux, bringup et trace le suppriment. `EspNativeEngineRoot_initRender(Render_t* render)` initialise désormais le shell sans paramètre DoomRPG, sans retour vers la racine. Dans `render_startup_bridge.c`, l'initialiseur exige un canvas propriétaire vivant via `EspNativeRootOwners_live()`, conserve le framebuffer partagé 160×120 et lit les deux fichiers PAK `sintable.bin` / `palettes.bin` via `EspLegacyAssetSource_readAlloc` (ancien `DoomRPG_fileOpenRead` supprimé de ces deux chemins). Aucun runtime ZIP, nouveau framebuffer, backing buffer de carte ou mutation gameplay.

La taille Xtensa du `Render_t` est assertée à **108 B**, contre **112 B** avant Root-H : −4 B par instance, pas un gain RAM statique garanti. L'identité `DoomRPG_t` demeure active pour le canvas et des services de processus ; aucune suppression globale de `doomRpg` n'est encore autorisée.

## Validation CI et fingerprints

- `root_h_render_backref_host.py` : champs absents sur normal/bringup/trace, préservés sur desktop/oracle, scan **360 sources**.
- `native_render_init_retirement_host.py` : **4096 cycles** constructeur/OOM ; fingerprint legacy normalisé `d5805374` conservé.
- `native_render_setup_retirement_host.py` : **4096 cycles** layout/OOM ; fingerprint `67b0fc80` conservé.
- `native_render_startup_retirement_host.py` : **4096 cycles** framebuffer/sinus/palettes plus **six chemins fail-closed** ; fingerprint `1ca3268b` conservé.
- `native_render_palette_retirement_host.py` : **4096 cycles** conversions RGB565/allocation/libération ; fingerprint de la palette desktop `3573ad2c` conservé.
- Plateforme : builds `esp32-cyd` (859817 B flash / 80768 B RAM statique), `esp32-cyd-bringup` (882541 B / 80920 B), `esp32-cyd-trace` (877957 B / 80816 B), **0 symbole SDL lié / 0 octet de symbole SDL dans les 3 ELF**.
- Comparaison compile normale SDL-F2 seul : **859921 B / 80768 B** → Root-H + SDL-F2 **859817 B / 80768 B** : −104 B flash, RAM statique identique. Aucune conclusion sur la RAM dynamique sans A/B matériel.

## Validation matérielle ciblée

Le Serial démarre dans OPTIONS : il **ne contient pas** le reset/PSRAM, `Engine structs Render=108`, le log de lecture initiale `sintable.bin` ni le chargement `palettes.bin`. Le démarrage effectif et le bon usage en jeu des données sont validés indirectement par les images et sessions affichées ; la taille et les chemins directs sont validés par CI, pas par une ligne Serial du début.

Parcours vérifié :

1. **MENU_MAIN → OPTIONS/VIDEO** : luminosité **100→75→100**, deux `NATIVECONFIG SAVE result=committed` version 3 ; BACK sans reload, FNV menu `1945fafa`.
2. **HELP** : 112 lignes totales, navigation réelle pages `0→7→14→21→28→35→42→49→56→63` (pas jusqu'au bout), retour au même FNV menu.
3. **START → INTRO → Entrance** : intro texte/animation/hand validés, puis `/intro.bsp` CRC `623f34e4`, arène map **14095 B**, `ENGINESESSION FRESH-VISIBLE`, rendu 160×120 et HUD après chargement.
4. **Jeu** : **8 DIALOG OPEN / 8 CLOSE**, événements 8/26, scripts `EV_NEXTSTATE/EV_CHANGESTATE` repris ; **4 DOORANIM COMPLETE** ; **13 PLAYERRES COMMIT**, **13 PICKUPMINI START/END**, cases loot, Fire Ext, grenades de feedback ; aucun pickup mini laissé en attente.
5. **Combat** : Malwrath sprite322 attaque 3 fois avec projectiles 204, chaque effet drainé, **3 MONSTERRETAL COMMIT** ; impact, bordure directionnelle, flash, 3 tirs/animation et HP/armor commits. Le joueur riposte (monster combat/hit) et le gameplay continue.
6. **Deux cycles mémoire** : HUB/EXIT puis LOAD V11 slot04 (**5604 B**) et second HUB/EXIT. `2× [RESIDENTRESET] released=18008 ... empty=1` ; `2× [SYSEXIT] MENU-READY frame=1945fafa ... resident=empty`.
7. **RAM** : `2× [RAMBUDGET] RESERVE ... advisory=HEADROOM_OK` avec marges heap8 **28232 / 27420 B** et DMA **69192 / 68380 B** ; `shapeData=0x0 mediaTexels=0x0` simultanément à **13 frontières** observées.
8. **Diagnostics** : **13 `[NATIVEFRAME] BOUNDARY-INLINE`** (connus/non fatals dans ce parcours), aucun panic, Guru Meditation, WDT, abort ou `[ERROR]` fatal dans le log.

## Verdict et suite

**PASS matériel ciblé de Root-H** sur le vrai CYD pour le chargement et l'usage continu des palettes/sinus natifs, rendu/menus/jeu/combats, lecture V11 et libération RAM. Les limites sont explicitement conservées : capture sans boot ni preuves de taille `Render_t` dans le Serial ; pas de mesure A/B de performance ou de heap dynamique, pas de couverture exhaustive de toutes les cartes. Les tests CI portent précisément sur le layout et les lectures initiales.

**Gel du code après ce PASS.** Toute modification sur la PR #295 après test est strictement **documentaire**. Une fois les CI docs-only vertes, la fusion sur `main` est autorisée par l'utilisateur.

**Frontière suivante à auditer** : le pointeur retour `DoomCanvas_t::doomRpg` est encore fonctionnel dans le code natif ; il faut identifier ses véritables lecteurs (libération d'intro, images, menu) et les remplacer sans briser les paths PAK ou le fatal teardown. Ne pas retirer l'identité statique `DoomRPG_t` et le global `doomRpg` avant un jalon dédié.
