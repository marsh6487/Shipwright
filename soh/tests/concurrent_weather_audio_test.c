#include <assert.h>
#include <stdint.h>

#include "../src/code/concurrent_weather_audio.h"

int main(void) {
    ConcurrentWeatherAudioState state = { 0 };

    assert(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0028, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 1, 1, 0x0E, 0x0F, 1);
    assert(!ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));
    assert(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0028, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 1, 0x0E, 0x0F, 1);
    assert(!ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 1, 0, 0x0E, 0x0F, 1);
    assert(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0E, 4, 1, 0x0E, 0x0F, 1);
    assert(ConcurrentWeatherAudio_ShouldPlayRainSfx(&state, 0x0001, 0x0001));

    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 1, 0x0E, 0x0F, 1);
    assert(!ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0001, 0x0001));
    assert(ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0028, 0x0001));
    ConcurrentWeatherAudio_TrackNatureChannel(&state, 0x0F, 1, 0, 0x0E, 0x0F, 1);
    assert(ConcurrentWeatherAudio_ShouldPlayThunderSfx(&state, 0x0001, 0x0001));

    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_OFF) == 0x0000);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_CINEMATIC_LIGHTNING) == 0x282E);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_LIGHT_ARROW_HIT) ==
           0x3827);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_ATTACK) ==
           0x38A2);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_LIGHTNING_HIT) ==
           0x38A8);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_PHANTOM_GANON_GROUND_THUNDER) ==
           0x38AD);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_GANONDORF_THUNDER_IMPACT) ==
           0x390B);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(CONCURRENT_WEATHER_THUNDER_SFX_BARINADE_LIGHTNING_ATTACK) ==
           0x3942);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(-1) == 0x0000);
    assert(ConcurrentWeatherAudio_ResolveThunderSfx(999) == 0x0000);
    return 0;
}
