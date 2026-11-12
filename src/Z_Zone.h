#ifndef Z_ZONE_H__
#define Z_ZONE_H__

#include <stddef.h>

/* Native CYD uses the C ABI; desktop preserves the historical SDL calling convention. */
#if defined(DOOMRPG_ESP32)
#define DOOMRPG_ZONE_CALL
#else
#define DOOMRPG_ZONE_CALL SDLCALL
#endif

void  Z_Init(void);
void* DOOMRPG_ZONE_CALL Z_Malloc(size_t size);
void* DOOMRPG_ZONE_CALL Z_Calloc(size_t nmemb, size_t size);
void* DOOMRPG_ZONE_CALL Z_Realloc(void* ptr, size_t size);
void  DOOMRPG_ZONE_CALL Z_Free(void* ptr);

int   Z_FreeMemory(void);

#endif
