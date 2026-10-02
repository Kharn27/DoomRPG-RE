#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_level_up.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Level-up presentation is defined for the 160x120 logical framebuffer"
#endif

#define COLOR_BLACK      ESP_HUB_COLOR_BLACK
#define COLOR_BG         ESP_HUB_COLOR_BG
#define COLOR_PANEL      ESP_HUB_COLOR_PANEL
#define COLOR_PANEL_ALT  ESP_HUB_COLOR_PANEL_ALT
#define COLOR_STEEL_DARK ESP_HUB_COLOR_STEEL_DARK
#define COLOR_STEEL      ESP_HUB_COLOR_STEEL
#define COLOR_IVORY      ESP_HUB_COLOR_IVORY
#define COLOR_AMBER_DIM  ESP_HUB_COLOR_AMBER_DIM
#define COLOR_AMBER      ESP_HUB_COLOR_AMBER
#define COLOR_GREEN      ESP_HUB_COLOR_GREEN

static EspNativeGameplayLevelUpView levelUp;

/* Full-frame presentation owner: do not let gameplay feedback compositing
 * repaint over this modal. */
int __real_Esp32PlatformVideo_present(void);

static int framebufferReady(void) {
    return Esp32PlatformVideo_framebuffer() != NULL &&
           Esp32PlatformVideo_framebufferSizeBytes() ==
               (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                   sizeof(uint16_t);
}

static uint16_t* framebuffer(void) {
    return (uint16_t*)Esp32PlatformVideo_framebuffer();
}

static void pixel(int x, int y, uint16_t color) {
    uint16_t* fb = framebuffer();
    if (fb == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) return;
    fb[(uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x] = color;
}

static void fillRect(int left, int top, int right, int bottom, uint16_t color) {
    int x;
    int y;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right >= DOOMRPG_LOGICAL_WIDTH) right = DOOMRPG_LOGICAL_WIDTH - 1;
    if (bottom >= DOOMRPG_LOGICAL_HEIGHT) bottom = DOOMRPG_LOGICAL_HEIGHT - 1;
    if (right < left || bottom < top) return;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) pixel(x, y, color);
    }
}

static void rect(int left, int top, int right, int bottom, uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        pixel(x, top, color);
        pixel(x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        pixel(left, y, color);
        pixel(right, y, color);
    }
}

static uint32_t frameFNV(void) {
    const uint8_t* data = (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    uint32_t hash = 2166136261U;
    size_t i;
    if (data == NULL) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static void drawGainCard(int left,
                         int top,
                         const char* label,
                         uint8_t gain) {
    char value[8];
    const int right = left + 67;
    const int bottom = top + 16;
    const uint16_t accent = gain != 0U ? COLOR_GREEN : COLOR_STEEL_DARK;

    fillRect(left, top, right, bottom, COLOR_PANEL);
    rect(left, top, right, bottom, COLOR_STEEL_DARK);
    fillRect(left + 1, top + 1, left + 2, bottom - 1, accent);

    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), label, (left + right) / 2, top + 2, COLOR_STEEL);
    snprintf(value, sizeof(value), "+%u", (unsigned int)gain);
    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), value, (left + right) / 2, top + 9,
        gain != 0U ? COLOR_GREEN : COLOR_STEEL);
}

static int paint(void) {
    char levelText[24];

    if (!framebufferReady()) return 0;

    fillRect(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, COLOR_BLACK);
    fillRect(3, 3, 156, 116, COLOR_BG);
    rect(3, 3, 156, 116, COLOR_STEEL_DARK);

    fillRect(8, 7, 151, 29, COLOR_PANEL_ALT);
    fillRect(8, 7, 10, 29, COLOR_AMBER);
    fillRect(14, 27, 145, 28, COLOR_AMBER_DIM);

    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), "LEVEL UP", 80, 9, COLOR_AMBER);
    snprintf(levelText, sizeof(levelText), "LEVEL %u > %u",
             (unsigned int)levelUp.levelBefore,
             (unsigned int)levelUp.levelAfter);
    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), levelText, 80, 19, COLOR_IVORY);

    drawGainCard(10, 34, "MAX HP", levelUp.maxHealthGain);
    drawGainCard(82, 34, "MAX ARM", levelUp.maxArmorGain);
    drawGainCard(10, 52, "DEFENSE", levelUp.defenseGain);
    drawGainCard(82, 52, "STRENGTH", levelUp.strengthGain);
    drawGainCard(10, 70, "AGILITY", levelUp.agilityGain);
    drawGainCard(82, 70, "ACCURACY", levelUp.accuracyGain);

    fillRect(20, 91, 139, 102, COLOR_PANEL_ALT);
    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), "HEALTH RESTORED", 80, 94, COLOR_GREEN);
    EspNativeGameplayHubTouchUi_drawCrispText(
        framebuffer(), "TAP TO CONTINUE", 80, 108, COLOR_IVORY);

    return __real_Esp32PlatformVideo_present();
}

