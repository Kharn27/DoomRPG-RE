#!/usr/bin/env python3
"""SDL-F27: ten actual native gameplay C modules do not import SDL.h in
firmware. Reconstruct exact pre-F27 Git source bodies and compile the real
C11 DoomRPG/Render headers with native, host and geometry SDL boundaries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "ESP32/src"
BEFORE = {
    "esp_native_gameplay_action_engine.c": "aa55c75203ccd8ac9acc3c0b4b38f2021e13d214",
    "esp_native_gameplay_frame.c": "5c0cd0bf64beb8279ce992ff99a46f2fb3485884",
    "esp_native_gameplay_gib_fx.c": "12254012ee7c9089ee1a5e1eacbe200dd5626a55",
    "esp_native_gameplay_hazard_touch.c": "91f4b11fb4314007476318592ef4ceaf1825e257",
    "esp_native_gameplay_pickup.c": "19edc71d41586c2d93c754cf094ed34ffc9dd493",
    "esp_native_gameplay_player_death.c": "82f1cb8a4c6c573e6fa399f018b4564b6977ff1d",
    "esp_native_gameplay_player_resources.c": "8681d358abdc69af95c7362d0b295c777dc387f1",
    "esp_native_gameplay_session.c": "b84f7f973993e79135ab84963ae36fbf5ebc34eb",
    "esp_native_gameplay_weapon.c": "c12d2462615e1cd4ac02c02e7e911678f1ba24a3",
    "esp_native_resident_gameplay.c": "47ad4211f07e87dfa98dde050a26802d5cd0b760"
}
GUARD = ("#if !defined(DOOMRPG_ESP32) || "
         "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
         "#include <SDL.h>\n"
         "#endif\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()


def strip_ra2b_probe(source: bytes) -> bytes:
    """The R-A2b instrumentation is an explicitly opt-in, small diagnostic.
    Remove EXACTLY its four compiler-gated blocks before comparing against the
    immutable pre-F27 Git C body. Do not weaken any other 9 body oracles.
    """
    flag = b"#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE"
    assert source.count(flag) == 4, "R-A2b probe blocks missing or unexpected"
    for witness in (
        b"RA2B_MAX_FRAME_WITNESSES 48U",
        b"EspAssetPack_ra2bSdRangeMicros()",
        b"ra2bFlashBefore = EspAssetPack_mapFlashRuntimeReadMicros()",
        b'printf("[RA2B] FRAME v=1',
    ):
        assert witness in source, "R-A2b diagnostic contract changed"
    stripped, count = re.subn(
        rb"\n#if defined\(DOOMRPG_RA2B_CAPTURE\) && DOOMRPG_RA2B_CAPTURE\n"
        rb".*?^#endif\n",
        b"\n", source, flags=re.MULTILINE | re.DOTALL)
    assert count == 4, "Unexpected R-A2b probe extraction count"
    # The first diagnostic block was inserted after an intentionally empty
    # separator line. Restore the exact historical adjacency for this ONE
    # original C body, leaving every other line byte-identical.
    separator = (b"static GameplayFrameScratch frameScratch;\n\n"
                 b"/* PR236 B probe")
    assert stripped.count(separator) == 1, "R-A2b frame separator changed"
    stripped = stripped.replace(separator,
                                b"static GameplayFrameScratch frameScratch;\n"
                                b"/* PR236 B probe", 1)
    assert b"RA2B" not in stripped, "R-A2b diagnostic escaped its guards"
    return stripped


def original_body_parity() -> None:
    marker = GUARD.encode("ascii")
    for filename, blob in BEFORE.items():
        current = (SOURCES / filename).read_bytes()
        from game_ai14_enemy_barrel_history import restore_pre_enemy_barrel
        current = restore_pre_enemy_barrel(filename, current)
        from game_ai14_a2_blast_shake_history import restore_pre_ai14_blast_shake
        current = restore_pre_ai14_blast_shake(filename, current)
        from perf_game_p2a_history import restore_pre_perf_game_p2a
        current = restore_pre_perf_game_p2a(filename, current)
        # Remove ONLY P1 timing additions before historical Git identity.
        if filename == "esp_native_gameplay_frame.c":
            from perf_game_p1_history import restore_pre_perf_game_p1_frame
            current = restore_pre_perf_game_p1_frame(
                current.decode("utf-8")).encode("utf-8")
        elif filename == "esp_native_resident_gameplay.c":
            from perf_game_p1_history import restore_pre_perf_game_p1_resident
            current = restore_pre_perf_game_p1_resident(
                current.decode("utf-8")).encode("utf-8")
        # ARCH-RC24 has no GNU runtime alias; undo only public/physical
        # present spellings for the immutable pre-SDL-F27 source witnesses.
        if filename == "esp_native_gameplay_action_engine.c":
            # Strict inverse: committed monster splash sounds are additive.
            # No relaxation of the 10 immutable historical Git blobs.
            from game_ai14_a2_direct_death_action_history import restore_pre_direct_death_action
            current = restore_pre_direct_death_action(current)
            from game_ai14_a2_ordered_radius_history import restore_pre_ai14_ordered_radius
            current = restore_pre_ai14_ordered_radius(current)
            from game_ai14_a2_terminal_splash_history import restore_pre_ai14_terminal_splash, ACTION
            current = restore_pre_ai14_terminal_splash(current, ACTION)
            from game_ai14_a2_blast_source_history import restore_pre_ai14_blast_source
            current = restore_pre_ai14_blast_source(current)
            from audio_barrel_splash_history import restore_pre_barrel_splash_audio
            current = restore_pre_barrel_splash_audio(filename, current)
        if filename in ("esp_native_gameplay_action_engine.c",
                        "esp_native_gameplay_gib_fx.c"):
            from arch_rc24_history import restore_pre_rc24
            current = restore_pre_rc24(filename, current)
        # Undo RC23 and RC22 native owner renames before frozen Git parity;
        # keep the pre-F27 immutable blob SHAs STRICT and unchanged.
        from arch_rc23_history import restore_pre_rc23
        current = restore_pre_rc23(filename, current)
        from arch_rc22_history import restore_pre_rc22
        current = restore_pre_rc22(filename, current)
        from arch_rc21_history import restore_pre_rc21
        current = restore_pre_rc21(filename, current)
        from arch_rc20_history import restore_pre_rc20
        current = restore_pre_rc20(filename, current)
        from arch_rc14_history import restore_pre_rc14
        current = restore_pre_rc14(filename, current)
        from arch_rc13_history import restore_pre_rc13
        current = restore_pre_rc13(filename, current)
        assert current.startswith(marker), (
            f"{filename}: expected bounded host/geometry-only SDL guard")
        # GAME-PICKUP is a post-F27 presentation optimization. Restore only
        # its exact additive source delta before any older historical inverse.
        if filename == "esp_native_gameplay_player_resources.c":
            # Undo AUDIO-PICKUP #419 BEFORE the older GAME-PICKUP inverse,
            # which rewrites DRPG_LOGD into historical printf spellings.
            # The later inverse must see the exact pre-#419 C bytes.
            from audio_pickup_modal_history import restore_pre_audio_pickup_modal_resources
            current = restore_pre_audio_pickup_modal_resources(current)
            from game_pickup_redraw_history import restore_pre_game_pickup_resources
            current = restore_pre_game_pickup_resources(current)
        elif filename == "esp_native_resident_gameplay.c":
            from audio_door_history import restore_pre_audio_door_resident
            current = restore_pre_audio_door_resident(current)
            from game_pickup_redraw_history import restore_pre_game_pickup_resident
            current = restore_pre_game_pickup_resident(current)

        if filename == "esp_native_gameplay_frame.c":
            # AUDIO-R2d adds a single synchronous SD pump after a successful
            # frame. Reverse ONLY these two additions before checking the
            # immutable pre-F27 Git blob. No weakened hashes or skipped TU.
            audio_import = (b"#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
                            b'#include "esp_audio_entrance_music.h"\n'
                            b"#endif\n")
            audio_hook = (
                b"#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
                b"    /* AUDIO-R2d cooperative main-task inter-frame refill. Renderer and PAK\n"
                b"     * have completed/released ownership above. Never read SD during raster\n"
                b"     * or the TFT present, and never in the timer ISR. Metrics were frozen\n"
                b"     * above before servicing audio; gameplay state and frame FNV untouched. */\n"
                b"    if (ok && publish) EspEntranceMusic_pumpGameplay();\n"
                b"#endif\n"
            )
            assert current.count(audio_import) == 1
            assert current.count(audio_hook) == 1
            current = current.replace(audio_import, b"", 1)
            current = current.replace(audio_hook, b"", 1)
            current = strip_ra2b_probe(current)
        if filename == "esp_native_gameplay_session.c":
            # AUDIO-LOAD-L1 preserves the immutable pre-F27 session body.
            # Reverse ONLY the four new imports and one pre-world audio
            # boundary before comparing the exact historical Git blob.
            l1_import = (
                b'#include "esp_audio_entrance_music.h" /* LOAD-L1: after full loading, before gameplay present */\n'
                b'#include "esp_audio_loading_route.h"\n'
                b'#include "esp_native_audio_intent.h" /* SOUND OFF never scans PAK before first frame */\n'
                b'#include "esp_native_sfx_probe.h" /* drain one-shot PAK metadata before music starts */\n'
            )
            l1_hook = (
                b"                    /* LOAD-L1: the full 3D world and HUD are in the retained\n"
                b"                     * logical framebuffer, but the physical screen is STILL\n"
                b"                     * the final 100% READY loading card. No BSP/flash/cache\n"
                b"                     * access remains. Finish the one-shot SFX PAK scan BEFORE\n"
                b"                     * opening the music SD File; then start selected music\n"
                b"                     * (with bounded clock guard) immediately before presenting\n"
                b"                     * the world. Missing/unknown tracks fail silently. */\n"
                b"                    if (EspAudioLoadingRoute_forMap(view->targetMapId) ==\n"
                b"                            ESP_AUDIO_LOADING_TRACK_M_STALKS &&\n"
                b"                        EspNativeAudioIntent_isSoundEnabled()) {\n"
                b"                        EspNativeSfxProbe_service(true);\n"
                b"                    }\n"
                b"                    (void)EspEntranceMusic_prestartGameplayBeforeFirstFrame(\n"
                b"                        view->targetMapId);\n"
            )
            # Junction-M1 changes only the map-route predicate of the
            # pre-world SFX/music boundary; restore old predicate for
            # immutable SDL-F27 SHA, preserving ARCH-RC11 restorations.
            junction_hook = l1_hook.replace(
                b"EspAudioLoadingRoute_forMap(view->targetMapId) ==",
                b"EspAudioLoadingRoute_forMap(view->targetMapId) !="
            ).replace(
                b"ESP_AUDIO_LOADING_TRACK_M_STALKS &&",
                b"ESP_AUDIO_LOADING_TRACK_DEFERRED &&"
            )
            assert current.count(l1_import)==1, "LOAD-L1 session includes drift"
            assert current.count(junction_hook)==1, "JUNCTION-M1 LOAD-L1 session hook drift"
            current=current.replace(l1_import,b"",1).replace(junction_hook,b"",1)
        # AUDIO-SFX-P1 publishes two *post-commit-only* native PCM intents.
        # Strictly undo exactly their imported header and bounded event hooks
        # before the frozen ten-source pre-F27 Git blob witness. No hashes
        # are rewritten, no legacy semantics are accepted as new baselines.
        if filename == "esp_native_gameplay_action_engine.c":
            # AUDIO-BARREL-VISUAL: reverse just the two newly positioned
            # audio callbacks before verifying the ORIGINAL frozen Git blob.
            # The rollback/no-frame paths remain unmodified.
            gun_visible = (
                b"        /* BARREL-GUN-VISUAL: original Combat_playerSeq sounds the weapon at\n"
                b"         * its firing pose, NOT after all the world/explosion animations.\n"
                b"         * The real attack frame has been presented and PAK is closed. This\n"
                b"         * cue describes that already visible shot even if a later wave\n"
                b"         * needs rollback; a failed/no-present attack never reaches here. */\n"
                b"        if (isBarrel && frame.finalPresented != 0U &&\n"
                b"            crateWeapon != NULL && crateWeapon->resourceId != 0U) {\n"
                b"            (void)EspNativeAudioIntent_publish(\n"
                b"                (uint16_t)crateWeapon->resourceId, 0U, 5U);\n"
                b"            EspEntranceMusic_pumpGameplay();\n"
                b"            printf(\"[BARRELAUDIO] GUN-VISIBLE seq=%u resource=%u priority=5 phase=attack-frame audioPump=immediate\\n\",\n"
                b"                   (unsigned int)pending.sequence,\n"
                b"                   (unsigned int)crateWeapon->resourceId);\n"
                b"        }\n\n"
            )
            blast_visible = (
                b"                    /* BARREL-5061-VISUAL: original Game_gsprite_allocAnim\n"
                b"                     * starts the blast sound when its first actual frame\n"
                b"                     * becomes visible, not after 3 frames and radius work.\n"
                b"                     * Exactly one intent per chain, no speculative sound\n"
                b"                     * if the wave frame was not presented successfully. */\n"
                b"                    if (waveOrdinal == 1U &&\n"
                b"                        barrelExplosionFrame == 0U &&\n"
                b"                        barrelRemoved != 0U && barrelExplosionArmed != 0U &&\n"
                b"                        frame.finalPresented != 0U) {\n"
                b"                        (void)EspNativeAudioIntent_publish(5061U, 0U, 4U);\n"
                b"                        EspEntranceMusic_pumpGameplay();\n"
                b"                        printf(\"[BARRELAUDIO] BLAST-VISIBLE seq=%u resource=5061 priority=4 wave=1 frame=0 audioPump=immediate\\n\",\n"
                b"                               (unsigned int)pending.sequence);\n"
                b"                    }\n"
            )
            music_include=b'#include "esp_audio_entrance_music.h"\n'
            visual_log=(
                b"explosion=%s sound=%u-attack-visible "
                b"barrelExplosionSound=5061-first-wave-visible"
            )
            original_log=(
                b"explosion=%s sound=%u-deferred "
                b"barrelExplosionSound=5061-deferred"
            )
            assert current.count(gun_visible)==1, "BARREL-GUN visual owner drift"
            assert current.count(blast_visible)==1, "BARREL-5061 visual owner drift"
            assert current.count(music_include)==1, "BARREL music pump import drift"
            assert current.count(visual_log)==1, "BARREL audio log drift"
            current=(current.replace(gun_visible,b"",1)
                            .replace(blast_visible,b"",1)
                            .replace(music_include,b"",1)
                            .replace(visual_log,original_log,1))
            # AUDIO-SFX-P2 adds one committed extinguisher hook. Undo only
            # that exact two-line addition before the frozen pre-F27 blob.
            p2_hook = (
                b"            /* P2: only after a committed fire removal, never rollback. */\n"
                b"            (void)EspNativeAudioIntent_publish(5045U,0U,3U);\n"
            )
            assert current.count(p2_hook)==1, "SFX-P2 fire hook drift"
            current = current.replace(p2_hook, b"", 1)
            p1_import = b'#include "esp_native_audio_intent.h"\n'
            p1_hook = (
                b"            /* SFX-P1: publish only a successfully committed pistol/crate\n"
                b"             * hit. No I/O here, and no audio on rollback/invalid outcome. */\n"
                b"            if (crateWeapon != NULL && crateWeapon->resourceId==5046U &&\n"
                b"                crateOutcome != ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID)\n"
                b"                (void)EspNativeAudioIntent_publish(5046U,0U,3U);\n"
            )
            assert current.count(p1_import) == 1, "SFX-P1 crate import drift"
            assert current.count(p1_hook) == 1, "SFX-P1 crate commit hook drift"
            current = current.replace(p1_import, b"", 1).replace(p1_hook, b"", 1)
        elif filename == "esp_native_gameplay_player_resources.c":
            # Newer AUDIO-PICKUP and GAME-PICKUP inverse already applied above.
            # Continue with the original SFX-P1 inverse, no baseline changes.
            p1_import = b'#include "esp_native_audio_intent.h"\n'
            p1_hook = (
                b"        /* SFX-P1: only after irreversible resource/player/world commit.\n"
                b"         * Original desktop generic pickup cue 5060; no PAK access here. */\n"
                b"        (void)EspNativeAudioIntent_publish(5060U,0U,3U);\n"
            )
            assert current.count(p1_import) == 1, "SFX-P1 pickup import drift"
            assert current.count(p1_hook) == 1, "SFX-P1 pickup commit hook drift"
            current = current.replace(p1_import, b"", 1).replace(p1_hook, b"", 1)
        if filename == "esp_native_resident_gameplay.c":
            from audio_menu_m1_history import restore_pre_menu_audio_resident
            current = restore_pre_menu_audio_resident(current.decode("utf-8")).encode("utf-8")
        if filename == "esp_native_gameplay_gib_fx.c":
            # RC8: reverse only the explicit turn hook/reset after resources.
            added_header = b'#include "esp_native_gameplay_monster_turn.h"\n'
            added_service = (b'    EspNativeGameplayPlayerResources_sessionService(doomRpg);\n'
                             b'    EspNativeGameplayMonsterTurn_afterPlayerResourcesService(doomRpg);\n')
            prior_service = b'    EspNativeGameplayPlayerResources_sessionService(doomRpg);\n'
            added_reset = (b'    EspNativeGameplayMonsterTurn_reset();\n'
                           b'    EspNativeGameplayPlayerResources_sessionReset();\n')
            prior_reset = b'    EspNativeGameplayPlayerResources_sessionReset();\n'
            assert current.count(added_header) == 1 and current.count(added_service) == 1
            assert current.count(added_reset) == 1, "RC8 gibFX drift"
            current = current.replace(added_header, b"", 1)
            current = current.replace(added_service, prior_service, 1)
            current = current.replace(added_reset, prior_reset, 1)

        if filename == "esp_native_gameplay_gib_fx.c":
            # ARCH-RC9: restore only outer wrapper symbols/comment/un-defs
            # before ARCH-RC8's exact historical inverse below has run.
            new_comment = (
                b"/* RC9: explicit native session coordinator. Its service/reset choreography is\n"
                b" * unchanged from the proven outer wrapper, including fatal/early leaf returns. */\n"
            )
            old_comment = (
                b"/* This translation unit now owns the public session wrapper in front of the\n"
                b" * proven player-resource chain. The resource implementation is renamed to the\n"
                b" * private leaves declared by esp_native_gameplay_player_resources.h. */\n"
                b"#undef __wrap_EspNativeGameplaySession_reset\n"
                b"#undef __wrap_EspNativeGameplaySession_service\n\n"
            )
            coordinator_include = b'#include "esp_native_gameplay_session.h" /* RC9 coordinator declarations */\n'
            assert current.count(coordinator_include) == 1, "RC9 coordinator header drift"
            current = current.replace(coordinator_include, b"", 1)
            assert current.count(new_comment) == 1, "RC9 coordinator comment/owner drift"
            assert current.count(b"void EspNativeGameplaySessionCoordinator_service(") == 1
            assert current.count(b"void EspNativeGameplaySessionCoordinator_reset(") == 1
            current = current.replace(new_comment, old_comment, 1)
            current = current.replace(b"void EspNativeGameplaySessionCoordinator_service(",
                                      b"void __wrap_EspNativeGameplaySession_service(", 1)
            current = current.replace(b"void EspNativeGameplaySessionCoordinator_reset(",
                                      b"void __wrap_EspNativeGameplaySession_reset(", 1)
        if filename == "esp_native_gameplay_player_resources.c":
            # The old RC8 private leaves still used preprocessor __wrap
            # source spellings, which RC9 now retires as well.
            before_reset = b"void EspNativeGameplayPlayerResources_sessionReset(void) {"
            before_service = b"void EspNativeGameplayPlayerResources_sessionService(struct DoomRPG_s* doomRpg) {"
            assert current.count(before_reset) == 1 and current.count(before_service) == 1
            current = current.replace(before_reset,
                b"void __wrap_EspNativeGameplaySession_reset(void) {", 1)
            current = current.replace(before_service,
                b"void __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {", 1)
            # ARCH-RC9: direct raw leaf replaces GNU __real aliases. Undo
            # precisely two callsites and original declarations for F27 SHA.
            assert current.count(b"    EspNativeGameplaySession_service(doomRpg);") == 1
            assert current.count(b"    EspNativeGameplaySession_reset();") == 1
            current = current.replace(b"    EspNativeGameplaySession_service(doomRpg);",
                                      b"    __real_EspNativeGameplaySession_service(doomRpg);", 1)
            current = current.replace(b"    EspNativeGameplaySession_reset();",
                                      b"    __real_EspNativeGameplaySession_reset();", 1)
            anchor = b"    const EspPlayerViewState* preparedAfter);\n"
            prototypes = (b"void __real_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg);\n"
                          b"void __real_EspNativeGameplaySession_reset(void);\n")
            assert current.count(anchor) == 1, "RC9 resource declarations anchor drift"
            current = current.replace(anchor, anchor + prototypes, 1)
        if filename == "esp_native_resident_gameplay.c":
            from arch_rc11_history import restore_pre_rc11_resident
            from arch_rc10_history import restore_pre_rc10_resident
            current = restore_pre_rc11_resident(current)
            current = restore_pre_rc10_resident(current)
            # ARCH-RC9: the native EXIT route calls the explicit coordinator.
            assert current.count(b"    EspNativeGameplaySessionCoordinator_reset();") == 1
            current = current.replace(b"    EspNativeGameplaySessionCoordinator_reset();",
                                      b"    EspNativeGameplaySession_reset();", 1)
        if filename == "esp_native_gameplay_pickup.c":
            # ARCH-RC9: the historical weapon-only pickup session leaves had
            # no live caller after PlayerResources became canonical. Reconstruct
            # the two exact dormant bodies and the obsolete GNU aliases solely
            # inside this frozen pre-SDL-F27 full-blob oracle.
            old_aliases = "void __real_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg);\nvoid __real_EspNativeGameplaySession_reset(void);\n\n".encode("utf-8")
            old_leaves = "void __wrap_EspNativeGameplaySession_reset(void) {\n    EspNativeGameplayActionEngine_reset();\n    EspNativeGameplayPickup_reset();\n    __real_EspNativeGameplaySession_reset();\n}\n\nvoid __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {\n    if (pickup.fatal != 0U) return;\n    __real_EspNativeGameplaySession_service(doomRpg);\n    EspNativeGameplayPickup_logCorpus();\n    EspNativeGameplayInteractionInventory_log();\n    servicePendingMove(doomRpg);\n    if (!EspNativeGameplayActionEngine_service(doomRpg)) {\n        printf(\"[ACTIONENGINE] FAILED reason=service fatal=1\\n\");\n        pickup.fatal = 1U;\n    }\n}\n".encode("utf-8")
            tile_anchor = b"static uint16_t tileForView(const EspPlayerViewState* view) {"
            assert current.count(tile_anchor) == 1, "RC9 pickup tile anchor drift"
            assert current.endswith(b"}\n\n"), "RC9 pickup tail drift"
            assert b"__wrap_EspNativeGameplaySession_" not in current
            current = current.replace(tile_anchor, old_aliases + tile_anchor, 1)
            current += old_leaves
        restored = b"#include <SDL.h>\n" + current[len(marker):]
        assert git_blob(restored) == blob, (
            f"{filename}: original pre-F27 body not preserved exactly")
        assert current.count(b"#include <SDL.h>") == 1, filename
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|SDLCALL|Uint(?:8|16|32))\b",
            clean), f"{filename}: SDL API retained in body"
        assert '#include "DoomRPG.h"' in clean, filename
    print("SDL-F27 PASS sources: 10 real gameplay C bodies are exact "
          "pre-F27 Git blobs, native SDL.h imports absent")


def header_probe() -> None:
    code = r"""
