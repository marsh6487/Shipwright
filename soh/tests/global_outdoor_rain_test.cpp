#include "test_require.h"

#include "soh/Enhancements/audio/GlobalOutdoorRain.h"

int main() {
    GlobalOutdoorRainState state = {
        .enabled = false,
        .outdoors = true,
        .source = GlobalOutdoorRainSource::None,
        .rainAlreadyActive = false,
    };
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    state.enabled = true;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Start);

    state.source = GlobalOutdoorRainSource::EnhancedOutdoor;
    state.rainAlreadyActive = true;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Maintain);

    state.enabled = false;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Stop);

    state.enabled = true;
    state.outdoors = false;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Stop);

    state.source = GlobalOutdoorRainSource::None;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    state.outdoors = true;
    state.rainAlreadyActive = true;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    // A native weather tag is authoritative even when enhanced rain is otherwise eligible to run.
    state.rainAlreadyActive = false;
    state.source = GlobalOutdoorRainSource::NativePlaced;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    // Disabling enhanced rain must not stop native placed rain.
    state.enabled = false;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    // A room transition leaves the native lifecycle active; source policy has no room-specific input.
    state.enabled = true;
    state.outdoors = true;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);

    // Leaving outdoors stops enhanced ownership, but never native ownership.
    state.outdoors = false;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::NoChange);
    state.source = GlobalOutdoorRainSource::EnhancedOutdoor;
    REQUIRE(GlobalOutdoorRain_Select(state) == GlobalOutdoorRainDecision::Stop);

    REQUIRE(GlobalOutdoorRain_ClampDensity(-1) == 0);
    REQUIRE(GlobalOutdoorRain_ClampDensity(25) == 25);
    REQUIRE(GlobalOutdoorRain_ClampDensity(80) == 64);

    GlobalOutdoorRainCycle cycle = {
        .phase = GlobalOutdoorRainPhase::Dry,
        .framesRemaining = 1,
        .intensity = 0.0f,
    };
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    REQUIRE(cycle.intensity == 0.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    REQUIRE(cycle.intensity == 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::Sustain);
    REQUIRE(cycle.framesRemaining == 2);
    REQUIRE(cycle.intensity == 1.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::FadeOut);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.intensity == 0.5f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Intermittent, true, 3, 2, 0.5f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::Dry);
    REQUIRE(cycle.framesRemaining == 3);
    REQUIRE(cycle.intensity == 0.0f);

    cycle = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, true, 10, 10, 0.25f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::FadeIn);
    for (int i = 0; i < 4; ++i) {
        GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, true, 10, 10, 0.25f);
    }
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::Sustain);
    REQUIRE(cycle.intensity == 1.0f);
    GlobalOutdoorRain_AdvanceCycle(cycle, GlobalOutdoorRainMode::Persistent, false, 10, 10, 0.25f);
    REQUIRE(cycle.phase == GlobalOutdoorRainPhase::FadeOut);

    REQUIRE(GlobalOutdoorRain_ScaleDensity(25, 0.0f) == 0);
    REQUIRE(GlobalOutdoorRain_ScaleDensity(25, 0.5f) == 13);
    REQUIRE(GlobalOutdoorRain_ScaleDensity(25, 1.0f) == 25);
    REQUIRE(GlobalOutdoorRain_ScaleVolume(0.8f, 0.5f) == 0.4f);

    const GlobalOutdoorRainColor vanilla = { 150, 255, 255 };
    const GlobalOutdoorRainColor blue = { 48, 128, 255 };
    GlobalOutdoorRainColor selected = GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource::None, vanilla, blue);
    REQUIRE(selected.red == vanilla.red);
    REQUIRE(selected.green == vanilla.green);
    REQUIRE(selected.blue == vanilla.blue);

    selected = GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource::NativePlaced, vanilla, blue);
    REQUIRE(selected.red == vanilla.red);
    REQUIRE(selected.green == vanilla.green);
    REQUIRE(selected.blue == vanilla.blue);

    selected = GlobalOutdoorRain_SelectColor(GlobalOutdoorRainSource::EnhancedOutdoor, vanilla, blue);
    REQUIRE(selected.red == blue.red);
    REQUIRE(selected.green == blue.green);
    REQUIRE(selected.blue == blue.blue);
    return 0;
}
