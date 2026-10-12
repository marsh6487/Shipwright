#include "test_require.h"
#include "z64.h"
#include "mods/extended_inventory.h"
#include "soh/Enhancements/audio/GlobalOutdoorRain.h"
#include "soh/Enhancements/audio/GlobalOutdoorRainBridge.h"
#include "soh/Enhancements/audio/SeasonWeather.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>

// Weak declarations let the first run fail on the absent behavior, before the
// new presentation controller is implemented.
extern "C" {
int32_t SeasonWeather_SeasonForPlay(const PlayState*) __attribute__((weak));
int32_t SeasonWeather_ParticlesEnabled(int32_t) __attribute__((weak));
int32_t SeasonWeather_FlurriesEnabled(void) __attribute__((weak));
int32_t SeasonWeather_SummerEffectEnabled(int32_t) __attribute__((weak));
int32_t SeasonWeather_LightOffset(const PlayState*, int32_t, int32_t) __attribute__((weak));
void SeasonWeather_Update(PlayState*) __attribute__((weak));
void SeasonWeather_Reset(void) __attribute__((weak));
void SeasonWeather_TestRodBridge(PlayState*, u8);
void Seasons_PrecipTint(u8*, u8*, u8*);
void func_800766C4(PlayState*);
void func_8006FB94(EnvironmentContext*, u16);
void SeasonWeather_TestBombFade(PlayState*);
void SeasonWeather_TestBombFlash(PlayState*);
void SeasonWeather_TestNativeLighting(PlayState*);
void SeasonWeather_TestNativeSkyBlend(EnvironmentContext*);
void EnWeatherTag_Init(Actor*, PlayState*);
void EnWeatherTag_Destroy(Actor*, PlayState*);
void EnWeatherTag_Update(Actor*, PlayState*);
void EnWeatherTag_DisabledRainLakeHylia(EnWeatherTag*, PlayState*);
void EnWeatherTag_EnabledRainLakeHylia(EnWeatherTag*, PlayState*);
void EnWeatherTag_DisabledCloudyRainThunderKakariko(EnWeatherTag*, PlayState*);
void EnWeatherTag_EnabledCloudyRainThunderKakariko(EnWeatherTag*, PlayState*);
}

GameInteractor* GameInteractor::Instance = nullptr;
extern "C" {
SaveContext gSaveContext = {};
PlayState* gPlayState = nullptr;
LightningStrike gLightningStrike = {};
u8 gWeatherMode = 0, gSkyboxBlendingEnabled = 0, D_8011FB34 = 0, D_8011FB38 = 0;
u16 gTimeSpeed = 0;
static GameInfo sGameInfo = {};
GameInfo* gGameInfo = &sGameInfo;
u32 gBitFlags[32];
// Platform and unrelated sandstorm/debug IO boundaries. The tested rain and
// cloud actor functions, range checks and event flags are production code.
f32 Actor_WorldDistXZToActor(Actor* a, Actor* b) {
    return std::hypot(a->world.pos.x - b->world.pos.x, a->world.pos.z - b->world.pos.z);
}
void Actor_Kill(Actor* actor) {
    actor->update = nullptr;
}
s16 Math_SmoothStepToS(s16*, s16, s16, s16, s16) {
    return 0;
}
DebugDispObject* DebugDisplay_AddObject(f32, f32, f32, s16, s16, s16, f32, f32, f32, u8, u8, u8, u8, s16,
                                        GraphicsContext*) {
    return nullptr;
}
void lusprintf(const char*, int32_t, int32_t, const char*, ...) {
}
static NeiSaveData sSave;
NeiSaveData* Nei_Save(void) {
    return &sSave;
}
static std::map<std::string, int32_t> sSettings;
static float sLoopGain;
static int sNatureWrites;
static int sNativeRandomCalls;
int32_t CVarGetInteger(const char* key, int32_t fallback) {
    auto found = sSettings.find(key);
    return found == sSettings.end() ? fallback : found->second;
}
Color_RGB8 CVarGetColor24(const char*, Color_RGB8 fallback) {
    return fallback;
}
s16 Rand_S16Offset(s16 base, s16) {
    ++sNativeRandomCalls;
    return base;
}
u8 Audio_IsNatureRainEnabled(void) {
    return 0;
}
void Audio_SetNatureAmbienceChannelIO(u8, u8, u8) {
    ++sNatureWrites;
}
void WeatherSamplePlayer_SetLoop(const char* path, float gain) {
    sLoopGain = path ? gain : 0.0f;
}
void GlobalOutdoorRain_Log(const char*) {
}
}

