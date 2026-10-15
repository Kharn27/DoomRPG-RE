#include <SDL.h>
#include "DoomRPG.h"

/*
 * Exact inherited binary config reader ABI, outside generated DoomRPG.c.
 * Preserve the old signed char return semantics, RWops ownership, and
 * SDL_SwapLE32 conversion. A NULL or short stream is NOT given new behavior.
 * These functions are still called by legacy Game_loadConfig startup.
 */
int File_readByte(SDL_RWops* rw)
{
	char bData;

	if (rw) {
		SDL_RWread(rw, &bData, sizeof(byte), 1);
	}

	return (int)bData;
}

int File_readInt(SDL_RWops* rw)
{
	int iData;

	if (rw) {
		SDL_RWread(rw, &iData, sizeof(int), 1);
	}

	return (int)SDL_SwapLE32(iData);
}
