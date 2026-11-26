/* A2-P0: compile the actual post-drop special-death tail from Entity_died.
 * World leaves are fixtures: this establishes legacy contract, not native parity. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct { int eSubType; } Def;
typedef struct { int x,y; } Monster;
typedef struct Entity Entity_t;
typedef struct { uint32_t info; int x,y; Entity_t *ent; } Sprite_t;
typedef struct { int time; } GameSprite_t;
typedef struct { Sprite_t mapSprites[3]; } Render;
typedef struct { int destX,destY; } Canvas;
typedef struct { int god; } Player;
typedef struct { int dummy; } Game_t;
typedef struct { Render *render; } Doom;
struct Entity { Def *def; uint32_t info; Doom *doomRpg; Monster *monster; };

static const int dx[5]={0,-64,64,0,0}, dy[5]={0,0,0,-64,64};
static Game_t g;
static Render render;
static Canvas canvas;
static Player pl;
static Doom doom={&render};
static Def def;
static Monster monsters[3];
static Entity_t entities[3], blocker;
static GameSprite_t anim[3];
static int ax[3],ay[3],eventX,eventY,eventMask;
static unsigned occupied,queries,queryOrder[5],links,relinks,animations,events;
static unsigned linkOrder[2], calls[8], ncalls;
static int lx[2],ly[2];
static void record(unsigned v) { assert(ncalls<8); calls[ncalls++]=v; }
static Entity_t *Game_findMapEntityXYFlag(Game_t *game,int x,int y,int mask) {
    assert(game==&g && mask==0xFF87);
    for(unsigned i=0;i<5;i++) if(x==672+dx[i] && y==672+dy[i]) {
        assert(queries<5); queryOrder[queries++]=i;
        return occupied&(1U<<i)?&blocker:NULL;
    }
    assert(0); return NULL;
}
static void Render_relinkSprite(Render *r, Sprite_t *s) {
    assert(r==&render && relinks<2);
    assert(s==&render.mapSprites[1] || s==&render.mapSprites[2]);
    ++relinks; record(1);
}
static void Game_linkEntity(Game_t *game,Entity_t *e,int x,int y) {
    assert(game==&g && links<2);
    assert(e==&entities[1] || e==&entities[2]);
    assert(relinks==links+1);
    linkOrder[links]=(unsigned)(e-entities);
    lx[links]=x; ly[links]=y; ++links; record(2);
}
static void Game_executeTile(Game_t *game,int x,int y,int mask) {
    assert(game==&g && events==0); ++events;
    eventX=x; eventY=y; eventMask=mask; record(3);
}
static GameSprite_t *Game_gsprite_allocAnim(Game_t *game,int type,int x,int y) {
    assert(game==&g && type==1 && animations<3);
    ax[animations]=x; ay[animations]=y; anim[animations].time=1000;
    record(4); return &anim[animations++];
}
static void runLegacy(void) {
    Entity_t *entity=&entities[0];
    Sprite_t *sprite=&render.mapSprites[0],*sprTmp;
    Game_t *game=&g;
    Canvas *doomCanvas=&canvas;
    Player *player=&pl;
    GameSprite_t *gSprite;
    switch(1) {
        case 1:
__LEGACY_TAIL__
            break;
        default: assert(0);
    }
}
static void init(int subtype,int gib,unsigned blocked,int playerTile,int already) {
    memset(&render,0,sizeof render); memset(entities,0,sizeof entities);
    memset(monsters,0,sizeof monsters); memset(&pl,0,sizeof pl);
    memset(anim,0,sizeof anim);
    occupied=blocked; queries=links=relinks=animations=events=ncalls=0;
    eventX=eventY=eventMask=0;
    def.eSubType=subtype;
    for(unsigned i=0;i<3;i++) {
        entities[i].def=&def; entities[i].doomRpg=&doom;
        entities[i].monster=&monsters[i]; entities[i].info=i+1;
        render.mapSprites[i].ent=&entities[i];
        render.mapSprites[i].x=render.mapSprites[i].y=32;
        monsters[i].x=monsters[i].y=32;
        render.mapSprites[i].info=0x10000U;
    }
    entities[0].info|=(gib?0x20000U:0U)|(already?0x10000000U:0U);
    render.mapSprites[0].x=render.mapSprites[0].y=672;
    render.mapSprites[0].info=0x800U|(gib?0x10000U:0x1000000U)|0x20U;
    canvas.destX=playerTile<0?32:672+dx[playerTile];
    canvas.destY=playerTile<0?32:672+dy[playerTile];
}
static void child(unsigned id,int dest,unsigned *link) {
    Sprite_t *s=&render.mapSprites[id];
    if(dest<0) {
        assert((entities[id].info&0x20000U)!=0);
        assert((s->info&0x10000U)!=0);
        assert(s->x==32 && s->y==32);
    } else {
        assert((entities[id].info&0x20000U)==0);
        assert((s->info&0x10000U)==0);
        assert(s->x==672+dx[dest] && s->y==672+dy[dest]);
        assert(monsters[id].x==s->x && monsters[id].y==s->y);
        assert(*link<links && linkOrder[*link]==id);
        assert(lx[*link]==s->x/64 && ly[*link]==s->y/64);
        ++*link;
    }
}
static void spawnCase(int gib,unsigned blocked,int playerTile,int already) {
    init(7,gib,blocked,playerTile,already); runLegacy();
    assert((entities[0].info&0x10000000U)!=0);
    assert(events==0 && animations==0 && pl.god==0);
    if(already) {
        assert(queries==0 && links==0 && relinks==0 && ncalls==0);
        assert(entities[1].info==2 && entities[2].info==3);
        assert(render.mapSprites[1].info==0x10000U && render.mapSprites[2].info==0x10000U);
        return;
    }
    int first=(blocked&1U)?-1:0, second=-1;
    unsigned n=1, link=0;
    assert(queryOrder[0]==0);
    for(int i=1;i<5;i++) {
        assert(n<queries && queryOrder[n++]==(unsigned)i);
        if(!(blocked&(1U<<i)) && playerTile!=i) { second=i; break; }
    }
    assert(n==queries);
    child(1,first,&link); child(2,second,&link);
    assert(link==links && links==relinks && ncalls==2*links);
    for(unsigned i=0;i<ncalls;i++) assert(calls[i]==(i%2?2U:1U));
}
static void otherCase(int subtype,int gib) {
    init(subtype,gib,0,-1,0); runLegacy();
    int hidden=gib||subtype==8||subtype==12||subtype==13;
    assert(render.mapSprites[0].info==(0x820U|(hidden?0x10000U:0x1000000U)));
    assert(entities[0].info==(1U|(hidden?0x20000U:0U)));
    assert(queries==0 && links==0 && relinks==0);
    assert(events==(unsigned)(subtype==12||subtype==13));
    if(events) {
        assert(eventX==(subtype==12?0x480:1984));
        assert(eventY==(subtype==12?0x4c0:0));
        assert(eventMask==0x100 && calls[0]==3);
    }
    assert(pl.god==(subtype==12&&!gib));
    unsigned count=subtype==8?1U:(subtype==12&&!gib?3U:0U);
    assert(animations==count && ncalls==events+count);
    for(unsigned i=0;i<count;i++) {
        assert(calls[events+i]==4 && ay[i]==672);
        assert(ax[i]==672+(count==3?(i==0?16:i==1?-16:0):0));
        assert(anim[i].time==1000+(count==3?(i==0?33:i==1?66:0):0));
    }
}
int main(void) {
    unsigned spawn=0,other=0;
    for(int gib=0;gib<2;gib++) {
        for(unsigned mask=0;mask<32;mask++) for(int player=-1;player<5;player++)
            for(int already=0;already<2;already++) {
                spawnCase(gib,mask,player,already); ++spawn;
            }
        for(int subtype=0;subtype<14;subtype++) if(subtype!=7) {
            otherCase(subtype,gib); ++other;
        }
    }
    assert(spawn==768 && other==26);
    printf("GAMEAI14 A2-P0 PASS legacy special death: %u spawn + %u other cases; native publication unproven\n",spawn,other);
    return 0;
}
