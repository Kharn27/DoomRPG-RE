#include <stdint.h>

#include <SDL.h>

#include "DoomRPG.h"
#include "Render.h"
#include "esp_render_geometry_primitives.h"

/* src/Render.c fixes this mode locally; mirror its exact arithmetic. */
#define FIXED_VERSION 1
#if FIXED_VERSION != 1
#error "ESP32 geometry must match desktop Render.c fixed-point mode"


#endif

/*
 * Permanent CYD owner for the legacy-named projection/culling
 * primitives still consumed by the ESP32-native world and sprite renderers.
 *
 * These functions intentionally preserve the desktop arithmetic/ABI exactly.
 * They mutate only Render_t frame scratch. They do not own map topology,
 * texels, shapes, entities or gameplay state.
 */
#if defined(DOOMRPG_ESP32)

void EspNativeRenderGeometry_resetColumns(int* columnScale, int screenWidth)
{
    for (int i = screenWidth; --i >= 0;
         columnScale[i] = COLUMN_SCALE_INIT);
}

/* Native BSP node-bounds culling. Only scalar extents cross the API:
 * no Node_t alias/copy or desktop Render_cullBoundingBox symbol. This retains
 * the exact fixed-point transform/clip/projection and column occlusion test
 * from the desktop oracle. The caller explicitly preserves render->tmpLine
 * around this scratch-only operation, as the old --wrap contract required. */
int EspNativeRenderGeometry_cullBounds(struct Render_s* renderBase,
                                      int x1, int y1, int x2, int y2,
                                      EspNativeRenderGeometryClipFn clipLine)
{
    Render_t* render = (Render_t*)renderBase;
    Line_t* line;
    int i;

    if (render->skipCull) {
        return false;
    }

    if ((render->viewX >= (x1 - 5)) &&
        (render->viewX <= (x2 + 5)) &&
        (render->viewY >= (y1 - 5)) &&
        (render->viewY <= (y2 + 5))) {
        return false;
    }

    line = &render->tmpLine;
    if (render->viewX < x1)
    {
        if (render->viewY < y1)
        {
            line->vert1.x = x2;
            line->vert1.y = y1;
            line->vert2.x = x1;
            line->vert2.y = y2;
        }
        else if (render->viewY < y2)
        {
            line->vert1.x = x1;
            line->vert1.y = y1;
            line->vert2.x = x1;
            line->vert2.y = y2;
        }
        else
        {
            line->vert1.x = x1;
            line->vert1.y = y1;
            line->vert2.x = x2;
            line->vert2.y = y2;
        }
    }
    else if (render->viewX < x2)
    {
        if (render->viewY < y1)
        {
            line->vert1.x = x2;
            line->vert1.y = y1;
            line->vert2.x = x1;
            line->vert2.y = y1;
        }
        else if (render->viewY < y2)
        {
            return false;
        }
        else
        {
            line->vert1.x = x1;
            line->vert1.y = y2;
            line->vert2.x = x2;
            line->vert2.y = y2;
        }
    }
    else if (render->viewY < y1)
    {
        line->vert1.x = x2;
        line->vert1.y = y2;
        line->vert2.x = x1;
        line->vert2.y = y1;
    }
    else if (render->viewY < y2)
    {
        line->vert1.x = x2;
        line->vert1.y = y2;
        line->vert2.x = x2;
        line->vert2.y = y1;
    }
    else
    {
        line->vert1.x = x1;
        line->vert1.y = y2;
        line->vert2.x = x2;
        line->vert2.y = y1;
    }

    EspNativeRenderGeometry_transformPoint(
        render->viewCos_, render->viewSin_, render->viewSin,
        render->viewCos, render->viewTransX, render->viewTransY,
        &line->vert1.x, &line->vert1.y);
    EspNativeRenderGeometry_transformPoint(
        render->viewCos_, render->viewSin_, render->viewSin,
        render->viewCos, render->viewTransX, render->viewTransY,
        &line->vert2.x, &line->vert2.y);

    if (clipLine != NULL && clipLine(line)) {
        EspNativeRenderGeometry_projectVertex(
            render->screenWidth, render->fracHalfScreenWidth,
            &line->vert1.x, &line->vert1.y, &line->vert1.z);
        EspNativeRenderGeometry_projectVertex(
            render->screenWidth, render->fracHalfScreenWidth,
            &line->vert2.x, &line->vert2.y, &line->vert2.z);

        {
            int x1 = (line->vert1.x + 0xFFFF) >> FRACBITS;
            int x2 = (line->vert2.x + 0xFFFF) >> FRACBITS;

            for (i = x1; i < x2; ++i) {
                if (render->columnScale[i] == COLUMN_SCALE_INIT) {
                    return false;
                }
            }
        }
    }

    return true;
}

