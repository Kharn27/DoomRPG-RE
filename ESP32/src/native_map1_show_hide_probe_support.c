#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Entity.h"
#include "EntityDef.h"
#include "Game.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_change_map_probe.h"
#include "native_map1_show_hide_probe_internal.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_BYTES 1024U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_BYTES 81U
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_BYTECODE_COUNT 265U
#define EXPECTED_LINE_COUNT 480U
#define EXPECTED_MAP_SPRITE_COUNT 344U
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_LINE_STATE_BYTES 120U
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_STATE_BYTES 60U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_STORAGE_BYTES 103U
#define EXPECTED_AUTOMAP_STATE_FNV 0x669b1aa7U
#define EXPECTED_TOPOLOGY_PAYLOAD_BYTES (EXPECTED_MAP_SPRITE_COUNT * 7U)

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t hashU16(uint32_t hash, uint16_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t hashU32(uint32_t hash, uint32_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, data[i]);
    return hash;
}

uint32_t Esp32ShowHideProbe_framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();

    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

uint32_t Esp32ShowHideProbe_hudHash(const struct Hud_s* hudOpaque) {
    const Hud_t* hud = (const Hud_t*)hudOpaque;
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (hud == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)hud->msgCount);
    hash = hashU32(hash, (uint32_t)hud->msgTime);
    hash = hashU32(hash, (uint32_t)hud->msgDuration);
    hash = hashU32(hash, (uint32_t)hud->isUpdate);
    hash = hashU32(hash, (uint32_t)(uintptr_t)hud->statBarMessage);
    for (i = 0U; i < (uint32_t)sizeof(hud->messages); ++i) {
        hash = hashByte(hash, ((const uint8_t*)hud->messages)[i]);
    }
    for (i = 0U; i < (uint32_t)sizeof(hud->logMessage); ++i) {
        hash = hashByte(hash, ((const uint8_t*)hud->logMessage)[i]);
    }
    return hash;
}

uint32_t Esp32ShowHideProbe_passwordHash(
    const struct DoomCanvas_s* canvasOpaque) {
    const DoomCanvas_t* canvas = (const DoomCanvas_t*)canvasOpaque;
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (canvas == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)canvas->passwordTime);
    hash = hashByte(hash, (uint8_t)canvas->passInput);
    for (i = 0U; i < (uint32_t)sizeof(canvas->passCode); ++i) {
        hash = hashByte(hash, (uint8_t)canvas->passCode[i]);
    }
    for (i = 0U; i < (uint32_t)sizeof(canvas->strPassCode); ++i) {
        hash = hashByte(hash, (uint8_t)canvas->strPassCode[i]);
    }
    return hash;
}

uint32_t Esp32ShowHideProbe_continuationHash(
    const struct Game_s* gameOpaque) {
    const Game_t* game = (const Game_t*)gameOpaque;
    uint32_t hash = 2166136261U;

    if (game == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)game->skipAdvanceTurn);
    hash = hashU32(hash, (uint32_t)game->saveTileEvent);
    hash = hashU32(hash, (uint32_t)game->tileEvent);
    hash = hashU32(hash, (uint32_t)game->tileEventIndex);
    hash = hashU32(hash, (uint32_t)game->tileEventFlags);
    return hashU32(hash, (uint32_t)(uintptr_t)game->passCode);
}

uint32_t Esp32ShowHideProbe_legacyTopologyHash(
    const struct Game_s* gameOpaque) {
    const Game_t* game = (const Game_t*)gameOpaque;
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (game == NULL) return 0U;
    hash = hashU32(hash, (uint32_t)game->numEntities);
    hash = hashU32(hash, (uint32_t)game->numMonsters);
    hash = hashU32(hash, (uint32_t)game->firstSpecialEntityIndex);
    hash = hashU32(hash, (uint32_t)game->lastSpecialEntityIndex);
    for (i = 0U; i < 1024U; ++i) {
        hash = hashU32(hash, (uint32_t)(uintptr_t)game->entityDb[i]);
    }
    for (i = 0U; i < 400U; ++i) {
        const Entity_t* entity = &game->entities[i];
        hash = hashU32(hash, (uint32_t)entity->info);
        hash = hashU16(hash, (uint16_t)entity->linkIndex);
        hash = hashU32(hash, (uint32_t)(uintptr_t)entity->def);
        hash = hashU32(hash, (uint32_t)(uintptr_t)entity->nextOnTile);
        hash = hashU32(hash, (uint32_t)(uintptr_t)entity->prevOnTile);
        hash = hashU32(hash, (uint32_t)(uintptr_t)entity->monster);
    }
    return hash;
}

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL && canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() && !EspNativeSpriteCache_isActive();
}

