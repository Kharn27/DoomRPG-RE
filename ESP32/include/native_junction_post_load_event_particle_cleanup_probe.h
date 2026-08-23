#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_EVENT_PARTICLE_CLEANUP_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_EVENT_PARTICLE_CLEANUP_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadEventParticleCleanupProbe_reset(void);
int Esp32JunctionPostLoadEventParticleCleanupProbe_isDone(void);
void Esp32JunctionPostLoadEventParticleCleanupProbe_service(
    struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
