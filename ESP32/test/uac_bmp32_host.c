/* Host execution of the *actual* ESP32/src/esp_uac_bmp32.c.
 * No Pillow/image emulation. Synthetic bottom-up BMP V4 BGRA32 fixture. */
#include "esp_uac_bmp32.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {uint8_t* bytes;uint32_t length;int open, fail;unsigned reads;} Blob;
static void put16(uint8_t* p,uint32_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void put32(uint8_t* p,uint32_t x){
    for(int i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i));
}
static Blob make(uint16_t w,uint16_t h){
    const uint32_t n=122U+(uint32_t)w*h*4U;
    Blob b={0};b.bytes=(uint8_t*)calloc(n,1);
    assert(b.bytes);b.length=n;b.open=1;
    b.bytes[0]='B';b.bytes[1]='M';
    put32(b.bytes+2,n);put32(b.bytes+10,122);
    put32(b.bytes+14,108);put32(b.bytes+18,w);put32(b.bytes+22,h);
    put16(b.bytes+26,1);put16(b.bytes+28,32);put32(b.bytes+30,3);
    put32(b.bytes+34,n-122);
    put32(b.bytes+54,0x00ff0000);put32(b.bytes+58,0x0000ff00);
    put32(b.bytes+62,0x000000ff);put32(b.bytes+66,0xff000000);
    return b;
}
static void destroy(Blob* b){free(b->bytes);b->bytes=NULL;b->open=0;}
static int readAt(void* context,uint32_t at,void* out,size_t n){
    Blob* b=(Blob*)context;
    if(!b->open||b->fail||at>b->length||n>b->length-at)return 0;
    memcpy(out,b->bytes+at,n);++b->reads;return 1;
}
static void pixel(Blob* b,int x,int y,uint8_t blue,uint8_t green,
                  uint8_t red,uint8_t alpha){
    const uint32_t p=122U+((uint32_t)(b->bytes[22]-(1+y))*b->bytes[18]+x)*4U;
    b->bytes[p]=blue;b->bytes[p+1]=green;
    b->bytes[p+2]=red;b->bytes[p+3]=alpha;
}
static void parser(void){
    Blob b=make(2,2);
    EspUacBmp32 info;uint16_t frame[5];uint8_t scratch[8];
    assert(EspUacBmp32_open(&info,readAt,&b,b.length,2,2));
    assert(info.width==2&&info.height==2&&info.fileBytes==138);
    pixel(&b,0,0,0,0,255,255); /* top-left opaque red */
    pixel(&b,1,0,255,0,0,128); /* top-right half blue */
    pixel(&b,0,1,0,255,0,0);   /* transparent lower */
    pixel(&b,1,1,0,255,0,255); /* opaque green lower */
    for(int i=0;i<5;++i)frame[i]=0xffff;
    assert(EspUacBmp32_blendRow(&info,readAt,&b,0,1,2,frame,5,scratch,8));
    assert(frame[0]==0xffff&&frame[1]==0xf800&&frame[2]==0xf800);
    assert(frame[3]!=0xffff&&frame[3]!=0x001f&&frame[3]==frame[4]);
    for(int i=0;i<5;++i)frame[i]=0xffff;
    assert(EspUacBmp32_blendRow(&info,readAt,&b,1,-1,2,frame,5,scratch,8));
    assert(frame[0]==0xffff); /* transparent source pixel zero */
    assert(frame[1]==0x07e0&&frame[2]==0x07e0); /* clipped opaque pixel */
    assert(!EspUacBmp32_blendRow(&info,readAt,&b,2,0,1,frame,5,scratch,8));
    assert(!EspUacBmp32_blendRow(&info,readAt,&b,0,0,3,frame,5,scratch,8));
    assert(!EspUacBmp32_blendRow(&info,readAt,&b,0,0,1,frame,5,scratch,7));
    b.fail=1;
    assert(!EspUacBmp32_blendRow(&info,readAt,&b,0,0,1,frame,5,scratch,8));
    b.fail=0;
    uint8_t h=b.bytes[0];b.bytes[0]='?';
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));
    b.bytes[0]=h;
    h=b.bytes[28];b.bytes[28]=24;
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));b.bytes[28]=h;
    h=b.bytes[30];b.bytes[30]=0;
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));b.bytes[30]=h;
    h=b.bytes[54];b.bytes[54]=1;
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));b.bytes[54]=h;
    h=b.bytes[10];b.bytes[10]=54;
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));b.bytes[10]=h;
    put32(b.bytes+22,0xffffffffU); /* negative/top-down unsupported */
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));
    put32(b.bytes+22,2);
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length-1,2,2));
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,158,240));
    put32(b.bytes+2,0xffffffffU);
    assert(!EspUacBmp32_open(&info,readAt,&b,b.length,2,2));
    destroy(&b);
}
/* Actual decoder, bottom-up batch vs direct, alpha and scale 1/2. */
static void batches(void){
    Blob b=make(6,8);
    EspUacBmp32 bmp;
    uint8_t storage[4*6*4],directScratch[6*4],batchScratch[6*4];
    for(int y=0;y<8;++y)for(int x=0;x<6;++x)
        pixel(&b,x,y,(uint8_t)(x*31),(uint8_t)(y*27),
              (uint8_t)(x+y*15),(uint8_t)((x+y)%3==0?0:((x+y)%3==1?128:255)));
    assert(EspUacBmp32_open(&bmp,readAt,&b,b.length,6,8));
    for(int first=0;first<8;first+=4){
        EspUacBmpRowBatch batch={0};
        const unsigned before=b.reads;
        assert(EspUacBmp32_loadBatch(&bmp,readAt,&b,(uint16_t)first,4,
                                     storage,sizeof(storage),&batch));
        assert(b.reads==before+1);
        for(int y=first;y<first+4;++y)for(int scale=1;scale<=2;++scale){
            uint16_t direct[16],cached[16];
            for(int x=0;x<16;++x)direct[x]=cached[x]=0x39e7;
            assert(EspUacBmp32_blendRow(&bmp,readAt,&b,(uint16_t)y,
                                        1,(uint8_t)scale,direct,16,
                                        directScratch,sizeof(directScratch)));
            const unsigned noSD=b.reads;
            assert(EspUacBmp32_blendRow(&bmp,EspUacBmp32_readBatch,&batch,
                                        (uint16_t)y,1,(uint8_t)scale,cached,16,
                                        batchScratch,sizeof(batchScratch)));
            assert(noSD==b.reads);
            assert(!memcmp(direct,cached,sizeof(direct)));
        }
        uint8_t scratch[6*4];
        assert(!EspUacBmp32_readBatch(&batch,batch.fileOffset-1U,scratch,1));
        assert(!EspUacBmp32_readBatch(&batch,batch.fileOffset+batch.size-1U,scratch,2));
        assert(!EspUacBmp32_loadBatch(&bmp,readAt,&b,(uint16_t)first,4,
                                      storage,sizeof(storage)-1U,&batch));
        assert(batch.bytes==NULL&&batch.size==0U);
    }
    EspUacBmpRowBatch failed={0};
    assert(!EspUacBmp32_loadBatch(&bmp,readAt,&b,7,2,storage,sizeof(storage),&failed));
    assert(!EspUacBmp32_loadBatch(&bmp,readAt,&b,8,1,storage,sizeof(storage),&failed));
    assert(!EspUacBmp32_loadBatch(&bmp,readAt,&b,0,0,storage,sizeof(storage),&failed));
    b.fail=1;
    assert(!EspUacBmp32_loadBatch(&bmp,readAt,&b,0,4,storage,sizeof(storage),&failed));
    assert(failed.bytes==NULL);
    destroy(&b);
}
/* Patch x=6..11 must be byte-identical to the full compositor
 * for both direct 240-scale and doubled 120-scale, including frame alpha. */
