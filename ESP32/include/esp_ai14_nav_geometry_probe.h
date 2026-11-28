#ifndef ESP_AI14_NAV_GEOMETRY_PROBE_H
#define ESP_AI14_NAV_GEOMETRY_PROBE_H
/* Temporary A4-NAV-P3 witness. Read-only, outside planner timing/cache scope.
 * Four captures per planner/session; radius 3 covers initial cardinal step
 * plus both hypothetical calcPath steps. No allocation, RNG or world write.
 * The line tile callback is the actual native planner mapping, not an oracle.
 */
#include <stdint.h>
#include <stdio.h>
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_map_line_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_special_plane_geometry.h"

static inline int EspAi14NavGeometry_near(uint16_t tile, int x, int y) {
    int dx = (int)(tile & 31U) - x, dy = (int)(tile >> 5) - y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy <= 3;
}

static inline void EspAi14NavGeometry_capture(const char* phase, unsigned sample,
    uint16_t attacker, int32_t sx, int32_t sy,
    int (*lineTileFor)(const EspMapLine*, uint16_t*)) {
    const EspMapRuntimeView* rt = EspMapRuntime_view();
    const EspMapSpriteTopologyView* topo = EspMapSpriteTopology_view();
    unsigned lines = 0, sprites = 0, errors = 0, truncated = 0;
    int x = (int)(sx >> 6), y = (int)(sy >> 6);
    if (!rt || !topo || !lineTileFor || x < 0 || x >= 32 || y < 0 || y >= 32) {
        printf("[AI14GEO] UNAVAILABLE phase=%s sample=%u sprite=%u\n", phase, sample, (unsigned)attacker);
        return;
    }
    printf("[AI14GEO] BEGIN phase=%s sample=%u sprite=%u src=%d,%d arena=%08x radius=3 planeNudge=legacy-render\n",
        phase, sample, (unsigned)attacker, (int)sx, (int)sy, (unsigned)rt->arenaFNV1a);
    for (int yy = y - 3; yy <= y + 3; ++yy) {
        char row[15];
        const char hex[] = "0123456789abcdef";
        for (int xx = x - 3; xx <= x + 3; ++xx) {
            unsigned j = (unsigned)(xx - x + 3) * 2U;
            uint8_t flags = 0;
            row[j] = row[j + 1U] = '-';
            if (xx < 0 || xx >= 32 || yy < 0 || yy >= 32) continue;
            uint16_t tile = (uint16_t)(yy * 32 + xx);
            if (!EspAi14NavGeometry_near(tile, x, y)) continue;
            if (!EspMapState_getTileFlags(tile, &flags)) { ++errors; continue; }
            row[j] = hex[flags >> 4]; row[j + 1U] = hex[flags & 15U];
        }
        row[14] = '\0';
        printf("[AI14GEO] ROW y=%d x0=%d flags=%s\n", yy, x - 3, row);
    }
    for (uint32_t i = 0; i < rt->lineCount; ++i) {
        EspMapLine line; uint16_t tile; uint8_t open, type = 255U, sub = 255U;
        if (!EspMapRuntime_getLine(i, &line)) { ++errors; continue; }
        if (!lineTileFor(&line, &tile)) continue;
        if (!EspAi14NavGeometry_near(tile, x, y)) continue;
        if (!EspMapLineState_getOpen(i, &open)) { ++errors; continue; }
        uint32_t id = 305U + line.texture;
        int has = id < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
            EspEntityDefTypeCatalog_getTypeAndSubtype((uint16_t)id, &type, &sub);
        if (!has) { type = (line.flags & 0x18U) ? 0U : 255U; sub = 255U; }
        if (lines >= 64U) { truncated = 1; continue; }
        ++lines;
        printf("[AI14GEO] LINE id=%u tile=%u open=%u type=%u sub=%u def=%d tex=%u flags=%08x xy=%u,%u,%u,%u\n",
            (unsigned)i, (unsigned)tile, (unsigned)open, (unsigned)type, (unsigned)sub, has,
            (unsigned)line.texture, (unsigned)line.flags, (unsigned)line.x1, (unsigned)line.y1,
            (unsigned)line.x2, (unsigned)line.y2);
    }
    for (uint32_t i = 0; i < topo->spriteCount; ++i) {
        uint8_t type, sub; uint16_t state, order; EspMapSprite sprite;
        if (!EspMapSpriteTopology_getEntity(i, &type, &sub, &state, &order)) { ++errors; continue; }
        if (!(state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) ||
            !EspAi14NavGeometry_near(state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK, x, y)) continue;
        if (!EspMapRuntime_getMapSprite(i, &sprite)) { ++errors; continue; }
        if (sprites >= 64U) { truncated = 1; continue; }
        ++sprites;
        if ((type == 14U || type == 15U) && (sprite.info & 0x00820000UL) == 0x00820000UL) {
            int32_t px, py;
            EspNativeSpecialPlane_legacyPosition(&sprite, &px, &py);
            printf("[AI14NUDGE] sprite=%u type=%u raw=%u,%u trace=%d,%d info=%08x\n",
                (unsigned)i, (unsigned)type, (unsigned)sprite.x, (unsigned)sprite.y,
                (int)px, (int)py, (unsigned)sprite.info);
        }
        printf("[AI14GEO] SPRITE id=%u type=%u sub=%u state=%04x order=%u xy=%u,%u info=%08x\n",
            (unsigned)i, (unsigned)type, (unsigned)sub, (unsigned)state, (unsigned)order,
            (unsigned)sprite.x, (unsigned)sprite.y, (unsigned)sprite.info);
    }
    printf("[AI14GEO] END phase=%s sample=%u sprite=%u lines=%u sprites=%u errors=%u truncated=%u\n",
        phase, sample, (unsigned)attacker, lines, sprites, errors, truncated);
}
#endif
