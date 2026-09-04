#include <cassert>

#include "soh/Enhancements/audio/HyruleFieldNightMusic.h"

static HyruleFieldNightMusicState BaseState() {
    return {
        .enabled = true,
        .inHyruleField = true,
        .isNight = true,
        .ownsNightBgm = false,
        .nightBgmPlaying = false,
        .explicitAudioOverride = false,
    };
}

int main() {
    auto state = BaseState();
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);

    state.ownsNightBgm = true;
    state.nightBgmPlaying = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.nightBgmPlaying = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);
    state.nightBgmPlaying = true;

    state.isNight = false;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNight);

    state = BaseState();
    state.inHyruleField = false;
    state.ownsNightBgm = true;
    assert(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::Release);

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
    return 0;
}
