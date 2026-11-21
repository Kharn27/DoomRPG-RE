"""Strip the AUDIO-R2a main hook for historical root/main SHA parity.

This function is deliberately fail-closed when the exact instrumented
main.cpp changes; it does NOT remove native gameplay or any other features.
"""

from native_boot_legals_history import restore_pre_boot_legals


def restore_pre_res60_p1(main: str) -> str:
    """Remove only P1's isolated video-only boot seam for historical blob parity."""
    p6_include = ('#if DOOMRPG_RES60_P6_MENU\n'
                  '#include "esp_res60_menu.h"\n'
                  '#include "platform_touch_events.h"\n'
                  '#endif\n')
    assert main.count(p6_include) == 1, "P6 include scope changed"
    main = main.replace(p6_include, "", 1)
    p7_include = ('#if DOOMRPG_RES60_P7_NAV\n'
                  '#include "esp_res60_nav.h"\n'
                  '#endif\n')
    assert main.count(p7_include) == 1, "P7 include scope changed"
    main = main.replace(p7_include, "", 1)
    p8_include = ('#if DOOMRPG_RES60_P8_TEXEL\n'
                  '#include "esp_res60_texture_probe.h"\n'
                  '#endif\n')
    assert main.count(p8_include) == 1, "P8 cache include scope changed"
    main = main.replace(p8_include, "", 1)
    for added in ("#include <esp_heap_caps.h>\n",
                  '#include "platform_video_config.h"\n'):
        # Use exactly one removal; do not alter any historical main line.
        assert main.count(added) == 1, added
        main = main.replace(added, "", 1)
    begin = "    initializePlatformVideo();\n#if DOOMRPG_RES60_P1_DIAGNOSTIC\n"
    end = "#endif\n    initializeSdCard();"
    assert main.count(begin) == 1 and main.count(end) == 1
    left = main.index(begin) + len("    initializePlatformVideo();\n")
    right = main.index(end, left) + len("#endif\n")
    removed = main[left:right]
    assert 'PlatformVideo_showTestPattern();' in removed
    assert 'return;' in removed and '55296U' in removed
    main = main[:left] + main[right:]
    # P5 introduces two gated scalar input/title states. Reverse them
    # precisely before checking the frozen historical pre-RES60 blob.
    p5_state = ("#if DOOMRPG_RES60_P5_TITLE\n"
                "    bool res60TitleVisible = false;\n"
                "    bool res60TouchWasDown = false;\n"
                "#endif\n")
    assert main.count(p5_state) == 1, "P5 global input/title owner drift"
    main = main.replace(p5_state, "", 1)
    p7_state = ("#if DOOMRPG_RES60_P7_NAV\n"
                "    uint8_t res60QuarterTurns = 0U;\n"
                "#endif\n")
    assert main.count(p7_state) == 1, "P7 rotation owner drift"
    main = main.replace(p7_state, "", 1)
    p6_start = "#if DOOMRPG_RES60_P6_MENU\n    bool res60MenuVisible = false;\n"
    p6_end = "\n#endif\n"
    assert main.count(p6_start) == 1, "P6 input owner drift"
    p6_at = main.index(p6_start)
    p6_end_at = main.index(p6_end, p6_at) + len(p6_end)
    p6_state = main[p6_at:p6_end_at]
    assert "res60TapPending" in p6_state and "res60MenuTapCallback" in p6_state
    main = main[:p6_at] + main[p6_end_at:]
    loop_start = "void loop()\n{\n#if DOOMRPG_RES60_P1_DIAGNOSTIC\n"
    loop_end = "    delay(250);\n#endif\n    return;\n#else\n"
    assert main.count(loop_start) == 1 and main.count(loop_end) == 1
    start = main.index(loop_start)
    end = main.index(loop_end, start) + len(loop_end)
    p5_loop = main[start:end]
    # Strict evidence that ONLY the P5 pilot toggle is removed: never strip
    # a gameplay service or mutable world operation from a future version.
    assert p5_loop.count("#if DOOMRPG_RES60_P5_TITLE") == 1
    assert p5_loop.count("#if DOOMRPG_RES60_P6_MENU") == 1
    assert "EspRes60Menu_onTap" in p5_loop
    assert p5_loop.count("res60TouchWasDown = down;") == 1
    assert p5_loop.count("PlatformVideo_showRes60WorldPreview();") == 2
    assert p5_loop.count("PlatformVideo_showRes60TitlePreview();") == 1
    assert "EspNativeGameplaySessionCoordinator_service" not in p5_loop
    main = main[:start] + "void loop()\n{\n" + main[end:]
    assert main.endswith("    delay(5);\n#endif\n}\n")
    main = main[:-len("    delay(5);\n#endif\n}\n")] + "    delay(5);\n}\n"
    return main


