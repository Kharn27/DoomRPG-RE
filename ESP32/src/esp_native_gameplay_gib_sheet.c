#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp_native_gameplay_gib_sheet.h"

uint16_t EspNativeGibSheet_berserk565(uint16_t original) {
    uint16_t r = (original >> 11U) & 31U;
    r = r + 8U > 31U ? 31U : r + 8U;
    return (uint16_t)((r << 11U) |
                      (((original & 0xf7dfU) >> 1U) & 0x7ffU));
}

/* Recovered from ParticleSystem_startup() for a 160x80 raster:
 * (160<<16)/(128<<8)=320, (80<<16)/(88<<8)=232.
 * Source applies 174/256 once PER distance step after initial 174.
 * A helper avoids C signed-left-shift UB for source negative values. */
#define GIB_LEGACY_SCALE_X 320
#define GIB_LEGACY_SCALE_Y 232

static int32_t gibArshift(int64_t value, unsigned shift) {
    const int64_t divisor = (int64_t)1 << shift;
    if (value >= 0) return (int32_t)(value / divisor);
    return (int32_t)(-(((-value) + divisor - 1) / divisor));
}

static uint32_t gibNextVisual(uint32_t* seed) {
    uint32_t x = *seed;
    if (x == 0U) x = 0x6d2b79f5U;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *seed = x;
    return x;
}

static int32_t gibScaled(int32_t value, int32_t scale) {
    /* Equivalent to source (((value<<8)*scale)+32768)>>16. */
    return gibArshift((int64_t)value*256*scale + 32768,16U);
}

uint8_t EspNativeGibSheet_tileDistance(uint32_t worldDist) {
    if (worldDist <= 4096U) return 1U;
    if (worldDist <= 16384U) return 2U;
    if (worldDist <= 36864U) return 3U;
    if (worldDist <= 65536U) return 4U;
    return 0U;
}

int EspNativeGibSheet_sampleParticle(uint32_t visualSeed,
                                      uint8_t index,
                                      uint8_t tileDistance,
                                      uint32_t elapsedMs,
                                      EspNativeGibKineticPoint* outPoint) {
    int32_t scale,gravity;
    int32_t minX,maxX,minY,maxY,minVX,maxVX,minVY,maxVY;
    int32_t positionX=0,positionY=0,velocityX=0,velocityY=0;
    int32_t gravityAccum=0;
    uint32_t seed=visualSeed;
    uint32_t age=elapsedMs;
    uint8_t baseRow,i;
    if (outPoint != NULL) memset(outPoint,0,sizeof *outPoint);
    if (outPoint == NULL || index >= ESP_NATIVE_GIB_KINETIC_COUNT ||
        tileDistance < 1U || tileDistance > 4U ||
        age >= ESP_NATIVE_GIB_KINETIC_LIFETIME_MS) return 0;
    scale=GIB_LEGACY_SCALE_X;
    if (tileDistance>1U) {
        int32_t part=174;
        uint8_t count;
        for (count=1U; count<tileDistance; ++count)
            part=(part*174)>>8U;
        scale=(GIB_LEGACY_SCALE_X*part)>>8U;
    }
    minX=gibScaled(-6,scale);maxX=gibScaled(6,scale);
    minY=gibScaled(-9,scale);maxY=gibScaled(11,scale);
    minVX=gibScaled(-150,scale);maxVX=gibScaled(100,scale);
    minVY=gibScaled(-175,scale);maxVY=gibScaled(-75,scale);
    gravity=gibScaled(10,scale);
    if (maxX<minX || maxY<minY || maxVX<minVX || maxVY<minVY)
        return 0;

    /* Source consumes one introductory modulo-six draw, then precisely
     * four low-16 random fields PER negative-sized gib. Its fifth size
     * draw is skipped for the five source z=true chunks. */
    baseRow=(uint8_t)(gibNextVisual(&seed)%6U);
    for (i=0U;i<=index;++i) {
        uint32_t rx=gibNextVisual(&seed)&0xffffU;
        uint32_t ry=gibNextVisual(&seed)&0xffffU;
        uint32_t rvx=gibNextVisual(&seed)&0xffffU;
        uint32_t rvy=gibNextVisual(&seed)&0xffffU;
        const int32_t nativeVX=minVX+
            (int32_t)(rvx%(uint32_t)(maxVX-minVX+1U));
        positionX=(minX+(int32_t)(rx%(uint32_t)(maxX-minX+1U)))*256;
        positionY=(minY+(int32_t)(ry%(uint32_t)(maxY-minY+1U)))*256;
        velocityX=(int32_t)(((int64_t)nativeVX*65536)/25600);
        velocityY=(int32_t)(((int64_t)(minVY+
                          (int32_t)(rvy%(uint32_t)(maxVY-minVY+1U)))*65536)/25600);
    }

    /* Source ParticleSystem_render() uses i2=(deltaMs<<16)/2560;
     * update X, then accumulated gravity, then Y for each display tick.
     * Its result is dependent on actual FPS. We choose 25ms deterministic
     * ticks and one exact remainder to make replay independent of whether
     * the turn-based CYD happened to present intermediate frames. */
    while (age != 0U) {
        const uint32_t delta=age>ESP_NATIVE_GIB_KINETIC_STEP_MS
            ?ESP_NATIVE_GIB_KINETIC_STEP_MS:age;
        const int32_t i2=(int32_t)((delta*65536U)/2560U);
        positionX+=gibArshift((int64_t)velocityX*i2,8U);
        gravityAccum+=gibArshift(
            (int64_t)gravity*gibArshift((int64_t)i2*i2,8U),8U);
        positionY+=gravityAccum+gibArshift((int64_t)velocityY*i2,8U);
        age-=delta;
    }
    outPoint->centerX=(int16_t)(80+gibArshift(
        (int64_t)positionX*GIB_LEGACY_SCALE_X,16U));
    outPoint->centerY=(int16_t)(60+gibArshift(
        (int64_t)positionY*GIB_LEGACY_SCALE_Y,16U));
    outPoint->velocityX=(int16_t)velocityX;
    outPoint->variant=(uint8_t)((baseRow+index+1U)%6U);
    return 1;
}

