#include <stdint.h>
#include <SDL.h>
#include "DoomRPG.h"

/*
 * Permanent ESP32 owner for the six linked stateless DoomRPG primitives.
 * Retain the exact original fixed-point and little-endian arithmetic/ABI.
 * No RNG, process globals, PAK leases, SDL renderer or mutable map state.
 *
 * The generator and host differential test pin these bodies to the desktop
 * specification. Do not turn this into a catch-all DoomRPG compatibility TU.
 */
_Static_assert(sizeof(fixed_t) == 4, "DoomRPG fixed ABI must be 32-bit");
_Static_assert(sizeof(int) == 4, "DoomRPG integer ABI must be 32-bit");
_Static_assert(sizeof(short) == 2, "DoomRPG short ABI must be 16-bit");

fixed_t	DoomRPG_FixedMul(fixed_t a, fixed_t b)
{
	return (fixed_t)((int64_t)a * b >> FRACBITS);
	//return ((long long)a * (long long)b) >> FRACBITS;
}

fixed_t	DoomRPG_FixedDiv(fixed_t a, fixed_t b)
{
	// CALICO: rewritten to use PC version
	return (ABS(a) >> 14) >= ABS(b) ? ((a ^ b) >> 31) ^ MAXINT :
		(fixed_t)(((int64_t)a << FRACBITS) / b);

	//if ((abs(a) >> 14) >= abs(b))
		//return (a ^ b) < 0 ? MININT : MAXINT;
	//return DoomRPG_FixedDiv2(a, b);
}

short DoomRPG_shortAtNext(byte* data, int* posData)
{
	int pos = *posData + 2;
	*posData = pos;
	return (short)DoomRPG_shortAt(data, pos - 2);
}

short DoomRPG_shortAt(byte* data, int posData)
{
	return (short)((data[posData + 1] << 8) | data[posData + 0]);
}

int DoomRPG_intAtNext(byte* data, int* posData)
{
	int pos = *posData + 4;
	*posData = pos;
	return DoomRPG_intAt(data, pos - 4);
}

int DoomRPG_intAt(byte* data, int posData)
{
	return (int)((data[posData + 3] << 24) | (data[posData + 2] << 16) | (data[posData + 1] << 8) | data[posData + 0]);
}
