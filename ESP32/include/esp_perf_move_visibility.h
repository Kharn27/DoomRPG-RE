#ifndef DOOMRPG_PERF_MOVE_VISIBILITY_H
#define DOOMRPG_PERF_MOVE_VISIBILITY_H
#include <stdint.h>
struct EspNativeBspVisibilityState_s;
#ifndef DOOMRPG_PERF_MOVE_REUSE
#define DOOMRPG_PERF_MOVE_REUSE 1
#endif
/* Host oracle retains the complete shadow experiment by default.
 * Normal release profiles explicitly select CLEAN. */
#ifndef DOOMRPG_PERF_P2_SHADOW
#define DOOMRPG_PERF_P2_SHADOW 1
#endif
/* Experimental visual scheduling only; host baseline remains exact P2-CLEAN. */
#ifndef DOOMRPG_PERF_P2_COALESCE
#define DOOMRPG_PERF_P2_COALESCE 0
#endif
#define ESP_PERF_MOVE_BLOCK_WORLD (1U << 0U)
#define ESP_PERF_MOVE_BLOCK_AUTOMAP (1U << 1U)
#define ESP_PERF_MOVE_BLOCK_HUB (1U << 2U)
#define ESP_PERF_MOVE_BLOCK_TOUCH (1U << 3U)
#define ESP_PERF_MOVE_BLOCK_MODAL (1U << 4U)
#define ESP_PERF_MOVE_BLOCK_DOOR (1U << 5U)
#define ESP_PERF_MOVE_BLOCK_ATTACK (1U << 6U)
#define ESP_PERF_MOVE_BLOCK_PAIN (1U << 7U)
#define ESP_PERF_MOVE_BLOCK_INDICATOR (1U << 8U)
#define ESP_PERF_MOVE_BLOCK_PRESENT (1U << 9U)
#define ESP_PERF_MOVE_BLOCK_PAK (1U << 10U)
#define ESP_PERF_MOVE_BLOCK_MOVE_EVENT (1U << 11U)
#define ESP_PERF_MOVE_BLOCK_FEEDBACK (1U << 12U)
#define ESP_PERF_MOVE_BLOCK_FX (1U << 13U)
#define ESP_PERF_MOVE_BLOCK_PICKUP (1U << 14U)
#define ESP_PERF_MOVE_BLOCK_AMBIENT (1U << 15U)
#define ESP_PERF_MOVE_BLOCK_COUNT 16U
#if DOOMRPG_PERF_P2_SHADOW
/* P2e shadow only: a previous movement frame is not yet an authority
 * across cooperative service boundaries. Context equality is diagnostic. */
typedef struct EspPerfMoveVisibilityContext_s {
    uint32_t arena, lineFNV, textureFNV;
    int32_t x, y, z, angle;
} EspPerfMoveVisibilityContext;
/* P2o independent semantic shadow: phase-only blockers are ignored for
 * measurement, never for permission to skip. */
void EspPerfMoveVisibility_semanticBegin(const EspPerfMoveVisibilityContext* context,
    uint32_t blockers, uint16_t sprite, uint16_t oldX, uint16_t oldY,
    uint16_t newX, uint16_t newY, uint32_t nowMs, uint32_t automap,
    uint32_t activation, uint32_t player);
int EspPerfMoveVisibility_semanticEnd(int rendered, uint32_t beforeFNV,
    uint32_t afterFNV, int sideEffects, int quietAfter,
    uint32_t playerAfter, uint32_t frameUs);
void EspPerfMoveVisibility_semanticRemember(uint32_t automap, uint32_t activation,
    uint32_t player, uint32_t frame, int valid);
void EspPerfMoveVisibility_rememberContext(const EspPerfMoveVisibilityContext* context);
int EspPerfMoveVisibility_firstShadowBegin(const EspPerfMoveVisibilityContext* context,
    uint32_t blockers, uint16_t sprite, uint16_t oldX, uint16_t oldY,
    uint16_t newX, uint16_t newY, uint32_t nowMs);
void EspPerfMoveVisibility_firstShadowEnd(int rendered, uint32_t beforeFNV,
    uint32_t afterFNV, int sideEffects, int quietAfter);
#endif
void EspPerfMoveVisibility_invalidate(void);
void EspPerfMoveVisibility_commitReuse(int quiet);
void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y,
    uint16_t destX, uint16_t destY, int quiet);
void EspPerfMoveVisibility_observe(const struct EspNativeBspVisibilityState_s* state,
    uint32_t sampleMs);
/* Absolute phase window for every drawn/culled admitted billboard,
 * including glow and CROSS copies. A phase deadline never renews on reuse. */
void EspPerfMoveVisibility_noteAmbientPhase(uint32_t info, uint8_t type,
    uint8_t subtype, uint16_t index, uint32_t sampleMs);
/* Every non-movement compositor invalidates the old scene witness. */
void EspPerfMoveVisibility_frameStarted(int preview);
/* P2p: only caller-proven phase-independent billboard near culls omit
 * a deadline. The raster subset is diagnostic only. */
void EspPerfMoveVisibility_noteAmbientNearCull(uint32_t info, uint8_t type,
    uint32_t sampleMs);
void EspPerfMoveVisibility_noteAmbientRasterPhase(uint32_t info, uint8_t type,
    uint8_t subtype, uint16_t index, uint32_t sampleMs);
void EspPerfMoveVisibility_noteUnsafe(void);
/* 0 failed, 1 successful and reusable, 2 successful but not reusable. */
void EspPerfMoveVisibility_end(int rendered);
/* P4g: measurements only, never a presentation or reuse authority. */
typedef struct EspPerfMoveFrameCost_s {
    uint32_t presented, equalFNV, rejectedEqual, quietRejectedEqual;
    uint64_t equalFrameUs, equalPresentUs;
} EspPerfMoveFrameCost;
void EspPerfMoveVisibility_recordPresented(int continuation, int presented,
    uint32_t beforeFNV, uint32_t afterFNV, int quietAfter,
    uint32_t frameUs, uint32_t presentUs);
const EspPerfMoveFrameCost* EspPerfMoveVisibility_frameCost(int continuation);
int EspPerfMoveVisibility_tryReuse(int continuation, uint32_t blockers, uint16_t sprite,
    uint16_t oldX, uint16_t oldY, uint16_t newX, uint16_t newY, uint32_t nowMs);
#endif
