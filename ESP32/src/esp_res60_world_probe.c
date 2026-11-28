#include "platform_video_config.h"
#if DOOMRPG_RES60_P3_WORLD
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_map_runtime.h"
#include "esp_res60_world_probe.h"
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
#include "esp_native_plane_renderer.h"
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
#include "esp_res60_inventory_cache.h"
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
#include "esp_res60_map_lease.h"
#endif
#if DOOMRPG_RES60_P11_PROFILE
#include "esp_res60_profile.h"
#endif
#if DOOMRPG_RES60_P7_NAV
#include "esp_res60_nav.h"
#endif
#if DOOMRPG_RES60_P8_TEXEL
#include "esp_res60_texture_probe.h"
#endif

/* Native compact immutable world records, read-only. The only per-line
 * transient is a 16-byte scalar; no pointer-heavy desktop shapes are built. */
static int readCompactLine(uint32_t index, EspRes60PreviewLine* out,
                           void* context) {
    EspMapLine source;
    const EspMapRuntimeView* runtime = (const EspMapRuntimeView*)context;
    if (runtime == NULL || out == NULL ||
        index >= runtime->lineCount ||
        !EspMapRuntime_getLine(index, &source)) return 0;
    out->x1 = source.x1;
    out->y1 = source.y1;
    out->x2 = source.x2;
    out->y2 = source.y2;
    out->texture = source.texture;
    out->flags = source.flags;
    return 1;
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((((uint16_t)(r >> 3U)) << 11U) |
                      (((uint16_t)(g >> 2U)) << 5U) |
                      (uint16_t)(b >> 3U));
}

static int renderIntroCore(uint16_t* framebuffer,
                           size_t framebufferBytes, uint8_t quarterTurns,
                           EspRes60WorldProbeReport* outReport) {
    const char* const mapName = EspMapCatalog_nameForId(ESP_MAP_ID_INTRO);
    EspBspInventory inv;
    const EspMapRuntimeView* runtime = NULL;
    EspRes60WorldProbeReport report;
    EspRes60PreviewCamera camera;
    uint32_t spawnX, spawnY;
    int32_t viewX, viewY;
    uint8_t cameraAngle;
    int loaded = 0;
    int success = 0;
#if DOOMRPG_RES60_P31_SHARED_PAK
    int planePakOwned = 0;
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    uint8_t inventoryHit = 0U;
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    uint8_t mapLeaseHit = 0U;
#endif

    memset(&inv, 0, sizeof(inv));
    memset(&report, 0, sizeof(report));
    memset(&camera, 0, sizeof(camera));
    if (outReport != NULL) memset(outReport, 0, sizeof(*outReport));
    if (framebuffer == NULL || outReport == NULL || mapName == NULL ||
        framebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        quarterTurns > 3U || EspAssetPack_isOpen() ||
#if DOOMRPG_RES60_P13_MAP_LEASE
        (EspMapRuntime_isLoaded() && !EspRes60MapLease_isReady())) {
#else
        EspMapRuntime_isLoaded()) {
#endif
        printf("[RES60P3] FAIL CLOSED stage=admission alreadyActive=%u pakBusy=%u\n",
               (unsigned int)EspMapRuntime_isLoaded(),
               (unsigned int)EspAssetPack_isOpen());
        return 0;
    }

#if DOOMRPG_RES60_P11_PROFILE
    uint32_t phaseAt=EspRes60Profile_nowUs();
#endif
    /* Original compressed/packed source is never extracted map-wide. The
     * normal BSP reader inventories the real PAK through bounded windows,
     * then native EspMapRuntime builds one compact immutable arena. */
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    if (!EspRes60InventoryCache_read(mapName, &inv, &inventoryHit)) {
        EspRes60InventoryCache_reset();
        printf("[RES60P3] FAIL CLOSED stage=inventory-cache map=%s\n", mapName);
        return 0;
    }
#else
    if (!EspBspReader_inventoryPackEntry(mapName, &inv)) {
        printf("[RES60P3] FAIL CLOSED stage=inventory map=%s\n", mapName);
        return 0;
    }
#endif
#if DOOMRPG_RES60_P11_PROFILE
    report.inventoryUs=EspRes60Profile_nowUs()-phaseAt;
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    report.inventoryCacheHit = inventoryHit;
#endif
    if (inv.sourceBytes == 0U || inv.crc32 == 0U ||
        inv.expectedCrc32 != inv.crc32 ||
        inv.spawnIndex >= 1024U ||
        (inv.spawnDirection & 63U) != 0U ||
        inv.lines == 0U || inv.lines > ESP_RES60_PREVIEW_MAX_LINES) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
#endif
        printf("[RES60P3] FAIL CLOSED stage=inventory-geometry lines=%lu spawn=%u angle=%u crc=%08lx\n",
               (unsigned long)inv.lines, (unsigned int)inv.spawnIndex,
               (unsigned int)inv.spawnDirection, (unsigned long)inv.crc32);
        return 0;
    }
    cameraAngle=(uint8_t)(inv.spawnDirection + quarterTurns*64U);
