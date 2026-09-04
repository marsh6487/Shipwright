#include <cassert>
#include <cstdint>

#include "../soh/Enhancements/audio/WeatherSamplePlayer.h"

int main() {
    assert(WeatherSamplePlayer_ClampGain(-1.0f) == 0.0f);
    assert(WeatherSamplePlayer_ClampGain(0.35f) == 0.35f);
    assert(WeatherSamplePlayer_ClampGain(2.0f) == 1.0f);

    int16_t destination[] = { 32000, -32000, 100, -100 };
    const int16_t source[] = { 2000, -2000 };
    WeatherSamplePlayer_TestMixMono(destination, source, 2, 1.0f);
    assert(destination[0] == 32767);
    assert(destination[1] == -30000);
    assert(destination[2] == -1900);
    assert(destination[3] == -2100);
    return 0;
}
