#ifndef DOOMRPG_ESP32_NATIVE_INTRO_CLOCK_H
#define DOOMRPG_ESP32_NATIVE_INTRO_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP32_INTRO_CLOCK_STEP_MS 50U

/* Arm the bounded ESP32-owned ST_INTRO clock after the validated t=0 frame.
 * expectedStartFNV is the exact framebuffer witness produced by INTRO1 in the
 * same handoff, avoiding a stale pixel-layout constant while retaining an exact
 * no-mutation check between first-frame presentation and clock ownership. */
int Esp32IntroClock_arm(struct DoomRPG_s* doomRpg, unsigned int expectedStartFNV);

/* Service at most one quantized intro frame from the Arduino loop. */
void Esp32IntroClock_service(void);

/* Rebase story-local epochs onto the current quantized virtual time. */
int Esp32IntroClock_rebaseTextEpoch(void);
int Esp32IntroClock_rebasePageEpochs(void);

/* Deliberately stop rendering while retaining the current intro resources/state. */
void Esp32IntroClock_park(const char* reason);

int Esp32IntroClock_isActive(void);

#ifdef __cplusplus
}
#endif

#endif
