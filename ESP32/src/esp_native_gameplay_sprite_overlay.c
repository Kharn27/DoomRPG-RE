#include <stdint.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_player_resources.h"

#define SPRITE_TYPE_ENEMY 1U
#define SPRITE_HIDDEN 0x00010000UL
#define SPRITE_FIXED_ANIM 0x80000000UL
#define SPRITE_COMBAT_DEATH_VISUAL 4U
#define SPRITE_COMBAT_PAIN_VISUAL 6U

int __real_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite);

int __wrap_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite) {
    uint8_t visual;
    uint8_t type;
    uint8_t subtype;
    uint16_t linkState;
    uint16_t linkOrder;
    uint8_t animation;

    if (!__real_EspMapRuntime_getMapSprite(index, outSprite)) return 0;
    if (outSprite == NULL) return 1;

    /* One generic map-local consumed overlay serves every player resource
     * family. The immutable BSP sprite remains untouched; renderer and future
     * callers simply observe the resource as hidden after a committed pickup. */
    if (EspNativeGameplayPlayerResources_isConsumed(index)) {
        outSprite->info |= SPRITE_HIDDEN;
    }

    if (!EspMapSpriteTopology_isReady() ||
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