#if DOOMRPG_RES60_P7_NAV
    if (!EspRes60Nav_resolveAngle(inv.spawnDirection, quarterTurns,
                                 &cameraAngle)) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
#endif
        return 0;
    }
#else
    if (quarterTurns != 0U) return 0;
#endif
    spawnX = ((uint32_t)inv.spawnIndex & 31U) * 64U + 32U;
    spawnY = ((uint32_t)inv.spawnIndex >> 5U) * 64U + 32U;
    /* Match EspNativeFirstFrame.renderFrame() exactly for cardinal
     * angles: vx = spawnX - (16*cos)>>16,
     * vy = spawnY + (16*sin)>>16. No trig table or approximations. */
    viewX = (int32_t)spawnX;
    viewY = (int32_t)spawnY;
    switch (cameraAngle) {
        case 0U: viewX -= 16; break;
        case 64U: viewY += 16; break;
        case 128U: viewX += 16; break;
        case 192U: viewY -= 16; break;
        default: return 0;
    }
    if (viewX < 0 || viewX >= 2048 || viewY < 0 || viewY >= 2048) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
#endif
        printf("[RES60P3] FAIL CLOSED stage=legacy-camera-bounds\n");
        return 0;
    }

#if DOOMRPG_RES60_P11_PROFILE
    phaseAt=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    /* Keep ONE compact arena across rotations, not a second map copy.
     * A HIT checks the real PAK entry metadata and hashes all resident
     * immutable bytes before any pixel is drawn. */
#if DOOMRPG_RES60_P31_SHARED_PAK
    if (!EspRes60MapLease_acquireForPlanes(mapName, &inv, inventoryHit,
                                          &mapLeaseHit)) {
#else
    if (!EspRes60MapLease_acquire(mapName, &inv, inventoryHit,
                                  &mapLeaseHit)) {
#endif
        EspRes60InventoryCache_reset();
        EspRes60MapLease_reset();
        printf("[RES60P13] FAIL CLOSED stage=map-lease map=%s\n", mapName);
        return 0;
    }
    report.mapLeaseHit = mapLeaseHit;
#if DOOMRPG_RES60_P31_SHARED_PAK
    planePakOwned = mapLeaseHit != 0U;
#endif
#else
    if (!EspMapRuntime_loadPackEntry(mapName, &inv)) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#endif
        printf("[RES60P3] FAIL CLOSED stage=compact-runtime map=%s\n", mapName);
        return 0;
    }
#endif
    loaded = 1;
#if DOOMRPG_RES60_P11_PROFILE
    report.loadUs=EspRes60Profile_nowUs()-phaseAt;
#endif
    runtime = EspMapRuntime_view();
    if (runtime == NULL || runtime->lineCount != inv.lines ||
        runtime->nodeCount != inv.nodes ||
        runtime->sourceCrc32 != inv.crc32 ||
        runtime->arenaBytes != inv.plan.persistentBytes ||
        runtime->arenaFNV1a == 0U) {
        printf("[RES60P3] FAIL CLOSED stage=runtime-parity\n");
        goto done;
    }
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    /* Entry CRC/size was checked by EspMapRuntime_loadPackEntry above;
     * reject same-CRC but changed arena bytes before drawing on a HIT. */
    if (inventoryHit &&
        !EspRes60InventoryCache_verify(mapName, &inv,
                                       runtime->arenaFNV1a)) {
        printf("[RES60P12] FAIL CLOSED stage=cached-arena-fnv\n");
        success = 0;
        goto done;
    }
#endif

    camera.worldX = (uint16_t)viewX;
    camera.worldY = (uint16_t)viewY;
    camera.angle = cameraAngle;
    camera.viewZ = 36U;
    camera.floorRgb565 = rgb565(inv.floorRgb[0], inv.floorRgb[1], inv.floorRgb[2]);
    camera.ceilingRgb565 = rgb565(inv.ceilingRgb[0], inv.ceilingRgb[1], inv.ceilingRgb[2]);

    report.mapBytes = inv.sourceBytes;
    report.mapCrc32 = inv.crc32;
    report.arenaBytes = runtime->arenaBytes;
    report.arenaFNV = runtime->arenaFNV1a;
    report.nodeCount = runtime->nodeCount;
    report.lineCount = runtime->lineCount;
    report.spawnIndex = inv.spawnIndex;
    report.spawnX = spawnX;
    report.spawnY = spawnY;
    report.spawnAngle = cameraAngle;
    report.cameraX = (uint32_t)viewX;
    report.cameraY = (uint32_t)viewY;
    printf("[RES60P3] MAP source=PAK map=%s bytes=%lu crc=%08lx arena=%lu arenaFNV=%08lx nodes=%lu lines=%lu spawn=%lu,%lu angle=%lu camera=%lu,%lu heapOwner=compact-only\n",
           mapName, (unsigned long)report.mapBytes,
           (unsigned long)report.mapCrc32, (unsigned long)report.arenaBytes,
           (unsigned long)report.arenaFNV, (unsigned long)report.nodeCount,
           (unsigned long)report.lineCount,
           (unsigned long)report.spawnX, (unsigned long)report.spawnY,
           (unsigned long)report.spawnAngle,
           (unsigned long)report.cameraX, (unsigned long)report.cameraY);

