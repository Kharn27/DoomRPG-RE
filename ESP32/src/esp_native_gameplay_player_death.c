#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "doomrpg_log.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

#define DEATH_FALL_MS 750U
#define DEATH_MENU_READY_MS 3000U
#define DEATH_FRAME_MIN_MS 50U
#define DEATH_VIEWPORT_Y 20U
#define DEATH_VIEWPORT_HEIGHT 80U
#define DEATH_VIEWPORT_WIDTH 160U
#define DEATH_SCREEN_HEIGHT 120U

#define DEATH_MENU_LOAD 1U
#define DEATH_MENU_JUNCTION 2U
#define DEATH_MENU_RETRY 3U
#define DEATH_MENU_MAIN 4U

#define DEATH_MENU_LEFT 10
#define DEATH_MENU_RIGHT 149
#define DEATH_MENU_ROW0_TOP 33
#define DEATH_MENU_ROW_HEIGHT 19
#define DEATH_MENU_ROW_GAP 2

typedef struct EspNativeGameplayPlayerDeathState_s {
    uint32_t sequence;
    uint32_t startedMs;
    uint32_t lastFrameMs;
    uint16_t tileIndex;
    uint16_t framesPresented;
    int16_t lastViewZ;
    uint8_t active;
    uint8_t phase;
    uint8_t deathRand;
    uint8_t lastFade;
    uint8_t menuPainted;
    uint8_t loadAvailable;
    uint8_t pendingAction;
    uint8_t menuTaps;
} EspNativeGameplayPlayerDeathState;

static EspNativeGameplayPlayerDeathState deathState;

static int32_t legacyFallViewZ(uint32_t elapsed) {
    int64_t scaled;
    int32_t viewZ;
    if (elapsed >= DEATH_FALL_MS) elapsed = DEATH_FALL_MS - 1U;
    scaled = (((int64_t)elapsed << 16) / 192000LL);
    viewZ = 36 - (int32_t)((8192LL * scaled) >> 16);
    if (viewZ < 0) viewZ = 0;
    if (viewZ > 64) viewZ = 64;
    return viewZ;
}

static uint8_t legacyFadeCap(uint32_t fadeElapsed) {
    int64_t scaled;
    int32_t fade;
    if (fadeElapsed >= DEATH_MENU_READY_MS - DEATH_FALL_MS) return 0U;
    scaled = (((int64_t)fadeElapsed << 16) / 576000LL);
    fade = 255 - (int32_t)((65280LL * scaled) >> 16);
    if (fade < 0) fade = 0;
    if (fade > 255) fade = 255;
    return (uint8_t)fade;
}