void EspNativeGameplayLevelUp_reset(void) {
    memset(&levelUp, 0, sizeof(levelUp));
}

int EspNativeGameplayLevelUp_isActive(void) {
    return levelUp.active != 0U;
}

const EspNativeGameplayLevelUpView* EspNativeGameplayLevelUp_view(void) {
    return levelUp.active != 0U ? &levelUp : NULL;
}

int EspNativeGameplayLevelUp_begin(
    const EspNativeGameplayPlayerXpResult* xp,
    uint32_t sequence) {
    if (xp == NULL || xp->levelUps == 0U || xp->levelAfter <= xp->levelBefore ||
        levelUp.active != 0U || !framebufferReady()) {
        return 0;
    }

    memset(&levelUp, 0, sizeof(levelUp));
    levelUp.sequence = sequence;
    levelUp.levelBefore = xp->levelBefore;
    levelUp.levelAfter = xp->levelAfter;
    levelUp.levelUps = xp->levelUps;
    levelUp.maxHealthGain = xp->lastMaxHealthGain;
    levelUp.maxArmorGain = xp->lastMaxArmorGain;
    levelUp.defenseGain = xp->lastDefenseGain;
    levelUp.strengthGain = xp->lastStrengthGain;
    levelUp.agilityGain = xp->lastAgilityGain;
    levelUp.accuracyGain = xp->lastAccuracyGain;
    levelUp.active = 1U;

    /* Do not let the attack SELECT press (or a noisy release tail) become the
     * dismissal press for the screen that it just opened. */
    PlatformInput_requireFreshTapAfterRelease();

    if (!paint()) {
        memset(&levelUp, 0, sizeof(levelUp));
        return 0;
    }
    levelUp.frameFNV1a = frameFNV();

    printf("[LEVELUP] PRESENT seq=%u level=%u->%u levelUps=%u gains=hp+%u/armor+%u/def+%u/str+%u/agi+%u/acc+%u health=restored frame=%08x input=fresh-release+bottom-cta owner=dedicated-fullscreen timer=none\n",
           (unsigned int)levelUp.sequence,
           (unsigned int)levelUp.levelBefore,
           (unsigned int)levelUp.levelAfter,
           (unsigned int)levelUp.levelUps,
           (unsigned int)levelUp.maxHealthGain,
           (unsigned int)levelUp.maxArmorGain,
           (unsigned int)levelUp.defenseGain,
           (unsigned int)levelUp.strengthGain,
           (unsigned int)levelUp.agilityGain,
           (unsigned int)levelUp.accuracyGain,
           (unsigned int)levelUp.frameFNV1a);
    return 1;
}

int EspNativeGameplayLevelUp_requestDismiss(int16_t logicalX,
                                            int16_t logicalY) {
    (void)logicalX;
    if (levelUp.active == 0U || levelUp.dismissPending != 0U) return 0;

    /* The dedicated screen has an explicit CTA in its bottom strip. Requiring
     * that strip prevents a lingering combat-center press from ever closing the
     * screen, even if the resistive panel briefly reports a release/repress. */
    if (logicalY < 92 || logicalY >= DOOMRPG_LOGICAL_HEIGHT) return 0;

    levelUp.dismissPending = 1U;
    return 1;
}

int EspNativeGameplayLevelUp_isDismissPending(void) {
    return levelUp.active != 0U && levelUp.dismissPending != 0U;
}

int EspNativeGameplayLevelUp_armDismissPresent(void) {
    if (levelUp.active == 0U || levelUp.dismissPending == 0U ||
        levelUp.dismissPresentArmed != 0U) {
        return 0;
    }
    levelUp.dismissPresentArmed = 1U;
    return 1;
}

int EspNativeGameplayLevelUp_filterGameplayPresent(void) {
    if (levelUp.active == 0U) return 0;
    if (levelUp.dismissPresentArmed != 0U) {
        levelUp.dismissPresentArmed = 0U;
        return 0;
    }
    return 1;
}

int EspNativeGameplayLevelUp_finishDismiss(void) {
    EspNativeGameplayLevelUpView before;
    if (levelUp.active == 0U || levelUp.dismissPending == 0U) return 0;
    before = levelUp;
    memset(&levelUp, 0, sizeof(levelUp));
    printf("[LEVELUP] CLOSE seq=%u level=%u->%u input=tap worldRedraw=complete turnAdvance=no owner=released\n",
           (unsigned int)before.sequence,
           (unsigned int)before.levelBefore,
           (unsigned int)before.levelAfter);
    return 1;
}
