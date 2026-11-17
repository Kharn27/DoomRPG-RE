"""Strict inverse of the new g.bmp pre-title boot stage.

Used only when an old Root-J/SDL Git-blob oracle compares main.cpp to an
immutable historical SHA.  We reverse our exact independent boot delta before
existing AUDIO-R1/R2a inverses, not the historical oracle fingerprint.
"""


def restore_pre_boot_legals(text: str) -> str:
    def undo_once(new: str, old: str) -> None:
        nonlocal text
        assert text.count(new) == 1, ("BOOT-LEGALS main delta drift", new)
        text = text.replace(new, old, 1)

    undo_once('#include "esp_native_boot_legals.h"\n', "")
    undo_once(
        "    bool bootLegalsPending = false;\n"
        "    bool bootLegalsTouchDown = false;\n", ""
    )

    start = "\n    /* Original 1994-2005 legal logos, before MENU_MAIN."
    end = "    void printSystemInfo()\n"
    assert text.count(start) == 1 and text.count(end) == 1
    a = text.index(start)
    b = text.index(end, a)
    block = text[a:b]
    assert block.count("bool initializeBootLegals()") == 1
    assert block.count("void serviceBootLegals()") == 1
    assert block.count("initializeMainMenu();") == 1
    assert block.endswith("    }\n\n")
    text = text[:a] + text[b:]

    undo_once(
        "    initializeConfigMappings();\n"
        "    if (!initializeBootLegals())\n"
        "        initializeMainMenu();\n",
        "    initializeConfigMappings();\n"
        "    initializeMainMenu();\n",
    )
    undo_once(
        "void loop()\n"
        "{\n"
        "    if (bootLegalsPending)\n"
        "    {\n"
        "        serviceBootLegals();\n"
        "        printHeartbeat();\n"
        "        delay(5);\n"
        "        return;\n"
        "    }\n",
        "void loop()\n{\n",
    )
    assert "BootLegals" not in text and "bootLegals" not in text
    return text
