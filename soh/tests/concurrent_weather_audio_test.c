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
    return 0;
}
