"""Exact P3h inverse, applied before the P3g inverse for SDL-F28."""


def restore_pre_p3h_plane(value: bytes) -> bytes:
    # P4m: undo only the grouped immutable plane collection first.
    new_collect = "static int collectTextures(PlaneWork* work) {\n    uint32_t i;\n    uint32_t previousWord = 0U;\n    uint8_t seen[32] = {0};\n    const uint8_t* map;\n    if (work == NULL || work->runtime == NULL ||\n        work->runtime->planeMap == NULL ||\n        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;\n\n    map = work->runtime->planeMap;\n    work->textureCount = 0U;\n    /* P4m: the immutable 2048-byte plane map is exactly divisible by four.\n     * Identical consecutive groups can contain only already-admitted IDs.\n     * Read through memcpy (no alignment/aliasing assumptions); scan changed\n     * groups in original byte order, including the exact overflow boundary. */\n    _Static_assert(EXPECTED_PLANE_MAP_BYTES % 4U == 0U, \"plane group size\");\n    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; i += 4U) {\n        uint32_t word;\n        uint32_t lanes = 4U;\n        uint32_t j;\n        memcpy(&word, map + i, sizeof(word));\n        if (i != 0U && word == previousWord) continue;\n        previousWord = word;\n        if (word == (uint32_t)map[i] * 0x01010101U) lanes = 1U;\n        for (j = 0U; j < lanes; ++j) {\n            const uint8_t id = map[i + j];\n            const uint8_t bit = (uint8_t)(1U << (id & 7U));\n            if ((seen[id >> 3] & bit) != 0U) continue;\n            if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;\n            seen[id >> 3] |= bit;\n            memset(&work->textures[work->textureCount], 0,\n                   sizeof(work->textures[work->textureCount]));\n            work->textures[work->textureCount].logicalId = id;\n            ++work->textureCount;\n        }\n    }\n    return work->textureCount > 0U;\n}".encode("utf-8")
    old_collect = "static int collectTextures(PlaneWork* work) {\n    uint32_t i;\n    uint8_t seen[32] = {0};\n    if (work == NULL || work->runtime == NULL ||\n        work->runtime->planeMap == NULL ||\n        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;\n\n    work->textureCount = 0U;\n    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; ++i) {\n        const uint8_t id = work->runtime->planeMap[i];\n        const uint8_t bit = (uint8_t)(1U << (id & 7U));\n        /* P3h: one membership bit per possible logical ID. First encounter\n         * order and exact 24-descriptor failure boundary remain unchanged. */\n        if ((seen[id >> 3] & bit) != 0U) continue;\n        if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;\n        seen[id >> 3] |= bit;\n        memset(&work->textures[work->textureCount], 0,\n               sizeof(work->textures[work->textureCount]));\n        work->textures[work->textureCount].logicalId = id;\n        ++work->textureCount;\n    }\n    return work->textureCount > 0U;\n}".encode("utf-8")
    assert value.count(new_collect) == 1, "P4m collection inverse"
    value = value.replace(new_collect, old_collect, 1)
    edits = (
        (b"""static int collectTextures(PlaneWork* work) {
    uint32_t i;
    uint8_t seen[32] = {0};
    if (work == NULL || work->runtime == NULL ||
        work->runtime->planeMap == NULL ||
        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;

    work->textureCount = 0U;
    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; ++i) {
        const uint8_t id = work->runtime->planeMap[i];
        const uint8_t bit = (uint8_t)(1U << (id & 7U));
        /* P3h: one membership bit per possible logical ID. First encounter
         * order and exact 24-descriptor failure boundary remain unchanged. */
        if ((seen[id >> 3] & bit) != 0U) continue;
        if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;
        seen[id >> 3] |= bit;
        memset(&work->textures[work->textureCount], 0,
               sizeof(work->textures[work->textureCount]));
        work->textures[work->textureCount].logicalId = id;
        ++work->textureCount;
    }
    return work->textureCount > 0U;
}""",
         b"""static int collectTextures(PlaneWork* work) {
    uint32_t i;
    if (work == NULL || work->runtime == NULL ||
        work->runtime->planeMap == NULL ||
        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;

    work->textureCount = 0U;
    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; ++i) {
        const uint8_t id = work->runtime->planeMap[i];
        if (findTextureIndex(work, id) >= 0) continue;
        if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;
        memset(&work->textures[work->textureCount], 0,
               sizeof(work->textures[work->textureCount]));
        work->textures[work->textureCount].logicalId = id;
        ++work->textureCount;
    }
    return work->textureCount > 0U;
}"""),
        (b"""static int32_t planeRowStep(int32_t projected, int width) {
    /* P3h: for the native width and nonnegative projected values,
     * floor(floor(projected * 2^19 / (32 * 160)) / 256)
     * == floor(projected * 2 / 5). Bound multiplication explicitly.
     * Preserve the historical expression for other inputs. */
    if (width == 160 && projected >= 0 && projected <= INT32_MAX / 2)
        return (projected * 2) / 5;
    return (int32_t)((((int64_t)projected << 19) /
                      (32 * width)) >> 8);
}

""",
         b""""""),
        (b"""static int drawPlaneRow(PlaneWork* work,
                        int y,
                        const uint8_t* planeCells) {
    Render_t* render;
    int width;
    int height;
    int32_t viewX;
    int32_t viewY;
    int32_t viewZ;
    int32_t viewSin;
    int32_t viewCos;
    int32_t doubledX;
    int32_t doubledY;
    int32_t zScale;
    int32_t distance;
    int32_t projected;
    uint32_t projectedUnsigned;
    int32_t step;
    int32_t startX;
    int32_t startY;
    int32_t stepX;
    int32_t stepY;

    if (work == NULL || planeCells == NULL || work->render == NULL) return 0;
    render = work->render;
    width = render->screenWidth;
    height = render->screenHeight;
    if (width <= 0 || height <= 0) return 0;

    viewX = (int32_t)((uint32_t)render->viewX << 16);
    viewY = (int32_t)((uint32_t)render->viewY << 16);
    viewZ = render->viewZ;
    viewCos = render->viewCos;
    viewSin = render->viewSin;
    doubledX = render->screenLeft << 1;
    doubledY = y << 1;

    if (height > doubledY) viewZ = 64 - viewZ;
    zScale = viewZ << 3;
    distance = height > doubledY ? height - doubledY : doubledY - height;
    projected = (width * zScale) / (distance + 1);
    projectedUnsigned = (uint32_t)projected;
    step = planeRowStep(projected, width);

    startX = (int32_t)(
        (int64_t)viewX + (((int64_t)viewCos * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewSin >> 8) *
          (doubledX - width + 1)) >> 1));
    startY = (int32_t)(
        (int64_t)viewY + (((int64_t)(-viewSin) * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewCos >> 8) *
          (doubledX - width + 1)) >> 1));
    stepX = (int32_t)((int64_t)step * (viewSin >> 8));
    stepY = (int32_t)((int64_t)step * (viewCos >> 8));

    return spanPlane(work,
                     doubledX >> 1,
                     doubledY >> 1,
                     planeCells,
                     startX, startY, stepX, stepY,
                     render->screenRight);
}""",
         b"""static int drawPlaneRow(PlaneWork* work,
                        int y,
                        const uint8_t* planeCells) {
    Render_t* render;
    int width;
    int height;
    int32_t viewX;
    int32_t viewY;
    int32_t viewZ;
    int32_t viewSin;
    int32_t viewCos;
    int32_t doubledX;
    int32_t doubledY;
    int32_t zScale;
    int32_t distance;
    int32_t projected;
    uint32_t projectedUnsigned;
    int32_t step;
    int32_t startX;
    int32_t startY;
    int32_t stepX;
    int32_t stepY;

    if (work == NULL || planeCells == NULL || work->render == NULL) return 0;
    render = work->render;
    width = render->screenWidth;
    height = render->screenHeight;
    if (width <= 0 || height <= 0) return 0;

    viewX = (int32_t)((uint32_t)render->viewX << 16);
    viewY = (int32_t)((uint32_t)render->viewY << 16);
    viewZ = render->viewZ;
    viewCos = render->viewCos;
    viewSin = render->viewSin;
    doubledX = render->screenLeft << 1;
    doubledY = y << 1;

    if (height > doubledY) viewZ = 64 - viewZ;
    zScale = viewZ << 3;
    distance = height > doubledY ? height - doubledY : doubledY - height;
    projected = (width * zScale) / (distance + 1);
    projectedUnsigned = (uint32_t)projected;
    step = (int32_t)((((int64_t)projected << 19) /
                      (32 * width)) >> 8);

    startX = (int32_t)(
        (int64_t)viewX + (((int64_t)viewCos * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewSin >> 8) *
          (doubledX - width + 1)) >> 1));
    startY = (int32_t)(
        (int64_t)viewY + (((int64_t)(-viewSin) * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewCos >> 8) *
          (doubledX - width + 1)) >> 1));
    stepX = (int32_t)((int64_t)step * (viewSin >> 8));
    stepY = (int32_t)((int64_t)step * (viewCos >> 8));

    return spanPlane(work,
                     doubledX >> 1,
                     doubledY >> 1,
                     planeCells,
                     startX, startY, stepX, stepY,
                     render->screenRight);
}"""),
    )
    for new, old in edits:
        assert value.count(new) == 1, "P3h exact historical inverse anchor"
        value = value.replace(new, old, 1)
    return value
