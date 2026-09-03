#include <stdint.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_player_resources.h"

#define SPRITE_TYPE_ENEMY 1U
#define SPRITE_HIDDEN 0x00010000UL
#define SPRITE_SORT_BIAS 0x01000000UL
#define SPRITE_FIXED_ANIM 0x80000000UL
#define SPRITE_COMBAT_CORPSE_VISUAL 2U
#define SPRITE_COMBAT_DEATH_VISUAL 4U
#define SPRITE_COMBAT_PAIN_VISUAL 6U

int __real_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite);

int __wrap_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite) {
    const EspNativeGameplayMonsterPositionRecord* monsterPosition;
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
    (void)linkOrder;

    /*
     * Native MonsterPosition is the mutable logical spatial owner. Once an
     * enemy has been relinked, project its committed tile-center x/y into the
     * existing immutable-sprite overlay. The BSP record itself is never changed.
     * Interpolation is deliberately separate; this milestone snaps the rendered
     * sprite to the committed logical destination.
     */
    if (type == SPRITE_TYPE_ENEMY &&
        (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U) {
        monsterPosition = EspNativeGameplayMonsterPosition_find((uint16_t)index);
        if (monsterPosition != NULL &&
            monsterPosition->tileIndex ==
                (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK)) {
            outSprite->x = monsterPosition->worldX;
            outSprite->y = monsterPosition->worldY;
        }
    }

    /* Legacy monster rendering uses explicit visual 6 for pain, visual 4 for
     * the short death pose and visual 2 for the stable corpse. The native
     * sprite renderer only advances immutable FIXED_ANIM sprites, so promote
     * exactly those recovered combat states into its fixed-offset contract.
     * Non-gib deaths also receive the legacy corpse sort bias without mutating
     * the immutable BSP sprite. */
    if (type == SPRITE_TYPE_ENEMY) {
        animation = (uint8_t)(visual & 0x0fU);
        if (animation == SPRITE_COMBAT_CORPSE_VISUAL ||
            animation == SPRITE_COMBAT_DEATH_VISUAL ||
            animation == SPRITE_COMBAT_PAIN_VISUAL) {
            outSprite->info |= SPRITE_FIXED_ANIM;
        }
        if (animation == SPRITE_COMBAT_CORPSE_VISUAL ||
            animation == SPRITE_COMBAT_DEATH_VISUAL) {
            outSprite->info |= SPRITE_SORT_BIAS;
        }
    }
    return 1;
}
