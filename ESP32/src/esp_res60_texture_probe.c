#include "platform_video_config.h"
#if DOOMRPG_RES60_P8_TEXEL
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if DOOMRPG_RES60_P39_WALL_STREAM
#include <stdlib.h>
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
#include <stdio.h>
#endif
#include "esp_res60_texture_probe.h"
#if DOOMRPG_RES60_P11_PROFILE
#include "esp_res60_profile.h"
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
#include "esp_res60_wall_material.h"
#endif
#include "esp_res60_layout.h"
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"

#if !DOOMRPG_RES60_P40_FULL_WALLS
/* Exactly one 2 KiB packed source tile; never a map-wide texture buffer. */
static uint8_t texelScratch[ESP_RES60_TEXTURE_BYTES];
/* One 2KiB source texture for one MAP_INTRO preview session. Rebuilding
 * mappings.bin/palettes.bin on EVERY cardinal turn cost ~1.42 seconds on CYD.
 * CRC + arena FNV + source size guard reuse inside this session; both PAK and
 * graphics catalog owners still close between frames. */
static uint16_t cachedPalette[16];
static EspRes60TextureProbeReport cachedReport;
static uint32_t cachedSourceCrc, cachedArenaFNV, cachedSourceBytes;
static uint8_t cacheValid;
#if DOOMRPG_RES60_P39_WALL_STREAM
/* Exact-size texture-only immutable lease, never sprite records or payload.
 * Native catalog count is capped at 256, so at most 10240B; Entrance 1320B. */
static EspNativeGraphicsCatalogRecord* streamRecords;
static uint16_t streamCount;
static EspAssetPackEntry streamEntry;
static void releaseWallStream(void) {
    free(streamRecords);
    streamRecords=NULL;
    streamCount=0U;
    memset(&streamEntry,0,sizeof(streamEntry));
}
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
/* Session-owned provenance only, not retained palettes or texel payload. */
static uint8_t cachedWallAdmission[ESP_RES60_WALL_ADMISSION_BYTES];
static int admitWallPlan(const uint8_t* mask, uint16_t* count) {
    uint16_t missing;
    if(!EspRes60WallMaterial_admit(mask,ESP_RES60_WALL_ADMISSION_BYTES,
                                  count,&missing)) {
        printf("[RES60P38] FAIL CLOSED wall source id=%u\n",(unsigned)missing);
        return 0;
    }
    return 1;
}
#endif

void EspRes60TextureProbe_reset(void) {
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    EspRes60WallMaterial_reset();
#endif
    cacheValid=0U;
#if DOOMRPG_RES60_P39_WALL_STREAM
    releaseWallStream();
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
    memset(cachedWallAdmission,0,sizeof(cachedWallAdmission));
#endif
    memset(texelScratch,0,sizeof(texelScratch));
    memset(cachedPalette,0,sizeof(cachedPalette));
    memset(&cachedReport,0,sizeof(cachedReport));
    cachedSourceCrc=0U;
    cachedArenaFNV=0U;
    cachedSourceBytes=0U;
}
static uint16_t sourceToFramebuffer565(uint16_t c) {
    /* Proven 160x120 wall renderer's source BGR565 -> framebuffer RGB565. */
    return (uint16_t)(((c & 0x001fU)<<11) | (c & 0x07e0U) |
                      ((c & 0xf800U)>>11));
}
static uint32_t fnvAppend(uint32_t h, uint8_t value) {
    return (h ^ value) * 16777619U;
}
static uint32_t fnvBytes(const uint8_t* data, size_t n) {
    uint32_t h=2166136261U;
    size_t i;
    for(i=0U;i<n;++i) h=fnvAppend(h,data[i]);
    return h;
}
int EspRes60TextureProbe_paint(uint16_t* fb, size_t fbBytes,
                               const uint8_t* packed, size_t packedBytes,
                               const uint16_t raw[16], uint32_t* outFNV) {
    uint32_t hash=2166136261U;
    uint32_t y,x;
    if(outFNV) *outFNV=0;
    if(!fb || !packed || !raw || !outFNV ||
       fbBytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
       packedBytes!=ESP_RES60_TEXTURE_BYTES) return 0;
    /* Source layout follows native 64x64 packed 4-bit walls exactly:
     * source idx = y*64+x, even nibble LOW, odd nibble HIGH. */
    for(y=0;y<ESP_RES60_TEXTURE_INSET_SIZE;++y){
        const uint32_t sy=y*64U/ESP_RES60_TEXTURE_INSET_SIZE;
        for(x=0;x<ESP_RES60_TEXTURE_INSET_SIZE;++x){
            const uint32_t sx=x*64U/ESP_RES60_TEXTURE_INSET_SIZE;
            const uint32_t sourceIndex=sy*64U+sx;
            const uint8_t byte=packed[sourceIndex>>1];
            const uint8_t idx=(uint8_t)((sourceIndex&1U)?
                     (byte>>4) : (byte&15U));
            const uint16_t color=sourceToFramebuffer565(raw[idx]);
            fb[((size_t)ESP_RES60_TEXTURE_INSET_Y+y)*ESP_RES60_WIDTH+
               ESP_RES60_TEXTURE_INSET_X+x]=color;
            hash=fnvAppend(hash,(uint8_t)color);
            hash=fnvAppend(hash,(uint8_t)(color>>8));
        }
    }
    *outFNV=hash;
    return hash!=0U;
}

