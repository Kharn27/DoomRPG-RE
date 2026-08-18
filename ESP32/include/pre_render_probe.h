#ifndef DOOMRPG_ESP32_PRE_RENDER_PROBE_H
#define DOOMRPG_ESP32_PRE_RENDER_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cross the startup stages that sit between the validated DoomCanvas layout
 * and Render_startup().  The caller passes whether the previous layout probe
 * succeeded; this keeps the probe inert when an earlier bring-up stage failed.
 *
 * Returns non-zero only when ParticleSystem_startup(), MenuSystem_startup()
 * and EntityDef_startup() all complete.  Render_startup() is deliberately not
 * called here.
 */
int DoomRPG_probePreRenderStartup(int layoutReady);

#ifdef __cplusplus
}
#endif

#endif
