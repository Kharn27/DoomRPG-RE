from pathlib import Path

p = Path("ESP32/src/esp_native_plane_renderer.c")
s = p.read_text()

old = '''/* Six independent 2048-byte leases preserve the hardware-proven six-slot LRU
 * without requiring one contiguous 12288-byte heap block. This matters after
 * the resident PAK/cache owner is active: Entrance hardware reported largest8
 * below 12 KiB even though ample total 8-bit heap remained. */
static void releaseCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        free(work->cache[i].texels);
        work->cache[i].texels = NULL;
        work->cache[i].source = NULL;
        work->cache[i].lastUse = 0U;
        work->cache[i].valid = 0U;
    }
}

static int initCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return 0;
    memset(work->cache, 0, sizeof(work->cache));
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        work->cache[i].texels = (uint8_t*)malloc(PLANE_TEXTURE_BYTES);
        if (work->cache[i].texels == NULL) {
            releaseCache(work);
            return 0;
        }
    }
    return 1;
}
'''
new = '''/* Up to six independent 2048-byte leases preserve the hardware-proven LRU
 * without requiring one contiguous 12288-byte heap block. Six slots are a
 * performance target, not a rendering invariant: under later gameplay owners
 * (for example a dialog topology rollback snapshot), keep every lease already
 * obtained and degrade the cache width instead of failing the whole frame.
 * One slot is sufficient for exact sampling; fewer slots only increase PAK
 * reads/evictions. */
static void releaseCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        free(work->cache[i].texels);
        work->cache[i].texels = NULL;
        work->cache[i].source = NULL;
        work->cache[i].lastUse = 0U;
        work->cache[i].valid = 0U;
    }
}

static int initCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return 0;
    memset(work->cache, 0, sizeof(work->cache));
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        work->cache[i].texels = (uint8_t*)malloc(PLANE_TEXTURE_BYTES);
        if (work->cache[i].texels == NULL) {
            if (i == 0U) {
                printf("[NATIVEPLANE] CACHE-FAILED slots=0/%u leaseBytes=%u\\n",
                       (unsigned int)PLANE_CACHE_SLOTS,
                       (unsigned int)PLANE_TEXTURE_BYTES);
                return 0;
            }
            printf("[NATIVEPLANE] CACHE-FALLBACK slots=%u/%u leaseBytes=%u totalLeaseBytes=%u\\n",
                   (unsigned int)i,
                   (unsigned int)PLANE_CACHE_SLOTS,
                   (unsigned int)PLANE_TEXTURE_BYTES,
                   (unsigned int)(i * PLANE_TEXTURE_BYTES));
            break;
        }
    }
    return work->cache[0].texels != NULL;
}
'''
assert s.count(old) == 1, "cache init block changed"
s = s.replace(old, new, 1)

old = '''    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) return 0;
        if (slot->valid && slot->source != NULL &&
'''
new = '''    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (slot->valid && slot->source != NULL &&
'''
assert s.count(old) == 1, "cache lookup loop changed"
s = s.replace(old, new, 1)

old = '''    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (!slot->valid) {
            target = slot;
            break;
        }
'''
new = '''    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (!slot->valid) {
            target = slot;
            break;
        }
'''
assert s.count(old) == 1, "cache target loop changed"
s = s.replace(old, new, 1)

p.write_text(s)
