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

    assert(ConcurrentWeatherAudio_ClampPercent(-5) == 0);
    assert(ConcurrentWeatherAudio_ClampPercent(75) == 75);
    assert(ConcurrentWeatherAudio_ClampPercent(125) == 100);
    assert(ConcurrentWeatherAudio_ThunderStyle(-1) == CONCURRENT_WEATHER_THUNDER_LOW);
    assert(ConcurrentWeatherAudio_ThunderStyle(CONCURRENT_WEATHER_THUNDER_LAYERED) ==
           CONCURRENT_WEATHER_THUNDER_LAYERED);
    assert(ConcurrentWeatherAudio_ThunderStyle(999) == CONCURRENT_WEATHER_THUNDER_LOW);
    return 0;
}