struct Fixture {
    PlayState play = {};
    Camera camera = {};
    Fixture() {
        GlobalOutdoorRain_Reset();
        if (SeasonWeather_Reset)
            SeasonWeather_Reset();
        sSettings.clear();
        sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 0;
        sSave = {};
        gSaveContext = {};
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gLightningStrike = {};
        gWeatherMode = gSkyboxBlendingEnabled = D_8011FB34 = D_8011FB38 = 0;
        gTimeSpeed = 0;
        sGameInfo = {};
        play.skyboxId = SKYBOX_NORMAL_SKY;
        play.cameraPtrs[0] = &camera;
        play.activeCamera = 0;
        gPlayState = &play;
        GlobalOutdoorRain_BeginScene(&play, 0, 0, 0);
        sLoopGain = 0;
        sNatureWrites = 0;
        sNativeRandomCalls = 0;
    }
    ~Fixture() {
        SeasonWeather_Reset();
        GlobalOutdoorRain_Reset();
        gPlayState = nullptr;
    }
    void Frame() {
        ++play.state.frames;
        SeasonWeather_Update(&play);
        GlobalOutdoorRain_Update(&play);
    }
    void Frames(int frames) {
        for (int i = 0; i < frames; ++i)
            Frame();
    }
    void Select(int season) {
        sSettings["gAudioEditor.SeasonWeather.Season"] = season;
    }
};

struct NativeSkyFixture : Fixture {
    Player player = {};
    EnWeatherTag tag = {};
    int type;
    explicit NativeSkyFixture(int tagType) : type(tagType) {
        play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
        player.actor.world.pos.x = 2000.0f;
        // Actual story flags that retain Kakariko's native thunderstorm actor.
        for (int flag : { EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP, EVENTCHKINF_USED_FIRE_TEMPLE_BLUE_WARP,
                          EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP }) {
            if (type == EN_WEATHER_TAG_TYPE_THUNDERSTORM_KAKARIKO) {
                gSaveContext.eventChkInf[flag >> 4] |= 1 << (flag & 0xF);
            }
        }
    }
    void AttachTag() {
        tag.actor.id = ACTOR_EN_WEATHER_TAG;
        tag.actor.category = ACTORCAT_PROP;
        tag.actor.params = 0x0A00 | type; // native 1000-unit range, plus 100-unit restore margin
        tag.actor.update = EnWeatherTag_Update;
        play.actorCtx.actorLists[ACTORCAT_PROP].head = &tag.actor;
        EnWeatherTag_Init(&tag.actor, &play);
        REQUIRE(tag.actor.update != nullptr);
    }
    void NativeFrame() {
        ++play.state.frames;
        if (tag.actor.update) {
            EnWeatherTag_Update(&tag.actor, &play);
        }
        SeasonWeather_Update(&play);
        GlobalOutdoorRain_Update(&play);
        func_8006FB94(&play.envCtx, 0);
        SeasonWeather_TestNativeSkyBlend(&play.envCtx);
        func_800766C4(&play);
    }
    void NativeFrames(int frames) {
        for (int i = 0; i < frames; ++i)
            NativeFrame();
    }
    EnWeatherTagActionFunc Disabled() const {
        return type == EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA ? EnWeatherTag_DisabledRainLakeHylia
                                                           : EnWeatherTag_DisabledCloudyRainThunderKakariko;
    }
    EnWeatherTagActionFunc Enabled() const {
        return type == EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA ? EnWeatherTag_EnabledRainLakeHylia
                                                           : EnWeatherTag_EnabledCloudyRainThunderKakariko;
    }
};

static void NativeSkyTagsKeepTheirRangeActivationAndRestoration() {
    for (int type : { EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA, EN_WEATHER_TAG_TYPE_THUNDERSTORM_KAKARIKO }) {
        for (int season : { SEASON_SPRING, SEASON_AUTUMN, SEASON_WINTER }) {
            for (int globalRain : { 0, 1 }) {
                NativeSkyFixture f(type);
                f.AttachTag();
                f.Select(season);
                sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 1;
                sSettings["gAudioEditor.GlobalOutdoorRain"] = globalRain;
                sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 1;
                const EnWeatherTag before = f.tag;
                f.Frame(); // the controller must not mutate or retire native weather tags
                REQUIRE(std::memcmp(&before, &f.tag, sizeof(f.tag)) == 0);
                f.NativeFrames(150);
                REQUIRE(f.play.envCtx.gloomySkyMode == 0);
                REQUIRE(f.tag.actionFunc == f.Disabled());
                f.player.actor.world.pos.x = 0;
                f.NativeFrames(150);
                REQUIRE(f.tag.actionFunc == f.Enabled());
                REQUIRE(f.play.envCtx.unk_EE[0] == (type == EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA ? 25 : 30));
                REQUIRE(gWeatherMode == (type == EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA ? 4 : 5));
                REQUIRE(f.play.envCtx.gloomySkyMode == 0);
                f.player.actor.world.pos.x = 2000;
                f.NativeFrames(150);
                REQUIRE(f.tag.actionFunc == f.Disabled());
                REQUIRE(gWeatherMode == 0);
                REQUIRE(f.play.envCtx.unk_EE[0] == (globalRain ? 25 : season == SEASON_WINTER ? 0 : 30));
                REQUIRE((f.play.envFlags[0] & (1 << 5)) == 0); // presentation cannot grow gameplay beans
            }
        }
    }
}

