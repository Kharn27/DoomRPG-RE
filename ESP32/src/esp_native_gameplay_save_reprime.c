#include <stdint.h>
#include <stdio.h>

#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_native_gameplay_session.h"
#include "esp_player_view_state.h"

int __real_EspNativeGameplaySession_configure(
    const EspNativeGameplaySessionConfig* config);

int __wrap_EspNativeGameplaySession_configure(
    const EspNativeGameplaySessionConfig* config) {
    const EspPlayerViewState* view = EspPlayerView_view();

    /*
     * A checkpoint restore resumes after the transient post-spawn/facing path:
     * PlayerView is already exact and settled, while resetSpawnOwners() has
     * intentionally removed the two tiny HUD semantic owners. Normal New Game
     * reaches configure before any PlayerView exists; CHANGEMAP reaches it with
     * both owners already routed. This exact pattern therefore identifies only
     * the checkpoint reprime boundary without adding a global save-mode flag.
     */
    if (view != NULL && view->active == 1U && view->spawnApplied == 1U &&
        view->hudRefreshPending == 0U &&
        view->facingRefreshPending == 0U &&
        view->playerSetupPending == 0U &&
        view->tileEnterPending == 0U &&
        view->viewX == view->destX &&
        view->viewY == view->destY &&
        view->viewAngle == view->destAngle &&
        (view->viewAngle & 63) == 0 &&
        EspHudRefresh_peek() == NULL &&
        !EspHudPostLoadClear_isReady()) {
        const EspHudRefreshStatus refreshStatus =
            EspHudRefresh_restorePending(view);
        const EspHudPostLoadClearStatus clearStatus =
            refreshStatus == ESP_HUD_REFRESH_OK
                ? EspHudPostLoadClear_restoreSettled(view)
                : ESP_HUD_POST_LOAD_CLEAR_INVALID;

        if (refreshStatus != ESP_HUD_REFRESH_OK ||
            clearStatus != ESP_HUD_POST_LOAD_CLEAR_OK) {
            printf("[NATIVESAVE] REPRIME-HUD FAILED map=%u refresh=%u clear=%u failClosed=yes\n",
                   (unsigned int)view->targetMapId,
                   (unsigned int)refreshStatus,
                   (unsigned int)clearStatus);
            return 0;
        }

        printf("[NATIVESAVE] REPRIME-HUD map=%u gameplayLoadMapId=%u angle=%ld refresh=pending clear=ready mutation=owners-only turn=no\n",
               (unsigned int)view->targetMapId,
               (unsigned int)view->gameplayLoadMapId,
               (long)view->viewAngle);
    }

    return __real_EspNativeGameplaySession_configure(config);
}