#if DOOMRPG_RES60_P11_PROFILE
    phaseAt=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
    /* The permanent native PAK plane owner paints the SAME framebuffer.
     * Only the diagnostic wall spans overwrite its pixels. */
#if DOOMRPG_RES60_P26_BATCH_INDEX
    const uint32_t planeOpenAt = EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P31_SHARED_PAK
    if (planePakOwned ? !EspAssetPack_isOpen() :
                        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
#else
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
#endif
        printf("[RES60P25] FAIL CLOSED stage=pak-open\n");
        goto done;
    }
#if DOOMRPG_RES60_P31_SHARED_PAK
    planePakOwned = 1;
#endif
#if DOOMRPG_RES60_P26_BATCH_INDEX
    const uint32_t planeRasterAt = EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
    size_t loanBytes=0U;
    void* loan=EspRes60Preview_borrowPlaneScratch(&loanBytes);
    if (loan!=NULL) {
        success=EspNativePlaneRenderer_renderRes60WithScratch(
            framebuffer,framebufferBytes,viewX,viewY,cameraAngle,camera.viewZ,loan,loanBytes);
        EspRes60Preview_returnPlaneScratch();
    } else {
        success=0;
    }
#else
    success = EspNativePlaneRenderer_renderRes60(
        framebuffer, framebufferBytes, viewX, viewY, cameraAngle, camera.viewZ);
#endif
#if DOOMRPG_RES60_P26_BATCH_INDEX
    const uint32_t planeCloseAt = EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P33_QUIET_INDEX
    const uint8_t indexMask = EspAssetPack_res60ValidatedIndexMask();
#endif
    EspAssetPack_close();
#if DOOMRPG_RES60_P31_SHARED_PAK
    planePakOwned = 0;
#endif
#if DOOMRPG_RES60_P26_BATCH_INDEX
    const uint32_t planeDoneAt = EspRes60Profile_nowUs();
    printf("[RES60P26] TIMING angle=%u openUs=%lu rasterUs=%lu closeUs=%lu batchEntries=16"
#if DOOMRPG_RES60_P31_SHARED_PAK
           " sharedPak=%u"
#endif
#if DOOMRPG_RES60_P33_QUIET_INDEX
           " p33Mask=%x"
#endif
           "\n", (unsigned)cameraAngle, (unsigned long)(planeRasterAt-planeOpenAt),
           (unsigned long)(planeCloseAt-planeRasterAt),
           (unsigned long)(planeDoneAt-planeCloseAt)
#if DOOMRPG_RES60_P31_SHARED_PAK
           , (unsigned)mapLeaseHit
#endif
#if DOOMRPG_RES60_P33_QUIET_INDEX
           , (unsigned)indexMask
#endif
           );
#endif
    const EspNativePlaneRenderStats* planes = EspNativePlaneRenderer_view();
    if (!success || planes == NULL || planes->rowsRendered != 96U ||
        planes->pixelsRendered != 18432U || EspAssetPack_isOpen()) {
        printf("[RES60P25] FAIL CLOSED stage=planes\n");
        success = 0;
        goto done;
    }
    printf("[RES60P25] PLANES angle=%u rows=%lu pixels=%lu textures=%u cache=%luH/%luM/%luE reads=%luB pak=CLOSED\n",
           (unsigned)cameraAngle, (unsigned long)planes->rowsRendered,
           (unsigned long)planes->pixelsRendered, (unsigned)planes->uniqueLogicalTextures,
           (unsigned long)planes->cacheHits, (unsigned long)planes->cacheMisses,
           (unsigned long)planes->cacheEvictions, (unsigned long)planes->texelReadBytes);
    success = EspRes60Preview_renderWithPlanes(framebuffer, framebufferBytes, &camera,
                                     runtime->lineCount, readCompactLine,
                                     (void*)runtime, &report.raster);
#else
    success = EspRes60Preview_render(framebuffer, framebufferBytes, &camera,
                                     runtime->lineCount, readCompactLine,
                                     (void*)runtime, &report.raster);
