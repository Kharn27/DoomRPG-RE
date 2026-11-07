/* Exercise REAL native indexed-BMP PAK row range reading and gib geometry
 * on an in-memory 96x144 atlas, with guards around the shared framebuffer.
 * Do not depend on DoomRPG.zip or accidentally test a mock BMP decoder. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_gameplay_gib_sheet.h"

#define W 160U
#define H 120U
#define BMP_W 96U
#define BMP_H 144U
#define BMP_PITCH 48U
#define BMP_PIXELS 118U
#define BMP_BYTES (BMP_PIXELS+BMP_PITCH*BMP_H)
static uint8_t raw[BMP_BYTES];
static uint16_t frame[W*H+2U];
static unsigned opens,reads,readBytes;
static int isOpen = 1;

static void le16(uint8_t* p,uint16_t v) {
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8U);
}
static void le32(uint8_t* p,uint32_t v) {
    le16(p,(uint16_t)v);le16(p+2U,(uint16_t)(v>>16U));
}
static uint16_t rgb565(unsigned r,unsigned g,unsigned b) {
    return (uint16_t)(((r&0xf8U)<<8U)|((g&0xfcU)<<3U)|(b>>3U));
}
static uint16_t color(unsigned idx) {
    if(idx==0U)return 0xf81fU;
    return rgb565(idx*15U,(idx*11U)%256U,(idx*17U)%256U);
}
static unsigned pixelIndex(unsigned x,unsigned y) {
    return ((x%24U)==0U||(y%24U)==0U)
        ? 0U : 1U+(((x/24U)+(y/24U)*4U)%15U);
}
static void init(void) {
    unsigned i,x,y;
    memset(raw,0,sizeof raw);
    raw[0]='B';raw[1]='M';
    le32(raw+2U,BMP_BYTES);
    le32(raw+10U,BMP_PIXELS);
    le32(raw+14U,40U);
    le32(raw+18U,BMP_W);
    le32(raw+22U,BMP_H);
    le16(raw+26U,1U);le16(raw+28U,4U);
    le32(raw+34U,BMP_PITCH*BMP_H);
    le32(raw+46U,16U);
    for(i=0U;i<16U;++i) {
        uint8_t* c=raw+54U+i*4U;
        if(i==0U){c[0]=255;c[1]=0;c[2]=255;}
        else{c[2]=(uint8_t)(i*15U);c[1]=(uint8_t)((i*11U)%256U);
             c[0]=(uint8_t)((i*17U)%256U);}
    }
    for(y=0U;y<BMP_H;++y)for(x=0U;x<BMP_W;++x){
        const unsigned idx=pixelIndex(x,y);
        uint8_t* b=raw+BMP_PIXELS+(BMP_H-1U-y)*BMP_PITCH+x/2U;
        if((x&1U)==0U)*b=(uint8_t)(*b|idx<<4U);
        else *b=(uint8_t)(*b|idx);
    }
}
int EspAssetPack_isOpen(void){return isOpen;}
int EspAssetPack_findEntry(const char* name,EspAssetPackEntry* e){
    ++opens;
    if(name==NULL||strcmp(name,"gibs_24.bmp")!=0||e==NULL)return 0;
    memset(e,0,sizeof *e);e->size=BMP_BYTES;e->nameHash=1234U;return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* e,uint32_t off,
                           void* out,size_t bytes) {
    ++reads;readBytes+=bytes;
    if(!isOpen||e==NULL||out==NULL||
       bytes>BMP_BYTES||off>BMP_BYTES-bytes)return 0;
    memcpy(out,raw+off,bytes);return 1;
}
static void resetFB(void) {
    unsigned i;
    for(i=0U;i<W*H+2U;++i)frame[i]=0x5555U;
    frame[0]=0x1234U;frame[W*H+1U]=0x4321U;
}
static void compare(const EspNativeGibSheetRect* p,int berserk) {
    unsigned j;
    for(j=0U;j<W*H;++j){
        const int x=(int)(j%W),y=(int)(j/W);
        uint16_t expect=0x5555U;
        if(x>=p->destinationX && y>=p->destinationY &&
           x<p->destinationX+(int)p->width &&
           y<p->destinationY+(int)p->height){
            const unsigned sx=(unsigned)(x-p->destinationX)+p->sourceX;
            const unsigned sy=(unsigned)(y-p->destinationY)+p->sourceY;
            const unsigned idx=pixelIndex(sx,sy);
            if(idx!=0U) {
                const uint16_t original=color(idx);
                expect=berserk?EspNativeGibSheet_berserk565(original):original;
            }
        }
        assert(frame[1U+j]==expect);
    }
    assert(frame[0]==0x1234U&&frame[W*H+1U]==0x4321U);
}
int main(void){
    EspNativeIndexedBmp bmp;
    EspNativeIndexedBmpStats stats;
    EspNativeGibSheetRect p;
    unsigned row,col,cases=0U;
    init();
    memset(&stats,0,sizeof stats);
    assert(EspNativeIndexedBmp_open("gibs_24.bmp",&bmp,&stats)==
           ESP_NATIVE_INDEXED_BMP_OK);
    assert(bmp.width==BMP_W&&bmp.height==BMP_H&&
           bmp.filePitch==BMP_PITCH&&bmp.bitsPerPixel==4U);
    assert(opens==1U&&reads>=2U);
    for(row=0U;row<6U;++row)for(col=0U;col<4U;++col){
        uint16_t sourceY;
        memset(&stats,0,sizeof stats);
        resetFB();
        assert(EspNativeGibSheet_plan(col*175U,row,80,52,-1,
                                     bmp.width,bmp.height,&p));
        sourceY=p.sourceY;
        assert(sourceY==row*24U);
        assert(EspNativeIndexedBmp_blit(
            &bmp,&frame[1],W,H,p.sourceX,p.sourceY,p.width,p.height,
            p.destinationX,p.destinationY,1U,&stats)==
            ESP_NATIVE_INDEXED_BMP_OK);
        assert(stats.rowsRead==24U&&stats.packReads>=2U&&
               stats.pixelsWritten>0U);
        compare(&p,0);
        ++cases;
    }
    /* A clipping path where the 24px source is cropped at the viewport
     * top and framebuffer left, not into either HUD band. */
    resetFB();
    assert(EspNativeGibSheet_plan(0U,4U,-2,10,-1,
                                 bmp.width,bmp.height,&p));
    memset(&stats,0,sizeof stats);
    assert(EspNativeIndexedBmp_blit(
        &bmp,&frame[1],W,H,p.sourceX,p.sourceY,p.width,p.height,
        p.destinationX,p.destinationY,1U,&stats)==
        ESP_NATIVE_INDEXED_BMP_OK);
    compare(&p,0);
    /* Source DoomRPG_createImageBerserkColor() palette transformation
     * must keep the original SDL magenta color-key index transparent.
     * Native row reader keys by RGB565, so preserve that palette entry. */
    {
        EspNativeIndexedBmp berserk=bmp;
        unsigned k;
        for(k=0U;k<berserk.paletteCount;++k) {
            const uint16_t original=bmp.paletteRgb565[k];
            berserk.paletteRgb565[k]=original==0xf81fU
                ?original:EspNativeGibSheet_berserk565(original);
        }
        assert(berserk.paletteRgb565[0]==0xf81fU);
        for(row=0U;row<6U;++row)for(col=0U;col<4U;++col) {
            resetFB();
            assert(EspNativeGibSheet_plan(col*175U,row,80,52,1,
                                         berserk.width,berserk.height,&p));
            memset(&stats,0,sizeof stats);
            assert(EspNativeIndexedBmp_blit(
                &berserk,&frame[1],W,H,p.sourceX,p.sourceY,p.width,p.height,
                p.destinationX,p.destinationY,1U,&stats)==
                ESP_NATIVE_INDEXED_BMP_OK);
            compare(&p,1);
        }
    }
    assert(EspNativeIndexedBmp_open("not-gibs.bmp",&bmp,NULL)==
           ESP_NATIVE_INDEXED_BMP_NOT_FOUND);
    isOpen=0;
    assert(EspNativeIndexedBmp_open("gibs_24.bmp",&bmp,NULL)==
           ESP_NATIVE_INDEXED_BMP_PACK_NOT_OPEN);
    printf("PASS original gibs indexed 96x144 4bpp: %u normal +24 berserk cases, 4x6/175ms atlas, transparent magenta, all pixels/HUD/canaries, range reads=%u bytes=%u\\n",
           cases,reads,readBytes);
    return 0;
}
