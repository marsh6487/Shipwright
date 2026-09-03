#ifndef CONCURRENT_WEATHER_AUDIO_H
#define CONCURRENT_WEATHER_AUDIO_H

#include <stdint.h>

typedef struct {
    uint8_t natureRainEnabled;
} ConcurrentWeatherAudioState;

static inline void ConcurrentWeatherAudio_TrackNatureChannel(ConcurrentWeatherAudioState* state,
                                                              uint8_t channelRange, uint8_t port, uint8_t value,
                                                              uint8_t rainChannel, uint8_t enablePort) {
    uint8_t firstChannel = channelRange >> 4;
    uint8_t lastChannel = channelRange & 0xF;

    if (firstChannel == 0) {
        firstChannel = lastChannel;
    }

    if ((port == enablePort) && (rainChannel >= firstChannel) && (rainChannel <= lastChannel)) {
        state->natureRainEnabled = value != 0;
    }
}

static inline int ConcurrentWeatherAudio_ShouldPlayRainSfx(const ConcurrentWeatherAudioState* state) {
    return !state->natureRainEnabled;
}

static inline int ConcurrentWeatherAudio_ShouldPlayThunder(uint16_t currentMainBgm,
                                                            uint16_t natureAmbienceSequence) {
    return currentMainBgm != natureAmbienceSequence;
}

#endif
