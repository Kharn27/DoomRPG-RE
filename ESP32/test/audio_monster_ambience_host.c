/* Host-only: all monster families share one audio lifecycle.
 * Sound sight = high-subtype newly activated once; idle = sparse active. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_audio_intent.h"
#include "esp_native_monster_ambience.h"

static EspNativeGameplayMonsterRecord fakeRecords[4];
static EspNativeGameplayMonsterView fakeView;
static uint16_t order[4];
static uint32_t activeCount,restored;
static int soundOn=1;
static uint32_t published;
static uint16_t lastId;
static uint8_t lastPriority;
static uint32_t sightCount,idleCount;

const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void) {
    return fakeView.sourceArenaFNV1a!=0U ? &fakeView : NULL;
}
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(uint16_t sprite) {
    for (unsigned i=0;i<4U;++i)
        if (fakeRecords[i].spriteIndex==sprite) return &fakeRecords[i];
    return NULL;
}
uint32_t EspNativeGameplayMonsterActivation_count(void) {return activeCount;}
uint32_t EspNativeGameplayMonsterActivation_soundRestoreBaseline(void) {return restored;}
int EspNativeGameplayMonsterActivation_getOrdered(uint32_t n,uint16_t* sprite) {
    if (n>=activeCount || sprite==NULL) return 0;
    *sprite=order[n];return 1;
}
int EspNativeAudioIntent_isSoundEnabled(void) {return soundOn;}
int EspNativeAudioIntent_publish(uint16_t id,uint8_t flags,uint8_t priority) {
    (void)flags;
    assert(id!=0U);
    ++published;lastId=id;lastPriority=priority;
    if (priority==3U) ++sightCount;
    if (priority==1U) ++idleCount;
    return 1;
}

static void setup(uint32_t arena) {
    EspNativeMonsterAmbience_service(0U,0,0);
    memset(fakeRecords,0,sizeof(fakeRecords));
    memset(&fakeView,0,sizeof(fakeView));
    fakeView.sourceArenaFNV1a=arena;
    fakeView.count=4U;
    fakeView.records=fakeRecords;
    for (unsigned i=0;i<4U;++i) {
        fakeRecords[i].spriteIndex=(uint16_t)(100U+i);
        fakeRecords[i].alive=1U;
        order[i]=(uint16_t)(100U+i);
    }
    published=0U;lastId=0U;activeCount=0U;restored=0U;
    sightCount=0U;idleCount=0U;soundOn=1;
}
int main(void) {
    setup(0x12345678U);
    fakeRecords[0].mType=3U;fakeRecords[0].subtype=3U; /* Imp, 5070|5071 */
    fakeRecords[1].mType=5U;fakeRecords[1].subtype=5U; /* Pinky, 5093 */
    fakeRecords[2].mType=6U;fakeRecords[2].subtype=6U; /* Malwrath, 5097 */
    fakeRecords[3].mType=1U;fakeRecords[3].subtype=1U; /* Hellhound */
    EspNativeMonsterAmbience_service(100U,1,1);
    assert(published==0U);
    activeCount=2U; /* one visual frame activates Imp+Pinky: highest is Pinky */
    EspNativeMonsterAmbience_service(150U,1,1);
    assert(published==1U && sightCount==1U && lastId==5093U && lastPriority==3U);
    assert(EspNativeMonsterAmbience_sightSettling(101U,150U));
    assert(EspNativeMonsterAmbience_sightSettling(101U,499U));
    assert(!EspNativeMonsterAmbience_sightSettling(101U,500U));
    assert(!EspNativeMonsterAmbience_sightSettling(100U,151U));
    /* A confirmed first door frame pumps sight while idle is disabled.
     * Subsequent animation frames and the later main-loop tick are deduped. */
    EspNativeMonsterAmbience_service(250U,1,0);
    EspNativeMonsterAmbience_service(450U,1,0);
    EspNativeMonsterAmbience_service(1000U,1,1);
    assert(published==1U); /* no duplicate on later door frames or main loop */
    activeCount=3U;
    EspNativeMonsterAmbience_service(1200U,1,1);
    assert(sightCount==2U && lastId==5097U); /* only new Malwrath */
    assert(EspNativeMonsterAmbience_sightSettling(102U,1549U));
    assert(!EspNativeMonsterAmbience_sightSettling(102U,1550U));
    /* Initial timer 8100ms from first gameplay tick. */
    EspNativeMonsterAmbience_service(8100U,1,0);
    assert(idleCount==0U); /* combat/modal defers idle, not sight */
    EspNativeMonsterAmbience_service(8100U,1,1);
    assert(idleCount==1U && lastPriority==1U);
    assert(lastId==5069U || lastId==5108U); /* Imp or Pinky/Malwrath idle */
    EspNativeMonsterAmbience_service(8101U,1,1);
    assert(idleCount==1U); /* bounded interval, not per loop */
    /* SOUND OFF consumes newly activated status without replay later. */
    soundOn=0;activeCount=4U;
    EspNativeMonsterAmbience_service(9500U,1,1);
    soundOn=1;
    EspNativeMonsterAmbience_service(9501U,1,1);
    assert(sightCount==2U);

    /* LOAD restores three already-active monsters; no sight blast. */
    setup(0x12345678U);
    fakeRecords[3].mType=1U;
    fakeRecords[3].subtype=1U;
    activeCount=3U;restored=3U;
    EspNativeMonsterAmbience_service(15000U,1,1);
    assert(published==0U);
    assert(!EspNativeMonsterAmbience_sightSettling(102U,15000U));
    activeCount=4U;
    EspNativeMonsterAmbience_service(15001U,1,1);
    assert(sightCount==1U && lastId==5087U);
    /* New map arena restarts the 8-second initial idle timer. */
    fakeView.sourceArenaFNV1a=0xaabbccddU;activeCount=1U;restored=0U;
    EspNativeMonsterAmbience_service(16000U,1,1);
    assert(sightCount==2U);
    puts("[MONSTERAMBIENT-HOST] PASS sight highest subtype, once, idle timer, sound OFF, LOAD baseline, arena swap");
    return 0;
}
