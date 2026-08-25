#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "native_junction_sprite_census_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_FIRST_FRAME_FNV 0x8910c2edU
#define EXPECTED_CATALOG_FNV 0x969d5a77U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU
#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define VISUAL_STATE_INFO_MASK 0x0001fe00UL
#define SPRITE_FLAG_HIDDEN 0x00010000UL
#define SPRITE_FLAG_TILE 0x00040000UL
#define SPRITE_FLAG_CROSS 0x04000000UL
#define SPRITE_FLAG_SKIP_RESOURCE 0x20000000UL
#define SPRITE_FLAG_FIXED_ANIM 0x80000000UL
#define SPRITE_ORIENTATION_MASK 0x00780000UL

static struct {
    int attempted;
    int done;
} censusState;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t hashBytes(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferFNV(void) {
    const void* frame = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected =
        (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (frame == NULL || bytes != expected) return 0U;
    return hashBytes(frame, (uint32_t)bytes);
}

static int readMappedId(const EspAssetPackEntry* mappings,
                        uint32_t tableBase,
                        uint32_t count,
                        uint32_t logicalId,
                        uint16_t* outId) {
    uint8_t raw[2];
    uint32_t offset;
    if (mappings == NULL || outId == NULL || logicalId >= count) return 0;
    offset = tableBase + logicalId * 2U;
    if (offset > mappings->size || sizeof(raw) > mappings->size - offset ||
        !EspAssetPack_readRange(mappings, offset, raw, sizeof(raw))) return 0;
    *outId = readLe16(raw);
    return 1;
}

static const char* familyName(uint32_t info) {
    if ((info & SPRITE_FLAG_SKIP_RESOURCE) != 0U) return "skip";
    if ((info & SPRITE_FLAG_TILE) != 0U) return "tile";
    if ((info & SPRITE_FLAG_CROSS) != 0U) return "cross";
    if ((info & SPRITE_ORIENTATION_MASK) != 0U) return "oriented";
    return "billboard";
}

void Esp32JunctionSpriteCensusProbe_reset(void) {
    memset(&censusState, 0, sizeof(censusState));
}

int Esp32JunctionSpriteCensusProbe_isDone(void) {
    return censusState.done;
}

void Esp32JunctionSpriteCensusProbe_service(void) {
    const EspMapRuntimeView* runtime;
    const EspMapSpriteTopologyView* topology;
    const EspNativeGraphicsCatalogView* catalog;
    const EspNativeFirstFrameState* firstFrame;
    EspAssetPackEntry mappings;
    uint8_t header[MAPPINGS_HEADER_BYTES];
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t textureIdCount;
    uint32_t spriteIdCount;
    uint32_t textureIdBase;
    uint32_t spriteIdBase;
    uint64_t expectedMappingsBytes;
    uint32_t frameBefore;
    uint32_t topologyBefore;
    uint32_t catalogBefore;
    uint32_t visible = 0U;
    uint32_t hidden = 0U;
    uint32_t skip = 0U;
    uint32_t tile = 0U;
    uint32_t cross = 0U;
    uint32_t oriented = 0U;
    uint32_t billboard = 0U;
    uint32_t entities = 0U;
    uint32_t mappedHits = 0U;
    uint32_t mappedMisses = 0U;
    uint32_t rawHits = 0U;
    uint32_t identity = 0U;
    uint32_t implicitGlow = 0U;
    uint32_t implicitGlowHits = 0U;
    uint32_t i;
    int ok = 0;

    if (censusState.done || censusState.attempted) return;
    if (!EspNativeFirstFrame_isReady()) return;
    censusState.attempted = 1;

    runtime = EspMapRuntime_view();
    topology = EspMapSpriteTopology_view();
    catalog = EspNativeGraphicsCatalog_view();
    firstFrame = EspNativeFirstFrame_view();
    frameBefore = framebufferFNV();

    if (runtime == NULL || topology == NULL || catalog == NULL || firstFrame == NULL ||
        firstFrame->frameAfterFNV != EXPECTED_FIRST_FRAME_FNV ||
        frameBefore != EXPECTED_FIRST_FRAME_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        catalog->stateFNV1a != EXPECTED_CATALOG_FNV ||
        topology->spriteCount != runtime->mapSpriteCount ||
        EspAssetPack_isOpen()) {
        printf("[SPRITECENSUS] FAILED boundary frame=%08x first=%08x topology=%08x catalog=%08x sprites=%u/%u pack=%d\n",
               (unsigned int)frameBefore,
               (unsigned int)(firstFrame ? firstFrame->frameAfterFNV : 0U),
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               (unsigned int)(topology ? topology->spriteCount : 0U),
               (unsigned int)(runtime ? runtime->mapSpriteCount : 0U),
               EspAssetPack_isOpen());
        return;
    }

    topologyBefore = topology->stateFNV1a;
    catalogBefore = catalog->stateFNV1a;

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH) ||
        !EspAssetPack_findEntry("mappings.bin", &mappings) ||
        mappings.size < MAPPINGS_HEADER_BYTES ||
        !EspAssetPack_readRange(&mappings, 0U, header, sizeof(header))) {
        printf("[SPRITECENSUS] FAILED mappings open/header\n");
        goto done;
    }

    texelPairs = readLe32(header + 0U);
    bitShapePairs = readLe32(header + 4U);
    textureIdCount = readLe32(header + 8U);
    spriteIdCount = readLe32(header + 12U);
    textureIdBase = MAPPINGS_HEADER_BYTES +
                    texelPairs * MAPPING_PAIR_BYTES +
                    bitShapePairs * MAPPING_PAIR_BYTES;
    spriteIdBase = textureIdBase + textureIdCount * 2U;
    expectedMappingsBytes = (uint64_t)spriteIdBase + (uint64_t)spriteIdCount * 2U;
    if (texelPairs == 0U || bitShapePairs == 0U ||
        expectedMappingsBytes != mappings.size) {
        printf("[SPRITECENSUS] FAILED mappings layout texels=%u bitshapes=%u textures=%u sprites=%u size=%u expected=%u\n",
               (unsigned int)texelPairs, (unsigned int)bitShapePairs,
               (unsigned int)textureIdCount, (unsigned int)spriteIdCount,
               (unsigned int)mappings.size, (unsigned int)expectedMappingsBytes);
        goto done;
    }

    printf("\n=== Doom RPG ESP32-native Junction sprite census ===\n");
    printf("[SPRITECENSUS] START mapSprites=%u topology=%08x catalog=%08x frame=%08x animationTime=0\n",
           (unsigned int)runtime->mapSpriteCount,
           (unsigned int)topologyBefore,
           (unsigned int)catalogBefore,
           (unsigned int)frameBefore);

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        EspMapSprite sprite;
        uint8_t visual;
        uint8_t entityType;
        uint8_t entitySubType;
        uint16_t linkState;
        uint16_t linkOrder;
        uint32_t info;
        uint32_t logicalId;
        uint32_t lookupLogical;
        uint32_t anim;
        uint16_t mappedBase = 0xffffU;
        uint32_t mappedMedia = 0xffffffffU;
        int mappingOk = 0;
        int rawCatalogHit = 0;
        int mappedCatalogHit = 0;
        int isHidden;
        int hasEntity;
        const EspNativeGraphicsCatalogRecord* rawRecord = NULL;
        const EspNativeGraphicsCatalogRecord* mappedRecord = NULL;

        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapSpriteTopology_getVisualState(i, &visual) ||
            !EspMapSpriteTopology_getEntity(i, &entityType, &entitySubType,
                                            &linkState, &linkOrder)) {
            printf("[SPRITECENSUS] FAILED sprite access index=%u\n", (unsigned int)i);
            goto done;
        }

        info = (sprite.info & ~VISUAL_STATE_INFO_MASK) |
               ((uint32_t)visual << 9);
        logicalId = info & 511U;

        /* Legacy Render_beginLoadMapData() normalizes this BSP family into the
         * four-way cross path before render-time dispatch. Keep the immutable
         * runtime untouched and reproduce only that semantic view here. */
        if (logicalId >= 82U && logicalId <= 90U && (logicalId & 1U) == 0U) {
            info |= SPRITE_FLAG_CROSS;
        }

        anim = (info & SPRITE_FLAG_FIXED_ANIM) != 0U
                   ? ((info & 0x00001e00U) >> 9)
                   : 0U;
        isHidden = (info & SPRITE_FLAG_HIDDEN) != 0U;
        hasEntity = entityType != 0xffU;
        if (isHidden) ++hidden; else ++visible;
        if (hasEntity) ++entities;

        if ((info & SPRITE_FLAG_SKIP_RESOURCE) != 0U) {
            ++skip;
        }
        else if ((info & SPRITE_FLAG_TILE) != 0U) {
            ++tile;
            lookupLogical = logicalId;
            mappingOk = readMappedId(&mappings, textureIdBase, textureIdCount,
                                     lookupLogical, &mappedBase);
            if (mappingOk) {
                mappedMedia = (uint32_t)mappedBase + anim;
                mappedRecord = mappedMedia <= UINT16_MAX
                                   ? EspNativeGraphicsCatalog_findTexture((uint16_t)mappedMedia)
                                   : NULL;
            }
            rawRecord = logicalId <= UINT16_MAX
                            ? EspNativeGraphicsCatalog_findTexture((uint16_t)logicalId)
                            : NULL;
        }
        else {
            if ((info & SPRITE_FLAG_CROSS) != 0U) {
                ++cross;
                if (logicalId == 0U) {
                    printf("[SPRITECENSUS] FAILED cross logical=0 index=%u\n",
                           (unsigned int)i);
                    goto done;
                }
                lookupLogical = logicalId - 1U;
            }
            else {
                if ((info & SPRITE_ORIENTATION_MASK) != 0U) ++oriented;
                else ++billboard;
                lookupLogical = logicalId;
            }
            mappingOk = readMappedId(&mappings, spriteIdBase, spriteIdCount,
                                     lookupLogical, &mappedBase);
            if (mappingOk) {
                mappedMedia = (uint32_t)mappedBase + anim;
                mappedRecord = mappedMedia <= UINT16_MAX
                                   ? EspNativeGraphicsCatalog_findSprite((uint16_t)mappedMedia)
                                   : NULL;
            }
            rawRecord = logicalId <= UINT16_MAX
                            ? EspNativeGraphicsCatalog_findSprite((uint16_t)logicalId)
                            : NULL;
        }

        rawCatalogHit = rawRecord != NULL;
        mappedCatalogHit = mappedRecord != NULL;
        if (rawCatalogHit) ++rawHits;
        if (mappingOk && mappedMedia == logicalId) ++identity;
        if (mappedCatalogHit) ++mappedHits;
        else if ((info & SPRITE_FLAG_SKIP_RESOURCE) == 0U && !isHidden) ++mappedMisses;

        if (!isHidden && (info & SPRITE_FLAG_SKIP_RESOURCE) == 0U &&
            (logicalId == 135U || logicalId == 140U || logicalId == 131U)) {
            uint32_t glowLogical = logicalId == 131U ? 144U : 136U;
            uint16_t glowBase;
            ++implicitGlow;
            if (readMappedId(&mappings, spriteIdBase, spriteIdCount,
                             glowLogical, &glowBase) &&
                (uint32_t)glowBase + anim <= UINT16_MAX &&
                EspNativeGraphicsCatalog_findSprite(
                    (uint16_t)((uint32_t)glowBase + anim)) != NULL) {
                ++implicitGlowHits;
            }
        }

        printf("[SPRITECENSUS] ITEM i=%u pos=%u,%u raw=%08x info=%08x vis=%02x hidden=%d ent=%u/%u link=%04x order=%u family=%s logical=%u anim=%u map=%s%u catalogRaw=%d catalogMapped=%d\n",
               (unsigned int)i,
               (unsigned int)sprite.x, (unsigned int)sprite.y,
               (unsigned int)sprite.info, (unsigned int)info,
               (unsigned int)visual, isHidden,
               (unsigned int)entityType, (unsigned int)entitySubType,
               (unsigned int)linkState, (unsigned int)linkOrder,
               familyName(info), (unsigned int)logicalId, (unsigned int)anim,
               mappingOk ? "" : "ERR/",
               mappingOk ? (unsigned int)mappedMedia : 0U,
               rawCatalogHit, mappedCatalogHit);
    }

    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();

    if (!ok || framebufferFNV() != frameBefore ||
        !EspMapSpriteTopology_isReady() ||
        EspMapSpriteTopology_view()->stateFNV1a != topologyBefore ||
        !EspNativeGraphicsCatalog_isReady() ||
        EspNativeGraphicsCatalog_view()->stateFNV1a != catalogBefore ||
        EspAssetPack_isOpen()) {
        printf("[SPRITECENSUS] FAILED postcondition ok=%d frame=%08x/%08x topology=%08x/%08x catalog=%08x/%08x pack=%d\n",
               ok,
               (unsigned int)frameBefore, (unsigned int)framebufferFNV(),
               (unsigned int)topologyBefore,
               (unsigned int)(EspMapSpriteTopology_isReady()
                                  ? EspMapSpriteTopology_view()->stateFNV1a : 0U),
               (unsigned int)catalogBefore,
               (unsigned int)(EspNativeGraphicsCatalog_isReady()
                                  ? EspNativeGraphicsCatalog_view()->stateFNV1a : 0U),
               EspAssetPack_isOpen());
        return;
    }

    printf("[SPRITECENSUS] READY visible=%u hidden=%u entities=%u families=skip:%u tile:%u cross:%u oriented:%u billboard:%u rawCatalogHits=%u mappedHits=%u mappedMissesVisible=%u identity=%u implicitGlow=%u/%u framebuffer=%08x untouched=yes packClosed=yes\n",
           (unsigned int)visible, (unsigned int)hidden, (unsigned int)entities,
           (unsigned int)skip, (unsigned int)tile, (unsigned int)cross,
           (unsigned int)oriented, (unsigned int)billboard,
           (unsigned int)rawHits, (unsigned int)mappedHits,
           (unsigned int)mappedMisses, (unsigned int)identity,
           (unsigned int)implicitGlowHits, (unsigned int)implicitGlow,
           (unsigned int)frameBefore);
    censusState.done = 1;
}
