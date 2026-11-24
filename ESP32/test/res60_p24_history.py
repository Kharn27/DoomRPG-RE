"""RES60-P24 immutable inverse for SDL-F24's historical first-frame SHA.

Only the six explicitly reviewed scalar viewport changes are reversed.
The complete original Git blob is then still checked by SDL-F24.
"""

def restore_pre_res60_p24(source: bytes) -> bytes:
    pairs = [
        (
            b"static void fillBackground(Render_t* render,\n"
            b"                           uint16_t ceiling,\n"
            b"                           uint16_t floor,\n"
            b"                           int clearWholeFramebuffer) {",
            b"static void fillBackground(Render_t* render,\n"
            b"                           uint16_t ceiling,\n"
            b"                           uint16_t floor,\n"
            b"                           const EspNativeRasterWorldViewport* viewport,\n"
            b"                           int clearWholeFramebuffer) {",
        ),
        (
            b"        uint16_t* row = fb + (DOOMRPG_VIEWPORT_Y + y) * pitchPixels +\n"
            b"                        DOOMRPG_VIEWPORT_X;",
            b"        uint16_t* row = fb + viewport->worldFirstPixel + y * pitchPixels;",
        ),
        (
            b"    RenderScratch scratch;\n",
            b"    RenderScratch scratch;\n    EspNativeRasterWorldViewport worldViewport;\n",
        ),
        (
            b"    lineState = EspMapLineState_view();",
            b"    /* P24: admit the real permanent renderer's world offsets before any\n"
            b"     * heap or PAK lease. RES60 prepares the same geometry in the host oracle;\n"
            b"     * this runtime path remains explicitly CLASSIC until the planes, sprites,\n"
            b"     * HUD and gameplay coordinator have their own complete milestones. */\n"
            b"    if (!EspNativeRasterContract_prepareWorld(\n"
            b"            ESP_NATIVE_RASTER_CLASSIC,\n"
            b"            Esp32PlatformVideo_framebufferSizeBytes(), render->pitch,\n"
            b"            render->screenWidth, render->screenHeight,\n"
            b"            MAX_SCREEN_WIDTH, &worldViewport)) return 0;\n\n"
            b"    lineState = EspMapLineState_view();",
        ),
        (
            b"    fillBackground(render, outState->ceilingRgb565, outState->floorRgb565,\n"
            b"                   clearWholeFramebuffer);",
            b"    fillBackground(render, outState->ceilingRgb565, outState->floorRgb565,\n"
            b"                   &worldViewport, clearWholeFramebuffer);",
        ),
        (
            b"    render->pixels = (short*)&render->framebuffer[\n"
            b"        render->pitch * DOOMRPG_VIEWPORT_Y +\n"
            b"        DOOMRPG_VIEWPORT_X * (int)sizeof(short)];",
            b"    render->pixels = (short*)&render->framebuffer[\n"
            b"        worldViewport.worldFirstPixel * sizeof(uint16_t)];",
        ),
    ]
    for previous, reviewed in pairs:
        assert source.count(reviewed) == 1, (
            "P24 exact renderer inverse drifted")
        source = source.replace(reviewed, previous, 1)
    return source
