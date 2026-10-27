# PR #235 — jalon A : PASS CYD (27 octobre 2026)

Branche : agent/esp32-render-retire-tmpline
Base main : 47466adb879c2e4be0965fd0e62de0bf1b6fb242
Code A testé : 89f22d69a2e4e7d4f6b08bde14f8c8564357ff62
PR : https://github.com/Kharn27/DoomRPG-RE/pull/235

CI push #40547512112 et PR #40547636830 : SUCCESS (gardes hôte, géométrie différentielle 10 000 cas, builds normal/bringup/trace). Attribution du code testé par branche et flash ; les logs Serial ne contiennent pas de fingerprint de SHA.

Frontière : Render_t::tmpLine devient desktop-only, sauf fixture explicite de géométrie host. La transaction du renderer sprites n'en sauvegarde, restaure ou compare plus de copie ; segments de clipping/projection locaux inchangés. Render_t firmware normal : 1532 → 1500 octets (assert de compilation). Scratch sprites : -32 octets, borne <=740 octets. Aucune modification de Render.c desktop, PAK, RNG, SD, SAVE, moteur gameplay ou présentation.

START → INTRO → Entrance → EXIT : intro ade0195d, INTRODISP recovered=34056, BSP /intro.bsp CRC623f34e4, arena 14095/c3882516, FIRST_FRAME=71ca7465 (8 walls, 4430 pixels). MOVE, TURN, STRAFE, caisse127 transformée puis trois Armor Shards 0→4→8→12, dialogue88 EV_DIALOG(8) suivi de EV_NEXTSTATE(19) stateMutation=1, deuxième dialogue annulé sans mutation, porte275 quatre frames et fermeture différée HOLD/RELEASE. EXIT sans SAVE ; DIALOGCHAIN recovered=1144 ; RESIDENTRESET released=18008 empty=1 ; menu 0f2c9fc3.

LOAD slot04 V11 → Malwrath → EXIT : fichier 5604 B, 30 monstres beaeaccc, topologie 50e16fe3, positions e2a9d90a, activation 13→14 à la porte113. Sprite324 copies=4, draws 4→2→0, clipping cohérent à l'approche. Porte113 quatre frames et fermeture HOLD/RELEASE. Deux ripostes de trois projectiles 204 chacune, drainés avant MONSTERRETAL COMMIT ; joueur HP34→27→21 et armure23→16→10. Deux tirs shotgun Malwrath HP11→7→0, death4→corpse2, drop Armor Shard tile285 ramassé, Flak Jacket tile253, secret portes112/111 +5XP. EXIT sans SAVE ; RESIDENTRESET released=18008 empty=1 ; menu 0f2c9fc3.

Heap8 gameplay stable 113972 B, largest=86004 ; menu post-EXIT 159908 B, largest=110580, soit +36 B libres par rapport aux checkpoints comparables de PR #234. shapeData=mediaTexels=NULL aux frontières observées. Aucun panic, WORLD_POST fail ni divergence RNG visible.

Limites : BOUNDARY-INLINE et abscisses MALWRATHPROJECTILE supérieures de 0x40000000 à NATIVESPRITE TRANSIENT persistent, non diagnostiquées ici. Pas de test toutes cartes, endurance, SAVE neuf, parité pixels globale, A/B de performance ni SHA embarqué.

Décision : PASS matériel ciblé A, code gelé au 89f22d69. Documentation-only après PASS, vérification CI post-doc, puis jalon B distinct sur la même branche ; jamais de merge dans main par l'agent.
