"""Undo exactly P12 barrel weapon admission, before P11 historical inverse."""
import json
EDITS=json.loads("[[\"(unsigned)candidate.targetSprite,(unsigned)candidate.targetTile,\\n               (unsigned)candidate.weaponId,(unsigned)candidate.loops,\\n               (unsigned)aiDecision);\",\"(unsigned)candidate.targetSprite,(unsigned)candidate.targetTile,\\n               (unsigned)aiDecision);\"],[\"printf(\\\"[AI14TARGET] ARM probe=%u attacker=%u targetSprite=%u targetTile=%u weapon=%u loops=%u aiRand=%u combatRng=0 mutation=no\\\\n\\\",\",\"printf(\\\"[AI14TARGET] ARM probe=%u attacker=%u targetSprite=%u targetTile=%u weapon=15 aiRand=%u combatRng=0 mutation=no\\\\n\\\",\"],[\"if (barrelWeaponOwned(monster->subtype,monster->alternateAttack,weaponId) &&\\n            target.type\",\"if (monster->subtype == 3U && weaponId == 15U &&\\n            target.type\"],[\"/* The same non-player target transaction supports weapons whose native\\n * visual path is already owned. Legacy 2/3/4 have no missile, 12/13/14\\n * use an invisible missile, and owned weapon15 emits sprite204.\\n * Do not silently substitute a missing projectile for 7/16/17 or subtype7. */\\nstatic int barrelWeaponOwned(uint8_t subtype,uint8_t alternate,uint8_t weapon) {\\n    switch (weapon) {\\n        case 2U: case 3U: case 4U:\\n        case 12U: case 13U: case 14U:\\n            return 1;\\n        case 15U:\\n            return EspNativeMonsterProjectile204_attackOwned(subtype,alternate);\\n        default:\\n            return 0;\\n    }\\n}\\n\\nstatic int findCandidateForSprite(\",\"static int findCandidateForSprite(\"],[\"#include \\\"esp_native_gameplay_monster_turn.h\\\"\\n#include \\\"esp_native_monster_projectile204_contract.h\\\"\",\"#include \\\"esp_native_gameplay_monster_turn.h\\\"\"]]")

def restore_pre_barrel_weapon(name,current):
    if name!="esp_native_gameplay_monster_turn.c": return current
    for new,old in EDITS:
        new,old=new.encode(),old.encode()
        assert current.count(new)==1,(name,new[:100])
        current=current.replace(new,old,1)
    return current
