/* Exercise the real SHOW admission/commit/rollback kernel. Seed only its
 * private compact storage; runtime/immutable command reads are mocked. */
#include <assert.h>
#include <stdio.h>
#include "../src/esp_map_sprite_topology.c"

static uint8_t storage[14];
static EspMapRuntimeView runtime;
static EspMapEventDescriptor descriptor;
static EspMapByteCode command;
static EspMapSprite sprite;

const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
int EspMapRuntime_getEvent(uint32_t index, uint32_t* out) {
    if (index != 0U) return 0;
    *out = descriptor.value; return 1;
}
int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* out) {
    if (index >= 2U) return 0;
    *out = sprite; return 1;
}
int EspMapEvents_describe(const EspMapEventRef* ref, EspMapEventDescriptor* out) {
    if (ref->index != 0U || ref->value != descriptor.value) return 0;
    *out = descriptor; return 1;
}
int EspMapEvents_getCommand(const EspMapEventDescriptor* d, uint32_t offset, EspMapByteCode* out) {
    if (d->eventIndex != 0U || offset != 0U) return 0;
    *out = command; return 1;
}
static void seed(void) {
    memset(storage, 0, sizeof(storage)); memset(&runtime, 0, sizeof(runtime));
    memset(&topologyView, 0, sizeof(topologyView));
    runtime.mapSpriteCount = 2;
    topologyStorage = storage; entityTypes = storage; entitySubTypes = storage+2;
    visualStates = storage+4; linkStatesLE = storage+6; linkOrdersLE = storage+10;
    topologyView.storage = storage; topologyView.storageBytes = sizeof(storage);
    topologyView.spriteCount = 2;
    entityTypes[0] = 12; entityTypes[1] = ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE;
    visualStates[0] = VISUAL_HIDDEN;
    setLinkState(0, 675U | ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
        ESP_MAP_SPRITE_TOPOLOGY_EXISTS | ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
    memset(&descriptor,0,sizeof(descriptor)); descriptor.tileIndex = 675;
    descriptor.value = 675U | (1U << 19); descriptor.commandCount = 1;
    descriptor.commandEndIndex = 1;
    command.id = ESP_MAP_OPCODE_SHOW; command.arg1 = 0x00030000U; command.arg2 = 0x100U;
    sprite.x = 3U*64U+32U; sprite.y = 21U*64U+32U;
    refreshView();
}
static void unchangedPreview(EspMapSpriteTopologyStatus expected) {
    uint8_t before[14]; EspMapSpriteTopologyView prior = topologyView;
    EspMapShowResult result;
    memcpy(before,storage,sizeof(storage));
    assert(EspMapSpriteTopology_previewShow(&descriptor,0,&result) == expected);
    assert(memcmp(before,storage,sizeof(storage)) == 0);
    assert(memcmp(&prior,&topologyView,sizeof(prior)) == 0);
}
int main(void) {
    EspMapShowResult result;
    uint8_t before[14]; EspMapSpriteTopologyView prior;
    seed(); unchangedPreview(ESP_MAP_SPRITE_TOPOLOGY_OK);
    memcpy(before, storage, sizeof(storage)); prior = topologyView;
    assert(EspMapSpriteTopology_applyShow(&descriptor,0,&result) == ESP_MAP_SPRITE_TOPOLOGY_OK);
    assert(visualStates[0] == 3U && (linkStateAt(0) & ESP_MAP_SPRITE_TOPOLOGY_LINKED));
    assert(EspMapSpriteTopology_rollbackShow(&result));
    assert(memcmp(before,storage,sizeof(storage)) == 0);
    assert(memcmp(&prior,&topologyView,sizeof(prior)) == 0);
    seed(); setLinkState(0, linkStateAt(0) | ESP_MAP_SPRITE_TOPOLOGY_LINKED);
    unchangedPreview(ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED);
    seed(); topologyView.nextLinkOrder = UINT16_MAX;
    unchangedPreview(ESP_MAP_SPRITE_TOPOLOGY_ORDER_EXHAUSTED);
    seed(); sprite.x = 2048;
    unchangedPreview(ESP_MAP_SPRITE_TOPOLOGY_OUT_OF_RANGE);
    seed();
    entitySubTypes[1] = ESP_MAP_ENTITY_SUBTYPE_CRATE;
    setLinkState(1,675U | ESP_MAP_SPRITE_TOPOLOGY_LINKED | ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
    setLinkOrder(1,1);
    unchangedPreview(ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER);
    memcpy(before,storage,sizeof(storage));
    assert(EspMapSpriteTopology_applyShow(&descriptor,0,&result) == ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER);
    assert(memcmp(before,storage,sizeof(storage)) == 0);
    puts("show_preview_host: PASS admission read-only, live commit/rollback, linked/order/coordinate/random guards");
    return 0;
}
