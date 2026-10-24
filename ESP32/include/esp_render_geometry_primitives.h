#ifndef ESP_RENDER_GEOMETRY_PRIMITIVES_H
#define ESP_RENDER_GEOMETRY_PRIMITIVES_H

/*
 * Bounded native frame-scratch occlusion. x1/x2 are projected 16.16
 * screen coordinates, as produced by the unchanged legacy-equivalent
 * projection primitives. The caller owns columnScale and guarantees that
 * both rounded endpoints are within its allocated viewport width.
 *
 * The operation is allocation-free, touches only the admitted columns,
 * and intentionally preserves the desktop half-open ceil(x1)..ceil(x2)
 * arithmetic, including its empty/reversed span behavior.
 * No legacy render graph, BSP line object, map state or framebuffer is involved.
 */
void EspNativeRenderGeometry_occludeProjectedSpan(
    int* columnScale, int x1, int x2);

#endif
