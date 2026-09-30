/**
 * wand_shadow.c — Shadow Scepter (Skijer's NEI).
 *
 * A black Boe, thrown at the nearest enemy, which it stuns and then disperses into.
 *
 * The Boe is Majora's Mask's En_Mkk, and it is unusually cheap to borrow: it has no skeleton and no
 * animations at all, just billboarded display lists. The bolt owns no actor or skeleton. Load its
 * complete display-list graphs through the MM archive loader; the global extension index is not
 * an authoritative inventory of that optional archive. A native shadow puff keeps casts visible
 * when the MM model is unavailable.
 */

#include "align_asset_macro.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"

#define dgBlackBoeBodyMaterialDL "__OTR__objects/object_mkk/gBlackBoeBodyMaterialDL"
static const ALIGN_ASSET(2) char gBlackBoeBodyMaterialDL[] = dgBlackBoeBodyMaterialDL;
#define dgBlackBoeBodyModelDL "__OTR__objects/object_mkk/gBlackBoeBodyModelDL"
static const ALIGN_ASSET(2) char gBlackBoeBodyModelDL[] = dgBlackBoeBodyModelDL;
#define dgBlackBoeEndDL "__OTR__objects/object_mkk/gBlackBoeEndDL"
static const ALIGN_ASSET(2) char gBlackBoeEndDL[] = dgBlackBoeEndDL;
#define dgBlackBoeEyesDL "__OTR__objects/object_mkk/gBlackBoeEyesDL"
static const ALIGN_ASSET(2) char gBlackBoeEyesDL[] = dgBlackBoeEyesDL;

#define SHADOW_SEEK_RANGE 460.0f
#define SHADOW_SPAWN_DIST 30.0f
#define SHADOW_SPAWN_HEIGHT 25.0f
#define SHADOW_SPEED 11.0f
#define SHADOW_HIT_RADIUS 22.0f
#define SHADOW_LIFE_FRAMES 90
#define SHADOW_STUN_FRAMES 120
#define SHADOW_SCALE 0.014f

// Actor_SetColorFilter's blue/stun tint. SoH's headers carry no COLORFILTER_* names, so the flags
// are spelled out the way the trutefel enemies do: 0x0000 blue, 0x4000 red, 0x8000 grey.
#define SHADOW_FILTER_BLUE 0x0000
#define SHADOW_FILTER_STRENGTH 0xF8

static const u8 sShadowEnemyCats[2] = { ACTORCAT_ENEMY, ACTORCAT_BOSS };

static struct {
    Vec3f pos;
    Actor* target;
    s16 yaw;
    s16 life;
    u8 active;
} sShadowBolt;

// Resource ownership/caching stays with the MM graph loader, which retains the vertices and
// textures as well as the lists. Do not permanently cache a failed lookup in the wand.
static Gfx* sShadowModel[4];

static u8 WandShadow_LoadModel(void) {
    static const char* paths[] = { gBlackBoeBodyMaterialDL, gBlackBoeBodyModelDL, gBlackBoeEndDL, gBlackBoeEyesDL };

    MmAssets_Init();
    if (!MmAssets_IsLoaded()) {
        return 0;
    }
    for (u8 i = 0; i < ARRAY_COUNT(paths); i++) {
        sShadowModel[i] = MmAssets_LoadDisplayListGraphStrict(paths[i]);
        if (sShadowModel[i] == NULL) {
            return 0;
        }
    }
    return 1;
}