#endif
#if DOOMRPG_RES60_P11_PROFILE
    report.p3Us=EspRes60Profile_nowUs()-phaseAt;
#endif
    if (!success) {
        printf("[RES60P3] FAIL CLOSED stage=raycast geometry lines=%lu hitColumns=%lu reads=%lu\n",
               (unsigned long)report.lineCount,
               (unsigned long)report.raster.visibleColumns,
               (unsigned long)report.raster.readerCalls);
        goto done;
    }
#if DOOMRPG_RES60_P37_WALL_COVERAGE
    if (!EspRes60WallMaterial_measure(&report.wallDemand) ||
        report.wallDemand.pixels != report.raster.wallPixels) {
        printf("[RES60P37] FAIL CLOSED stage=wall-demand-parity\n");
        success=0;
        goto done;
    }
#endif
#if DOOMRPG_RES60_P8_TEXEL
    /* P3 raw raster fingerprint was committed BEFORE the P8 inset.
     * Catalog + PAK owners are transient, released before map + LCD present. */
#if DOOMRPG_RES60_P11_PROFILE
    phaseAt=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P40_FULL_WALLS
    {
        size_t wallLoanBytes=0U;
        void* wallLoan=EspRes60Preview_borrowPlaneScratch(&wallLoanBytes);
        if(wallLoan) {
            success=EspRes60TextureProbe_renderFullWalls(framebuffer,framebufferBytes,
                            wallLoan,wallLoanBytes,&report.texture);
            EspRes60Preview_returnPlaneScratch();
        } else success=0;
        if(!success)EspRes60TextureProbe_reset();
    }
#else
    success=EspRes60TextureProbe_render(framebuffer,framebufferBytes,
                                        &report.texture);
#if DOOMRPG_RES60_P39_WALL_STREAM
    if(success) {
        /* Raycast no longer owns its snapshot. Do not touch the retained
         * P8 swatch; loan a disjoint tile from the already allocated arena. */
        size_t wallLoanBytes=0U;
        void* wallLoan=EspRes60Preview_borrowPlaneScratch(&wallLoanBytes);
        if(wallLoan) {
            success=EspRes60TextureProbe_streamWalls(framebuffer,framebufferBytes,
                            wallLoan,wallLoanBytes,&report.texture);
            EspRes60Preview_returnPlaneScratch();
        } else success=0;
        if(!success)EspRes60TextureProbe_reset();
    }
#endif
#endif /* P40 */
#if DOOMRPG_RES60_P11_PROFILE
    report.textureUs=EspRes60Profile_nowUs()-phaseAt;
#endif
    if(!success){
        printf("[RES60P8] FAIL CLOSED stage=map-wall-texel-admission\n");
        goto done;
    }
#endif

done:
#if DOOMRPG_RES60_P31_SHARED_PAK
    if (planePakOwned && EspAssetPack_isOpen()) EspAssetPack_close();
#endif
#if DOOMRPG_RES60_P11_PROFILE
    phaseAt=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    if (loaded && !success) EspRes60MapLease_reset();
#else
    if (loaded) EspMapRuntime_reset();
#endif
#if DOOMRPG_RES60_P11_PROFILE
    report.releaseUs=EspRes60Profile_nowUs()-phaseAt;
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    if (EspAssetPack_isOpen() ||
        (success && !EspRes60MapLease_isReady()) ||
        (!success && EspMapRuntime_isLoaded())) {
#else
    if (EspMapRuntime_isLoaded() || EspAssetPack_isOpen()) {
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
#endif
        printf("[RES60P3] FAIL CLOSED stage=cleanup compactOrPakStillActive\n");
        return 0;
    }
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    if (success && !EspRes60InventoryCache_commit(mapName, &inv,
                                                   report.arenaFNV)) {
        printf("[RES60P12] FAIL CLOSED stage=cache-commit\n");
        success = 0;
    }
    if (!success) EspRes60InventoryCache_reset();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    if (!success) EspRes60MapLease_reset();
#endif
    if (success) *outReport = report;
    return success;
}

int EspRes60WorldProbe_renderIntro(uint16_t* framebuffer,
                                   size_t framebufferBytes,
                                   EspRes60WorldProbeReport* outReport) {
    return renderIntroCore(framebuffer, framebufferBytes, 0U, outReport);
}
#if DOOMRPG_RES60_P7_NAV
int EspRes60WorldProbe_renderIntroQuarterTurns(
    uint16_t* framebuffer, size_t framebufferBytes, uint8_t quarterTurns,
    EspRes60WorldProbeReport* outReport) {
    return renderIntroCore(framebuffer, framebufferBytes,
                           quarterTurns, outReport);
}
#endif

#endif /* DOOMRPG_RES60_P3_WORLD: production 50% keeps no new world code */
