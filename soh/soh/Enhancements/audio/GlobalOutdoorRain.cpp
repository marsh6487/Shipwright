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

static float GlobalOutdoorRain_ClampIntensity(float intensity) {
    if (intensity < 0.0f) {
        return 0.0f;
    }
    return intensity > 1.0f ? 1.0f : intensity;
}

void GlobalOutdoorRain_AdvanceCycle(GlobalOutdoorRainCycle& cycle, GlobalOutdoorRainMode mode, bool active,
                                    int dryFrames, int sustainFrames, float fadeStep) {
    fadeStep = GlobalOutdoorRain_ClampIntensity(fadeStep);

    if (!active) {
        if (cycle.phase != GlobalOutdoorRainPhase::Dry && cycle.phase != GlobalOutdoorRainPhase::FadeOut) {
            cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            return;
        }
    } else if (mode == GlobalOutdoorRainMode::Persistent && cycle.phase == GlobalOutdoorRainPhase::Dry) {
        cycle.phase = GlobalOutdoorRainPhase::FadeIn;
        return;
    }

    switch (cycle.phase) {
        case GlobalOutdoorRainPhase::Dry:
            cycle.intensity = 0.0f;
            if (active && mode == GlobalOutdoorRainMode::Intermittent && --cycle.framesRemaining <= 0) {
                cycle.phase = GlobalOutdoorRainPhase::FadeIn;
            }
            break;
        case GlobalOutdoorRainPhase::FadeIn:
            if (!active) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
                break;
            }
            cycle.intensity = GlobalOutdoorRain_ClampIntensity(cycle.intensity + fadeStep);
            if (cycle.intensity >= 1.0f) {
                cycle.phase = GlobalOutdoorRainPhase::Sustain;
                cycle.framesRemaining = sustainFrames;
            }
            break;
        case GlobalOutdoorRainPhase::Sustain:
            cycle.intensity = 1.0f;
            if (!active) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            } else if (mode == GlobalOutdoorRainMode::Intermittent && --cycle.framesRemaining <= 0) {
                cycle.phase = GlobalOutdoorRainPhase::FadeOut;
            }
            break;
        case GlobalOutdoorRainPhase::FadeOut:
            cycle.intensity = GlobalOutdoorRain_ClampIntensity(cycle.intensity - fadeStep);
            if (cycle.intensity <= 0.0f) {
                cycle.phase = GlobalOutdoorRainPhase::Dry;
                cycle.framesRemaining = dryFrames;
            }
            break;
    }
}

int GlobalOutdoorRain_ScaleDensity(int density, float intensity) {
    return GlobalOutdoorRain_ClampDensity(static_cast<int>(density * GlobalOutdoorRain_ClampIntensity(intensity) +
                                                           0.5f));
}

float GlobalOutdoorRain_ScaleVolume(float volume, float intensity) {
    return volume * GlobalOutdoorRain_ClampIntensity(intensity);
}

GlobalOutdoorRainColor GlobalOutdoorRain_SelectColor(bool ownsRain, GlobalOutdoorRainColor vanillaColor,
                                                     GlobalOutdoorRainColor configuredColor) {
    return ownsRain ? configuredColor : vanillaColor;
}

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "GlobalOutdoorRainBridge.h"
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
static constexpr int kFramesPerSecond = 60;
static constexpr float kFadeStep = 1.0f / kFramesPerSecond;
static bool sOwnsRain = false;
static int sLastMode = -1;
static GlobalOutdoorRainCycle sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };

extern "C" int32_t GlobalOutdoorRain_GetRenderColor(uint8_t* red, uint8_t* green, uint8_t* blue) {
    if (!sOwnsRain || red == nullptr || green == nullptr || blue == nullptr) {
        return false;
    }

    const Color_RGB8 configuredColor =
        CVarGetColor24(CVAR_AUDIO("GlobalOutdoorRainColor.Value"), { 150, 255, 255 });
    *red = configuredColor.r;
    *green = configuredColor.g;
    *blue = configuredColor.b;
    return true;
}

static int RandomDryFrames() {
    return Rand_S16Offset(10 * kFramesPerSecond, 20 * kFramesPerSecond);
}

