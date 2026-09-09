#pragma once

#include <cstdint>

#include "HyruleFieldNightMusic.h"

using HyruleFieldNightMusicSequenceValidator = bool (*)(uint16_t);
using HyruleFieldNightMusicSequenceResolver = uint16_t (*)(uint16_t);

uint16_t HyruleFieldNightMusic_StartSequence(uint16_t selected, uint16_t fallback,
                                             HyruleFieldNightMusicSequenceValidator isValid,
                                             HyruleFieldNightMusicSequenceResolver getReplacement);
uint16_t HyruleFieldNightMusic_RestoreSequence();
void HyruleFieldNightMusic_ClearSequence();
bool HyruleFieldNightMusic_ShouldRestoreDaySequence(const HyruleFieldNightMusicState& state);
bool HyruleFieldNightMusic_ShouldStopDaySequence(const HyruleFieldNightMusicState& state);
