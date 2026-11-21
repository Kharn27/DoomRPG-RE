#include "esp_uac_menu_preview.h"
#include <Arduino.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include "esp_uac_bmp32.h"
#include "esp_audio_entrance_music.h"
#include "esp_asset_pack.h"
#include "esp_native_font_sd.h"
#include "esp_native_indexed_bmp.h"

namespace {
constexpr int kPanelX=ESP_UAC_PANEL_LEFT,kWidth=320,kHeight=240;
static_assert(ESP_UAC_PANEL_LEFT+ESP_UAC_PANEL_WIDTH==kWidth,
              "panel must reach physical display right edge");
static_assert(ESP_UAC_PANEL_WIDTH==ESP_UAC_BUTTON_WIDTH+2*ESP_UAC_BUTTON_INSET,
              "button bezel must be symmetric");
constexpr uint16_t kMatte=0x1925U;
const char* const kButtons[4]={"start","load","options","help"};
/* The 120-only experiment draws actual Doom indexed palette glyphs, never
 * a fake solid-white alpha/luminance font. Each nibble is a canonical
 * a.bmp color index (0 transparent); 12 glyphs use one bounded 1428B
 * temporary UI cache, released on mode exit/reset, no gameplay BSS. */
constexpr char kFontAlphabet[]="ADEHILNOPRST";
constexpr int kGlyphW=13,kGlyphH=17,kFontAdvance=13;
/* Canonical a.bmp RGB565 palette, permuted into larger_font index order.
 * Slot 0 is transparent, slots 1,2 are source indices 0,1; slots 3..15
 * equal the corresponding larger_font indices (source index 2 is magenta).
 * Keep in sync with artwork/menu-uac/generic/tools/repair_assets.py. */
constexpr uint16_t kDoomFontInk[16]={
    0x0000U,0x0000U,0xffffU,0x0862U,
    0x5902U,0x31a8U,0xa9a1U,0x5b4fU,
    0x8cd5U,0xf322U,0xb63aU,0xc554U,
    0xdf3dU,0xfd44U,0xff51U,0xf7beU
};
struct FontInkCache {
    uint8_t glyph[sizeof(kFontAlphabet)-1][kGlyphH][(kGlyphW+1)/2];
};
static FontInkCache* fontInk=nullptr;
static bool fontReady=false;
static void releaseLargerFont(){
    free(fontInk);
    fontInk=nullptr;
    fontReady=false;
}
static const char* const kGeneric120="/doomrpg/img/menu_generic_120.bmp";
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
/* Four physical rows per TFT write: 6680B static scanout; font ink is temporary heap. 
 * Only frame plus current button can have live BMP handles. */
constexpr int kRows=4;
static uint16_t outRows[kRows*kWidth];
static uint8_t frameRows[kRows*118*4];
static uint8_t buttonRows[kRows*110*4];
static uint8_t pixels[118*4];
struct DrawStats {uint32_t sdUs,lcdUs;unsigned batches,buttonOpens,rowsSent,audioPumps,audioBlocks;};
struct OpenImage { File file; EspUacBmp32 bmp; };

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
    if(m==kNative120){
        snprintf(p,n,"%s",kGeneric120);
        return;
    }
    snprintf(p,n,"/doomrpg/img/menu_%s_%s_%d.bmp",kButtons[index],
             active?"active":"inactive",assetTier(m));
}

/* Select the existing SD-indexed font owner WITHOUT opening the PAK first:
 * a physical SD PAK lease would prohibit the preferred SD font source.
 * If absent/malformed, open a transient native PAK lease, decode the same
 * original 208x102 indexed atlas, then close it before any BMP/TFT paint.
 * No ZIP, font-sized retained buffer, or PAK lease held during music.
 * The resulting compact ink cache is the ONLY retained font state here. */
