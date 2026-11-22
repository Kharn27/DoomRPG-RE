#include "esp_uac_menu_preview.h"
#include <Arduino.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include <esp_heap_caps.h>
#include "esp_uac_bmp32.h"
#include "esp_audio_entrance_music.h"
#include "esp_asset_pack.h"
#include "esp_native_font_sd.h"
#include "esp_native_font_tint.h"
#include "esp_native_indexed_bmp.h"
#include "esp_main_menu_skin.h"
#include "esp_uac_font_theme.h"

namespace {
constexpr int kPanelX=ESP_UAC_PANEL_LEFT,kWidth=320,kHeight=240;
static_assert(ESP_UAC_PANEL_LEFT+ESP_UAC_PANEL_WIDTH==kWidth,
              "panel must reach physical display right edge");
static_assert(ESP_UAC_PANEL_WIDTH==ESP_UAC_BUTTON_WIDTH+2*ESP_UAC_BUTTON_INSET,
              "button bezel must be symmetric");
constexpr uint16_t kMatte=0x1925U;
const char* const kButtons[4]={"start","load","options","help"};
/* The 120-only renderer now reads the SAME 9x12 a.bmp indexed font
 * as HELP: one 25-glyph x 12-row x 5-byte cache (1500 B) serves MAIN and
 * OPTIONS/VIDEO/SOUND/presets, released on exit; no gameplay BSS. */
constexpr char kFontAlphabet[]="%01257ABCDEFHIKLMNOPRSTUV";
constexpr int kGlyphW=9,kGlyphH=12;
/* Pixel-faithful a.bmp (9x12 ASCII cells), sampled nearest into the
 * bounded 15x25 physical ink box. Existing centered text corridors remain
 * unchanged. Never load larger_font.bmp merely to paint menu buttons. */
constexpr int kPaintGlyphW=15,kPaintGlyphH=25,kPaintAdvance=13;
constexpr int kLabelYOffset=1; /* 8px top / 7px bottom for 25px labels */
static_assert(7*kPaintAdvance<=ESP_UAC_BUTTON_WIDTH-12,
              "OPTIONS must remain inside the generic button bezel");
static_assert(kPaintGlyphH<=40-12,
              "enlarged font must not overpaint the upper/lower bezel");
/* Every indexed nontransparent source shade is tinted at presentation. */
struct FontInkCache {
    uint8_t glyph[sizeof(kFontAlphabet)-1][kGlyphH][(kGlyphW+1)/2];
};
static FontInkCache* fontInk=nullptr;
static bool fontReady=false;
/* 16 RGB565 source shades (32 B) for the active SD atlas, never a second font. */
static uint16_t fontSourcePalette[16]={};
static bool fontIsNeutral=false;
static bool fontSourceIsSD=false;
static void releaseMenuFont(){
    free(fontInk);
    fontInk=nullptr;
    fontReady=false;
    fontIsNeutral=false;
    fontSourceIsSD=false;
}
/* Menu-only bounded BMP caches: original bottom-up V4 pixels, not a
 * framebuffer or desktop texture. 59x120x4 + 55x20x4 = 32720 heap bytes.
 * SD handles close before drawing; caches release before gameplay. */
struct BufferedBmp { EspUacBmp32 bmp; uint8_t* pixels; };
static BufferedBmp frameCache{},genericCache{};
static void releaseBufferedBmps(){
    free(frameCache.pixels);free(genericCache.pixels);
    frameCache=BufferedBmp{};genericCache=BufferedBmp{};
}
static void releaseMenuAssets(){releaseMenuFont();releaseBufferedBmps();}
static const char* const kGeneric120="/doomrpg/img/menu_generic_120.bmp";
const int kTop[4]={20,74,128,182};
/* Options subpages own presentation labels only: original UI model,
 * framebuffer hash oracles, actions and two-tap contracts remain untouched. */
static const char* const kOptionsRoot[]={"BACK","VIDEO","SOUND"};
static const char* const kOptionsVideo[]={"BACK","LUMINOSITE","ORIENTATION","THEME"};
static const char* const kOptionsBrightness[]={"BACK","25%","50%","75%","100%"};
static const char* const kOptionsVolume[]={"25%","50%","75%","100%"};
static const char* const kOptionsOrientation[]={"BACK","NORMAL","INVERSE"};
static const char* const kOptionsTheme[]={"BACK","CLASSIC","INFERNO","PHOSPHORE"};
static const char* const kOptionsSoundOn[]={"BACK","ON","VOLUME"};
static const char* const kOptionsSoundOff[]={"BACK","OFF","VOLUME"};
constexpr int kFiveButtonHeight=32;
static int optionsPage=ESP_UAC_OPTIONS_NONE;
static int optionsSelection=0,optionsArmed=0;
static uint8_t optionsEnabledMask=0x1fU;
static bool optionsPending=false;
static int paintedPage=ESP_UAC_OPTIONS_NONE,paintedArmed=0;
static uint8_t paintedEnabledMask=0U;
static uint8_t paintedTheme=0xffU;
constexpr uint32_t kBlinkHalfPeriodMs=450U;
static uint32_t blinkEpochMs=0U;
static bool paintedDark=false;
static const char* labelAt(int page,int index){
    switch(page){
        case ESP_UAC_OPTIONS_ROOT: return index<3?kOptionsRoot[index]:nullptr;
        case ESP_UAC_OPTIONS_VIDEO: return index<4?kOptionsVideo[index]:nullptr;
        case ESP_UAC_OPTIONS_BRIGHTNESS: return index<5?kOptionsBrightness[index]:nullptr;
        case ESP_UAC_OPTIONS_VOLUME: return index<4?kOptionsVolume[index]:nullptr;
        case ESP_UAC_OPTIONS_ORIENTATION: return index<3?kOptionsOrientation[index]:nullptr;
        case ESP_UAC_OPTIONS_THEME: return index<4?kOptionsTheme[index]:nullptr;
        case ESP_UAC_OPTIONS_SOUND_ON: return index<3?kOptionsSoundOn[index]:nullptr;
        case ESP_UAC_OPTIONS_SOUND_OFF: return index<3?kOptionsSoundOff[index]:nullptr;
        default: return index<4?kButtons[index]:nullptr;
    }
}
static int pageCount(int page){
    if(page==ESP_UAC_OPTIONS_VIDEO || page==ESP_UAC_OPTIONS_THEME ||
       page==ESP_UAC_OPTIONS_VOLUME)return 4;
    if(page==ESP_UAC_OPTIONS_BRIGHTNESS)return 5;
    return page==ESP_UAC_OPTIONS_NONE?4:3;
}
/* The frame contains FOUR fixed metal rails. Three-option pages must draw
 * the fourth generic button UNLABELLED, never stretch three over the frame.
 * The semantic count remains three: no selection or touch on slot #4. */
static int pageSlotCount(int page){return pageCount(page)==5?5:4;}
static int buttonHeight(int page){return pageCount(page)==5?kFiveButtonHeight:40;}
static int buttonTop(int page,int index){
    return pageCount(page)==5?12+46*index:kTop[index];
}
static_assert(12+46*4+kFiveButtonHeight<=240-10,"5-button rail must fit");
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
/* 0 CLASSIC is now strictly a fail-closed fallback. Normal MENU_MAIN opens
 * native-120 first; optional logo comparisons cycle 120 -> 240->120 ->
 * direct-240 -> 120, NEVER into CLASSIC without an asset/paint failure. */
enum { kClassic=0,kNative120=1,k240As120=2,kDirect240=3 };
constexpr bool kStrictSha256=false;
static int assetTier(int m){return m==kNative120?120:240;}
int mode=kClassic;
bool defaultPending=false; /* zero-allocation flag; preflight at next root present */
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

/* Prefer the same a.bmp SD source as HELP, WITHOUT a simultaneous
 * PAK lease. On malformed or missing SD atlas, decode the native a.bmp
 * from one temporary PAK lease and close it before SD button/audio use.
 * No ZIP and no retained full atlas or second frame buffer. */
static bool prepareMenuFont(){
    releaseMenuFont();
    fontInk=static_cast<FontInkCache*>(calloc(1U,sizeof(FontInkCache)));
    if(!fontInk){
        Serial.println("[UAC] FONT120 FAIL reason=bounded-cache-alloc");
        return false;
    }
    EspNativeIndexedBmp bmp{};
    EspNativeIndexedBmpStats stats{};
    bool temporaryPak=false;
    EspNativeIndexedBmpStatus st=EspNativeIndexedBmp_openSdFontStandalone(
        "a.bmp",&bmp,&stats);

    /* Cache a.bmp indexed glyphs (not RGB texels). Reserve ink 0 for
     * magenta transparency and preserve ALL 15 shades, including black #2. */
    auto decode=[&]()->bool {
        if(st!=ESP_NATIVE_INDEXED_BMP_OK ||
           bmp.width!=144U || bmp.height!=72U ||
           bmp.bitsPerPixel!=4U || bmp.paletteCount<2U ||
           bmp.paletteCount>16U || bmp.paletteRgb565[0]!=0xf81fU)
            return false;
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
                        /* Palette #0 is the magenta colorkey. Keep 1..15
                         * unchanged: real a.bmp uses index 2 for BLACK. */
                        if(sourceIndex==0U || sourceIndex>=16U)
                            return false;
                        ink=(uint8_t)sourceIndex;
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
            st=EspNativeIndexedBmp_open("a.bmp",&bmp,&stats);
            ok=decode();
        }
    }
    const bool fromSD=ok && bmp.sdFontSource!=0U;
    if(ok){
        /* Both SD and PAK a.bmp are accepted. Grayscale uses the exact
         * HELP tint path; colored legacy PAK is safely hue-tinted. */
        fontSourceIsSD=fromSD;
        fontIsNeutral=bmp.paletteCount==16U;
        fontSourcePalette[0]=0U; /* ink 0 is always transparent */
        for(unsigned ink=1U;ink<16U;++ink){
            const uint16_t c=ink<bmp.paletteCount?
                bmp.paletteRgb565[ink]:0U;
            fontSourcePalette[ink]=c;
            if(!EspUacFontTheme_isGrayRgb565(c))fontIsNeutral=false;
        }
    }
    if(temporaryPak)EspAssetPack_close();
    EspNativeFontSd_suspend();
    if(!ok)releaseMenuFont();
    else fontReady=true;
    Serial.printf("[UAC] FONT120 %s name=a.bmp backing=%s palette=%s ramp=vertical-3stops glyphs=%u inkBytes=%u reads=%lu bytes=%lu\n",
                  ok?"READY":"FAIL",fromSD?"SD":"PAK",
                  fontIsNeutral?"neutral-theme":"classic-metal",
                  (unsigned)(sizeof(kFontAlphabet)-1),(unsigned)sizeof(FontInkCache),
                  (unsigned long)stats.packReads,(unsigned long)stats.bytesRead);
    return ok;
}

