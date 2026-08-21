#ifndef DOOMRPG_ESP32_NATIVE_MAP1_NOTEBOOK_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAP1_NOTEBOOK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32Map1NotebookProbe_reset(void);
void Esp32Map1NotebookProbe_service(struct DoomRPG_s* doomRpg);
int Esp32Map1NotebookProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
