/* P1h: test pure index builder against a dynamic topology stub. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_perf_spatial_sprite.h"
static EspMapSpriteTopologyView view;
static uint16_t states[ESP_PERF_SPATIAL_SPRITE_CAPACITY];
static int failAt=-1;
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) {return &view;}
int EspMapSpriteTopology_getEntity(uint32_t index,uint8_t* type,
 uint8_t* subtype,uint16_t* state,uint16_t* order) {
 if(index >= view.spriteCount || (int)index == failAt) return 0;
 *type=1;*subtype=0;*state=states[index];*order=(uint16_t)index;
 return 1;
}
int main(void) {
 EspPerfSpatialSpriteIndex idx={0};
 uint16_t i;
 view.spriteCount=278;
 states[2]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 199U;
 states[5]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 200U;
 states[60]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 199U;
 states[260]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 900U;
 assert(EspPerfSpatialSprite_begin(&idx));
 assert(idx.active && idx.spriteCount==278);
 i=EspPerfSpatialSprite_first(&idx,199);
 assert(i==2);
 i=idx.next[i]; assert(i==5); /* same bucket, different exact tile */
 i=idx.next[i]; assert(i==60);
 assert(idx.next[i]==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_first(&idx,900)==260);
 assert(EspPerfSpatialSprite_first(&idx,1024)==ESP_PERF_SPATIAL_SPRITE_NONE);
 EspPerfSpatialSprite_end(&idx);
 assert(EspPerfSpatialSprite_first(&idx,199)==ESP_PERF_SPATIAL_SPRITE_NONE);
 failAt=12;
 assert(!EspPerfSpatialSprite_begin(&idx) && !idx.active);
 failAt=-1;view.spriteCount=513;
 assert(!EspPerfSpatialSprite_begin(&idx) && !idx.active);
 view.spriteCount=278;
 assert(EspPerfSpatialSprite_begin(&idx));
 puts("P1h spatial sprite host PASS order/bucket/capacity/getter-fail/scope");
 return 0;
}
