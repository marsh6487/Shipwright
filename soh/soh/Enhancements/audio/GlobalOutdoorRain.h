#pragma once

enum class GlobalOutdoorRainDecision {
    NoChange,
    Start,
    Maintain,
    Stop,
};

struct GlobalOutdoorRainState {
    bool enabled;
    bool outdoors;
    bool ownsRain;
    bool rainAlreadyActive;
};

GlobalOutdoorRainDecision GlobalOutdoorRain_Select(const GlobalOutdoorRainState& state);
int GlobalOutdoorRain_ClampDensity(int density);

#ifndef GLOBAL_OUTDOOR_RAIN_TEST
struct PlayState;
void GlobalOutdoorRain_Update(PlayState* play);
void GlobalOutdoorRain_Reset();
#endif