static void LateNativeTagsReceiveQueuedAndAppliedSeasonalSky() {
    for (int applied : { 0, 1 }) {
        NativeSkyFixture f(EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA);
        f.Select(SEASON_WINTER);
        f.Frame();
        REQUIRE(f.play.envCtx.gloomySkyMode == 1);
        if (applied) {
            func_8006FB94(&f.play.envCtx, 0);
            for (int i = 0; i < 100; ++i)
                SeasonWeather_TestNativeSkyBlend(&f.play.envCtx);
            REQUIRE(f.play.envCtx.unk_DE == 1);
            // Native light row one is also an activation gate. The actual
            // restore path must retire it instead of merely clearing mode one.
            f.play.envCtx.unk_1F = 1;
        }
        f.AttachTag();
        f.player.actor.world.pos.x = 0;
        if (applied) {
            gSkyboxBlendingEnabled = 1;
            f.NativeFrames(20);
            REQUIRE(f.play.envCtx.gloomySkyMode == 2 && f.play.envCtx.unk_DE == 1);
            REQUIRE(f.tag.actionFunc == f.Disabled());
            gSkyboxBlendingEnabled = 0;
        }
        f.NativeFrames(160);
        REQUIRE(f.play.envCtx.gloomySkyMode == 0);
        REQUIRE(f.play.envCtx.unk_1F != 1);
        REQUIRE(f.tag.actionFunc == f.Enabled());
        REQUIRE(f.play.envCtx.unk_EE[0] == 25);
        f.player.actor.world.pos.x = 2000;
        f.NativeFrames(150);
        REQUIRE(f.tag.actionFunc == f.Disabled());
        REQUIRE(f.play.envCtx.unk_EE[0] == 0);
    }
}

static void NativeTagsDoNotDisableTheUnmodifiedNativePresentation() {
    NativeSkyFixture f(EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA);
    f.AttachTag();
    f.Select(-1);
    sSettings["gAudioEditor.GlobalOutdoorRain"] = 1;
    sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 1;
    f.Frames(50);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1); // the independent baseline override still owns this setting
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
}

static void RetiredNativeTagsDoNotOverrideSavedRodWeather() {
    NativeSkyFixture f(EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA);
    f.AttachTag();
    Actor_Kill(&f.tag.actor); // the preserved SeasonScene retirement leaves this actor inactive
    sSave.seasonsOwned = 1 << SEASON_WINTER;
    sSave.season = SEASON_WINTER;
    f.Select(-2);
    f.Frame();
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == SEASON_WINTER);
    REQUIRE(f.tag.actor.update == nullptr);
}

static void SeasonalSkyYieldPreservesScriptOwnershipAndThenNativeTags() {
    NativeSkyFixture f(EN_WEATHER_TAG_TYPE_RAIN_LAKE_HYLIA);
    f.Select(SEASON_WINTER);
    f.Frame();
    func_8006FB94(&f.play.envCtx, 0);
    for (int i = 0; i < 100; ++i)
        SeasonWeather_TestNativeSkyBlend(&f.play.envCtx);
    f.AttachTag();
    f.player.actor.world.pos.x = 0;
    f.play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&f.play, 20);
    f.NativeFrames(10);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
    REQUIRE(f.play.envCtx.unk_EE[0] == 20);
    // The story's own clear command queues native restoration. Seasonal
    // presentation must keep yielding while that original handshake finishes.
    GlobalOutdoorRain_SetScriptedRain(&f.play, 0);
    f.play.envCtx.gloomySkyMode = 2;
    f.play.csCtx.state = CS_STATE_IDLE;
    f.NativeFrames(160);
    REQUIRE(f.play.envCtx.gloomySkyMode == 0);
    REQUIRE(f.tag.actionFunc == f.Enabled());
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
}