static int RandomSustainFrames() {
    return Rand_S16Offset(15 * kFramesPerSecond, 30 * kFramesPerSecond);
}

static void PlayRainLoop(float intensity) {
    if (Audio_IsNatureRainEnabled()) {
        return;
    }
    static float rainVolume = 0.5f;
    const float peakVolume = ConcurrentWeatherAudio_ClampPercent(
                                 CVarGetInteger(CVAR_AUDIO("ProximityWeatherRainVolume"), 50)) /
                             100.0f;
    rainVolume = GlobalOutdoorRain_ScaleVolume(peakVolume, intensity);
    Audio_PlaySoundGeneral(NA_SE_EV_RAIN - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &rainVolume,
                           &gSfxDefaultReverb);
}

void GlobalOutdoorRain_Update(PlayState* play) {
    if (play == nullptr) {
        GlobalOutdoorRain_Reset();
        return;
    }

    const bool enabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRain"), 0) != 0;
    const bool outdoors = play->envCtx.indoors == 0;
    const int modeValue = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRainMode"), 0);
    const GlobalOutdoorRainMode mode = modeValue == static_cast<int>(GlobalOutdoorRainMode::Intermittent)
                                           ? GlobalOutdoorRainMode::Intermittent
                                           : GlobalOutdoorRainMode::Persistent;
    const GlobalOutdoorRainState state = {
        .enabled = enabled,
        .outdoors = outdoors,
        .ownsRain = sOwnsRain,
        .rainAlreadyActive = play->envCtx.unk_EE[0] != 0,
    };

    const GlobalOutdoorRainDecision decision = GlobalOutdoorRain_Select(state);
    if (modeValue != sLastMode) {
        sLastMode = modeValue;
        sCycle = { GlobalOutdoorRainPhase::Dry,
                   mode == GlobalOutdoorRainMode::Intermittent ? RandomDryFrames() : 0, 0.0f };
        if (sOwnsRain && play->envCtx.unk_EE[0] <= kRainDensity) {
            play->envCtx.unk_EE[0] = 0;
        }
    }

    switch (decision) {
        case GlobalOutdoorRainDecision::Start:
            sOwnsRain = true;
            break;
        case GlobalOutdoorRainDecision::Maintain:
            break;
        case GlobalOutdoorRainDecision::Stop:
            break;
        case GlobalOutdoorRainDecision::NoChange:
            break;
    }

    if (!sOwnsRain) {
        return;
    }

    const GlobalOutdoorRainPhase previousPhase = sCycle.phase;
    GlobalOutdoorRain_AdvanceCycle(sCycle, mode, enabled && outdoors, 10 * kFramesPerSecond,
                                   15 * kFramesPerSecond, kFadeStep);
    if (mode == GlobalOutdoorRainMode::Intermittent && previousPhase != sCycle.phase) {
        if (sCycle.phase == GlobalOutdoorRainPhase::Dry) {
            sCycle.framesRemaining = RandomDryFrames();
        } else if (sCycle.phase == GlobalOutdoorRainPhase::Sustain) {
            sCycle.framesRemaining = RandomSustainFrames();
        }
    }
    play->envCtx.unk_EE[0] = GlobalOutdoorRain_ScaleDensity(kRainDensity, sCycle.intensity);
    if (sCycle.intensity > 0.0f) {
        PlayRainLoop(sCycle.intensity);
    }
    if ((!enabled || !outdoors) && sCycle.phase == GlobalOutdoorRainPhase::Dry) {
        sOwnsRain = false;
    }
}

void GlobalOutdoorRain_Reset() {
    sOwnsRain = false;
    sLastMode = -1;
    sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
}

static void RegisterGlobalOutdoorRain() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        GlobalOutdoorRain_Update(gPlayState);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        GlobalOutdoorRain_Reset();
    });
}

static RegisterShipInitFunc initFunc(RegisterGlobalOutdoorRain,
                                     { CVAR_AUDIO("GlobalOutdoorRain"), CVAR_AUDIO("GlobalOutdoorRainMode"),
                                       CVAR_AUDIO("GlobalOutdoorRainColor.Value") });
#endif
