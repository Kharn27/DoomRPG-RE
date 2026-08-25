#include <SDL.h>
#include <stdint.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_junction_sprite_renderer.h"
#include "esp_player_view_state.h"

#define MAP_HEADER 16U
#define PALETTE_HEADER 4U
#define TEXEL_HEADER 4U
#define PAIR_BYTES 8U
#define SHAPE_HEADER 12U
#define MAX_SPRITES 64U
#define MAX_DIM 64
#define MAX_MASK 512U
#define MAX_TEXELS 2048U
#define SCREEN_W 160

#define VISUAL_MASK 0x0001fe00UL
#define HIDDEN 0x00010000UL
#define TILE 0x00040000UL
#define CROSS 0x04000000UL
#define SKIP_RESOURCE 0x20000000UL
#define FIXED_ANIM 0x80000000UL
#define SORT_BIAS 0x01000000UL
#define ORIENT_MASK 0x00780000UL
#define TWO_SIDED 0x00000001UL
#define SPRITE_SPAN 0x00000002UL
#define AXIS_X 0x00000008UL
#define AXIS_NEG 0x00000010UL
#define Y_NUDGE 0x00000100UL
#define X_NUDGE 0x00000200UL
#define OCCLUDER 0x20000000UL
#define ENEMY_TYPE 1U

typedef struct Sources_s {
    EspAssetPackEntry mappings, palettes, bitshapes, wtexels, stexels;
    uint32_t texelPairs, bitShapePairs, textureIds, spriteIds;
    uint32_t spritePairBase, spriteIdBase, paletteEntries;
    uint32_t wallBytes, spriteBytes;
} Sources;

typedef struct Frame_s {
    uint16_t logical, actual;
    uint32_t texelOffset;
    int xMin, xMax, yMin, yMax, width, height, pitch;
    uint32_t maskBytes, active, packedBytes;
    uint16_t palette[16];
    uint16_t prefix[MAX_DIM + 1];
    uint8_t mask[MAX_MASK];
    uint8_t texels[MAX_TEXELS];
} Frame;

typedef struct Order_s {
    uint16_t index, logical;
    uint32_t info;
    int32_t sortZ;
} Order;

typedef struct Scratch_s {
    int viewCos_, viewSin_, viewTransX, viewSin, viewCos, viewTransY;
    int viewX, viewY, viewZ, viewAngle;
    int lineCount, lineRasterCount, nodeCount, nodeRasterCount;
    int spriteCount, spriteRasterCount;
    int screenLeft, screenTop, screenRight, screenBottom, numLines;
    byte spanMode;
    short* pixels;
    Line_t tmpLine;
    int columnScale[SCREEN_W];
} Scratch;

static Frame frame;
static Order order[MAX_SPRITES];
static uint32_t seenLogical[8];

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t fnv(uint32_t h, const void* data, uint32_t n) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    for (i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619U; }
    return h;
}
static uint16_t source565(uint16_t c) {
    return (uint16_t)(((c & 0x001fU) << 11) | (c & 0x07e0U) |
                      ((c & 0xf800U) >> 11));
}
static int readRange(const EspAssetPackEntry* e, uint32_t off, void* dst,
                     uint32_t n, EspNativeJunctionSpriteStats* s) {
    if (e == NULL || dst == NULL || s == NULL || off > e->size ||
        n > e->size - off || !EspAssetPack_readRange(e, off, dst, n)) return 0;
    ++s->packReads;
    return 1;
}