static void ManualPresentationDoesNotChangeSavedRodState() {
    Fixture f;
    sSave.season = SEASON_WINTER; // deliberately unowned: the gameplay getter would self-heal it
    sSave.extEquipOwnedBits = 0x13579;
    const NeiSaveData before = sSave;
    for (int season : { -1, SEASON_SPRING, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER }) {
        f.Select(season);
        REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == season);
        f.Frame();
        REQUIRE(std::memcmp(&before, &sSave, sizeof(sSave)) == 0);
    }
    f.Select(-2);
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    REQUIRE(std::memcmp(&before, &sSave, sizeof(sSave)) == 0);
    sSave.seasonsOwned = 1 << SEASON_AUTUMN;
    sSave.season = SEASON_AUTUMN;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == SEASON_AUTUMN);
    sSave.season = SEASON_OFF;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.Select(999);
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
}

static void ParticleSwitchesAreIndependentAndPreserveNativeSlots() {
    Fixture f;
    REQUIRE(SeasonWeather_ParticlesEnabled(SEASON_SPRING));
    REQUIRE(SeasonWeather_ParticlesEnabled(SEASON_AUTUMN));
    REQUIRE(SeasonWeather_ParticlesEnabled(SEASON_WINTER));
    REQUIRE(!SeasonWeather_ParticlesEnabled(SEASON_SUMMER));
    REQUIRE(!SeasonWeather_ParticlesEnabled(-1));
    REQUIRE(!SeasonWeather_FlurriesEnabled());
    sSettings["gAudioEditor.SeasonWeather.Winter.Flurries"] = 1;
    REQUIRE(SeasonWeather_FlurriesEnabled());
    sSettings["gAudioEditor.SeasonWeather.Winter.Snow"] = 0;
    REQUIRE(!SeasonWeather_FlurriesEnabled());
    sSettings["gAudioEditor.SeasonWeather.Spring.Blossoms"] = 0;
    REQUIRE(!SeasonWeather_ParticlesEnabled(SEASON_SPRING));
    REQUIRE(SeasonWeather_ParticlesEnabled(SEASON_AUTUMN));
    REQUIRE(SeasonWeather_SummerEffectEnabled(0));
    REQUIRE(SeasonWeather_SummerEffectEnabled(1));
    REQUIRE(!SeasonWeather_SummerEffectEnabled(2));
    REQUIRE(!SeasonWeather_SummerEffectEnabled(3));
    sSettings["gAudioEditor.SeasonWeather.Summer.Enabled"] = 0;
    REQUIRE(!SeasonWeather_SummerEffectEnabled(0));
    REQUIRE(!SeasonWeather_SummerEffectEnabled(1));
    f.play.envCtx.unk_EE[2] = 7;
    f.play.envCtx.unk_EE[3] = 23;
    for (int season = 0; season < SEASON_COUNT; ++season) {
        f.Select(season);
        f.Frames(50);
        REQUIRE(f.play.envCtx.unk_EE[2] == 7);
        REQUIRE(f.play.envCtx.unk_EE[3] == 23);
        REQUIRE(f.play.actorCtx.actorLists[ACTORCAT_ITEMACTION].length == 0);
    }
}

static void DefaultSpringIsDryAndDefaultAutumnIsThirtyDrops() {
    Fixture f;
    sSave.seasonsOwned = (1 << SEASON_SPRING) | (1 << SEASON_AUTUMN);
    sSave.season = SEASON_SPRING;
    f.Frames(50);
    REQUIRE(f.play.envCtx.unk_EE[0] == 0 && sLoopGain == 0);
    sSave.season = SEASON_AUTUMN;
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
    REQUIRE(std::fabs(sLoopGain - 0.5f) < 0.0001f);
    REQUIRE(f.play.envCtx.lightningMode == LIGHTNING_MODE_OFF);
    REQUIRE(GlobalOutdoorRain_HasRainIntent());
    uint8_t r = 20, g = 30, b = 40;
    REQUIRE(!GlobalOutdoorRain_GetRenderColor(&r, &g, &b));
    REQUIRE(r == 20 && g == 30 && b == 40);
}

