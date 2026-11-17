#!/usr/bin/env python3
"""AUDIO-LOAD-L1: final READY physical screen stays visible until selected music starts.

Fail-closed integration test: no new ring/ISR/producer, no SD I/O in
BSP or flash callbacks; same fully primed final-world frame as before.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
inc=root/"ESP32/include"
sess=(src/"esp_native_gameplay_session.c").read_text()
audio=(src/"esp_audio_entrance_music.cpp").read_text()
header=(src/"esp_audio_entrance_music.h").read_text()
screen=(src/"esp_native_transition_presentation.c").read_text()
policy=(inc/"esp_audio_loading_route.h").read_text()
ci=(root/".github/workflows/esp32-cyd.yml").read_text()

assert 'EspEntranceMusic_stopBeforeLoading(void);' in header
assert 'EspEntranceMusic_prestartGameplayBeforeFirstFrame(uint8_t targetMapId);' in header
assert 'void EspEntranceMusic_stopBeforeLoading(void) {' in audio
assert 'int EspEntranceMusic_prestartGameplayBeforeFirstFrame(uint8_t targetMapId) {' in audio
assert 'EspAudioLoadingRoute_forMap(targetMapId)' in audio
assert 'ESP_AUDIO_LOADING_TRACK_DEFERRED' in audio
assert 'EspNativeAudioIntent_isSoundEnabled()' in audio
assert 'EspAssetPack_isMapFlashActive()' in audio
assert 'EspAssetPack_isResident()' in audio
assert 'view->targetMapId!=targetMapId' in audio
assert 'view->gameplayLoadMapId!=targetMapId' in audio
assert 'closeMusic("LOAD_PRESTART_OWNER_NOT_READY");' in audio
assert 'attempted=true;' in audio

stop=audio[audio.index('void EspEntranceMusic_stopBeforeLoading(void) {'):
           audio.index('int EspEntranceMusic_prestartGameplayBeforeFirstFrame(')]
assert 'closeMusic("LOAD_START_BEFORE_BSP")' in stop
assert 'attempted=false;' in stop and 'activeIntroTrack=false;' in stop
assert 'EspEntranceMusic_syncIdleOutput();' in stop
assert '[LOADAUDIO-L1] STOP_FOR_LOADING' in stop

pre=audio[audio.index('int EspEntranceMusic_prestartGameplayBeforeFirstFrame('):
          audio.index('void EspEntranceMusic_syncIdleOutput(void) {')]
assert 'EspEntranceMusic_service(true,true);' in pre
assert 'while ((uint32_t)(millis()-startMs)<1800U)' in pre
assert 'if (phase==kPlay) break;' in pre
assert 'delay(1);' in pre
assert 'closeMusic("LOAD_PRESTART_TIMEOUT")' in pre
assert 'if (!ready) attempted=true;' in pre
assert 'beforeWorldPresent=1' in pre
assert 'malloc' not in pre and 'SD.open' not in pre
assert 'kRingCapacity=16384U' in audio
assert 'kIntroGainDiv=33U' in audio and 'kEntranceGainDiv=15U' in audio

begin=screen[screen.index('int EspNativeTransitionPresentation_beginLoading('):
             screen.index('void EspNativeTransitionPresentation_progress(')]
assert begin.index('EspEntranceMusic_stopBeforeLoading();') < begin.index(
    'EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)')
assert 'EspEntranceMusic_prestartGameplayBeforeFirstFrame' not in screen
assert 'EspEntranceMusic_service' not in screen
assert 'EspEntranceMusic_pumpGameplay' not in screen
assert 'SD.open' not in screen

active=sess[sess.index('sessionState.stage = SESSION_STAGE_ACTIVE;'):
            sess.index('printf("[ENGINESESSION] %s-VISIBLE')]
assert (active.index('EspNativeTransitionPresentation_checkpointProgress(100U, "READY");') <
        active.index('renderCacheWitness(doomRpgBase, view, "FINAL-READY"') <
        active.index('EspNativeTransitionPresentation_releaseLoading(releaseReason);') <
        active.index('EspNativeGameplayHud_repaint(hud, &hudStats)') <
        active.index('EspNativeSfxProbe_service(true);') <
        active.index('EspEntranceMusic_prestartGameplayBeforeFirstFrame(') <
        active.index('if (!Esp32PlatformVideo_present())'))
assert 'EspAudioLoadingRoute_forMap(view->targetMapId)' in active
assert 'EspNativeAudioIntent_isSoundEnabled()' in active
assert 'ESP_AUDIO_LOADING_TRACK_M_STALKS' in active
assert 'EspNativeTransitionPresentation_isLoadingActive()' in active
assert 'EspNativeSfxProbe_service(true);' in active

# No map-wide music: only Entrance ID 1 has a known title.
assert 'mapId == ESP_MAP_ID_INTRO' in policy
assert 'ESP_AUDIO_LOADING_TRACK_DEFERRED' in policy
assert 'Test AUDIO-LOAD-L1' in ci
isr=audio[audio.index('static bool IRAM_ATTR onAudioTick'):
          audio.index('static uint32_t queued()')]
assert 'EspEntranceMusic_prestartGameplay' not in isr
assert 'SD.' not in isr and 'delay(' not in isr

print('PASS AUDIO-LOAD-L1: final-ready loading -> scan PAK -> one music prestart -> world present, no loader audio')
