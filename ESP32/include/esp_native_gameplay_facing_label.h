#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_FACING_LABEL_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_FACING_LABEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_FACING_LABEL_NAME_BYTES 17U
#define ESP_NATIVE_GAMEPLAY_FACING_LABEL_NO_INDEX 0xffffU

typedef struct EspNativeGameplayFacingLabelView_s {
    char name[ESP_NATIVE_GAMEPLAY_FACING_LABEL_NAME_BYTES];
    uint16_t spriteIndex;
    uint16_t lineIndex;
    uint16_t defTile;
    uint16_t tileIndex;
    uint8_t type;
    uint8_t subtype;
    uint8_t distance;
    uint8_t isLine;
    uint8_t active;
    uint8_t displayable;
    uint8_t dirty;
    uint8_t reserved;
} EspNativeGameplayFacingLabelView;

/*
 * Compact derived equivalent of legacy Player.facingEntity for top-bar
 * presentation. No Entity_t pointer survives: only the currently resolved
 * target and its one on-demand 16-byte EntityDef name are retained.
 */
void EspNativeGameplayFacingLabel_reset(void);
int EspNativeGameplayFacingLabel_refresh(const char* reason);
const EspNativeGameplayFacingLabelView* EspNativeGameplayFacingLabel_view(void);

/* The top-bar compositor clears dirty only after the derived fallback was
 * actually painted (or an empty fallback was explicitly restored). */
int EspNativeGameplayFacingLabel_isDirty(void);
void EspNativeGameplayFacingLabel_markPainted(void);

#ifdef __cplusplus
}
#endif

#endif
