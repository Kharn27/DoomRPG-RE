#include "platform_video_config.h"
#if DOOMRPG_RES60_P8_TEXEL
#include <stdint.h>
#include <stddef.h>
#include <string.h>
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

void EspRes60TextureProbe_reset(void) {
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    EspRes60WallMaterial_reset();
#endif
    cacheValid=0U;
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
    if(EspAssetPack_isOpen() || EspNativeGraphicsCatalog_isReady()) {
        EspRes60TextureProbe_reset();
        return 0;
    }
    /* Publish only AFTER successful source IO, swatch decode and cleanup. */
    if(success) cacheValid=1U;
#if DOOMRPG_RES60_P11_PROFILE
    if(success) out->profileTotalUs=EspRes60Profile_nowUs()-p11Start;
#endif
    return success;
}
#endif
