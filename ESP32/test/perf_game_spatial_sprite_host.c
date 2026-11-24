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
 states[5]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 198U;
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
 /* A new goal must see a moved entity, a removed entity and a new link. */
 EspPerfSpatialSprite_end(&idx);
 states[2]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 900U;
 states[60]=0;
 states[7]=ESP_MAP_SPRITE_TOPOLOGY_LINKED | 199U;
 assert(EspPerfSpatialSprite_begin(&idx));
 assert(EspPerfSpatialSprite_first(&idx,199)==5);
 assert(idx.next[5]==7);
 assert(idx.next[7]==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_first(&idx,900)==2);
 assert(idx.next[2]==260);
 /* Compare every bucket against a full ascending scan, including capacity. */
 EspPerfSpatialSprite_end(&idx);
 view.spriteCount=ESP_PERF_SPATIAL_SPRITE_CAPACITY;
 for(unsigned j=0;j<view.spriteCount;j++)
     states[j]=(j%3U)?(uint16_t)(ESP_MAP_SPRITE_TOPOLOGY_LINKED | ((j*73U)%1024U)):0U;
 assert(EspPerfSpatialSprite_begin(&idx));
 for(unsigned tile=0;tile<1024U;tile++) {
     uint16_t cursor=EspPerfSpatialSprite_first(&idx,(uint16_t)tile);
     for(unsigned j=0;j<view.spriteCount;j++) {
         if(!(states[j]&ESP_MAP_SPRITE_TOPOLOGY_LINKED) ||
            EspPerfSpatialSprite_bucket(states[j]&ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK)!=
            EspPerfSpatialSprite_bucket((uint16_t)tile)) continue;
         assert(cursor==j);
         cursor=idx.next[cursor];
     }
     assert(cursor==ESP_PERF_SPATIAL_SPRITE_NONE);
 }
 EspPerfSpatialSprite_end(&idx);
 puts("P1i spatial sprite host PASS order/bucket/capacity/getter-fail/scope");
 return 0;
}
