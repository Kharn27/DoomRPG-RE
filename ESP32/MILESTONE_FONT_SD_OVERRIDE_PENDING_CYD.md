> **Note postérieure — PR #413 :** la livraison SD a été dédupliquée (plus de `a_doom.bmp` ni de variantes `larger_font_doom*` dans `sdroot`) et la palette de `larger_font.bmp` harmonisée avec `a.bmp`, sans changement des pixels-index de glyphes. Les assertions historiques d'identité binaire ci-dessous décrivent uniquement l'état de ce milestone d'origine. Voir `artwork/fonts/README.md` et `MILESTONE_UAC_120_GENERIC_FONT_PENDING_CYD.md`.

# FONT-SD-P1 — surcharge optionnelle des deux planches de police, PAK natif en fallback — PENDING CYD

**Base exacte** : `main=0ecf61366140f9ef3c279e33111055ea0f6dd8c8` après fusion de la PR #407. **Branche** : `agent/font-sd-overrides-pak-fallback`. **Statut : loader SD observé sur vrai CYD pour `a.bmp` AVANT retouche ; aspect des 4 glyphes corrigés et `larger_font.bmp` encore PENDING CYD.**

## Contrat fonctionnel

- `a.bmp` : `/doomrpg/img/a.bmp`, **144×72**, glyphes 9×12, 16 colonnes × 6 lignes.
- `larger_font.bmp` : `/doomrpg/img/larger_font.bmp`, **208×102**, glyphes 13×17.
- Le répertoire **`sdroot/doomrpg/img/` contient désormais les deux fichiers directement prêts à copier** : `a.bmp` (copie exacte de `a_doom.bmp`, 4 bits) et `larger_font.bmp` (copie exacte de `larger_font_doom.bmp`, 4 bits). `main` contenait auparavant sous ce second nom le BMP **24 bits incompatible** ; la branche le remplace par l'atlas indexé. Les variantes `_doom` restent disponibles en références, mais le BMP RGB24 `larger_font_doom_24bit.bmp` (63 702 o) est désormais **supprimé du répertoire SD** : il n'est pas pris en charge, et reste récupérable dans l'historique Git #407. Aucun alias runtime implicite.
- Un BMP SD présent et compatible (BMP indexé non compressé 1/4/8-bpp, palette et bornes valides, géométrie exacte) prime. En absence de SD, erreur d'ouverture, mauvaise géométrie, format 24-bpp non supporté ou mauvais contenu initial, le PAK natif `/DoomRPG-ESP32.pak` reste la source. Pas de ZIP runtime.
- Pas d'auto hot-reload : mise à jour par **redémarrage**.
- Les erreurs physiques ultérieures pendant un blit provoquent l'échec borné de cette peinture ; l'ouverture suivante utilise le PAK. Il n'y a pas de substitution en cours de demi-frame.
- Le filtre de teinte des dialogues reste appliqué au PAK d'origine, mais pas au thème SD coloré, dont la palette doit rester intacte.

## Architecture native

1. `esp_native_font_sd.cpp` possède les deux seuls fichiers autorisés ; état et handles minuscules, sans copie du BMP en RAM. C API `esp_native_font_sd.h` : probe, read range, reject et suspension.
2. `EspNativeIndexedBmp_open/blit/tile` choisit un ID de source et lit les plages glyphes/scanlines au travers du même **scratch statique borné à 1 KiB**. Les autres BMP demeurent PAK-only ; géométries exactes vérifiées et repli avant publication des métadonnées.
3. `EspNativeImage_loadPack` étend son chargement initial aux deux polices seulement, réutilise `EspNativeImage_decode`, son allocation persistante des pixels **indexés compacts** et sa palette, puis transfère leur propriété comme avant. En cas de rejet, route PAK inchangée.
4. `esp_asset_pack.cpp` appelle `EspNativeFontSd_suspend` avant les trois ouvertures physiques SD du PAK/probe. Si `EspAssetPack_musicSdSafe()==0`, le lecteur de police décline le SD et garde le PAK pour ne pas violer l'exclusivité du lecteur. Dès retour au PAK flash-backed, le SD peut être reprobed.
5. Aucune PSRAM, aucun `shapeData`, aucun `mediaTexels`, aucun buffer BMP complet, aucune modification du framebuffer logique 160×120. Les caches SDK/handles `File` éventuels sont à mesurer **sur matériel**, pas présumés nuls.

