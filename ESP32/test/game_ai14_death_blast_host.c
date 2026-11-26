/* Execute the original GSprite update and radius dispatch, with stubbed world leaves. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t byte;
#define MAX_CUSTOM_SPRITES 2
typedef struct { uint32_t info; int x,y; } Sprite;
typedef struct { unsigned flags; int index,frame,time; Sprite *sprite; } GameSprite_t;
typedef struct { int weapon; } Player;
typedef struct { int unused; } Canvas;
typedef struct { uint32_t word; } Random;
typedef struct { Random random; Player *player; Canvas *doomCanvas; } Doom;
typedef struct {
    int activeSprites,gSpriteDurationTime,f684l;
    GameSprite_t gsprites[MAX_CUSTOM_SPRITES];
    Doom *doomRpg;
} Game_t;
static int now;
static unsigned draws,hits,redraws,selects;
static int hx[8],hy[8],hd[8];
static int DoomRPG_GetUpTimeMS(void) { return now; }
static uint32_t DoomRPG_randNextInt(Random *r) { ++draws; return r->word; }
static void DoomCanvas_updateViewTrue(Canvas *c) { assert(c); ++redraws; }
static void Player_selectWeapon(Player *p,int w) { assert(p && w==p->weapon); ++selects; }
static void Game_hurtEntityAt(Game_t *g,int x,int y,int a,int b,int z,int z2) {
    assert(g && hits<8 && a==b && !z && !z2);
    hx[hits]=x; hy[hits]=y; hd[hits]=a; ++hits;
}
__RADIUS__
__UPDATE__
static void test(uint32_t word,unsigned suppress) {
    Player p={2}; Canvas c={0}; Doom d={{word},&p,&c};
    Sprite s={180,672,672}; Game_t g={0};
    g.activeSprites=1; g.f684l=1; g.doomRpg=&d;
    g.gsprites[0]=(GameSprite_t){1U|(suppress?4U:0U),1,-1,1000,&s};
    draws=hits=redraws=selects=0;
    now=999; Game_gsprite_update(&g); assert(g.gsprites[0].frame==-1);
    const int times[]={1000,1149,1150,1299,1300,1449};
    for(unsigned i=0;i<sizeof times/sizeof times[0];i++) {
        now=times[i]; Game_gsprite_update(&g);
        assert(g.gsprites[0].frame==(int)(i/2));
        assert((s.info&0x10000U)==0 && draws==0 && hits==0);
        assert(g.gSpriteDurationTime==1000+150*(int)(i/2+1));
    }
    assert(redraws==3 && selects==0);
    now=1450; Game_gsprite_update(&g);
    assert((s.info&0x10000U)!=0 && (g.gsprites[0].flags&3U)==2U);
    assert(draws==(suppress?0U:1U) && hits==(suppress?0U:8U));
    const int dx[]={-64,64,0,0,-64,64,64,-64};
    const int dy[]={0,0,-64,64,64,64,-64,-64};
    int damage=(int)((word&255U)/11U)+5;
    for(unsigned i=0;i<hits;i++) {
        assert(hx[i]==672+dx[i] && hy[i]==672+dy[i]);
        assert(hd[i]==(i<4?damage:damage/2));
    }
    now=1451; Game_gsprite_update(&g);
    assert(selects==1 && g.activeSprites==0 && g.f684l==0);
    now=2000; Game_gsprite_update(&g);
    assert(selects==1 && draws==(suppress?0U:1U) && hits==(suppress?0U:8U));
}
int main(void) {
    for(unsigned low=0;low<256;low++) for(unsigned suppress=0;suppress<2;suppress++)
        test(0xdeadbe00U|low,suppress);
    puts("GAMEAI14 A2-P0 PASS legacy explosion: 512 timing/RNG/radius cases");
    return 0;
}