int EspRes60TextureProbe_render(uint16_t* fb, size_t fbBytes,
                                EspRes60TextureProbeReport* out) {
    const EspNativeGraphicsCatalogView* catalog;
    const EspMapRuntimeView* activeMap;
    EspNativeGraphicsCatalogRecord selection;
    EspAssetPackEntry texels;
    uint8_t header[4];
    uint32_t expectedBytes, relative, id;
    uint32_t swatchHash=0U;
#if DOOMRPG_RES60_P38_WALL_ADMISSION
    uint8_t admission[ESP_RES60_WALL_ADMISSION_BYTES]={0};
    uint16_t admittedIds=0U;
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    uint8_t selectedFromVisible=0U;
#endif
    int open=0,success=0,found=0;
    if(out) memset(out,0,sizeof(*out));
    if(!fb || !out || fbBytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
       !EspMapRuntime_isLoaded() || EspAssetPack_isOpen() ||
       EspNativeGraphicsCatalog_isReady()) return 0;
    activeMap=EspMapRuntime_view();
    if(!activeMap || !activeMap->sourceCrc32 || !activeMap->arenaFNV1a ||
       !activeMap->sourceBytes || !activeMap->lineCount) return 0;
#if DOOMRPG_RES60_P11_PROFILE
    const uint32_t p11Start=EspRes60Profile_nowUs();
    uint32_t p11At=0U;
#endif
    if(cacheValid &&
       cachedSourceCrc==activeMap->sourceCrc32 &&
       cachedArenaFNV==activeMap->arenaFNV1a &&
       cachedSourceBytes==activeMap->sourceBytes) {
        /* The map owner is fresh each turn, but the bounded immutable swatch
         * source is not: no catalog rebuild and zero PAK reads on HIT. */
#if DOOMRPG_RES60_P38_WALL_ADMISSION
        if(!admitWallPlan(cachedWallAdmission,&admittedIds)) {
            EspRes60TextureProbe_reset();
            return 0;
        }
#endif
#if DOOMRPG_RES60_P11_PROFILE
        p11At=EspRes60Profile_nowUs();
#endif
        if(!EspRes60TextureProbe_paint(fb,fbBytes,texelScratch,
               sizeof(texelScratch),cachedPalette,&swatchHash) ||
           swatchHash!=cachedReport.sampleFNV) {
            EspRes60TextureProbe_reset();
            return 0;
        }
#if DOOMRPG_RES60_P11_PROFILE
        const uint32_t swatchUs=EspRes60Profile_nowUs()-p11At;
#endif
        *out=cachedReport;
#if DOOMRPG_RES60_P38_WALL_ADMISSION
        out->admittedWallIds=admittedIds;
#endif
        out->cacheHit=1U;
        out->readBytes=0U;
#if DOOMRPG_RES60_P11_PROFILE
        out->profileCatalogUs=0U;
        out->profilePackUs=0U;
        out->profileSwatchUs=swatchUs;
        out->profileOverlayUs=0U;
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
        {
            EspRes60WallMaterialStats material;
#if DOOMRPG_RES60_P11_PROFILE
            p11At=EspRes60Profile_nowUs();
#endif
            if(!EspRes60WallMaterial_overlay(fb,fbBytes,texelScratch,
                   sizeof(texelScratch),cachedPalette,
                   cachedReport.logicalId,&material)) {
                EspRes60TextureProbe_reset();
                return 0;
            }
            out->wallColumns=material.columns;
            out->wallPixels=material.pixels;
            out->wallFNV=material.sampledFNV;
            out->visibleWallColumns=(uint16_t)
                 EspRes60WallMaterial_visibleColumns(cachedReport.logicalId);
#if DOOMRPG_RES60_P11_PROFILE
            out->profileOverlayUs=EspRes60Profile_nowUs()-p11At;
#endif
        }
#endif
#if DOOMRPG_RES60_P11_PROFILE
        out->profileTotalUs=EspRes60Profile_nowUs()-p11Start;
#endif
        return 1;
    }
    if(cacheValid) EspRes60TextureProbe_reset();

#if DOOMRPG_RES60_P11_PROFILE
    p11At=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P34_CATALOG_WINDOWS
    /* Cold miss only: swatch storage is not yet populated. The loan ends
     * before the real tile read; no cached texel can be overwritten. */
    if(EspNativeGraphicsCatalog_buildFromRuntimeWithScratch(
           texelScratch,sizeof(texelScratch))!=
          ESP_NATIVE_GRAPHICS_CATALOG_OK) goto done;
#else
    if(EspNativeGraphicsCatalog_buildFromRuntime()!=
          ESP_NATIVE_GRAPHICS_CATALOG_OK) goto done;
#endif
    catalog=EspNativeGraphicsCatalog_view();
    if(!catalog || catalog->textureCount==0U ||
       catalog->textureCount>256U) goto done;
    memset(&selection,0,sizeof(selection));
    /* P10 source priority: a material actually hit by the current 192
     * cardinal rays, scanning from view center out. P3's complete immutable
     * plan already exists: no second BSP traversal, raycast, heap or PAK IO. */
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    for(id=0U;id<ESP_RES60_WIDTH;++id) {
        const uint32_t center=ESP_RES60_WIDTH/2U;
        const uint32_t col=(id&1U) ? center-(id+1U)/2U
                                    : center+id/2U;
        uint16_t visibleTexture=0U;
        const EspNativeGraphicsCatalogRecord* rec;
        if(!EspRes60WallMaterial_visibleTextureAt(col,&visibleTexture))
            continue;
        rec=EspNativeGraphicsCatalog_findTexture(visibleTexture);
        if(!rec || (rec->sourceOffset&1U) ||
           rec->resourceId!=visibleTexture)continue;
        selection=*rec;found=1;selectedFromVisible=1U;break;
    }
#endif
    /* Preserve P8's independent swatch contract if NO visible source is
     * catalogued: a mapped original wall line still supplies the sample.
     * An unmapped material or odd nibble offset must never be fabricated. */
    if(!found) {
        for(id=0U;id<EspMapRuntime_view()->lineCount;++id){
            EspMapLine line;
            const EspNativeGraphicsCatalogRecord* rec;
            if(!EspMapRuntime_getLine(id,&line)) goto done;
            if((line.flags&0x20000002UL)!=0U || line.texture>255U)continue;
            rec=EspNativeGraphicsCatalog_findTexture(line.texture);
            if(!rec || (rec->sourceOffset&1U))continue;
            selection=*rec;found=1;break;
        }
    }
    if(!found)goto done;
#if DOOMRPG_RES60_P11_PROFILE
    out->profileCatalogUs=EspRes60Profile_nowUs()-p11At;
    p11At=EspRes60Profile_nowUs();
#endif
    if(!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))goto done;
    open=1;
    if(!EspAssetPack_findEntry("wtexels.bin",&texels) ||
       texels.size<4U ||
       !EspAssetPack_readRange(&texels,0U,header,4U))goto done;
    expectedBytes=(uint32_t)header[0] |
                  ((uint32_t)header[1]<<8) |
                  ((uint32_t)header[2]<<16) |
                  ((uint32_t)header[3]<<24);
    if((uint64_t)expectedBytes+4U!=(uint64_t)texels.size)goto done;
