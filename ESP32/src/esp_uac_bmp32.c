#include "esp_uac_bmp32.h"
#include <limits.h>
#include <string.h>
static uint16_t le16(const uint8_t* p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t le32(const uint8_t* p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
int EspUacBmp32_open(EspUacBmp32* out,EspUacBmpReadAt reader,void* ctx,
                     uint32_t size,uint16_t expectedW,uint16_t expectedH){
    uint8_t h[122];
    uint32_t w,y,offset,rowBytes,payload;
    if(out)memset(out,0,sizeof(*out));
    if(!out||!reader||size<sizeof(h)||!expectedW||!expectedH||
       !reader(ctx,0U,h,sizeof(h)))return 0;
    w=le32(h+18);y=le32(h+22);offset=le32(h+10);
    if(h[0]!='B'||h[1]!='M'||le32(h+2)!=size||le32(h+14)!=108U||
       w!=expectedW||y!=expectedH||le16(h+26)!=1U||le16(h+28)!=32U||
       le32(h+30)!=3U||le32(h+54)!=0x00ff0000U||
       le32(h+58)!=0x0000ff00U||le32(h+62)!=0x000000ffU||
       le32(h+66)!=0xff000000U||offset!=sizeof(h)||
       w>UINT32_MAX/4U)return 0;
    rowBytes=w*4U;
    if(y>UINT32_MAX/rowBytes)return 0;
    payload=y*rowBytes;
    if(payload>UINT32_MAX-offset||offset+payload!=size||
       (le32(h+34)!=0U&&le32(h+34)!=payload))return 0;
    out->pixelOffset=offset;out->fileBytes=size;
    out->width=(uint16_t)w;out->height=(uint16_t)y;
    return 1;
}
int EspUacBmp32_loadBatch(const EspUacBmp32* bmp,EspUacBmpReadAt reader,
                          void* ctx,uint16_t firstTop,uint16_t rowCount,
                          uint8_t* storage,size_t capacity,
                          EspUacBmpRowBatch* out){
    uint32_t rowBytes,bytes,firstBottom,offset;
    if(out)memset(out,0,sizeof(*out));
    if(!bmp||!reader||!storage||!out||!bmp->width||!bmp->height||
       !rowCount||firstTop>=bmp->height||
       rowCount>(uint32_t)bmp->height-firstTop)return 0;
    rowBytes=(uint32_t)bmp->width*4U;
    if(rowCount>UINT32_MAX/rowBytes)return 0;
    bytes=(uint32_t)rowCount*rowBytes;
    if(bytes>capacity)return 0;
    firstBottom=(uint32_t)bmp->height-firstTop-rowCount;
    if(firstBottom>(UINT32_MAX-bmp->pixelOffset)/rowBytes)return 0;
    offset=bmp->pixelOffset+firstBottom*rowBytes;
    if(offset>bmp->fileBytes||bytes>bmp->fileBytes-offset||
       !reader(ctx,offset,storage,bytes))return 0;
    out->bytes=storage;out->fileOffset=offset;out->size=bytes;
    return 1;
}
int EspUacBmp32_readBatch(void* ctx,uint32_t offset,void* dst,size_t n){
    const EspUacBmpRowBatch* batch=(const EspUacBmpRowBatch*)ctx;
    if(!batch||!batch->bytes||!dst||offset<batch->fileOffset||
       offset-batch->fileOffset>batch->size||
       n>batch->size-(offset-batch->fileOffset))return 0;
    memcpy(dst,batch->bytes+(offset-batch->fileOffset),n);
    return 1;
}
static uint16_t blend(uint16_t dst,const uint8_t* p){
    uint32_t a=p[3],inv=255U-a,r,g,b;
    if(a==0U)return dst;
    if(a==255U)return (uint16_t)(((uint16_t)(p[2]&0xf8U)<<8)|
                                 ((uint16_t)(p[1]&0xfcU)<<3)|(p[0]>>3));
    r=((uint32_t)p[2]*a+((dst>>11&31U)*255U/31U)*inv+127U)/255U;
    g=((uint32_t)p[1]*a+((dst>>5&63U)*255U/63U)*inv+127U)/255U;
    b=((uint32_t)p[0]*a+((dst&31U)*255U/31U)*inv+127U)/255U;
    return (uint16_t)(((r&0xf8U)<<8)|((g&0xfcU)<<3)|(b>>3));
}
int EspUacBmp32_blendRow(const EspUacBmp32* bmp,EspUacBmpReadAt reader,
                        void* ctx,uint16_t topRow,int32_t destX,uint8_t scale,
                        uint16_t* dest,uint16_t destWidth,
                        uint8_t* scratch,size_t cap){
    uint32_t bytes,offset,bottom;
    uint16_t x;
    if(!bmp||!reader||!dest||!scratch||!destWidth||
       (scale!=1U&&scale!=2U)||!bmp->width||!bmp->height||
       topRow>=bmp->height)return 0;
    bytes=(uint32_t)bmp->width*4U;
    if(bytes>cap)return 0;
    bottom=(uint32_t)bmp->height-1U-topRow;
    if(bottom>(UINT32_MAX-bmp->pixelOffset)/bytes)return 0;
    offset=bmp->pixelOffset+bottom*bytes;
    if(offset>bmp->fileBytes||bytes>bmp->fileBytes-offset||
       !reader(ctx,offset,scratch,bytes))return 0;
    for(x=0;x<bmp->width;++x){
        uint8_t repeat;
        for(repeat=0;repeat<scale;++repeat){
            const int64_t dx=(int64_t)destX+(int64_t)x*scale+repeat;
            if(dx>=0&&dx<destWidth)
                dest[(uint16_t)dx]=blend(dest[(uint16_t)dx],scratch+(size_t)x*4U);
        }
    }
    return 1;
}
/* Exact nearest-neighbor downsample: skip odd source x/y, repeat 2x2.
 * No unbounded allocation, intermediate framebuffer, or alpha averaging. */
int EspUacBmp32_blendRow240As120(const EspUacBmp32* bmp,
                         EspUacBmpReadAt reader,void* ctx,
                         uint16_t virtualTopRow,int32_t destX,uint8_t scale,
                         uint16_t* dest,uint16_t destWidth,
                         uint8_t* scratch,size_t cap){
    uint32_t rowBytes,bottom,offset;
    if(!bmp||!reader||!dest||!scratch||!destWidth||scale!=2U||
       !bmp->width||!bmp->height||(bmp->width&1U)||(bmp->height&1U)||
       virtualTopRow>=bmp->height/2U)return 0;
    rowBytes=(uint32_t)bmp->width*4U;
    if(rowBytes>cap)return 0;
    bottom=(uint32_t)bmp->height-1U-(uint32_t)virtualTopRow*2U;
    if(bottom>(UINT32_MAX-bmp->pixelOffset)/rowBytes)return 0;
    offset=bmp->pixelOffset+bottom*rowBytes;
    if(offset>bmp->fileBytes||rowBytes>bmp->fileBytes-offset||
       !reader(ctx,offset,scratch,rowBytes))return 0;
    for(uint16_t x=0;x<bmp->width/2U;++x){
        const uint8_t* pixel=scratch+(size_t)x*8U;
        for(uint8_t sub=0;sub<2U;++sub){
            const int64_t dx=(int64_t)destX+(int64_t)x*2+sub;
            if(dx>=0&&dx<destWidth)
                dest[(uint16_t)dx]=blend(dest[(uint16_t)dx],pixel);
        }
    }
    return 1;
}
int EspUacMenu_hitPhysical(int x,int y){
    const int top[4]={20,74,128,182};
    int i;
    if(x<186||x>=296)return -1;
    for(i=0;i<4;++i)if(y>=top[i]&&y<top[i]+40)return i;
    return -1;
}
