#ifndef DOOMRPG_PERF_MOVE_VISIBILITY_H
#define DOOMRPG_PERF_MOVE_VISIBILITY_H
#include <stdint.h>
struct EspNativeBspVisibilityState_s;
void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y);
void EspPerfMoveVisibility_observe(const struct EspNativeBspVisibilityState_s* state);
void EspPerfMoveVisibility_end(int rendered);
#endif
