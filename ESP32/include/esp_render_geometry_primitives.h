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

/*
 * Desktop-equivalent fixed-point perspective projection using scalar frame
 * parameters and caller-owned coordinates. Input x is a positive, nonzero,
 * forward-facing camera depth in 16.16; y is camera-relative 16.16; z is
 * the inherited vertical extent. Output x is projected 16.16 screen x,
 * y is depth scale and z is scaled vertical extent. Three pointers must
 * refer to distinct scalar fields. No allocations or engine object inputs.
 */
void EspNativeRenderGeometry_projectVertex(
    int screenWidth, int fracHalfScreenWidth, int* x, int* y, int* z);

#endif
