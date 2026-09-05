#pragma once

#include <cstdint>

enum class HyruleFieldNightMusicDecision {
    NoChange,
    StartNight,
    StopNight,
    Release,
};

struct HyruleFieldNightMusicState {
    bool enabled;
    bool inHyruleField;
    bool isNight;
    bool ownsNightBgm;
    bool nightBgmPlaying;
    bool explicitAudioOverride;
};

HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState& state);
uint16_t HyruleFieldNightMusic_ValidateSequence(uint16_t selected, bool isValid, uint16_t fallback);

#ifndef HYRULE_FIELD_NIGHT_MUSIC_TEST
struct PlayState;
void HyruleFieldNightMusic_Update(PlayState* play);
void HyruleFieldNightMusic_Reset();
#endif
