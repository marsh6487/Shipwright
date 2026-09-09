#include "test_require.h"
#include <cstring>
#include "weather_audio_stubs/weather_runtime.h"
#include "weather_audio_fixture.h"
#include "weather_sfx_engine_fixture.h"
#include "code/concurrent_weather_audio.h"
#include "soh/Enhancements/audio/GlobalOutdoorRain.h"
#include "soh/Enhancements/audio/GlobalOutdoorRainBridge.h"
#include "soh/Enhancements/audio/WeatherSamplePlayer.h"

static int sEnabled = 1;
static int sMode = 0;
static int sRainVolume = 100;
static int sOvercast = 1;
static ConcurrentWeatherAudioState sNatureWeather = {};
static int sNatureRainWrites = 0;
GameInteractor* GameInteractor::Instance = nullptr;
extern "C" {
PlayState* gPlayState = nullptr;
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path) {
    return WeatherAudioFixture(path);
}
int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRain") == 0)
        return sEnabled;
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRainMode") == 0)
        return sMode;
    if (std::strcmp(name, "gAudioEditor.ProximityWeatherRainVolume") == 0)
        return sRainVolume;
    if (std::strcmp(name, "gAudioEditor.GlobalOutdoorRainOvercast") == 0)
        return sOvercast;
    return fallback;
}
Color_RGB8 CVarGetColor24(const char*, Color_RGB8 fallback) {
    return fallback;
}
int16_t Rand_S16Offset(int16_t base, int16_t) {
    return base;
}
// The sequence-player boundary is fixed to native ambience (0x0001).
// Run the real policy used by code_800EC960.c, not a weather eligibility mock.
uint8_t Audio_IsNatureRainEnabled(void) {
    return !ConcurrentWeatherAudio_ShouldPlayRainSfx(&sNatureWeather, 0x0001, 0x0001);
}
void Audio_SetNatureAmbienceChannelIO(uint8_t channel, uint8_t port, uint8_t value) {
    ++sNatureRainWrites;
    ConcurrentWeatherAudio_TrackNatureChannel(&sNatureWeather, channel, port, value, 0x0E, 0x0F, 1);
}
}

static void SetNatureRain(bool enabled) {
    ConcurrentWeatherAudio_TrackNatureChannel(&sNatureWeather, 0x0E, 1, enabled, 0x0E, 0x0F, 1);
}

static void RequireMixFrame(int16_t expected) {
    WeatherSfxEngine_RequirePlaying();
    int16_t buffer[2] = { 100, -100 };
    WeatherSamplePlayer_Mix(buffer, 1);
    REQUIRE(buffer[0] - 100 == buffer[1] + 100);
    REQUIRE(buffer[0] == expected);
    WeatherSfxEngine_Refresh();
}

static void TestSimultaneousEngineAndWeatherVoices() {
    // Catch weather suppressing real requests or consuming/muting ordinary
    // engine voices. Submit AFTER rain and both thunder layers are active.
    for (bool nativeRain : { false, true }) {
        WeatherSamplePlayer_Init();
        GlobalOutdoorRain_Reset();
        WeatherSfxEngine_Reset();
        SetNatureRain(nativeRain);
        PlayState play = {};
        for (int frame = 0; frame < 62; ++frame)
            GlobalOutdoorRain_Update(&play);
        const bool thunderEligible = ConcurrentWeatherAudio_ShouldPlayThunderSfx(&sNatureWeather, 0x0001, 0x0001);
        REQUIRE(thunderEligible); // rain channel alone must not suppress thunder
        const bool thunderStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
        const bool lightningStarted = WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f);
        REQUIRE(thunderStarted);
        REQUIRE(lightningStarted);
        WeatherSfxEngine_Start();
        RequireMixFrame(nativeRain ? 105 : 106);
        SetNatureRain(!nativeRain);
        GlobalOutdoorRain_Update(&play);
        RequireMixFrame(nativeRain ? 106 : 105);
        GlobalOutdoorRain_Reset();
        RequireMixFrame(105);
    }
    SetNatureRain(false);
}

static void TestDenseFlameHubDoesNotStealNaviOrThunder() {
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    SetNatureRain(false);
    PlayState play = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f));
    REQUIRE(WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f));
    WeatherSfxEngine_StartDenseFlameHub();
    RequireMixFrame(106);
}

static void TestPlacedWeatherRainUsesPrivateLoop() {
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_StartDenseFlameHub();
    SetNatureRain(false);

    GlobalOutdoorRain_NotifyNativeRainActive(1);
    RequireMixFrame(101);

    GlobalOutdoorRain_NotifyNativeRainActive(0);
    RequireMixFrame(100);

    // Placed weather relinquishes the unscaled native rain channel and uses
    // the same live slider-controlled private loop as enhanced outdoor rain.
    SetNatureRain(true);
    sNatureRainWrites = 0;
    GlobalOutdoorRain_NotifyNativeRainActive(1);
    REQUIRE(sNatureRainWrites == 1);
    RequireMixFrame(101);
    sRainVolume = 0;
    GlobalOutdoorRain_NotifyNativeRainActive(1);
    RequireMixFrame(100);
    sRainVolume = 100;
    GlobalOutdoorRain_NotifyNativeRainActive(0);
    SetNatureRain(false);
}

