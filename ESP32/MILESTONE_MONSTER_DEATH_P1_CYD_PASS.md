# MONSTER-DEATH-P1 — PASS CYD borné (24 novembre 2026)

PR #452. Base main 277ba73345040eec9d8669dada98f4a3a2c0b32f ; code candidat a4bcaf247fe8f5ecf76c9ba2fb9674c0755c1a58 (identifié par contexte de test, SHA non imprimé par le firmware).
Double CI code SUCCESS : [push](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45357955137), [PR](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45358027934). Builds GitHub Actions, pas locaux.

## Contrat
Le propriétaire combat natif partage prepareDeathAfterXp entre direct et radius baril : XP reste chez les appelants, choix sonore puis un mot RNG drop et plan non commis. Mort normale P5, gib 5091 P3 sans RNG sonore, douleur P2. Transactions, rollback et publication post-commit existants conservés. Aucun nouveau buffer ou allocation. Oracle C extrait : 100 cas ordinaires, gardes des familles spéciales et échecs, historique SDL préservé.

## Preuve matérielle
Log platformio-device-monitor-261124-094612.log : 270665 octets, 1811 LF (1812 lignes logiques), SHA256 a9720746d7b42934d36f5582091c42485d0e30c7388be69c40507b976a2b8a56. Classic CYD sans PSRAM, framebuffer 160x120, LOAD V11 Junction puis Sector 1.
- seq12 : baril sprite269, douleur sprite237 subtype5 HP11->3, 5099 P2 publié après BARREL COMMIT.
- seq16/18/22 : morts directes gib sprites0/1/237, XP5/5/6, 5091 START, drop live seq22.
- seq40 : baril sprite238, mort sprite218 subtype3 HP11->0, XP7, dropRoll 08d86ad7, rngConsequence=2, 5076 P5 après commit.
- HEADROOM_OK (marge8 Sector1 18416 avant audio), shapeData/mediaTexels NULL. Audit retour menu : queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0, 39 SFX joués, 3 handoffDiscarded (distinct des drops), pending=0 ; SYSEXIT MENU-READY resident=empty.
- Retour auditif utilisateur : plusieurs tests barils, sons nickel, attaques multiples ; autorisation explicite de fusion.
 
## Limites
5095 non exercé ici : subtype5 survit au baril puis gib. Mort directe normale, chaîne multi-barils (chainRemoved=1 ici), familles spéciales 7/8/12/13, autres armes différées et rollback forcé non validés physiquement dans cette capture. Ne pas confondre oracle host et test exhaustif. Corpse trim et conséquences spéciales restent différés. Latence des tours multi-monstres et ciblage à travers feu sont des sujets indépendants.
Post-test : documentation uniquement.