static void NativeAndScriptedRainRemainAuthoritative() {
    Fixture f;
    f.Select(SEASON_AUTUMN);
    int actor;
    GlobalOutdoorRain_SetNativeRequest(&f.play, &actor, 25, 1);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
    REQUIRE(f.play.envCtx.lightningMode == LIGHTNING_MODE_ON);
    f.Select(SEASON_SUMMER);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
    f.Select(SEASON_SPRING);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 0;
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
    GlobalOutdoorRain_BeginScene(&f.play, 40, 1, 0);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 40);
    f.play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&f.play, 0);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 0 && sLoopGain == 0);
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.play.csCtx.state = CS_STATE_IDLE;
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 40);
}

static void GlobalRainComposesAboveSeasonalRain() {
    Fixture f;
    f.Select(SEASON_AUTUMN);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
    sSettings["gAudioEditor.GlobalOutdoorRain"] = 1;
    f.Frames(50);
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
    uint8_t r = 0, g = 0, b = 0;
    REQUIRE(GlobalOutdoorRain_GetRenderColor(&r, &g, &b));
    f.Select(SEASON_SUMMER);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 25);
    sSettings["gAudioEditor.GlobalOutdoorRain"] = 0;
    f.Frames(50);
    REQUIRE(f.play.envCtx.unk_EE[0] == 0);
    f.Select(SEASON_AUTUMN);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
}

static void SpringShowersUseTheSharedOvercastSetting() {
    Fixture f;
    f.Select(SEASON_SPRING);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 1;
    sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 1;
    f.Frames(50);
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
    func_8006FB94(&f.play.envCtx, 0); // the native engine consumes the queued overcast
    REQUIRE(f.play.envCtx.unk_DE == 1);
    sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 0;
    f.Frame();
    REQUIRE(f.play.envCtx.gloomySkyMode == 2);
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
}

static void IntermittentSeasonRainHasPrivatePausedCadence() {
    Fixture f;
    f.Select(SEASON_SPRING);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 2;
    const u16 dayTime = gSaveContext.dayTime = 0x3456;
    bool sawRain = false, sawDryAfterRain = false, sawRainAgain = false;
    for (int i = 0; i < 2000; ++i) {
        f.Frame();
        int density = f.play.envCtx.unk_EE[0];
        if (density > 0) {
            if (sawDryAfterRain)
                sawRainAgain = true;
            sawRain = true;
        } else if (sawRain) {
            sawDryAfterRain = true;
        }
        REQUIRE(density >= 0 && density <= 30);
        REQUIRE(gSaveContext.dayTime == dayTime);
        if (density == 30) {
            f.play.pauseCtx.state = 1;
            float gain = sLoopGain;
            f.Frames(200);
            REQUIRE(f.play.envCtx.unk_EE[0] == 30);
            REQUIRE(std::fabs(sLoopGain - gain) < 0.0001f);
            f.play.pauseCtx.state = 0;
        }
        if (sawRainAgain)
            break;
    }
    REQUIRE(sawRain && sawDryAfterRain && sawRainAgain);
    REQUIRE(sNativeRandomCalls == 0);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 0;
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 0 && sLoopGain == 0);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 1;
    f.Frames(50);
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
    // Autumn's separate Off setting survives while Spring is changed.
    sSettings["gAudioEditor.SeasonWeather.Autumn.RainMode"] = 0;
    f.Select(SEASON_AUTUMN);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 0);
}

static void SavedRodGameplayAndNativeParticleColorStaySeparate() {
    Fixture f;
    f.Select(-1);
    SeasonWeather_TestRodBridge(&f.play, SEASON_AUTUMN);
    REQUIRE(f.play.envFlags[0] & (1 << 5));
    REQUIRE(f.play.envCtx.unk_EE[0] == 0);
    f.play.envFlags[0] = 0;
    f.Select(SEASON_AUTUMN);
    SeasonWeather_TestRodBridge(&f.play, SEASON_SPRING);
    REQUIRE((f.play.envFlags[0] & (1 << 5)) == 0);
    f.play.envCtx.indoors = 1;
    SeasonWeather_TestRodBridge(&f.play, SEASON_AUTUMN);
    REQUIRE((f.play.envFlags[0] & (1 << 5)) == 0);
    f.play.envCtx.indoors = 0;
    f.play.envCtx.unk_EE[2] = 11;
    f.play.envCtx.unk_EE[3] = 31;
    sSave.seasonsOwned = 1 << SEASON_SPRING;
    sSave.season = SEASON_SPRING;
    u8 r = 0, g = 0, b = 0;
    Seasons_PrecipTint(&r, &g, &b);
    REQUIRE(r == 200 && g == 200 && b == 200);
    SeasonWeather_TestRodBridge(&f.play, SEASON_WINTER);
    REQUIRE(f.play.envCtx.unk_EE[2] == 11 && f.play.envCtx.unk_EE[3] == 31);
}

