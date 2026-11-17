#ifndef ESP_ASSET_PACK_IO_PROBE_H
#define ESP_ASSET_PACK_IO_PROBE_H

#include <stdint.h>

/* ARCH-RC2: explicit read-only instrumentation around the native PAK stats
 * reset. The backing-store owner calls these in order; neither callback may
 * own the PAK lease, mutate the cache or allocate a map-wide buffer.
 * The TRACE implementation observes/invalidates only its bounded counters.
 * Both callbacks are harmless in INFO builds.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* ARCH-RC4: TRACE-only logical read metering. The pair surrounds one entire
 * public readRange call (including failed and empty reads); INFO has no calls.
 * The observer may sample counters/timing but may not access destination. */
int64_t EspAssetPackIoProbe_beforeReadRange(void);
void EspAssetPackIoProbe_afterReadRange(int64_t start);

void EspAssetPackIoProbe_beforeResidentStatsReset(void);
void EspAssetPackIoProbe_afterResidentStatsReset(void);

/* ARCH-RC3: close-side TRACE flush and physical baseline invalidation happen
 * before the PAK owner checks openReady or mutates any resident state. */
void EspAssetPackIoProbe_beforeResidentEnd(void);

#ifdef __cplusplus
}
#endif
#endif
