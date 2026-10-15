#include <SDL.h>
#include "DoomRPG.h"

/*
 * Permanent classic-CYD owner for the two linked DoomRPG clock ABI entries.
 * Keep the original SDL_GetTicks() arithmetic and the zero-valued basetime
 * sentinel verbatim: it is process-lifetime, NOT map/session lifetime.
 *
 * Do not substitute a new monotonic epoch, reset on LOAD, or couple to the
 * native RNG replay guard. Generator CRC + host parity protect this boundary.
 */
static Uint32 basetime = 0;

unsigned int DoomRPG_GetTimeMS(void)
{
	Uint32 ticks = SDL_GetTicks();

	if (basetime == 0)
		basetime = ticks;

	return ticks;
}

unsigned int DoomRPG_GetUpTimeMS(void)
{
	Uint32 ticks = SDL_GetTicks();

	if (basetime == 0)
		basetime = ticks;

	return ticks - basetime;
}
