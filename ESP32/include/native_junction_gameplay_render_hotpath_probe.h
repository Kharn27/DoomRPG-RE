#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_RENDER_HOTPATH_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_GAMEPLAY_RENDER_HOTPATH_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionGameplayRenderHotpathProbe_reset(void);
int Esp32JunctionGameplayRenderHotpathProbe_isDone(void);
void Esp32JunctionGameplayRenderHotpathProbe_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
