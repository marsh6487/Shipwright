#include "GlobalOutdoorRain.h"

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state) {
    if (!state.enabled || !state.outdoors) {
        return state.ownsRain ? GlobalOutdoorRainDecision::Stop : GlobalOutdoorRainDecision::NoChange;
    }
    if (state.ownsRain) {
        return GlobalOutdoorRainDecision::Maintain;
    }
    return state.rainAlreadyActive ? GlobalOutdoorRainDecision::NoChange : GlobalOutdoorRainDecision::Start;
}

int GlobalOutdoorRain_ClampDensity(int density) {
    if (density < 0) {
        return 0;
    }
    return density > 64 ? 64 : density;
}

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "code/concurrent_weather_audio.h"

extern "C" {
#include "functions.h"
#include "sfx.h"
#include "variables.h"
#include "z64.h"
extern PlayState* gPlayState;
}

static constexpr int kRainDensity = 25;
static bool sOwnsRain = false;

static void PlayRainLoop() {
    if (Audio_IsNatureRainEnabled()) {
        return;
    }
    static float rainVolume = 0.5f;
    rainVolume = ConcurrentWeatherAudio_ClampPercent(
                     CVarGetInteger(CVAR_AUDIO("ProximityWeatherRainVolume"), 50)) /
                 100.0f;
    Audio_PlaySoundGeneral(NA_SE_EV_RAIN - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &rainVolume,
                           &gSfxDefaultReverb);
}

void GlobalOutdoorRain_Update(PlayState* play) {
    if (play == nullptr) {
        GlobalOutdoorRain_Reset();
        return;
    }

    const GlobalOutdoorRainState state = {
        .enabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRain"), 0) != 0,
        .outdoors = play->envCtx.indoors == 0,
        .ownsRain = sOwnsRain,
        .rainAlreadyActive = play->envCtx.unk_EE[0] != 0,
    };

    switch (GlobalOutdoorRain_Select(state)) {
        case GlobalOutdoorRainDecision::Start:
            play->envCtx.unk_EE[0] = GlobalOutdoorRain_ClampDensity(kRainDensity);
            sOwnsRain = true;
            PlayRainLoop();
            break;
        case GlobalOutdoorRainDecision::Maintain:
            PlayRainLoop();
            break;
        case GlobalOutdoorRainDecision::Stop:
            if (play->envCtx.unk_EE[0] == kRainDensity) {
                play->envCtx.unk_EE[0] = 0;
            }
            sOwnsRain = false;
            break;
        case GlobalOutdoorRainDecision::NoChange:
            break;
    }
}

void GlobalOutdoorRain_Reset() {
    sOwnsRain = false;
}

static void RegisterGlobalOutdoorRain() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        GlobalOutdoorRain_Update(gPlayState);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        GlobalOutdoorRain_Reset();
    });
}

static RegisterShipInitFunc initFunc(RegisterGlobalOutdoorRain, { CVAR_AUDIO("GlobalOutdoorRain") });
#endif
