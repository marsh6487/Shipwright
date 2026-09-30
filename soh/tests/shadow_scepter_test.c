/* Execute the production wand against real actor/GBI types. Only the resource,
 * audio, and matrix boundaries are stubbed; no game executable/ROM is needed. */
#include "global.h"
#include "tests/test_require.h"
#include <math.h>
#include <string.h>

static Gfx opa[128], xlu[128], model[4][1], renderMode[1];
static Gfx* draws[16];
static Mtx matrix;
static PlayState play;
static Player player;
static GraphicsContext graphics;
static Actor enemy, farther;
static unsigned drawCount, pushes, pops, bindings, loads, mmInits;
static int mmLoaded, missingPart;
Gfx gEmptyDL[] = { gsSPEndDisplayList() };
f32 gSfxDefaultFreqAndVolScale = 1.0f;
s8 gSfxDefaultReverb;

void FrameInterpolation_RecordOpenChild(const void* source, int line) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* format, ...) {
}
void MmAssets_Init(void) {
    ++mmInits;
}
u8 MmAssets_IsLoaded(void) {
    return mmLoaded;
}
Gfx* MmAssets_LoadDisplayListGraphStrict(const char* path) {
    static const char* names[] = { "gBlackBoeBodyMaterialDL", "gBlackBoeBodyModelDL", "gBlackBoeEndDL",
                                   "gBlackBoeEyesDL" };
    ++loads;
    for (int i = 0; i < 4; ++i) {
        if (strstr(path, names[i])) {
            return missingPart == i + 1 ? NULL : model[i];
        }
    }
    REQUIRE(0);
    return NULL;
}
void MmAssets_EnsureStrictTextureBindings(void) {
    ++bindings;
}
Gfx* MmAssets_GetOpaqueRenderMode(void) {
    return renderMode;
}
void gSPDisplayList(Gfx* command, Gfx* list) {
    REQUIRE(drawCount < ARRAY_COUNT(draws));
    draws[drawCount++] = list;
    __gSPDisplayList(command, list);
}
void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)command, segment, target);
}
void Gfx_SetupDL_25Xlu(GraphicsContext* gfx) {
}
void Gfx_SetupDL_25Opa(GraphicsContext* gfx) {
}
void Matrix_Push(void) {
    ++pushes;
}
void Matrix_Pop(void) {
    ++pops;
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    REQUIRE(isfinite(x) && isfinite(y) && isfinite(z));
}
void Matrix_ReplaceRotation(MtxF* rotation) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    REQUIRE(x > 0 && y == x && z == x);
}
Mtx* Matrix_NewMtx(GraphicsContext* gfx, char* file, s32 line) {
    return &matrix;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (M_PI / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (M_PI / 32768.0f));
}
s16 Math_Vec3f_Yaw(Vec3f* from, Vec3f* to) {
    return (s16)(atan2f(to->x - from->x, to->z - from->z) * (32768.0f / M_PI));
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* frequency, f32* volume, s8* reverb) {
}
void Actor_SetColorFilter(Actor* actor, s16 flag, s16 intensity, s16 xluFlag, s16 duration) {
    actor->colorFilterTimer = duration;
}

#include "mods/items/logic/wand/wand_shadow.c"

static void alive(Actor* actor, PlayState* state) {
}

static void resetDraw(void) {
    memset(opa, 0, sizeof(opa));
    memset(xlu, 0, sizeof(xlu));
    graphics.polyOpa.p = opa;
    graphics.polyXlu.p = xlu;
    drawCount = pushes = pops = bindings = loads = mmInits = 0;
}

static void reset(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&enemy, 0, sizeof(enemy));
    memset(&farther, 0, sizeof(farther));
    memset(&graphics, 0, sizeof(graphics));
    play.state.gfxCtx = &graphics;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    enemy.category = ACTORCAT_ENEMY;
    enemy.update = alive;
    enemy.world.pos.z = enemy.focus.pos.z = 150.0f;
    enemy.colChkInfo.health = 5;
    play.actorCtx.actorLists[ACTORCAT_ENEMY].head = &enemy;
    WandShadow_Forget();
    mmLoaded = 0;
    missingPart = 0;
    resetDraw();
}

