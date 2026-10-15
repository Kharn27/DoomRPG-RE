#include <SDL.h>
#include "DoomRPG.h"

/*
 * Permanent classic-CYD process-lifetime RNG provider.
 * Preserve the four exact original function bodies and both hidden globals.
 * Random_t snapshots alone are insufficient: native speculative replay
 * crossing 127->0 must keep relying on EspNativeRngReplayGuard's persistent
 * reservation rather than resetting/regenerating this hidden byte stream.
 *
 * NO seeding changes, no session reset, no new entropy, no gameplay RNG reads.
 * Both original code and generator CRC/body parity are retained as contracts.
 */
static unsigned int _seed = 0;

void Random_SetSeed(unsigned int seed)
{
	_seed = seed;
}

unsigned int Random_Get(void)
{
	_seed = ((1664525 * _seed) + 1013904223);
	return (_seed ^ (_seed >> 10));
}

static int resetRand = 0;

void  Random_getRand(byte* buff, int i)
{
	byte* bf;
	unsigned int result;

	bf = buff;
	if (resetRand == 0)
	{
		resetRand = 1;
		Random_SetSeed(DoomRPG_GetTimeMS());
	}

	result = resetRand;
	while (i-- > 0)
	{
		if (result <= 0xFF) {
			result = Random_Get() | 0x80000000;
		}

		*bf++ = result;
		result >>= 8;
	}
	resetRand = result;
}

void DoomRPG_setRand(Random_t* rand)
{
	Random_getRand(rand->randTable, RANDTABLESIZE);
	rand->nextRand = 0;
}