static void saveScratch(Render_t* r, Scratch* s) {
    memset(s, 0, sizeof(*s));
    s->viewCos_=r->viewCos_; s->viewSin_=r->viewSin_; s->viewTransX=r->viewTransX;
    s->viewSin=r->viewSin; s->viewCos=r->viewCos; s->viewTransY=r->viewTransY;
    s->viewX=r->viewX; s->viewY=r->viewY; s->viewZ=r->viewZ; s->viewAngle=r->viewAngle;
    s->lineCount=r->lineCount; s->lineRasterCount=r->lineRasterCount;
    s->nodeCount=r->nodeCount; s->nodeRasterCount=r->nodeRasterCount;
    s->spriteCount=r->spriteCount; s->spriteRasterCount=r->spriteRasterCount;
    s->screenLeft=r->screenLeft; s->screenTop=r->screenTop;
    s->screenRight=r->screenRight; s->screenBottom=r->screenBottom;
    s->numLines=r->numLines; s->spanMode=r->spanMode; s->pixels=r->pixels;
    s->tmpLine=r->tmpLine;
    memcpy(s->columnScale, r->columnScale, sizeof(s->columnScale));
}
static void restoreScratch(Render_t* r, const Scratch* s) {
    r->viewCos_=s->viewCos_; r->viewSin_=s->viewSin_; r->viewTransX=s->viewTransX;
    r->viewSin=s->viewSin; r->viewCos=s->viewCos; r->viewTransY=s->viewTransY;
    r->viewX=s->viewX; r->viewY=s->viewY; r->viewZ=s->viewZ; r->viewAngle=s->viewAngle;
    r->lineCount=s->lineCount; r->lineRasterCount=s->lineRasterCount;
    r->nodeCount=s->nodeCount; r->nodeRasterCount=s->nodeRasterCount;
    r->spriteCount=s->spriteCount; r->spriteRasterCount=s->spriteRasterCount;
    r->screenLeft=s->screenLeft; r->screenTop=s->screenTop;
    r->screenRight=s->screenRight; r->screenBottom=s->screenBottom;
    r->numLines=s->numLines; r->spanMode=s->spanMode; r->pixels=s->pixels;
    r->tmpLine=s->tmpLine;
    memcpy(r->columnScale, s->columnScale, sizeof(s->columnScale));
}

static int setupView(Render_t* r, const EspPlayerViewState* v) {
    int sin_, cos_, vx, vy, x;
    if (r == NULL || v == NULL || r->framebuffer == NULL || r->columnScale == NULL ||
        r->screenWidth != SCREEN_W || r->screenHeight != 80 ||
        r->screenX != 0 || r->screenY != 20) return 0;
    sin_ = r->sinTable[v->viewAngle & 255];
    cos_ = r->sinTable[(v->viewAngle + 64) & 255];
    vx = v->viewX - ((16 * cos_) >> 16);
    vy = v->viewY + ((16 * sin_) >> 16);
    r->viewX=vx; r->viewY=vy; r->viewZ=v->viewZ;
    r->viewCos_=cos_; r->viewSin_=-sin_;
    r->viewTransX=-((vx*r->viewCos_)+(vy*r->viewSin_));
    r->viewSin=sin_; r->viewCos=cos_;
    r->viewTransY=-((vx*r->viewSin)+(vy*r->viewCos));
    r->viewAngle=v->viewAngle;
    r->pixels=(short*)&r->framebuffer[r->pitch*r->screenY];
    r->screenLeft=0; r->screenTop=0; r->screenRight=SCREEN_W; r->screenBottom=80;
    for (x=0; x<SCREEN_W; ++x) r->columnScale[x]=MAXINT;
    return 1;
}

static void sourceLine(const EspMapLine* src, Line_t* l) {
    memset(l, 0, sizeof(*l));
    l->vert1.x=src->x1; l->vert1.y=src->y1;
    l->vert2.x=src->x2; l->vert2.y=src->y2; l->flags=(int)src->flags;
    if (src->flags & X_NUDGE) {
        if (src->flags & AXIS_X) l->vert1.x+=3, l->vert2.x+=3;
        else if (src->flags & AXIS_NEG) l->vert1.x-=3, l->vert2.x-=3;
    } else if (src->flags & Y_NUDGE) {
        if (src->flags & AXIS_X) l->vert1.y+=3, l->vert2.y+=3;
        else if (src->flags & AXIS_NEG) l->vert1.y-=3, l->vert2.y-=3;
    }
}