int main() {
    TestSimultaneousEngineAndWeatherVoices();
    TestDenseFlameHubDoesNotStealNaviOrThunder();
    TestPlacedWeatherRainUsesPrivateLoop();

    // Overcast follows the enhanced rain lifecycle, stays off fixed skies,
    // and is reclaimed by the incoming PlayState without restarting audio.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
    sOvercast = 1;
    PlayState overcastPlay = {};
    overcastPlay.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_Update(&overcastPlay);
    REQUIRE(overcastPlay.envCtx.gloomySkyMode == 1);
    GlobalOutdoorRain_OnPlayDestroy();
    PlayState incomingOvercastPlay = {};
    incomingOvercastPlay.skyboxId = SKYBOX_NORMAL_SKY;
    GlobalOutdoorRain_Update(&incomingOvercastPlay);
    REQUIRE(incomingOvercastPlay.envCtx.gloomySkyMode == 1);
    sOvercast = 0;
    GlobalOutdoorRain_Update(&incomingOvercastPlay);
    REQUIRE(incomingOvercastPlay.envCtx.gloomySkyMode == 2);
    GlobalOutdoorRain_Reset();
    sOvercast = 1;

    // A normal scene handoff must not restart the authored rain loop. The next
    // outdoor PlayState adopts the existing global voice and cycle.
    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    sEnabled = 1;
    sMode = 0;
    sRainVolume = 100;
    SetNatureRain(false);
    PlayState outgoing = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&outgoing);
    int16_t beforeHandoff[6] = {};
    WeatherSamplePlayer_Mix(beforeHandoff, 3);
    REQUIRE(beforeHandoff[0] == 1 && beforeHandoff[2] == 2 && beforeHandoff[4] == 3);
    GlobalOutdoorRain_OnPlayDestroy();
    PlayState incoming = {};
    GlobalOutdoorRain_Update(&incoming);
    int16_t afterHandoff[2] = {};
    WeatherSamplePlayer_Mix(afterHandoff, 1);
    REQUIRE(afterHandoff[0] == 4);

    WeatherSamplePlayer_Init();
    GlobalOutdoorRain_Reset();
    WeatherSfxEngine_Reset();
    WeatherSfxEngine_Start();
    PlayState play = {};
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    RequireMixFrame(101);

    // Persistent -> intermittent resets to dry immediately, including audio.
    // The old updater skipped zero intensity and left the persistent loop alive.
    sMode = 1;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    RequireMixFrame(100);

    // Exercise a full intermittent fade-out: dry must leave no residual loop.
    for (int frame = 0; frame < 1625; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    RequireMixFrame(100);

    sMode = 0;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    const bool thunderStarted = WeatherSamplePlayer_Play("audio/samples/Low Thunder_META", 1.0f);
    const bool lightningStarted = WeatherSamplePlayer_Play("audio/samples/Lightning_META", 1.0f);
    REQUIRE(thunderStarted);
    REQUIRE(lightningStarted);
    SetNatureRain(true);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(105); // native nature channel takes priority; shots survive
    SetNatureRain(false);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(106);

    GlobalOutdoorRain_NotifyNativeRainActive(1);
    play.envCtx.unk_EE[0] = 40;
    play.envCtx.indoors = 1;
    sEnabled = 0;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 40);
    RequireMixFrame(107);
    uint8_t red = 0, green = 0, blue = 0;
    const bool nativeHasEnhancedColor = GlobalOutdoorRain_GetRenderColor(&red, &green, &blue);
    REQUIRE(!nativeHasEnhancedColor);
    play.envCtx.indoors = 0;
    sEnabled = 1;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 40);
    RequireMixFrame(108);
    GlobalOutdoorRain_NotifyNativeRainActive(0);
    play.envCtx.unk_EE[0] = 0;
    GlobalOutdoorRain_Update(&play);
    const bool enhancedHasColor = GlobalOutdoorRain_GetRenderColor(&red, &green, &blue);
    REQUIRE(enhancedHasColor);
    RequireMixFrame(106);
    GlobalOutdoorRain_Reset();
    RequireMixFrame(105); // teardown stops only the loop, not either one-shot

    // A placed-weather notification adopts the shared rain loop without
    // restarting it, and reset releases that explicitly acquired ownership.
    WeatherSamplePlayer_Reset();
    WeatherSamplePlayer_SetLoop("audio/samples/Rainfall_META", 1.0f);
    sEnabled = 0;
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    GlobalOutdoorRain_NotifyNativeRainActive(1);
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(102);
    GlobalOutdoorRain_Reset();
    GlobalOutdoorRain_Reset();
    RequireMixFrame(100);
    WeatherSamplePlayer_Reset();

    // Volume zero must release owned fallback without changing visual density.
    play = {};
    sEnabled = 1;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    sRainVolume = 0;
    GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 25);
    RequireMixFrame(100);
    sRainVolume = 100;
    GlobalOutdoorRain_Update(&play);
    RequireMixFrame(101);
    sEnabled = 0;
    for (int frame = 0; frame < 62; ++frame)
        GlobalOutdoorRain_Update(&play);
    REQUIRE(play.envCtx.unk_EE[0] == 0);
    RequireMixFrame(100);
}
