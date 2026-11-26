/* Compare actual continuation trace with extracted original Game_trace. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if CONTINUATION
#include "../src/esp_native_gameplay_monster_three_goal_turn.c"
#else
#include "../src/esp_native_gameplay_monster_movement.c"
static EspPlayerViewState player;
const EspPlayerViewState* EspPlayerView_view(void) { return &player; }
#endif

static EspMapRuntimeView rt;
static EspMapSpriteTopologyView topo;
static EspMapSprite plane;
static uint16_t planeTile;
static uint8_t planeType;
static int readFail;
const EspMapRuntimeView* EspMapRuntime_view(void) { return &rt; }
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return &topo; }
int EspMapState_getTileFlags(uint32_t tile,uint8_t* out) {
    if(tile>=1024) return 0;
    *out=0; return 1;
}
int EspMapRuntime_getLine(uint32_t i,EspMapLine* out) { (void)i;(void)out;return 0; }
int EspMapLineState_getOpen(uint32_t i,uint8_t* out) { (void)i;(void)out;return 0; }
int EspEntityDefTypeCatalog_isReady(void) { return 1; }
int EspEntityDefTypeCatalog_getTypeAndSubtype(uint16_t id,uint8_t* type,uint8_t* sub) {
    (void)id;(void)type;(void)sub;return 0;
}
int EspMapRuntime_getMapSprite(uint32_t i,EspMapSprite* out) {
    if(i!=1 || readFail) return 0;
    *out=plane;return 1;
}
int EspMapSpriteTopology_getEntity(uint32_t i,uint8_t* type,uint8_t* sub,
                                  uint16_t* state,uint16_t* order) {
    if(i>=2) return 0;
    *type=i?planeType:1; *sub=0;
    *state=(i?planeTile:330U)|ESP_MAP_SPRITE_TOPOLOGY_LINKED;
    *order=(uint16_t)(i+1);return 1;
}

/* Minimal legacy fixture: actual linked-list traversal, not a rewritten
 * plane predicate. There are no wall/line entities in this corpus. */
typedef struct RefDef { byte eType; } RefDef;
typedef struct RefEntity { RefDef* def; unsigned int info; struct RefEntity* nextOnTile; } RefEntity;
typedef struct RefSprite { int x,y,info; } RefSprite;
typedef struct RefRender { RefSprite mapSprites[1]; } RefRender;
typedef struct RefCanvas { int destX,destY; } RefCanvas;
typedef struct RefRoot { RefCanvas* doomCanvas; RefRender* render; } RefRoot;
typedef struct RefGame {
    RefRoot* doomRpg;
    RefEntity entities[4];
    RefEntity* entityDb[1024];
    RefEntity* traceEntities[8];
    int numTraceEntities;
} RefGame;
#define Game_t RefGame
#define Entity_t RefEntity
#define Sprite_t RefSprite
#include "legacy_trace.inc"
#undef Game_t
#undef Entity_t
#undef Sprite_t

int main(void) {
    RefGame g={0};
    RefCanvas canvas={2016,2016}; RefRender render={0};
    RefRoot root={&canvas,&render}; RefDef def={14};
    g.doomRpg=&root; g.entities[2].def=&def; g.entities[2].info=1;
    topo.spriteCount=2;
#if !CONTINUATION
    player.active=1;
    player.viewX=player.destX=2016;
    player.viewY=player.destY=2016;
#endif
    const uint32_t flags[]={0x600000U,0x20000U,0x220000U,0x1a0000U,0x7a0000U};
    const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    const int offsets[]={-8,0,8,32,56,64,72};
    unsigned cases=0, blocked=0, clear=0, differences=0;
    for(unsigned type=14;type<=15;type++)
    for(unsigned f=0;f<sizeof(flags)/sizeof(flags[0]);f++)
    for(unsigned d=0;d<4;d++)
    for(unsigned cell=0;cell<2;cell++)
    for(unsigned o=0;o<sizeof(offsets)/sizeof(offsets[0]);o++)
    for(unsigned excluded=0;excluded<2;excluded++) {
        int sx=672,sy=672,dx=sx+64*dirs[d][0],dy=sy+64*dirs[d][1];
        int minX=sx<dx?sx:dx,minY=sy<dy?sy:dy;
        plane=(EspMapSprite){(uint16_t)(minX-32+offsets[o]),
                             (uint16_t)(minY-32+offsets[o]),flags[f]};
        planeType=def.eType=(uint8_t)type;
        planeTile=(uint16_t)(cell?(dy/64)*32+dx/64:330);
        memset(g.entityDb,0,sizeof(g.entityDb));
        g.entityDb[planeTile]=&g.entities[2];
        render.mapSprites[0]=(RefSprite){plane.x,plane.y,(int)plane.info};
        uint16_t mask=(uint16_t)(0xf387U & (excluded?~(1U<<type):0xffffU));
        int results[2];
        for(int origin=0;origin<2;origin++) {
            int nudge=origin?32:0;
            RefGame_trace(&g,sx-nudge,sy-nudge,dx-nudge,dy-nudge,&g.entities[3],mask);
            int expected=g.numTraceEntities==0;
            int actual=cardinalTraceClear(sx,sy,dx,dy,0,mask,origin);
            if(actual!=expected) {
                fprintf(stderr,"plane mismatch type=%u flags=%08x direction=%u cell=%u offset=%d excluded=%u origin=%d expected=%d actual=%d\n",
                        type,(unsigned)flags[f],d,cell,offsets[o],excluded,origin,expected,actual);
                return 1;
            }
            results[origin]=actual;
            blocked+=(actual==0);clear+=(actual==1);++cases;
        }
        differences+=(results[0]!=results[1]);
    }
    assert(cases==2240 && blocked>0 && clear>0 && differences>0);
    planeType=14;planeTile=331;plane=(EspMapSprite){704,672,0x220000U};
    readFail=1;assert(cardinalTraceClear(672,672,736,672,0,0xf387U,1)==-1);
    readFail=0;
    assert(cardinalTraceClear(672,672,800,672,0,0xf387U,1)==-2);
    printf("PASS GAME-AI-14 A4 special-plane original Game_trace continuation=%d cases=%u blocked=%u clear=%u origin-differences=%u\n",
           CONTINUATION,cases,blocked,clear,differences);
    return 0;
}
