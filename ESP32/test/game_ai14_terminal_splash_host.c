/* Actual sequence functions with minimal owner leaves, including fault injection.
 * Test scope: snapshot lifetime and terminal finalization, not full HUD/AI. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { int id; } DoomRPG_t;
typedef struct { int value; } EspNativeGameplayMonsterStateSnapshot;
typedef struct { int value; } EspNativeGameplayMonsterDropSnapshot;
typedef struct { int value; } EspNativeGameplayPlayerState;
typedef struct { int value; } Random_t;
typedef struct { struct { uint8_t active; } pending; int visual; } MonsterCombatOwner;
typedef struct { uint32_t levelUps; } EspNativeGameplayPlayerXpResult;
typedef struct { EspNativeGameplayMonsterStateSnapshot monsters; EspNativeGameplayMonsterDropSnapshot drops; EspNativeGameplayPlayerState player; MonsterCombatOwner combat; Random_t random; } MonsterSplashRollback;
static MonsterSplashRollback splashRollback;
static MonsterCombatOwner combatOwner;
static EspNativeGameplayPlayerXpResult splashLevelUp;
static uint32_t splashLevelUpSequence, splashSequenceNumber, splashAudioSequence;
static uint8_t splashSequenceActive, splashAudioCount;
static EspNativeGameplayMonsterStateSnapshot monsters;
static EspNativeGameplayMonsterDropSnapshot drops;
static EspNativeGameplayPlayerState player;
static Random_t rng;
static unsigned failRestore;
static int syncOwner(void) { return 1; }
static Random_t* EspNativeProcessRng_live(void) { return &rng; }
static int EspNativeGameplayMonsterState_snapshot(EspNativeGameplayMonsterStateSnapshot *s) {*s=monsters;return 1;}
static int EspNativeGameplayMonsterDrop_snapshot(EspNativeGameplayMonsterDropSnapshot *s) {*s=drops;return 1;}
static int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState *s) {*s=player;return 1;}
static int EspNativeGameplayMonsterState_restoreInPlace(const EspNativeGameplayMonsterStateSnapshot *s) {monsters=*s;return !(failRestore&1U);}
static int EspNativeGameplayMonsterDrop_restore(const EspNativeGameplayMonsterDropSnapshot *s) {drops=*s;return !(failRestore&2U);}
static int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState *s) {player=*s;return !(failRestore&4U);}
__RESTORE__
__BEGIN__
__CANCEL__
__COMMIT__
__QUERY__
__TERMINAL__
static void seed(void) {
    memset(&combatOwner,0,sizeof combatOwner);
    memset(&splashLevelUp,0,sizeof splashLevelUp);
    splashSequenceActive=0; splashSequenceNumber=splashLevelUpSequence=splashAudioSequence=0;
    splashAudioCount=0; failRestore=0;
    monsters.value=1; drops.value=2; player.value=3; rng.value=4; combatOwner.visual=5;
}
static void mutate(uint32_t seq,unsigned level) {
    monsters.value=11; drops.value=12; player.value=13; rng.value=14; combatOwner.visual=15;
    splashAudioCount=2;
    if(level){splashLevelUpSequence=seq;splashLevelUp.levelUps=1;}
}
int main(void) {
    DoomRPG_t game={1};
    for(unsigned level=0;level<2;level++) {
        seed(); assert(EspNativeGameplayMonsterCombat_beginSplashSequence(&game,7));
        mutate(7,level);
        assert(EspNativeGameplayMonsterCombat_splashSequenceArmed(7));
        assert(!EspNativeGameplayMonsterCombat_splashSequenceArmed(8));
        assert(!EspNativeGameplayMonsterCombat_splashSequenceArmed(0));
        assert(!EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(8));
        assert(splashSequenceActive && splashAudioCount==2);
        /* Simulated late arm rejection: cancel real sequence, all owners restored. */
        assert(EspNativeGameplayMonsterCombat_cancelSplashSequence(&game));
        assert(monsters.value==1 && drops.value==2 && player.value==3 && rng.value==4 && combatOwner.visual==5);
        assert(!splashSequenceActive && !splashAudioSequence && !splashAudioCount && !splashLevelUpSequence && !splashLevelUp.levelUps);
        seed(); assert(EspNativeGameplayMonsterCombat_beginSplashSequence(&game,7));
        mutate(7,level);
        assert(EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(7));
        assert(!splashSequenceActive && !splashSequenceNumber && !splashLevelUpSequence && !splashLevelUp.levelUps);
        assert(splashAudioSequence==7 && splashAudioCount==2);
        assert(EspNativeGameplayMonsterCombat_cancelSplashSequence(&game));
        assert(monsters.value==11 && drops.value==12 && player.value==13 && rng.value==14 && combatOwner.visual==15);
        assert(!EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(7));
        seed(); assert(EspNativeGameplayMonsterCombat_beginSplashSequence(&game,7));
        mutate(7,level);
        assert(EspNativeGameplayMonsterCombat_commitSplashSequence(7));
        assert(!splashSequenceActive && splashLevelUpSequence==(level?7U:0U));
        assert(splashAudioCount==2);
    }
    for(unsigned failure=1;failure<8;failure++) {
        seed(); assert(EspNativeGameplayMonsterCombat_beginSplashSequence(&game,7));
        mutate(7,1); failRestore=failure;
        assert(!EspNativeGameplayMonsterCombat_cancelSplashSequence(&game));
        assert(!splashSequenceActive && !splashAudioCount && !splashLevelUpSequence);
    }
    puts("GAMEAI14 A2-P1b PASS real sequence snapshot lifetime, late cancel, terminal commit, normal popup, seven restore-fault masks");
    return 0;
}
