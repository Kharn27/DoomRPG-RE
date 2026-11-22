/* WALL-SD-P1: exercise real C image and indexed-row decoders with exact
 * SD wallpaper bytes and PAK-compatible 4-bpp synthetic fallbacks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_native_font_sd.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_indexed_image.h"

static uint8_t pakC[12408], pakE[8312];
static uint8_t sdC[25654], sdE[17462];
static int available[2] = {1, 1}, packOpen;
static unsigned pakReads, sdReads, rejects, opens, probes;
static void wr16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t* p, uint32_t v) { wr16(p,(uint16_t)v); wr16(p+2,(uint16_t)(v>>16)); }

static void fakePak(uint8_t* out, size_t size, uint32_t w, uint32_t h) {
    const uint32_t pitch = ((w*4U+31U)/32U)*4U, offset=118U;
    assert(size >= offset+pitch*h);
    memset(out, 0, size); out[0]='B'; out[1]='M';
    wr32(out+2, (uint32_t)size); wr32(out+10, offset); wr32(out+14,40U);
    wr32(out+18,w); wr32(out+22,h); wr16(out+26,1U);
    wr16(out+28,4U); wr32(out+34,pitch*h); wr32(out+46,16U);
    out[54]=255U; out[55]=0; out[56]=255U; /* colorkey */
    out[58]=255U; out[59]=255U; out[60]=255U;
    memset(out+offset, 0x11, pitch*h);
}
static void load(const char* file, uint8_t* dst, size_t expected) {
    FILE* f=fopen(file,"rb"); assert(f);
    assert(fread(dst,1,expected,f)==expected);
    assert(fgetc(f)==EOF); fclose(f);
}
static int indexOf(const char* s) {
    if(strcmp(s,"c.bmp")==0)return 0;
    if(strcmp(s,"e.bmp")==0)return 1;
    return -1;
}
int EspAssetPack_open(const char* path) {
    assert(strcmp(path,ESP_ASSET_PACK_DEFAULT_PATH)==0);
    if(packOpen)return 0;
    packOpen=1; ++opens; return 1;
}
void EspAssetPack_close(void){packOpen=0;}
int EspAssetPack_isOpen(void){return packOpen;}
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* out) {
    int i=indexOf(name);if(!packOpen||i<0)return 0;
    memset(out,0,sizeof(*out));
    out->nameHash=(uint32_t)(10+i);
    out->size=i==0?sizeof(pakC):sizeof(pakE);
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* ent, uint32_t off,
                           void* dest, size_t n) {
    int i=(int)ent->nameHash-10;
    if(i<0||i>1||!packOpen)return 0;
    const uint8_t* b=i==0?pakC:pakE;
    uint32_t sz=i==0?sizeof(pakC):sizeof(pakE);
    if(off>sz||n>sz-off)return 0;
    memcpy(dest,b+off,n);++pakReads;return 1;
}
uint8_t EspNativeFontSd_probe(const char* name, uint32_t* size) {
    int i=indexOf(name);++probes;*size=0;
    if(i<0||!available[i])return 0;
    *size=i==0?sizeof(sdC):sizeof(sdE);
    return (uint8_t)(3+i);
}
int EspNativeFontSd_read(uint8_t source,uint32_t off,
                         void* dest,uint32_t n) {
    int i=(int)source-3;
    if(i<0||i>1||!available[i])return 0;
    uint32_t sz=i==0?sizeof(sdC):sizeof(sdE);
    const uint8_t* b=i==0?sdC:sdE;
    if(off>sz||n>sz-off)return 0;
    memcpy(dest,b+off,n);++sdReads;return 1;
}
void EspNativeFontSd_reject(uint8_t source) {
    assert(source==3U||source==4U);
    available[source-3U]=0;++rejects;
}
int main(void) {
    EspNativeIndexedImage img={0};
    EspNativeIndexedBmp row={0};
    EspNativeIndexedBmpStats stats={0};
    uint16_t framebuffer[32*32]={0};
    unsigned old;
    fakePak(pakC,sizeof(pakC),192U,128U);
    fakePak(pakE,sizeof(pakE),128U,128U);
    load("sdroot/doomrpg/img/c.bmp",sdC,sizeof(sdC));
    load("sdroot/doomrpg/img/e.bmp",sdE,sizeof(sdE));
    assert(EspNativeIndexedBmp_openSdFontStandalone("c.bmp",&row,&stats)
           == ESP_NATIVE_INDEXED_BMP_INVALID);
    assert(EspNativeIndexedBmp_openSdFontStandalone("e.bmp",&row,&stats)
           == ESP_NATIVE_INDEXED_BMP_INVALID);

    /* The SD asset is read BEFORE the PAK lease is opened, then adopted
     * without setting SD-font tint metadata. */
    assert(EspNativeImage_loadPack("c.bmp",0,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.width==192&&img.height==128&&img.bitsPerPixel==8U&&
           img.paletteSize==256U&&!img.sdFont&&!img.hasColorKey&&
           !packOpen&&pakReads==0U);
    EspNativeImage_dispose(&img);
    assert(EspNativeImage_loadPack("e.bmp",1,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.width==128&&img.height==128&&img.bitsPerPixel==8U&&
           img.paletteSize==256U&&!img.sdFont&&img.hasColorKey&&
           img.palette[0].r==255U&&img.palette[0].g==0U&&
           img.palette[0].b==255U&&!packOpen&&pakReads==0U);
    EspNativeImage_dispose(&img);
    assert(sdReads>=4U);

    /* Existing streaming reader also supports both names where the
     * PAK lease is already physically safe for SD I/O. */
    packOpen=1;
    assert(EspNativeIndexedBmp_open("c.bmp",&row,&stats)
           ==ESP_NATIVE_INDEXED_BMP_OK);
    assert(row.sdFontSource==ESP_NATIVE_WALLPAPER_SD_STARS&&
           row.bitsPerPixel==8U&&row.width==192U);
    assert(EspNativeIndexedBmp_open("e.bmp",&row,&stats)
           ==ESP_NATIVE_INDEXED_BMP_OK);
    assert(row.sdFontSource==ESP_NATIVE_WALLPAPER_SD_MARS&&
           row.bitsPerPixel==8U&&row.height==128U);
    assert(EspNativeIndexedBmp_blit(&row,framebuffer,32,32,
                                   0,0,32,32,0,0,1,&stats)
           ==ESP_NATIVE_INDEXED_BMP_OK);
    packOpen=0;

    /* Invalid geometry rejects C but E remains available. */
    wr32(sdC+18,191U);old=pakReads;
    assert(EspNativeImage_loadPack("c.bmp",0,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.bitsPerPixel==4U&&img.width==192&&
           !img.sdFont&&!packOpen&&pakReads>old&&rejects==1U);
    EspNativeImage_dispose(&img);
    old=pakReads;
    assert(EspNativeImage_loadPack("e.bmp",1,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.bitsPerPixel==8U&&pakReads==old&&rejects==1U);
    EspNativeImage_dispose(&img);

    /* Unsupported RGB24 similarly falls back, without changing the PAK. */
    wr16(sdE+28,24U);old=pakReads;
    assert(EspNativeImage_loadPack("e.bmp",1,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.bitsPerPixel==4U&&img.height==128&&
           pakReads>old&&rejects==2U&&!packOpen);
    EspNativeImage_dispose(&img);

    /* Missing (or now rejected) overrides are independently PAK-only. */
    old=sdReads;available[0]=available[1]=0;
    assert(EspNativeImage_loadPack("c.bmp",0,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.bitsPerPixel==4U&&!img.sdFont&&!packOpen);
    EspNativeImage_dispose(&img);
    assert(EspNativeImage_loadPack("e.bmp",1,&img)==ESP_NATIVE_IMAGE_OK);
    assert(img.bitsPerPixel==4U&&!img.sdFont&&!packOpen);
    EspNativeImage_dispose(&img);
    assert(sdReads==old&&pakReads>0U&&opens>0U&&probes>0U);
    puts("WALL-SD HOST PASS: real indexed decoders, 8bpp SD C/E, "
         "colorkey, independent malformed/missing PAK fallback");
    return 0;
}