def restore_pre_audio_r1(text: str) -> str:
    if "#if DOOMRPG_RES60_P1_DIAGNOSTIC" in text:
        text = restore_pre_res60_p1(text)
    # ARCH-RC9: main loop uses the explicit session coordinator. The
    # historical Root-J2 SHA still describes the original raw public call.
    live = "EspNativeGameplaySessionCoordinator_service(doomRpg);"
    old = "EspNativeGameplaySession_service(doomRpg);"
    assert text.count(live) == 1, "RC9 main loop direct-call drift"
    text = text.replace(live, old, 1)
    text = restore_pre_boot_legals(text)
    # MENU-M1 adds a bounded title prestart between model creation and first
    # dashboard present. Reverse ONLY that exact later insertion for Root-J2's
    # frozen, historically tested main.cpp blob.
    title_current = (
        "        /* MENU-M1: title audio is already playing before the first MENU_MAIN\n"
        "         * physical present. A missing WAV never blocks navigation. */\n"
        "        const bool menuBuilt =\n"
        "            doomRpg != nullptr &&\n"
        "            DoomRPG_esp32MainMenuModelBuildMain(doomRpg) != 0;\n"
        "        if (menuBuilt) (void)EspEntranceMusic_prestartMenuBeforeFirstFrame();\n"
        "        engineMainMenuReady =\n"
        "            menuBuilt &&\n"
        "            DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV) != 0;\n"
        "        if (!engineMainMenuReady && menuBuilt)\n"
        "            EspEntranceMusic_stopMenuBeforeAction();\n"
    )
    title_original = (
        "        engineMainMenuReady =\n"
        "            doomRpg != nullptr &&\n"
        "            DoomRPG_esp32MainMenuModelBuildMain(doomRpg) != 0 &&\n"
        "            DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV) != 0;\n"
    )
    assert text.count(title_current) == 1, "MENU-M1 first-title-present drift"
    text = text.replace(title_current, title_original, 1)
    # SOUND-S1 and INTRO-I1 preserve the exact older Root-J2 main image by
    # reversing ONLY their two main-loop/boot insertions. Keep the historical
    # blob SHA, next R2a/R1 inverse and original ownership untouched.
    idle_hook = (
        '    /* SOUND=ON by default: drive DAC2 at digital silence before menus/intro.\n'
        '     * No timer, SD read, ring or audio allocation here. */\n'
        '    EspEntranceMusic_syncIdleOutput();\n'
    )
    intro_hook = (
        '    /* Release the one intro SD File/ring AFTER FINAL-CONTINUE has parked the\n'
        '     * clock, but BEFORE the next clock service starts BSP/flash loading.\n'
        '     * Preserve the native intro C files byte-exact (SDL-F8 regression). */\n'
        '    EspEntranceMusic_stopIntroAtHandoff();\n'
    )
    assert text.count(idle_hook) == 1, "AUDIO-S1 boot idle integration drift"
    assert text.count(intro_hook) == 1, "AUDIO-I1 pre-BSP release integration drift"
    text = text.replace(idle_hook, "", 1).replace(intro_hook, "", 1)
    # SFX-P0 added a strictly temporary PAK-format scan before AUDIO-R2a.
    # Remove precisely the added header and call, retaining the historical
    # fixed Root-J2 Git blob (never update the SHA witness).
    sfx_include = '#include "esp_native_sfx_probe.h"\n'
    sfx_hook = (
        '    /* SFX-P0: first validate the original WAV formats from verified PAK.\n'
        '     * Exactly once per gameplay session, before music producer activity. */\n'
        '    EspNativeSfxProbe_service(EspNativeGameplaySession_isActive());\n'
    )
    assert text.count(sfx_include) == 1, "SFX-P0 header integration drift"
    assert text.count(sfx_hook) == 1, "SFX-P0 one-shot service integration drift"
    text = text.replace(sfx_include, "", 1).replace(sfx_hook, "", 1)
    include = '#include "esp_asset_pack.h"\n#include "esp_audio_entrance_music.h"\n'
    hook = (
        '    /* AUDIO-R2a: selected Entrance soundtrack, SD IMA-ADPCM 22050Hz.\n'
        '     * Game-loop producer owns all SD reads/decoding; DAC timer ISR DRAM-only.\n'
        '     * Automatically stop and close on session or map exit, fail closed. */\n'
        '    EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);\n'
    )
    assert text.count(include) == 1, "AUDIO-R2a include integration drift"
    assert text.count(hook) == 1, "AUDIO-R2a service integration drift"
    return text.replace(include, '#include "esp_asset_pack.h"\n', 1).replace(hook, "", 1)
