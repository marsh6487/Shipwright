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

    GlobalOutdoorRainCycle cycle = {
        .phase = GlobalOutdoorRainPhase::Dry,
        .framesRemaining = 1,
        .intensity = 0.0f,
    };
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    assert(cycle.intensity == 0.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    assert(cycle.intensity == 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.phase == GlobalOutdoorRainPhase::Sustain);
    assert(cycle.framesRemaining == 2);
    assert(cycle.intensity == 1.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.phase == GlobalOutdoorRainPhase::FadeOut);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.intensity == 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    assert(cycle.phase == GlobalOutdoorRainPhase::Dry);
    assert(cycle.framesRemaining == 3);
    assert(cycle.intensity == 0.0f);

    cycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, true, 10, 10, 0.25f);
    assert(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    for (int i = 0; i < 4; ++i) {
        GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, true, 10, 10, 0.25f);
    }
    assert(cycle.phase == GlobalOutdoorRainPhase::Sustain);
    assert(cycle.intensity == 1.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, false, 10, 10, 0.25f);
    assert(cycle.phase == GlobalOutdoorRainPhase::FadeOut);

    assert(GlobalOutdoorRain_ScaleDensity(25, 0.0f) == 0);
    assert(GlobalOutdoorRain_ScaleDensity(25, 0.5f) == 13);
    assert(GlobalOutdoorRain_ScaleDensity(25, 1.0f) == 25);
    assert(GlobalOutdoorRain_ScaleVolume(0.8f, 0.5f) == 0.4f);

    const GlobalOutdoorRainColor vanilla = { 150, 255, 255 };
    const GlobalOutdoorRainColor blue = { 48, 128, 255 };
    GlobalOutdoorRainColor selected = GlobalOutdoorRain_SelectColor(false, vanilla, blue);
    assert(selected.red == vanilla.red);
    assert(selected.green == vanilla.green);
    assert(selected.blue == vanilla.blue);

    selected = GlobalOutdoorRain_SelectColor(true, vanilla, blue);
    assert(selected.red == blue.red);
    assert(selected.green == blue.green);
    assert(selected.blue == blue.blue);
    return 0;
}