static void RealEnvironmentRainAndSkyRestoreComposeWithSeason() {
    Fixture f;
    f.Select(SEASON_AUTUMN);
    for (int i = 0; i < 160; ++i) {
        f.Frame();
        func_800766C4(&f.play);
    }
    REQUIRE(f.play.envCtx.unk_EE[0] == 30 && f.play.envCtx.unk_EE[1] == 30);
    // The real overcast restoration used to clear the rain target. The shared
    // resolver's rain-intent signal must protect the seasonal owner too.
    f.play.envCtx.gloomySkyMode = 2;
    f.play.envCtx.unk_DE = 1;
    func_8006FB94(&f.play.envCtx, 0);
    REQUIRE(f.play.envCtx.unk_EE[0] == 30);
    f.play.envCtx.unk_F2[0] = 40; // native Song of Storms priority
    for (int i = 0; i < 100; ++i) {
        f.Frame();
        func_800766C4(&f.play);
    }
    REQUIRE(f.play.envCtx.unk_EE[1] == 40);
    f.play.envCtx.unk_F2[0] = 0;
    f.play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&f.play, 0);
    for (int i = 0; i < 180; ++i) {
        f.Frame();
        func_800766C4(&f.play);
    }
    REQUIRE(f.play.envCtx.unk_EE[0] == 0 && f.play.envCtx.unk_EE[1] == 0);
}

static void DuplicateUpdatesAndSceneReuseDoNotLeakOwnership() {
    Fixture f;
    f.Select(SEASON_SPRING);
    sSettings["gAudioEditor.SeasonWeather.Spring.RainMode"] = 1;
    f.Frame();
    int density = f.play.envCtx.unk_EE[0];
    float gain = sLoopGain;
    SeasonWeather_Update(&f.play); // Rod update + frame hook on the same native frame
    REQUIRE(f.play.envCtx.unk_EE[0] == density && sLoopGain == gain);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] > density);
    f.Select(SEASON_AUTUMN);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 30 && sLoopGain > 0);
    GlobalOutdoorRain_OnPlayDestroy();
    SeasonWeather_Reset();
    // Engine scene initialization at the same PlayState address.
    f.play.envCtx = {};
    f.Select(-1);
    GlobalOutdoorRain_BeginScene(&f.play, 0, 0, 0);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 0 && sLoopGain == 0);
    REQUIRE(f.play.envCtx.gloomySkyMode == 0);
    REQUIRE(f.play.envCtx.adjLight1Color[0] == 0 && f.play.envCtx.adjAmbientColor[0] == 0);
}

static void EligibilityAndShadeRestoreRespectSceneState() {
    Fixture f;
    f.Select(SEASON_WINTER);
    f.play.envCtx.adjLight1Color[0] = 9;
    f.play.envCtx.adjAmbientColor[0] = 12;
    f.Frame();
    REQUIRE(f.play.envCtx.adjLight1Color[0] == 9);
    REQUIRE(f.play.envCtx.adjAmbientColor[0] == 12);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == -46);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 1) == -54);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
    f.Frame();
    REQUIRE(f.play.envCtx.adjAmbientColor[0] == 12); // no shared-field writes
    f.Select(-1);
    f.Frame();
    REQUIRE(f.play.envCtx.adjLight1Color[0] == 9);
    REQUIRE(f.play.envCtx.adjAmbientColor[0] == 12);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == 0);
    REQUIRE(f.play.envCtx.gloomySkyMode == 0); // an unapplied request needs no native restore handshake
    f.play.envCtx.gloomySkyMode = 0;
    f.Select(SEASON_WINTER);
    f.Frame();
    f.play.envCtx.adjAmbientColor[0] = 100; // a native writer took this channel
    f.play.envCtx.indoors = 1;
    f.Frame();
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    REQUIRE(f.play.envCtx.adjAmbientColor[0] == 100);
    REQUIRE(f.play.envCtx.adjLight1Color[0] == 9);
    f.play.envCtx.indoors = 0;
    f.camera.unk_14C |= 0x100;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.camera.unk_14C = 0;
    f.play.envCtx.skyboxDisabled = 1;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.play.envCtx.skyboxDisabled = 0;
    f.play.envCtx.customSkyboxFilter = 1;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.play.envCtx.customSkyboxFilter = 0;
    f.play.gameOverCtx.state = GAMEOVER_DEATH_START;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == -1);
    f.play.gameOverCtx.state = GAMEOVER_INACTIVE;
    REQUIRE(SeasonWeather_SeasonForPlay(nullptr) == -1);
    // An open-air scene does not need the normal skybox for seasonal particles.
    f.play.skyboxId = SKYBOX_NONE;
    REQUIRE(SeasonWeather_SeasonForPlay(&f.play) == SEASON_WINTER);
}

