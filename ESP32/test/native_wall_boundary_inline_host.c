/* Dedicated host harness for the production renderer's two real functions.
 * Production source is injected by native_wall_boundary_inline_host.py. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define WALL_TEXEL_HEADER_BYTES 4U
#define WALL_PACKED_BYTES 2048U
#define LEGACY_WALL_GUARD_LIMIT 4U
#define ESP_NATIVE_GRAPHICS_PALETTE_COLORS 16U

typedef struct { uint32_t nameHash, size; } EspAssetPackEntry;
typedef struct {
    uint16_t logicalId, actualId, paletteSourceOffset, reserved;
    uint32_t sourceTexelOffset;
    uint16_t paletteRgb565[ESP_NATIVE_GRAPHICS_PALETTE_COLORS];
} ResolvedWallTexture;
typedef struct {
    uint16_t logicalId, actualId, successorActualId;
    uint8_t packedByte, valid;
    uint32_t sourceOffset, successorSourceOffset;
} LegacyWallGuard;
typedef uint8_t byte;
typedef struct {
    unsigned short* pixels;
    int pitch, screenLeft, screenRight, screenTop, screenBottom;
    byte* mediaTexels;
    unsigned short* spanPalettes;
} Render_t;
typedef struct {
    Render_t* render;
    EspAssetPackEntry mappings, wallTexels;
    uint32_t spanCalls, pixelsDrawn;
    uint32_t diagnosticLineIndex;
} FirstFrameWork;
typedef struct { uint16_t resourceId; } EspNativeGraphicsCatalogRecord;
typedef struct {
    uint16_t textureCount;
    const EspNativeGraphicsCatalogRecord* textures;
} EspNativeGraphicsCatalogView;

enum {
    FIRST_FRAME_FAIL_NONE, FIRST_FRAME_FAIL_SPAN_SCREEN,
    FIRST_FRAME_FAIL_SPAN_NEGATIVE, FIRST_FRAME_FAIL_SPAN_OOB
};
typedef struct {
    uint32_t code, lineIndex, logicalId, actualId, flags, sourceOffset;
    int32_t value0, value1, value2, value3;
} FirstFrameFailure;
static FirstFrameFailure frameFailure;
static LegacyWallGuard legacyWallGuards[LEGACY_WALL_GUARD_LIMIT];
static uint8_t legacyWallGuardCount;
#define RECORD_FRAME_FAILURE(c,l,g,a,f,s,v0,v1,v2,v3) do { \
    if (frameFailure.code == FIRST_FRAME_FAIL_NONE) { \
        frameFailure.code=(c); frameFailure.lineIndex=(l); \
        frameFailure.logicalId=(g); frameFailure.actualId=(a); \
        frameFailure.flags=(f); frameFailure.sourceOffset=(s); \
        frameFailure.value0=(v0); frameFailure.value1=(v1); \
        frameFailure.value2=(v2); frameFailure.value3=(v3); \
    } \
} while(0)

static uint8_t mappingData[1024];
static uint8_t texelData[65536];
static uint32_t mappingBytes, texelBytes;
static unsigned readCalls;
static uint64_t readBytes;
static byte legacyAtlas[32768];
static int packOpened = 1;
static EspNativeGraphicsCatalogRecord records[2] = {{15}, {16}};
static EspNativeGraphicsCatalogView catalog = {2, records};
static int EspAssetPack_isOpen(void) { return packOpened; }
static int EspNativeGraphicsCatalog_isReady(void) { return 1; }
static const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {
    return &catalog;
}
static int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                  uint32_t offset, void* target, size_t size) {
    const uint8_t* data;
    uint32_t length;
    if (!packOpened || !entry || !target) return 0;
    if (entry->nameHash == 1) { data = mappingData; length = mappingBytes; }
    else if (entry->nameHash == 2) { data = texelData; length = texelBytes; }
    else return 0;
    if (entry->size != length || offset > length || size > length - offset) return 0;
    ++readCalls;
    readBytes += size;
    memcpy(target, data + offset, size);
    return 1;
}

/* PRODUCTION_FUNCTIONS */