#if DOOMRPG_RES60_P38_WALL_ADMISSION
    if(!catalog->textures)goto done;
    /* The existing catalog build already validated palette provenance.
     * Admit only logical IDs with an aligned complete 64x64 packed range.
     * No reads of the other tile payloads and no hardcoded map material IDs. */
    for(id=0U;id<catalog->textureCount;++id) {
        const EspNativeGraphicsCatalogRecord* rec=&catalog->textures[id];
        const uint16_t logical=rec->resourceId;
        if(logical>255U || (rec->sourceOffset&1U) ||
           (uint64_t)rec->sourceOffset/2U+ESP_RES60_TEXTURE_BYTES>expectedBytes)
            continue;
        admission[logical>>3U]|=(uint8_t)(1U<<(logical&7U));
    }
    if(!admitWallPlan(admission,&admittedIds))goto done;
    out->admittedWallIds=admittedIds;
#endif
#if DOOMRPG_RES60_P39_WALL_STREAM
    releaseWallStream();
    streamRecords=(EspNativeGraphicsCatalogRecord*)malloc(
        (size_t)catalog->textureCount*sizeof(*streamRecords));
    if(!streamRecords)goto done;
    streamCount=catalog->textureCount;
    memcpy(streamRecords,catalog->textures,(size_t)streamCount*sizeof(*streamRecords));
    streamEntry=texels;
