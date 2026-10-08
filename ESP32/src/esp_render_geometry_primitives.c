#include <stdint.h>

#include <SDL.h>

#include "DoomRPG.h"
#include "Render.h"

/* src/Render.c fixes this mode locally; mirror its exact arithmetic. */
#define FIXED_VERSION 1
#if FIXED_VERSION != 1
#error "ESP32 geometry must match desktop Render.c fixed-point mode"
/* Framebuffer-only floor/ceiling clear, byte-for-byte legacy semantics. */
void Render_renderFloorAndCeilingSolidBG(Render_t* render)
{
	int i, h, pitch;

	pitch = ((render->pitch * render->screenY) + (render->screenX * sizeof(short)));

	h = render->screenHeight >> 1;
	for (i = 0; i < h; i++) {
		SDL_memmove(&render->framebuffer[pitch + (render->pitch * i)], render->ceilingColor, (render->screenWidth * sizeof(short)));
	}

	for (i = h; i < render->screenHeight; i++) {
		SDL_memmove(&render->framebuffer[pitch + (render->pitch * i)], render->floorColor, (render->screenWidth * sizeof(short)));
	}
}

/* Legacy-equivalent bounded RGB565 fade; framebuffer scratch only. */
void Render_fadeScreen(Render_t* render, int fade)
{
	int pitch, i, j;
	short color;
	int r, g, b;
	short* pixels;

	pixels = (short*)&render->framebuffer[render->pitch * render->screenY + render->screenX * sizeof(short)];
	pitch = render->pitch >> 1;

	for (i = 0; i < render->screenHeight; i++) {
		for (j = 0; j < render->screenWidth; j++) {
			color = pixels[i * pitch + j];

			b = (color & 0x1F);
			g = (color >> 5) & 0x3f;
			r = (color >> 11) & 0x1f;

			if (r > (fade >> 3)) {
				r = (fade >> 3);
			}

			if (g > (fade >> 2)) {
				g = (fade >> 2);
			}

			if (b > (fade >> 3)) {
				b = (fade >> 3);
			}

			color = (r << 11) | (g << 5) | b;

			pixels[i * pitch + j] = color;
		}
	}
}

#endif

/*
 * Permanent production owner for the legacy-named projection/culling
 * primitives still consumed by the ESP32-native world and sprite renderers.
 *
 * These functions intentionally preserve the desktop arithmetic/ABI exactly.
 * They mutate only Render_t frame scratch. They do not own map topology,
 * texels, shapes, entities or gameplay state.
 */
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_BRINGUP_PROBES)

void Render_initColumnScale(Render_t* render)
{
    for (int i = render->screenWidth; --i >= 0;
         render->columnScale[i] = COLUMN_SCALE_INIT);
}

