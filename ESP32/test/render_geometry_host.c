/* Differential geometry versus the seven desktop reference bodies extracted
 * by render_geometry_host.py. No live game RNG, raster, SD or CYD is mocked
 * into existence: these pure frame-scratch operations have no such owners. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "Render.h"

void Reference_Render_initColumnScale(Render_t*);
boolean Reference_Render_cullBoundingBox(Render_t*, Node_t*);
void Reference_Render_transform2DVerts(Render_t*, Vertex_t*);
boolean Reference_Render_clipLine(Render_t*, Line_t*);
void Reference_Render_clipVertex(Render_t*, Vertex_t*, Line_t*, int, int);
void Reference_Render_projectVertex(Render_t*, Vertex_t*);
void Reference_Render_occludeClippedLine(Render_t*, Line_t*);

/* Same inherited fixed arithmetic, using multiplication rather than a signed
 * negative left shift so UBSan can check the geometry itself. */
fixed_t DoomRPG_FixedMul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * b) >> FRACBITS);
}
fixed_t DoomRPG_FixedDiv(fixed_t a, fixed_t b) {
    int64_t aa = a < 0 ? -(int64_t)a : a;
    int64_t bb = b < 0 ? -(int64_t)b : b;
    return (aa >> 14) >= bb ? ((a ^ b) >> 31) ^ INT_MAX :
        (fixed_t)(((int64_t)a * FRACUNIT) / b);
}

static uint32_t sample = 0x12345678U;
static int pick(int maximum) {
    sample ^= sample << 13; sample ^= sample >> 17; sample ^= sample << 5;
    return (int)(sample % (uint32_t)maximum);
}
static Render_t native, reference;
static int columns[160 + 2], refColumns[160 + 2];

static void seed(unsigned angle) {
    const int sine[4] = {0, 65536, 0, -65536};
    const int cosine[4] = {65536, 0, -65536, 0};
    memset(&native, 0, sizeof(native));
    native.screenWidth = 160; native.screenHeight = 80;
    native.screenY = 20; native.pitch = 320;
    native.fracHalfScreenWidth = 80 * FRACUNIT;
    native.viewX = pick(2048); native.viewY = pick(2048);
    native.viewCos_ = cosine[angle]; native.viewSin_ = sine[angle];
    native.viewCos = -sine[angle]; native.viewSin = cosine[angle];
    native.viewTransX = -native.viewX * native.viewCos_ - native.viewY * native.viewSin_;
    native.viewTransY = -native.viewX * native.viewSin - native.viewY * native.viewCos;
    reference = native;
    native.columnScale = columns + 1; reference.columnScale = refColumns + 1;
    for (unsigned i = 0; i < 162; ++i) columns[i] = refColumns[i] = 0xbeef;
}
static void sameScratch(void) {
    assert(memcmp(&native.tmpLine, &reference.tmpLine, sizeof(native.tmpLine)) == 0);
    assert(memcmp(columns, refColumns, sizeof(columns)) == 0);
    assert(columns[0] == 0xbeef && columns[161] == 0xbeef);
    assert(native.shapeData == NULL && native.mediaTexels == NULL);
    assert(reference.shapeData == NULL && reference.mediaTexels == NULL);
}

int main(void) {
    for (unsigned i = 0; i < 10000; ++i) {
        Line_t line, other;
        Vertex_t vertex, refVertex;
        Node_t node;
        seed(i % 4);
        Render_initColumnScale(&native);
        Reference_Render_initColumnScale(&reference);
        sameScratch();
        memset(&line, 0, sizeof(line));
        line.vert1.x = (pick(37) - 4) * FRACUNIT;
        line.vert1.y = (pick(65) - 32) * FRACUNIT;
        line.vert1.z = pick(13) * FRACUNIT;
        line.vert2.x = (pick(37) - 4) * FRACUNIT;
        line.vert2.y = (pick(65) - 32) * FRACUNIT;
        line.vert2.z = pick(13) * FRACUNIT;
        other = line;
        assert(Render_clipLine(&native, &line) == Reference_Render_clipLine(&reference, &other));
        assert(memcmp(&line, &other, sizeof(line)) == 0);
        /* Exercise project/occlude only on admitted, forward-facing segments. */
        if (line.vert1.x >= 4 * FRACUNIT && line.vert2.x >= 4 * FRACUNIT &&
            line.vert1.y >= -line.vert1.x && line.vert1.y <= line.vert1.x &&
            line.vert2.y >= -line.vert2.x && line.vert2.y <= line.vert2.x) {
            Render_projectVertex(&native, &line.vert1);
            Render_projectVertex(&native, &line.vert2);
            Reference_Render_projectVertex(&reference, &other.vert1);
            Reference_Render_projectVertex(&reference, &other.vert2);
            assert(memcmp(&line, &other, sizeof(line)) == 0);
            Render_occludeClippedLine(&native, &line);
            Reference_Render_occludeClippedLine(&reference, &other);
            sameScratch();
        }
        memset(&vertex, 0, sizeof(vertex));
        vertex.x = pick(2048); vertex.y = pick(2048); vertex.z = pick(64);
        refVertex = vertex;
        Render_transform2DVerts(&native, &vertex);
        Reference_Render_transform2DVerts(&reference, &refVertex);
        assert(memcmp(&vertex, &refVertex, sizeof(vertex)) == 0);
        memset(&node, 0, sizeof(node));
        node.x1 = pick(1800); node.y1 = pick(1800);
        node.x2 = node.x1 + pick(248); node.y2 = node.y1 + pick(248);
        native.skipCull = reference.skipCull = (i % 13 == 0);
        assert(Render_cullBoundingBox(&native, &node) ==
               Reference_Render_cullBoundingBox(&reference, &node));
        sameScratch();
    }
    puts("render_geometry_host: PASS 10000 seeded geometry cases, four cardinals and guarded columns");
    return 0;
}
