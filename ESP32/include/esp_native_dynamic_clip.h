#ifndef ESP_NATIVE_DYNAMIC_CLIP_H
#define ESP_NATIVE_DYNAMIC_CLIP_H

/* Explicit gameplay-world clip gate replacing the hidden link wrapper.
 * Adapts scoped animated-door texture z before pure native clipping.
 * Shared by walls, BSP bounds and sprites, without map/RNG mutation. */
struct Line_s;
int EspNativeDynamicLineRender_clipLine(struct Line_s* line);

#endif