static void patchParity(void){
    Blob frame=make(6,8),button=make(2,8);
    EspUacBmp32 fdesc,bdesc;
    uint8_t fscratch[6*4],bscratch[2*4];
    for(int y=0;y<8;++y){
        for(int x=0;x<6;++x)
            pixel(&frame,x,y,(uint8_t)(x*18),(uint8_t)(y*25),
                  (uint8_t)(180+x),(uint8_t)((x+y)%3==0?0:128));
        for(int x=0;x<2;++x)
            pixel(&button,x,y,40,80,235,(uint8_t)(x==0?255:128));
    }
    assert(EspUacBmp32_open(&fdesc,readAt,&frame,frame.length,6,8));
    assert(EspUacBmp32_open(&bdesc,readAt,&button,button.length,2,8));
    for(int y=0;y<8;++y)for(int scale=1;scale<=2;++scale){
        uint16_t whole[20],part[10];
        for(int x=0;x<20;++x)whole[x]=0x1925;
        for(int x=0;x<10;++x)part[x]=0x1925;
        assert(EspUacBmp32_blendRow(&bdesc,readAt,&button,(uint16_t)y,
                                    6,(uint8_t)scale,whole,20,
                                    bscratch,sizeof(bscratch)));
        assert(EspUacBmp32_blendRow(&fdesc,readAt,&frame,(uint16_t)y,
                                    4,(uint8_t)scale,whole,20,
                                    fscratch,sizeof(fscratch)));
        assert(EspUacBmp32_blendRow(&bdesc,readAt,&button,(uint16_t)y,
                                    0,(uint8_t)scale,part,10,
                                    bscratch,sizeof(bscratch)));
        assert(EspUacBmp32_blendRow(&fdesc,readAt,&frame,(uint16_t)y,
                                    -2,(uint8_t)scale,part,10,
                                    fscratch,sizeof(fscratch)));
        for(int x=0;x<10;++x)assert(part[x]==whole[6+x]);
    }
    destroy(&frame);destroy(&button);
}
/* 240 virtual-120 must be bit-exact to a 120 BMP generated with
 * the top-left source texel; deliberately different odd source texels. */
