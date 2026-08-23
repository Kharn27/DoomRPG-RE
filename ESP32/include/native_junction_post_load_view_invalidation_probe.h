#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_VIEW_INVALIDATION_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_VIEW_INVALIDATION_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadViewInvalidationProbe_reset(void);
void Esp32JunctionPostLoadViewInvalidationProbe_service(
    struct DoomRPG_s* doomRpg);
int Esp32JunctionPostLoadViewInvalidationProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
