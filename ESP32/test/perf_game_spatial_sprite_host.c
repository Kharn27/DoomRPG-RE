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
 i=EspPerfSpatialSprite_next(&idx,i,199); assert(i==60);
 assert(EspPerfSpatialSprite_next(&idx,i,199)==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_first(&idx,198)==5);
 assert(EspPerfSpatialSprite_next(&idx,5,198)==ESP_PERF_SPATIAL_SPRITE_NONE);
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
 assert(EspPerfSpatialSprite_first(&idx,199)==7);
 assert(EspPerfSpatialSprite_next(&idx,7,199)==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_first(&idx,198)==5);
 assert(EspPerfSpatialSprite_first(&idx,900)==2);
 assert(EspPerfSpatialSprite_next(&idx,2,900)==260);
 /* Compare every exact tile against a full ascending scan, including capacity. */
 EspPerfSpatialSprite_end(&idx);
 view.spriteCount=ESP_PERF_SPATIAL_SPRITE_CAPACITY;
 for(unsigned j=0;j<view.spriteCount;j++)
     states[j]=(j%3U)?(uint16_t)(ESP_MAP_SPRITE_TOPOLOGY_LINKED | ((j*73U)%1024U)):0U;
 assert(EspPerfSpatialSprite_begin(&idx));
 for(unsigned tile=0;tile<1024U;tile++) {
     uint16_t cursor=EspPerfSpatialSprite_first(&idx,(uint16_t)tile);
     for(unsigned j=0;j<view.spriteCount;j++) {
         if(!(states[j]&ESP_MAP_SPRITE_TOPOLOGY_LINKED) ||
            (states[j]&ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK)!=tile) continue;
         assert(cursor==j);
         cursor=EspPerfSpatialSprite_next(&idx,cursor,(uint16_t)tile);
     }
     assert(cursor==ESP_PERF_SPATIAL_SPRITE_NONE);
 }
 /* Dense duplicates in all 16 local tiles, including last sprite 511. */
 for(unsigned j=0;j<view.spriteCount;j++)
     states[j]=(uint16_t)(ESP_MAP_SPRITE_TOPOLOGY_LINKED | ((j/4U)%4U)*32U | (j%4U));
 assert(EspPerfSpatialSprite_begin(&idx));
 for(unsigned tile=0;tile<1024U;tile++) {
     uint16_t cursor=EspPerfSpatialSprite_first(&idx,(uint16_t)tile);
     for(unsigned j=0;j<view.spriteCount;j++) {
         if((states[j]&ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK)!=tile) continue;
         assert(cursor==j);
         cursor=EspPerfSpatialSprite_next(&idx,cursor,(uint16_t)tile);
     }
     assert(cursor==ESP_PERF_SPATIAL_SPRITE_NONE);
 }
 assert(EspPerfSpatialSprite_next(&idx,511U,99U)==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_next(&idx,UINT16_MAX,0U)==ESP_PERF_SPATIAL_SPRITE_NONE);
 assert(EspPerfSpatialSprite_next(&idx,0U,1024U)==ESP_PERF_SPATIAL_SPRITE_NONE);
 EspPerfSpatialSprite_end(&idx);
 assert(EspPerfSpatialSprite_next(&idx,0U,0U)==ESP_PERF_SPATIAL_SPRITE_NONE);
 puts("P3e spatial sprite host PASS exact tile/order/packed links/capacity/getter-fail/scope");
 return 0;
}
