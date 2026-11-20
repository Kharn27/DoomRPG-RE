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
/* Normal menu: native 240 assets; only the procedural original is fallback.
 * No logo switch or experimental 120 modes. Keep exact V4 BGRA32 preflight. */
enum { kClassic=0,kUac240=3 };
constexpr bool kStrictSha256=false;
int mode=kClassic;
bool activationPending=false; /* no heap/SD work inside MENU_MAIN owner's check */
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

/* One BRIGHT RGB565 110x40 patch per labelled button. Inactive patches
 * are derived by RGB565 dimming; no extra SD reads or second-state cache.
 * 4x8800=35200B vs 70400B, still with a 20KB heap8 guard. */
constexpr uint32_t kCacheSlotBytes=(uint32_t)ESP_UAC_BUTTON_WIDTH*40U*2U;
constexpr uint32_t kCacheReserve8=20U*1024U;
static uint16_t* cachePixels[4]={};
static uint8_t cacheReady[4]={};
static unsigned cacheSlots=0,cacheBytes=0;
static int cacheMode=0;

static void freeCache(const char* reason) {
    const unsigned bytes=cacheBytes,slots=cacheSlots;
    const int previous=cacheMode;
    const uint32_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for(int i=0;i<4;++i){
        if(cachePixels[i])heap_caps_free(cachePixels[i]);
        cachePixels[i]=nullptr;
        cacheReady[i]=0;
    }
    cacheMode=0;cacheSlots=0;cacheBytes=0;
    if(slots)Serial.printf(
        "[UAC] CACHE FREE reason=%s mode=%d slots=%u bytes=%u heap8=%lu->%lu largest8=%lu\n",
        reason,previous,slots,bytes,
        (unsigned long)before,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void allocateCache(int m){
    cacheMode=m;
    const uint32_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for(int i=0;i<4;++i){
        const uint32_t free8=heap_caps_get_free_size(MALLOC_CAP_8BIT);
        const uint32_t largest8=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if(free8<kCacheSlotBytes+kCacheReserve8||largest8<kCacheSlotBytes)
            continue;
        uint16_t* ptr=(uint16_t*)heap_caps_malloc(kCacheSlotBytes,MALLOC_CAP_8BIT);
        if(!ptr)continue;
        cachePixels[i]=ptr;
        ++cacheSlots;
        cacheBytes+=(unsigned)kCacheSlotBytes;
    }
    Serial.printf(
        "[UAC] CACHE ALLOC mode=%d slots=%u/4 bytes=%u reserve8=%lu heap8=%lu->%lu largest8=%lu fallback=SD\n",
        m,cacheSlots,cacheBytes,(unsigned long)kCacheReserve8,
        (unsigned long)before,
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

/* Dim after preserving the bright composite. Integer RGB565 channel math
 * produces exactly the same result for cold and cached presentation. */
static uint16_t dimRgb565(uint16_t pixel){
    const uint16_t r=(uint16_t)((((pixel>>11)&31U)*5U)>>3);
    const uint16_t g=(uint16_t)((((pixel>>5)&63U)*5U)>>3);
    const uint16_t b=(uint16_t)(((pixel&31U)*5U)>>3);
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static bool renderCachedSelection(TFT_eSPI* lcd,int m,int selected,int previous,
                                  DrawStats* stats){
    if(!lcd||!stats||m!=cacheMode||selected<0||selected>=4||
       previous<0||previous>=4||previous==selected||
       !cacheReady[previous]||!cacheReady[selected]||
       !cachePixels[previous]||!cachePixels[selected])return false;
    uint16_t inactiveRow[ESP_UAC_BUTTON_WIDTH]; /* 220B bounded stack */
    const uint32_t t=micros();
    lcd->startWrite();
    const int targets[2]={previous,selected};
    for(int k=0;k<2;++k){
        const int item=targets[k];
        const uint16_t* pixels=cachePixels[item];
        lcd->setAddrWindow(kPanelX+ESP_UAC_BUTTON_INSET,kTop[item],
                           ESP_UAC_BUTTON_WIDTH,40);
        for(int row=0;row<40;++row){
            const uint16_t* bright=pixels+row*ESP_UAC_BUTTON_WIDTH;
            if(k==0){
                for(int x=0;x<ESP_UAC_BUTTON_WIDTH;++x)
                    inactiveRow[x]=dimRgb565(bright[x]);
                lcd->pushPixels(inactiveRow,ESP_UAC_BUTTON_WIDTH);
            }else{
                lcd->pushPixels(const_cast<uint16_t*>(bright),
                                ESP_UAC_BUTTON_WIDTH);
            }
        }
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
static void framePath(char* p,size_t n){
    snprintf(p,n,"/doomrpg/img/menu_frame_240.bmp");
}
static void buttonPath(char* p,size_t n,int index,bool active){
    snprintf(p,n,"/doomrpg/img/menu_%s_%s_240.bmp",kButtons[index],
             active?"active":"inactive");
}
static bool preflight(void){
    OpenImage img{};
    char p[72];
    const uint16_t fw=118U,fh=240U,bw=110U,bh=40U;
    framePath(p,sizeof(p));
    bool valid=openBmp(img,p,fw,fh);
    if(valid&&kStrictSha256)valid=verifySha256(img.file,kSha256[1][0]);
    if(img.file)img.file.close();
    (void)EspEntranceMusic_pumpMenuPreview();
    if(!valid){Serial.printf("[UAC] PREFLIGHT FAIL %s\n",p);return false;}
    for(int i=0;i<4;++i){
        buttonPath(p,sizeof(p),i,true);
        valid=openBmp(img,p,bw,bh);
        if(valid&&kStrictSha256)valid=verifySha256(img.file,kSha256[1][2+i*2]);
        if(img.file)img.file.close();
        (void)EspEntranceMusic_pumpMenuPreview();
        if(!valid){Serial.printf("[UAC] PREFLIGHT FAIL %s\n",p);return false;}
    }
    Serial.printf("[UAC] PREFLIGHT OK mode=3 tier=240 integrity=%s bmpV4=valid\n",
                  kStrictSha256?"strict-sha256":"structure+dimensions");
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
    const int scale=1; /* Only direct physical pixels survive this milestone. */
    const int sourceStep=1;
    const auto blendRow=EspUacBmp32_blendRow;
    const uint16_t fw=118U,fh=240U,bw=110U,bh=40U;
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
                if(cachePixels[warmTarget])cacheReady[warmTarget]=1;
            }else if(patch){
                if(cachePixels[previous])cacheReady[previous]=1;
                if(cachePixels[selected])cacheReady[selected]=1;
            }else{
                for(int i=0;i<4;++i)
                    if(cachePixels[i])cacheReady[i]=1;
            }
        }
        return ok;
    };
    framePath(path,sizeof(path));
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
                    buttonPath(path,sizeof(path),active,true);
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
                if(active>=0){
                    uint16_t* bright=out+(patch?0:kPanelX+ESP_UAC_BUTTON_INSET);
                    /* The bezel is already composited. Save the BRIGHT
                     * surface before applying the software inactive tint. */
                    if(cacheMode==m&&cachePixels[active]){
                        uint16_t* dst=cachePixels[active];
                        memcpy(dst+(py-kTop[active])*ESP_UAC_BUTTON_WIDTH,
                               bright,ESP_UAC_BUTTON_WIDTH*sizeof(uint16_t));
                    }
                    if(selected!=active){
                        for(int x=0;x<ESP_UAC_BUTTON_WIDTH;++x)
                            bright[x]=dimRgb565(bright[x]);
                    }
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

/* FULL already captures all four BRIGHT states. Only a missing slot may
 * need an auxiliary decode; no alternate/INACTIVE warmup remains. */
static bool warmCache(const uint16_t* fb,uint16_t stride,int m){
    if(cacheMode!=m||cacheSlots==0)return true;
    const uint32_t begin=millis();
    unsigned filled=0,reads=0;
    for(int item=0;item<4;++item){
        if(!cachePixels[item]||cacheReady[item])continue;
        DrawStats stats{};
        if(!draw(fb,stride,nullptr,m,item,-1,&stats,item))
            return false;  /* SD failure is still fail-closed */
        ++filled;
        reads+=stats.batches;
    }
    unsigned ready=0;
    for(int i=0;i<4;++i)ready+=cacheReady[i]?1U:0U;
    Serial.printf(
        "[UAC] CACHE WARM mode=%d filled=%u ready=%u/%u batches=%u ms=%lu heap8=%lu largest8=%lu\n",
        m,filled,ready,cacheSlots,reads,(unsigned long)(millis()-begin),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return true;
}
} // namespace

/* Registration into MENU_MAIN MUST be allocation-/SD-free. The software
 * compositor verifies its framebuffer hashes AND identical heap8 before it
 * makes the first physical present(). Defer every UAC SD check/allocation
 * to that present() or Options->Back will fail even with valid bitmaps. */
extern "C" int EspUacMenuPreview_mode(void){
    return mode==kUac240||activationPending?kUac240:kClassic;
}
extern "C" void EspUacMenuPreview_reset(void){
    if(mode||activationPending)
        Serial.printf("[UAC] RESET mode=%d pending=%u fallback=classic\n",
                      mode,activationPending?1U:0U);
    activationPending=false;
    freeCache("menu-exit");
    mode=kClassic;paintedMode=kClassic;paintedSelection=-1;
}
extern "C" int EspUacMenuPreview_activate(void){
    if(mode==kUac240)return 1;
    activationPending=true; /* never preflight or malloc here */
    return 1;
}
static bool activateOnFirstPresent(void){
    if(!activationPending)return mode==kUac240;
    activationPending=false;
    freeCache("menu-activate");
    if(!preflight()){
        mode=kClassic;paintedMode=kClassic;paintedSelection=-1;
        Serial.println("[UAC] FALLBACK SD missing/invalid -> original MENU_MAIN");
        return false;
    }
    mode=kUac240;
    paintedMode=kClassic;paintedSelection=-1;
    allocateCache(mode);
    Serial.printf("[UAC] MODE 240 source=118x240 DIRECT physical=320x240 buttons=110x40 RAM=6680B-BSS default=yes\n");
    return true;
}
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride){
    if(!lcd||!fb||stride<160U)return false;
    if(activationPending&&!activateOnFirstPresent())return false;
    const int current=mode;
    if(!current)return true;
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
