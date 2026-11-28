#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_barrel_radius.h"
#include "esp_native_gameplay_explosion.h"

/*
 * Regression for CYD barrel at sprite 186 / tile 241:
 * radius neighbor tile 177 contains linked line 288, type=0/subtype=1.
 * The legacy leaves that line non-hurtable. The previous native preflight
 * handled only type=0/subtype=255 and rejected the entire explosion.
 */
int main(void) {
    unsigned int subtype;
    for (subtype = 0U; subtype <= UINT8_MAX; ++subtype) {
        assert(EspNativeGameplayBarrelRadius_lineNoop(
                   0U, (uint8_t)subtype));
    }
    assert(EspNativeGameplayBarrelRadius_lineNoop(12U, 3U));
    assert(EspNativeGameplayBarrelRadius_lineNoop(12U, 4U));

    /* Other destructive and gameplay-bearing families remain fail-closed. */
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 1U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 2U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 0U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(1U, 4U));
    assert(EspNativeGameplayBarrelRadius_lineNoop(2U, 0U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(7U, 1U));
    assert(EspNativeGameplayBarrelRadius_lineNoop(10U, 0U));
    /* Exhaust all type/subtype bytes, including unsupported/unknown types. */
    for (unsigned type=0; type<=UINT8_MAX; ++type) {
        for (unsigned sub=0; sub<=UINT8_MAX; ++sub) {
            int expected = type==0 || type==2 || type==9 || type==10 ||
                           type==14 || (type==12 && (sub==3 || sub==4));
            assert(EspNativeGameplayBarrelRadius_entityNoop(type,sub)==expected);
            assert(EspNativeGameplayBarrelRadius_lineNoop(type,sub)==expected);
        }
    }
    puts("A2-P1j inert radius: PASS 65536 type/subtype pairs, shared line/sprite rule");
    /* Single-barrel monster damage is applied after its one blast word.
     * Multi-barrel without monsters preserves the full consecutive preview.
     * Multi-barrel WITH monsters previews only the first word; subsequent
     * words must be drawn after earlier monster consequence RNG. */
    assert(EspNativeGameplayBarrelRadius_previewWords(1U, 0U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(1U, 1U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(2U, 0U) == 2U);
    assert(EspNativeGameplayBarrelRadius_previewWords(16U, 0U) == 16U);
    assert(EspNativeGameplayBarrelRadius_previewWords(2U, 1U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(16U, 8U) == 1U);
    /* Every planned ordinal can kill; invalid cursors are rejected.
     * First-only preview is required for potential player death. */
    assert(EspNativeGameplayBarrelRadius_previewWordsForChain(3,0,1)==1);
    assert(EspNativeGameplayBarrelRadius_previewWordsForChain(3,0,0)==3);
    assert(EspNativeGameplayBarrelRadius_previewWordsForChain(3,1,0)==1);
    assert(EspNativeGameplayBarrelRadius_previewWordsForChain(3,1,1)==1);
    assert(EspNativeGameplayBarrelRadius_previewWordsForChain(1,0,1)==1);
    assert(EspNativeGameplayBarrelRadius_terminalDeathOwned(1U));
    assert(!EspNativeGameplayBarrelRadius_terminalDeathOwned(0U));
    assert(EspNativeGameplayBarrelRadius_terminalDeathOwned(2U));
    assert(EspNativeGameplayBarrelRadius_terminalDeathOwned(16U));
    for (unsigned count = 0; count <= 16; ++count) {
        for (unsigned cursor = 0; cursor <= 17; ++cursor) {
            assert(EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(
                       (uint8_t)count, (uint8_t)cursor) ==
                   (cursor < count));
        }
    }
    assert(!EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(3U, 255U));
    /* Original ordered radius dispatch, including all map edges and
     * non-centered current coordinates. Invalid ordinals do not publish. */
    const int dx[] = {-1,1,0,0,-1,1,1,-1};
    const int dy[] = {0,0,-1,1,1,1,-1,-1};
    for (int y=0; y<32; ++y) for (int x=0; x<32; ++x)
        for (unsigned n=0; n<8; ++n) for (int offset=0; offset<64; offset+=31) {
            uint16_t tile = 0xffffU;
            const int nx=x+dx[n], ny=y+dy[n];
            const int valid=nx>=0 && nx<32 && ny>=0 && ny<32;
            assert(EspNativeExplosion_neighborTile(x*64+offset,y*64+offset,
                                                   (uint8_t)n,&tile)==valid);
            assert(tile==(valid?(uint16_t)(ny*32+nx):0xffffU));
        }
    for (unsigned low=0; low<256; ++low) {
        uint8_t damage=EspNativeExplosion_damageFromWord(0xdeadbe00U|low);
        assert(damage==low/11U+5U);
        for (unsigned n=0;n<8;++n)
            assert(EspNativeExplosion_component(damage,(uint8_t)n)==
                   (n<4?damage:damage/2));
        assert(EspNativeExplosion_component(damage,8U)==0U);
    }
    uint16_t invalid=0xffffU;
    assert(!EspNativeExplosion_neighborTile(32,32,8U,&invalid));
    assert(invalid==0xffffU);
    assert(!EspNativeExplosion_neighborTile(32,32,0U,0));
    puts("A2-P1 blast geometry: PASS 24576 tile cases + 2048 damage components");
    puts("barrel_radius_inert_line_host: PASS inert types 0/2/9/10/14 and type12=3,4, blast RNG preview modes, lethal any-blast boundary");
    return 0;
}
