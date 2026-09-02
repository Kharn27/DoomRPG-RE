#include <stdint.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_pickup.h"

/* The pickup header renames its historical wrapper implementation into a
 * private leaf. This translation unit is now the one public --wrap owner. */
#undef __wrap_EspMapRuntime_getMapSprite

#define SPRITE_TYPE_ENEMY 1U
#define SPRITE_FIXED_ANIM 0x80000000UL
#define SPRITE_COMBAT_DEATH_VISUAL 4U
#define SPRITE_COMBAT_PAIN_VISUAL 6U

int __wrap_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite) {
    uint8_t visual;
    uint8_t type;
    uint8_t subtype;
    uint16_t linkState;
    uint16_t linkOrder;
    uint8_t animation;

    if (!EspNativeGameplayPickup_getMapSprite(index, outSprite)) return 0;
    if (outSprite == NULL || !EspMapSpriteTopology_isReady() ||
        !EspMapSpriteTopology_getVisualState(index, &visual) ||
        !EspMapSpriteTopology_getEntity(index, &type, &subtype,
                                        &linkState, &linkOrder)) {
        return 1;
    }

    (void)subtype;
    (void)linkState;
    (void)linkOrder;

    /* Native monster combat owns explicit fixed pain/death frames through the
     * topology visual overlay. The sprite renderer already knows how to add a
     * fixed animation offset, but previously only honored the immutable BSP
     * FIXED_ANIM bit. Promote just these combat states into that existing
     * renderer contract; leave ordinary enemy idle/attack animation untouched. */
    if (type == SPRITE_TYPE_ENEMY) {
        animation = (uint8_t)(visual & 0x0fU);
        if (animation == SPRITE_COMBAT_DEATH_VISUAL ||
            animation == SPRITE_COMBAT_PAIN_VISUAL) {
            outSprite->info |= SPRITE_FIXED_ANIM;
        }
    }
    return 1;
}