static int fadeViewport(uint8_t fade) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    size_t expected =
        (size_t)DEATH_VIEWPORT_WIDTH * 120U * sizeof(uint16_t);
    uint32_t y;
    uint16_t rCap = (uint16_t)(fade >> 3);
    uint16_t gCap = (uint16_t)(fade >> 2);
    uint16_t bCap = (uint16_t)(fade >> 3);

    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) return 0;

    for (y = 0U; y < DEATH_VIEWPORT_HEIGHT; ++y) {
        uint16_t* row =
            framebuffer + (DEATH_VIEWPORT_Y + y) * DEATH_VIEWPORT_WIDTH;
        uint32_t x;
        for (x = 0U; x < DEATH_VIEWPORT_WIDTH; ++x) {
            uint16_t c = row[x];
            uint16_t r = (uint16_t)((c >> 11) & 0x1fU);
            uint16_t g = (uint16_t)((c >> 5) & 0x3fU);
            uint16_t b = (uint16_t)(c & 0x1fU);
            if (r > rCap) r = rCap;
            if (g > gCap) g = gCap;
            if (b > bCap) b = bCap;
            row[x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
    return 1;
}

static void fillRect(uint16_t* fb, int left, int top, int right, int bottom,
                     uint16_t color) {
    int x;
    int y;
    if (fb == NULL) return;
    for (y = top; y <= bottom; ++y) {
        if (y < 0 || y >= (int)DEATH_SCREEN_HEIGHT) continue;
        for (x = left; x <= right; ++x) {
            if (x < 0 || x >= (int)DEATH_VIEWPORT_WIDTH) continue;
            fb[y * DEATH_VIEWPORT_WIDTH + x] = color;
        }
    }
}

static void drawRect(uint16_t* fb, int left, int top, int right, int bottom,
                     uint16_t color) {
    int x;
    int y;
    if (fb == NULL) return;
    for (x = left; x <= right; ++x) {
        if (x < 0 || x >= (int)DEATH_VIEWPORT_WIDTH) continue;
        if (top >= 0 && top < (int)DEATH_SCREEN_HEIGHT)
            fb[top * DEATH_VIEWPORT_WIDTH + x] = color;
        if (bottom >= 0 && bottom < (int)DEATH_SCREEN_HEIGHT)
            fb[bottom * DEATH_VIEWPORT_WIDTH + x] = color;
    }
    for (y = top; y <= bottom; ++y) {
        if (y < 0 || y >= (int)DEATH_SCREEN_HEIGHT) continue;
        if (left >= 0 && left < (int)DEATH_VIEWPORT_WIDTH)
            fb[y * DEATH_VIEWPORT_WIDTH + left] = color;
        if (right >= 0 && right < (int)DEATH_VIEWPORT_WIDTH)
            fb[y * DEATH_VIEWPORT_WIDTH + right] = color;
    }
}

static int menuRowForY(int y) {
    int row;
    for (row = 0; row < 4; ++row) {
        int top = DEATH_MENU_ROW0_TOP +
                  row * (DEATH_MENU_ROW_HEIGHT + DEATH_MENU_ROW_GAP);
        int bottom = top + DEATH_MENU_ROW_HEIGHT - 1;
        if (y >= top && y <= bottom) return row + 1;
    }
    return 0;
}

static int paintDeathMenu(void) {
    uint16_t* fb = (uint16_t*)Esp32PlatformVideo_framebuffer();
    size_t expected =
        (size_t)DEATH_VIEWPORT_WIDTH * DEATH_SCREEN_HEIGHT * sizeof(uint16_t);
    static const char* const labels[4] = {
        "LOAD SAVED GAME", "GO TO JUNCTION", "RETRY SECTOR", "MAIN MENU"
    };
    int row;

    if (fb == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) return 0;

    fillRect(fb, 0, 0, DEATH_VIEWPORT_WIDTH - 1,
             DEATH_SCREEN_HEIGHT - 1, ESP_HUB_COLOR_BLACK);
    fillRect(fb, 3, 3, 156, 116, ESP_HUB_COLOR_BG);
    drawRect(fb, 3, 3, 156, 116, ESP_HUB_COLOR_STEEL_DARK);
    fillRect(fb, 8, 7, 151, 28, ESP_HUB_COLOR_PANEL_ALT);
    fillRect(fb, 8, 7, 10, 28, ESP_HUB_COLOR_RED);
    EspNativeGameplayHubTouchUi_drawCrispText(
        fb, "YOU DIED", 80, 10, ESP_HUB_COLOR_RED);
    EspNativeGameplayHubTouchUi_drawMiniText(
        fb, "MISSION FAILED", 80, 21, ESP_HUB_COLOR_STEEL);

    for (row = 0; row < 4; ++row) {
        int top = DEATH_MENU_ROW0_TOP +
                  row * (DEATH_MENU_ROW_HEIGHT + DEATH_MENU_ROW_GAP);
        int bottom = top + DEATH_MENU_ROW_HEIGHT - 1;
        const int enabled = row == 0 && deathState.loadAvailable != 0U;
        const char* hint = row == 0
            ? (enabled ? "TAP TO LOAD" : "NO SAVE") : "NOT AVAILABLE";
        const uint16_t accent = enabled ? ESP_HUB_COLOR_AMBER
                                       : ESP_HUB_COLOR_STEEL_DARK;

        fillRect(fb, DEATH_MENU_LEFT, top, DEATH_MENU_RIGHT, bottom,
                 enabled ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_PANEL);
        drawRect(fb, DEATH_MENU_LEFT, top, DEATH_MENU_RIGHT, bottom, accent);
        fillRect(fb, DEATH_MENU_LEFT + 3, top + 3,
                 DEATH_MENU_LEFT + 4, bottom - 3, accent);
        EspNativeGameplayHubTouchUi_drawCrispText(
            fb, labels[row], 83, top + 3,
            enabled ? ESP_HUB_COLOR_IVORY : ESP_HUB_COLOR_STEEL);
        EspNativeGameplayHubTouchUi_drawMiniText(
            fb, hint, 83, top + 12,
            enabled ? ESP_HUB_COLOR_AMBER :
            row == 0 ? ESP_HUB_COLOR_RED : ESP_HUB_COLOR_STEEL_DARK);
    }
    return Esp32PlatformVideo_present();
}

void EspNativeGameplayPlayerDeath_reset(void) {
    memset(&deathState, 0, sizeof(deathState));
}

int EspNativeGameplayPlayerDeath_isActive(void) {
    return deathState.active != 0U;
}

int EspNativeGameplayPlayerDeath_isMenuReady(void) {
    return deathState.active != 0U &&
           deathState.phase == ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_MENU_READY;
}

int EspNativeGameplayPlayerDeath_handleTap(int logicalX, int logicalY) {
    int action;
    if (!EspNativeGameplayPlayerDeath_isMenuReady() ||
        deathState.menuPainted == 0U ||
        deathState.pendingAction != 0U) {
        return 0;
    }
    if (EspNativeGameplaySave_mainSelectorActive()) {
        const int result = EspNativeGameplaySave_mainSelectorTap(logicalX, logicalY);
        if (result == -1) {
            if (!paintDeathMenu()) return 0;
            printf("[DEATHMENU] SLOT-BACK return=death-menu\n");
        } else if (result == 1) {
            deathState.pendingAction = DEATH_MENU_LOAD;
        }
        return 1;
    }
    if (logicalX < DEATH_MENU_LEFT || logicalX > DEATH_MENU_RIGHT) return 0;
    action = menuRowForY(logicalY);
    if (action == 0) return 0;
    deathState.pendingAction = (uint8_t)action;
    if (deathState.menuTaps != UINT8_MAX) ++deathState.menuTaps;
    printf("[DEATHMENU] TAP n=%u logical=%d,%d action=%u route=%s queued=yes\n",
           (unsigned int)deathState.menuTaps,
           logicalX,
           logicalY,
           (unsigned int)deathState.pendingAction,
           action == DEATH_MENU_LOAD ? "LOAD" :
           action == DEATH_MENU_JUNCTION ? "JUNCTION" :
           action == DEATH_MENU_RETRY ? "RETRY" : "MAIN");
    return 1;
}

int EspNativeGameplayPlayerDeath_arm(struct DoomRPG_s* doomRpgBase,
                                     uint32_t sequence,
                                     uint16_t tileIndex) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayPlayerState* player;
    const EspNativeGameplayHudState* hud;
    EspNativeGameplayPlayerState playerBefore;
    EspNativeGameplayHudStats hudStats;
    uint16_t weaponsBefore = 0U;
    uint8_t weaponBefore = 0U;
    uint32_t fnvBefore;
    uint32_t fnvAfter;

    if (deathState.active != 0U || doomRpg == NULL ||
        doomRpg->render == NULL || view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle || (view->viewAngle & 63) != 0 ||
        view->viewZ != 36 ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) return 0;

    player = EspNativeGameplayPlayerState_view();
    if (player == NULL || (player->param1 & 0xffU) != 0U) return 0;

    fnvBefore = EspNativeGameplayPlayerState_fingerprint();
    if (!EspNativeGameplayPlayerState_enterDeath(
            &weaponsBefore, &weaponBefore)) return 0;

    hud = EspNativeGameplayHud_view();
    memset(&hudStats, 0, sizeof(hudStats));
    if (hud == NULL || hud->active != 1U || hud->painted != 1U ||
        EspNativeGameplayHud_repaint(hud, &hudStats) !=
            ESP_NATIVE_GAMEPLAY_HUD_OK) {
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        printf("[PLAYERDEATH] ARM-DEFER seq=%u tile=%u reason=hud-repaint playerRollback=yes rngConsumed=0 active=no\n",
               (unsigned int)sequence, (unsigned int)tileIndex);
        return 0;
    }

    fnvAfter = EspNativeGameplayPlayerState_fingerprint();
    memset(&deathState, 0, sizeof(deathState));
    deathState.sequence = sequence;
    deathState.tileIndex = tileIndex;
    deathState.startedMs = DoomRPG_GetUpTimeMS();
    deathState.lastFrameMs = deathState.startedMs;
    deathState.lastViewZ = 36;
    deathState.lastFade = 255U;
    deathState.deathRand = DoomRPG_randNextByte(&doomRpg->random);
    deathState.phase = ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_FALL;
    deathState.active = 1U;

    printf("[PLAYERDEATH] ARM seq=%u tile=%u playerFNV=%08x->%08x hp=0 weapons=%04x->0000 weapon=%u->0 rngByte=%u deathSound=%u-deferred shake=350/5-deferred viewZ=36 fallMs=750 fadeMs=750..3000 menu=deferred input=blocked hudPixels=%u ownerBytes=%u\n",
           (unsigned int)sequence, (unsigned int)tileIndex,
           (unsigned int)fnvBefore, (unsigned int)fnvAfter,
           (unsigned int)weaponsBefore, (unsigned int)weaponBefore,
           (unsigned int)deathState.deathRand,
           (unsigned int)((deathState.deathRand & 1U) ? 5058U : 5059U),
           (unsigned int)hudStats.pixelsWritten,
           (unsigned int)sizeof(deathState));
    return 1;
}

int EspNativeGameplayPlayerDeath_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    uint32_t now;
    uint32_t elapsed;

    if (deathState.active == 0U) return 1;
    if (doomRpg == NULL || doomRpg->render == NULL) return 0;
    if (deathState.phase == ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_MENU_READY) {
        uint8_t action = deathState.pendingAction;
        if (action == 0U) return 1;
        deathState.pendingAction = 0U;

        if (action == DEATH_MENU_LOAD) {
            if (deathState.loadAvailable == 0U) {
                printf("[DEATHMENU] DEFER action=LOAD reason=no-readable-checkpoint mutation=no sessionReplace=no\n");
                return 1;
            }
            if (!EspNativeGameplaySave_mainSelectorActive()) {
                (void)EspNativeGameplaySave_mainSelectorBegin();
                printf("[DEATHMENU] SLOT-OPEN slots=10 choice=required\n");
                return 1;
            }
            if (!EspNativeGameplaySave_mainSelectorReady()) return 1;
            printf("[DEATHMENU] DISPATCH action=LOAD slot=confirmed sessionReplace=checkpoint-native input=blocked-until-reset\n");
            if (!EspNativeGameplaySave_loadCheckpoint()) {
                printf("[DEATHMENU] FAILED action=LOAD checkpointRestore=no state=fail-closed\n");
                return 0;
            }
            EspNativeGameplaySave_mainSelectorFinish();
            printf("[DEATHMENU] TRANSITION action=LOAD result=session-replaced ownerReset=checkpoint-load\n");
            return 1;
        }

        printf("[DEATHMENU] DEFER action=%s reason=route-backend-unowned mutation=no sessionReplace=no menu=retained\n",
               action == DEATH_MENU_JUNCTION ? "JUNCTION" :
               action == DEATH_MENU_RETRY ? "RETRY" : "MAIN");
        return 1;
    }

    now = DoomRPG_GetUpTimeMS();
    elapsed = now - deathState.startedMs;

    if (elapsed >= DEATH_MENU_READY_MS) {
        if (deathState.lastFade != 0U) {
            if (!fadeViewport(0U) || !Esp32PlatformVideo_present()) return 0;
            deathState.lastFade = 0U;
            ++deathState.framesPresented;
        }
        deathState.loadAvailable =
            EspNativeGameplaySave_hasReadableCheckpoint() ? 1U : 0U;
        deathState.phase = ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_MENU_READY;
        if (!paintDeathMenu()) return 0;
        deathState.menuPainted = 1U;
        ++deathState.framesPresented;
        printf("[PLAYERDEATH] READY seq=%u tile=%u elapsedMs=%u phase=death-menu-ready viewZ=%d fade=0 frames=%u input=death-menu load=%s routes=LOAD-live/JUNCTION+RETRY+MAIN-fail-closed menuOwner=native\n",
               (unsigned int)deathState.sequence,
               (unsigned int)deathState.tileIndex,
               (unsigned int)elapsed,
               (int)deathState.lastViewZ,
               (unsigned int)deathState.framesPresented,
               deathState.loadAvailable ? "available" : "missing");
        return 1;
    }

    if (now - deathState.lastFrameMs < DEATH_FRAME_MIN_MS) return 1;
    deathState.lastFrameMs = now;

    if (elapsed < DEATH_FALL_MS) {
        int32_t targetViewZ = legacyFallViewZ(elapsed);
        EspNativeGameplayFrameStats frame;
        view = EspPlayerView_view();
        if (view == NULL || view->viewZ != deathState.lastViewZ) return 0;
        if (targetViewZ == deathState.lastViewZ) return 1;
        if (!EspPlayerView_commitDeathViewZ(deathState.lastViewZ,
                                            targetViewZ)) return 0;
        memset(&frame, 0, sizeof(frame));
        if (!EspNativeGameplayFrame_renderTurn(
                doomRpg->render, (uint8_t)view->viewAngle, &frame)) return 0;
        deathState.lastViewZ = (int16_t)targetViewZ;
        ++deathState.framesPresented;
        DRPG_LOGD("[PLAYERDEATH] FALL seq=%u elapsedMs=%u viewZ=%d frame=%08x presented=%u\n",
                  (unsigned int)deathState.sequence, (unsigned int)elapsed,
                  (int)targetViewZ, (unsigned int)frame.frameAfterFNV,
                  (unsigned int)frame.finalPresented);
        return 1;
    }

    if (deathState.phase != ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_FADE) {
        deathState.phase = ESP_NATIVE_GAMEPLAY_PLAYER_DEATH_FADE;
        printf("[PLAYERDEATH] PHASE seq=%u elapsedMs=%u fall=complete viewZ=%d fade=begin input=blocked\n",
               (unsigned int)deathState.sequence, (unsigned int)elapsed,
               (int)deathState.lastViewZ);
    }

    {
        uint8_t fade = legacyFadeCap(elapsed - DEATH_FALL_MS);
        if (fade < deathState.lastFade) {
            if (!fadeViewport(fade) || !Esp32PlatformVideo_present()) return 0;
            deathState.lastFade = fade;
            ++deathState.framesPresented;
            DRPG_LOGT("[PLAYERDEATH] FADE seq=%u elapsedMs=%u cap=%u frames=%u\n",
                      (unsigned int)deathState.sequence,
                      (unsigned int)elapsed, (unsigned int)fade,
                      (unsigned int)deathState.framesPresented);
        }
    }
    return 1;
}