#endif
    if((uint64_t)selection.sourceOffset/2U + 4U +
       ESP_RES60_TEXTURE_BYTES>texels.size)goto done;
    relative=4U+selection.sourceOffset/2U;
    if(!EspAssetPack_readRange(&texels,relative,texelScratch,
                               ESP_RES60_TEXTURE_BYTES))goto done;
#if DOOMRPG_RES60_P11_PROFILE
    out->profilePackUs=EspRes60Profile_nowUs()-p11At;
    p11At=EspRes60Profile_nowUs();
#endif
    /* All IO, source and palette admission precede any framebuffer write. */
    if(!EspRes60TextureProbe_paint(fb,fbBytes,texelScratch,
           ESP_RES60_TEXTURE_BYTES,selection.paletteRgb565,
           &swatchHash))goto done;
#if DOOMRPG_RES60_P11_PROFILE
    out->profileSwatchUs=EspRes60Profile_nowUs()-p11At;
#endif
    out->logicalId=selection.resourceId;
    out->catalogCount=catalog->textureCount;
    out->sourceOffsetNibbles=selection.sourceOffset;
    out->catalogFNV=catalog->stateFNV1a;
    out->texelFNV=fnvBytes(texelScratch,sizeof(texelScratch));
    out->sampleFNV=swatchHash;
    out->readBytes=4U+ESP_RES60_TEXTURE_BYTES;
    out->pixels=ESP_RES60_TEXTURE_INSET_SIZE*ESP_RES60_TEXTURE_INSET_SIZE;
    out->cacheHit=0U;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    out->selectedFromVisible=selectedFromVisible;
    out->visibleWallColumns=(uint16_t)
         EspRes60WallMaterial_visibleColumns(selection.resourceId);