int EspNativeGibSheet_plan(uint32_t presentMs,
                           uint8_t row,
                           int16_t centerX,
                           int16_t centerY,
                           int16_t velX,
                           uint16_t sheetWidth,
                           uint16_t sheetHeight,
                           EspNativeGibSheetRect* outRect) {
    int32_t sx, sy, x, y, w, h;
    uint8_t frame;
    if (outRect == NULL) return 0;
    memset(outRect,0,sizeof *outRect);
    if (row >= ESP_NATIVE_GIB_SHEET_ROWS ||
        sheetWidth < ESP_NATIVE_GIB_SHEET_WIDTH ||
        sheetHeight < ESP_NATIVE_GIB_SHEET_HEIGHT) return 0;
    frame = (uint8_t)((presentMs / ESP_NATIVE_GIB_FRAME_MS) & 3U);
    if (velX > 0) frame = 3U - frame;
    sx = (int32_t)frame * ESP_NATIVE_GIB_SHEET_FRAME_WH;
    sy = (int32_t)row * ESP_NATIVE_GIB_SHEET_FRAME_WH;
    x = (int32_t)centerX - 12;
    y = (int32_t)centerY - 12;
    w = 24; h = 24;
    /* No writes to HUD bars. Source rectangle follows each crop exactly. */
    if (x < 0) { sx -= x; w += x; x = 0; }
    if (y < 20) { sy += 20 - y; h -= 20 - y; y = 20; }
    if (x + w > 160) w = 160 - x;
    if (y + h > 100) h = 100 - y;
    if (w <= 0 || h <= 0 || sx < 0 || sy < 0 ||
        sx + w > sheetWidth || sy + h > sheetHeight) return 0;
    outRect->sourceX = (uint16_t)sx;
    outRect->sourceY = (uint16_t)sy;
    outRect->width = (uint16_t)w;
    outRect->height = (uint16_t)h;
    outRect->destinationX = (int16_t)x;
    outRect->destinationY = (int16_t)y;
    outRect->frame = frame;
    outRect->variant = row;
    return 1;
}
