/* Real sprite admission path; visibility/catalog/topology are read-only mocks.
 * Does not prove raster or CYD parity. Compare query counts in both builds. */
#include <assert.h>
#include "../src/esp_native_sprite_renderer.c"

/* The admission test passes NULL for the on-device profiling sink.
 * Resolve the timer symbol without adding a dependency on ESP-IDF. */
int64_t esp_timer_get_time(void) { return 0LL; }

static unsigned mapReads, visualReads, entityReads, liveQueries, previewQueries;
static EspMapSprite fixture = {544, 928, 1};
static uint8_t fixtureVisual;
static int fixtureVisible = 1;
static const EspNativeGraphicsCatalogRecord catalog = {0};

int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* out) {
    assert(index < 2U);
    ++mapReads;
    *out = fixture;
    return 1;
}
int EspMapSpriteTopology_getVisualState(uint32_t index, uint8_t* out) {
    assert(index < 2U);
    ++visualReads;
    *out = fixtureVisual;
    return 1;
}
int EspMapSpriteTopology_getEntity(uint32_t index, uint8_t* type,
                                  uint8_t* subtype, uint16_t* link,
                                  uint16_t* order) {
    assert(index < 2U);
    ++entityReads;
    *type = ENEMY_TYPE;
    *subtype = 0;
    *link = 456U | ESP_MAP_SPRITE_TOPOLOGY_LINKED | ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    *order = 3;
    return 1;
}
int EspNativeBspVisibility_mapSpriteVisibleReadOnly(
    const EspNativeBspVisibilityState* state, uint32_t index, uint32_t* leaf) {
    assert(state != NULL && index < 2U);
    ++previewQueries;
    *leaf = 7;
    return fixtureVisible;
}
int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state, uint32_t index, uint32_t* leaf) {
    assert(state != NULL && index < 2U);
    ++liveQueries;
    *leaf = 7;
    return fixtureVisible;
}
const EspNativeGraphicsCatalogRecord* EspNativeGraphicsCatalog_findSprite(uint16_t id) {
    assert(id == 1);
    return &catalog;
}

int main(void) {
    Render_t render = {0};
    EspMapRuntimeView runtime = {0};
    EspNativeBspVisibilityState visibility = {0};
    EspNativeSpriteStats stats;
    Order order[MAX_VISIBLE_SPRITES];
    uint32_t count;
    runtime.mapSpriteCount = 2;
    render.viewCos_ = 1;
    for (unsigned preview = 0; preview < 2U; ++preview) {
        for (unsigned outcome = 0; outcome < 3U; ++outcome) {
            fixtureVisual = outcome == 0 ? 0x80U : 0;
            fixtureVisible = outcome == 2;
            memset(&stats, 0, sizeof(stats));
            assert(buildOrder(&render, &runtime, &visibility, order, &stats,
                              &count, (int)preview, NULL));
            assert(count == (outcome == 2 ? 2U : 0U));
            assert(stats.hidden == (outcome == 0 ? 2U : 0U));
            assert(stats.bspRejected == (outcome == 1 ? 2U : 0U));
            assert(stats.bspCandidates == (outcome == 2 ? 2U : 0U));
            if (count) {
                assert(order[0].sortZ == 543 && order[1].sortZ == 543);
                assert(order[0].info == fixture.info);
            }
        }
    }
    assert(mapReads == 12U && visualReads == 12U && entityReads == 12U);
    assert(liveQueries == 4U && previewQueries == 4U);
    assert(fixture.x == 544 && fixture.y == 928 && fixture.info == 1);
    puts("sprite_trace_host: PASS hidden/BSP/admitted, preview/live paths, exact read counts and unchanged fixtures");
    return 0;
}
