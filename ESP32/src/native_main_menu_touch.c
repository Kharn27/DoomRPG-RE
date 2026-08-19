#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_touch.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define MENU_TOUCH_HAND_WIDTH 13
#define MENU_TOUCH_HAND_HEIGHT 10
#define MENU_TOUCH_HAND_PIXELS (MENU_TOUCH_HAND_WIDTH * MENU_TOUCH_HAND_HEIGHT)
#define MENU_TOUCH_PATCH_BYTES \
    (DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT * MENU_TOUCH_HAND_PIXELS * 2U)
#define MENU_TOUCH_GLYPH_ADVANCE 7
#define MENU_TOUCH_HIT_PAD_X 4

static DoomRPG_t* touchDoomRpg = NULL;
static uint16_t handBackground[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT]
                              [MENU_TOUCH_HAND_PIXELS];
static int16_t handAnchorX[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t handAnchorY[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t handRectX[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t handRectY[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t hitLeft[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t hitRight[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t hitTop[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static int16_t hitBottom[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static uint32_t selectionHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static uint32_t tapCount = 0;
static uint32_t selectionCount = 0;
static uint32_t confirmCount = 0;
static uint32_t missCount = 0;
static int touchPrepared = 0;
static int touchActive = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(const Render_t* render) {
    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }

    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

static int centeredTextX(const DoomCanvas_t* doomCanvas, const char* text) {
    int length;

    if (doomCanvas == NULL || text == NULL) {
        return 0;
    }

    /* Match the original MENUTYPE_MAIN centering arithmetic exactly. */
    length = ((((int)strlen(text) << 16) >> 9) * MENU_TOUCH_GLYPH_ADVANCE) >> 8;
    return doomCanvas->SCR_CX - length;
}

static void copyFramebufferRectOut(const Render_t* render,
                                   int x,
                                   int y,
                                   uint16_t* destination) {
    int row;
    const uint8_t* framebuffer = (const uint8_t*)render->framebuffer;

    for (row = 0; row < MENU_TOUCH_HAND_HEIGHT; ++row) {
        const uint8_t* source = framebuffer +
                                ((y + row) * render->pitch) +
                                (x * (int)sizeof(uint16_t));
        memcpy(&destination[row * MENU_TOUCH_HAND_WIDTH],
               source,
               MENU_TOUCH_HAND_WIDTH * sizeof(uint16_t));
    }
}

static void copyFramebufferRectIn(Render_t* render,
                                  int x,
                                  int y,
                                  const uint16_t* sourcePixels) {
    int row;
    uint8_t* framebuffer = (uint8_t*)render->framebuffer;

    for (row = 0; row < MENU_TOUCH_HAND_HEIGHT; ++row) {
        uint8_t* destination = framebuffer +
                               ((y + row) * render->pitch) +
                               (x * (int)sizeof(uint16_t));
        memcpy(destination,
               &sourcePixels[row * MENU_TOUCH_HAND_WIDTH],
               MENU_TOUCH_HAND_WIDTH * sizeof(uint16_t));
    }
}

static int findHitItem(int logicalX, int logicalY) {
    int i;

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        if (logicalX >= hitLeft[i] && logicalX <= hitRight[i] &&
            logicalY >= hitTop[i] && logicalY <= hitBottom[i]) {
            return i;
        }
    }

    return -1;
}

static int graphicsBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

int DoomRPG_esp32MainMenuTouchPrepare(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Render_t* render;
    int i;

    touchActive = 0;
    touchPrepared = 0;
    touchDoomRpg = NULL;
    PlatformInput_setTapCallback(NULL);
    memset(selectionHashes, 0, sizeof(selectionHashes));

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MENUTOUCH] FAILED prepare graphics/core boundary unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->numItems != DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
        menuSystem->imgHand.width != MENU_TOUCH_HAND_WIDTH ||
        menuSystem->imgHand.height != MENU_TOUCH_HAND_HEIGHT ||
        doomCanvas->displayRect.w != DOOMRPG_LOGICAL_WIDTH ||
        doomCanvas->displayRect.h != DOOMRPG_LOGICAL_HEIGHT ||
        render->pitch < DOOMRPG_LOGICAL_WIDTH * (int)sizeof(uint16_t)) {
        printf("[MENUTOUCH] FAILED prepare model/layout menu=%d items=%d hand=%dx%d display=%dx%d pitch=%d\n",
               menuSystem->menu,
               menuSystem->numItems,
               menuSystem->imgHand.width,
               menuSystem->imgHand.height,
               doomCanvas->displayRect.w,
               doomCanvas->displayRect.h,
               render->pitch);
        return 0;
    }

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        const char* text = menuSystem->items[i].textField;
        const int textX = centeredTextX(doomCanvas, text);
        const int textWidth = (int)strlen(text) * MENU_TOUCH_GLYPH_ADVANCE;
        const int itemY = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                          (i * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
        int left;
        int right;

        handAnchorX[i] = (int16_t)textX;
        handAnchorY[i] = (int16_t)(itemY +
                         (DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT >> 1));
        handRectX[i] = (int16_t)(doomCanvas->displayRect.x + textX -
                                 MENU_TOUCH_HAND_WIDTH);
        handRectY[i] = (int16_t)(doomCanvas->displayRect.y +
                                 handAnchorY[i] -
                                 (MENU_TOUCH_HAND_HEIGHT >> 1));

        if (handRectX[i] < 0 || handRectY[i] < 0 ||
            handRectX[i] + MENU_TOUCH_HAND_WIDTH > DOOMRPG_LOGICAL_WIDTH ||
            handRectY[i] + MENU_TOUCH_HAND_HEIGHT > DOOMRPG_LOGICAL_HEIGHT) {
            printf("[MENUTOUCH] FAILED hand rect item=%d rect=%d,%d %dx%d\n",
                   i,
                   handRectX[i],
                   handRectY[i],
                   MENU_TOUCH_HAND_WIDTH,
                   MENU_TOUCH_HAND_HEIGHT);
            return 0;
        }

        copyFramebufferRectOut(render,
                               handRectX[i],
                               handRectY[i],
                               handBackground[i]);

        left = handRectX[i] - MENU_TOUCH_HIT_PAD_X;
        right = doomCanvas->displayRect.x + textX + textWidth +
                MENU_TOUCH_HIT_PAD_X;
        if (left < 0) {
            left = 0;
        }
        if (right >= DOOMRPG_LOGICAL_WIDTH) {
            right = DOOMRPG_LOGICAL_WIDTH - 1;
        }

        hitLeft[i] = (int16_t)left;
        hitRight[i] = (int16_t)right;
        hitTop[i] = (int16_t)(doomCanvas->displayRect.y + itemY);
        hitBottom[i] = (int16_t)(hitTop[i] +
                                 DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT - 1);

        printf("[MENUTOUCH] ZONE item=%d logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d text=\"%s\"\n",
               i,
               hitLeft[i], hitRight[i], hitTop[i], hitBottom[i],
               hitLeft[i] * DOOMRPG_INTEGER_SCALE,
               ((hitRight[i] + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               hitTop[i] * DOOMRPG_INTEGER_SCALE,
               ((hitBottom[i] + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               text);
    }

    touchDoomRpg = doomRpg;
    touchPrepared = 1;
    tapCount = 0;
    selectionCount = 0;
    confirmCount = 0;
    missCount = 0;

    printf("[MENUTOUCH] PREPARED handPatches=%uB rows=%d selectionStyle=hand-only textPosition=fixed\n",
           (unsigned int)MENU_TOUCH_PATCH_BYTES,
           DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT);
    return 1;
}

int DoomRPG_esp32MainMenuTouchActivate(struct DoomRPG_s* doomRpgBase,
                                       uint32_t initialFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    uint32_t currentHash;

    if (!touchPrepared || doomRpg == NULL || doomRpg != touchDoomRpg ||
        !graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MENUTOUCH] FAILED activate prepared=%d sameDoom=%d safe=%d\n",
               touchPrepared,
               doomRpg == touchDoomRpg,
               graphicsBoundaryIsSafe(doomRpg));
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    if (menuSystem->menu != MENU_MAIN || menuSystem->selectedIndex != 0) {
        printf("[MENUTOUCH] FAILED activate menu=%d selected=%d\n",
               menuSystem->menu,
               menuSystem->selectedIndex);
        return 0;
    }

    currentHash = framebufferHash(doomRpg->render);
    if (currentHash == 0 || currentHash != initialFramebufferFNV) {
        printf("[MENUTOUCH] FAILED activate framebuffer=%08x supplied=%08x\n",
               (unsigned int)currentHash,
               (unsigned int)initialFramebufferFNV);
        return 0;
    }

    selectionHashes[0] = currentHash;
    touchActive = 1;
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);

    printf("[MENUTOUCH] READY physical=%dx%d logical=%dx%d scale=%d selected=0 initialFNV=%08x patches=%uB releaseDebounce=50ms\n",
           DOOMRPG_PHYSICAL_WIDTH,
           DOOMRPG_PHYSICAL_HEIGHT,
           DOOMRPG_LOGICAL_WIDTH,
           DOOMRPG_LOGICAL_HEIGHT,
           DOOMRPG_INTEGER_SCALE,
           (unsigned int)currentHash,
           (unsigned int)MENU_TOUCH_PATCH_BYTES);
    printf("[MENUTOUCH] READY first tap selects; second released tap on same item emits CONFIRM with action deferred\n");
    return 1;
}

int DoomRPG_esp32MainMenuTouchIsActive(void) {
    return touchActive;
}

void DoomRPG_esp32MainMenuTouchOnTap(int16_t screenX,
                                     int16_t screenY,
                                     uint16_t pressure,
                                     uint16_t rawX,
                                     uint16_t rawY) {
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Render_t* render;
    int logicalX;
    int logicalY;
    int hit;
    int selectedBefore;
    uint32_t hashBefore;
    uint32_t hashAfter;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (!touchActive || touchDoomRpg == NULL) {
        return;
    }

    doomCanvas = touchDoomRpg->doomCanvas;
    menuSystem = touchDoomRpg->menuSystem;
    render = touchDoomRpg->render;

    if (!graphicsBoundaryIsSafe(touchDoomRpg) ||
        menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex < 0 ||
        menuSystem->selectedIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        printf("[MENUTOUCH] FAILED runtime boundary menu=%d selected=%d shapeData=%p mediaTexels=%p\n",
               menuSystem != NULL ? menuSystem->menu : -999,
               menuSystem != NULL ? menuSystem->selectedIndex : -999,
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        touchActive = 0;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    hit = findHitItem(logicalX, logicalY);
    selectedBefore = menuSystem->selectedIndex;
    hashBefore = framebufferHash(render);
    tapCount++;

    printf("[MENUTOUCH] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d hit=%d selectedBefore=%d\n",
           (unsigned int)tapCount,
           rawX, rawY, pressure,
           screenX, screenY,
           logicalX, logicalY,
           hit, selectedBefore);

    if (hit < 0) {
        missCount++;
        printf("[MENUTOUCH] MISS taps=%u misses=%u framebufferFNV=%08x\n",
               (unsigned int)tapCount,
               (unsigned int)missCount,
               (unsigned int)hashBefore);
        return;
    }

    if (hit == selectedBefore) {
        confirmCount++;
        hashAfter = framebufferHash(render);
        printf("[MENUTOUCH] CONFIRM item=%d text=\"%s\" count=%u framebufferFNV=%08x action=deferred\n",
               hit,
               menuSystem->items[hit].textField,
               (unsigned int)confirmCount,
               (unsigned int)hashAfter);
        printf("[MENUTOUCH] CONFIRM deferred intentionally: MENU_MAIN Start Game can enter the not-yet-migrated gameplay loader\n");
        if (hashAfter != hashBefore) {
            printf("[MENUTOUCH] FAILED confirm changed framebuffer before=%08x after=%08x\n",
                   (unsigned int)hashBefore,
                   (unsigned int)hashAfter);
        }
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    copyFramebufferRectIn(render,
                          handRectX[selectedBefore],
                          handRectY[selectedBefore],
                          handBackground[selectedBefore]);

    menuSystem->selectedIndex = hit;
    DoomCanvas_drawImage(doomCanvas,
                         &menuSystem->imgHand,
                         handAnchorX[hit],
                         handAnchorY[hit],
                         40);

    hashAfter = framebufferHash(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (selectionHashes[hit] == 0) {
        selectionHashes[hit] = hashAfter;
    }

    printf("[MENUTOUCH] SELECT %d->%d text=\"%s\" framebufferFNV=%08x previousKnown=%08x heap8=%u->%u largest8=%u->%u\n",
           selectedBefore,
           hit,
           menuSystem->items[hit].textField,
           (unsigned int)hashAfter,
           (unsigned int)selectionHashes[hit],
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter);

    if (hashAfter == hashBefore ||
        selectionHashes[hit] != hashAfter ||
        heapAfter != heapBefore ||
        largestAfter != largestBefore ||
        !graphicsBoundaryIsSafe(touchDoomRpg)) {
        printf("[MENUTOUCH] FAILED selection invariant hashBefore=%08x hashAfter=%08x expected=%08x heapDelta=%d largestDelta=%d\n",
               (unsigned int)hashBefore,
               (unsigned int)hashAfter,
               (unsigned int)selectionHashes[hit],
               (int)heapBefore - (int)heapAfter,
               (int)largestBefore - (int)largestAfter);
        return;
    }

    selectionCount++;
    SDL_RenderPresent(NULL);
    printf("[MENUTOUCH] READY selection=%d selections=%u confirms=%u misses=%u noSceneRerender=yes noSDRead=yes\n",
           hit,
           (unsigned int)selectionCount,
           (unsigned int)confirmCount,
           (unsigned int)missCount);
}
