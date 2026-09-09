#include "GlobalOutdoorRain.h"

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state) {
    if (state.source == GlobalOutdoorRainSource::NativePlaced) {
        return GlobalOutdoorRainDecision::NoChange;
    }
    if (!state.enabled || !state.outdoors) {
        return state.source == GlobalOutdoorRainSource::EnhancedOutdoor ? GlobalOutdoorRainDecision::Stop
                                                                        : GlobalOutdoorRainDecision::NoChange;
    }
    if (state.source == GlobalOutdoorRainSource::EnhancedOutdoor) {
        return GlobalOutdoorRainDecision::Maintain;
    }
    // Visual rain density can survive a room transition after its audible
    // owner disappears. With no explicit native owner, reacquire the enhanced
    // loop even when the scene still reports rain particles.
    return GlobalOutdoorRainDecision::Start;
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
    return GlobalOutdoorRain_ClampDensity(
        static_cast<int>(density * GlobalOutdoorRain_ClampIntensity(intensity) + 0.5f));
}

float GlobalOutdoorRain_ScaleVolume(float volume, float intensity) {
    return volume * GlobalOutdoorRain_ClampIntensity(intensity);
}

GlobalOutdoorRainOvercastDecision GlobalOutdoorRain_SelectOvercast(const GlobalOutdoorRainOvercastState& state) {
    const bool shouldOwn = state.enabled && state.outdoors && state.compatibleSky && state.enhancedRainActive;
    if (shouldOwn && !state.ownsOvercast) {
        return GlobalOutdoorRainOvercastDecision::Enable;
    }
    if (!shouldOwn && state.ownsOvercast) {
        return GlobalOutdoorRainOvercastDecision::Restore;
    }
    return GlobalOutdoorRainOvercastDecision::NoChange;
}

GlobalOutdoorRainColor GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource source,
                                                     GlobalOutdoorRainColor vanillaColor,
                                                     GlobalOutdoorRainColor configuredColor) {
    return source == GlobalOutdoorRainSource::EnhancedOutdoor ? configuredColor : vanillaColor;
}

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
#include <cstdio>
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "GlobalOutdoorRainBridge.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "code/concurrent_weather_audio.h"
#include "WeatherSamplePlayer.h"

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "sfx.h"
#include "variables.h"
#include "z64.h"
extern PlayState* gPlayState;
}

static constexpr int kRainDensity = 25;
static constexpr int kFramesPerSecond = 60;
static constexpr float kFadeStep = 1.0f / kFramesPerSecond;
static GlobalOutdoorRainSource sRainSource = GlobalOutdoorRainSource::None;
static int sLastMode = -1;
static GlobalOutdoorRainCycle sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
static bool sOwnsFallbackLoop = false;
static bool sOwnsOvercast = false;
static void PlayRainLoop(float intensity);

extern "C" int32_t GlobalOutdoorRain_GetRenderColor(uint8_t* red, uint8_t* green, uint8_t* blue) {
    if (sRainSource != GlobalOutdoorRainSource::EnhancedOutdoor || red == nullptr || green == nullptr ||
        blue == nullptr) {
        return false;
    }

    const Color_RGB8 configuredColor = CVarGetColor24(CVAR_AUDIO("GlobalOutdoorRainColor.Value"), { 150, 255, 255 });
    *red = configuredColor.r;
    *green = configuredColor.g;
    *blue = configuredColor.b;
    return true;
}

extern "C" void GlobalOutdoorRain_NotifyNativeRainActive(int32_t active) {
    if (active) {
        sRainSource = GlobalOutdoorRainSource::NativePlaced;
        // Native nature rain has no per-feature gain. Hand only that channel
        // off to the private loop so placed and enhanced rain share the same
        // live volume curve without occupying ordinary SFX voices.
        if (Audio_IsNatureRainEnabled()) {
            Audio_SetNatureAmbienceChannelIO(NATURE_CHANNEL_RAIN, CHANNEL_IO_PORT_1, 0);
        }
        PlayRainLoop(1.0f);
    } else if (sRainSource == GlobalOutdoorRainSource::NativePlaced) {
        PlayRainLoop(0.0f);
        sRainSource = GlobalOutdoorRainSource::None;
    }
}

