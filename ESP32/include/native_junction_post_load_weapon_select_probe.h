#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_WEAPON_SELECT_PROBE_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_POST_LOAD_WEAPON_SELECT_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

void Esp32JunctionPostLoadWeaponSelectProbe_reset(void);
void Esp32JunctionPostLoadWeaponSelectProbe_service(struct DoomRPG_s* doomRpg);
int Esp32JunctionPostLoadWeaponSelectProbe_isDone(void);

#ifdef __cplusplus
}
#endif

#endif
