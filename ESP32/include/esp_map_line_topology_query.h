#ifndef DOOMRPG_ESP32_MAP_LINE_TOPOLOGY_QUERY_H
#define DOOMRPG_ESP32_MAP_LINE_TOPOLOGY_QUERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_LINE_TOPOLOGY_NO_LINE 0xffffU

typedef struct EspMapLineTopologyRef_s {
    uint32_t flags;
    uint16_t lineIndex;
    uint16_t tileIndex;
    uint16_t texture;
    uint16_t entityDefTile;
    uint8_t entityType;
    uint8_t open;
    uint8_t locked;
    uint8_t linked;
} EspMapLineTopologyRef;

/*
 * Resolve the first currently linked line-derived entity on one tile using the
 * same recovered reverse-line ordering, geometry nudge, entity-def fallback
 * and trace-mask semantics as native gameplay collision.
 *
 * Return values:
 *   1  linked line-derived entity returned
 *   0  valid native context, no linked line on this tile
 *  -1  invalid/inconsistent native context
 *
 * Open lines are logically unlinked and therefore skipped. `locked` comes from
 * the mutable EspMapLineState overlay, not from immutable source flags alone.
 */
int EspMapLineTopologyQuery_findLinkedOnTile(
    uint16_t tileIndex,
    EspMapLineTopologyRef* outLine);

#ifdef __cplusplus
}
#endif

#endif
