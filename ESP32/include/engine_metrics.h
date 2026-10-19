#ifndef DOOMRPG_ESP32_ENGINE_METRICS_H
#define DOOMRPG_ESP32_ENGINE_METRICS_H

#include <stdint.h>

/* Historic Canvas allocation fingerprints, now owned by the native engine
 * root. These constants carry no runtime storage and are not desktop APIs. */
#define ESP32_DOOMCANVAS_DESKTOP_BYTES 3740U
#define ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES (2048U + 512U)
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_FIELD_BYTES 239U
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES 240U
#define ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES (300U + 128U)
#define ESP32_DOOMCANVAS_RETIRED_STATE_FIELD_BYTES 113U
#define ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES 116U
#define ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES (7U * sizeof(void*))
#define ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES 96U
#define ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES 56U
#define ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES 72U
#define ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES 16U
#define ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES 56U
#define ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES 28U
#define ESP32_DOOMCANVAS_COMPACT_BYTES \
    (ESP32_DOOMCANVAS_DESKTOP_BYTES - ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES)



#ifdef __cplusplus
extern "C" {
#endif

typedef struct DoomRpgEngineMetrics_s {
    uint32_t doomRpg;
    uint32_t doomCanvas;
    uint32_t render;
    uint32_t game;
    uint32_t player;
    uint32_t combat;
    uint32_t supportObjects;
    uint32_t totalInitialObjects;
} DoomRpgEngineMetrics;

typedef enum DoomRpgCoreStage_e {
    DOOMRPG_CORE_ROOT = 0,
    DOOMRPG_CORE_CANVAS,
    DOOMRPG_CORE_RENDER,
    DOOMRPG_CORE_MENU_SYSTEM,
    DOOMRPG_CORE_GAME,
    DOOMRPG_CORE_PLAYER,
    DOOMRPG_CORE_STAGE_COUNT
} DoomRpgCoreStage;

#define DOOMRPG_CORE_NO_FAILURE 0xffu

typedef struct DoomRpgCoreInitReport_s {
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBlockBefore;
    uint32_t largestBlockAfter;
    uint32_t bytesUsed;
    uint32_t stageBytes[DOOMRPG_CORE_STAGE_COUNT];
    uint16_t clipWidth;
    uint16_t clipHeight;
    uint8_t completedStages;
    uint8_t failedStage;
    uint8_t ready;
} DoomRpgCoreInitReport;

typedef struct DoomRpgLayoutReport_s {
    uint32_t heap8Before;
    uint32_t heap8After;
    uint32_t largest8Before;
    uint32_t largest8After;
    uint32_t bytesUsed;
    uint32_t renderArrayPayloadBytes;

    int16_t clipX;
    int16_t clipY;
    uint16_t clipWidth;
    uint16_t clipHeight;

    int16_t displayX;
    int16_t displayY;
    uint16_t displayWidth;
    uint16_t displayHeight;

    int16_t screenX;
    int16_t screenY;
    uint16_t screenWidth;
    uint16_t screenHeight;

    uint16_t renderWidth;
    uint16_t renderHeight;
    uint16_t statusTopBarHeight;
    uint16_t statusBarHeight;
    uint8_t ready;
} DoomRpgLayoutReport;

void DoomRPG_getEngineMetrics(DoomRpgEngineMetrics* metrics);
int DoomRPG_initEngineCore(DoomRpgCoreInitReport* report);
const char* DoomRPG_coreStageName(uint8_t stage);

int DoomRPG_startEngineLayout(DoomRpgLayoutReport* report);
uint32_t DoomRPG_getHeap8Free(void);
uint32_t DoomRPG_getLargest8BitBlock(void);

#ifdef __cplusplus
}
#endif

#endif
