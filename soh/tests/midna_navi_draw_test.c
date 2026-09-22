/* The runner inserts the production draw functions unchanged. GPU/resource
 * execution is the boundary; actor types, visibility and GBI commands are real. */
#include <math.h>
#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "soh/ResourceManagerHelpers.h"

static Gfx commands[64], resource[1], allocated[4];
static unsigned nativeDraws, midnaSetups, resourceChecks, resourceLoads, pushes, pops;
static int packPresent, loadSucceeds;
static u8 inputAlpha = 255;
static f32 fairySize = 1.0f;
static Mtx matrix;
static GameInfo registers;
GameInfo* gGameInfo = &registers;

uint8_t ResourceMgr_FileExists(const char* path) {
    REQUIRE(strcmp(path, "objects/midna_navi/poc1/MidnaFloatDL") == 0);
    ++resourceChecks;
    return packPresent;
}
Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    REQUIRE(strcmp(path, "objects/midna_navi/poc1/MidnaFloatDL") == 0);
    ++resourceLoads;
    return loadSucceeds ? resource : NULL;
}
void* Graph_Alloc(GraphicsContext* gfx, size_t size) {
    REQUIRE(size == sizeof(allocated));
    return allocated;
}
void Gfx_SetupDL_27Xlu(GraphicsContext* gfx) {
}
void Gfx_SetupDL_25Xlu(GraphicsContext* gfx) {
    ++midnaSetups;
}
void FrameInterpolation_RecordOpenChild(const void* source, int line) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)command, segment, target);
}
void gSPDisplayList(Gfx* command, Gfx* list) {
    __gSPDisplayList(command, list);
}
void Matrix_Push(void) {
    ++pushes;
}
void Matrix_Pop(void) {
    ++pops;
}
void Matrix_RotateZ(f32 angle, u8 mode) {
    REQUIRE(isfinite(angle) && fabsf(angle) <= 0.04f);
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    /* POC1's packed 4471-unit height becomes 21.4608 world units at
     * native Navi scale 0.008. Apply the same factor on every axis. */
    REQUIRE(fabsf(x * 4471.0f * 0.008f - 21.4608f * fairySize) < 0.001f);
    REQUIRE(x == y && y == z && mode == MTXMODE_APPLY);
}
Mtx* Matrix_NewMtx(GraphicsContext* gfx, char* file, s32 line) {
    return &matrix;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
float CVarGetFloat(const char* name, float defaultValue) {
    return fairySize;
}
s32 EnElf_OverrideLimbDraw(PlayState* play, s32 limb, Gfx** dl, Vec3f* p, Vec3s* r, void* actor, Gfx** gfx) {
    return 0;
}
Gfx* SkelAnime_DrawSkeleton2(PlayState* play, SkelAnime* skel, OverrideLimbDrawOpa override, PostLimbDrawOpa post,
                             void* actor, Gfx* gfx) {
    ++nativeDraws;
    return gfx;
}

/* PRODUCTION_MIDNA_DRAW */

static void checkCase(int fairyType, int present, int loaded, int hiddenState, int hiddenFlag, int firstPerson,
                      int inFront, int expectMidna, int expectNative) {
    static PlayState play;
    static Player player;
    GraphicsContext gfx = { 0 };
    EnElf fairy = { 0 };
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(commands, 0, sizeof(commands));
    gfx.polyXlu.p = commands;
    play.state.gfxCtx = &gfx;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.stateFlags1 = firstPerson ? PLAYER_STATE1_FIRST_PERSON : 0;
    fairy.actor.params = fairyType;
    fairy.actor.projectedPos.z = inFront ? 1000 : -1000;
    fairy.actor.scale.x = fairy.actor.scale.y = fairy.actor.scale.z = 0.008f;
    fairy.unk_2A8 = hiddenState ? 8 : 0;
    fairy.fairyFlags = hiddenFlag ? 8 : 0;
    fairy.innerColor.a = inputAlpha;
    fairy.timer = 17;
    packPresent = present;
    loadSucceeds = loaded;
    nativeDraws = midnaSetups = resourceChecks = resourceLoads = pushes = pops = 0;
    EnElf before = fairy;
    Player playerBefore = player;
    EnElf_Draw(&fairy.actor, &play);
    REQUIRE(nativeDraws == expectNative);
    REQUIRE(midnaSetups == expectMidna);
    REQUIRE(pushes == pops && pushes == expectMidna);
    REQUIRE(memcmp(&before, &fairy, sizeof(fairy)) == 0);
    REQUIRE(memcmp(&playerBefore, &player, sizeof(player)) == 0);
    if (fairyType != FAIRY_NAVI || hiddenState || hiddenFlag || (firstPerson && !inFront)) {
        REQUIRE(resourceChecks == 0 && resourceLoads == 0);
    }
    if (expectMidna) {
        unsigned found = 0;
        for (Gfx* cmd = commands; cmd < gfx.polyXlu.p; ++cmd) {
            if ((cmd->words.w0 >> 24) == G_DL && cmd->words.w1 == (uintptr_t)resource)
                ++found;
            if (cmd->words.w0 == 0xE200001C && inputAlpha < 255)
                REQUIRE(!(cmd->words.w1 & Z_UPD));
        }
        REQUIRE(found == 1);
    }
}

int main(void) {
    /* Installed pack selects Midna only for the companion fairy. */
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    fairySize = 1.5f;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    fairySize = 1.0f;
    for (int type = FAIRY_REVIVE_BOTTLE; type <= FAIRY_HEAL_BIG; ++type)
        checkCase(type, 1, 1, 0, 0, 0, 1, 0, 1);
    /* Removing the pack or a failed load retains the native draw. */
    checkCase(FAIRY_NAVI, 0, 1, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 0, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 1, 1, 0, 0, 1, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 1, 0, 1, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 1, 0, 0, 0);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 1, 1, 1, 0);
    /* Simulated resource availability changes check fallback without pointer caching.
     * This does not claim support for mounting new archives without restarting. */
    checkCase(FAIRY_NAVI, 0, 0, 0, 0, 0, 1, 0, 1);
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    inputAlpha = 128;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 1, 0);
    inputAlpha = 0;
    checkCase(FAIRY_NAVI, 1, 1, 0, 0, 0, 1, 0, 0);
    puts("PASS: Midna is Navi-only; fallback, visibility, fade depth, actor state and matrix balance");
    return 0;
}
