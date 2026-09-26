#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_FX_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_FX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adopt an already-restored checkpoint monster state without replaying death
 * presentation. Existing dead monsters are marked as historical in the bounded
 * GIB FX owner; monsters that are alive remain eligible for a future real death
 * burst. No gameplay, RNG, topology or renderer state is mutated.
 */
int EspNativeGameplayGibFx_adoptCheckpointState(void);

#ifdef __cplusplus
}
#endif

#endif
