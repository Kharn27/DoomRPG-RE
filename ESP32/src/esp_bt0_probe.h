#ifndef ESP_BT0_PROBE_H
#define ESP_BT0_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* BT-0: normal esp32-cyd gameplay-only A2DP source feasibility probe.
 * No MIDI, DAC, I2S, SD or streaming is touched by this module. */
void EspBt0Probe_service(int gameplayActive);

#ifdef __cplusplus
}
#endif

#endif /* ESP_BT0_PROBE_H */
