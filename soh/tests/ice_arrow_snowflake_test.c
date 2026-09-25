// Real actor code, game headers, and GBI. Engine allocation/resource services
// are fixtures; these checks cannot prove visual appearance in a running game.
#include "global.h"
#include "overlays/actors/ovl_Arrow_Ice/z_arrow_ice.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "soh/ResourceManagerHelpers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ArrowIce_Draw(Actor*, PlayState*);
void ArrowIce_Charge(ArrowIce*, PlayState*);
void ArrowIce_Fly(ArrowIce*, PlayState*);
void ArrowIce_Hit(ArrowIce*, PlayState*);

static GraphicsContext gfx;
static PlayState play;
static ArrowIce ice;
static EnArrow arrow;
static Gfx commands[256], reference[256], scroll[1];
static Mtx matrix;
static u8 pixels[64 * 64];
static int alt, asset, badLoad, customColors, matrixCount, matrixDepth;
static int audioCalls, magicCalls;
static f32 lastX, lastY, lastZ, lastScale, scales[8], positions[8][3];
static const char* texturePath = "__OTR__custom/henriko_effects/arrows/ice_snowflake_poc2";

#define REQUIRE(c)                                               \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            exit(1);                                             \
        }                                                        \
    } while (0)

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    return strstr(name, "Changed") ? customColors : fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    return strstr(name, "Primary") ? (Color_RGB8){ 241, 73, 19 } : (Color_RGB8){ 11, 23, 47 };
}
bool ResourceMgr_IsAltAssetsEnabled(void) {
    return alt;
}
uint8_t ResourceMgr_FileAltExists(const char* path) {
    return asset && !strcmp(path, texturePath);
}
void* ResourceGetDataByName(const char* path) {
    REQUIRE(!strcmp(path, texturePath));
    return badLoad ? NULL : pixels;
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void Gfx_SetupDL_25Xlu(GraphicsContext* context) {
    gDPPipeSync(context->polyXlu.p++);
}
Gfx* Gfx_SetupDL_57(Gfx* p) {
    gDPPipeSync(p++);
    return p;
}
Gfx* Gfx_TwoTexScrollEx(GraphicsContext* context, s32 a, u32 b, u32 c, s32 d, s32 e, s32 f, u32 g, u32 h, s32 i, s32 j,
                        s32 k, s32 l, s32 m, s32 n) {
    return scroll;
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void gSPVertex(Gfx* packet, uintptr_t vertices, int count, int first) {
    __gSPVertex(packet, vertices, count, first);
}
void Matrix_Push(void) {
    ++matrixDepth;
}
void Matrix_Pop(void) {
    --matrixDepth;
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    if (mode == MTXMODE_NEW) {
        lastX = x;
        lastY = y;
        lastZ = z;
    }
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    lastScale = x;
}
void Matrix_RotateX(f32 a, u8 mode) {
}
void Matrix_RotateY(f32 a, u8 mode) {
}
void Matrix_RotateZ(f32 a, u8 mode) {
}
void Matrix_RotateZYX(s16 x, s16 y, s16 z, u8 mode) {
}
void Matrix_ReplaceRotation(MtxF* m) {
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    REQUIRE(matrixCount < 8);
    scales[matrixCount] = lastScale;
    positions[matrixCount][0] = lastX;
    positions[matrixCount][1] = lastY;
    positions[matrixCount++][2] = lastZ;
    return &matrix;
}
void Actor_Kill(Actor* actor) {
    actor->update = NULL;
}
void Actor_SetScale(Actor* actor, f32 scale) {
    actor->scale = (Vec3f){ scale, scale, scale };
}
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* chain) {
}
void Actor_PlaySfx_Flagged(Actor* actor, u16 id) {
    ++audioCalls;
}
void Audio_PlayActorSound2(Actor* actor, u16 id) {
    ++audioCalls;
}
void Magic_Reset(PlayState* context) {
    ++magicCalls;
}
f32 Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b) {
    return sqrtf(SQ(a->x - b->x) + SQ(a->y - b->y) + SQ(a->z - b->z));
}
static void alive(Actor* a, PlayState* p) {
}

