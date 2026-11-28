#include "platform_video_config.h"
#if DOOMRPG_RES60_P10_WALL_MATERIAL
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_res60_layout.h"
#include "esp_res60_wall_material.h"
#include "esp_res60_texture_probe.h"

/* 192 x 8B = 1536B statically owned; never an allocation, second framebuffer
 * or a giant decoded texture table. P3 overwrites this plan each frame. */
typedef struct EspRes60WallColumn_s {
    int16_t top;
    int16_t height;
    uint16_t texture;
    uint8_t sourceU;
    uint8_t hit;
} EspRes60WallColumn;
typedef char EspRes60WallColumn_size_must_be_8[
    sizeof(EspRes60WallColumn)==8 ? 1 : -1];
static EspRes60WallColumn columns[ESP_RES60_WIDTH];
static uint8_t planReady;

void EspRes60WallMaterial_reset(void) {
    planReady=0U;
    memset(columns,0,sizeof(columns));
}

int EspRes60WallMaterial_record(uint32_t column, uint16_t logicalTexture,
                               int32_t top, int32_t height, uint8_t sourceU) {
    EspRes60WallColumn* item;
    if(planReady || column>=ESP_RES60_WIDTH || height<=0 ||
       height>2048 || top< -2048 || top>2048 || sourceU>=64U)
        return 0;
    item=&columns[column];
    item->top=(int16_t)top;
    item->height=(int16_t)height;
    item->texture=logicalTexture;
    item->sourceU=sourceU;
    item->hit=1U;
    return 1;
}
void EspRes60WallMaterial_publish(void) {planReady=1U;}

/* A ray contributes only if at least one of its wall pixels survives
 * clipping to 192x96 and the original P8 48x48 upper-right swatch. */
static int paintableColumn(uint32_t x, const EspRes60WallColumn* item) {
    const int32_t start=item->top < 0 ? 0 : item->top;
    const int32_t unclippedEnd=(int32_t)item->top+item->height;
    const int32_t end=unclippedEnd>(int32_t)ESP_RES60_WORLD_ROWS
        ? (int32_t)ESP_RES60_WORLD_ROWS : unclippedEnd;
    return item->hit && end>start &&
       (x<ESP_RES60_TEXTURE_INSET_X ||
        end>(int32_t)ESP_RES60_TEXTURE_INSET_SIZE);
}
int EspRes60WallMaterial_visibleTextureAt(uint32_t x,
                                          uint16_t* outTexture) {
    if(!planReady || !outTexture || x>=ESP_RES60_WIDTH ||
       !paintableColumn(x,&columns[x]))return 0;
    *outTexture=columns[x].texture;
    return 1;
}
uint32_t EspRes60WallMaterial_visibleColumns(uint16_t texture) {
    uint32_t matches=0U;
    if(!planReady)return 0U;
    for(uint32_t x=0U;x<ESP_RES60_WIDTH;++x)
       if(columns[x].texture==texture && paintableColumn(x,&columns[x]))
           ++matches;
    return matches;
}

