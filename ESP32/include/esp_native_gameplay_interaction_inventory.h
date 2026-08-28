#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_INTERACTION_INVENTORY_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_INTERACTION_INVENTORY_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read-only resident-map diagnostic.  Emit one map-level inventory of every
 * bytecode family plus every event that still contains a production-deferred
 * opcode.  The inventory is keyed by immutable runtime FNV, so a future map
 * automatically receives its own report without per-NPC instrumentation.
 */
void EspNativeGameplayInteractionInventory_log(void);

#ifdef __cplusplus
}
#endif

#endif
