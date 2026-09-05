#pragma once

enum class GlobalOutdoorRainDecision {
    NoChange,
    Start,
    Maintain,
    Stop,
};

enum class GlobalOutdoorRainMode {
    Persistent = 0,
    Intermittent = 1,
};

enum class GlobalOutdoorRainPhase {
    Dry,
    FadeIn,
    Sustain,
    FadeOut,
};

struct GlobalOutdoorRainCycle {
    GlobalOutdoorRainPhase phase;
    int framesRemaining;
    float intensity;
};

struct GlobalOutdoorRainState {
    bool enabled;
    bool outdoors;
    bool ownsRain;
    bool rainAlreadyActive;
};

struct GlobalOutdoorRainColor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state);
int GlobalOutdoorRain_ClampDensity(int density);
void GlobalOutdoorRain_AdvanceCycle(GlobalOutdoorRainCycle& cycle, GlobalOutdoorRainMode mode, bool active,
                                    int dryFrames, int sustainFrames, float fadeStep);
int GlobalOutdoorRain_ScaleDensity(int density, float intensity);
float GlobalOutdoorRain_ScaleVolume(float volume, float intensity);
GlobalOutdoorRainColor GlobalOutdoorRain_SelectColor(bool ownsRain, GlobalOutdoorRainColor vanillaColor,
                                                     GlobalOutdoorRainColor configuredColor);

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
struct PlayState;
void GlobalOutdoorRain_Update(PlayState* play);
void GlobalOutdoorRain_Reset();
#endif