#include <stdint.h>
#if EXPECT_SDL
#include <SDL.h>
#endif
#include "DoomRPG.h"
#include "Render.h"
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "host SDL oracle header no longer included"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native DoomRPG/Render headers imported SDL"
#endif
_Static_assert(sizeof(EspNativeRect) == 4*sizeof(int),
               "native render rectangle ABI drift");
#endif
_Static_assert(sizeof(Random_t) == 132, "RNG ABI changed");
int main(void) {
    Random_t state = {0};
    return state.nextRand != 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl-f27-gameplay-") as temp:
        root = Path(temp)
        src, exe = root / "probe.c", root / "probe"
        src.write_text(code, encoding="utf-8")
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "ESP32/test/oracles"),
                "-I", str(ROOT / "ESP32/include"),
               "-I", str(ROOT / "src")]
        for label, flags in (
            ("native", ["-DDOOMRPG_ESP32=1", "-DEXPECT_SDL=0"]),
            ("desktop", ["-DEXPECT_SDL=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1",
                          "-DEXPECT_SDL=1"]),
        ):
            subprocess.run(cmd + flags + [str(src), "-o", str(exe)],
                           check=True)
            subprocess.run([str(exe)], check=True)
            print(f"SDL-F27 PASS C11 {label}: native DoomRPG/Render ABI")


def compiled_profile(profile: str) -> None:
    db = ROOT / "ESP32/compile_commands.json"
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    assert db.is_file(), f"{profile}: missing compiledb"
    assert elf.is_file(), f"{profile}: missing ELF"
    compiled = {Path(x["file"]).name for x in
                json.loads(db.read_text(encoding="utf-8"))}
    assert all(name in compiled for name in BEFORE), (
        f"{profile}: F27 native gameplay source not compiled")
    assert "esp32_sdl.cpp" not in compiled, profile
    assert "esp32_bmp.cpp" not in compiled, profile
    print(f"SDL-F27 PASS {profile}: 10 gameplay TUs compiled; "
          "old SDL shim and BMP excluded; existing ELF SDL-F3 audit retained")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=(
        "esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    args = parser.parse_args()
    original_body_parity()
    if args.profile:
        compiled_profile(args.profile)
    else:
        header_probe()


if __name__ == "__main__":
    main()
