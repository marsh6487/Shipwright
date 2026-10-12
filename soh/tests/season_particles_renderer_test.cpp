#if __has_include("soh/Enhancements/Graphics/SeasonParticles.h")
#include "z64.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
}
#include "soh/Enhancements/Graphics/SeasonParticles.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/audio/SeasonWeather.h"
#include "mods/extended_inventory.h"
#include "soh/Enhancements/Graphics/SeasonParticleState.h"
#include "soh/Enhancements/Graphics/SeasonSummerState.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

SaveContext gSaveContext{};
GameInteractor* GameInteractor::Instance = nullptr;
extern "C" {
PlayState* gPlayState = nullptr;
}
static GameInfo gameInfo{};
GameInfo* gGameInfo = &gameInfo;
static int season = 2, enabled = 1, flurries = 0;
static int summerEffects[3] = { 1, 1, 0 };
static int summerEnabled = 1;
static int winterEffects[2] = { 0, 0 };
static bool floorValid = true;
static int floorQueries = 0;
static int matrices = 0, stack = 0, children = 0, scopes = 0, beamVertices = 0, setupCalls = 0;
static Gfx xlu[4096], opa[1024];
static Vtx dynamicVertices[512];
static size_t allocatedVertices = 0;
static Mtx matrix;
static std::vector<Vec3f> translations;
static Vec3f currentTranslation;
static std::vector<Vec3f> meshOrigins;
static std::vector<Gfx*> displayLists;
static std::vector<unsigned> alphas;
static float aspect = 16.0f / 9.0f;
#include "season_particles_native_setup.inc"
extern "C" int32_t CVarGetInteger(const char* key, int32_t fallback) {
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Season")) == 0)
        return season;
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Spring.Blossoms")) == 0 ||
        std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Autumn.Leaves")) == 0 ||
        std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Winter.Snow")) == 0)
        return enabled;
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Winter.Flurries")) == 0)
        return flurries;
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Winter.Mist")) == 0)
        return winterEffects[0];
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Winter.Aurora")) == 0)
        return winterEffects[1];
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Summer.Enabled")) == 0)
        return summerEnabled;
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Summer.Fluff")) == 0)
        return summerEffects[0];
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Summer.Fireflies")) == 0)
        return summerEffects[1];
    if (std::strcmp(key, SOH_SEASON_WEATHER_CVAR("Summer.Sunbeams")) == 0)
        return summerEffects[2];
    return fallback;
}
extern "C" uint8_t Seasons_SeasonCount() {
    assert(false);
    return 0;
}
extern "C" uint8_t Seasons_GetSeason() {
    assert(false);
    return 0;
}
// Verbatim controller predicates from production drive the complete renderer.
#include "season_particles_controller.inc"
extern "C" float OTRGetAspectRatio() {
    return aspect;
}
extern "C" void FrameInterpolation_RecordOpenChild(const void*, int) {
    ++children;
}
extern "C" void FrameInterpolation_RecordCloseChild() {
    assert(children > 0);
    --children;
}
extern "C" void Graph_OpenDisps(Gfx**, GraphicsContext*, const char*, s32) {
    ++scopes;
}
extern "C" void Graph_CloseDisps(Gfx**, GraphicsContext*, const char*, s32) {
    assert(scopes > 0);
    --scopes;
}
extern "C" void Gfx_SetupDL_25Xlu(GraphicsContext* gfx) {
    ++setupCalls;
    // Submit SoH's exact production setup material, including its textured
    // combiner, cycle/render mode and geometry flags.
    __gSPDisplayList(gfx->polyXlu.p++, sSetupDL[SETUPDL_25]);
}
extern "C" void Matrix_Push() {
    ++stack;
}
extern "C" void Matrix_Pop() {
    assert(stack > 0);
    --stack;
}
extern "C" void Matrix_Translate(f32 x, f32 y, f32 z, u8) {
    currentTranslation = { x, y, z };
    translations.push_back({ x, y, z });
}
extern "C" void Matrix_Mult(MtxF*, u8) {
}
extern "C" void Matrix_Scale(f32 x, f32 y, f32, u8) {
    assert(x > 0 && y > 0);
}
extern "C" void Matrix_RotateZ(f32 angle, u8) {
    assert(std::isfinite(angle));
}
extern "C" Mtx* Matrix_NewMtx(GraphicsContext*, char*, s32) {
    ++matrices;
    return &matrix;
}
extern "C" void* Graph_Alloc(GraphicsContext*, size_t size) {
    assert(size % sizeof(Vtx) == 0 && allocatedVertices + size / sizeof(Vtx) <= 512);
    auto* result = dynamicVertices + allocatedVertices;
    allocatedVertices += size / sizeof(Vtx);
    return result;
}
extern "C" void gSPDisplayList(Gfx* pkt, Gfx* list) {
    displayLists.push_back(list);
    __gSPDisplayList(pkt, list);
}
extern "C" void gSPVertex(Gfx* pkt, uintptr_t address, int count, int start) {
    assert(count > 0 && count <= 32 && start == 0);
    ++beamVertices;
    meshOrigins.push_back(currentTranslation);
    __gSPVertex(pkt, address, count, start);
}
extern "C" void gSPSegment(void* pkt, int segment, uintptr_t address) {
    __gSPSegment(static_cast<Gfx*>(pkt), segment, address);
}
extern "C" f32 BgCheck_AnyRaycastFloor1(CollisionContext*, CollisionPoly*, Vec3f* point) {
    ++floorQueries;
    return floorValid ? point->x * 0.04f + point->z * 0.02f : BGCHECK_Y_MIN;
}