int EspRes60WallMaterial_measure(EspRes60WallCoverage* out) {
    uint8_t seen[32]={0};
    EspRes60WallCoverage demand={0};
    if(out==NULL)return 0;
    *out=demand;
    if(!planReady)return 0;
    for(uint32_t x=0U;x<ESP_RES60_WIDTH;++x) {
        const EspRes60WallColumn* item=&columns[x];
        int32_t start=item->top<0?0:item->top;
        int32_t end=(int32_t)item->top+item->height;
        if(end>(int32_t)ESP_RES60_WORLD_ROWS)end=ESP_RES60_WORLD_ROWS;
        if(!item->hit || end<=start)continue;
        ++demand.columns;
        demand.pixels+=(uint16_t)(end-start);
        if(x>=ESP_RES60_TEXTURE_INSET_X && start<(int32_t)ESP_RES60_TEXTURE_INSET_SIZE) {
            int32_t insetEnd=end<(int32_t)ESP_RES60_TEXTURE_INSET_SIZE?end:(int32_t)ESP_RES60_TEXTURE_INSET_SIZE;
            demand.insetPixels+=(uint16_t)(insetEnd-start);
        }
        if(item->texture>255U) {
            ++demand.unsupportedColumns;
        } else {
            uint8_t mask=(uint8_t)(1U<<(item->texture&7U));
            if(!(seen[item->texture>>3]&mask)) {
                seen[item->texture>>3]|=mask;
                ++demand.materials;
            }
        }
    }
    *out=demand;
    return 1;
}
#if DOOMRPG_RES60_P38_WALL_ADMISSION
int EspRes60WallMaterial_admit(const uint8_t* allowed, size_t allowedBytes,
                               uint16_t* outIds, uint16_t* firstMissing) {
    uint8_t seen[ESP_RES60_WALL_ADMISSION_BYTES]={0};
    uint16_t count=0U;
    if(outIds)*outIds=0U;
    if(firstMissing)*firstMissing=UINT16_MAX;
    if(!planReady || !allowed || allowedBytes!=sizeof(seen) ||
       !outIds || !firstMissing)return 0;
    for(uint32_t x=0U;x<ESP_RES60_WIDTH;++x) {
        const EspRes60WallColumn* item=&columns[x];
        const int32_t start=item->top<0?0:item->top;
        int32_t end=(int32_t)item->top+item->height;
        if(end>(int32_t)ESP_RES60_WORLD_ROWS)end=(int32_t)ESP_RES60_WORLD_ROWS;
        if(!item->hit || end<=start)continue;
        const uint16_t id=item->texture;
        if(id>255U || !(allowed[id>>3U]&(uint8_t)(1U<<(id&7U)))) {
            *firstMissing=id;
            return 0;
        }
        const uint8_t bit=(uint8_t)(1U<<(id&7U));
        if(!(seen[id>>3U]&bit)) {
            seen[id>>3U]|=bit;
            ++count;
        }
    }
    *outIds=count;
    return 1;
}
#endif
static uint16_t framebuffer565(uint16_t c) {
    return (uint16_t)(((c&0x001fU)<<11U)|(c&0x07e0U)|((c&0xf800U)>>11U));
}
static uint32_t appendFNV(uint32_t h, uint8_t b) {
    return (h^b)*16777619U;
}
int EspRes60WallMaterial_overlay(uint16_t* fb, size_t fbBytes,
                                 const uint8_t* packed, size_t packedBytes,
                                 const uint16_t raw[16], uint16_t logicalTexture,
                                 EspRes60WallMaterialStats* out) {
    EspRes60WallMaterialStats stats={0U,0U,2166136261U};
    if(out)memset(out,0,sizeof(*out));
    if(!planReady || !fb || !packed || !raw || !out ||
       fbBytes!=ESP_RES60_FRAMEBUFFER_BYTES ||
       packedBytes!=ESP_RES60_WALL_MATERIAL_SOURCE_BYTES)
        return 0;
    for(uint32_t x=0U;x<ESP_RES60_WIDTH;++x) {
        const EspRes60WallColumn* item=&columns[x];
        uint32_t count=0U;
        if(!item->hit || item->texture!=logicalTexture)continue;
        for(uint32_t row=0U;row<ESP_RES60_WORLD_ROWS;++row) {
            int32_t local=(int32_t)row-item->top;
            uint32_t v,ix;
            uint8_t packedByte,nibble;
            uint16_t color;
            if(local<0 || local>=item->height)continue;
            /* Keep P8's independently verifiable 48x48 original swatch. */
            if(x>=ESP_RES60_TEXTURE_INSET_X &&
               row<ESP_RES60_TEXTURE_INSET_SIZE)continue;
            v=(uint32_t)(((int64_t)local*64)/item->height);
            if(v>=64U)return 0; /* never read outside the one 2KiB tile */
            ix=v*64U+item->sourceU;
            packedByte=packed[ix>>1U];
            nibble=(uint8_t)((ix&1U)?(packedByte>>4U):(packedByte&15U));
            color=framebuffer565(raw[nibble]);
            fb[((size_t)ESP_RES60_WORLD_Y+row)*ESP_RES60_WIDTH+x]=color;
            stats.sampledFNV=appendFNV(stats.sampledFNV,(uint8_t)color);
            stats.sampledFNV=appendFNV(stats.sampledFNV,(uint8_t)(color>>8U));
            ++stats.pixels;
            ++count;
        }
        if(count)++stats.columns;
    }
    *out=stats;
    return 1;
}
#endif