static int depthColumns(Render_t* r, Line_t* l) {
    int dx=l->vert2.x-l->vert1.x, step, dDepth, x, x2, depth;
    if (dx <= 0) return 1;
    step=(MAXINT/dx)<<1;
    dDepth=(int)DoomRPG_FixedMul(l->vert2.y-l->vert1.y, step);
    x=(l->vert1.x+65535)>>16; x2=(l->vert2.x+65535)>>16;
    if (x<r->screenLeft) x=r->screenLeft; if (x2>r->screenRight) x2=r->screenRight;
    depth=l->vert1.y + DoomRPG_FixedMul((x<<16)-l->vert1.x, dDepth);
    while (x<x2) {
        int scale;
        if (depth<=0) return 0;
        scale=(0x40000000/depth)<<2; depth+=dDepth;
        if (r->columnScale[x]>=scale) r->columnScale[x]=scale;
        ++x;
    }
    return 1;
}

static int rebuildDepth(Render_t* r, const EspMapRuntimeView* rt,
                        EspNativeJunctionSpriteStats* s) {
    uint32_t i;
    for (i=0; i<rt->lineCount; ++i) {
        EspMapLine src; Line_t l; Vertex_t tmp;
        if (!EspMapRuntime_getLine(i,&src)) return 0;
        sourceLine(&src,&l); ++s->depthLines;
        if (((l.vert1.x-r->viewX)*(l.vert2.y-l.vert1.y))+
            ((l.vert1.y-r->viewY)*(-(l.vert2.x-l.vert1.x)))<=0) {
            if (!(l.flags&TWO_SIDED)) { ++s->depthBackfaceCulled; continue; }
            tmp=l.vert1; l.vert1=l.vert2; l.vert2=tmp;
        }
        Render_transform2DVerts(r,&l.vert1); Render_transform2DVerts(r,&l.vert2);
        if (!Render_clipLine(r,&l)) { ++s->depthClipCulled; continue; }
        Render_projectVertex(r,&l.vert1); Render_projectVertex(r,&l.vert2);
        if (l.flags&OCCLUDER) { Render_occludeClippedLine(r,&l); ++s->depthOccluders; }
        else if (l.flags&SPRITE_SPAN) ++s->depthSpriteSpans;
        else if (!depthColumns(r,&l)) return 0;
    }
    return 1;
}

