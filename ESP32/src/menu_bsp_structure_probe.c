#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"
#include "Z_Zip.h"

#include "menu_bsp_structure_probe.h"

/* Keep ESP-IDF headers after DoomRPG.h: <stdbool.h> defines false/true macros
 * that collide with the engine's legacy `typedef enum { false, true } boolean;`. */
#include <esp_heap_caps.h>

extern DoomRPG_t* doomRpg;

#define MENU_BSP_HEADER_BYTES 33U
#define NODE_SERIAL_BYTES 10U
#define LINE_SERIAL_BYTES 10U
#define SPRITE_SERIAL_BYTES 5U
#define EVENT_SERIAL_BYTES 4U
#define BYTECODE_SERIAL_BYTES 9U
#define BLOCKMAP_BYTES 256U
#define PLANE_TEXTURE_BYTES (2U * 1024U)

static int structureAttempted = 0;
static int structureReady = 0;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static const zip_entry_t* findZipEntry(const char* name) {
    int i;

    if (name == NULL || zipFile.entry == NULL) {
        return NULL;
    }

    for (i = 0; i < zipFile.entry_count; ++i) {
        const zip_entry_t* entry = &zipFile.entry[i];
        if (entry->name != NULL && SDL_strcasecmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

static int ensureBytes(uint32_t pos, uint32_t need, uint32_t size,
                       const char* section) {
    if (pos > size || need > size - pos) {
        printf("[BSPPLAN] ERROR %s overruns BSP: pos=%u need=%u size=%u\n",
               section,
               (unsigned int)pos,
               (unsigned int)need,
               (unsigned int)size);
        return 0;
    }
    return 1;
}

static int readU16(const byte* data, uint32_t size, uint32_t* pos,
                   const char* section, uint32_t* value) {
    uint32_t p;

    if (data == NULL || pos == NULL || value == NULL) {
        return 0;
    }
    p = *pos;
    if (!ensureBytes(p, 2U, size, section)) {
        return 0;
    }

    *value = (uint32_t)data[p] | ((uint32_t)data[p + 1U] << 8);
    *pos = p + 2U;
    return 1;
}

static int skipBytes(uint32_t size, uint32_t* pos, uint64_t bytes,
                     const char* section) {
    if (bytes > UINT32_MAX) {
        printf("[BSPPLAN] ERROR %s serialized size overflows 32-bit\n", section);
        return 0;
    }
    if (!ensureBytes(*pos, (uint32_t)bytes, size, section)) {
        return 0;
    }
    *pos += (uint32_t)bytes;
    return 1;
}

static uint32_t maxU32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

int DoomRPG_probeMenuBspStructure(int menuBspReady) {
    const zip_entry_t* entry;
    byte* data;
    uint32_t size;
    uint32_t pos;
    uint32_t nodes;
    uint32_t lines;
    uint32_t mapSprites;
    uint32_t runtimeSprites;
    uint32_t events;
    uint32_t byteCodes;
    uint32_t strings;
    uint32_t stringChars = 0;
    uint32_t maxStringAlloc = 0;
    uint32_t i;
    uint32_t strLen;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapResident;
    uint32_t largestResident;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint64_t nodeBytes;
    uint64_t lineBytes;
    uint64_t spriteBytes;
    uint64_t eventBytes;
    uint64_t byteCodeBytes;
    uint64_t stringPointerBytes;
    uint64_t stringCharBytes;
    uint64_t scratchTextureBytes;
    uint64_t scratchSpriteBytes;
    uint64_t structuralPayload;
    uint32_t largestAllocation;
    int payloadFits;
    int largestFits;

    if (structureAttempted) {
        return structureReady;
    }
    structureAttempted = 1;

    printf("\n=== Doom RPG menu BSP complete structure plan ===\n");

    if (!menuBspReady) {
        printf("[BSPPLAN] menu BSP header probe is not ready; skipped safely\n");
        return 0;
    }

    if (doomRpg == NULL || doomRpg->render == NULL) {
        printf("[BSPPLAN] Render object unavailable; probe refused\n");
        return 0;
    }

    entry = findZipEntry("menu.bsp");
    if (entry == NULL || entry->usize < (int)MENU_BSP_HEADER_BYTES) {
        printf("[BSPPLAN] menu.bsp missing or shorter than fixed header\n");
        return 0;
    }

    size = (uint32_t)entry->usize;
    heapBefore = heap8Free();
    largestBefore = largest8Block();

    printf("[BSPPLAN] menu.bsp c=%d u=%d starting heap8=%u largest8=%u\n",
           entry->csize, entry->usize,
           (unsigned int)heapBefore,
           (unsigned int)largestBefore);
    printf("[BSPPLAN] sizeof Node=%u Line=%u Sprite=%u ptr=%u int=%u\n",
           (unsigned int)sizeof(Node_t),
           (unsigned int)sizeof(Line_t),
           (unsigned int)sizeof(Sprite_t),
           (unsigned int)sizeof(void*),
           (unsigned int)sizeof(int));

    data = DoomRPG_fileOpenRead(doomRpg, "/menu.bsp");
    if (data == NULL) {
        printf("[BSPPLAN] ERROR unable to read menu.bsp\n");
        return 0;
    }

    heapResident = heap8Free();
    largestResident = largest8Block();
    printf("[BSPPLAN] BSP resident heap8=%u largest8=%u used=%u\n",
           (unsigned int)heapResident,
           (unsigned int)largestResident,
           (unsigned int)(heapBefore >= heapResident ? heapBefore - heapResident : 0));

    pos = MENU_BSP_HEADER_BYTES;

    if (!readU16(data, size, &pos, "nodes count", &nodes) ||
        !skipBytes(size, &pos, (uint64_t)nodes * NODE_SERIAL_BYTES, "nodes")) {
        SDL_free(data);
        return 0;
    }
    printf("[BSPPLAN] nodes=%u serialized=%uB end=%u\n",
           (unsigned int)nodes,
           (unsigned int)(nodes * NODE_SERIAL_BYTES),
           (unsigned int)pos);

    if (!readU16(data, size, &pos, "lines count", &lines) ||
        !skipBytes(size, &pos, (uint64_t)lines * LINE_SERIAL_BYTES, "lines")) {
        SDL_free(data);
        return 0;
    }
    printf("[BSPPLAN] lines=%u serialized=%uB end=%u\n",
           (unsigned int)lines,
           (unsigned int)(lines * LINE_SERIAL_BYTES),
           (unsigned int)pos);

    if (!readU16(data, size, &pos, "sprites count", &mapSprites) ||
        !skipBytes(size, &pos, (uint64_t)mapSprites * SPRITE_SERIAL_BYTES, "sprites")) {
        SDL_free(data);
        return 0;
    }
    runtimeSprites = mapSprites + MAX_CUSTOM_SPRITES + MAX_DROP_SPRITES;
    printf("[BSPPLAN] mapSprites=%u runtimeSprites=%u serialized=%uB end=%u\n",
           (unsigned int)mapSprites,
           (unsigned int)runtimeSprites,
           (unsigned int)(mapSprites * SPRITE_SERIAL_BYTES),
           (unsigned int)pos);

    if (!readU16(data, size, &pos, "events count", &events) ||
        !skipBytes(size, &pos, (uint64_t)events * EVENT_SERIAL_BYTES, "events")) {
        SDL_free(data);
        return 0;
    }
    printf("[BSPPLAN] events=%u serialized=%uB end=%u\n",
           (unsigned int)events,
           (unsigned int)(events * EVENT_SERIAL_BYTES),
           (unsigned int)pos);

    if (!readU16(data, size, &pos, "bytecodes count", &byteCodes) ||
        !skipBytes(size, &pos, (uint64_t)byteCodes * BYTECODE_SERIAL_BYTES, "bytecodes")) {
        SDL_free(data);
        return 0;
    }
    printf("[BSPPLAN] byteCodes=%u serialized=%uB end=%u\n",
           (unsigned int)byteCodes,
           (unsigned int)(byteCodes * BYTECODE_SERIAL_BYTES),
           (unsigned int)pos);

    if (!readU16(data, size, &pos, "strings count", &strings)) {
        SDL_free(data);
        return 0;
    }

    for (i = 0; i < strings; ++i) {
        if (!readU16(data, size, &pos, "string length", &strLen) ||
            !skipBytes(size, &pos, strLen, "string payload")) {
            SDL_free(data);
            return 0;
        }
        if (strLen > UINT32_MAX - stringChars - 1U) {
            SDL_free(data);
            printf("[BSPPLAN] ERROR string allocation total overflow\n");
            return 0;
        }
        stringChars += strLen + 1U;
        maxStringAlloc = maxU32(maxStringAlloc, strLen + 1U);
    }
    printf("[BSPPLAN] strings=%u runtimeChars=%uB maxStringAlloc=%uB end=%u\n",
           (unsigned int)strings,
           (unsigned int)stringChars,
           (unsigned int)maxStringAlloc,
           (unsigned int)pos);

    if (!skipBytes(size, &pos, BLOCKMAP_BYTES, "blockmap") ||
        !skipBytes(size, &pos, PLANE_TEXTURE_BYTES, "plane textures")) {
        SDL_free(data);
        return 0;
    }

    printf("[BSPPLAN] blockmap=%uB planes=%uB finalPos=%u/%u\n",
           (unsigned int)BLOCKMAP_BYTES,
           (unsigned int)PLANE_TEXTURE_BYTES,
           (unsigned int)pos,
           (unsigned int)size);

    if (pos != size) {
        SDL_free(data);
        printf("[BSPPLAN] ERROR parser did not consume exact BSP: remaining=%d\n",
               (int)size - (int)pos);
        return 0;
    }

    scratchTextureBytes = 256ULL * sizeof(int);
    scratchSpriteBytes = 1024ULL * sizeof(int);
    nodeBytes = (uint64_t)nodes * sizeof(Node_t);
    lineBytes = (uint64_t)lines * sizeof(Line_t);
    spriteBytes = (uint64_t)runtimeSprites * sizeof(Sprite_t);
    eventBytes = (uint64_t)events * sizeof(int);
    byteCodeBytes = (uint64_t)byteCodes * BYTE_CODE_MAX * sizeof(int);
    stringPointerBytes = (uint64_t)strings * sizeof(char*);
    stringCharBytes = stringChars;

    structuralPayload = scratchTextureBytes + scratchSpriteBytes +
                        nodeBytes + lineBytes + spriteBytes + eventBytes +
                        byteCodeBytes + stringPointerBytes + stringCharBytes;

    if (structuralPayload > UINT32_MAX || nodeBytes > UINT32_MAX ||
        lineBytes > UINT32_MAX || spriteBytes > UINT32_MAX ||
        eventBytes > UINT32_MAX || byteCodeBytes > UINT32_MAX ||
        stringPointerBytes > UINT32_MAX) {
        SDL_free(data);
        printf("[BSPPLAN] ERROR runtime allocation plan overflows 32-bit\n");
        return 0;
    }

    largestAllocation = (uint32_t)scratchSpriteBytes;
    largestAllocation = maxU32(largestAllocation, (uint32_t)scratchTextureBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)nodeBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)lineBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)spriteBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)eventBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)byteCodeBytes);
    largestAllocation = maxU32(largestAllocation, (uint32_t)stringPointerBytes);
    largestAllocation = maxU32(largestAllocation, maxStringAlloc);

    printf("[BSPPLAN] Runtime scratch tex=%uB sprite=%uB\n",
           (unsigned int)scratchTextureBytes,
           (unsigned int)scratchSpriteBytes);
    printf("[BSPPLAN] Runtime nodes=%uB lines=%uB sprites=%uB events=%uB\n",
           (unsigned int)nodeBytes,
           (unsigned int)lineBytes,
           (unsigned int)spriteBytes,
           (unsigned int)eventBytes);
    printf("[BSPPLAN] Runtime byteCodes=%uB stringPtrs=%uB stringChars=%uB\n",
           (unsigned int)byteCodeBytes,
           (unsigned int)stringPointerBytes,
           (unsigned int)stringCharBytes);
    printf("[BSPPLAN] Structural payload=%uB largestAlloc=%uB whileBSP heap8=%u largest8=%u\n",
           (unsigned int)structuralPayload,
           (unsigned int)largestAllocation,
           (unsigned int)heapResident,
           (unsigned int)largestResident);

    payloadFits = structuralPayload <= heapResident;
    largestFits = largestAllocation <= largestResident;
    printf("[BSPPLAN] Fit aggregate=%s contiguous=%s (allocator overhead not included)\n",
           payloadFits ? "yes" : "NO",
           largestFits ? "yes" : "NO");

    SDL_free(data);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[BSPPLAN] Released BSP heap8=%u largest8=%u deltaFromStart=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter);

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[BSPPLAN] WARNING probe changed heap topology; inspect before next stage\n");
    }

    structureReady = 1;
    printf("[BSPPLAN] READY complete menu.bsp structure parsed byte-for-byte\n");
    printf("[BSPPLAN] Render_beginLoadMapData / bitshapes / texels still NOT executed\n");

    return 1;
}
