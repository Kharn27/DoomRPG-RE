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

/*
 * Desktop-equivalent view-space 2D transform for one caller-owned (x,y).
 * Coefficients are the six per-frame Render viewCos_, viewSin_, viewSin,
 * viewCos, viewTransX and viewTransY scalars, in that order. Source point
 * coordinates are read before either output is stored; x/y must be distinct.
 * The inherited integer multiplication, addition order and wrapping semantics
 * are unchanged. z is deliberately untouched. No renderer object, vertex
 * object or allocation crosses this boundary.
 */
void EspNativeRenderGeometry_transformPoint(
    int viewCosNeg, int viewSinNeg, int viewSin, int viewCos,
    int viewTransX, int viewTransY, int* x, int* y);

/*
 * Initialize an explicitly owned viewport column scratch array to the
 * inherited COLUMN_SCALE_INIT sentinel. screenWidth is the number of valid
 * int elements supplied by the caller. Only indices [0, screenWidth) are
 * touched; width zero is a no-op. No Render object, allocation or map state.
 */
void EspNativeRenderGeometry_resetColumns(int* columnScale, int screenWidth);

#endif