static bool prepareLargerFont(){
    releaseLargerFont();
    fontInk=static_cast<FontInkCache*>(calloc(1U,sizeof(FontInkCache)));
    if(!fontInk){
        Serial.println("[UAC] FONT120 FAIL reason=bounded-cache-alloc");
        return false;
    }
    EspNativeIndexedBmp bmp{};
    EspNativeIndexedBmpStats stats{};
    bool temporaryPak=false;
    EspNativeIndexedBmpStatus st=EspNativeIndexedBmp_openSdFontStandalone(
        "larger_font.bmp",&bmp,&stats);

    /* Blit actual indexed glyph pixels via the existing bounded decoder;
     * retain the palette INDEX, not RGB luminance, so colors and black
     * outlines match a.bmp. The same mapping works with the old PAK atlas
     * and the new recolored SD atlas (the indices have not changed). */
    auto decode=[&]()->bool {
        if(st!=ESP_NATIVE_INDEXED_BMP_OK ||
           bmp.width!=208U || bmp.height!=102U)return false;
        for(size_t i=0;i<sizeof(kFontAlphabet)-1;++i){
            const unsigned ch=(unsigned)kFontAlphabet[i]-33U;
            uint16_t pixels[kGlyphW*kGlyphH];
            for(auto& pixel:pixels)pixel=0xf81fU;
            if(EspNativeIndexedBmp_blit(&bmp,pixels,kGlyphW,kGlyphH,
                   (uint16_t)((ch&15U)*kGlyphW),
                   (uint16_t)((ch>>4U)*kGlyphH),
                   kGlyphW,kGlyphH,0,0,1,&stats)!=ESP_NATIVE_INDEXED_BMP_OK)
                return false;
            for(int y=0;y<kGlyphH;++y){
                for(int x=0;x<kGlyphW;++x){
                    uint8_t ink=0;
                    const uint16_t p=pixels[y*kGlyphW+x];
                    if(p!=0xf81fU){
                        uint16_t sourceIndex=16U;
                        for(uint16_t j=0U;j<bmp.paletteCount&&j<16U;++j){
                            if(p==bmp.paletteRgb565[j]){sourceIndex=j;break;}
                        }
                        if(sourceIndex>=16U || sourceIndex==2U)
                            return false; /* unexpected/ambiguous palette */
                        ink=(uint8_t)(sourceIndex<2U?sourceIndex+1U:sourceIndex);
                    }
                    uint8_t& pair=fontInk->glyph[i][y][x/2];
                    if((x&1)==0)pair=(uint8_t)((pair&0x0fU)|(ink<<4));
                    else pair=(uint8_t)((pair&0xf0U)|ink);
                }
            }
        }
        return true;
    };
    bool ok=decode();
    if(!ok){
        /* SD may pass its header/palette preflight then fail mid-glyph.
         * Reject THAT source and retry the entire ink cache from PAK. */
        if(bmp.sdFontSource!=0U)EspNativeFontSd_reject(bmp.sdFontSource);
        if(!EspAssetPack_isOpen()){
            if(EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))
                temporaryPak=true;
            else
                Serial.println("[UAC] FONT PAK fallback unavailable");
        }
        if(EspAssetPack_isOpen()){
            st=EspNativeIndexedBmp_open("larger_font.bmp",&bmp,&stats);
            ok=decode();
        }
    }
    const bool fromSD=ok && bmp.sdFontSource!=0U;
    if(temporaryPak)EspAssetPack_close();
    EspNativeFontSd_suspend();
    if(!ok)releaseLargerFont();
    else fontReady=true;
    Serial.printf("[UAC] FONT120 %s name=larger_font.bmp backing=%s palette=a.bmp-indexed glyphs=%u inkBytes=%u reads=%lu bytes=%lu\n",
                  ok?"READY":"FAIL",fromSD?"SD":"PAK",
                  (unsigned)(sizeof(kFontAlphabet)-1),(unsigned)sizeof(FontInkCache),
                  (unsigned long)stats.packReads,(unsigned long)stats.bytesRead);
    return ok;
}

/* Physical 1:1 larger_font (13x17), optically centered on a 110x40 plate.
 * Render each true Doom palette index with the same 16 a.bmp colors.
 * Inactive labels use the same palette gently dimmed; never paint
 * background/magenta or overwrite the top/bottom metal bezel. */
