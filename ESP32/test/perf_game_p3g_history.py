"""Exact inverse of P3g plane-row optimization for the immutable SDL-F28 blob.
Runtime parity is covered separately by perf_game_plane_row_host.py.
"""


def restore_pre_p3g_plane(value: bytes) -> bytes:
    # Restore P4f before P4e and P3g exact inverses.
    for current, prior in (
        (b"        work->stats.cacheHits += pendingHits;\n        /* P4f: every deferred hit also painted exactly one pixel. */\n        work->stats.pixelsRendered += pendingHits;\n", b"        work->stats.cacheHits += pendingHits;\n"),
        (b"            previousLogical = logicalId;\n            /* First pixel of this run: no failure remains before painting. */\n            ++work->stats.pixelsRendered;\n        }\n", b"            previousLogical = logicalId;\n        }\n"),
        (b"        paletteIndex = (uint8_t)((packed >> ((texelIndex & 1U) << 2)) & 0x0fU);\n        *pixels++ = desc->paletteRgb565[paletteIndex];\n", b"        paletteIndex = (uint8_t)((texelIndex & 1U)\n                                     ? ((packed >> 4) & 0x0fU)\n                                     : (packed & 0x0fU));\n        *pixels++ = desc->paletteRgb565[paletteIndex];\n        ++work->stats.pixelsRendered;\n"),
    ):
        assert value.count(current) == 1, "P4f historical inverse anchor"
        value = value.replace(current, prior, 1)
    # Restore P4e batching before matching the frozen P3g body.
    current = "/* P4e: pendingHits is at most the positive int row length minus one.\n * A valid previous slot implies a nonzero clock. At most one wrap is\n * possible, and the original per-pixel clock skips zero on wrap.\n * Flush before any lookup/eviction (including a failed lookup), so\n * every externally observable cache state and statistic stays exact. */\nstatic inline void flushPlaneHits(PlaneWork* work, PlaneCacheSlot* slot,\n                                  uint32_t pendingHits) {\n    if (pendingHits != 0U) {\n        const uint32_t before = work->cacheClock;\n        uint32_t after = before + pendingHits;\n        if (after < before) ++after;\n        work->cacheClock = after;\n        slot->lastUse = after;\n        work->stats.cacheHits += pendingHits;\n    }\n}\n\nstatic int spanPlane(PlaneWork* work,\n                     int x, int y,\n                     const uint8_t* planeCells,\n                     int32_t param5, int32_t param6,\n                     int32_t param7, int32_t param8,\n                     int count) {\n    uint16_t* pixels;\n    int pitch;\n    const PlaneTextureDesc* previousDesc = NULL;\n    PlaneCacheSlot* previousSlot = NULL;\n    uint8_t previousLogical = 0U;\n    uint32_t pendingHits = 0U;\n\n    if (work == NULL || work->render == NULL || planeCells == NULL ||\n        count <= 0) return 0;\n\n    pitch = work->render->pitch >> 1;\n    pixels = (uint16_t*)work->render->pixels + pitch * y + x;\n\n    while (--count >= 0) {\n        const uint32_t u5 = (uint32_t)param5;\n        const uint32_t u6 = (uint32_t)param6;\n        const uint32_t cellIndex =\n            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);\n        const uint32_t texelIndex =\n            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);\n        const uint8_t logicalId = planeCells[cellIndex];\n        const PlaneTextureDesc* desc;\n        PlaneCacheSlot* slot;\n        uint8_t packed;\n        uint8_t paletteIndex;\n\n        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U) {\n            flushPlaneHits(work, previousSlot, pendingHits);\n            return 0;\n        }\n        /* P4e: no cache operation occurs inside a same-logical run.\n         * Defer its LRU touches until the next lookup or end of row. */\n        if (previousSlot != NULL && logicalId == previousLogical) {\n            desc = previousDesc;\n            slot = previousSlot;\n            ++pendingHits;\n        } else {\n            flushPlaneHits(work, previousSlot, pendingHits);\n            pendingHits = 0U;\n            const int descIndex = findTextureIndex(work, logicalId);\n            if (descIndex < 0) return 0;\n            desc = &work->textures[descIndex];\n            if (!acquireTexture(work, desc, &slot)) return 0;\n            previousDesc = desc;\n            previousSlot = slot;\n            previousLogical = logicalId;\n        }\n\n        packed = slot->texels[texelIndex >> 1];\n        paletteIndex = (uint8_t)((texelIndex & 1U)\n                                     ? ((packed >> 4) & 0x0fU)\n                                     : (packed & 0x0fU));\n        *pixels++ = desc->paletteRgb565[paletteIndex];\n        ++work->stats.pixelsRendered;\n\n        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);\n        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);\n    }\n    flushPlaneHits(work, previousSlot, pendingHits);\n    return 1;\n}".encode("utf-8")
    prior = "static int spanPlane(PlaneWork* work,\n                     int x, int y,\n                     const uint8_t* planeCells,\n                     int32_t param5, int32_t param6,\n                     int32_t param7, int32_t param8,\n                     int count) {\n    uint16_t* pixels;\n    int pitch;\n    const PlaneTextureDesc* previousDesc = NULL;\n    PlaneCacheSlot* previousSlot = NULL;\n    uint8_t previousLogical = 0U;\n\n    if (work == NULL || work->render == NULL || planeCells == NULL ||\n        count <= 0) return 0;\n\n    pitch = work->render->pitch >> 1;\n    pixels = (uint16_t*)work->render->pixels + pitch * y + x;\n\n    while (--count >= 0) {\n        const uint32_t u5 = (uint32_t)param5;\n        const uint32_t u6 = (uint32_t)param6;\n        const uint32_t cellIndex =\n            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);\n        const uint32_t texelIndex =\n            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);\n        const uint8_t logicalId = planeCells[cellIndex];\n        const PlaneTextureDesc* desc;\n        PlaneCacheSlot* slot;\n        uint8_t packed;\n        uint8_t paletteIndex;\n\n        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U)\n            return 0;\n        /* P3g: consecutive pixels with the same logical texture cannot\n         * evict their own slot. Retain the exact per-pixel LRU clock (even\n         * wrap), hit count, and palette; no pointer survives this row. */\n        if (previousSlot != NULL && logicalId == previousLogical) {\n            desc = previousDesc;\n            slot = previousSlot;\n            ++work->cacheClock;\n            if (work->cacheClock == 0U) work->cacheClock = 1U;\n            slot->lastUse = work->cacheClock;\n            ++work->stats.cacheHits;\n        } else {\n            const int descIndex = findTextureIndex(work, logicalId);\n            if (descIndex < 0) return 0;\n            desc = &work->textures[descIndex];\n            if (!acquireTexture(work, desc, &slot)) return 0;\n            previousDesc = desc;\n            previousSlot = slot;\n            previousLogical = logicalId;\n        }\n\n        packed = slot->texels[texelIndex >> 1];\n        paletteIndex = (uint8_t)((texelIndex & 1U)\n                                     ? ((packed >> 4) & 0x0fU)\n                                     : (packed & 0x0fU));\n        *pixels++ = desc->paletteRgb565[paletteIndex];\n        ++work->stats.pixelsRendered;\n\n        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);\n        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);\n    }\n    return 1;\n}".encode("utf-8")
    assert value.count(current) == 1, "P4e historical inverse anchor"
    value = value.replace(current, prior, 1)
    edits = (
        (b"""static int acquireTexture(PlaneWork* work,
                          const PlaneTextureDesc* source,
                          PlaneCacheSlot** outSlot) {
    PlaneCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;
    uint32_t readOffset;

    if (outSlot != NULL) *outSlot = NULL;
    if (work == NULL || source == NULL || outSlot == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (slot->valid && slot->source != NULL &&
            slot->source->actualId == source->actualId &&
            slot->source->sourceTexelOffset == source->sourceTexelOffset) {
            slot->lastUse = work->cacheClock;
            ++work->stats.cacheHits;
            *outSlot = slot;
            return 1;
        }
    }

    ++work->stats.cacheMisses;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (!slot->valid) {
            target = slot;
            break;
        }
        if (slot->lastUse < oldest) {
            oldest = slot->lastUse;
            target = slot;
        }
    }
    if (target == NULL) return 0;
    if (target->valid) ++work->stats.cacheEvictions;

    readOffset = WALL_TEXEL_HEADER_BYTES + (source->sourceTexelOffset >> 1);
    if (readOffset > work->wallTexels.size ||
        PLANE_TEXTURE_BYTES > work->wallTexels.size - readOffset ||
        !EspAssetPack_readRange(&work->wallTexels, readOffset,
                                target->texels, PLANE_TEXTURE_BYTES)) return 0;

    target->source = source;
    target->lastUse = work->cacheClock;
    target->valid = 1U;
    work->stats.texelReadBytes += PLANE_TEXTURE_BYTES;
    *outSlot = target;
    return 1;
}""",
         b"""static int acquireTexture(PlaneWork* work,
                          const PlaneTextureDesc* source,
                          const uint8_t** outTexels) {
    PlaneCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;
    uint32_t readOffset;

    if (outTexels != NULL) *outTexels = NULL;
    if (work == NULL || source == NULL || outTexels == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (slot->valid && slot->source != NULL &&
            slot->source->actualId == source->actualId &&
            slot->source->sourceTexelOffset == source->sourceTexelOffset) {
            slot->lastUse = work->cacheClock;
            ++work->stats.cacheHits;
            *outTexels = slot->texels;
            return 1;
        }
    }

    ++work->stats.cacheMisses;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (!slot->valid) {
            target = slot;
            break;
        }
        if (slot->lastUse < oldest) {
            oldest = slot->lastUse;
            target = slot;
        }
    }
    if (target == NULL) return 0;
    if (target->valid) ++work->stats.cacheEvictions;

    readOffset = WALL_TEXEL_HEADER_BYTES + (source->sourceTexelOffset >> 1);
    if (readOffset > work->wallTexels.size ||
        PLANE_TEXTURE_BYTES > work->wallTexels.size - readOffset ||
        !EspAssetPack_readRange(&work->wallTexels, readOffset,
                                target->texels, PLANE_TEXTURE_BYTES)) return 0;

    target->source = source;
    target->lastUse = work->cacheClock;
    target->valid = 1U;
    work->stats.texelReadBytes += PLANE_TEXTURE_BYTES;
    *outTexels = target->texels;
    return 1;
}"""),
        (b"""static int spanPlane(PlaneWork* work,
                     int x, int y,
                     const uint8_t* planeCells,
                     int32_t param5, int32_t param6,
                     int32_t param7, int32_t param8,
                     int count) {
    uint16_t* pixels;
    int pitch;
    const PlaneTextureDesc* previousDesc = NULL;
    PlaneCacheSlot* previousSlot = NULL;
    uint8_t previousLogical = 0U;

    if (work == NULL || work->render == NULL || planeCells == NULL ||
        count <= 0) return 0;

    pitch = work->render->pitch >> 1;
    pixels = (uint16_t*)work->render->pixels + pitch * y + x;

    while (--count >= 0) {
        const uint32_t u5 = (uint32_t)param5;
        const uint32_t u6 = (uint32_t)param6;
        const uint32_t cellIndex =
            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);
        const uint32_t texelIndex =
            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);
        const uint8_t logicalId = planeCells[cellIndex];
        const PlaneTextureDesc* desc;
        PlaneCacheSlot* slot;
        uint8_t packed;
        uint8_t paletteIndex;

        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U)
            return 0;
        /* P3g: consecutive pixels with the same logical texture cannot
         * evict their own slot. Retain the exact per-pixel LRU clock (even
         * wrap), hit count, and palette; no pointer survives this row. */
        if (previousSlot != NULL && logicalId == previousLogical) {
            desc = previousDesc;
            slot = previousSlot;
            ++work->cacheClock;
            if (work->cacheClock == 0U) work->cacheClock = 1U;
            slot->lastUse = work->cacheClock;
            ++work->stats.cacheHits;
        } else {
            const int descIndex = findTextureIndex(work, logicalId);
            if (descIndex < 0) return 0;
            desc = &work->textures[descIndex];
            if (!acquireTexture(work, desc, &slot)) return 0;
            previousDesc = desc;
            previousSlot = slot;
            previousLogical = logicalId;
        }

        packed = slot->texels[texelIndex >> 1];
        paletteIndex = (uint8_t)((texelIndex & 1U)
                                     ? ((packed >> 4) & 0x0fU)
                                     : (packed & 0x0fU));
        *pixels++ = desc->paletteRgb565[paletteIndex];
        ++work->stats.pixelsRendered;

        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);
        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);
    }
    return 1;
}""",
         b"""static int spanPlane(PlaneWork* work,
                     int x, int y,
                     const uint8_t* planeCells,
                     int32_t param5, int32_t param6,
                     int32_t param7, int32_t param8,
                     int count) {
    uint16_t* pixels;
    int pitch;

    if (work == NULL || work->render == NULL || planeCells == NULL ||
        count <= 0) return 0;

    pitch = work->render->pitch >> 1;
    pixels = (uint16_t*)work->render->pixels + pitch * y + x;

    while (--count >= 0) {
        const uint32_t u5 = (uint32_t)param5;
        const uint32_t u6 = (uint32_t)param6;
        const uint32_t cellIndex =
            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);
        const uint32_t texelIndex =
            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);
        const uint8_t logicalId = planeCells[cellIndex];
        const int descIndex = findTextureIndex(work, logicalId);
        const PlaneTextureDesc* desc;
        const uint8_t* texels;
        uint8_t packed;
        uint8_t paletteIndex;

        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U ||
            descIndex < 0) return 0;
        desc = &work->textures[descIndex];
        if (!acquireTexture(work, desc, &texels)) return 0;

        packed = texels[texelIndex >> 1];
        paletteIndex = (uint8_t)((texelIndex & 1U)
                                     ? ((packed >> 4) & 0x0fU)
                                     : (packed & 0x0fU));
        *pixels++ = desc->paletteRgb565[paletteIndex];
        ++work->stats.pixelsRendered;

        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);
        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);
    }
    return 1;
}"""),
    )
    for new, old in edits:
        assert value.count(new) == 1, "P3g exact historical inverse anchor"
        value = value.replace(new, old, 1)
    return value
