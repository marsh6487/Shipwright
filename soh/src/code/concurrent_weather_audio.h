#ifndef CONCURRENT_WEATHER_AUDIO_H
#define CONCURRENT_WEATHER_AUDIO_H

#include <stdint.h>
#include "sfx.h"

typedef struct {
    uint8_t natureRainEnabled;
    uint8_t natureLightningEnabled;
} ConcurrentWeatherAudioState;

typedef enum {
    CONCURRENT_WEATHER_THUNDER_SFX_OFF = 0,
    CONCURRENT_WEATHER_THUNDER_SFX_CINEMATIC_LIGHTNING,
    CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_LIGHT_ARROW_HIT,
    CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_ATTACK,
    CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_HIT,
    CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_GROUND_THUNDER,
    CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_THUNDER_IMPACT,
    CONCURRENT_WEATHER_THUNDER_SFX_BARINADE_LIGHTNING_ATTACK,
} ConcurrentWeatherThunderSfxPreset;

static inline uint16_t ConcurrentWeatherAudio_ResolveThunderSfx(int32_t preset) {
    switch (preset) {
        case CONCURRENT_WEATHER_THUNDER_SFX_CINEMATIC_LIGHTNING:
            return NA_SE_EV_LIGHTNING;
        case CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_LIGHT_ARROW_HIT:
            return NA_SE_EN_GANON_DD_THUNDER;
        case CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_ATTACK:
            return NA_SE_EN_FANTOM_THUNDER;
        case CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_HIT:
            return NA_SE_EN_FANTOM_HIT_THUNDER;
        case CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_GROUND_THUNDER:
            return NA_SE_EN_FANTOM_THUNDER_GND;
        case CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_THUNDER_IMPACT:
            return NA_SE_EN_GANON_HIT_THUNDER;
        case CONCURRENT_WEATHER_THUNDER_SFX_BARINADE_LIGHTNING_ATTACK:
            return NA_SE_EN_BALINADE_THUNDER;
        case CONCURRENT_WEATHER_THUNDER_SFX_OFF:
        default:
            return 0;
    }
}

static inline void ConcurrentWeatherAudio_TrackNatureChannel(ConcurrentWeatherAudioState* state,
                                                              uint8_t channelRange, uint8_t port, uint8_t value,
                                                              uint8_t rainChannel, uint8_t lightningChannel,
                                                              uint8_t enablePort) {
    uint8_t firstChannel = channelRange >> 4;
    uint8_t lastChannel = channelRange & 0xF;

    if (firstChannel == 0) {
        firstChannel = lastChannel;
    }

    if ((port == enablePort) && (rainChannel >= firstChannel) && (rainChannel <= lastChannel)) {
        state->natureRainEnabled = value != 0;
    }

    if ((port == enablePort) && (lightningChannel >= firstChannel) && (lightningChannel <= lastChannel)) {
        state->natureLightningEnabled = value != 0;
    }
}

static inline int ConcurrentWeatherAudio_ShouldPlayRainSfx(const ConcurrentWeatherAudioState* state,
                                                            uint16_t currentMainBgm,
                                                            uint16_t natureAmbienceSequence) {
    return !state->natureRainEnabled || (currentMainBgm != natureAmbienceSequence);
}

static inline int ConcurrentWeatherAudio_ShouldPlayThunderSfx(const ConcurrentWeatherAudioState* state,
                                                               uint16_t currentMainBgm,
                                                               uint16_t natureAmbienceSequence) {
    return !state->natureLightningEnabled || (currentMainBgm != natureAmbienceSequence);
}

#endif
