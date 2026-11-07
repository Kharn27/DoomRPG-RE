#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_SHEET_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_SHEET_H

#include <stdint.h>

/* ParticleSystem_startup chooses gibs_24.bmp when displayRect.w >= 160.
 * ParticleSystem_render selects one of four 24px frames per row and one of
 * six negative particleSize rows. The 160x120 CYD therefore uses a
 * 96x144 indexed sheet: never a monolithic sprite-cache or desktop graph. */
#define ESP_NATIVE_GIB_SHEET_FRAME_WH 24U
#define ESP_NATIVE_GIB_SHEET_FRAMES 4U
#define ESP_NATIVE_GIB_SHEET_ROWS 6U
#define ESP_NATIVE_GIB_SHEET_WIDTH 96U
#define ESP_NATIVE_GIB_SHEET_HEIGHT 144U
#define ESP_NATIVE_GIB_FRAME_MS 175U

typedef struct EspNativeGibSheetRect_s {
    uint16_t sourceX;
    uint16_t sourceY;
    uint16_t width;
    uint16_t height;
    int16_t destinationX;
    int16_t destinationY;
    uint8_t frame;
    uint8_t variant;
} EspNativeGibSheetRect;

/* One original negative-size (gib chunk) particle from the source
 * Combat_spawnBloodParticles -> ParticleSystem_calculateParticles/render.
 * One visual PRNG is replayed from a stable seed; the particle has exactly
 * four random fields (startX,startY,velX,velY), while its variant comes
 * from the single source randNext%6 draw shared by all five chunks.
 * Geometry/velocity scales match source 160x80 render and 160x120 HUD.
 * 25ms deterministic integrations adapt the original per-present update to
 * a turn-based CYD without consuming gameplay RNG or a framebuffer. */
#define ESP_NATIVE_GIB_KINETIC_COUNT 5U
#define ESP_NATIVE_GIB_KINETIC_STEP_MS 25U
#define ESP_NATIVE_GIB_KINETIC_LIFETIME_MS 350U

typedef struct EspNativeGibKineticPoint_s {
    int16_t centerX;
    int16_t centerY;
    int16_t velocityX; /* Original source fixed point, used for frame flip. */
    uint8_t variant;
    uint8_t reserved;
} EspNativeGibKineticPoint;

int EspNativeGibSheet_sampleParticle(uint32_t visualSeed,
                                      uint8_t index,
                                      uint8_t tileDistance,
                                      uint32_t elapsedMs,
                                      EspNativeGibKineticPoint* outPoint);

/* Combat_worldDistToTileDist uses squared world distance and exactly
 * four inclusive thresholds; outside 4 tiles, the gib effect is absent. */
uint8_t EspNativeGibSheet_tileDistance(uint32_t squaredWorldDistance);

/* Exact source frame/row selection and viewport clipping, independent of
 * asset availability. VelX sign mirrors the 0..3 frame direction.
 * Returns 0 for malformed metadata, invalid row or completely offscreen. */
/* DoomRPG_createImageBerserkColor(): red+8 saturate; green/blue half.
 * The magenta transparency index must remain keyed by the caller. */
uint16_t EspNativeGibSheet_berserk565(uint16_t original);

int EspNativeGibSheet_plan(uint32_t presentMs,
                           uint8_t row,
                           int16_t centerX,
                           int16_t centerY,
                           int16_t velX,
                           uint16_t sheetWidth,
                           uint16_t sheetHeight,
                           EspNativeGibSheetRect* outRect);

#endif