#endif
    memcpy(cachedPalette,selection.paletteRgb565,sizeof(cachedPalette));
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    {
        EspRes60WallMaterialStats material;
#if DOOMRPG_RES60_P11_PROFILE
        p11At=EspRes60Profile_nowUs();
#endif
        if(!EspRes60WallMaterial_overlay(fb,fbBytes,texelScratch,
               sizeof(texelScratch),cachedPalette,
               selection.resourceId,&material))goto done;
        out->wallColumns=material.columns;
        out->wallPixels=material.pixels;
        out->wallFNV=material.sampledFNV;
#if DOOMRPG_RES60_P11_PROFILE
        out->profileOverlayUs=EspRes60Profile_nowUs()-p11At;
#endif
    }
#endif
    cachedReport=*out;
    cachedSourceCrc=activeMap->sourceCrc32;
    cachedArenaFNV=activeMap->arenaFNV1a;
    cachedSourceBytes=activeMap->sourceBytes;
    success=1;
done:
    if(open)EspAssetPack_close();
    EspNativeGraphicsCatalog_reset();
#if DOOMRPG_RES60_P39_WALL_STREAM
    if(!success)releaseWallStream();
#endif
    if(EspAssetPack_isOpen() || EspNativeGraphicsCatalog_isReady()) {
        EspRes60TextureProbe_reset();
        return 0;
    }
    /* Publish only AFTER successful source IO, swatch decode and cleanup. */
    if(success) {
#if DOOMRPG_RES60_P38_WALL_ADMISSION
        memcpy(cachedWallAdmission,admission,sizeof(cachedWallAdmission));
#endif
        cacheValid=1U;
    }
#if DOOMRPG_RES60_P11_PROFILE
    if(success) out->profileTotalUs=EspRes60Profile_nowUs()-p11Start;
#endif
    return success;
}

#if DOOMRPG_RES60_P39_WALL_STREAM
/* Preserve the independently verified P8 tile and P10 sample FNV. The
 * existing 6KiB projection snapshot is dead after raycast; its caller loans
 * 2KiB here. Only one additional packed tile is live at any instant. */
int EspRes60TextureProbe_streamWalls(uint16_t* fb, size_t fbBytes,
                                     void* scratch, size_t scratchBytes,
                                     EspRes60TextureProbeReport* out) {
    EspRes60WallCoverage demand;
    EspAssetPackEntry entry;
    uint32_t cols,pixels,hash=2166136261U;
    uint16_t reads=0U,admitted=0U;
    int opened=0,ok=0;
    const EspMapRuntimeView* map=EspMapRuntime_view();
    if(!out || !fb || fbBytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
       !scratch || scratchBytes<ESP_RES60_TEXTURE_BYTES ||
       !cacheValid || !streamRecords || !streamCount ||
       !map || map->sourceCrc32!=cachedSourceCrc ||
       map->arenaFNV1a!=cachedArenaFNV || map->sourceBytes!=cachedSourceBytes ||
       EspAssetPack_isOpen() || EspNativeGraphicsCatalog_isReady())
        return 0;
    /* Protect the sole framebuffer and session-owned swatch/metadata. */
    const uintptr_t a=(uintptr_t)scratch,b=(uintptr_t)fb;
    if((a>=b && a-b<fbBytes) || (b>=a && b-a<ESP_RES60_TEXTURE_BYTES))return 0;
    if(!admitWallPlan(cachedWallAdmission,&admitted) ||
       !EspRes60WallMaterial_measure(&demand) ||
       admitted!=demand.materials || demand.unsupportedColumns ||
       out->admittedWallIds!=admitted || out->logicalId!=cachedReport.logicalId ||
       out->streamReads || out->streamMetadataBytes)goto done;
    cols=out->wallColumns;
    pixels=out->wallPixels;
    /* Stream only other paintable materials, in immutable catalog order.
     * Fully inset-hidden winners are admitted by P38 but need no tile IO. */
    for(uint16_t i=0U;i<streamCount;++i) {
        const EspNativeGraphicsCatalogRecord* rec=&streamRecords[i];
        const uint16_t id=rec->resourceId;
        EspRes60WallMaterialStats material;
        if(id==out->logicalId || !EspRes60WallMaterial_visibleColumns(id))continue;
        if(id>255U || !(cachedWallAdmission[id>>3U]&(uint8_t)(1U<<(id&7U))) ||
           (rec->sourceOffset&1U) ||
           (uint64_t)rec->sourceOffset/2U+4U+ESP_RES60_TEXTURE_BYTES>streamEntry.size)
            goto done;
        if(!opened) {
            if(!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))goto done;
            opened=1;
            if(!EspAssetPack_findEntry("wtexels.bin",&entry) ||
               entry.nameHash!=streamEntry.nameHash || entry.offset!=streamEntry.offset ||
               entry.size!=streamEntry.size || entry.crc32!=streamEntry.crc32 ||
               entry.flags!=streamEntry.flags)goto done;
        }
        if(!EspAssetPack_readRange(&entry,4U+rec->sourceOffset/2U,
                                   scratch,ESP_RES60_TEXTURE_BYTES))goto done;
        ++reads;
        if(!EspRes60WallMaterial_overlay(fb,fbBytes,(const uint8_t*)scratch,
                   ESP_RES60_TEXTURE_BYTES,rec->paletteRgb565,id,&material) ||
           !material.pixels || material.columns!=EspRes60WallMaterial_visibleColumns(id))
            goto done;
        cols+=material.columns;
        pixels+=material.pixels;
        hash=fnvAppend(hash,(uint8_t)id);
        for(unsigned shift=0U;shift<32U;shift+=8U)
            hash=fnvAppend(hash,(uint8_t)(material.sampledFNV>>shift));
    }
    if(pixels!=(uint32_t)demand.pixels-demand.insetPixels ||
       cols>demand.columns || reads>admitted)goto done;
    ok=1;