int Esp32ShowHideProbe_boundaryIsSafe(const struct DoomRPG_s* doomRpgOpaque) {
    const DoomRPG_t* doomRpg = (const DoomRPG_t*)doomRpgOpaque;
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapAutomapStateView* automapState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->entityDef == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->hud == NULL || doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32Map1ChangeMapProbe_isDone() && runtime != NULL &&
           mapState != NULL && scriptState != NULL && lineState != NULL &&
           textureState != NULL && automapState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           runtime->sourceCrc32 == EXPECTED_INTRO_BSP_CRC32 &&
           runtime->lineCount == EXPECTED_LINE_COUNT &&
           runtime->mapSpriteCount == EXPECTED_MAP_SPRITE_COUNT &&
           runtime->eventCount == EXPECTED_EVENT_COUNT &&
           runtime->byteCodeCount == EXPECTED_BYTECODE_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           fnv1a32(scriptState->storage, scriptState->storageBytes) ==
               EXPECTED_SCRIPT_FNV &&
           lineState->storageBytes == EXPECTED_LINE_STATE_BYTES &&
           lineState->stateFNV1a == EXPECTED_LINE_STATE_FNV &&
           textureState->storageBytes == EXPECTED_TEXTURE_STATE_BYTES &&
           textureState->stateFNV1a == EXPECTED_TEXTURE_STATE_FNV &&
           automapState->storageBytes == EXPECTED_AUTOMAP_STORAGE_BYTES &&
           automapState->stateFNV1a == EXPECTED_AUTOMAP_STATE_FNV &&
           fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                   (uint32_t)sizeof(doomRpg->player->NotebookString)) ==
               EXPECTED_LEGACY_NOTEBOOK_FNV &&
           !EspAssetPack_isOpen() && !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() &&
           doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) &&
           legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0;
}

