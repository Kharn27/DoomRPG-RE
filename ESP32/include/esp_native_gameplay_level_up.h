#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_LEVEL_UP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_LEVEL_UP_H

#include <stdint.h>
#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayLevelUpView_s {
    uint32_t sequence;
    uint32_t frameFNV1a;
    uint8_t levelBefore;
    uint8_t levelAfter;
    uint8_t levelUps;
    uint8_t maxHealthGain;
    uint8_t maxArmorGain;
    uint8_t defenseGain;
    uint8_t strengthGain;
    uint8_t agilityGain;
    uint8_t accuracyGain;
    uint8_t active;
    uint8_t dismissPending;
    uint8_t dismissPresentArmed;
    uint8_t reserved[3];
} EspNativeGameplayLevelUpView;

void EspNativeGameplayLevelUp_reset(void);
int EspNativeGameplayLevelUp_isActive(void);
const EspNativeGameplayLevelUpView* EspNativeGameplayLevelUp_view(void);
int EspNativeGameplayLevelUp_begin(
    const EspNativeGameplayPlayerXpResult* xp,
    uint32_t sequence);
int EspNativeGameplayLevelUp_requestDismiss(int16_t logicalX,
                                            int16_t logicalY);
int EspNativeGameplayLevelUp_isDismissPending(void);

/* Resident gameplay arms exactly one wrapped world present when closing.
 * Every other gameplay present stays physically suppressed while the full-screen
 * owner is active, including late death/gib/HITFX redraws. */
int EspNativeGameplayLevelUp_armDismissPresent(void);
int EspNativeGameplayLevelUp_filterGameplayPresent(void);

/* Rebuild and physically present the modal from compact owner state after any
 * hidden gameplay compositor touched the shared framebuffer. */
int EspNativeGameplayLevelUp_repaintOwned(void);

int EspNativeGameplayLevelUp_finishDismiss(void);

#ifdef __cplusplus
}
#endif

#endif