static void testRendering(void) {
    reset();
    WandShadow_Draw(&play);
    REQUIRE(drawCount == 0 && mmInits == 0);

    u8 cast = WandShadow_Cast(&player, &play);
    REQUIRE(cast);
    WandShadow_Draw(&play);
    REQUIRE(drawCount > 0); /* Missing MM must never be a successful invisible cast. */
    REQUIRE(pushes == 1 && pops == 1 && bindings == 0);

    mmLoaded = 1; /* A previous miss must not latch the renderer off. */
    resetDraw();
    gDPSetEnvColor(graphics.polyXlu.p++, 0, 0, 0, 0);
    WandShadow_Draw(&play);
    REQUIRE(drawCount == 4 && loads == 4 && bindings == 1);
    for (unsigned i = 0; i < 4; ++i) {
        REQUIRE(draws[i] == model[i]);
    }
    REQUIRE(graphics.polyOpa.p > opa); /* Bright eyes have their opaque draw. */
    unsigned bodyAlpha = 0;
    for (Gfx* command = xlu; command < graphics.polyXlu.p; ++command) {
        if ((command->words.w0 >> 24) == G_SETENVCOLOR) {
            bodyAlpha = command->words.w1 & 0xFF;
        }
    }
    REQUIRE(bodyAlpha == 255 && pushes == pops);

    for (missingPart = 1; missingPart <= 4; ++missingPart) {
        resetDraw();
        WandShadow_Draw(&play);
        REQUIRE(drawCount == 1 && bindings == 0 && pushes == pops);
        for (unsigned i = 0; i < 4; ++i) {
            REQUIRE(draws[0] != model[i]); /* No partial MM model graph is submitted. */
        }
    }
    missingPart = 0;
    resetDraw();
    WandShadow_Draw(&play);
    REQUIRE(drawCount == 4 && bindings == 1);
    WandShadow_Forget();
    resetDraw();
    WandShadow_Draw(&play);
    REQUIRE(drawCount == 0 && loads == 0);
    puts("PASS visible fallback, complete MM body/eyes, explicit alpha, recovery, and balanced matrices");
}

static void testHoming(void) {
    const f32 heights[] = { 0.0f, 50.0f, 160.0f, -80.0f };
    for (unsigned i = 0; i < ARRAY_COUNT(heights); ++i) {
        reset();
        enemy.focus.pos.y = heights[i];
        u8 cast = WandShadow_Cast(&player, &play);
        REQUIRE(cast);
        for (unsigned frame = 0; frame < 80 && sShadowBolt.active; ++frame) {
            WandShadow_Tick(&play);
        }
        REQUIRE(!sShadowBolt.active && enemy.freezeTimer == 120);
        REQUIRE(enemy.colorFilterTimer == 120 && enemy.colChkInfo.health == 5);
    }
    reset();
    farther = enemy;
    farther.focus.pos.z = farther.world.pos.z = 300.0f;
    farther.next = &enemy;
    play.actorCtx.actorLists[ACTORCAT_ENEMY].head = &farther;
    u8 cast = WandShadow_Cast(&player, &play);
    REQUIRE(cast && sShadowBolt.target == &enemy);
    for (unsigned frame = 0; frame < 80 && sShadowBolt.active; ++frame) {
        WandShadow_Tick(&play);
    }
    REQUIRE(enemy.freezeTimer == 120 && farther.freezeTimer == 0);
    puts("PASS stun connects at foot, torso, elevated and lower focus points; nearest target; no health damage");
}

static void testLifetime(void) {
    reset();
    u8 cast = WandShadow_Cast(&player, &play);
    REQUIRE(cast);
    u8 second = WandShadow_Cast(&player, &play);
    REQUIRE(!second);
    /* Model the retained pointer after its actor has been removed/freed. */
    play.actorCtx.actorLists[ACTORCAT_ENEMY].head = NULL;
    sShadowBolt.target = (Actor*)(uintptr_t)1;
    WandShadow_Tick(&play);
    REQUIRE(sShadowBolt.target == NULL && sShadowBolt.active);
    for (unsigned frame = 0; frame < 90; ++frame) {
        WandShadow_Tick(&play);
    }
    REQUIRE(!sShadowBolt.active);
    reset();
    enemy.focus.pos.z = 1000.0f;
    cast = WandShadow_Cast(&player, &play);
    REQUIRE(cast && sShadowBolt.target == NULL);
    WandShadow_Tick(&play);
    REQUIRE(sShadowBolt.pos.z > 30.0f && sShadowBolt.pos.y == 25.0f);
    WandShadow_Forget();
    REQUIRE(!sShadowBolt.active && sShadowBolt.target == NULL);
    puts("PASS one-shot limit, removed target safety, range, free flight, expiry and scene reset");
}

int main(void) {
    testRendering();
    testHoming();
    testLifetime();
    return 0;
}
