#include <cassert>

#include "soh/Enhancements/audio/HyruleFieldNightMusic.h"

static HyruleFieldNightMusicState BaseState() {
    return {
        .enabled = true,
        .inHyruleField = true,
        .isNight = true,
        .ownsNightBgm = false,
        .nightBgmPlaying = false,
        .nightAmbienceReady = false,
        .explicitAudioOverride = false,
    };
}

int main() {
    auto state = BaseState();
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::WaitForNightAmbience);
    state.nightAmbienceReady = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);

    state.ownsNightBgm = true;
    state.nightBgmPlaying = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.nightBgmPlaying = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);
    state.nightBgmPlaying = true;

    state.isNight = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNight);

    // A second complete cycle must make the same transitions instead of
    // leaving the main player silent after the first dawn.
    state.ownsNightBgm = false;
    state.nightBgmPlaying = false;
    state.isNight = true;
    state.nightAmbienceReady = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::WaitForNightAmbience);
    state.nightAmbienceReady = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);
    state.ownsNightBgm = true;
    state.nightBgmPlaying = true;
    state.isNight = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNight);

    state = BaseState();
    state.inHyruleField = false;
    state.ownsNightBgm = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNight);

    state = BaseState();
    state.enabled = false;
    state.ownsNightBgm = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNight);

    state = BaseState();
    state.explicitAudioOverride = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.ownsNightBgm = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.isNight = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    assert(HyruleFieldNightMusic_ValidateSequence(0x02, true, 0x21) == 0x02);
    assert(HyruleFieldNightMusic_ValidateSequence(0x7FFF, false, 0x21) == 0x21);
    assert(HyruleFieldNightMusic_ResolveSequence(0x45) == 0x45);
    return 0;
}
