#ifndef DOOMRPG_ESP32_PROBE_LOG_H
#define DOOMRPG_ESP32_PROBE_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

void EspProbeLog_setQuiet(int quiet);
int EspProbeLog_isQuiet(void);

#ifdef __cplusplus
}
#endif

#endif