/* Sample the identical 9x12 HELP glyph bitmaps into the existing 15x25
 * physical ink boxes, without another atlas or touching game presentation. */
static void paintLargerTextRow(uint16_t* dst,int py,int index,int selected,
                               bool darkPhase,bool patch,int page){
    if(!fontReady||index<0||index>=pageCount(page))return;
    const char* label=labelAt(page,index);
    if(!label)return;
    const int len=(int)strlen(label);
    /* Never overwrite the 6px bevel at either side; long labels (up to
     * ORIENTATION, 11 chars) fit without truncation or extra font buffers. */
    const int advance=len*kPaintAdvance<=98?kPaintAdvance:98/len;
    const int textW=advance==kPaintAdvance?kPaintGlyphW:advance;
    const int textH=pageCount(page)==5?19:(advance<kPaintAdvance?21:kPaintGlyphH);
    const int inset=(buttonHeight(page)-textH)/2+
                    (pageCount(page)==5?0:kLabelYOffset);
    const int paintY=py-buttonTop(page,index)-inset;
    if(paintY<0||paintY>=textH)return;
    const int sourceY=paintY*kGlyphH/textH;
    const int xStart=(ESP_UAC_BUTTON_WIDTH-(len*advance+textW-advance))/2;
    const int destOrigin=patch?0:kPanelX+ESP_UAC_BUTTON_INSET;
    /* Smooth 19/21/25-row vertical tone, dark-to-light.
     * 16 shades on stack; transparency and outlines unchanged. */
    uint16_t rowInk[16];
    const uint16_t tone=EspUacFontTheme_verticalTone(
        EspMainMenuSkin_letterTone(0,9),
        EspMainMenuSkin_letterTone(4,9),
        EspMainMenuSkin_letterTone(8,9),paintY,textH);
    const uint8_t modR=(uint8_t)(((tone>>11U)&31U)*255U/31U);
    const uint8_t modG=(uint8_t)(((tone>>5U)&63U)*255U/63U);
    const uint8_t modB=(uint8_t)((tone&31U)*255U/31U);
    for(unsigned k=0;k<16U;++k)
        rowInk[k]=fontIsNeutral?
            EspNativeFontTint_apply(fontSourcePalette[k],modR,modG,modB,
                                    fontSourceIsSD?1U:0U):
            EspUacFontTheme_tintColor(fontSourcePalette[k],tone);
    for(int i=0;i<len;++i){
        /* Main-menu labels are lowercase, OPTIONS labels already uppercase.
         * An unconditional ASCII transform turns 'U' into '5'. */
        const char raw=label[i];
        const char ch=(raw>='a'&&raw<='z')?(char)(raw-'a'+'A'):raw;
        const char* found=strchr(kFontAlphabet,ch);
        if(!found)continue; /* No unsupported glyph can index outside cache. */
        const unsigned gi=(unsigned)(found-kFontAlphabet);
        for(int x=0;x<textW;++x){
            const int sourceX=x*kGlyphW/textW;
            const uint8_t pair=fontInk->glyph[gi][sourceY][sourceX/2];
            const unsigned ink=(sourceX&1)?(pair&15U):(pair>>4U);
            if(ink==0U)continue; /* atlas transparent magenta */
            const int dx=destOrigin+xStart+i*advance+x;
            if(dx<0||dx>=(patch?ESP_UAC_BUTTON_WIDTH:kWidth))continue;
            uint16_t color=rowInk[ink];
            /* Unselected = full, legible three-stop gradient. Only the
             * selected label pulses light/dark, retaining a static button
             * chassis and always keeping disabled options visibly dim. */
            const bool disabled=page!=ESP_UAC_OPTIONS_NONE &&
                               !(optionsEnabledMask&(1U<<index));
            if(disabled)color=EspUacFontTheme_scale565(color,1U,2U);
            else if(index==selected && darkPhase)
                color=EspUacFontTheme_scale565(color,2U,5U);
            dst[dx]=color;
        }
    }
}
/* Fail closed on malformed BMP or allocation/short-read failure.
 * Content is accessed only after validation and copying from SD. */
