"""ARCH-RC13 frozen-historical SHA reconstruction ONLY.

Strictly invert the four retired action-path linker wrappers and exact API
handoffs. Never run on firmware sources or accept updated historical hashes.
"""

def _invert(source, edits):
    as_bytes = isinstance(source, bytes)
    if as_bytes:
        source = source.decode("utf-8")
    for new, old, count in edits:
        found = source.count(new)
        assert found == count, ("ARCH-RC13 historic source drift", new, found, count)
        source = source.replace(new, old)
    return source.encode("utf-8") if as_bytes else source


def restore_pre_rc13(name, source):
    edits = {
        "esp_native_gameplay_action_engine.c": (
            ("EspNativeGameplayActionStatus EspNativeGameplayActionEngine_executeSelect(",
             "EspNativeGameplayActionStatus __wrap_EspNativeGameplayAction_executeSelect(", 1),
            ("EspNativeGameplayAction_executeSelect(intent, outResult)",
             "__real_EspNativeGameplayAction_executeSelect(intent, outResult)", 1),
            ("static uint32_t actionNowMs(void) {",
             "EspNativeGameplayActionStatus __real_EspNativeGameplayAction_executeSelect(\n"
             "    const EspNativeGameplayInputState* intent,\n"
             "    EspNativeGameplayActionResult* outResult);\n\n"
             "static uint32_t actionNowMs(void) {", 1),
        ),
        "esp_native_gameplay_monster_combat.c": (
            ("/* Explicit MonsterCombat SELECT/service/reset owner; the sprite topology\n"
             " * linker wrappers remain unchanged in RC13. */",
             "/* This translation unit is the public linker-wrapper owner. The older action\n"
             " * and monster-state wrappers are private chain leaves renamed by their headers. */", 1),
            ("#undef __wrap_EspMapSpriteTopology_getVisualState\n",
             "#undef __wrap_EspMapSpriteTopology_getVisualState\n"
             "#undef __wrap_EspNativeGameplayActionEngine_service\n"
             "#undef __wrap_EspNativeGameplayActionEngine_reset\n", 1),
            ("EspNativeGameplayActionStatus EspNativeGameplayMonsterCombat_executeSelect(",
             "EspNativeGameplayActionStatus __wrap_EspNativeGameplayActionEngine_executeSelect(", 1),
            ("EspNativeGameplayActionEngine_executeSelect(intent, outResult)",
             "__real_EspNativeGameplayActionEngine_executeSelect(intent, outResult)", 1),
            ("static uint8_t p1Health(uint32_t p)",
             "EspNativeGameplayActionStatus __real_EspNativeGameplayActionEngine_executeSelect(\n"
             "    const EspNativeGameplayInputState* intent,\n"
             "    EspNativeGameplayActionResult* outResult);\n\n"
             "static uint8_t p1Health(uint32_t p)", 1),
            ("int EspNativeGameplayMonsterCombat_actionService(DoomRPG_t* runtime)",
             "int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* runtime)", 1),
            ("void EspNativeGameplayMonsterCombat_actionReset(void)",
             "void __wrap_EspNativeGameplayActionEngine_reset(void)", 1),
        ),
        "esp_native_gameplay_player_resources.c": (
            ('#include "esp_native_gameplay_monster_combat.h"\n', '', 1),
            ("    EspNativeGameplayMonsterCombat_actionReset();",
             "    EspNativeGameplayActionEngine_reset();", 1),
            ("    if (!EspNativeGameplayMonsterCombat_actionService(doomRpg)) {",
             "    if (!EspNativeGameplayActionEngine_service(doomRpg)) {", 1),
        ),
        "esp_native_gameplay_session.c": (
            ('#include "esp_native_gameplay_monster_combat.h"\n', '', 1),
            ("    EspNativeGameplayMonsterCombat_actionReset();",
             "    EspNativeGameplayActionEngine_reset();", 1),
        ),
        "esp_native_resident_gameplay.c": (
            ("EspNativeGameplayTransition_executeSelect(intent, &result)",
             "EspNativeGameplayAction_executeSelect(intent, &result)", 1),
        ),
        "esp_native_gameplay_monster_state.c": (
            ("int EspNativeGameplayMonsterState_actionService(DoomRPG_t* doomRpg)",
             "int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg)", 1),
            ("EspNativeGameplayActionEngine_service(doomRpg)",
             "__real_EspNativeGameplayActionEngine_service(doomRpg)", 2),
            ("void EspNativeGameplayMonsterState_actionReset(void)",
             "void __wrap_EspNativeGameplayActionEngine_reset(void)", 1),
            ("    EspNativeGameplayActionEngine_reset();",
             "    __real_EspNativeGameplayActionEngine_reset();", 1),
            ("int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg) {",
             "int __real_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg);\n"
             "void __real_EspNativeGameplayActionEngine_reset(void);\n\n"
             "int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* doomRpg) {", 1),
        ),
        "esp_native_gameplay_transition.c": (
            ('#include "esp_native_gameplay_monster_combat.h"\n', '', 1),
            ("#define TRANSITION_NAME_CAPACITY",
             "/* action_engine.h deliberately renames the historical SELECT wrapper into the\n"
             " * private fallback leaf. This translation unit owns the one public linker\n"
             " * wrapper and therefore restores the real wrapper token here. */\n"
             "#undef __wrap_EspNativeGameplayAction_executeSelect\n\n"
             "#define TRANSITION_NAME_CAPACITY", 1),
            ("/* Explicit SELECT entrypoint: transition event first, then MonsterCombat,\n"
             " * generic ActionEngine and the raw event action, in the original order. */",
             "/* Public SELECT wrapper: transition event first, then the already-proven\n"
             " * tile-event/action-engine chain. This preserves legacy event-before-entity\n"
             " * ordering while keeping non-transition SELECT behavior byte-for-byte in the\n"
             " * existing private leaf. */", 1),
            ("EspNativeGameplayActionStatus EspNativeGameplayTransition_executeSelect(",
             "EspNativeGameplayActionStatus __wrap_EspNativeGameplayAction_executeSelect(", 1),
            ("EspNativeGameplayMonsterCombat_executeSelect(intent, outResult)",
             "EspNativeGameplayActionEngine_executeSelect(intent, outResult)", 2),
        ),
    }
    return _invert(source, edits.get(name, ()))
