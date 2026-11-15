/* Dedicated host harness for the production renderer's two real functions.
 * Production source is injected by native_wall_boundary_inline_host.py. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

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
/* HOST-ONLY physical legacy byte at source+2048, never allocated on CYD. */
static byte legacyAtlas[65536];
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
/* R-A2a: pack-v2 data + exact map-selected texture order recovered from BSP.
 * The desktop Render_loadTexels reorders and compacts source blocks. Thus
 * legacy compact-atlas adjacency is NOT raw physical PAK adjacency.
 * Compare actual source-extracted functions, reject compact-atlas divergence. */
static int readFixtureBytes(FILE* f, void* p, size_t n) {
    return fread(p, 1U, n, f) == n;
}
static int runPackFixture(const char* path) {
    static const uint8_t signature[8] = {'D','R','P','G','R','2','A','2'};
    uint8_t raw[8];
    uint8_t wall[WALL_PACKED_BYTES];
    uint8_t paletteRaw[32];
    FILE* f = fopen(path, "rb");
    int mismatches = 0;
    if (f == NULL) { fprintf(stderr, "FAIL RA2 PACK fixture-open\n"); return 1; }
    if (!readFixtureBytes(f,raw,8U) || memcmp(raw,signature,8U)!=0 ||
        !readFixtureBytes(f,raw,4U) || readLe32(raw)!=2U) {
        fprintf(stderr, "FAIL RA2 PACK fixture-header\n");
        fclose(f);
        return 1;
    }
    for (int fixtureIndex=0; fixtureIndex<2; ++fixtureIndex) {
        Render_t render = {0};
        FirstFrameWork work = {0};
        ResolvedWallTexture src = {0};
        unsigned short fb[16] = {0}, legacyFb[16] = {0};
        uint8_t physicalByte, mappedByte, compactLegacyByte;
        uint32_t sourceOffset, successorSource, expectedHash;
        uint16_t successorActual;
        uint32_t nativeHash=2166136261U, legacyHash=2166136261U;
        int base;
        if (!readFixtureBytes(f,raw,8U)) goto invalid;
        src.logicalId=readLe16(raw);
        src.actualId=readLe16(raw+2);
        successorActual=readLe16(raw+4);
        if (readLe16(raw+6)!=0U) goto invalid;
        if (!readFixtureBytes(f,raw,8U)) goto invalid;
        sourceOffset=readLe32(raw);
        successorSource=readLe32(raw+4);
        if ((sourceOffset&1U)!=0U || sourceOffset>131072U ||
            successorSource<=sourceOffset || successorActual==src.actualId)
            goto invalid;
        if (!readFixtureBytes(f,paletteRaw,sizeof(paletteRaw)) ||
            !readFixtureBytes(f,wall,sizeof(wall)) ||
            !readFixtureBytes(f,&physicalByte,1U) ||
            !readFixtureBytes(f,&mappedByte,1U) ||
            !readFixtureBytes(f,&compactLegacyByte,1U) ||
            !readFixtureBytes(f,raw,4U)) goto invalid;
        expectedHash=readLe32(raw);
        base=(int)(sourceOffset/2U);
        if (base<0 || (uint32_t)base+WALL_PACKED_BYTES>=sizeof(legacyAtlas))
            goto invalid;
        memcpy(legacyAtlas+base,wall,sizeof(wall));
        /* Desktop Render_loadTexels sorts the map-selected 4-phase textures
         * and copies 2048-byte blocks into its COMPACT atlas. Never use
         * the raw PAK-adjacent byte as the desktop oracle. */
        legacyAtlas[base+WALL_PACKED_BYTES]=compactLegacyByte;
        for (int i=0;i<16;++i)
            src.paletteRgb565[i]=readLe16(paletteRaw+i*2);
        src.sourceTexelOffset=sourceOffset;

        memset(legacyWallGuards,0,sizeof(legacyWallGuards));
        legacyWallGuardCount=1U;
        legacyWallGuards[0].valid=1U;
        legacyWallGuards[0].logicalId=src.logicalId;
        legacyWallGuards[0].actualId=src.actualId;
        legacyWallGuards[0].sourceOffset=sourceOffset;
        legacyWallGuards[0].successorActualId=successorActual;
        legacyWallGuards[0].successorSourceOffset=successorSource;
        legacyWallGuards[0].packedByte=mappedByte;
        memset(&frameFailure,0,sizeof(frameFailure));
        render.screenRight=1;
        render.screenBottom=16;
        render.pitch=2;
        render.mediaTexels=legacyAtlas;
        render.spanPalettes=src.paletteRgb565;
        work.render=&render;
        for (uint32_t nibble=0;nibble<4096U;++nibble) {
            const int local=(int)(((nibble/2U)<<13)+((nibble&1U)<<12));
            render.pixels=fb;
            if (!sampleWallSpan(&work,&src,wall,0,0,local,0,1)) goto invalid;
            render.pixels=legacyFb;
            Render_SpanMode0(&render,0,0,(base<<13)+local,0,1);
            if (fb[0]!=legacyFb[0]) goto invalid;
            nativeHash=(nativeHash^(uint8_t)fb[0])*16777619U;
            nativeHash=(nativeHash^(uint8_t)(fb[0]>>8))*16777619U;
            legacyHash=(legacyHash^(uint8_t)legacyFb[0])*16777619U;
            legacyHash=(legacyHash^(uint8_t)(legacyFb[0]>>8))*16777619U;
        }
        if (nativeHash!=expectedHash || nativeHash!=legacyHash) goto invalid;
        for (int high=0;high<2;++high) {
            const int local=(2048<<13)+(high<<12);
            render.pixels=fb;
            if (!sampleWallSpan(&work,&src,wall,0,0,local,0,1)) goto invalid;
            render.pixels=legacyFb;
            Render_SpanMode0(&render,0,0,(base<<13)+local,0,1);
            if (fb[0]!=legacyFb[0]) ++mismatches;
        }
        printf("RA2 PACK logical=%u actual=%u source=%u successor=%u/%u rawPakAdjacent=%02x mappedGuard=%02x compactLegacy=%02x fullTexelNibbles=4096 pixelFNV=%08x boundaryParity=%s rawPakNeighbor=%s\n",
               src.logicalId,src.actualId,sourceOffset,successorActual,
               successorSource,physicalByte,mappedByte,compactLegacyByte,nativeHash,
               compactLegacyByte==mappedByte?"EXACT":"DIFF",
               physicalByte==mappedByte?"SAME":"DIFF");
        continue;
invalid:
        fprintf(stderr,"FAIL RA2 PACK malformed/incorrect fixture=%d\n",fixtureIndex);
        fclose(f);
        return 1;
    }
    if (fgetc(f)!=EOF) {
        fprintf(stderr,"FAIL RA2 PACK trailing-bytes\n");
        fclose(f);
        return 1;
    }
    fclose(f);
    if (mismatches!=0) {
        fprintf(stderr,"FAIL RA2 PACK compact legacy adjacency != mapped guard: differing nibbles=%d\n",mismatches);
        return 2;
    }
    printf("PASS RA2 PACK native/legacy compact-atlas internal=8192 boundary=4 raw-PAK-adjacency-audited=yes\n");
    return 0;
}

int main(int argc, char** argv) {
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
    if (argc == 3 && strcmp(argv[1],"--fixture")==0)
        return runPackFixture(argv[2]);
    if (argc != 1) {
        fprintf(stderr,"Usage: wall_boundary [--fixture file]\n");
        return 1;
    }
    return 0;
}
