#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_TRANSITION_HANDOFF_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bridge the existing WAIT_STATS pause into the first production native
 * resident-map handoff. PlatformInput calls this only when a caller requests a
 * NULL tap callback. Returning nonzero means WAIT_STATS owns that pause and a
 * one-shot stats-ack tap callback was installed instead.
 *
 * The handoff remains fail-closed: only the exact committed transition already
 * prepared by esp_native_gameplay_transition is accepted. No legacy Game,
 * Player, Render or DoomCanvas world state is mutated.
 */
int EspNativeGameplayTransitionHandoff_tryArmNullCallback(void);

/*
 * Service a showStats=0 committed transition only after the source gameplay
 * service has returned from the SELECT/door transaction that staged it.
 * This prevents the destructive resident-map reset from invalidating source
 * pointers still live on the SELECT stack.
 */
void EspNativeGameplayTransitionHandoff_service(void);

#ifdef __cplusplus
}
#endif

#endif