static void NativeBombFadeNeverConsumesSeasonalShade() {
    for (int season : { SEASON_AUTUMN, SEASON_WINTER }) {
        Fixture f;
        f.Select(season);
        f.Frame();
        for (int i = 0; i < 3; ++i) {
            f.play.envCtx.lightSettings.ambientColor[i] = 100;
            f.play.envCtx.lightSettings.light1Color[i] = 100;
            f.play.envCtx.lightSettings.light2Color[i] = 100;
        }
        SeasonWeather_TestBombFlash(&f.play);
        SeasonWeather_TestNativeLighting(&f.play);
        for (int i = 0; i < 3; ++i) {
            REQUIRE(f.play.envCtx.adjLight1Color[i] == 250);
            REQUIRE(f.play.envCtx.adjAmbientColor[i] == 250);
            REQUIRE(f.play.lightCtx.ambientColor[i] == 255);
            REQUIRE(f.play.envCtx.dirLight1.params.dir.color[i] == 255);
            REQUIRE(f.play.envCtx.dirLight2.params.dir.color[i] == 255);
        }
        for (int frame = 0; frame < 10; ++frame) {
            SeasonWeather_TestBombFade(&f.play);
            f.Frame();
            SeasonWeather_TestNativeLighting(&f.play);
            for (int i = 0; i < 3; ++i) {
                REQUIRE(f.play.envCtx.adjLight1Color[i] == 250 - 25 * (frame + 1));
                REQUIRE(f.play.envCtx.adjAmbientColor[i] == 250 - 25 * (frame + 1));
            }
        }
        for (int i = 0; i < 3; ++i) {
            REQUIRE(f.play.envCtx.adjLight1Color[i] == 0);
            REQUIRE(f.play.envCtx.adjAmbientColor[i] == 0);
        }
        f.Select(-1);
        f.Frame();
        REQUIRE(f.play.envCtx.adjLight1Color[0] == 0 && f.play.envCtx.adjAmbientColor[0] == 0);
    }
}

static void NativeLightingClampPreservesClassicColorsAndBothDirectionalShades() {
    Fixture f;
    const u8 ambient[] = { 20, 90, 200 }, light1[] = { 80, 140, 220 }, light2[] = { 70, 60, 190 };
    const u8 fog[] = { 20, 150, 230 };
    const s16 adjAmbient[] = { -50, 20, 60 }, adjLight[] = { -20, 10, 20 }, adjFog[] = { -50, 10, 50 };
    for (int i = 0; i < 3; ++i) {
        f.play.envCtx.lightSettings.ambientColor[i] = ambient[i];
        f.play.envCtx.lightSettings.light1Color[i] = light1[i];
        f.play.envCtx.lightSettings.light2Color[i] = light2[i];
        f.play.envCtx.lightSettings.fogColor[i] = fog[i];
        f.play.envCtx.adjAmbientColor[i] = adjAmbient[i];
        f.play.envCtx.adjLight1Color[i] = adjLight[i];
        f.play.envCtx.adjFogColor[i] = adjFog[i];
    }
    const auto requireColors = [&](const u8* expectedAmbient, const u8* expectedLight1, const u8* expectedLight2) {
        SeasonWeather_TestNativeLighting(&f.play); // exact current Environment_Update clamp block
        for (int i = 0; i < 3; ++i) {
            REQUIRE(f.play.lightCtx.ambientColor[i] == expectedAmbient[i]);
            REQUIRE(f.play.envCtx.dirLight1.params.dir.color[i] == expectedLight1[i]);
            REQUIRE(f.play.envCtx.dirLight2.params.dir.color[i] == expectedLight2[i]);
            REQUIRE(f.play.envCtx.adjAmbientColor[i] == adjAmbient[i]);
            REQUIRE(f.play.envCtx.adjLight1Color[i] == adjLight[i]);
            REQUIRE(f.play.envCtx.adjFogColor[i] == adjFog[i]);
        }
        REQUIRE(f.play.lightCtx.fogColor[0] == 0 && f.play.lightCtx.fogColor[1] == 160 &&
                f.play.lightCtx.fogColor[2] == 255);
    };
    const u8 nativeAmbient[] = { 0, 110, 255 }, nativeLight1[] = { 60, 150, 240 }, nativeLight2[] = { 50, 70, 210 };
    for (int season : { -1, -2, SEASON_SPRING, SEASON_SUMMER }) {
        f.Select(season);
        requireColors(nativeAmbient, nativeLight1, nativeLight2);
    }
    const u8 winterAmbient[] = { 0, 68, 240 }, winterLight1[] = { 14, 116, 226 }, winterLight2[] = { 4, 36, 196 };
    f.Select(SEASON_WINTER);
    requireColors(winterAmbient, winterLight1, winterLight2);
    const u8 autumnAmbient[] = { 0, 70, 226 }, autumnLight1[] = { 26, 118, 214 }, autumnLight2[] = { 16, 38, 184 };
    f.Select(SEASON_AUTUMN);
    requireColors(autumnAmbient, autumnLight1, autumnLight2);
    gLightningStrike.state = LIGHTNING_STRIKE_START;
    requireColors(nativeAmbient, nativeLight1, nativeLight2);
    gLightningStrike.state = LIGHTNING_STRIKE_WAIT;
}