## Vérification hôte / CI

Le test `ESP32/test/font_sd_override_host.c`, qui compile les **deux vrais décodeurs C** avec des stubs déterministes PAK/SD, doit vérifier :

- manque de surcharges -> lectures PAK et même géométrie ;
- 4-bpp 16 couleurs SD -> blit 9×12 et transparence magenta 0xf81f ;
- `larger_font` 208×102 SD ;
- le test Python d'assets vérifie les **fichiers livrés sous les noms runtime**, leurs palettes et leur stricte identité binaire avec les variantes Doom ; il verrouille également les **94 autres glyphes intacts de chaque planche**, les nouveaux `a/o`, leurs dimensions et la suppression du RGB24 ;
- dimension incorrecte -> repli PAK sans métadonnées ambiguës ;
- RGB24 -> rejet et PAK, sans allocation/chargement 24-bpp ;
- importer `EspNativeIndexedImage` depuis SD, puis rejet et repli PAK, fermeture logique de PAK.

**Ces tests n'établissent ni compilation firmware locale ni PASS CYD physique.** Vérifier les workflows push et PR et l'ELF PlatformIO normal `esp32-cyd`.

## Testeur, sur vrai ESP32-2432S028R classic CYD

1. Compiler/flasher la branche `agent/font-sd-overrides-pak-fallback` avec `esp32-cyd` ; conserver les Serial depuis reset et les captures des menus/texte.
2. SD **sans** `doomrpg/img/a.bmp` ni `doomrpg/img/larger_font.bmp` : le jeu doit fonctionner à l'identique avec fallback. Inspecter `[FONTSD] FALLBACK` et absence de nouveau panic/WDT.
3. SD avec les deux BMP 4-bpp **directement copiés depuis `sdroot/doomrpg/img`** : inspecter `[FONTSD] ACTIVE` et les zones de jeu suivantes : HUD, dialogue (incluant teintes), HUB inventaire et notes, démarrage/menu, texte Canvas.
4. Retirer un seul des deux BMP et recommencer : source mixte et repli individuel.
5. À la transition MENU/NEW GAME/MAPLOAD, tester le fallback lors d'un PAK physiquement SD ; vérifier absence d'accès SD concurrent non sécurisé, musique/SFX normaux, 0 underrun/recovery, HEADROOM_OK et `shapeData/mediaTexels=NULL`.
6. Si le rendu paraît tronqué ou colorisé par erreur, joindre le log exact et une photo. Pas de fusion avant test sur vrai CYD et revue des logs.

## Limites

La source PAK indexée est conservée et la surcharge n'est pas une dépendance du runtime. Les polices SD sont sensibles à l'encodage BMP et à la palette magenta. L'ancien BMP 24-bpp de la PR #407 n'est plus présent dans la livraison SD ; il reste récupérable dans l'historique Git (jamais décodeur runtime).

## Retour CYD réel du 21 novembre 2026 — preuve bornée

Log utilisateur `platformio-device-monitor-261121-110206.log` :
328 lignes, 29 837 octets, SHA-256
`950da58b2f5b04bbb7901e13375e8995a9be23858d9a3b56f5ebbf8625eb095d`.
La session a montré la vraie source SD `[FONTSD] ACTIVE name=a.bmp source=SD indexed=4 palette=16 bytes=5302`, puis l'affichage Help/About (`[MAINMODEL] HELP-NATIVE lines=112`), la pagination et le retour au menu. Aucun panic/WDT n'est visible dans cet extrait ; `larger_font.bmp` n'y est **pas** attestée chargée. L'utilisateur confirme l'attrait des effets de coloration, mais signale des `a`/`o` malformés.