static void setup(void) {
    memset(&gfx, 0, sizeof(gfx));
    memset(&play, 0, sizeof(play));
    memset(&ice, 0, sizeof(ice));
    memset(&arrow, 0, sizeof(arrow));
    play.state.gfxCtx = &gfx;
    play.state.frames = play.gameplayFrames = 42;
    arrow.actor.update = alive;
    arrow.actor.world.pos = (Vec3f){ 10, 20, 30 };
    ice.actor.parent = &arrow.actor;
    ice.actor.world.pos = (Vec3f){ 100, 200, 300 };
    ice.actor.projectedW = 300.0f;
    ice.radius = 10;
    ice.alpha = 100;
    ice.unk_160 = 1.0f;
    ice.actionFunc = ArrowIce_Charge;
    alt = asset = 1;
    badLoad = customColors = audioCalls = magicCalls = 0;
}
static size_t draw(void) {
    ArrowIce before = ice;
    EnArrow parentBefore = arrow;
    memset(commands, 0, sizeof(commands));
    gfx.polyXlu.p = commands;
    matrixCount = matrixDepth = 0;
    ArrowIce_Draw(&ice.actor, &play);
    REQUIRE(matrixDepth == 0 && gfx.polyXlu.p < commands + ARRAY_COUNT(commands));
    REQUIRE(memcmp(&before, &ice, sizeof(ice)) == 0);
    REQUIRE(memcmp(&parentBefore, &arrow, sizeof(arrow)) == 0);
    REQUIRE(audioCalls == 0 && magicCalls == 0);
    return (size_t)(gfx.polyXlu.p - commands);
}
static int textures(size_t count, const char* suffix) {
    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((commands[i].words.w0 >> 24) != G_SETTIMG)
            continue;
        uintptr_t address = commands[i].words.w1;
        REQUIRE(address && !(address & 1));
        REQUIRE(!strncmp((const char*)address, "__OTR__", 7));
        if (strstr((const char*)address, suffix))
            ++found;
    }
    return found;
}
static int primAlpha(size_t count, unsigned rgb) {
    for (size_t i = count; i-- > 0;) {
        if ((commands[i].words.w0 >> 24) == G_SETPRIMCOLOR && ((commands[i].words.w1 >> 8) & 0xFFFFFF) == rgb)
            return commands[i].words.w1 & 255;
    }
    return -1;
}
static void hit(u16 timer) {
    ice.actionFunc = ArrowIce_Hit;
    ice.timer = timer;
    ice.alpha = 255;
    ice.unk_164 = 1.0f;
}

int main(void) {
    // Missing addition: charging must submit one named high-resolution sprite.
    setup();
    size_t count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1);
    REQUIRE(primAlpha(count, 0xAAFFFF) > 0 && primAlpha(count, 0xAAFFFF) < 128);

    // Fallback paths must produce identical native packets for every phase.
    for (int phase = 0; phase < 3; ++phase) {
        setup();
        if (phase == 1)
            ice.actionFunc = ArrowIce_Fly;
        if (phase == 2)
            hit(24);
        alt = 0;
        count = draw();
        memcpy(reference, commands, sizeof(commands));
        alt = 1;
        asset = 0;
        REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
        asset = 1;
        badLoad = 1;
        REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    }
    setup();
    ice.actionFunc = ArrowIce_Fly;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 0);
    setup();
    ice.radius = 0;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 0);
    setup();
    ice.actor.parent = NULL;
    REQUIRE(draw() == 0);
    setup();
    arrow.actor.update = NULL;
    REQUIRE(draw() == 0);

    // Impact expands at its captured position; moving the parent cannot drag it.
    setup();
    hit(32);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1);
    REQUIRE(textures(count, "gFlashTex") == 1);
    f32 initialScale = scales[matrixCount - 1];
    hit(24);
    count = draw();
    REQUIRE(scales[matrixCount - 1] > initialScale);
    for (int i = 0; i < matrixCount; ++i)
        REQUIRE(positions[i][0] == 100 && positions[i][1] == 200 && positions[i][2] == 300);
    int peakAlpha = primAlpha(count, 0xAAFFFF);
    arrow.actor.world.pos = (Vec3f){ -999, -999, -999 };
    hit(17);
    count = draw();
    REQUIRE(primAlpha(count, 0xAAFFFF) > 0 && primAlpha(count, 0xAAFFFF) < peakAlpha);
    REQUIRE(positions[matrixCount - 1][0] == 100);
    hit(16);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 0);
    REQUIRE(textures(count, "gFlashTex") == 0);
    hit(255);
    REQUIRE(draw() == 0);

    // Live cosmetics and pause redraws must not advance the effect or mutate state.
    setup();
    hit(24);
    customColors = 1;
    count = draw();
    REQUIRE(primAlpha(count, 0xF14913) > 0);
    memcpy(reference, commands, sizeof(commands));
    play.pauseCtx.state = 6;
    REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    alt = 0;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 0);
    alt = 1;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 1);
    puts("PASS: charge/impact packets, native fallbacks, missing/failed assets, phase gates, captured position, "
         "expansion/fade, cosmetics, pause, and draw-only state preservation");
    return 0;
}