boolean Render_cullBoundingBox(Render_t* render, Node_t* node)
{
    Line_t* line;
    int i;

    if (render->skipCull) {
        return false;
    }

    if ((render->viewX >= (node->x1 - 5)) &&
        (render->viewX <= (node->x2 + 5)) &&
        (render->viewY >= (node->y1 - 5)) &&
        (render->viewY <= (node->y2 + 5))) {
        return false;
    }

    line = &render->tmpLine;
    if (render->viewX < node->x1)
    {
        if (render->viewY < node->y1)
        {
            line->vert1.x = node->x2;
            line->vert1.y = node->y1;
            line->vert2.x = node->x1;
            line->vert2.y = node->y2;
        }
        else if (render->viewY < node->y2)
        {
            line->vert1.x = node->x1;
            line->vert1.y = node->y1;
            line->vert2.x = node->x1;
            line->vert2.y = node->y2;
        }
        else
        {
            line->vert1.x = node->x1;
            line->vert1.y = node->y1;
            line->vert2.x = node->x2;
            line->vert2.y = node->y2;
        }
    }
    else if (render->viewX < node->x2)
    {
        if (render->viewY < node->y1)
        {
            line->vert1.x = node->x2;
            line->vert1.y = node->y1;
            line->vert2.x = node->x1;
            line->vert2.y = node->y1;
        }
        else if (render->viewY < node->y2)
        {
            return false;
        }
        else
        {
            line->vert1.x = node->x1;
            line->vert1.y = node->y2;
            line->vert2.x = node->x2;
            line->vert2.y = node->y2;
        }
    }
    else if (render->viewY < node->y1)
    {
        line->vert1.x = node->x2;
        line->vert1.y = node->y2;
        line->vert2.x = node->x1;
        line->vert2.y = node->y1;
    }
    else if (render->viewY < node->y2)
    {
        line->vert1.x = node->x2;
        line->vert1.y = node->y2;
        line->vert2.x = node->x2;
        line->vert2.y = node->y1;
    }
    else
    {
        line->vert1.x = node->x1;
        line->vert1.y = node->y2;
        line->vert2.x = node->x2;
        line->vert2.y = node->y1;
    }

    Render_transform2DVerts(render, &line->vert1);
    Render_transform2DVerts(render, &line->vert2);

    if (Render_clipLine(render, line)) {
        Render_projectVertex(render, &line->vert1);
        Render_projectVertex(render, &line->vert2);

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

void Render_transform2DVerts(Render_t* render, Vertex_t* vert)
{
    int x, y;
    x = (vert->x * render->viewCos_) +
        (vert->y * render->viewSin_) + render->viewTransX;
    y = (vert->x * render->viewSin) +
        (vert->y * render->viewCos) + render->viewTransY;

    vert->x = x;
    vert->y = y;
}

boolean Render_clipLine(Render_t* render, Line_t* line)
{
    int i, i2, i3, i4, i5, i6;

    i = line->vert1.x + line->vert1.y;
    i2 = line->vert2.x + line->vert2.y;
    if (i < 0)
    {
        if (i2 < 0) {
            return false;
        }
        Render_clipVertex(render, &line->vert1, line, i, i2);
    }
    else if (i2 < 0)
    {
        Render_clipVertex(render, &line->vert2, line, -i, -i2);
    }

    i3 = line->vert1.x - line->vert1.y;
    i4 = line->vert2.x - line->vert2.y;
    if (i4 < 0)
    {
        if (i3 < 0) {
            return false;
        }
        Render_clipVertex(render, &line->vert2, line, i3, i4);
    }
    else if (i3 < 0)
    {
        Render_clipVertex(render, &line->vert1, line, -i3, -i4);
    }

    i5 = line->vert1.x - 0x40000;
    i6 = line->vert2.x - 0x40000;
    if (i6 < 0)
    {
        if (i5 < 0) {
            return false;
        }
        Render_clipVertex(render, &line->vert2, line, i5, i6);
    }
    else if (i5 < 0)
    {
        Render_clipVertex(render, &line->vert1, line, -i5, -i6);
    }

    return true;
}

void Render_clipVertex(Render_t* render, Vertex_t* vert, Line_t* line, int i, int i2)
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

void Render_projectVertex(Render_t* render, Vertex_t* vert)
{
#if FIXED_VERSION == 1
    int y;
    y = vert->y;

    vert->y = DoomRPG_FixedDiv(render->screenWidth << 16, vert->x);
    vert->x = (DoomRPG_FixedMul(y, vert->y) >> 1);
    vert->x += render->fracHalfScreenWidth;
    vert->z *= vert->y;
#else
    long long y;
    y = vert->y;

    vert->y = (int)((((int64_t)render->screenWidth) * 4294967296L) /
                    ((int64_t)vert->x));
    vert->x = (int)((((int64_t)y) * ((int64_t)vert->y)) >> 17);
    vert->x += render->fracHalfScreenWidth;
    vert->z *= vert->y;
#endif
}

void Render_occludeClippedLine(Render_t* render, Line_t* line)
{
    int x1, x2, i;

    x2 = line->vert2.x;
    x1 = line->vert1.x;
    if ((x2 - x1) > 0) {
        x1 = (x1 + 0xFFFF) >> FRACBITS;
        x2 = (x2 + 0xFFFF) >> FRACBITS;

        for (i = x1; i < x2; ++i) {
            render->columnScale[i] = COLUMN_SCALE_OCCLUDED;
        }
    }
}

#endif