void EspNativeRenderGeometry_transformPoint(
    int viewCosNeg, int viewSinNeg, int viewSin, int viewCos,
    int viewTransX, int viewTransY, int* x, int* y)
{
    const int sourceX = *x;
    const int sourceY = *y;
    int transformedX;
    int transformedY;

    transformedX = (sourceX * viewCosNeg) +
                   (sourceY * viewSinNeg) + viewTransX;
    transformedY = (sourceX * viewSin) +
                   (sourceY * viewCos) + viewTransY;

    *x = transformedX;
    *y = transformedY;
}

/* The interpolation is an internal clipping detail; no Render_t or public
 * desktop Render_clipVertex ABI is needed. The input segment endpoints are
 * sampled before overwriting the selected endpoint, preserving legacy alias
 * semantics and exact fixed-point arithmetic. */
static void clipVertex(Vertex_t* vert, Line_t* line, int i, int i2);

int EspNativeRenderGeometry_clipLine(struct Line_s* lineBase)
{
    Line_t* line = (Line_t*)lineBase;
    int i, i2, i3, i4, i5, i6;

    i = line->vert1.x + line->vert1.y;
    i2 = line->vert2.x + line->vert2.y;
    if (i < 0)
    {
        if (i2 < 0) {
            return false;
        }
        clipVertex(&line->vert1, line, i, i2);
    }
    else if (i2 < 0)
    {
        clipVertex(&line->vert2, line, -i, -i2);
    }

    i3 = line->vert1.x - line->vert1.y;
    i4 = line->vert2.x - line->vert2.y;
    if (i4 < 0)
    {
        if (i3 < 0) {
            return false;
        }
        clipVertex(&line->vert2, line, i3, i4);
    }
    else if (i3 < 0)
    {
        clipVertex(&line->vert1, line, -i3, -i4);
    }

    i5 = line->vert1.x - 0x40000;
    i6 = line->vert2.x - 0x40000;
    if (i6 < 0)
    {
        if (i5 < 0) {
            return false;
        }
        clipVertex(&line->vert2, line, i5, i6);
    }
    else if (i5 < 0)
    {
        clipVertex(&line->vert1, line, -i5, -i6);
    }

    return true;
}

static void clipVertex(Vertex_t* vert, Line_t* line, int i, int i2)
{
#if FIXED_VERSION == 1
    fixed_t j, j2;
    int x, y, z;

    j = DoomRPG_FixedDiv((-i), (i2 - i));
    j2 = 0x10000 - j;

    x = (int)(DoomRPG_FixedMul(line->vert1.x, j2) +
              DoomRPG_FixedMul(line->vert2.x, j));
    y = (int)(DoomRPG_FixedMul(line->vert1.y, j2) +
              DoomRPG_FixedMul(line->vert2.y, j));
    z = (int)(DoomRPG_FixedMul(line->vert1.z, j2) +
              DoomRPG_FixedMul(line->vert2.z, j));

    vert->x = x;
    vert->y = y;
    vert->z = z;
#else
    long long j, j2;

    j = (((long long)(-i)) << 16) / ((long long)(i2 - i));
    j2 = 0x10000 - j;

    vert->x = (int)((((((int64_t)line->vert1.x) * j2) +
                       (((int64_t)line->vert2.x) * j)) + (0x8000)) >> 16);
    vert->y = (int)((((((int64_t)line->vert1.y) * j2) +
                       (((int64_t)line->vert2.y) * j)) + (0x8000)) >> 16);
    vert->z = (int)((((((int64_t)line->vert1.z) * j2) +
                       (((int64_t)line->vert2.z) * j)) + (0x8000)) >> 16);
#endif
}

void EspNativeRenderGeometry_projectVertex(
    int screenWidth, int fracHalfScreenWidth, int* x, int* y, int* z)
{
    int sourceY = *y;
    int depthScale = DoomRPG_FixedDiv(screenWidth << FRACBITS, *x);

    *y = depthScale;
    *x = (DoomRPG_FixedMul(sourceY, depthScale) >> 1);
    *x += fracHalfScreenWidth;
    *z *= depthScale;
}

void EspNativeRenderGeometry_occludeProjectedSpan(int* columnScale, int x1, int x2)
{
    int i;

    if ((x2 - x1) > 0) {
        x1 = (x1 + 0xFFFF) >> FRACBITS;
        x2 = (x2 + 0xFFFF) >> FRACBITS;

        for (i = x1; i < x2; ++i) {
            columnScale[i] = COLUMN_SCALE_OCCLUDED;
        }
    }
}

#endif