static size_t Draw(PlayState& play) {
    auto& gfx = *play.state.gfxCtx;
    gfx.polyXlu.p = xlu;
    gfx.polyOpa.p = opa;
    gfx.polyOpa.d = opa + 1024;
    translations.clear();
    meshOrigins.clear();
    displayLists.clear();
    alphas.clear();
    matrices = beamVertices = setupCalls = 0;
    allocatedVertices = 0;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDrawEnd>();
    const size_t commands = gfx.polyXlu.p - xlu;
    assert(commands < 2048);
    assert(stack == 0 && children == 0 && scopes == 0);
    assert(matrices <= 96);
    bool texture = true, depthTest = false;
    Gfx lastCombine{};
    auto inspect = [&](auto&& self, const Gfx* p) -> void {
        const unsigned opcode = p->words.w0 >> 24;
        if (opcode == G_DL && p->words.w1 == reinterpret_cast<uintptr_t>(sSetupDL[SETUPDL_25])) {
            for (const auto& command : sSetupDL[SETUPDL_25])
                self(self, &command);
        }
        if (opcode == G_SETCOMBINE)
            lastCombine = *p;
        if (opcode == G_TEXTURE)
            texture = (p->words.w0 & 0xFF) != 0;
        if (opcode == G_SETPRIMCOLOR)
            alphas.push_back(p->words.w1 & 0xFF);
        if (opcode == G_SETOTHERMODE_L && (p->words.w1 & Z_CMP))
            depthTest = true;
    };
    for (auto* p = xlu; p < gfx.polyXlu.p; ++p)
        inspect(inspect, p);
    if (matrices) {
        assert(texture && depthTest);
        assert(lastCombine.words.w0 == sSetupDL[SETUPDL_25][2].words.w0);
        assert(lastCombine.words.w1 == sSetupDL[SETUPDL_25][2].words.w1);
        assert(setupCalls == 2 || setupCalls == 4); // Each drawn layer restores its material.
    } else {
        assert(commands == 0 && setupCalls == 0); // Disabled/all-culled layers preserve their consumer.
    }
    return commands;
}
static bool Same(Vec3f a, Vec3f b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
static void Advance(PlayState& play, int frames) {
    for (int frame = 0; frame < frames; ++frame) {
        ++play.gameplayFrames;
        Draw(play);
    }
}
int main() {
    static PlayState play{};
    static Camera camera{};
    static GraphicsContext gfx{};
    static Actor player{};
    play.state.gfxCtx = &gfx;
    gPlayState = &play;
    static GameInteractor interactor;
    GameInteractor::Instance = &interactor;
    ShipInit::InitAll();
    assert(GameInteractor::RegisteredGameHooks<GameInteractor::OnPlayDrawEnd>::functions.size() == 1);
    play.sceneNum = SCENE_HYRULE_FIELD;
    play.roomCtx.curRoom.num = 0;
    play.cameraPtrs[0] = &camera;
    play.view.eye = camera.eye = { 0, 100, 0 };
    play.view.lookAt = camera.at = { 0, 100, 1000 };
    play.view.up = { 0, 1, 0 };
    play.view.fovy = camera.fov = 60;
    player.world.pos = { 0, 100, -300 };
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player;
    gSaveContext.dayTime = CLOCK_TIME(12, 0);
    R_UPDATE_RATE = 3;
    const auto saveBefore = gSaveContext;
    const auto environmentBefore = play.envCtx;
    const auto playerBefore = player;
    Advance(play, 25);
    assert(matrices == 96 && displayLists.size() == 96);
    auto autumn = translations;
    auto autumnList = displayLists[0];
    Draw(play);
    for (size_t i = 0; i < 96; ++i)
        assert(Same(translations[i], autumn[i]));
    play.pauseCtx.state = 6;
    Advance(play, 10);
    for (size_t i = 0; i < 96; ++i)
        assert(Same(translations[i], autumn[i]));
    play.pauseCtx.state = 0;
    play.pauseCtx.debugState = 1;
    Advance(play, 3);
    for (size_t i = 0; i < 96; ++i)
        assert(Same(translations[i], autumn[i]));
    play.pauseCtx.debugState = 0;
    play.pauseCtx.state = 6;
    enabled = 0;
    assert(Draw(play) == 0);
    enabled = 1;
    season = 0;
    Draw(play);
    assert(matrices == 96 && displayLists[0] != autumnList);
    auto petalList = displayLists[0];
    play.view.eye.x += 50000;
    play.view.lookAt.x += 50000;
    assert(Draw(play) == 0); // A paused, fully culled untextured layer emits no setup or restoration.
    play.view.eye.x -= 50000;
    play.view.lookAt.x -= 50000;
    Draw(play);
    assert(matrices == 96);
    // The submitted spring mesh must retain the donor13 vertices and12
    // curved triangles; a tinted snow quad cannot satisfy this contract.
    assert((petalList[0].words.w0 >> 24) == G_VTX);
    const auto* petalVertices = reinterpret_cast<const Vtx*>(petalList[0].words.w1);
    bool curved = false;
    for (int i = 0; i < 13; ++i)
        curved |= petalVertices[i].v.ob[2] != 0;
    assert(curved);
    play.pauseCtx.state = 0;
    season = 3;
    Draw(play);
    assert(matrices == 64);
    static Actor fairy{};
    fairy.id = ACTOR_OBJECT_KANKYO;
    fairy.params = 0;
    fairy.update = reinterpret_cast<ActorFunc>(1);
    play.actorCtx.actorLists[ACTORCAT_ITEMACTION].head = &fairy;
    assert(Draw(play) > 0 && matrices == 64); // Kokiri fairy emitter remains independent.
    const auto fairyBefore = fairy;
    static Actor snow{};
    snow.id = ACTOR_OBJECT_KANKYO;
    snow.params = 3;
    snow.update = reinterpret_cast<ActorFunc>(1);
    fairy.next = &snow;
    assert(Draw(play) == 0); // Live native snow owns winter instead of a duplicate.
    snow.update = nullptr;
    assert(Draw(play) > 0 && matrices == 64);
    fairy.next = nullptr;
    assert(std::memcmp(&fairy, &fairyBefore, sizeof(fairy)) == 0);
    play.actorCtx.actorLists[ACTORCAT_ITEMACTION].head = nullptr;
    assert(std::memcmp(&gSaveContext, &saveBefore, sizeof(saveBefore)) == 0);
    assert(std::memcmp(&play.envCtx, &environmentBefore, sizeof(environmentBefore)) == 0);
    assert(std::memcmp(&player, &playerBefore, sizeof(playerBefore)) == 0);
    for (int value : { -1, 0, 2, 3 }) {
        season = value;
        play.transitionTrigger = TRANS_TRIGGER_START;
        assert(Draw(play) == 0);
        play.transitionTrigger = TRANS_TRIGGER_OFF;
        play.transitionMode = TRANS_MODE_INSTANCE_RUNNING;
        assert(Draw(play) == 0);
        play.transitionMode = TRANS_MODE_OFF;
        camera.unk_14C = 0x100;
        assert(Draw(play) == 0);
        camera.unk_14C = 0;
    }
    season = 2;
    for (int gate = 0; gate < 4; ++gate) {
        play.envCtx.indoors = gate == 0;
        play.envCtx.skyboxDisabled = gate == 1;
        play.csCtx.state = gate == 2 ? 1 : CS_STATE_IDLE;
        play.gameOverCtx.state = gate == 3 ? GAMEOVER_DEATH_START : GAMEOVER_INACTIVE;
        assert(Draw(play) == 0);
    }
    play.envCtx.indoors = play.envCtx.skyboxDisabled = 0;
    play.csCtx.state = CS_STATE_IDLE;
    play.gameOverCtx.state = GAMEOVER_INACTIVE;
    season = 1;
    assert(!SeasonWeather_ParticlesEnabled(1));
    assert(SeasonWeather_SummerEffectEnabled(0));
    play.envCtx.sunPos = { 450, 800, -250 };
    Advance(play, 30);
    assert(matrices == 32 && beamVertices == 0);
    summerEnabled = 0;
    assert(Draw(play) == 0);
    summerEnabled = 1;
    Advance(play, 30);
    assert(matrices == 32);
    summerEffects[2] = 1;
    Draw(play);
    assert(matrices == 33 && beamVertices == 3 && allocatedVertices == 36);
    gSaveContext.dayTime = CLOCK_TIME(23, 0);
    Draw(play);
    assert(matrices == 56 && beamVertices == 0);
    summerEffects[2] = 0;
    gSaveContext.dayTime = CLOCK_TIME(18, 0);
    Draw(play);
    assert(matrices > 56 && matrices <= 88);
    const auto dusk = translations;
    play.pauseCtx.state = 6;
    summerEffects[0] = 0;
    Draw(play);
    assert(matrices > 0 && matrices <= 56);
    for (const auto& point : translations)
        assert(std::any_of(dusk.begin(), dusk.end(), [&](Vec3f p) { return Same(point, p); }));
    summerEffects[1] = 0;
    summerEffects[2] = 0;
    assert(Draw(play) == 0);
    gSaveContext.dayTime = CLOCK_TIME(12, 0);
    summerEffects[2] = 1;
    Draw(play);
    assert(matrices == 1 && beamVertices == 3);
    play.envCtx.unk_EE[1] = 30;
    assert(Draw(play) == 0);
    play.envCtx.unk_EE[1] = 0;
    play.envCtx.sunMoonDisabled = 1;
    assert(Draw(play) == 0);
    play.envCtx.sunMoonDisabled = 0;
    summerEffects[0] = summerEffects[1] = 1;
    summerEffects[2] = 0;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayDestroy>();
    Draw(play);
    assert(matrices == 0); // New summer entries fade in.
    play.pauseCtx.state = 0;
    Advance(play, 30);
    assert(matrices == 32);
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSceneInit>(play.sceneNum);
    assert(Draw(play) == 0);
    Advance(play, 30);
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnExitGame>(0);
    assert(Draw(play) == 0);
    Advance(play, 30);
    ++play.roomCtx.curRoom.num;
    assert(Draw(play) == 0);
    Advance(play, 30);
    ++play.sceneNum;
    assert(Draw(play) == 0);
    Advance(play, 30);
    play.gameplayFrames = 0;
    assert(Draw(play) == 0);
    // Removing the snow-independent winter draw path must fail this test.
    play.pauseCtx.state = 0;
    play.skyboxId = SKYBOX_NORMAL_SKY;
    play.view.zFar = 12800;
    season = 3;
    enabled = 0;
    winterEffects[0] = 1;
    winterEffects[1] = 0;
    gSaveContext.dayTime = CLOCK_TIME(12, 0);
    const auto winterSave = gSaveContext;
    const auto winterEnvironment = play.envCtx;
    Advance(play, 30);
    assert(Draw(play) > 0 && matrices > 0 && matrices <= 12 && allocatedVertices > 0 && displayLists.empty());
    assert(floorQueries > 0);
    assert(allocatedVertices <= 252);
    // Every mist sheet is grounded on the sampled sloped terrain, not camera Y.
    for (size_t i = 0; i < allocatedVertices; i += 15) {
        const auto& center = dynamicVertices[i + 7];
        const auto origin = meshOrigins[i / 15];
        const float ground = (center.v.ob[0] + origin.x) * 0.04f + (center.v.ob[2] + origin.z) * 0.02f;
        const float clearance = center.v.ob[1] + origin.y - ground;
        assert(clearance >= 5 && clearance <= 40);
    }
    const auto mistVertices = std::vector<Vtx>(dynamicVertices, dynamicVertices + allocatedVertices);
    const int samples = floorQueries;
    play.pauseCtx.state = 6;
    Draw(play);
    assert(floorQueries == samples && allocatedVertices == mistVertices.size());
    assert(std::memcmp(dynamicVertices, mistVertices.data(), allocatedVertices * sizeof(Vtx)) == 0);
    const auto oldOrigins = meshOrigins;
    play.view.eye.x += 20;
    play.view.lookAt.x += 20;
    Draw(play);
    assert(allocatedVertices == mistVertices.size() && meshOrigins.size() == oldOrigins.size());
    for (size_t i = 0; i < meshOrigins.size(); ++i) {
        const float oldCenter = oldOrigins[i].x + mistVertices[i * 15 + 7].v.ob[0];
        // SoH interpolates matrix translations but consumes current vertices.
        const float middle = (oldOrigins[i].x + meshOrigins[i].x) * 0.5f + dynamicVertices[i * 15 + 7].v.ob[0];
        assert(std::abs(middle - oldCenter) <= 1);
    }
    play.view.eye.x -= 20;
    play.view.lookAt.x -= 20;
    Draw(play);
    winterEffects[0] = 0;
    assert(Draw(play) == 0);
    winterEffects[0] = 1;
    assert(Draw(play) > 0);
    play.pauseCtx.state = 0;
    floorValid = false;
    ++play.roomCtx.curRoom.num;
    Advance(play, 30);
    assert(Draw(play) == 0); // Missing collision cannot produce floating fog.
    floorValid = true;
    winterEffects[0] = 0;
    winterEffects[1] = 1;
    assert(Draw(play) == 0); // Aurora cannot appear in daylight.
    gSaveContext.dayTime = CLOCK_TIME(23, 0);
    Advance(play, 30);
    assert(Draw(play) > 0 && matrices == 1 && allocatedVertices == 72);
    bool blue = false, green = false;
    for (size_t i = 0; i < allocatedVertices; ++i) {
        const auto& vertex = dynamicVertices[i];
        blue |= vertex.v.cn[2] > vertex.v.cn[1] && vertex.v.cn[3] > 0;
        green |= vertex.v.cn[1] > vertex.v.cn[2] && vertex.v.cn[3] > 0;
    }
    assert(blue && green);
    play.view.zFar = 2000;
    Draw(play);
    for (size_t i = 0; i < allocatedVertices; ++i) {
        const auto& vertex = dynamicVertices[i];
        const float length = std::sqrt(float(vertex.v.ob[0]) * vertex.v.ob[0] + float(vertex.v.ob[1]) * vertex.v.ob[1] +
                                       float(vertex.v.ob[2]) * vertex.v.ob[2]);
        assert(length < 2000);
    }
    play.skyboxId = SKYBOX_NONE;
    assert(Draw(play) == 0);
    play.skyboxId = SKYBOX_NORMAL_SKY;
    winterEffects[0] = 1;
    enabled = 1;
    Advance(play, 30);
    assert(Draw(play) > 0 && displayLists.size() == 64 && allocatedVertices > 0 && matrices > 65 && matrices <= 77);
    enabled = 0;
    fairy.next = &snow;
    snow.update = reinterpret_cast<ActorFunc>(1);
    play.actorCtx.actorLists[ACTORCAT_ITEMACTION].head = &fairy;
    Advance(play, 30);
    assert(Draw(play) > 0 && displayLists.empty()); // Native snow cannot suppress mist/aurora.
    assert(std::memcmp(&play.envCtx, &winterEnvironment, sizeof(winterEnvironment)) == 0);
    gSaveContext.dayTime = winterSave.dayTime;
    assert(std::memcmp(&gSaveContext, &winterSave, sizeof(winterSave)) == 0);
    for (int gate = 0; gate < 4; ++gate) {
        play.envCtx.indoors = gate == 0;
        play.envCtx.skyboxDisabled = gate == 1;
        play.csCtx.state = gate == 2 ? 1 : CS_STATE_IDLE;
        camera.unk_14C = gate == 3 ? 0x100 : 0;
        assert(Draw(play) == 0);
    }
    std::puts("PASS winter renderer: snow-independent terrain mist and night-only blue/green aurora, pause/toggles, "
              "clipping, native snow coexistence, gates and ownership");
    std::puts("PASS native renderer:96leaf/petal,64snow,32seed/56firefly/3beam; command/matrix budget, texture "
              "restoration, pause/toggle/gates/reset, native actor/save/environment ownership");
}
#else
#include <cstdio>
int main() {
    std::fputs("FAIL: standalone seasonal native renderer is missing\n", stderr);
    return 1;
}
#endif