static bool cacheNativeBmp(BufferedBmp& target,const char* path,
                           uint16_t w,uint16_t h,const char* sha){
    OpenImage source{};
    bool ok=openBmp(source,path,w,h);
    if(ok&&sha&&kStrictSha256)ok=verifySha256(source.file,sha);
    const uint32_t n=(uint32_t)w*h*4U;
    uint8_t* data=ok?static_cast<uint8_t*>(malloc(n)):nullptr;
    ok=ok&&data&&readAt(&source.file,source.bmp.pixelOffset,data,n);
    if(source.file)source.file.close();
    if(!ok){free(data);return false;}
    target.bmp=source.bmp;target.pixels=data;return true;
}
static bool preflight(int m){
    if(m==kNative120){
        releaseBufferedBmps();
        char frameName[72];
        framePath(frameName,sizeof(frameName),m);
        if(!cacheNativeBmp(frameCache,frameName,59U,120U,kSha256[0][0]) ||
           !cacheNativeBmp(genericCache,kGeneric120,55U,20U,nullptr)){
            Serial.println("[UAC] PREFLIGHT FAIL reason=bounded-bmp-cache");
            releaseBufferedBmps();return false;
        }
        if(!prepareMenuFont()){releaseBufferedBmps();return false;}
        Serial.println("[UAC] PREFLIGHT OK mode=1 tier=120 cachedBMP=32720 menu-only font=larger SD-or-PAK");
        return true;
    }
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
                 int m,int selected,int previous,bool darkPhase,DrawStats* stats){
    OpenImage frame{},button{};
    char path[72];
    const int scale=m==kDirect240?1:2;
    const int sourceStep=m==k240As120?2:1;
    const auto blendRow=m==k240As120?
        EspUacBmp32_blendRow240As120:EspUacBmp32_blendRow;
    const uint16_t fw=m==1?59U:118U,fh=m==1?120U:240U;
    const uint16_t bw=m==1?55U:110U,bh=m==1?20U:40U;
    const int countButtons=pageSlotCount(optionsPage);
    /* Same-page selection/arm: repaint only one or two metal plates. */
    const bool patch=previous>=0&&previous<pageCount(optionsPage);
    const int regions=patch?(previous==selected?1:2):1;
    int openedButton=-1;
    auto finish=[&](bool ok,const char* stage)->bool{
        if(!ok)Serial.printf("[UAC] DRAW FAIL mode=%d stage=%s path=%s\n",m,stage,path);
        if(button.file)button.file.close();
        if(frame.file)frame.file.close();
        return ok;
    };
    if(m==kNative120){
        if(!frameCache.pixels||!genericCache.pixels||!fontReady)
            return finish(false,"cache-missing");
        frame.bmp=frameCache.bmp;button.bmp=genericCache.bmp;
    }else{
        framePath(path,sizeof(path),m);
        if(!openBmp(frame,path,fw,fh))return finish(false,"frame-open");
    }
    for(int region=0;region<regions;++region){
        const int target=patch?(region==0?previous:selected):-1;
        const int firstY=target>=0?buttonTop(optionsPage,target):0;
        const int finalY=target>=0?buttonTop(optionsPage,target)+buttonHeight(optionsPage):kHeight;
        const int outputX=target>=0?kPanelX+ESP_UAC_BUTTON_INSET:0;
        const int outputW=target>=0?ESP_UAC_BUTTON_WIDTH:kWidth;
        for(int y=firstY;y<finalY;){
            int active=target,until=finalY;
            if(!patch){
                active=-1;
                for(int i=0;i<countButtons;++i){
                    const int top=buttonTop(optionsPage,i);
                    if(y<top){until=top;break;}
                    if(y<top+buttonHeight(optionsPage)){
                        active=i;until=top+buttonHeight(optionsPage);break;
                    }
                }
            }
            int count=kRows;
            if(count>until-y)count=until-y;
            if(count>finalY-y)count=finalY-y;
            if(count<=0)return finish(false,"row-boundary");
            const uint32_t sdStart=micros();
            if(m==kNative120){
                /* One cached generic source for all labels/states; no SD I/O. */
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
            if(m==kNative120){
                frameBatch.bytes=frameCache.pixels;
                frameBatch.fileOffset=frame.bmp.pixelOffset;
                frameBatch.size=(uint32_t)fw*fh*4U;
            }else if(!EspUacBmp32_loadBatch(&frame.bmp,readAt,&frame.file,first,
                    (uint16_t)(last-first+1U),frameRows,sizeof(frameRows),
                    &frameBatch))return finish(false,"frame-batch");
            if(active>=0){
                const uint16_t bFirst=(uint16_t)((optionsPage!=ESP_UAC_OPTIONS_NONE &&
                    countButtons==5)?
                    ((y-buttonTop(optionsPage,active))*20/buttonHeight(optionsPage)):
                    ((y-buttonTop(optionsPage,active))/scale)*sourceStep);
                const uint16_t bLast=(uint16_t)((optionsPage!=ESP_UAC_OPTIONS_NONE &&
                    countButtons==5)?
                    ((y+count-1-buttonTop(optionsPage,active))*20/buttonHeight(optionsPage)):
                    ((y+count-1-buttonTop(optionsPage,active))/scale)*sourceStep);
                if(m==kNative120){
                    buttonBatch.bytes=genericCache.pixels;
                    buttonBatch.fileOffset=button.bmp.pixelOffset;
                    buttonBatch.size=(uint32_t)bw*bh*4U;
                }else if(!EspUacBmp32_loadBatch(&button.bmp,readAt,&button.file,
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
                        (uint16_t)((optionsPage!=ESP_UAC_OPTIONS_NONE && countButtons==5)?
                            ((py-buttonTop(optionsPage,active))*20/buttonHeight(optionsPage)):
                            ((py-buttonTop(optionsPage,active))/scale)),
                        patch?0:kPanelX+ESP_UAC_BUTTON_INSET,(uint8_t)scale,
                        out,(uint16_t)outputW,pixels,sizeof(pixels)))
                    return finish(false,"button-blend");
                if(!blendRow(&frame.bmp,
                        EspUacBmp32_readBatch,&frameBatch,(uint16_t)(py/scale),
                        patch?-ESP_UAC_BUTTON_INSET:kPanelX,(uint8_t)scale,
                        out,(uint16_t)outputW,pixels,
                        sizeof(pixels)))return finish(false,"frame-blend");
                if(m==kNative120&&active>=0)
                    paintLargerTextRow(out,py,active,selected,darkPhase,patch,optionsPage);
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
extern "C" int EspUacMenuPreview_needsBlink(void){
    const uint32_t nowMs=millis();
    return mode==kNative120 && paintedMode==kNative120 && fontReady &&
           (((nowMs-blinkEpochMs)/kBlinkHalfPeriodMs)&1U)!=(unsigned)paintedDark;
}
extern "C" void EspUacMenuPreview_armOptions(void){
    optionsPending=true; /* preflight deferred until after root FNV/heap oracles */
}
extern "C" int EspUacMenuPreview_optionsActive(void){
    return mode==kNative120 && optionsPage!=ESP_UAC_OPTIONS_NONE;
}
extern "C" int EspUacMenuPreview_optionsHit(int16_t x,int16_t y){
    if(!EspUacMenuPreview_optionsActive() ||
       x<kPanelX+ESP_UAC_BUTTON_INSET ||
       x>=kPanelX+ESP_UAC_BUTTON_INSET+ESP_UAC_BUTTON_WIDTH)return -1;
    for(int i=0;i<pageCount(optionsPage);++i){
        const int top=buttonTop(optionsPage,i);
        if(y>=top && y<top+buttonHeight(optionsPage))return i;
    }
    return -1;
}
extern "C" void EspUacMenuPreview_optionsPage(int page,int selected,int armed,
                                               uint8_t mask){
    if(!EspUacMenuPreview_optionsActive() ||
       page<ESP_UAC_OPTIONS_ROOT || page>ESP_UAC_OPTIONS_VOLUME)return;
    /* Only the physical volume page is four 40px buttons with no BACK.
     * Preserve the canonical 5-card SOUND model and choice numbers 1..4. */
    if(page==ESP_UAC_OPTIONS_VOLUME){
        if(selected<1||selected>4)return;
        selected-=1;mask=0x0fU;
    }
    if(selected<0||selected>=pageCount(page))return;
    optionsPage=page;optionsSelection=selected;
    optionsArmed=armed?1:0;optionsEnabledMask=mask;
}
extern "C" int EspUacMenuPreview_enterOptions(void){
    if(!optionsPending)return EspUacMenuPreview_optionsActive();
    optionsPending=false;
    optionsPage=ESP_UAC_OPTIONS_NONE;
    if(mode!=kClassic)EspUacMenuPreview_reset();
    if(!preflight(kNative120)){
        Serial.println("[UAC] OPTIONS fallback=procedural reason=sd-or-font");
        return 0;
    }
    mode=kNative120;optionsPage=ESP_UAC_OPTIONS_ROOT;
    optionsSelection=0;optionsArmed=0;optionsEnabledMask=0x07U;
    paintedMode=0;paintedSelection=-1;paintedPage=ESP_UAC_OPTIONS_NONE;
    Serial.printf("[UAC] OPTIONS ready=1 generic=menu_generic_120 buttons=1 fontBytes=%u\n",
                  (unsigned)sizeof(FontInkCache));
    return 1;
}
extern "C" void EspUacMenuPreview_armDefault(void){
    defaultPending=true; /* no heap or SD reads before root invariant checks */
}
extern "C" void EspUacMenuPreview_reset(void){
    if(mode)Serial.printf("[UAC] RESET mode=%d heap8=%u largest8=%u\n",
                          mode,
                          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    mode=kClassic;paintedMode=0;paintedSelection=-1;defaultPending=false;
    optionsPending=false;optionsPage=ESP_UAC_OPTIONS_NONE;
    paintedPage=ESP_UAC_OPTIONS_NONE;paintedEnabledMask=0U;paintedArmed=0;
    paintedTheme=0xffU;paintedDark=false;blinkEpochMs=0U;
    releaseMenuAssets();
}
/* Called ONLY after MENU_MAIN's strict heap-equality and framebuffer FNV
 * oracles and touch activation have passed. The 1428B ink cache therefore
 * cannot invalidate those pre-existing memory checks. All SD/PAK failures
 * leave the already-constructed native CLASSIC framebuffer as fallback. */
extern "C" int EspUacMenuPreview_enterDefault(void){
    if(!defaultPending)return mode; /* all other present calls are side-effect free */
    defaultPending=false;
    const unsigned heapBefore=(unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const unsigned largestBefore=(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    mode=kClassic;paintedMode=0;paintedSelection=-1;
    releaseMenuFont();
    if(!preflight(kNative120)){
        releaseMenuFont();
        Serial.printf("[UAC] DEFAULT FALLBACK mode=CLASSIC reason=assets-or-font heap8=%u largest8=%u\n",
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return kClassic;
    }
    mode=kNative120;
    Serial.printf("[UAC] DEFAULT READY mode=120 generic=1 font=a.bmp inkBytes=%u glyphPaint=%dx%d heap8=%u->%u largest8=%u->%u\n",
                  (unsigned)sizeof(FontInkCache),kPaintGlyphW,kPaintGlyphH,
                  heapBefore,(unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  largestBefore,(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return mode;
}
extern "C" int EspUacMenuPreview_cycle(void){
    defaultPending=false;
    /* Classic is never offered as a normal mode; in fallback tap the logo
     * to reattempt 120 after repairing/remounting SD. */
    const int next=mode==kNative120?k240As120:
                   mode==k240As120?kDirect240:kNative120;
    if(mode==kNative120 && next!=kNative120)releaseMenuAssets();
    if(!preflight(next)){
        releaseMenuAssets();
        mode=kClassic;paintedMode=0;paintedSelection=-1;
        Serial.println("[UAC] FALLBACK SD missing/invalid -> original MENU_MAIN");
        return kClassic;
    }
    mode=next;
    paintedMode=0;paintedSelection=-1;
    Serial.printf("[UAC] MODE %s source=%s physical=320x240 buttons=110x40 RAM=6680B-BSS+1428B-menu-only\n",
                  mode==kClassic?"CLASSIC":mode==kNative120?"120":
                    mode==k240As120?"240->120":"240",
                  mode==kClassic?"fallback":mode==kNative120?
                    "59x120 nearest-x2":mode==k240As120?
                    "118x240 decimate2x+nearest-x2":"118x240 DIRECT");
    return mode;
}
bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride){
    const int current=mode;
    if(!current)return true;
    if(!lcd||!fb||stride<160U)return false;
    const int page=optionsPage;
    const int sel=page==ESP_UAC_OPTIONS_NONE?
                  EspUacMenuPreview_selectedIndex():optionsSelection;
    if(sel<0||sel>=pageCount(page))return false;
    const uint8_t theme=EspMainMenuSkin_getTheme();
    const uint32_t now=millis();
    const bool changedSelection=current!=paintedMode ||
                               sel!=paintedSelection || paintedPage!=page;
    if(changedSelection)blinkEpochMs=now;
    const bool darkWanted=current==kNative120 &&
                (((now-blinkEpochMs)/kBlinkHalfPeriodMs)&1U)!=0U;
    if(current==paintedMode && sel==paintedSelection &&
       paintedPage==page && paintedArmed==optionsArmed &&
       paintedEnabledMask==optionsEnabledMask &&
       paintedTheme==theme && paintedDark==darkWanted)return true;
    const bool blinkOnly=!changedSelection && current==paintedMode &&
        paintedArmed==optionsArmed && paintedEnabledMask==optionsEnabledMask &&
        paintedTheme==theme && paintedDark!=darkWanted;
    /* Repaint all labels when the palette changes, not only the armed button. */
    const int previous=(current==paintedMode && paintedPage==page &&
                        paintedTheme==theme)?
                       paintedSelection:-1;
    const uint32_t start=millis();
    DrawStats stats{};
    const bool ok=draw(fb,stride,lcd,current,sel,previous,darkWanted,&stats);
    if(ok){paintedMode=current;paintedSelection=sel;paintedPage=page;
           paintedArmed=optionsArmed;paintedEnabledMask=optionsEnabledMask;
           paintedTheme=theme;paintedDark=darkWanted;}
    if(!blinkOnly)Serial.printf("[UAC] PRESENT mode=%d page=%d selected=%d prior=%d paint=%s result=%s ms=%lu sdMs=%lu lcdMs=%lu batches=%u rows=%u buttonOpens=%u audioPumps=%u audioBlocks=%u heap=%u framebuffer=unchanged\n",
                  current,page,sel,previous,previous>=0?"BUTTONS":"FULL",
                  ok?"OK":"FAIL",static_cast<unsigned long>(millis()-start),
                  static_cast<unsigned long>(stats.sdUs/1000U),
                  static_cast<unsigned long>(stats.lcdUs/1000U),
                  stats.batches,stats.rowsSent,stats.buttonOpens,
                  stats.audioPumps,stats.audioBlocks,
                  static_cast<unsigned>(ESP.getFreeHeap()));
    return ok;
}