done:
    if(opened)EspAssetPack_close();
    if(!ok || EspAssetPack_isOpen()) {
        EspRes60TextureProbe_reset();
        printf("[RES60P39] FAIL CLOSED stage=wall-stream-or-coverage\n");
        return 0;
    }
    out->wallColumns=cols;
    out->wallPixels=pixels;
    out->streamReads=reads;
    out->streamBytes=(uint32_t)reads*ESP_RES60_TEXTURE_BYTES;
    out->streamMetadataBytes=(uint16_t)((size_t)streamCount*sizeof(*streamRecords));
    out->streamFNV=hash;
    return 1;
}
#endif
#else /* P40: sole temporary packed tile borrowed from existing snapshot. */
static EspNativeGraphicsCatalogRecord* fullRecords;
static uint16_t fullCount;
static uint32_t fullCrc,fullArena,fullBytes,fullCatalogFNV;
static EspAssetPackEntry fullEntry;
static uint8_t fullAdmission[ESP_RES60_WALL_ADMISSION_BYTES];

void EspRes60TextureProbe_reset(void) {
    EspRes60WallMaterial_reset();
    free(fullRecords);
    fullRecords=NULL;
    fullCount=0U;
    fullCrc=fullArena=fullBytes=fullCatalogFNV=0U;
    memset(&fullEntry,0,sizeof(fullEntry));
    memset(fullAdmission,0,sizeof(fullAdmission));
}
static int sameWallEntry(const EspAssetPackEntry* a,const EspAssetPackEntry* b) {
    return a->nameHash==b->nameHash && a->offset==b->offset &&
           a->size==b->size && a->crc32==b->crc32 && a->flags==b->flags;
}
static uint32_t fullHashByte(uint32_t h,uint8_t b) {
    return (h^b)*16777619U;
}
int EspRes60TextureProbe_renderFullWalls(uint16_t* fb,size_t fbBytes,
                                        void* scratch,size_t scratchBytes,
                                        EspRes60TextureProbeReport* out) {
    const EspMapRuntimeView* map;
    const EspNativeGraphicsCatalogView* catalog;
    EspAssetPackEntry entry;
    EspRes60WallCoverage demand;
    EspRes60TextureProbeReport report={0};
    uint8_t header[4];
    uint16_t missing,admitted;
    uint32_t hash=2166136261U;
    int opened=0,catalogOwned=0,ok=0;
    if(out)memset(out,0,sizeof(*out));
    if(!out || !fb || fbBytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
       !scratch || scratchBytes<ESP_RES60_TEXTURE_BYTES ||
       !EspMapRuntime_isLoaded() || EspAssetPack_isOpen() ||
       EspNativeGraphicsCatalog_isReady())return 0;
    const uintptr_t a=(uintptr_t)scratch,b=(uintptr_t)fb;
    if((a>=b && a-b<fbBytes) || (b>=a && b-a<ESP_RES60_TEXTURE_BYTES))return 0;
    map=EspMapRuntime_view();
    if(!map || !map->sourceCrc32 || !map->arenaFNV1a || !map->sourceBytes ||
       !map->lineCount || !EspRes60WallMaterial_measure(&demand) ||
       demand.unsupportedColumns || demand.insetPixels)goto done;
#if DOOMRPG_RES60_P11_PROFILE
    const uint32_t start=EspRes60Profile_nowUs();
    uint32_t phase=start;
#endif
    if(fullRecords) {
        /* Do not reset the just-published ray plan on an identity mismatch.
         * Reject this frame and invalidate the lease through the failure path. */
        if(fullCrc!=map->sourceCrc32 || fullArena!=map->arenaFNV1a ||
           fullBytes!=map->sourceBytes)goto done;
        report.cacheHit=1U;
    } else {
        catalogOwned=1;
#if DOOMRPG_RES60_P34_CATALOG_WINDOWS
        if(EspNativeGraphicsCatalog_buildFromRuntimeWithScratch(scratch,scratchBytes)
           !=ESP_NATIVE_GRAPHICS_CATALOG_OK)goto done;
#else
        if(EspNativeGraphicsCatalog_buildFromRuntime()!=ESP_NATIVE_GRAPHICS_CATALOG_OK)goto done;
#endif
        catalog=EspNativeGraphicsCatalog_view();
        if(!catalog || !catalog->textures || !catalog->textureCount ||
           catalog->textureCount>256U)goto done;
        fullCount=catalog->textureCount;
        fullRecords=(EspNativeGraphicsCatalogRecord*)malloc(
                    (size_t)fullCount*sizeof(*fullRecords));
        if(!fullRecords)goto done;
        memcpy(fullRecords,catalog->textures,(size_t)fullCount*sizeof(*fullRecords));
        fullCatalogFNV=catalog->stateFNV1a;
        fullCrc=map->sourceCrc32;fullArena=map->arenaFNV1a;fullBytes=map->sourceBytes;
        EspNativeGraphicsCatalog_reset();
        catalogOwned=0;
        if(EspNativeGraphicsCatalog_isReady() || EspAssetPack_isOpen())goto done;
#if DOOMRPG_RES60_P11_PROFILE
        report.profileCatalogUs=EspRes60Profile_nowUs()-phase;
#endif
    }
#if DOOMRPG_RES60_P11_PROFILE
    phase=EspRes60Profile_nowUs();
#endif
    if(!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))goto done;
    opened=1;
    if(!EspAssetPack_findEntry("wtexels.bin",&entry) || entry.size<4U)goto done;
    if(report.cacheHit) {
        if(!sameWallEntry(&entry,&fullEntry))goto done;
    } else {
        if(!EspAssetPack_readRange(&entry,0U,header,sizeof(header)))goto done;
        const uint32_t bytes=(uint32_t)header[0]|((uint32_t)header[1]<<8U)|
             ((uint32_t)header[2]<<16U)|((uint32_t)header[3]<<24U);
        if((uint64_t)bytes+4U!=entry.size)goto done;
        fullEntry=entry;
        report.readBytes=4U;
        for(uint16_t i=0U;i<fullCount;++i) {
            const EspNativeGraphicsCatalogRecord* rec=&fullRecords[i];
            const uint16_t id=rec->resourceId;
            if(id>255U || (rec->sourceOffset&1U) ||
               (uint64_t)rec->sourceOffset/2U+ESP_RES60_TEXTURE_BYTES>bytes)continue;
            const uint8_t bit=(uint8_t)(1U<<(id&7U));
            if(fullAdmission[id>>3U]&bit)goto done; /* duplicate logical ID */
            fullAdmission[id>>3U]|=bit;
        }
    }
    if(!EspRes60WallMaterial_admit(fullAdmission,sizeof(fullAdmission),&admitted,&missing) ||
       admitted!=demand.materials)goto done;
    report.admittedWallIds=admitted;
    report.catalogCount=fullCount;
    report.catalogFNV=fullCatalogFNV;
    report.streamMetadataBytes=(uint16_t)((size_t)fullCount*sizeof(*fullRecords));
