#ifndef DOOMRPG_PERF_MOVE_VISIBILITY_H
#define DOOMRPG_PERF_MOVE_VISIBILITY_H
#include <stdint.h>
struct EspNativeBspVisibilityState_s;
#ifndef DOOMRPG_PERF_MOVE_REUSE
#define DOOMRPG_PERF_MOVE_REUSE 1
#endif
void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y,
    uint16_t destX, uint16_t destY, int quiet);
void EspPerfMoveVisibility_observe(const struct EspNativeBspVisibilityState_s* state,
    uint32_t sampleMs);
void EspPerfMoveVisibility_noteUnsafe(void);
/* 0 failed, 1 successful and reusable, 2 successful but not reusable. */
void EspPerfMoveVisibility_end(int rendered);
int EspPerfMoveVisibility_tryReuse(int continuation, int quiet, uint16_t sprite,
    uint16_t oldX, uint16_t oldY, uint16_t newX, uint16_t newY, uint32_t nowMs);
#endif