int Esp32ShowHideProbe_descriptorByIndex(
    uint32_t index,
    EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t value;

    if (outDescriptor == NULL || index > 0xffffU ||
        !EspMapRuntime_getEvent(index, &value)) return 0;
    ref.index = (uint16_t)index;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int initialEntityTile(const EspMapSprite* sprite, uint16_t* outTile) {
    int32_t x;
    int32_t y;

    if (sprite == NULL || outTile == NULL) return 0;
    x = sprite->x;
    y = sprite->y;
    if ((sprite->info & 0x00200000UL) != 0U) x -= 64;
    else if ((sprite->info & 0x00100000UL) != 0U) y -= 64;
    else if ((sprite->info & 0x00080000UL) != 0U) y += 32;
    else if ((sprite->info & 0x00400000UL) != 0U) x += 32;
    x >>= 6;
    y >>= 6;
    if (x < 0 || x >= 32 || y < 0 || y >= 32) return 0;
    *outTile = (uint16_t)(((uint32_t)y * 32U) + (uint32_t)x);
    return 1;
}

int Esp32ShowHideProbe_auditInitial(
    const struct DoomRPG_s* doomRpgOpaque,
    Esp32ShowHideTopologyAudit* audit) {
    const DoomRPG_t* doomRpg = (const DoomRPG_t*)doomRpgOpaque;
    const EspMapSpriteTopologyView* view = EspMapSpriteTopology_view();
    uint32_t i;
    uint32_t lookup;
    uint16_t state;
    uint16_t order;
    uint16_t expectedOrder = 0U;
    uint16_t expectedTile;
    uint8_t type;
    uint8_t subtype;
    uint8_t visual;
    EntityDef_t* def;
    EspMapSprite sprite;

    if (doomRpg == NULL || doomRpg->entityDef == NULL || audit == NULL ||
        view == NULL || view->spriteCount != EXPECTED_MAP_SPRITE_COUNT ||
        view->storageBytes != EXPECTED_TOPOLOGY_PAYLOAD_BYTES) return 0;
    memset(audit, 0, sizeof(*audit));

    for (i = 0U; i < view->spriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapSpriteTopology_getVisualState(i, &visual) ||
            !EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &state, &order)) return 0;
        if (visual != (uint8_t)((sprite.info >> 9) & 0xffU)) return 0;

        if ((sprite.info & 0x01000000UL) != 0U) {
            if (type != 0xffU ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U) return 0;
            continue;
        }

        lookup = sprite.info & 511U;
        if ((sprite.info & 0x00040000UL) != 0U) lookup += 305U;
        def = EntityDef_lookup(doomRpg->entityDef, (int)lookup);
        if (def != NULL) {
            if (type != def->eType || subtype != def->eSubType ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT) == 0U) {
                return 0;
            }
            ++audit->hasDefCount;
        }
        else if ((sprite.info & 0x00020000UL) != 0U) {
            if (type != (uint8_t)(lookup == 216U ? 15U : 14U) ||
                subtype != 0U ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT) != 0U) {
                return 0;
            }
            ++audit->fallbackCount;
        }
        else {
            if (type != 0xffU ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U) return 0;
            continue;
        }

        ++audit->entityCount;
        if (type == ESP_MAP_ENTITY_TYPE_ENEMY) ++audit->enemyCount;
        if (type == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) {
            ++audit->destructibleCount;
        }
        if ((sprite.info & 0x00010000UL) != 0U) {
            if ((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U || order != 0U) {
                return 0;
            }
            ++audit->hiddenEntityCount;
        }
        else {
            if (!initialEntityTile(&sprite, &expectedTile) ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
                (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != expectedTile) {
                return 0;
            }
            ++expectedOrder;
            if (order != expectedOrder) return 0;
            ++audit->linkedCount;
        }
    }

    if (audit->entityCount != view->entityCount ||
        audit->linkedCount != view->linkedCount ||
        audit->enemyCount != view->enemyCount ||
        audit->destructibleCount != view->destructibleCount ||
        view->nextLinkOrder != expectedOrder) return 0;
    audit->topologyFNV = view->stateFNV1a;
    return 1;
}

uint32_t Esp32ShowHideProbe_showResultHash(const EspMapShowResult* result) {
    uint32_t hash = 2166136261U;
    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->spriteIndex);
    hash = hashU16(hash, result->tileIndex);
    hash = hashU16(hash, result->blocker0SpriteIndex);
    hash = hashU16(hash, result->blocker1SpriteIndex);
    hash = hashU16(hash, result->effectFlags);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->showFlags);
    hash = hashByte(hash, result->visualBefore);
    hash = hashByte(hash, result->visualAfter);
    hash = hashByte(hash, result->blockersFound);
    hash = hashByte(hash, result->blockersRemoved);
    hash = hashByte(hash, result->blockerNoops);
    hash = hashByte(hash, result->targetHasEntity);
    hash = hashByte(hash, result->targetLinkedBefore);
    hash = hashByte(hash, result->targetLinkedAfter);
    hash = hashByte(hash, result->legacyReturnValue);
    return hashByte(hash, result->removeCommandIfHandled);
}

uint32_t Esp32ShowHideProbe_hideResultHash(const EspMapHideResult* result) {
    uint32_t hash = 2166136261U;
    if (result == NULL) return 0U;
    hash = hashU16(hash, result->sourceEventIndex);
    hash = hashU16(hash, result->globalCommandIndex);
    hash = hashU16(hash, result->tileIndex);
    hash = hashU16(hash, result->firstHiddenSpriteIndex);
    hash = hashU16(hash, result->lastHiddenSpriteIndex);
    hash = hashU16(hash, result->hiddenEntityCount);
    hash = hashByte(hash, result->sourceCommandOffset);
    hash = hashByte(hash, result->tileX);
    hash = hashByte(hash, result->tileY);
    hash = hashByte(hash, result->legacyReturnValue);
    hash = hashByte(hash, result->removeCommandIfHandled);
    return hashByte(hash, result->effectFlags);
}

int Esp32ShowHideProbe_showResultIsZero(const EspMapShowResult* result) {
    EspMapShowResult zero;
    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}

int Esp32ShowHideProbe_hideResultIsZero(const EspMapHideResult* result) {
    EspMapHideResult zero;
    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}