**Suite ciblée, PENDING vrai CYD :** seuls les quatre glyphes `a/o` des deux BMP sont remplacés (fond/placement et 94 autres glyphes inchangés), ainsi que les variantes `_doom` strictement identiques ; RGB24 de 63 702 o retiré ; aucune retouche au code du moteur ni aux thèmes. Contrôle hôte attendu : 94 empreintes de glyphes inchangées par planche, deux nouveaux glyphes complets et BMP indexés valides. **Le log ci-dessus ne valide pas les nouveaux pixels ; un nouveau test visuel hardware est nécessaire** (Help, HUD, dialogues, et un trajet faisant réellement charger `larger_font.bmp`).

## Retour matériel approfondi et éclaircissement thématique (21 novembre, PENDING nouveau firmware)

Le vrai test `platformio-device-monitor-261121-121000.log` comporte
**1312 lignes**, SHA-256
`827674016d701b7ff4ca0ce94a9435334564fc6dc18005768baf4dd69e3c1b8b`.
Thème persistant `theme=2`, **165** annonces
`[FONTSD] ACTIVE name=a.bmp source=SD indexed=4 palette=16 bytes=5302`,
mais aucune annonce `larger_font.bmp`. Help/About, HUD, dialogues événements
88/89/79/69, HUB et EXIT SYS sont exercés ; 4 dialogues sont fermés,
`[RAMBUDGET] ... HEADROOM_OK margin8=12828`, puis
`[RESIDENTRESET] released=18008 empty=1` et
`[SYSEXIT] MENU-READY`. Aucun panic/WDT dans ce log.

**Observation visuelle utilisateur** : police Doom appréciée mais glyphes teintés
un peu trop sombres sous le thème ; les lettres corrigées conviennent.
Point audio **hors scope** à suivre séparément si reproductible :
le générique menu lors de `MENU_END_ONCE` rapporte
`underruns=674 recoveries=1 maxSilentFrames=674`, tandis que les finales
`INTRO_EXIT_BEFORE_MAP` et `MENU_ENTRY_REPLACE_PRIOR` n'ont aucun underrun.
Ne pas attribuer cette observation au rendu de police en l'absence de preuve.

### Correction de rendu proposée : SD font themed lift

- Les **deux BMP 4 bits réparés ne changent pas** (source d'art et palettes).
- Origine explicitement marquée lors d'un **décodage SD validé** :
  `EspNativeIndexedImage.sdFont`, puis `EspNativeTexture.sdFont`.
  **PAK = 0**, tout autre BMP = 0.
- `EspNativeFontTint_apply` conserve à l'identique la modulation RGB565
  précédente lorsque source PAK/texture ordinaire ou couleur de modération
  **blanche (255,255,255)**. Pour **police SD effectivement teintée**,
  canal par canal : `min(max565, (ancien_canal*5+2)/4)`.
  Cela représente un gain borné d'environ **25 %**, laissant le noir noir
  et les canaux de thème nuls à zéro, sans blanchir le fond ni dégrader
  la clé magenta (éliminée **avant** modulation).
- Ne pas retoucher `EspNativeIndexedBmp_blit` : HUD/dialog/HUB à palette
  directe n'appliquent pas la même multiplication RGB que
  `EspNativeTexture_blit` de la Canvas. Leurs palettes SD et le filtrage
  PAK historique restent intacts ; aucun changement global de thème.
- Test hôte dédié : **65 536 couleurs RGB565** vérifiées pour les chemins
  PAK et SD sans modulation, teintes multiples/contraste/black, saturation,
  plus contrôle de propagation de source dans les décodeurs C existants.
  Aucune allocation, LUT, copie de palette ou nouveau buffer framebuffer.

**Nouveau changement de code** après le test utilisateur :
la branche requiert à nouveau **double CI SUCCESS puis un test du firmware
normal `esp32-cyd` sur le vrai CYD** avant de passer merge-ready. Les logs
hardware précédents attestent la base et le défaut visuel mais **pas**
la nouvelle luminosité. L'utilisateur n'a pas besoin de remplacer les BMP
une nouvelle fois : ce correctif est intégralement dans le moteur.