#if DOOMRPG_RES60_P11_PROFILE
    report.profilePackUs=EspRes60Profile_nowUs()-phase;
#endif
    for(uint16_t i=0U;i<fullCount;++i) {
        const EspNativeGraphicsCatalogRecord* rec=&fullRecords[i];
        const uint16_t id=rec->resourceId;
        const uint32_t columns=EspRes60WallMaterial_visibleColumns(id);
        EspRes60WallMaterialStats material;
        if(!columns)continue;
        if(id>255U || !(fullAdmission[id>>3U]&(uint8_t)(1U<<(id&7U))) ||
           (rec->sourceOffset&1U) ||
           (uint64_t)rec->sourceOffset/2U+4U+ESP_RES60_TEXTURE_BYTES>entry.size)goto done;
#if DOOMRPG_RES60_P11_PROFILE
        phase=EspRes60Profile_nowUs();
#endif
        if(!EspAssetPack_readRange(&entry,4U+rec->sourceOffset/2U,scratch,
                                   ESP_RES60_TEXTURE_BYTES))goto done;
#if DOOMRPG_RES60_P11_PROFILE
        report.profilePackUs+=EspRes60Profile_nowUs()-phase;
        phase=EspRes60Profile_nowUs();
#endif
        if(!EspRes60WallMaterial_overlay(fb,fbBytes,(const uint8_t*)scratch,
                    ESP_RES60_TEXTURE_BYTES,rec->paletteRgb565,id,&material) ||
           material.columns!=columns || !material.pixels)goto done;
#if DOOMRPG_RES60_P11_PROFILE
        report.profileOverlayUs+=EspRes60Profile_nowUs()-phase;
#endif
        ++report.streamReads;
        report.wallColumns+=material.columns;
        report.wallPixels+=material.pixels;
        hash=fullHashByte(hash,(uint8_t)id);
        for(unsigned shift=0U;shift<32U;shift+=8U)
            hash=fullHashByte(hash,(uint8_t)(material.sampledFNV>>shift));
    }
    if(report.streamReads!=admitted || report.wallColumns!=demand.columns ||
       report.wallPixels!=demand.pixels)goto done;
    report.streamBytes=(uint32_t)report.streamReads*ESP_RES60_TEXTURE_BYTES;
    report.readBytes+=report.streamBytes;
    report.streamFNV=hash;
    report.wallFNV=hash;
    ok=1;
done:
    if(opened)EspAssetPack_close();
    if(catalogOwned)EspNativeGraphicsCatalog_reset();
    if(!ok || EspAssetPack_isOpen() || EspNativeGraphicsCatalog_isReady()) {
        EspRes60TextureProbe_reset();
        printf("[RES60P40] FAIL CLOSED stage=full-wall-stream\n");
        return 0;
    }
#if DOOMRPG_RES60_P11_PROFILE
    report.profileTotalUs=EspRes60Profile_nowUs()-start;
#endif
    *out=report;
    return 1;
}
#endif /* P40 */
#endif /* P8 */
