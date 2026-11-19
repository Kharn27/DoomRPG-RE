#include "esp_uac_menu_preview.h"
#include <Arduino.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include "esp_uac_bmp32.h"
#include "esp_audio_entrance_music.h"

namespace {
constexpr int kPanelX=ESP_UAC_PANEL_LEFT,kWidth=320,kHeight=240;
static_assert(ESP_UAC_PANEL_LEFT+ESP_UAC_PANEL_WIDTH==kWidth,
              "panel must reach physical display right edge");
static_assert(ESP_UAC_PANEL_WIDTH==ESP_UAC_BUTTON_WIDTH+2*ESP_UAC_BUTTON_INSET,
              "button bezel must be symmetric");
constexpr uint16_t kMatte=0x1925U;
const char* const kButtons[4]={"start","load","options","help"};
const int kTop[4]={20,74,128,182};
/* Manifest from artwork/menu-uac/SHA256SUMS.txt (PR #399). Mode and
 * ordering: FRAME, START inactive/active, LOAD inactive/active,
 * OPTIONS inactive/active, HELP inactive/active. No SD-side manifest
 * dependency: corrupted payloads fail closed even at unchanged file size. */
const char* const kSha256[2][9] = {
  {
    "694e9ae29169adf731f85195bb581bff6f2b6cdb84b4a92feddd46c34f0ea260",
    "101b13334059f448586bf0e0e5cdc60d476d80380e3300142d2bf68a4499548e",
    "2a4967a80b7accb57e8481ebaaa27c86006fbff53cb512a96d8f7c9a1bef8a56",
    "37716377cd3bf9f1bd773835b49eab9c4296066d749be82343ad20ec31015a5b",
    "9fa5bf484ba8d3f213a9626454ee5596ad0c315587de87e322ad9edf3b1fad80",
    "0874fb1781b704d4e03fa0c6067cf84294de8a196d22a2a344ad68d8da566fb2",
    "8ad4bf03bae92326239ac44869b90ba9eb85a5ae1d2064d87849c7e5650df9aa",
    "c70c02fe8e8565ebc38460b924a8080d9c10f16057356faa74813d04a3936274",
    "3e07d8ea55520cbb47c6a73e5f01e0aac78a351738e7bac05afabacd1d86cc81"
  },
  {
    "343b29a61a665d4a92335bf0155d0686f807687fdeb2240776fa4b83ab7f58a7",
    "c70c56e7cf9f050c1eeb1f0698a96ecd1f1beace8499894807f94e3b55755319",
    "91fd666bde662870c6d1d3934158b0c535ca7919d3df19df7ff6a6704fde9c5a",
    "0b8e30a1e1dfcd48d2248d306bd111598404cecd417bbb832b06b67a55020062",
    "8913ded7eb69e951d6f4176418916b398e14fa6623c9af2651b586826d165abc",
    "af772df03205a770822434814e8940939cfbbe9f232208313c59c37ede6e4709",
    "c8b2bdd076218897c8159be80e7b54fc6e3548dce583955a4dead2d354900463",
    "a3aae4fb6f2f8de23ca4914110b980b1f2a12a6d47ede24786039f74a329d03d",
    "63054b67f2c7c6024e4e1392dd34cb165e074eef6f810b518fca0804ad9ecb55"
  }
};
/* Reversible comparison: 0 CLASSIC, 1 native-120, 2 sampled 240->120,
 * 3 direct-240. P1 accepts artist-updated SD BMP with strict V4 geometry. */
enum { kClassic=0,kNative120=1,k240As120=2,kDirect240=3 };
constexpr bool kStrictSha256=false;
static int assetTier(int m){return m==kNative120?120:240;}
int mode=kClassic;
int paintedMode=0,paintedSelection=-1;
/* Four physical rows per TFT write: 6680B BSS, no frame allocation.
 * Only frame plus current button can have live BMP handles. */
constexpr int kRows=4;
static uint16_t outRows[kRows*kWidth];
static uint8_t frameRows[kRows*118*4];
static uint8_t buttonRows[kRows*110*4];
static uint8_t pixels[118*4];
struct DrawStats {
    uint32_t sdUs,lcdUs;
    unsigned batches,buttonOpens,rowsSent,audioPumps,audioBlocks,cacheHits;
};
struct OpenImage { File file; EspUacBmp32 bmp; };

/* Menu-scoped hot RGB565 patches, two states per button, 110*40*2=8800B.
 * Eight independent 8-bit heap allocations avoid requiring a contiguous
 * 70.4KB block: the title music can leave <66KB as its largest free block.
 * Keep 20KB 8-bit heap headroom while music/SD is alive; missing cache
 * slots are allowed and use the original bounded SD paint path. */
constexpr uint32_t kCacheSlotBytes=(uint32_t)ESP_UAC_BUTTON_WIDTH*40U*2U;
constexpr uint32_t kCacheReserve8=20U*1024U;
static uint16_t* cachePixels[4][2]={};
static uint8_t cacheReady[4][2]={};
static unsigned cacheSlots=0,cacheBytes=0;
static int cacheMode=0;

static void freeCache(const char* reason) {
    const unsigned bytes=cacheBytes,slots=cacheSlots;
    const int previous=cacheMode;
    const uint32_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for(int i=0;i<4;++i)for(int s=0;s<2;++s){
        if(cachePixels[i][s])heap_caps_free(cachePixels[i][s]);
        cachePixels[i][s]=nullptr;
        cacheReady[i][s]=0;
    }
    cacheMode=0;cacheSlots=0;cacheBytes=0;
    if(slots)Serial.printf(
        "[UAC] CACHE FREE reason=%s mode=%d slots=%u bytes=%u heap8=%lu->%lu largest8=%lu\\n",
        reason,previous,slots,bytes,
        (unsigned long)before,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void allocateCache(int m){
    cacheMode=m;
    const uint32_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for(int s=0;s<2;++s)for(int i=0;i<4;++i){
        const uint32_t free8=heap_caps_get_free_size(MALLOC_CAP_8BIT);
        const uint32_t largest8=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if(free8<kCacheSlotBytes+kCacheReserve8||largest8<kCacheSlotBytes)
            continue;
        uint16_t* ptr=(uint16_t*)heap_caps_malloc(kCacheSlotBytes,MALLOC_CAP_8BIT);
        if(!ptr)continue;
        cachePixels[i][s]=ptr;
        ++cacheSlots;
        cacheBytes+=(unsigned)kCacheSlotBytes;
    }
    Serial.printf(
        "[UAC] CACHE ALLOC mode=%d slots=%u/8 bytes=%u reserve8=%lu heap8=%lu->%lu largest8=%lu fallback=SD\\n",
        m,cacheSlots,cacheBytes,(unsigned long)kCacheReserve8,
        (unsigned long)before,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

/* The cached patches are the exact LCD-bound output AFTER matte + active/
 * inactive button alpha + frame alpha. No SD or per-touch allocation. */
static bool renderCachedSelection(TFT_eSPI* lcd,int m,int selected,int previous,
                                  DrawStats* stats){
    if(!lcd||!stats||m!=cacheMode||selected<0||selected>=4||
       previous<0||previous>=4||previous==selected||
       !cacheReady[previous][0]||!cacheReady[selected][1])return false;
    const uint32_t t=micros();
    lcd->startWrite();
    const int targets[2]={previous,selected};
    const int states[2]={0,1};
    for(int k=0;k<2;++k){
        const int item=targets[k];
        const uint16_t* pixels=cachePixels[item][states[k]];
        if(!pixels){lcd->endWrite();return false;}
        lcd->setAddrWindow(kPanelX+ESP_UAC_BUTTON_INSET,kTop[item],
                           ESP_UAC_BUTTON_WIDTH,40);
        for(int row=0;row<40;++row)
            lcd->pushPixels(const_cast<uint16_t*>(
                pixels+row*ESP_UAC_BUTTON_WIDTH),ESP_UAC_BUTTON_WIDTH);
    }
    lcd->endWrite();
    stats->lcdUs+=(uint32_t)(micros()-t);
    stats->rowsSent+=80;stats->batches+=2;stats->cacheHits+=2;
    ++stats->audioPumps;
    stats->audioBlocks+=(unsigned)EspEntranceMusic_pumpMenuPreview();
    return true;
}

static int readAt(void* ctx,uint32_t offset,void* dst,size_t n){
    File* f=static_cast<File*>(ctx);
    return f&&*f&&dst&&offset<=f->size()&&n<=f->size()-offset&&
           f->seek(offset)&&f->read(static_cast<uint8_t*>(dst),n)==n;
}
static bool openBmp(OpenImage& i,const char* path,uint16_t w,uint16_t h){
    i.file=SD.open(path,FILE_READ);
    if(!i.file||i.file.isDirectory()||i.file.size()>UINT32_MAX)return false;
    return EspUacBmp32_open(&i.bmp,readAt,&i.file,
                            static_cast<uint32_t>(i.file.size()),w,h)!=0;
}
static int hexDigit(char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    return -1;
}
/* Fixed 256B stack chunk, no file/image allocation. Only at mode switch;
 * never hash during frame rendering or on the gameplay path. */
static bool verifySha256(File& file,const char* expected){
    uint8_t chunk[256];
    uint8_t digest[32];
    mbedtls_sha256_context state;
    mbedtls_sha256_init(&state);
    int result=mbedtls_sha256_starts_ret(&state,0);
    if(result==0&&!file.seek(0))result=-1;
    const size_t bytes=file.size();
    size_t done=0U;
    while(result==0&&done<bytes){
        const size_t n=bytes-done>sizeof(chunk)?sizeof(chunk):bytes-done;
        if(file.read(chunk,n)!=n)result=-1;
        else result=mbedtls_sha256_update_ret(&state,chunk,n);
        done+=n;
        if((done&4095U)==0U){
            (void)EspEntranceMusic_pumpMenuPreview();
            yield();
        }
    }
    if(result==0)result=mbedtls_sha256_finish_ret(&state,digest);
    mbedtls_sha256_free(&state);
    if(result!=0)return false;
    for(unsigned i=0;i<32U;++i){
        const int hi=hexDigit(expected[i*2U]);
        const int lo=hexDigit(expected[i*2U+1U]);
        if(hi<0||lo<0||digest[i]!=(uint8_t)((hi<<4)|lo))
            return false;
    }
    return true;
}
static void framePath(char* p,size_t n,int m){
    snprintf(p,n,"/doomrpg/img/menu_frame_%d.bmp",assetTier(m));
}
static void buttonPath(char* p,size_t n,int m,int index,bool active){
    snprintf(p,n,"/doomrpg/img/menu_%s_%s_%d.bmp",kButtons[index],
             active?"active":"inactive",assetTier(m));
}
static bool preflight(int m){
    OpenImage img{};
    char p[72];
    const uint16_t fw=m==1?59U:118U,fh=m==1?120U:240U;
    const uint16_t bw=m==1?55U:110U,bh=m==1?20U:40U;
    framePath(p,sizeof(p),m);
    bool valid=openBmp(img,p,fw,fh);
    if(valid&&kStrictSha256)valid=verifySha256(img.file,kSha256[assetTier(m)==120?0:1][0]);
    if(img.file)img.file.close();
    (void)EspEntranceMusic_pumpMenuPreview();
    if(!valid){Serial.printf("[UAC] PREFLIGHT FAIL %s\n",p);return false;}
    for(int i=0;i<4;++i)for(int s=0;s<2;++s){
        buttonPath(p,sizeof(p),m,i,s==1);
        valid=openBmp(img,p,bw,bh);
        if(valid&&kStrictSha256)valid=verifySha256(img.file,kSha256[assetTier(m)==120?0:1][1+i*2+s]);
        if(img.file)img.file.close();
        (void)EspEntranceMusic_pumpMenuPreview();
        if(!valid){Serial.printf("[UAC] PREFLIGHT FAIL %s\n",p);return false;}
    }
    Serial.printf("[UAC] PREFLIGHT OK mode=%d tier=%d integrity=%s bmpV4=valid\n",
                  m,assetTier(m),kStrictSha256?"strict-sha256":"structure+dimensions");
    return true;
}
/* Full paint only on mode entry. On selection changes the physical
 * difference is confined to two 110x40 button rectangles: the opaque matte,
 * button BMP and fixed frame overlay are reproduced pixel-for-pixel there.
 * Preserve the original 160x120 framebuffer and all native input owners. */
static bool draw(const uint16_t* fb,uint16_t stride,TFT_eSPI* lcd,
                 int m,int selected,int previous,DrawStats* stats,
                 int warmTarget=-1){
    OpenImage frame{},button{};
    char path[72];
    const int scale=m==kDirect240?1:2;
    const int sourceStep=m==k240As120?2:1;
    const auto blendRow=m==k240As120?
        EspUacBmp32_blendRow240As120:EspUacBmp32_blendRow;
    const uint16_t fw=m==1?59U:118U,fh=m==1?120U:240U;
    const uint16_t bw=m==1?55U:110U,bh=m==1?20U:40U;
    const bool warming=warmTarget>=0&&warmTarget<4;
    const bool patch=warming||(previous>=0&&previous<4&&previous!=selected);
    const int regions=warming?1:(patch?2:1);
    int openedButton=-1;
    auto finish=[&](bool ok,const char* stage)->bool{
        if(!ok)Serial.printf("[UAC] DRAW FAIL mode=%d stage=%s path=%s\n",m,stage,path);
        if(button.file)button.file.close();
        if(frame.file)frame.file.close();
        if(ok&&cacheMode==m){
            if(warming){
                if(cachePixels[warmTarget][selected==warmTarget])
                    cacheReady[warmTarget][selected==warmTarget]=1;
            }else if(patch){
                if(cachePixels[previous][0])cacheReady[previous][0]=1;
                if(cachePixels[selected][1])cacheReady[selected][1]=1;
            }else{
                for(int i=0;i<4;++i)
                    if(cachePixels[i][selected==i])
                        cacheReady[i][selected==i]=1;
            }
        }
        return ok;
    };
    framePath(path,sizeof(path),m);
    if(!openBmp(frame,path,fw,fh))return finish(false,"frame-open");
    for(int region=0;region<regions;++region){
        const int target=warming?warmTarget:
                         patch?(region==0?previous:selected):-1;
        const int firstY=target>=0?kTop[target]:0;
        const int finalY=target>=0?kTop[target]+40:kHeight;
        const int outputX=target>=0?kPanelX+ESP_UAC_BUTTON_INSET:0;
        const int outputW=target>=0?ESP_UAC_BUTTON_WIDTH:kWidth;
        for(int y=firstY;y<finalY;){
            int active=target,until=finalY;
            if(!patch){
                active=-1;
                for(int i=0;i<4;++i){
                    if(y<kTop[i]){until=kTop[i];break;}
                    if(y<kTop[i]+40){active=i;until=kTop[i]+40;break;}
                }
            }
            int count=kRows;
            if(count>until-y)count=until-y;
            if(count>finalY-y)count=finalY-y;
            if(count<=0)return finish(false,"row-boundary");
            const uint32_t sdStart=micros();
            if(active!=openedButton){
                if(button.file)button.file.close();
                openedButton=active;
                if(active>=0){
                    buttonPath(path,sizeof(path),m,active,selected==active);
                    if(!openBmp(button,path,bw,bh))return finish(false,"button-open");
                    ++stats->buttonOpens;
                }
            }
            const uint16_t first=(uint16_t)((y/scale)*sourceStep);
            const uint16_t last=(uint16_t)(((y+count-1)/scale)*sourceStep);
            EspUacBmpRowBatch frameBatch{},buttonBatch{};
            if(!EspUacBmp32_loadBatch(&frame.bmp,readAt,&frame.file,first,
                    (uint16_t)(last-first+1U),frameRows,sizeof(frameRows),
                    &frameBatch))return finish(false,"frame-batch");
            if(active>=0){
                const uint16_t bFirst=(uint16_t)(((y-kTop[active])/scale)*sourceStep);
                const uint16_t bLast=(uint16_t)(((y+count-1-kTop[active])/scale)*sourceStep);
                if(!EspUacBmp32_loadBatch(&button.bmp,readAt,&button.file,
                        bFirst,(uint16_t)(bLast-bFirst+1U),buttonRows,
                        sizeof(buttonRows),&buttonBatch))
                    return finish(false,"button-batch");
            }
            stats->sdUs+=(uint32_t)(micros()-sdStart);
            for(int row=0;row<count;++row){
                const int py=y+row;
                uint16_t* out=outRows+row*kWidth;
                if(!patch){
                    for(int x=0;x<kPanelX;++x)
                        out[x]=fb[(py/2)*stride+(x*102/kPanelX)];
                    for(int x=kPanelX;x<kWidth;++x)out[x]=kMatte;
                } else {
                    for(int x=0;x<outputW;++x)out[x]=kMatte;
                }
                if(active>=0&&!blendRow(&button.bmp,
                        EspUacBmp32_readBatch,&buttonBatch,
                        (uint16_t)((py-kTop[active])/scale),
                        patch?0:kPanelX+ESP_UAC_BUTTON_INSET,(uint8_t)scale,
                        out,(uint16_t)outputW,pixels,sizeof(pixels)))
                    return finish(false,"button-blend");
                if(!blendRow(&frame.bmp,
                        EspUacBmp32_readBatch,&frameBatch,(uint16_t)(py/scale),
                        patch?-ESP_UAC_BUTTON_INSET:kPanelX,(uint8_t)scale,
                        out,(uint16_t)outputW,pixels,
                        sizeof(pixels)))return finish(false,"frame-blend");
                if(active>=0&&cacheMode==m){
                    uint16_t* dst=cachePixels[active][selected==active];
                    if(dst)memcpy(dst+(py-kTop[active])*ESP_UAC_BUTTON_WIDTH,
                                  out+(patch?0:kPanelX+ESP_UAC_BUTTON_INSET),
                                  ESP_UAC_BUTTON_WIDTH*sizeof(uint16_t));
                }
            }
            /* Finish SD work before any TFT transaction or audio pump. In
             * warm-only mode the LCD keeps its current image untouched. */
            if(!warming){
                const uint32_t lcdStart=micros();
                lcd->startWrite();
                lcd->setAddrWindow(outputX,y,outputW,count);
                for(int row=0;row<count;++row)
                    lcd->pushPixels(outRows+row*kWidth,outputW);
                lcd->endWrite();
                stats->lcdUs+=(uint32_t)(micros()-lcdStart);
                stats->rowsSent+=(unsigned)count;
            }
            ++stats->batches;
            ++stats->audioPumps;
            stats->audioBlocks+=(unsigned)EspEntranceMusic_pumpMenuPreview();
            y+=count;
            if((y&15)==0)yield();
        }
    }
    return finish(true,"OK");
}

/* One-time warmup per mode: after the first FULL LCD paint, fill all
 * affordable alternate button states. Avoid returning to input while cache
 * is half-built; retain menu music service after each bounded SD strip. */
static bool warmCache(const uint16_t* fb,uint16_t stride,int m){
    if(cacheMode!=m||cacheSlots==0)return true;
    const uint32_t begin=millis();
    unsigned filled=0,reads=0;
    for(int item=0;item<4;++item)for(int state=0;state<2;++state){
        if(!cachePixels[item][state]||cacheReady[item][state])continue;
        DrawStats stats{};
        if(!draw(fb,stride,nullptr,m,state?item:-1,-1,&stats,item))
            return false;  /* SD failure is still fail-closed */
        ++filled;
        reads+=stats.batches;
    }
    unsigned ready=0;
    for(int i=0;i<4;++i)for(int s=0;s<2;++s)ready+=cacheReady[i][s]?1U:0U;
    Serial.printf(
        "[UAC] CACHE WARM mode=%d filled=%u ready=%u/%u batches=%u ms=%lu heap8=%lu largest8=%lu\\n",
        m,filled,ready,cacheSlots,reads,(unsigned long)(millis()-begin),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return true;
}
} // namespace

extern "C" int EspUacMenuPreview_mode(void){return mode;}
extern "C" void EspUacMenuPreview_reset(void){
    if(mode)Serial.printf("[UAC] RESET mode=%d fallback=classic\n",mode);
    freeCache("menu-exit");
    mode=0;paintedMode=0;paintedSelection=-1;
}
extern "C" int EspUacMenuPreview_cycle(void){
    const int next=(mode+1)%4;
    freeCache(next==0?"return-classic":"mode-change");
    if(next!=0&&!preflight(next)){
        mode=0;
        Serial.println("[UAC] FALLBACK SD missing/invalid -> original MENU_MAIN");
        return 0;
    }
    mode=next;
    paintedMode=0;paintedSelection=-1;
    if(mode!=kClassic)allocateCache(mode);
    Serial.printf("[UAC] MODE %s source=%s physical=320x240 buttons=110x40 RAM=6680B-BSS\n",
                  mode==kClassic?"CLASSIC":mode==kNative120?"120":
                    mode==k240As120?"240->120":"240",
                  mode==kClassic?"original":mode==kNative120?
                    "59x120 nearest-x2":mode==k240As120?
                    "118x240 decimate2x+nearest-x2":"118x240 DIRECT");
    return mode;
}
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride){
    const int current=mode;
    if(!current)return true;
    if(!lcd||!fb||stride<160U)return false;
    const int sel=EspUacMenuPreview_selectedIndex();
    if(sel<0||sel>3)return false;
    if(current==paintedMode && sel==paintedSelection)return true;
    const int previous=current==paintedMode?paintedSelection:-1;
    const uint32_t start=millis();
    DrawStats stats{};
    bool fromCache=false;
    if(previous>=0)
        fromCache=renderCachedSelection(lcd,current,sel,previous,&stats);
    const bool painted=fromCache||
                     draw(fb,stride,lcd,current,sel,previous,&stats);
    const uint32_t paintMs=millis()-start;
    bool warmed=true;
    if(painted&&previous<0)warmed=warmCache(fb,stride,current);
    const bool ok=painted&&warmed;
    if(ok){paintedMode=current;paintedSelection=sel;}
    Serial.printf("[UAC] PRESENT mode=%d selected=%d prior=%d paint=%s result=%s ms=%lu sdMs=%lu lcdMs=%lu batches=%u rows=%u buttonOpens=%u audioPumps=%u audioBlocks=%u cache=%s cacheSlots=%u cacheBytes=%u warmMs=%lu heap=%u heap8=%lu framebuffer=unchanged\n",
                  current,sel,previous,previous>=0?"BUTTONS":"FULL",
                  ok?"OK":"FAIL",static_cast<unsigned long>(paintMs),
                  static_cast<unsigned long>(stats.sdUs/1000U),
                  static_cast<unsigned long>(stats.lcdUs/1000U),
                  stats.batches,stats.rowsSent,stats.buttonOpens,
                  stats.audioPumps,stats.audioBlocks,
                  fromCache?"HIT":previous>=0?"MISS-SD":"INITIAL",
                  cacheSlots,cacheBytes,
                  static_cast<unsigned long>((millis()-start)-paintMs),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    return ok;
}