static void UpdateOvercast(PlayState* play, bool enabled, bool outdoors) {
    const bool compatibleSky = play->skyboxId == SKYBOX_NORMAL_SKY && !play->envCtx.skyboxDisabled &&
                               play->csCtx.state == CS_STATE_IDLE;
    const GlobalOutdoorRainOvercastState state = {
        .enabled = enabled,
        .outdoors = outdoors,
        .compatibleSky = compatibleSky,
        .enhancedRainActive = sRainSource == GlobalOutdoorRainSource::EnhancedOutdoor &&
                              sCycle.phase != GlobalOutdoorRainPhase::Dry,
        .ownsOvercast = sOwnsOvercast,
    };

    switch (GlobalOutdoorRain_SelectOvercast(state)) {
        case GlobalOutdoorRainOvercastDecision::Enable:
            if (play->envCtx.gloomySkyMode == 0) {
                play->envCtx.gloomySkyMode = 1;
                sOwnsOvercast = true;
            }
            break;
        case GlobalOutdoorRainOvercastDecision::Restore:
            if (play->envCtx.gloomySkyMode == 1) {
                play->envCtx.gloomySkyMode = 2;
            }
            sOwnsOvercast = false;
            break;
        case GlobalOutdoorRainOvercastDecision::NoChange:
            break;
    }
}

static int RandomDryFrames() {
    return Rand_S16Offset(10 * kFramesPerSecond, 20 * kFramesPerSecond);
}

static int RandomSustainFrames() {
    return Rand_S16Offset(15 * kFramesPerSecond, 30 * kFramesPerSecond);
}

static void PlayRainLoop(float intensity) {
    const float peakVolume =
        ConcurrentWeatherAudio_ClampPercent(CVarGetInteger(CVAR_AUDIO("ProximityWeatherRainVolume"), 50)) / 100.0f;
    const float rainVolume = GlobalOutdoorRain_ScaleVolume(peakVolume, intensity);
    switch (ConcurrentWeatherAudio_SelectRainAction(sOwnsFallbackLoop, sRainSource != GlobalOutdoorRainSource::None,
                                                    Audio_IsNatureRainEnabled(), rainVolume)) {
        case CONCURRENT_WEATHER_RAIN_SET_LOOP:
            WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", rainVolume);
            sOwnsFallbackLoop = true;
            break;
        case CONCURRENT_WEATHER_RAIN_STOP_LOOP:
            WeatherSamplePlayer_SetLoop(nullptr, 0.0f);
            sOwnsFallbackLoop = false;
            break;
        case CONCURRENT_WEATHER_RAIN_NO_CHANGE:
            break;
    }
}