static void decimationParity(void){
    Blob hi=make(6,8),lo=make(3,4);
    EspUacBmp32 hiBmp,loBmp;
    uint8_t hiScratch[24],loScratch[12],batchRows[24];
    for(int y=0;y<8;++y)for(int x=0;x<6;++x){
        uint8_t b=(uint8_t)(x*37+y),g=(uint8_t)(y*21+x);
        uint8_t r=(uint8_t)(y*13+x*29);
        uint8_t a=(uint8_t)((x+y)%3==0?0:((x+y)%3==1?128:255));
        pixel(&hi,x,y,b,g,r,a);
        if(!(x&1)&&!(y&1))pixel(&lo,x/2,y/2,b,g,r,a);
    }
    assert(EspUacBmp32_open(&hiBmp,readAt,&hi,hi.length,6,8));
    assert(EspUacBmp32_open(&loBmp,readAt,&lo,lo.length,3,4));
    for(int y=0;y<4;++y){
        uint16_t ref[18],out[18],cached[18];
        for(int x=0;x<18;++x)
            ref[x]=out[x]=cached[x]=(uint16_t)(0x2925+x*33);
        assert(EspUacBmp32_blendRow(&loBmp,readAt,&lo,(uint16_t)y,
                                    -1,2,ref,18,loScratch,sizeof(loScratch)));
        assert(EspUacBmp32_blendRow240As120(&hiBmp,readAt,&hi,(uint16_t)y,
                                    -1,2,out,18,hiScratch,sizeof(hiScratch)));
        assert(!memcmp(ref,out,sizeof(ref)));
        EspUacBmpRowBatch batch={0};
        assert(EspUacBmp32_loadBatch(&hiBmp,readAt,&hi,(uint16_t)(2*y),
                                    1,batchRows,sizeof(batchRows),&batch));
        const unsigned reads=hi.reads;
        assert(EspUacBmp32_blendRow240As120(&hiBmp,EspUacBmp32_readBatch,
                                    &batch,(uint16_t)y,-1,2,cached,18,
                                    hiScratch,sizeof(hiScratch)));
        assert(hi.reads==reads&&!memcmp(ref,cached,sizeof(ref)));
    }
    uint16_t out[12]={0};
    assert(!EspUacBmp32_blendRow240As120(&hiBmp,readAt,&hi,4,0,2,
                                         out,12,hiScratch,sizeof(hiScratch)));
    assert(!EspUacBmp32_blendRow240As120(&hiBmp,readAt,&hi,0,0,1,
                                         out,12,hiScratch,sizeof(hiScratch)));
    Blob odd=make(5,8);EspUacBmp32 oddBmp;
    assert(EspUacBmp32_open(&oddBmp,readAt,&odd,odd.length,5,8));
    assert(!EspUacBmp32_blendRow240As120(&oddBmp,readAt,&odd,0,0,2,
                                         out,12,hiScratch,sizeof(hiScratch)));
    assert(!EspUacBmp32_blendRow240As120(&hiBmp,readAt,&hi,0,0,2,
                                         out,12,hiScratch,sizeof(hiScratch)-1));
    destroy(&odd);destroy(&hi);destroy(&lo);
}
static void geometry(void){
    const int tops[4]={20,74,128,182};
    for(int i=0;i<4;++i){
        assert(EspUacMenu_hitPhysical(186,tops[i])==i);
        assert(EspUacMenu_hitPhysical(295,tops[i]+39)==i);
        assert(EspUacMenu_hitPhysical(185,tops[i])==-1);
        assert(EspUacMenu_hitPhysical(296,tops[i])==-1);
        assert(EspUacMenu_hitPhysical(200,tops[i]+40)==-1);
    }
    assert(EspUacMenu_hitPhysical(-1,20)==-1);
    assert(EspUacMenu_hitPhysical(200,240)==-1);
}
/* Same 9-file preflight acquire/release discipline as C++ SD adapter.
 * Failure at any one slot must never leave a file open. */
static void failures(void){
    unsigned opens,closes;
    for(int missing=-1;missing<9;++missing){
        opens=closes=0;
        for(int i=0;i<9;++i){
            if(i==missing)break;
            Blob b=make(i==0?79:55,i==0?120:20);
            ++opens;
            EspUacBmp32 desc;
            assert(EspUacBmp32_open(&desc,readAt,&b,b.length,
                                     i==0?79:55,i==0?120:20));
            destroy(&b);++closes;
        }
        assert(opens==closes);
        if(missing==-1)assert(opens==9);
        else assert(opens==(unsigned)missing);
    }
    Blob large=make(158,240);EspUacBmp32 info;
    assert(EspUacBmp32_open(&info,readAt,&large,large.length,158,240));
    destroy(&large);
    Blob button=make(110,40);
    assert(EspUacBmp32_open(&info,readAt,&button,button.length,110,40));
    destroy(&button);
}
int main(void){
    parser();batches();patchParity();decimationParity();geometry();failures();
    puts("UAC-P1 PASS real BMP parser: invalid V4/masks/extent/alpha/clip/SD short read/touch/resource lifetime/row batches/patch parity/240-as-120 parity");
    return 0;
}