static void LightOffsetKeepsNativeLightningAndBounds() {
    Fixture f;
    f.Select(SEASON_WINTER);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == -46);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 2, 1) == -20);
    REQUIRE(SeasonWeather_LightOffset(&f.play, -1, 1) == 0);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 3, 0) == 0);
    gLightningStrike.state = LIGHTNING_STRIKE_START;
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == 0);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 2, 1) == 0);
    gLightningStrike.state = LIGHTNING_STRIKE_WAIT;
    f.Select(SEASON_AUTUMN);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == -34);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 2, 1) == -34);
    f.Select(SEASON_SPRING);
    REQUIRE(SeasonWeather_LightOffset(&f.play, 0, 0) == 0);
}

static void ScriptedRainRetainsItsExistingSkyOwnership() {
    Fixture f;
    f.Select(-1);
    sSettings["gAudioEditor.GlobalOutdoorRain"] = 1;
    sSettings["gAudioEditor.GlobalOutdoorRainOvercast"] = 1;
    f.Frames(50);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
    f.play.csCtx.state = 1;
    GlobalOutdoorRain_SetScriptedRain(&f.play, 20);
    f.Frame();
    REQUIRE(f.play.envCtx.unk_EE[0] == 20);
    REQUIRE(f.play.envCtx.gloomySkyMode == 1);
}

int main() {
    REQUIRE(SeasonWeather_SeasonForPlay != nullptr);
    REQUIRE(SeasonWeather_Update != nullptr);
    REQUIRE(SeasonWeather_Reset != nullptr);
    REQUIRE(SeasonWeather_ParticlesEnabled != nullptr);
    REQUIRE(SeasonWeather_FlurriesEnabled != nullptr);
    REQUIRE(SeasonWeather_SummerEffectEnabled != nullptr);
    REQUIRE(SeasonWeather_LightOffset != nullptr);
    ManualPresentationDoesNotChangeSavedRodState();
    ParticleSwitchesAreIndependentAndPreserveNativeSlots();
    DefaultSpringIsDryAndDefaultAutumnIsThirtyDrops();
    NativeAndScriptedRainRemainAuthoritative();
    GlobalRainComposesAboveSeasonalRain();
    SpringShowersUseTheSharedOvercastSetting();
    IntermittentSeasonRainHasPrivatePausedCadence();
    EligibilityAndShadeRestoreRespectSceneState();
    SavedRodGameplayAndNativeParticleColorStaySeparate();
    RealEnvironmentRainAndSkyRestoreComposeWithSeason();
    DuplicateUpdatesAndSceneReuseDoNotLeakOwnership();
    NativeBombFadeNeverConsumesSeasonalShade();
    NativeLightingClampPreservesClassicColorsAndBothDirectionalShades();
    LightOffsetKeepsNativeLightningAndBounds();
    ScriptedRainRetainsItsExistingSkyOwnership();
    NativeSkyTagsKeepTheirRangeActivationAndRestoration();
    LateNativeTagsReceiveQueuedAndAppliedSeasonalSky();
    NativeTagsDoNotDisableTheUnmodifiedNativePresentation();
    RetiredNativeTagsDoNotOverrideSavedRodWeather();
    SeasonalSkyYieldPreservesScriptOwnershipAndThenNativeTags();
    puts("PASS SoH seasonal presentation: saved Rod state, independent controls, native particle slots, actual "
         "weather-tag entry/exit, queued/applied/scripted sky handoffs, private cadence, native lighting clamp and "
         "bomb decay");
}
