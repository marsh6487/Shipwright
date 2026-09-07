#include "test_require.h"
#include <cstdint>

#include "../soh/Enhancements/audio/WeatherSamplePlayer.h"
#include "weather_audio_fixture.h"

static int32_t sSfxVolume = 100;
extern "C" SoundFontSample* ResourceMgr_LoadAudioSample(const char* path) {
    return WeatherAudioFixture(path);
}
extern "C" int32_t CVarGetInteger(const char*, int32_t) {
    return sSfxVolume;
}

static void TestLoopAndOneShotsPreserveEngineMix() {
    WeatherSamplePlayer_Init();
    WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", 1.0f);
    const bool thunderStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
    const bool lightningStarted = WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f);
    const bool excessStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
    REQUIRE(thunderStarted);
    REQUIRE(lightningStarted);
    REQUIRE(!excessStarted);
    // Arbitrary incoming stereo PCM checks additive mixing only. Real engine
    // SFX eligibility is covered by global_outdoor_rain_audio_test.
    int16_t mixed[] = { 10, -10, 20, -20, 30, -30, 40, -40, 50, -50, 60, -60 };
    WeatherSamplePlayer_Mix(mixed, 6);
    const int16_t expected[] = { 16, -4, 27, -13, 38, -22, 49, -31, 60, -40, 71, -49 };
    for (size_t i = 0; i < 12; ++i)
        REQUIRE(mixed[i] == expected[i]);

    WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", 0.5f);
    int16_t next[] = { 100, -100 };
    WeatherSamplePlayer_Mix(next, 1);
    REQUIRE(next[0] == 108 && next[1] == -92); // rain position 6: 7 * .5 -> 3; shots: 2 + 3

    WeatherSamplePlayer_SetLoop(nullptr, 0.0f);
    int16_t afterStop[] = { 100, -100 };
    WeatherSamplePlayer_Mix(afterStop, 1);
    REQUIRE(afterStop[0] == 105 && afterStop[1] == -95);
    WeatherSamplePlayer_SetLoop("missing", 1.0f);
    sSfxVolume = 0;
    WeatherSamplePlayer_Mix(afterStop, 1);
    REQUIRE(afterStop[0] == 105 && afterStop[1] == -95);
    sSfxVolume = 100;
    int16_t tail[14] = {};
    WeatherSamplePlayer_Mix(tail, 7);
    for (int16_t sample : tail)
        REQUIRE(sample == 5);
    int16_t silent[2] = {};
    WeatherSamplePlayer_Mix(silent, 1);
    REQUIRE(silent[0] == 0 && silent[1] == 0);

    WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", 1.0f);
    int16_t wrapped[36] = {};
    WeatherSamplePlayer_Mix(wrapped, 18);
    REQUIRE(wrapped[30] == 2 && wrapped[32] == 1 && wrapped[34] == 2);
    WeatherSamplePlayer_Reset();
}

int main() {
    TestLoopAndOneShotsPreserveEngineMix();
    REQUIRE(WeatherSamplePlayer_ClampGain(-1.0f) == 0.0f);
    REQUIRE(WeatherSamplePlayer_ClampGain(0.35f) == 0.35f);
    REQUIRE(WeatherSamplePlayer_ClampGain(2.0f) == 1.0f);

    int16_t destination[] = { 32000, -32000, 100, -100 };
    const int16_t source[] = { 2000, -2000 };
    WeatherSamplePlayer_TestMixMono(destination, source, 2, 1.0f);
    REQUIRE(destination[0] == 32767);
    REQUIRE(destination[1] == -30000);
    REQUIRE(destination[2] == -1900);
    REQUIRE(destination[3] == -2100);

    size_t position = 3;
    REQUIRE(WeatherSamplePlayer_AdvanceLoopPosition(position, 4, 1) == 0);
    REQUIRE(WeatherSamplePlayer_AdvanceLoopPosition(position, 4, 6) == 1);
    REQUIRE(WeatherSamplePlayer_AdvanceLoopPosition(position, 0, 6) == 0);
    return 0;
}
