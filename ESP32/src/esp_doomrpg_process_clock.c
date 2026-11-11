#include <stdint.h>
#include <esp_timer.h>
#include "DoomRPG.h"
#include "esp_native_process_clock.h"

/*
 * Permanent classic-CYD owner for the two linked DoomRPG clock ABI entries.
 * Preserve the original 32-bit Arduino millis() tick arithmetic and zero basetime
 * sentinel verbatim: it is process-lifetime, NOT map/session lifetime.
 *
 * Do not substitute a new monotonic epoch, reset on LOAD, or couple to the
 * native RNG replay guard. Generator CRC + host parity protect this boundary.
 */
static uint32_t basetime = 0;

/* Arduino-ESP32 millis() is esp_timer_get_time()/1000, truncated to 32 bits.
 * Do not reset this clock on map/LOAD/EXIT. */
uint32_t EspNativeProcessClock_ticks(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / 1000ULL);
}

unsigned int DoomRPG_GetTimeMS(void)
{
	uint32_t ticks = EspNativeProcessClock_ticks();

	if (basetime == 0)
		basetime = ticks;

	return ticks;
}

unsigned int DoomRPG_GetUpTimeMS(void)
{
	uint32_t ticks = EspNativeProcessClock_ticks();

	if (basetime == 0)
		basetime = ticks;

	return ticks - basetime;
}
