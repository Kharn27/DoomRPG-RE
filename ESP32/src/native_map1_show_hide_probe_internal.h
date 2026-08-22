#ifndef DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_PROBE_INTERNAL_H
#define DOOMRPG_ESP32_NATIVE_MAP1_SHOW_HIDE_PROBE_INTERNAL_H

#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"

struct DoomRPG_s;
struct Hud_s;
struct DoomCanvas_s;
struct Game_s;

typedef struct Esp32ShowHideTopologyAudit_s {
    uint32_t hasDefCount;
    uint32_t fallbackCount;
    uint32_t entityCount;
    uint32_t linkedCount;
    uint32_t hiddenEntityCount;
    uint32_t enemyCount;
    uint32_t destructibleCount;
    uint32_t topologyFNV;
} Esp32ShowHideTopologyAudit;

typedef struct Esp32ShowHideCorpusAudit_s {
    uint32_t refs;
    uint32_t showRefs;
    uint32_t hideRefs;
    uint32_t removableRefs;
    uint32_t stateExecutorRefused;
    uint32_t rollbackProofs;
    uint32_t showMutated;
    uint32_t hideMutated;
    uint32_t hideNoMutation;
    uint32_t showTargetEntities;
    uint32_t showTargetNoEntity;
    uint32_t showBlockersFound;
    uint32_t showBlockersRemoved;
    uint32_t showBlockerNoops;
    uint32_t showDeferredDeaths;
    uint32_t hideEntitiesTotal;
    uint32_t showResultFNV;
    uint32_t hideResultFNV;
    uint32_t showStateFNV;
    uint32_t hideStateFNV;
    uint32_t initialStateFNV;
    uint32_t showRepeatGuard;
    uint32_t hideIdempotent;
    uint32_t resetProof;
    uint32_t unsupportedRefused;
    uint32_t badOffsetRefused;
    uint32_t badDescriptorRefused;
    uint32_t nullDescriptorRefused;
    uint32_t nullResultRefused;
    EspMapEventDescriptor showDescriptor;
    EspMapEventDescriptor hideDescriptor;
    EspMapEventDescriptor unsupportedDescriptor;
    EspMapByteCode showCommand;
    EspMapByteCode hideCommand;
    EspMapShowResult showResult;
    EspMapHideResult hideResult;
    uint8_t showOffset;
    uint8_t hideOffset;
    uint8_t unsupportedOffset;
    uint8_t haveShow;
    uint8_t haveHide;
    uint8_t haveUnsupported;
} Esp32ShowHideCorpusAudit;

uint32_t Esp32ShowHideProbe_framebufferHash(void);
uint32_t Esp32ShowHideProbe_hudHash(const struct Hud_s* hud);
uint32_t Esp32ShowHideProbe_passwordHash(const struct DoomCanvas_s* canvas);
uint32_t Esp32ShowHideProbe_continuationHash(const struct Game_s* game);
uint32_t Esp32ShowHideProbe_legacyTopologyHash(const struct Game_s* game);

int Esp32ShowHideProbe_boundaryIsSafe(const struct DoomRPG_s* doomRpg);
int Esp32ShowHideProbe_descriptorByIndex(uint32_t index,
                                         EspMapEventDescriptor* outDescriptor);
int Esp32ShowHideProbe_auditInitial(const struct DoomRPG_s* doomRpg,
                                    Esp32ShowHideTopologyAudit* audit);
int Esp32ShowHideProbe_auditCorpus(Esp32ShowHideCorpusAudit* audit);

uint32_t Esp32ShowHideProbe_showResultHash(const EspMapShowResult* result);
uint32_t Esp32ShowHideProbe_hideResultHash(const EspMapHideResult* result);
int Esp32ShowHideProbe_showResultIsZero(const EspMapShowResult* result);
int Esp32ShowHideProbe_hideResultIsZero(const EspMapHideResult* result);

#endif