static int initSources(Sources* c, EspNativeJunctionSpriteStats* s) {
    uint8_t mh[16], ph[4], wh[4], sh[4]; uint32_t pbytes, tidBase; uint64_t expected;
    memset(c,0,sizeof(*c));
    if (!EspAssetPack_findEntry("mappings.bin",&c->mappings) ||
        !EspAssetPack_findEntry("palettes.bin",&c->palettes) ||
        !EspAssetPack_findEntry("bitshapes.bin",&c->bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin",&c->wtexels) ||
        !EspAssetPack_findEntry("stexels.bin",&c->stexels) ||
        !readRange(&c->mappings,0,mh,16,s) || !readRange(&c->palettes,0,ph,4,s) ||
        !readRange(&c->wtexels,0,wh,4,s) || !readRange(&c->stexels,0,sh,4,s)) return 0;
    c->texelPairs=le32(mh); c->bitShapePairs=le32(mh+4);
    c->textureIds=le32(mh+8); c->spriteIds=le32(mh+12); pbytes=le32(ph);
    c->wallBytes=le32(wh); c->spriteBytes=le32(sh);
    c->spritePairBase=MAP_HEADER+c->texelPairs*PAIR_BYTES;
    tidBase=c->spritePairBase+c->bitShapePairs*PAIR_BYTES;
    c->spriteIdBase=tidBase+c->textureIds*2U;
    expected=(uint64_t)c->spriteIdBase+(uint64_t)c->spriteIds*2U;
    if (!c->texelPairs || !c->bitShapePairs || !c->spriteIds ||
        c->texelPairs>4096U || c->bitShapePairs>4096U || c->spriteIds>4096U ||
        expected!=c->mappings.size || (pbytes&1U) || pbytes+4U!=c->palettes.size ||
        c->wallBytes+4U!=c->wtexels.size || c->spriteBytes+4U!=c->stexels.size ||
        c->wallBytes>UINT32_MAX/2U) return 0;
    c->paletteEntries=pbytes/2U; return 1;
}

static int loadFrame(const Sources* c, uint16_t logical, uint32_t anim,
                     EspNativeJunctionSpriteStats* s) {
    uint8_t id[2], pair[8], h[12], pal[32]; uint32_t actual, shapeOff, maskOff;
    int32_t srcOff, palOff; uint32_t x, active=0, base, rel, stexOff, p;
    if (logical>=c->spriteIds || !EspNativeGraphicsCatalog_findSprite(logical)) return 0;
    memset(&frame,0,sizeof(frame));
    if (!readRange(&c->mappings,c->spriteIdBase+(uint32_t)logical*2U,id,2,s)) return 0;
    actual=(uint32_t)le16(id)+anim;
    if (actual>=c->bitShapePairs || actual>UINT16_MAX ||
        !readRange(&c->mappings,c->spritePairBase+actual*8U,pair,8,s)) return 0;
    srcOff=(int32_t)le32(pair); palOff=(int32_t)le32(pair+4);
    if (srcOff<0 || palOff<0 || (uint32_t)palOff>c->paletteEntries ||
        16U>c->paletteEntries-(uint32_t)palOff) return 0;
    shapeOff=TEXEL_HEADER+(uint32_t)srcOff;
    if (!readRange(&c->bitshapes,shapeOff,h,12,s) ||
        !readRange(&c->palettes,PALETTE_HEADER+(uint32_t)palOff*2U,pal,32,s)) return 0;
    frame.logical=logical; frame.actual=(uint16_t)actual; frame.texelOffset=le32(h);
    frame.xMin=h[8]; frame.xMax=h[9]; frame.yMin=h[10]; frame.yMax=h[11];
    if (frame.xMax<frame.xMin || frame.yMax<frame.yMin) return 0;
    frame.width=frame.xMax-frame.xMin+1; frame.height=frame.yMax-frame.yMin+1;
    frame.pitch=(frame.height+7)/8;
    if (frame.width<=0 || frame.width>MAX_DIM || frame.height<=0 || frame.height>MAX_DIM) return 0;
    frame.maskBytes=(uint32_t)frame.width*(uint32_t)frame.pitch;
    if (!frame.maskBytes || frame.maskBytes>MAX_MASK) return 0;
    maskOff=shapeOff+SHAPE_HEADER;
    if (!readRange(&c->bitshapes,maskOff,frame.mask,frame.maskBytes,s)) return 0;
    frame.prefix[0]=0;
    for (x=0; x<(uint32_t)frame.width; ++x) {
        const uint8_t* col=frame.mask+x*(uint32_t)frame.pitch; int y;
        for (y=0;y<frame.height;++y) if (col[y/8]&(1U<<(y&7))) ++active;
        frame.prefix[x+1]=(uint16_t)active;
    }
    frame.active=active; frame.packedBytes=((active+1U)&~1U)/2U;
    if (!frame.packedBytes || frame.packedBytes>MAX_TEXELS) return 0;
    base=c->wallBytes*2U;
    if (frame.texelOffset<base || ((frame.texelOffset-base)&1U)) return 0;
    rel=frame.texelOffset-base; stexOff=TEXEL_HEADER+rel/2U;
    if (!readRange(&c->stexels,stexOff,frame.texels,frame.packedBytes,s)) return 0;
    for (p=0;p<16U;++p) frame.palette[p]=source565(le16(pal+p*2U));
    ++s->frameLoads; s->frameBytes+=frame.maskBytes+frame.packedBytes;
    if (frame.maskBytes+frame.packedBytes>s->maxFrameBytes) s->maxFrameBytes=frame.maskBytes+frame.packedBytes;
    { uint32_t w=(uint32_t)logical>>5, b=1U<<((uint32_t)logical&31U);
      if (!(seenLogical[w]&b)) { seenLogical[w]|=b; ++s->uniqueLogical; } }
    return 1;
}

static int buildOrder(Render_t* r, const EspMapRuntimeView* rt,
                      EspNativeJunctionSpriteStats* s, uint32_t* n) {
    uint32_t i,count=0,h=2166136261U;
    if (rt->mapSpriteCount>MAX_SPRITES) return 0;
    for (i=0;i<rt->mapSpriteCount;++i) {
        EspMapSprite sp; uint8_t vis,type,sub; uint16_t link,ord; uint32_t info,id,pos; int32_t z;
        if (!EspMapRuntime_getMapSprite(i,&sp) || !EspMapSpriteTopology_getVisualState(i,&vis) ||
            !EspMapSpriteTopology_getEntity(i,&type,&sub,&link,&ord)) return 0;
        (void)sub; (void)link; (void)ord; ++s->objects;
        info=(sp.info&~VISUAL_MASK)|((uint32_t)vis<<9); id=info&511U;
        if (info&HIDDEN) { ++s->hidden; continue; }
        if (id>=82U && id<=90U && !(id&1U)) info|=CROSS;
        if ((info&(TILE|CROSS|SKIP_RESOURCE|ORIENT_MASK)) || !EspNativeGraphicsCatalog_findSprite((uint16_t)id)) {
            ++s->unsupported; return 0;
        }
        if (id==135U || id==140U || id==131U) ++s->glowDeferred;
        z=(int32_t)((sp.x*r->viewCos_)+(sp.y*r->viewSin_)+r->viewTransX);
        if (info&SORT_BIAS) ++z; else if (type==ENEMY_TYPE) --z;
        else if (id>=180U && id<=191U) z-=2;
        pos=count; while (pos>0 && z>=order[pos-1].sortZ) { order[pos]=order[pos-1]; --pos; }
        order[pos].index=(uint16_t)i; order[pos].logical=(uint16_t)id;
        order[pos].info=info; order[pos].sortZ=z; ++count;
    }
    for (i=0;i<count;++i) { h=fnv(h,&order[i].index,2); h=fnv(h,&order[i].sortZ,4); }
    s->orderFNV1a=h; *n=count; return count>0;
}

static int spans(Render_t* r, Line_t* l, EspNativeJunctionSpriteStats* s) {
    int dx=l->vert2.x-l->vert1.x, step,dSide,dTex,x,x2,tex,depth;
    if (dx<=0) return 1;
    step=(MAXINT/dx)<<1; dSide=(int)DoomRPG_FixedMul(l->vert2.y-l->vert1.y,step);
    dTex=(int)DoomRPG_FixedMul(l->vert2.z-l->vert1.z,step);
    x=(l->vert1.x+65535)>>16; x2=(l->vert2.x+65535)>>16;
    if (x<r->screenLeft) x=r->screenLeft; if (x2>r->screenRight) x2=r->screenRight;
    { int j=(x<<16)-l->vert1.x; tex=l->vert1.z+DoomRPG_FixedMul(j,dTex);
      depth=l->vert1.y+DoomRPG_FixedMul(j,dSide); }
    while (x<x2) {
        int scale,col,texStep;
        if (depth<=0) return 0;
        scale=(0x40000000/depth)<<2; col=(int)(DoomRPG_FixedMul(tex,scale)>>16);
        depth+=dSide; tex+=dTex;
        if (r->columnScale[x]>=scale) {
            const uint8_t* bits; uint32_t cursor; int y=0;
            if (col<0 || col>=frame.width) return 0;
            texStep=scale>>3; bits=frame.mask+(uint32_t)col*(uint32_t)frame.pitch;
            cursor=frame.prefix[col];
            while (y<frame.height) {
                int start,len,sy,pixels,pitch,remain,world; uint32_t base; int64_t pos; uint16_t* dst;
                while (y<frame.height && !(bits[y/8]&(1U<<(y&7)))) ++y;
                if (y>=frame.height) break;
                start=y; base=cursor;
                while (y<frame.height && (bits[y/8]&(1U<<(y&7)))) { ++cursor; ++y; }
                len=y-start; world=(64-(frame.yMin+start))-r->viewZ;
                pixels=(len*depth)>>17; sy=r->halfScreenHeight-((world*depth)>>17);
                pos=((int64_t)base)<<12;
                if (sy<r->screenTop) { int cut=r->screenTop-sy; pos+=(int64_t)texStep*cut; pixels-=cut; sy=r->screenTop; }
                if (sy+pixels>r->screenBottom) pixels=r->screenBottom-sy;
                if (pixels<=0) continue;
                pitch=r->pitch>>1; dst=(uint16_t*)r->pixels+pitch*sy+x; remain=pixels; ++s->spanRuns;
                while (remain-- >0) {
                    uint32_t pi; uint8_t packed; int shift;
                    if (pos<0) return 0; pi=(uint32_t)(pos>>13); if (pi>=frame.packedBytes) return 0;
                    packed=frame.texels[pi]; shift=(int)((pos>>10)&4); *dst=frame.palette[(packed>>shift)&15U];
                    dst+=pitch; pos+=texStep; ++s->pixelsDrawn;
                }
            }
            if (cursor!=frame.prefix[col+1]) return 0;
        } else ++s->wallOccludedColumns;
        ++x;
    }
    return 1;
}

static int drawOne(Render_t* r, const Sources* c, const Order* o,
                   EspNativeJunctionSpriteStats* s) {
    EspMapSprite sp; Vertex_t center; Line_t l; uint32_t anim; int min,max;
    if (!EspMapRuntime_getMapSprite(o->index,&sp)) return 0;
    memset(&center,0,sizeof(center)); center.x=sp.x; center.y=sp.y;
    Render_transform2DVerts(r,&center); center.x-=0x100000;
    if (center.x<0x40000) { ++s->nearCulled; return 1; }
    anim=(o->info&FIXED_ANIM)?((o->info&0x1e00U)>>9):0U;
    if (!loadFrame(c,o->logical,anim,s)) return 0;
    min=frame.xMin-32; max=frame.xMax-32; memset(&l,0,sizeof(l));
    l.vert1=center; l.vert2.x=center.x; l.vert2.y=center.y+(max<<16);
    l.vert2.z=max-min; l.vert1.y+=min<<16;
    if (!Render_clipLine(r,&l)) { ++s->clipCulled; return 1; }
    Render_projectVertex(r,&l.vert1); Render_projectVertex(r,&l.vert2);
    if (!spans(r,&l,s)) return 0; ++s->draws; return 1;
}

int EspNativeJunctionSprite_render(struct Render_s* renderBase,
                                   EspNativeJunctionSpriteStats* outStats) {
    Render_t* r=(Render_t*)renderBase; const EspMapRuntimeView* rt=EspMapRuntime_view();
    const EspPlayerViewState* v=EspPlayerView_view(); Scratch saved,after; Sources src;
    EspNativeJunctionSpriteStats s; uint32_t n=0,i; int opened=0,ok=0;
    if (outStats) memset(outStats,0,sizeof(*outStats));
    if (!r || !outStats || !rt || !v || !EspMapSpriteTopology_isReady() ||
        !EspNativeGraphicsCatalog_isReady() || EspAssetPack_isOpen() ||
        r->screenWidth!=SCREEN_W || r->columnScale==NULL || r->framebuffer==NULL) return 0;
    memset(&s,0,sizeof(s)); memset(seenLogical,0,sizeof(seenLogical)); saveScratch(r,&saved);
    if (!setupView(r,v) || !rebuildDepth(r,rt,&s) || !buildOrder(r,rt,&s,&n)) goto done;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done; opened=1;
    if (!initSources(&src,&s)) goto done;
    for (i=0;i<n;++i) if (!drawOne(r,&src,&order[i],&s)) goto done;
    ok=s.draws>0U && s.pixelsDrawn>0U;
done:
    if (opened || EspAssetPack_isOpen()) EspAssetPack_close();
    restoreScratch(r,&saved); saveScratch(r,&after);
    if (memcmp(&saved,&after,sizeof(saved))!=0) ok=0;
    *outStats=s; return ok;
}