void GlobalOutdoorRain_Update(PlayState* play) {
    if (play == nullptr) {
        GlobalOutdoorRain_Reset();
        return;
    }

    const bool enabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRain"), 0) != 0;
    const bool outdoors = play->envCtx.indoors == 0;
    const bool overcastEnabled = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRainOvercast"), 1) != 0;
    if (CVarGetInteger(CVAR_AUDIO("WeatherAudioDiagnostics"), 0)) {
        static int16_t previousScene = -1;
        static int8_t previousRoom = -1;
        static int8_t previousIndoors = -1;
        if (previousScene != play->sceneNum || previousRoom != play->roomCtx.curRoom.num ||
            previousIndoors != play->envCtx.indoors) {
            std::fprintf(stderr, "[weather-audio] location scene=%d room=%d indoors=%d source=%d density=%d\n",
                         play->sceneNum, play->roomCtx.curRoom.num, play->envCtx.indoors,
                         static_cast<int>(sRainSource), play->envCtx.unk_EE[0]);
            previousScene = play->sceneNum;
            previousRoom = play->roomCtx.curRoom.num;
            previousIndoors = play->envCtx.indoors;
        }
    }
    const int modeValue = CVarGetInteger(CVAR_AUDIO("GlobalOutdoorRainMode"), 0);
    const GlobalOutdoorRainMode mode = modeValue == static_cast<int>(GlobalOutdoorRainMode::Intermittent)
                                           ? GlobalOutdoorRainMode::Intermittent
                                           : GlobalOutdoorRainMode::Persistent;
    const GlobalOutdoorRainState state = {
        .enabled = enabled,
        .outdoors = outdoors,
        .source = sRainSource,
        .rainAlreadyActive = play->envCtx.unk_EE[0] != 0,
    };

    const GlobalOutdoorRainDecision decision = GlobalOutdoorRain_Select(state);
    if (modeValue != sLastMode) {
        sLastMode = modeValue;
        sCycle = { GlobalOutdoorRainPhase::Dry, mode == GlobalOutdoorRainMode::Intermittent ? RandomDryFrames() : 0,
                   0.0f };
        if (sRainSource == GlobalOutdoorRainSource::EnhancedOutdoor && play->envCtx.unk_EE[0] <= kRainDensity) {
            play->envCtx.unk_EE[0] = 0;
        }
    }

    switch (decision) {
        case GlobalOutdoorRainDecision::Start:
            sRainSource = GlobalOutdoorRainSource::EnhancedOutdoor;
            break;
        case GlobalOutdoorRainDecision::Maintain:
            break;
        case GlobalOutdoorRainDecision::Stop:
            break;
        case GlobalOutdoorRainDecision::NoChange:
            break;
    }

    if (sRainSource != GlobalOutdoorRainSource::EnhancedOutdoor) {
        UpdateOvercast(play, overcastEnabled, outdoors);
        return;
    }

    const GlobalOutdoorRainPhase previousPhase = sCycle.phase;
    GlobalOutdoorRain_AdvanceCycle(sCycle, mode, enabled && outdoors, 10 * kFramesPerSecond, 15 * kFramesPerSecond,
                                   kFadeStep);
    if (mode == GlobalOutdoorRainMode::Intermittent && previousPhase != sCycle.phase) {
        if (sCycle.phase == GlobalOutdoorRainPhase::Dry) {
            sCycle.framesRemaining = RandomDryFrames();
        } else if (sCycle.phase == GlobalOutdoorRainPhase::Sustain) {
            sCycle.framesRemaining = RandomSustainFrames();
        }
    }
    play->envCtx.unk_EE[0] = GlobalOutdoorRain_ScaleDensity(kRainDensity, sCycle.intensity);
    // Zero intensity is also an audio transition (intermittent dry/mode reset).
    PlayRainLoop(sCycle.intensity);
    UpdateOvercast(play, overcastEnabled, outdoors);
    if ((!enabled || !outdoors) && sCycle.phase == GlobalOutdoorRainPhase::Dry) {
        sRainSource = GlobalOutdoorRainSource::None;
    }
}

void GlobalOutdoorRain_OnPlayDestroy() {
    // Rain is global rather than PlayState-owned. Keep its cycle and decoded
    // loop voice alive across ordinary scene transitions; the next PlayState
    // decides whether to maintain it outdoors or fade it out indoors.
    // Sky state is PlayState-owned, so the incoming scene must claim its own
    // overcast transition even while the rain loop itself remains continuous.
    sOwnsOvercast = false;
}

void GlobalOutdoorRain_Reset() {
    sRainSource = GlobalOutdoorRainSource::None;
    sLastMode = -1;
    sCycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    sOwnsOvercast = false;
    PlayRainLoop(0.0f);
}

static void RegisterGlobalOutdoorRain() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { GlobalOutdoorRain_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>(
        []() { GlobalOutdoorRain_OnPlayDestroy(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { GlobalOutdoorRain_Reset(); });
}

static RegisterShipInitFunc initFunc(RegisterGlobalOutdoorRain,
                                     { CVAR_AUDIO("GlobalOutdoorRain"), CVAR_AUDIO("GlobalOutdoorRainMode"),
                                       CVAR_AUDIO("GlobalOutdoorRainColor.Value"),
                                       CVAR_AUDIO("GlobalOutdoorRainOvercast") });
#endif