// Self-contained fallback: a soft dark billboard with bright eyes. No MM archive, external
// texture, or Alt resource is needed to see where a successfully cast projectile is travelling.
static Vtx sShadowFallbackVtx[] = {
    { { { 0, 0, 0 }, 0, { 0, 0 }, { 18, 10, 26, 245 } } },
    { { { -14, 0, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { -10, 10, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { 0, 14, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { 10, 10, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { 14, 0, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { 10, -10, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { 0, -14, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { -10, -10, 0 }, 0, { 0, 0 }, { 36, 20, 52, 0 } } },
    { { { -6, 1, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { -4, 4, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { -2, 1, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { -4, -2, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { 2, 1, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { 4, 4, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { 6, 1, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
    { { { 4, -2, 1 }, 0, { 0, 0 }, { 255, 240, 160, 255 } } },
};

static Gfx sShadowFallbackDL[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BOTH | G_FOG),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPVertex(sShadowFallbackVtx, ARRAY_COUNT(sShadowFallbackVtx), 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(0, 3, 4, 0, 0, 4, 5, 0),
    gsSP2Triangles(0, 5, 6, 0, 0, 6, 7, 0),
    gsSP2Triangles(0, 7, 8, 0, 0, 8, 1, 0),
    gsSP2Triangles(9, 10, 11, 0, 9, 11, 12, 0),
    gsSP2Triangles(13, 14, 15, 0, 13, 15, 16, 0),
    gsSPEndDisplayList(),
};

// Use the same point for seeking and hitting. Comparing a bolt 25 units above the floor with
// enemy foot origins and a 22-unit hit radius made level-ground casts miss without exception.
static Actor* WandShadow_FindTarget(PlayState* play, Vec3f* pos, f32 range) {
    Actor* closest = NULL;
    f32 closestDistSq = range * range;

    for (u8 i = 0; i < ARRAY_COUNT(sShadowEnemyCats); i++) {
        for (Actor* enemy = play->actorCtx.actorLists[sShadowEnemyCats[i]].head; enemy != NULL; enemy = enemy->next) {
            f32 dx = enemy->focus.pos.x - pos->x;
            f32 dy = enemy->focus.pos.y - pos->y;
            f32 dz = enemy->focus.pos.z - pos->z;
            f32 distSq = (dx * dx) + (dy * dy) + (dz * dz);

            if ((enemy->update != NULL) && (distSq < closestDistSq)) {
                closest = enemy;
                closestDistSq = distSq;
            }
        }
    }
    return closest;
}

static u8 WandShadow_TargetIsAlive(PlayState* play, Actor* target) {
    for (u8 i = 0; i < ARRAY_COUNT(sShadowEnemyCats); i++) {
        for (Actor* enemy = play->actorCtx.actorLists[sShadowEnemyCats[i]].head; enemy != NULL; enemy = enemy->next) {
            if (enemy == target) {
                return enemy->update != NULL;
            }
        }
    }
    return 0;
}

// A frozen enemy is the Deku Nut effect: the freeze stops its update, the colour filter is what
// makes it read as stunned. Same recipe as the Divine Shield's parry.
static void WandShadow_Stun(Actor* enemy) {
    enemy->freezeTimer = SHADOW_STUN_FRAMES;
    Actor_SetColorFilter(enemy, SHADOW_FILTER_BLUE, SHADOW_FILTER_STRENGTH, 0, SHADOW_STUN_FRAMES);
}

void WandShadow_Forget(void) {
    sShadowBolt.active = 0;
    sShadowBolt.target = NULL;
}

// Validate against live lists before dereferencing a retained target: it may have been removed
// since the last frame. Home in all three axes so focus points above/below the launch can be hit.
void WandShadow_Tick(PlayState* play) {
    Actor* hit;

    if (!sShadowBolt.active) {
        return;
    }
    if (--sShadowBolt.life <= 0) {
        WandShadow_Forget();
        return;
    }

    if ((sShadowBolt.target != NULL) && !WandShadow_TargetIsAlive(play, sShadowBolt.target)) {
        sShadowBolt.target = WandShadow_FindTarget(play, &sShadowBolt.pos, SHADOW_SEEK_RANGE);
    }
    if (sShadowBolt.target != NULL) {
        Vec3f* aim = &sShadowBolt.target->focus.pos;
        f32 dx = aim->x - sShadowBolt.pos.x;
        f32 dy = aim->y - sShadowBolt.pos.y;
        f32 dz = aim->z - sShadowBolt.pos.z;
        f32 distance = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
        f32 step = (distance > SHADOW_SPEED) ? (SHADOW_SPEED / distance) : 1.0f;

        sShadowBolt.yaw = Math_Vec3f_Yaw(&sShadowBolt.pos, aim);
        sShadowBolt.pos.x += dx * step;
        sShadowBolt.pos.y += dy * step;
        sShadowBolt.pos.z += dz * step;
    } else {
        sShadowBolt.pos.x += Math_SinS(sShadowBolt.yaw) * SHADOW_SPEED;
        sShadowBolt.pos.z += Math_CosS(sShadowBolt.yaw) * SHADOW_SPEED;
    }

    hit = WandShadow_FindTarget(play, &sShadowBolt.pos, SHADOW_HIT_RADIUS);
    if (hit != NULL) {
        WandShadow_Stun(hit);
        Audio_PlaySoundGeneral(NA_SE_EN_GANON_DARKWAVE, &sShadowBolt.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        WandShadow_Forget();
    }
}

void WandShadow_Draw(PlayState* play) {
    u8 hasModel;

    if (!sShadowBolt.active) {
        return;
    }
    hasModel = WandShadow_LoadModel();

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(sShadowBolt.pos.x, sShadowBolt.pos.y, sShadowBolt.pos.z, MTXMODE_NEW);
    // The Boe is a flat billboard in MM too — its own draw replaces the rotation the same way.
    Matrix_ReplaceRotation(&play->billboardMtxF);
    if (hasModel) {
        Mtx* matrix;

        MmAssets_EnsureStrictTextureBindings();
        Matrix_Scale(SHADOW_SCALE, SHADOW_SCALE, SHADOW_SCALE, MTXMODE_APPLY);
        matrix = Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__);

        // MM's EnMkk explicitly sets body alpha and draws its eyes separately. Inheriting alpha
        // from the previous translucent draw can make the entire body disappear.
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)gEmptyDL);
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, sShadowModel[0]);
        gSPDisplayList(POLY_XLU_DISP++, sShadowModel[1]);
        gSPDisplayList(POLY_XLU_DISP++, sShadowModel[2]);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 255, 255, 255, 255, 255);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)MmAssets_GetOpaqueRenderMode());
        gSPMatrix(POLY_OPA_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, sShadowModel[3]);
    } else {
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, sShadowFallbackDL);
    }
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

u8 WandShadow_Cast(Player* player, PlayState* play) {
    s16 yaw = player->actor.shape.rot.y;

    if (sShadowBolt.active) {
        return 0; // one bolt at a time
    }

    sShadowBolt.pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * SHADOW_SPAWN_DIST);
    sShadowBolt.pos.y = player->actor.world.pos.y + SHADOW_SPAWN_HEIGHT;
    sShadowBolt.pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * SHADOW_SPAWN_DIST);

    // No enemy in reach is not a failed cast: the bolt flies off Link's nose and fades on its timer.
    sShadowBolt.target = WandShadow_FindTarget(play, &sShadowBolt.pos, SHADOW_SEEK_RANGE);
    sShadowBolt.yaw = yaw;
    sShadowBolt.life = SHADOW_LIFE_FRAMES;
    sShadowBolt.active = 1;

    // NOT NA_SE_EN_GANON_DARKWAVE_M: vanilla only ever plays that one as `- SFX_FLAG`, the
    // continuous variant re-issued every frame, so started raw it would never stop.
    Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REFLECT_MG, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    return 1;
}
