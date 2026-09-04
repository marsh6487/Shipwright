#include <cassert>

#include "soh/Enhancements/audio/GlobalOutdoorRain.h"

int main() {
    GlobalOutdoorRainState state = {
        .enabled = false,
        .outdoors = true,
        .ownsRain = false,
        .rainAlreadyActive = false,
    };
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    state.enabled = true;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Start);

    state.ownsRain = true;
    state.rainAlreadyActive = true;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Maintain);

    state.enabled = false;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Stop);

    state.enabled = true;
    state.outdoors = false;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Stop);

    state.ownsRain = false;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    state.outdoors = true;
    state.rainAlreadyActive = true;
    assert(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    assert(GlobalOutdoorRain_ClampDensity(-1) == 0);
    assert(GlobalOutdoorRain_ClampDensity(25) == 25);
    assert(GlobalOutdoorRain_ClampDensity(80) == 64);
    return 0;
}