static void paintLargerTextRow(uint16_t* dst,int py,int index,int selected,
                              bool patch){
    if(!fontReady||index<0||index>=4)return;
    const int gy=py-kTop[index]-(40-kGlyphH)/2;
    if(gy<0||gy>=kGlyphH)return;
    const char* label=kButtons[index];
    const int len=(int)strlen(label);
    const int xStart=(ESP_UAC_BUTTON_WIDTH-
                      ((len-1)*kFontAdvance+kGlyphW))/2;
    const int destOrigin=patch?0:kPanelX+ESP_UAC_BUTTON_INSET;
    for(int i=0;i<len;++i){
        const char ch=(char)(label[i]-'a'+'A');
        const char* found=strchr(kFontAlphabet,ch);
        if(!found)continue; /* No unsupported glyph can index outside cache. */
        const unsigned gi=(unsigned)(found-kFontAlphabet);
        for(int x=0;x<kGlyphW;++x){
            const uint8_t pair=fontInk->glyph[gi][gy][x/2];
            const unsigned ink=(x&1)?(pair&15U):(pair>>4U);
            if(ink==0U)continue; /* atlas transparent magenta */
            const int dx=destOrigin+xStart+i*kFontAdvance+x;
            if(dx<0||dx>=(patch?ESP_UAC_BUTTON_WIDTH:kWidth))continue;
            uint16_t color=kDoomFontInk[ink];
            if(index!=selected){
                /* 75% channel intensity, preserving hue and Doom outlines. */
                const unsigned r=((color>>11U)&31U)*3U/4U;
                const unsigned g=((color>>5U)&63U)*3U/4U;
                const unsigned b=(color&31U)*3U/4U;
                color=(uint16_t)((r<<11U)|(g<<5U)|b);
            }
            dst[dx]=color;
        }
    }
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
    if(m==kNative120){
        if(!openBmp(img,kGeneric120,55U,20U)){
            Serial.printf("[UAC] PREFLIGHT FAIL %s\n",kGeneric120);
            if(img.file)img.file.close();
            return false;
        }
        if(img.file)img.file.close();
        if(!prepareLargerFont())return false;
        Serial.println("[UAC] PREFLIGHT OK mode=1 tier=120 generic=1 font=larger SD-or-PAK bmpV4=valid");
        return true;
    }
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
                 int m,int selected,int previous,DrawStats* stats){
    OpenImage frame{},button{};
    char path[72];
    const int scale=m==kDirect240?1:2;
    const int sourceStep=m==k240As120?2:1;
    const auto blendRow=m==k240As120?
        EspUacBmp32_blendRow240As120:EspUacBmp32_blendRow;
    const uint16_t fw=m==1?59U:118U,fh=m==1?120U:240U;
    const uint16_t bw=m==1?55U:110U,bh=m==1?20U:40U;
    const bool patch=previous>=0&&previous<4&&previous!=selected;
    const int regions=patch?2:1;
    int openedButton=-1;
    auto finish=[&](bool ok,const char* stage)->bool{
        if(!ok)Serial.printf("[UAC] DRAW FAIL mode=%d stage=%s path=%s\n",m,stage,path);
        if(button.file)button.file.close();
        if(frame.file)frame.file.close();
        return ok;
    };
    framePath(path,sizeof(path),m);
    if(!openBmp(frame,path,fw,fh))return finish(false,"frame-open");
    for(int region=0;region<regions;++region){
        const int target=patch?(region==0?previous:selected):-1;
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
            if(m==kNative120){
                /* One generic SD handle for ALL four labels and both states. */
                if(active>=0&&!button.file){
                    buttonPath(path,sizeof(path),m,active,selected==active);
                    if(!openBmp(button,path,bw,bh))return finish(false,"button-open");
                    ++stats->buttonOpens;
                }
                openedButton=active;
            } else if(active!=openedButton){
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
                if(m==kNative120&&active>=0)
                    paintLargerTextRow(out,py,active,selected,patch);
            }
            /* The audio pump may read SD, so invoke it ONLY AFTER TFT
             * endWrite and after this strip's BMP reads have completed. */
            const uint32_t lcdStart=micros();
            lcd->startWrite();
            lcd->setAddrWindow(outputX,y,outputW,count);
            for(int row=0;row<count;++row)
                lcd->pushPixels(outRows+row*kWidth,outputW);
            lcd->endWrite();
            stats->lcdUs+=(uint32_t)(micros()-lcdStart);
            stats->rowsSent+=(unsigned)count;
            ++stats->batches;
            ++stats->audioPumps;
            stats->audioBlocks+=(unsigned)EspEntranceMusic_pumpMenuPreview();
            y+=count;
            if((y&15)==0)yield();
        }
    }
    return finish(true,"OK");
}
} // namespace

extern "C" int EspUacMenuPreview_mode(void){return mode;}
extern "C" void EspUacMenuPreview_reset(void){
    if(mode)Serial.printf("[UAC] RESET mode=%d fallback=classic\n",mode);
    mode=0;paintedMode=0;paintedSelection=-1; releaseLargerFont();
}
extern "C" int EspUacMenuPreview_cycle(void){
    const int next=(mode+1)%4;
    if(mode==kNative120 && next!=kNative120)releaseLargerFont();
    if(next!=0&&!preflight(next)){
        releaseLargerFont();
        mode=0;
        Serial.println("[UAC] FALLBACK SD missing/invalid -> original MENU_MAIN");
        return 0;
    }
    mode=next;
    paintedMode=0;paintedSelection=-1;
    Serial.printf("[UAC] MODE %s source=%s physical=320x240 buttons=110x40 RAM=6680B-BSS+1428B-menu-only\n",
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
    const bool ok=draw(fb,stride,lcd,current,sel,previous,&stats);
    if(ok){paintedMode=current;paintedSelection=sel;}
    Serial.printf("[UAC] PRESENT mode=%d selected=%d prior=%d paint=%s result=%s ms=%lu sdMs=%lu lcdMs=%lu batches=%u rows=%u buttonOpens=%u audioPumps=%u audioBlocks=%u heap=%u framebuffer=unchanged\n",
                  current,sel,previous,previous>=0?"BUTTONS":"FULL",
                  ok?"OK":"FAIL",static_cast<unsigned long>(millis()-start),
                  static_cast<unsigned long>(stats.sdUs/1000U),
                  static_cast<unsigned long>(stats.lcdUs/1000U),
                  stats.batches,stats.rowsSent,stats.buttonOpens,
                  stats.audioPumps,stats.audioBlocks,
                  static_cast<unsigned>(ESP.getFreeHeap()));
    return ok;
}