static void le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void le32(uint8_t* p, uint32_t v) {
    for (int k = 0; k < 4; ++k) p[k] = (uint8_t)(v >> (8 * k));
}
static void fixture(void) {
    const uint32_t pairs=72, bitPairs=1, ids=20;
    const uint32_t idBase = 16 + pairs * 8 + bitPairs * 8;
    memset(mappingData, 0, sizeof(mappingData));
    memset(texelData, 0, sizeof(texelData));
    le32(mappingData, pairs);
    le32(mappingData+4, bitPairs);
    le32(mappingData+8, ids);
    le32(mappingData+12, 0);
    mappingBytes = idBase + ids * 2;
    le16(mappingData + idBase + 15 * 2, 40);
    le16(mappingData + idBase + 16 * 2, 68);
    const uint32_t a[8] = {40,41,42,43,68,69,70,71};
    const uint32_t offsets[8] = {20480,0,4096,8192,32768,40960,49152,57344};
    for (int i=0;i<8;++i) le32(mappingData + 16 + a[i]*8, offsets[i]);
    texelBytes = sizeof(texelData);
    texelData[WALL_TEXEL_HEADER_BYTES + 32768/2] = 0xb4U;
    readCalls = 0;
    readBytes = 0;
    packOpened = 1;
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(legacyWallGuards, 0, sizeof(legacyWallGuards));
    legacyWallGuardCount = 0;
}
int main(void) {
    fixture();
    uint8_t wall[WALL_PACKED_BYTES];
    memset(wall, 0x21, sizeof(wall));
    unsigned short framebuffer[16];
    unsigned short legacyFrame[16];
    Render_t render={0};
    FirstFrameWork work={0};
    ResolvedWallTexture src={0};
    render.pixels=framebuffer; render.pitch=4; render.screenRight=2;
    render.screenBottom=4; work.diagnosticLineIndex=161U;
    work.render=&render;
    work.mappings.nameHash=1; work.mappings.size=mappingBytes;
    work.wallTexels.nameHash=2; work.wallTexels.size=texelBytes;
    src.logicalId=15; src.actualId=40; src.sourceTexelOffset=20480;
    for(int i=0;i<16;++i) src.paletteRgb565[i]=(uint16_t)(0x1000+i);

    /* The caller's 2048-byte cache cannot read position 2048.
     * A logically adjacent successor texture starts at offset 32768, not
     * immediately after the previous physical PAK range. Verify the
     * resolver selects the mapped successor byte and not an invented clamp. */
    memset(framebuffer, 0, sizeof(framebuffer));
    assert(sampleWallSpan(&work,&src,wall,0,0,2047<<13,1<<13,2));
    assert((uint16_t)framebuffer[0]==0x1001);
    assert((uint16_t)framebuffer[2]==0x1004);
    assert(legacyWallGuardCount==1);
    assert(legacyWallGuards[0].successorActualId==68);
    assert(legacyWallGuards[0].successorSourceOffset==32768);
    assert(legacyWallGuards[0].packedByte==0xb4);
    assert(frameFailure.code==FIRST_FRAME_FAIL_NONE);
    unsigned afterFirst=readCalls;

    /* Upper nibble and same-frame guard reuse without additional I/O. */
    assert(sampleWallSpan(&work,&src,wall,1,0,(2048<<13)+(1<<12),0,1));
    assert((uint16_t)framebuffer[1]==0x100b);
    assert(readCalls==afterFirst && legacyWallGuardCount==1);
    assert(packOpened);

    /* Deterministic mixed-span parity against the legacy nibble oracle. */
    for(uint32_t n=0;n<2000;++n) {
        uint32_t idx=n%2049U;
        uint32_t shift=(n&1U)?4096U:0U;
        uint32_t position=(idx<<13)+shift;
        uint8_t packed=idx==2048U?0xb4:wall[idx];
        uint16_t oracle=src.paletteRgb565[(packed >> ((position>>10)&4U))&15U];
        assert(sampleWallSpan(&work,&src,wall,0,0,(int)position,0,1));
        assert((uint16_t)framebuffer[0]==oracle);
    }
    assert(legacyWallGuardCount==1 && readCalls==afterFirst);

    /* R-A: use the *actual* Render_SpanMode0 body from src/Render.c as
     * the independent nibble/raster oracle (both its scalar and 8-wide
     * unrolled paths). The legacy map-wide atlas is a synthetic logical
     * layout: the boundary byte is at base + 2048; the PAK double above
     * independently proves that its source is actual68/32768, NOT the
     * physically adjacent PAK byte. This is not a real-game asset fixture. */
    for (uint32_t i=0; i<WALL_PACKED_BYTES; ++i)
        wall[i]=(byte)((i*73U+17U)&255U);
    const uint32_t legacyBase=src.sourceTexelOffset/2U;
    assert(legacyBase + WALL_PACKED_BYTES < sizeof(legacyAtlas));
    memset(legacyAtlas, 0xa5, sizeof(legacyAtlas));
    memcpy(legacyAtlas + legacyBase, wall, sizeof(wall));
    legacyAtlas[legacyBase + WALL_PACKED_BYTES] = 0xb4;
    render.mediaTexels=legacyAtlas;
    render.spanPalettes=src.paletteRgb565;
    render.pitch=2;
    render.screenRight=1;
    render.screenBottom=16;
    uint32_t nativeHash=2166136261U, legacyHash=2166136261U;
    for (uint32_t n=0; n<4098U; ++n) {
        const uint32_t index=n/2U;
        const int local=(int)((index<<13)+((n&1U)?4096U:0U));
        render.pixels=framebuffer;
        assert(sampleWallSpan(&work,&src,wall,0,0,local,0,1));
        render.pixels=legacyFrame;
        Render_SpanMode0(&render,0,0,(int)(legacyBase<<13)+local,0,1);
        assert(framebuffer[0]==legacyFrame[0]);
        const uint16_t a=framebuffer[0], b=legacyFrame[0];
        nativeHash=(nativeHash^(uint8_t)a)*16777619U;
        nativeHash=(nativeHash^(uint8_t)(a>>8))*16777619U;
        legacyHash=(legacyHash^(uint8_t)b)*16777619U;
        legacyHash=(legacyHash^(uint8_t)(b>>8))*16777619U;
    }
    assert(nativeHash==legacyHash && nativeHash==0xae456812U);
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(legacyFrame, 0, sizeof(legacyFrame));
    render.pixels=framebuffer;
    assert(sampleWallSpan(&work,&src,wall,0,0,2040<<13,1<<13,9));
    render.pixels=legacyFrame;
    Render_SpanMode0(&render,0,0,(int)(legacyBase<<13)+(2040<<13),
                     1<<13,9);
    for (int i=0; i<9; ++i) assert(framebuffer[i]==legacyFrame[i]);
    assert(legacyWallGuardCount==1 && readCalls==afterFirst);
    const uint64_t firstGuardBytes=readBytes;
    render.pixels=framebuffer;
    render.pitch=4;
    render.screenRight=2;
    render.screenBottom=4;

    /* Beyond the one-byte legacy successor: strict SPAN_OOB, no overwrite. */
    framebuffer[0]=(short)0x5a5a;
    memset(&frameFailure,0,sizeof(frameFailure));
    assert(!sampleWallSpan(&work,&src,wall,0,0,2049<<13,0,1));
    assert(frameFailure.code==FIRST_FRAME_FAIL_SPAN_OOB);
    assert(frameFailure.lineIndex==161U);
    assert((uint16_t)framebuffer[0]==0x5a5a);

    /* Exceeding four distinct boundaries cannot widen the BSS owner. */
    fixture();
    legacyWallGuardCount=LEGACY_WALL_GUARD_LIMIT;
    framebuffer[0]=(short)0x5a5a;
    assert(!sampleWallSpan(&work,&src,wall,0,0,2048<<13,0,1));
    assert(frameFailure.code==FIRST_FRAME_FAIL_SPAN_OOB);
    assert(frameFailure.lineIndex==161U);
    assert((uint16_t)framebuffer[0]==0x5a5a);

    /* Missing/invalid mapping and closed PAK fail closed before painting. */
    fixture();
    work.mappings.size=mappingBytes-1;
    framebuffer[0]=(short)0x5a5a;
    assert(!sampleWallSpan(&work,&src,wall,0,0,2048<<13,0,1));
    assert(frameFailure.code==FIRST_FRAME_FAIL_SPAN_OOB);
    assert(frameFailure.lineIndex==161U);
    assert((uint16_t)framebuffer[0]==0x5a5a);
    fixture();
    work.mappings.size=mappingBytes;
    packOpened=0;
    framebuffer[0]=(short)0x5a5a;
    assert(!sampleWallSpan(&work,&src,wall,0,0,2048<<13,0,1));
    assert(frameFailure.code==FIRST_FRAME_FAIL_SPAN_OOB);
    assert(frameFailure.lineIndex==161U);
    assert((uint16_t)framebuffer[0]==0x5a5a && !packOpened);

    printf("PASS native wall boundary inline spans=2000 parity=4098 golden=ae456812 successor=68 source=32768 guardMax=4 guardIO=%u reads/%llu bytes retry=0\\n", afterFirst, (unsigned long long)firstGuardBytes);
    return 0;
}
