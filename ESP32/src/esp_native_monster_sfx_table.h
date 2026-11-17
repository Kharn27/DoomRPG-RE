#ifndef ESP_NATIVE_MONSTER_SFX_TABLE_H
#define ESP_NATIVE_MONSTER_SFX_TABLE_H
#include <stdint.h>

/* Exact legacy EntityMonster.c monsterSounds[14][12], indexed by mType,
 * not sprite index/subtype. No gameplay RNG is consumed by a lookup.
 * Columns: idle, sight1..3, primary attack, alternate attack, pain,
 * unused, death1..3, unused. No special case for Malwrath or any family. */
static inline uint16_t EspNativeMonsterSfx_lookup(uint8_t mType, uint8_t field) {
    static const uint16_t sounds[14][12] = {
        {5078,5079,5080,5106,5046,5047,5085,0,5082,5084,5107,0},
        {5086,5087,0,0,5088,5088,5089,0,5090,0,0,0},
        {5078,5079,5080,5106,5046,5046,5085,0,5082,5084,5107,0},
        {5069,5070,5071,0,5072,5096,5085,0,5076,5077,0,0},
        {5108,0,0,0,5100,5100,5099,0,5101,0,0,0},
        {5108,5093,0,0,5094,5094,5099,0,5095,0,0,0},
        {5108,5097,0,0,5072,5096,5099,0,5102,0,0,0},
        {5108,5109,0,0,5072,5096,5110,0,5111,0,0,0},
        {5104,5103,0,0,5105,5112,5085,0,5113,0,0,5114},
        {5078,5115,0,0,5116,5047,5117,0,5118,0,0,0},
        {5119,5120,0,0,5121,5121,5122,0,5123,0,0,5124},
        {5108,5125,0,0,5072,5072,5099,0,5126,0,0,0},
        {5108,5128,0,0,5051,5130,5099,0,5129,0,0,5127},
        {0,5097,0,0,5096,5096,5137,0,5138,0,0,0}
    };
    return mType<14U && field<12U ? sounds[mType][field] : 0U;
}
#endif
