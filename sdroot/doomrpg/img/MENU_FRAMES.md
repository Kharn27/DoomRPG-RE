# UAC main-menu frame assets (isolated)

These are two **source artwork** variants of the same standalone UAC metallic
frame (four transparent openings). The buttons START, LOAD, OPTIONS, HELP
will be isolated separately and layered into those openings later.

- `menu_frame_120.bmp`: **79 × 120** pixels, 32-bit BGRA BITMAPV4HEADER,
  explicit alpha mask, **38,042 bytes**, SHA-256
  `3ed326faf1ab8617b6ddcea788b29f7d7bcf181964b7a8032d0325c981a8aa69`.
- `menu_frame_240.bmp`: **158 × 240** pixels, same format, **151,802 bytes**,
  SHA-256
  `e202155218f2ba917f011a3eebf6f28f9f251f77325d5d8f8d9a2d0f70f87609`.

The original composite reference had a *flattened RGB checkerboard*, which
was removed during alpha extraction. These frames include actual 8-bit
transparency; they are **not** magenta-keyed BMPs.

Important: these are not yet used by the current firmware.
Before loading from SD on the no-PSRAM ESP32, the native UI path needs an
explicitly bounded BMP32+alpha decoder (or a conversion to its preferred
runtime image format). There must be no full-image allocation.
