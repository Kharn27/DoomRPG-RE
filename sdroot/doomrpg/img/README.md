# Optional boot credits image (classic CYD)

To replace the startup legal/credits sequence, put **credits.bmp** on your SD
card at exactly:

`/doomrpg/img/credits.bmp`

It must be an **uncompressed 24-bit RGB BMP, 128 × 640** (five vertically
stacked 128 × 128 panels, top to bottom). Every panel shows for 3 seconds or
until the next touch, just like the original startup screens.

The repository also keeps **credits.png** (128 × 640) as the editable/source
image. The classic CYD has no PSRAM, so the firmware deliberately reads bounded
BMP ranges instead of decoding a large PNG. Convert on your computer:

```shell
python -m pip install pillow
python ESP32/tools/credits_png_to_bmp.py
```

Copy the generated `sdroot/doomrpg/img/credits.bmp` to the SD card. The
`sdroot/` prefix models the SD card's root; **do not** create a literal
`sdroot` directory on the card.

Without `credits.bmp`, with an invalid BMP header/size, or if a SD read
fails, the firmware automatically uses the original four `g.bmp` panels from
`DoomRPG-ESP32.pak` plus the built-in Kharn27 panel. The game never needs the
override to boot. Neither the PAK nor its legacy image is modified.

The pre-title display is **not** a gameplay texture, and no PNG decoder has
been added to the firmware.
