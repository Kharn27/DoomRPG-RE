#ifndef ESP_ASSET_PACK_IO_PROBE_H
#define ESP_ASSET_PACK_IO_PROBE_H

/* ARCH-RC2: explicit read-only instrumentation around the native PAK stats
 * reset. The backing-store owner calls these in order; neither callback may
 * own the PAK lease, mutate the cache or allocate a map-wide buffer.
 * The TRACE implementation observes/invalidates only its bounded counters.
 * Both callbacks are harmless in INFO builds.
 */
#ifdef __cplusplus
extern "C" {
#endif

void EspAssetPackIoProbe_beforeResidentStatsReset(void);
void EspAssetPackIoProbe_afterResidentStatsReset(void);

#ifdef __cplusplus
}
#endif
#endif
