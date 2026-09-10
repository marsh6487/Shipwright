/*
 * File: z_en_viewer.c
 * Overlay: ovl_En_Viewer
 * Description: Cutscene Actors
 */

#include "z_en_viewer.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"
#include "objects/object_zl4/object_zl4.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_horse_zelda/object_horse_zelda.h"
#include "objects/object_horse_ganon/object_horse_ganon.h"
#include "objects/object_im/object_im.h"
#include "objects/object_gndd/object_gndd.h"
#include "objects/object_gnd/object_gnd.h"
#include "objects/object_ganon/object_ganon.h"
#include "objects/object_opening_demo1/object_opening_demo1.h"
#include "objects/object_ma1/object_ma1.h"
#include "objects/object_sa/object_sa.h"
#include "objects/object_xc/object_xc.h"
#include "objects/object_ru2/object_ru2.h"
#include "objects/object_ru1/object_ru1.h"
#include "objects/object_kw1/object_kw1.h"
#include "objects/object_fa/object_fa.h"
#include "objects/object_ma2/object_ma2.h"
#include "objects/object_zl2/object_zl2.h"
#include "objects/object_zl2_anime2/object_zl2_anime2.h"
#include "objects/object_du/object_du.h"
#include "objects/object_nb/object_nb.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "objects/object_os_anime/object_os_anime.h"
#include "static_story_actor.h"
#include "static_story_kokiri.h"
#include "static_story_mm_actor.h"
#include "static_story_ganon.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
#include "soh/frame_interpolation.h"
#include <assert.h>

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_ATTENTION_ENABLED)

static ColliderCylinderInit sStaticCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 18, 46, 0, { 0, 0, 0 } },
};

void EnViewer_Init(Actor* thisx, PlayState* play);
void EnViewer_Destroy(Actor* thisx, PlayState* play);
void EnViewer_Update(Actor* thisx, PlayState* play);
void EnViewer_Draw(Actor* thisx, PlayState* play);

void EnViewer_UpdatePosition(EnViewer* this, PlayState* play);
void EnViewer_DrawFireEffects(EnViewer* this2, PlayState* play);
void EnViewer_UpdateGanondorfCape(PlayState* play, EnViewer* this);
void EnViewer_InitImpl(EnViewer* this, PlayState* play);
void EnViewer_UpdateImpl(EnViewer* this, PlayState* play);
void EnViewerStatic_Init(EnViewer* this, PlayState* play);
void EnViewerStatic_WaitForObjects(EnViewer* this, PlayState* play);
void EnViewerStatic_Update(EnViewer* this, PlayState* play);
void EnViewerStatic_Draw(EnViewer* this, PlayState* play);
void EnViewerStatic_OfferTalk(EnViewer* this, PlayState* play);
static void EnViewerStatic_UpdateTracking(EnViewer* this, PlayState* play);
static void EnViewerStatic_InitRutoWater(EnViewer* this, PlayState* play,
                                         const StaticStoryPoseDescriptor* poseDescriptor);
static bool EnViewerStatic_UpdateRutoWater(EnViewer* this, PlayState* play, bool animationEnded);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
void ActorCatalogue_LogLifecycle(const char* stage, int params, int type, int pose, int modelObjectId, int modelSlot,
                                 int animationObjectId, int animationSlot);

static u8 sHorseSfxPlayed = false;

const ActorInit En_Viewer_InitVars = {
    ACTOR_EN_VIEWER,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_GAMEPLAY_KEEP,
    sizeof(EnViewer),
    (ActorFunc)EnViewer_Init,
    (ActorFunc)EnViewer_Destroy,
    (ActorFunc)EnViewer_Update,
    (ActorFunc)EnViewer_Draw,
    NULL,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneScale, 300, ICHAIN_STOP),
};

static EnViewerInitData sInitData[] = {
    /* ENVIEWER_TYPE_0_HORSE_ZELDA */
    { OBJECT_HORSE_ZELDA, OBJECT_HORSE_ZELDA, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseZeldaSkel,
      &gHorseZeldaGallopingAnim },
    /* ENVIEWER_TYPE_1_IMPA */
    { OBJECT_IM, OBJECT_OPENING_DEMO1, 1, 0, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_IMPA, &gImpaSkel,
      &object_opening_demo1_Anim_0029CC },
    /* ENVIEWER_TYPE_2_ZELDA */
    { OBJECT_ZL4, OBJECT_OPENING_DEMO1, 1, 0, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_ZELDA, &gChildZeldaSkel,
      &object_opening_demo1_Anim_000450 },
    /* ENVIEWER_TYPE_3_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfHorsebackIdleAnim },
    /* ENVIEWER_TYPE_4_HORSE_GANONDORF */
    { OBJECT_HORSE_GANON, OBJECT_HORSE_GANON, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseGanonSkel,
      &gHorseGanonRearingAnim },
    /* ENVIEWER_TYPE_5_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfHorsebackRideAnim },
    /* ENVIEWER_TYPE_6_HORSE_GANONDORF */
    { OBJECT_HORSE_GANON, OBJECT_HORSE_GANON, 1, 0, ENVIEWER_SHADOW_HORSE, 20, ENVIEWER_DRAW_HORSE, &gHorseGanonSkel,
      &gHorseGanonGallopingAnim },
    /* ENVIEWER_TYPE_7_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfArmsCrossedAnim },
    /* ENVIEWER_TYPE_8_GANONDORF */
    { OBJECT_GNDD, OBJECT_GNDD, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gYoungGanondorfSkel,
      &gYoungGanondorfWalkAnim },
    /* ENVIEWER_TYPE_9_GANONDORF */
    { OBJECT_GANON, OBJECT_GANON, 1, -6, ENVIEWER_SHADOW_NONE, 10, ENVIEWER_DRAW_GANONDORF, &gGanondorfSkel,
      &gGanondorfEndingFloatAnim },
};

static EnGanonMant* sGanondorfCape;

static Vec3f sGanondorfNeckWorldPos;

void EnViewer_SetupAction(EnViewer* this, EnViewerActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnViewer_Init(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;
    StaticStoryActorType staticType;
    u8 type;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    sHorseSfxPlayed = false;
    this->staticState.type = STATIC_STORY_ACTOR_NONE;
    this->staticState.initialized = false;
    this->staticState.staticMode = StaticStoryActor_IsParam(this->actor.params);
    if (StaticStoryActor_IsParam(this->actor.params)) {
        staticType = StaticStoryActor_GetType(this->actor.params);
        if (staticType == STATIC_STORY_ACTOR_NONE || !StaticStoryActor_IsAvailable(staticType)) {
            osSyncPrintf("Static story actor: invalid or unavailable type %d\n", staticType);
            Actor_Kill(&this->actor);
            return;
        }
        EnViewerStatic_Init(this, play);
        return;
    }

    EnViewer_SetupAction(this, EnViewer_InitImpl);
    type = (u16)this->actor.params >> 8;
    this->unused = 0;
    this->state = 0;
    this->isVisible = false;
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF || type == ENVIEWER_TYPE_7_GANONDORF ||
        type == ENVIEWER_TYPE_8_GANONDORF || type == ENVIEWER_TYPE_9_GANONDORF) {
        sGanondorfCape = (EnGanonMant*)Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_GANON_MANT,
                                                          0.0f, 0.0f, 0.0f, 0, 0, 0, 35);
    }
}

void EnViewer_Destroy(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;

    if (this->staticState.staticMode) {
        if (this->staticState.initialized) {
            if (this->staticState.tatlInitialized) {
                SkelAnime_Free(&this->staticState.tatlSkelAnime, play);
                this->staticState.tatlInitialized = false;
            }
            if (StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose)
                    ->skeletonFamily != STATIC_SKELETON_NONE) {
                SkelAnime_Free(&this->skin.skelAnime, play);
            }
            Collider_DestroyCylinder(play, &this->staticState.collider);
        }
        return;
    }
    Skin_Free(play, &this->skin);
}

void EnViewer_InitAnimGanondorfOrZelda(EnViewer* this, PlayState* play, void* skeletonHeaderSeg,
                                       AnimationHeader* anim) {
    s16 type = this->actor.params >> 8;

    if (type == ENVIEWER_TYPE_2_ZELDA || type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF ||
        type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF || type == ENVIEWER_TYPE_9_GANONDORF) {
        SkelAnime_InitFlex(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    } else {
        SkelAnime_Init(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    }

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF ||
        type == ENVIEWER_TYPE_9_GANONDORF) {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 1.0f);
    } else {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
    }
}

void EnViewer_InitAnimImpa(EnViewer* this, PlayState* play, void* skeletonHeaderSeg, AnimationHeader* anim) {
    SkelAnime_InitFlex(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
}

void EnViewer_InitAnimStatic(EnViewer* this, PlayState* play, void* skeletonHeaderSeg, AnimationHeader* anim) {
    SkelAnime_InitFlex(play, &this->skin.skelAnime, skeletonHeaderSeg, NULL, NULL, NULL, 0);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 1.0f);
}

void EnViewer_InitAnimHorse(EnViewer* this, PlayState* play, void* skeletonHeaderSeg, AnimationHeader* anim) {
    u8 type;

    Skin_Init(play, &this->skin, skeletonHeaderSeg, anim);
    type = this->actor.params >> 8;
    if (!(type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_4_HORSE_GANONDORF ||
          type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF ||
          type == ENVIEWER_TYPE_9_GANONDORF)) {
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, anim, 3.0f);
    } else {
        Animation_PlayOnceSetSpeed(&this->skin.skelAnime, anim, 1.0f);
    }
}

static EnViewerInitAnimFunc sInitAnimFuncs[] = {
    EnViewer_InitAnimGanondorfOrZelda, EnViewer_InitAnimHorse,
    EnViewer_InitAnimGanondorfOrZelda, EnViewer_InitAnimImpa,
    EnViewer_InitAnimStatic,           EnViewer_InitAnimStatic,
};

static ActorShadowFunc sShadowDrawFuncs[] = {
    NULL,
    ActorShadow_DrawCircle,
    ActorShadow_DrawHorse,
};

static AnimationHeader* EnViewerStatic_GetAnimation(uint16_t animation) {
    switch (animation) {
        case STATIC_ANIM_IMPA_IDLE:
            return &gImpaIdleAnim;
        case STATIC_ANIM_MALON_IDLE:
            return &gMalonChildIdleAnim;
        case STATIC_ANIM_MALON_SING:
            return &gMalonChildSingAnim;
        case STATIC_ANIM_SARIA_ARMS_TO_SIDE:
            return &gSariaWaitArmsToSideAnim;
        case STATIC_ANIM_SARIA_HANDS_BEHIND:
            return &gSariaHandsBehindBackWaitAnim;
        case STATIC_ANIM_SARIA_OCARINA:
            return &gSariaPlayingOcarinaAnim;
        case STATIC_ANIM_SARIA_SEATED:
            return &gSariaSitting3Anim;
        case STATIC_ANIM_SHEIK_IDLE:
            return &gSheikIdleAnim;
        case STATIC_ANIM_SHEIK_ARMS_CROSSED:
            return &gSheikArmsCrossedIdleAnim;
        case STATIC_ANIM_SHEIK_HARP:
            return &gSheikPlayingHarp5Anim;
        case STATIC_ANIM_ADULT_RUTO_IDLE:
            return &gAdultRutoIdleAnim;
        case STATIC_ANIM_ADULT_RUTO_HANDS_HIPS:
            return &gAdultRutoIdleHandsOnHipsAnim;
        case STATIC_ANIM_ADULT_RUTO_LOOK_DOWN_LEFT:
            return &gAdultRutoLookingDownLeftAnim;
        case STATIC_ANIM_CHILD_RUTO_HANDS_BEHIND:
            return &gRutoChildWaitHandsBehindBackAnim;
        case STATIC_ANIM_CHILD_RUTO_HANDS_HIPS:
            return &gRutoChildWaitHandsOnHipsAnim;
        case STATIC_ANIM_CHILD_RUTO_SITTING:
            return &gRutoChildSittingAnim;
        case STATIC_ANIM_KOKIRI_IDLE:
            return &gKokiriIdleAnim;
        case STATIC_ANIM_KOKIRI_ARMS_BEHIND:
            return &gKokiriStandingArmsBehindBackAnim;
        case STATIC_ANIM_KOKIRI_HANDS_HIPS:
            return &gKokiriStandingHandsOnHipsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND:
            return &gKokiriSittingHeadOnHandAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS:
            return &gKokiriSittingCrossedLegsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS:
            return &gKokiriSittingCrossedArmsLegsAnim;
        case STATIC_ANIM_ADULT_MALON_IDLE:
            return &gMalonAdultIdleAnim;
        case STATIC_ANIM_ADULT_MALON_BASKET:
            return &gMalonAdultStandStillAnim;
        case STATIC_ANIM_ADULT_MALON_SING:
            return &gMalonAdultSingAnim;
        case STATIC_ANIM_DARUNIA_IDLE:
            return &gDaruniaIdleAnim;
        case STATIC_ANIM_DARUNIA_DANCE_1:
            return &gDaruniaDancingLoop1Anim;
        case STATIC_ANIM_DARUNIA_DANCE_2:
            return &gDaruniaDancingLoop2Anim;
        case STATIC_ANIM_DARUNIA_DANCE_3:
            return &gDaruniaDancingLoop3Anim;
        case STATIC_ANIM_DARUNIA_DANCE_4:
            return &gDaruniaDancingLoop4Anim;
        case STATIC_ANIM_NABOORU_IDLE:
            return &gNabooruStandingHandsOnHipsAnim;
        case STATIC_ANIM_GREAT_FAIRY_SITTING:
            return &gGreatFairySittingAnim;
        case STATIC_ANIM_GREAT_FAIRY_LAYING:
            return &gGreatFairyLayingSidewaysAnim;
        case STATIC_ANIM_GREAT_FAIRY_AFTER_SPELL:
            return &gGreatFairyAfterSpellAnim;
        case STATIC_ANIM_PHANTOM_GANON_NEUTRAL:
            return &gPhantomGanonNeutralAnim;
        case STATIC_ANIM_ADULT_ZELDA_IDLE:
            return &gZelda2Anime2Anim_009FBC;
        default:
            return NULL;
    }
}

static void EnViewerStatic_InitSkeleton(EnViewer* this, PlayState* play,
                                        const StaticStoryPoseDescriptor* poseDescriptor) {
    switch (poseDescriptor->skeletonFamily) {
        case STATIC_SKELETON_IMPA:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gImpaSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_MALON_CHILD:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gMalonChildSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_SARIA:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gSariaSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_SHEIK:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gSheikSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_ADULT_RUTO:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gAdultRutoSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_CHILD_RUTO:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gRutoChildSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_MALON_ADULT:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gMalonAdultSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_ADULT_ZELDA:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gZelda2Skel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_DARUNIA:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gDaruniaSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_NABOORU:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gNabooruSkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_GREAT_FAIRY:
            SkelAnime_InitFlex(play, &this->skin.skelAnime, &gGreatFairySkel, NULL, NULL, NULL, 0);
            break;
        case STATIC_SKELETON_PHANTOM_GANON:
            SkelAnime_Init(play, &this->skin.skelAnime, &gPhantomGanonSkel, NULL, NULL, NULL, 0);
            break;
        default:
            return;
    }
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, EnViewerStatic_GetAnimation(poseDescriptor->animation),
                               poseDescriptor->playbackSpeed);
}

static void EnViewerStatic_SetDaruniaDanceStep(EnViewer* this, uint8_t step) {
    static AnimationHeader* sDanceAnimations[] = {
        &gDaruniaDancingLoop1Anim,
        &gDaruniaDancingLoop2Anim,
        &gDaruniaDancingLoop3Anim,
        &gDaruniaDancingLoop4Anim,
    };
    static const float sDanceSpeeds[] = { 0.78f, 0.78f, 0.77f, 0.78f };

    this->staticState.danceStep = step % 4;
    Animation_Change(&this->skin.skelAnime, sDanceAnimations[this->staticState.danceStep],
                     sDanceSpeeds[this->staticState.danceStep], 0.0f,
                     Animation_GetLastFrame(sDanceAnimations[this->staticState.danceStep]), ANIMMODE_ONCE, 0.0f);
}

void EnViewerStatic_Init(EnViewer* this, PlayState* play) {
    StaticStoryActorType type = StaticStoryActor_GetType(this->actor.params);
    StaticStoryObjectRequirements objects = StaticStoryActor_GetObjectRequirements(type);

    StaticStoryActor_NormalizePlacementRotation(&this->actor.world.rot.x, &this->actor.world.rot.y,
                                                 &this->actor.world.rot.z);
    this->actor.shape.rot.x = this->actor.world.rot.x;
    this->actor.shape.rot.z = this->actor.world.rot.z;
    this->actor.home.rot.x = this->actor.world.rot.x;
    this->actor.home.rot.z = this->actor.world.rot.z;

    this->staticState.type = type;
    this->staticState.pose = StaticStoryActor_SanitizePose(type, StaticStoryActor_GetPose(this->actor.params));
    if (this->staticState.pose != StaticStoryActor_GetPose(this->actor.params)) {
        osSyncPrintf("Static story actor: unsupported pose %d; using pose 0\n",
                     StaticStoryActor_GetPose(this->actor.params));
    }
    this->staticState.objectSlots[0] = -1;
    this->staticState.objectSlots[1] = -1;
    this->staticState.objectSlots[2] = -1;
    this->staticState.objectSlots[3] = -1;
    this->staticState.previousRutoWaterPhase = STATIC_RUTO_PHASE_GROUNDED;
    this->staticState.danceStep = 0;
    this->staticState.diagnosticDrawLogged = false;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    ActorCatalogue_LogLifecycle("init", this->actor.params, type, this->staticState.pose, objects.modelObjectId, -1,
                                objects.animationObjectId, -1);
    EnViewer_SetupAction(this, EnViewerStatic_WaitForObjects);
}

static bool EnViewerStatic_QueryWater(EnViewer* this, PlayState* play, float* surfaceY) {
    WaterBox* waterBox = NULL;

    return WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, surfaceY,
                                &waterBox) != 0;
}

static void EnViewerStatic_SetRutoWaterAnimation(EnViewer* this, StaticRutoWaterPhase phase) {
    float lastFrame = Animation_GetLastFrame(&gAdultRutoSwimmingUpAnim);

    switch (phase) {
        case STATIC_RUTO_PHASE_PREPARING_DIVE:
            Animation_Change(&this->skin.skelAnime, &gAdultRutoSwimmingUpAnim, -1.0f, lastFrame, 0.0f, ANIMMODE_ONCE,
                             -4.0f);
            break;
        case STATIC_RUTO_PHASE_DIVING:
            this->skin.skelAnime.playSpeed = 0.0f;
            break;
        case STATIC_RUTO_PHASE_RISING:
            Animation_Change(&this->skin.skelAnime, &gAdultRutoSwimmingUpAnim, 1.0f, 0.0f, lastFrame, ANIMMODE_ONCE,
                             -4.0f);
            break;
        case STATIC_RUTO_PHASE_GROUNDED:
        case STATIC_RUTO_PHASE_SURFACED:
            Animation_PlayLoop(&this->skin.skelAnime, &gAdultRutoIdleAnim);
            break;
        case STATIC_RUTO_PHASE_SUBMERGED:
        default:
            break;
    }
}

static void EnViewerStatic_InitRutoWater(EnViewer* this, PlayState* play,
                                         const StaticStoryPoseDescriptor* poseDescriptor) {
    float surfaceY = 0.0f;
    bool hasWater = EnViewerStatic_QueryWater(this, play, &surfaceY);

    StaticRutoWater_Init(&this->staticState.rutoWater, (StaticRutoWaterMode)poseDescriptor->waterMode, hasWater,
                         this->actor.home.pos.y, surfaceY);
    this->actor.world.pos.y = this->staticState.rutoWater.currentY;
    this->actor.velocity.y = this->staticState.rutoWater.velocityY;
    this->staticState.previousRutoWaterPhase = this->staticState.rutoWater.phase;
    EnViewerStatic_SetRutoWaterAnimation(this, this->staticState.rutoWater.phase);
}

static bool EnViewerStatic_UpdateRutoWater(EnViewer* this, PlayState* play, bool animationEnded) {
    float surfaceY = 0.0f;
    bool hasWater = EnViewerStatic_QueryWater(this, play, &surfaceY);
    bool playerNear = false;
    uint16_t diveDelay = Rand_S16Offset(40, 40);
    StaticRutoWaterEvents events;
    StaticRutoWaterPhase phase;

    if (hasWater) {
        Player* player = GET_PLAYER(play);
        float yDist = fabsf(player->actor.world.pos.y - surfaceY);

        playerNear = this->actor.xzDistToPlayer < 240.0f && yDist < 80.0f;
    }
    events = StaticRutoWater_Update(&this->staticState.rutoWater, hasWater, surfaceY, playerNear, animationEnded,
                                    diveDelay);
    phase = this->staticState.rutoWater.phase;
    if (phase != this->staticState.previousRutoWaterPhase) {
        osSyncPrintf("[ActorCatalogueProbe] Ruto params=%04X phase=%d->%d y=%.2f surface=%.2f water=%d\n",
                     (uint16_t)this->actor.params, this->staticState.previousRutoWaterPhase, phase,
                     this->staticState.rutoWater.currentY, surfaceY, hasWater);
        EnViewerStatic_SetRutoWaterAnimation(this, phase);
        this->staticState.previousRutoWaterPhase = phase;
    }

    if (events != STATIC_RUTO_WATER_EVENT_NONE) {
        Vec3f waterPos = this->actor.world.pos;

        waterPos.y = this->staticState.rutoWater.surfaceY;
        if (events & STATIC_RUTO_WATER_EVENT_EMERGED) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_OUT_OF_WATER);
            EffectSsGSplash_Spawn(play, &waterPos, NULL, NULL, 1, 700);
            EffectSsGRipple_Spawn(play, &waterPos, 100, 500, 0);
        }
        if (events & STATIC_RUTO_WATER_EVENT_DIVE) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_DIVE_WATER);
            EffectSsGSplash_Spawn(play, &waterPos, NULL, NULL, 1, 500);
            EffectSsGRipple_Spawn(play, &waterPos, 100, 500, 0);
        }
        if (events & STATIC_RUTO_WATER_EVENT_RIPPLE) {
            EffectSsGRipple_Spawn(play, &waterPos, 20, 100, 0);
        }
    }

    this->actor.world.pos.y = this->staticState.rutoWater.currentY;
    this->actor.velocity.y = this->staticState.rutoWater.velocityY;
    this->isVisible = this->staticState.rutoWater.alpha != 0;
    if (phase == STATIC_RUTO_PHASE_GROUNDED || phase == STATIC_RUTO_PHASE_SURFACED) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        return true;
    }

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->staticState.talking = false;
    this->staticState.tracking = false;
    this->staticState.greatFairyHoverPhase = 0;
    return false;
}

void EnViewerStatic_WaitForObjects(EnViewer* this, PlayState* play) {
    const StaticStoryActorDefinition* definition =
        StaticStoryActor_GetDefinition((StaticStoryActorType)this->staticState.type);
    const StaticStoryPoseDescriptor* poseDescriptor =
        StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose);
    StaticStoryObjectRequirements objects =
        StaticStoryActor_GetObjectRequirements((StaticStoryActorType)this->staticState.type);
    int16_t objectSlot = this->staticState.objectSlots[0];
    bool usesMmAssets = StaticStoryActor_GetResourceSource((StaticStoryActorType)this->staticState.type) ==
                        STATIC_STORY_RESOURCE_MM_ARCHIVE;

    if (definition == NULL || !definition->available || poseDescriptor == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    if (usesMmAssets && !this->staticState.initialized) {
        const StaticStoryMmPresentation* presentation =
            StaticStoryMm_GetPresentation((StaticStoryActorType)this->staticState.type, this->staticState.pose);
        FlexSkeletonHeader* skeleton;
        AnimationHeader* animation;
        FlexSkeletonHeader* secondarySkeleton = NULL;
        AnimationHeader* secondaryAnimation = NULL;
        bool secondaryComplete = false;

        MmAssets_Init();
        skeleton = (FlexSkeletonHeader*)MmAssets_LoadSkeleton(presentation != NULL ? presentation->skeletonPath : "");
        animation =
            (AnimationHeader*)MmAssets_LoadAnimation(presentation != NULL ? presentation->animationPath : "");
        if (presentation != NULL && presentation->requiresSecondarySkeleton) {
            secondarySkeleton = (FlexSkeletonHeader*)MmAssets_LoadSkeleton(presentation->secondarySkeletonPath);
            secondaryAnimation = (AnimationHeader*)MmAssets_LoadAnimation(presentation->secondaryAnimationPath);
            this->staticState.skullKidMaskDL = (Gfx*)MmAssets_LoadResource(presentation->maskDisplayListPath);
            this->staticState.skullKidHeadDL = (Gfx*)MmAssets_LoadResource(presentation->headDisplayListPath);
            this->staticState.skullKidEyesDL = (Gfx*)MmAssets_LoadResource(presentation->eyesDisplayListPath);
            secondaryComplete = secondarySkeleton != NULL && secondaryAnimation != NULL &&
                                this->staticState.skullKidMaskDL != NULL && this->staticState.skullKidHeadDL != NULL &&
                                this->staticState.skullKidEyesDL != NULL;
            for (int limb = 0; limb < 6 && secondaryComplete; ++limb) {
                StandardLimb* sourceLimb =
                    (StandardLimb*)MmAssets_LoadResource(StaticStoryMm_GetTatlLimbPath(limb));
                Gfx* sourceDList = (Gfx*)MmAssets_LoadResource(StaticStoryMm_GetTatlDListPath(limb));

                secondaryComplete = sourceLimb != NULL && sourceDList != NULL;
                if (secondaryComplete) {
                    this->staticState.tatlLimbs[limb] = *sourceLimb;
                    this->staticState.tatlLimbs[limb].dList = sourceDList;
                    this->staticState.tatlLimbPtrs[limb] = &this->staticState.tatlLimbs[limb];
                }
            }
        }
        if (!StaticStoryMm_ResourcesComplete(presentation, skeleton != NULL, animation != NULL,
                                              secondaryComplete)) {
            osSyncPrintf("Static story actor: MM resources unavailable for type %d pose %d\n",
                         this->staticState.type, this->staticState.pose);
            Actor_Kill(&this->actor);
            return;
        }
        SkelAnime_InitFlex(play, &this->skin.skelAnime, skeleton, NULL, NULL, NULL, 0);
        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, animation, poseDescriptor->playbackSpeed);
        if (presentation->requiresSecondarySkeleton) {
            this->staticState.tatlSkeleton = *secondarySkeleton;
            this->staticState.tatlSkeleton.sh.segment = this->staticState.tatlLimbPtrs;
            this->staticState.tatlSkeleton.sh.limbCount = 6;
            SkelAnime_Init(play, &this->staticState.tatlSkelAnime, &this->staticState.tatlSkeleton.sh,
                           secondaryAnimation, NULL, NULL, 0);
            this->staticState.tatlInitialized = true;
        }
    } else if (objectSlot < 0) {
        if ((this->staticState.type == STATIC_STORY_ACTOR_FADO ||
             this->staticState.type == STATIC_STORY_ACTOR_KOKIRI_GIRL) &&
            !StaticStoryKokiri_RequestObjects(this, play)) {
            osSyncPrintf("Static story actor: Kokiri object request failed for type %d\n", this->staticState.type);
            Actor_Kill(&this->actor);
            return;
        } else if (this->staticState.type != STATIC_STORY_ACTOR_FADO &&
                   this->staticState.type != STATIC_STORY_ACTOR_KOKIRI_GIRL) {
            this->staticState.objectSlots[0] = Object_GetIndex(&play->objectCtx, objects.modelObjectId);
            if (this->staticState.objectSlots[0] < 0) {
                this->staticState.objectSlots[0] = Object_Spawn(&play->objectCtx, objects.modelObjectId);
            }
            if (this->staticState.objectSlots[0] < 0) {
                osSyncPrintf("Static story actor: object request failed for type %d\n", this->staticState.type);
                Actor_Kill(&this->actor);
                return;
            }
        }
        objectSlot = this->staticState.objectSlots[0];
    }
    if (!usesMmAssets && this->staticState.type != STATIC_STORY_ACTOR_FADO &&
        this->staticState.type != STATIC_STORY_ACTOR_KOKIRI_GIRL && this->staticState.objectSlots[1] < 0) {
        if (objects.animationObjectId == objects.modelObjectId) {
            this->staticState.objectSlots[1] = objectSlot;
        } else {
            this->staticState.objectSlots[1] = Object_GetIndex(&play->objectCtx, objects.animationObjectId);
        }
        if (this->staticState.objectSlots[1] < 0) {
            this->staticState.objectSlots[1] = Object_Spawn(&play->objectCtx, objects.animationObjectId);
        }
        if (this->staticState.objectSlots[1] < 0) {
            osSyncPrintf("Static story actor: animation object request failed for type %d\n", this->staticState.type);
            Actor_Kill(&this->actor);
            return;
        }
    }
    ActorCatalogue_LogLifecycle("objects", this->actor.params, this->staticState.type, this->staticState.pose,
                                objects.modelObjectId, this->staticState.objectSlots[0], objects.animationObjectId,
                                this->staticState.objectSlots[1]);
    for (int slot = 0; !usesMmAssets && slot < 4 && this->staticState.objectSlots[slot] >= 0; ++slot) {
        if (!Object_IsLoaded(&play->objectCtx, this->staticState.objectSlots[slot])) {
            this->actor.flags &= ~ACTOR_FLAG_INSIDE_CULLING_VOLUME;
            return;
        }
    }

    if (!usesMmAssets) {
        this->actor.objBankIndex =
            (this->staticState.type == STATIC_STORY_ACTOR_FADO ||
             this->staticState.type == STATIC_STORY_ACTOR_KOKIRI_GIRL)
                ? this->staticState.objectSlots[2]
                : objectSlot;
        this->animObjBankIndex = (this->staticState.type == STATIC_STORY_ACTOR_FADO ||
                                    this->staticState.type == STATIC_STORY_ACTOR_KOKIRI_GIRL)
                                     ? this->staticState.objectSlots[3]
                                     : this->staticState.objectSlots[1];
    }
    this->isVisible = true;
    if (!usesMmAssets) {
        Actor_SetObjectDependency(play, &this->actor);
    }
    Actor_SetScale(&this->actor, definition->scale);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, definition->colliderRadius);
    if (!usesMmAssets) {
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    }
    if (usesMmAssets) {
        /* The archive-backed skeleton and animation were initialized above. */
    } else if (this->staticState.type == STATIC_STORY_ACTOR_FADO ||
               this->staticState.type == STATIC_STORY_ACTOR_KOKIRI_GIRL) {
        StaticStoryKokiri_Init(this, play);
    } else {
        EnViewerStatic_InitSkeleton(this, play, poseDescriptor);
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER) {
        EnViewerStatic_InitRutoWater(this, play, poseDescriptor);
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_DARUNIA && this->staticState.pose == 1) {
        EnViewerStatic_SetDaruniaDanceStep(this, 0);
    }
    Collider_InitCylinder(play, &this->staticState.collider);
    Collider_SetCylinder(play, &this->staticState.collider, &this->actor, &sStaticCylinderInit);
    this->staticState.collider.dim.radius = definition->colliderRadius;
    this->staticState.collider.dim.height = definition->colliderHeight;
    this->staticState.collider.dim.yShift = definition->colliderYShift;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->staticState.blinkTimer = Rand_S16Offset(definition->blinkMin, definition->blinkRange);
    int8_t fixedEyeIndex = StaticStoryActor_GetFixedEyeIndex((StaticStoryActorType)this->staticState.type,
                                                             this->staticState.pose);
    this->staticState.eyeIndex = fixedEyeIndex >= 0 ? fixedEyeIndex : 0;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY;
    this->staticState.talking = false;
    this->staticState.tracking = false;
    this->staticState.interactInfo.talkState = NPC_TALK_STATE_IDLE;
    this->staticState.initialized = true;
    ActorCatalogue_LogLifecycle("ready", this->actor.params, this->staticState.type, this->staticState.pose,
                                objects.modelObjectId, this->actor.objBankIndex, objects.animationObjectId,
                                this->animObjBankIndex);
    osSyncPrintf(
        "[ActorCatalogueProbe] ready params=%04X type=%d pose=%d modelObj=%d modelSlot=%d animSlot=%d limbs=%d "
        "pos=(%.2f,%.2f,%.2f) visible=%d\n",
        (uint16_t)this->actor.params, this->staticState.type, this->staticState.pose, definition->objectId,
        this->actor.objBankIndex, this->animObjBankIndex, this->skin.skelAnime.dListCount, this->actor.world.pos.x,
        this->actor.world.pos.y, this->actor.world.pos.z, this->isVisible);
    EnViewer_SetupAction(this, EnViewerStatic_Update);
}

void EnViewerStatic_Update(EnViewer* this, PlayState* play) {
    const StaticStoryActorDefinition* definition =
        StaticStoryActor_GetDefinition((StaticStoryActorType)this->staticState.type);
    const StaticStoryPoseDescriptor* poseDescriptor =
        StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose);

    if (definition == NULL || poseDescriptor == NULL) {
        return;
    }
    bool animationEnded = false;
    bool canInteract = true;

    if (poseDescriptor->skeletonFamily != STATIC_SKELETON_NONE &&
        poseDescriptor->animation != STATIC_ANIM_ADULT_ZELDA_NEUTRAL) {
        animationEnded = SkelAnime_Update(&this->skin.skelAnime);
        if (StaticStoryActor_LocksRootTranslation((StaticStoryActorType)this->staticState.type,
                                                   this->staticState.pose)) {
            this->skin.skelAnime.jointTable[0].x = 0;
            this->skin.skelAnime.jointTable[0].y = 0;
            this->skin.skelAnime.jointTable[0].z = 0;
        }
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER) {
        canInteract = EnViewerStatic_UpdateRutoWater(this, play, animationEnded);
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_DARUNIA && this->staticState.pose == 1 && animationEnded) {
        EnViewerStatic_SetDaruniaDanceStep(this, this->staticState.danceStep + 1);
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_GREAT_FAIRY) {
        this->staticState.greatFairyHoverPhase += 0x300;
        this->actor.world.pos.y = this->actor.home.pos.y +
                                  Math_SinS(this->staticState.greatFairyHoverPhase) *
                                      StaticStoryActor_GetGreatFairyHoverAmplitude(this->staticState.pose);
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_SKULL_KID) {
        this->staticState.mmHoverPhase += 0x4B0;
        this->staticState.tatlPulsePhase += 0x600;
        this->actor.world.pos.y = StaticStoryMm_ComposeHoverY(this->actor.home.pos.y,
                                                              this->staticState.mmHoverPhase);
        if (this->staticState.tatlInitialized) {
            SkelAnime_Update(&this->staticState.tatlSkelAnime);
        }
    }
    int8_t fixedEyeIndex = StaticStoryActor_GetFixedEyeIndex((StaticStoryActorType)this->staticState.type,
                                                             this->staticState.pose);
    if (fixedEyeIndex >= 0) {
        this->staticState.eyeIndex = fixedEyeIndex;
    } else if (this->staticState.blinkTimer > 0) {
        this->staticState.blinkTimer--;
    } else if (++this->staticState.eyeIndex >= 3) {
        this->staticState.eyeIndex = 0;
        this->staticState.blinkTimer = Rand_S16Offset(definition->blinkMin, definition->blinkRange);
    }
    if (canInteract) {
        Collider_UpdateCylinder(&this->actor, &this->staticState.collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->staticState.collider.base);
    }
    Actor_SetFocus(&this->actor, definition->focusHeight);
    if (canInteract) {
        EnViewerStatic_OfferTalk(this, play);
        EnViewerStatic_UpdateTracking(this, play);
        if (this->staticState.type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER && this->staticState.tracking &&
            StaticRutoWater_ShouldTurnBody(&this->staticState.rutoWater)) {
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x400, 1);
            this->actor.world.rot.y = this->actor.shape.rot.y;
        }
    }
}

void EnViewer_InitImpl(EnViewer* this, PlayState* play) {
    EnViewerInitData* initData = &sInitData[(u16)this->actor.params >> 8];
    s32 skelObjBankIndex = Object_GetIndex(&play->objectCtx, initData->skeletonObject);

    assert(skelObjBankIndex >= 0);

    this->animObjBankIndex = Object_GetIndex(&play->objectCtx, initData->animObject);
    assert(this->animObjBankIndex >= 0);

    if (!Object_IsLoaded(&play->objectCtx, skelObjBankIndex) ||
        !Object_IsLoaded(&play->objectCtx, this->animObjBankIndex)) {
        this->actor.flags &= ~ACTOR_FLAG_INSIDE_CULLING_VOLUME;
        return;
    }

    this->isVisible = true;
    this->actor.objBankIndex = skelObjBankIndex;
    Actor_SetObjectDependency(play, &this->actor);
    Actor_SetScale(&this->actor, initData->scale / 100.0f);
    ActorShape_Init(&this->actor.shape, initData->yOffset * 100, sShadowDrawFuncs[initData->shadowType],
                    initData->shadowScale);
    this->drawFuncIndex = initData->drawType;
    sInitAnimFuncs[this->drawFuncIndex](this, play, initData->skeletonHeaderSeg, initData->anim);
    EnViewer_SetupAction(this, EnViewer_UpdateImpl);
}

static s16 sTimer = 0;

static StaticStoryProgression EnViewerStatic_ReadProgression(void) {
    StaticStoryProgression progression = {
        .metZelda = Flags_GetEventChkInf(EVENTCHKINF_ZELDA_FLED_HYRULE_CASTLE),
        .forestComplete = CHECK_QUEST_ITEM(QUEST_SONG_SARIA),
        .waterComplete = Flags_GetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP),
        .eponaComplete = Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED),
    };

    return progression;
}

static void EnViewerStatic_UpdateTracking(EnViewer* this, PlayState* play) {
    const StaticStoryActorDefinition* definition =
        StaticStoryActor_GetDefinition((StaticStoryActorType)this->staticState.type);
    NpcInteractInfo* interactInfo = &this->staticState.interactInfo;

    if (definition != NULL &&
        StaticStoryActor_CanTrack((StaticStoryActorType)this->staticState.type, this->staticState.pose) &&
        this->staticState.tracking) {
        interactInfo->trackPos = GET_PLAYER(play)->actor.world.pos;
        interactInfo->trackPos.y += definition->trackingTargetYOffset;
        interactInfo->yOffset = definition->trackingYOffset;
        Npc_TrackPoint(&this->actor, interactInfo, definition->trackingPreset, NPC_TRACKING_HEAD_AND_TORSO);
    } else {
        Npc_TrackPoint(&this->actor, interactInfo, definition != NULL ? definition->trackingPreset : 0,
                       NPC_TRACKING_NONE);
    }
}

static void EnViewerStatic_RestorePlacementPose(EnViewer* this) {
    /* Talk never changes the animation; clearing the state preserves the selected placement pose exactly. */
    this->staticState.talking = false;
    this->staticState.tracking = false;
    this->staticState.interactInfo.talkState = NPC_TALK_STATE_IDLE;
}

void EnViewerStatic_OfferTalk(EnViewer* this, PlayState* play) {
    const StaticStoryActorDefinition* definition =
        StaticStoryActor_GetDefinition((StaticStoryActorType)this->staticState.type);
    StaticStoryProgression progression;

    if (this->staticState.talking) {
        this->staticState.tracking = true;
        if (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT &&
            StaticStoryActor_ShouldCloseEventMessage(true, Message_ShouldAdvance(play))) {
            Message_CloseTextbox(play);
            EnViewerStatic_RestorePlacementPose(this);
        } else if (Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING) {
            EnViewerStatic_RestorePlacementPose(this);
        }
        return;
    }

    progression = EnViewerStatic_ReadProgression();
    this->actor.textId = StaticStoryActor_SelectTextId((StaticStoryActorType)this->staticState.type, &progression);
    if (definition == NULL) {
        return;
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->staticState.talking = true;
        this->staticState.tracking = true;
        this->staticState.interactInfo.talkState = NPC_TALK_STATE_TALKING;
    } else {
        this->staticState.tracking = Actor_OfferTalk(&this->actor, play, definition->talkDistance);
    }
}

void EnViewer_UpdateImpl(EnViewer* this, PlayState* play) {
    u8 type = (u16)this->actor.params >> 8;
    u16 csFrames;
    s32 animationEnded;

    if (type == ENVIEWER_TYPE_2_ZELDA) {
        if (gSaveContext.sceneLayer == 5) {
            csFrames = play->csCtx.frames;
            if (csFrames == 792) {
                Audio_PlayActorSound2(&this->actor, NA_SE_VO_Z0_SURPRISE);
            } else if (csFrames == 845) {
                Audio_PlayActorSound2(&this->actor, NA_SE_VO_Z0_THROW);
            }
        }
    } else if (type == ENVIEWER_TYPE_7_GANONDORF) {
        Actor_SetScale(&this->actor, 0.3f);
        this->actor.uncullZoneForward = 10000.0f;
        this->actor.uncullZoneScale = 10000.0f;
        this->actor.uncullZoneDownward = 10000.0f;
    } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
        if (gSaveContext.sceneLayer == 4) {
            switch (play->csCtx.frames) {
                case 20:
                case 59:
                case 71:
                case 129:
                case 140:
                case 219:
                case 280:
                case 320:
                case 380:
                case 409:
                case 438:
                    Audio_PlaySoundGeneral(NA_SE_SY_DEMO_CUT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    break;
            }
        }
        if (gSaveContext.sceneLayer == 5) {
            if (play->csCtx.frames == 1508) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EN_FANTOM_ST_LAUGH);
            }
            if (play->csCtx.frames == 1545) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_DEMO_6K, 32.0f, 101.0f, 1226.0f, 0, 0, 0,
                                   0xC);
            }
        }
        if (play->csCtx.frames == 1020) {
            Audio_QueueSeqCmd(SEQ_PLAYER_FANFARE << 24 | NA_BGM_OPENING_GANON);
        }
        if (play->csCtx.frames == 960) {
            Audio_PlaySoundGeneral(NA_SE_EV_HORSE_GROAN, &this->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if (type == ENVIEWER_TYPE_6_HORSE_GANONDORF) {
        if (gSaveContext.sceneLayer == 5 || gSaveContext.sceneLayer == 10) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
        }
    } else if (type == ENVIEWER_TYPE_4_HORSE_GANONDORF) {
        s16 curFrame = this->skin.skelAnime.curFrame;

        if (this->skin.skelAnime.animation == &gHorseGanonRearingAnim) {
            if (curFrame == 8) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_GANON_HORSE_NEIGH);
            }
            if (curFrame == 30) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_LAND2);
            }
        } else if (this->skin.skelAnime.animation == &gHorseGanonIdleAnim) {
            if (curFrame == 25) {
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_SANDDUST);
            }
        } else if (this->skin.skelAnime.animation == &gHorseGanonGallopingAnim) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
        }
    }

    if (sTimer != 0) {
        sTimer--;
    }

    EnViewer_UpdatePosition(this, play);
    Actor_MoveXZGravity(&this->actor); // has no effect, speed/velocity and gravity are 0

    animationEnded = SkelAnime_Update(&this->skin.skelAnime);
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_4_HORSE_GANONDORF) {
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL) {
            if (play->csCtx.npcActions[1]->action == 2 && sTimer == 0) {
                if (type == ENVIEWER_TYPE_3_GANONDORF) {
                    if (this->skin.skelAnime.animation != &gYoungGanondorfHorsebackIdleAnim) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackIdleAnim, 1.0f);
                    }
                } else if (this->skin.skelAnime.animation != &gHorseGanonIdleAnim) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gHorseGanonIdleAnim, 1.0f);
                }
            } else if (play->csCtx.npcActions[1]->action == 1) {
                sTimer = 100;
                if (type == ENVIEWER_TYPE_3_GANONDORF) {
                    if (this->skin.skelAnime.animation != &gYoungGanondorfHorsebackRearAnim) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackRearAnim, 1.0f);
                    }
                } else if (this->skin.skelAnime.animation != &gHorseGanonRearingAnim) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gHorseGanonRearingAnim, 1.0f);
                }
            } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
                switch (this->state) {
                    case 0:
                        if (play->csCtx.npcActions[1]->action == 4) {
                            Animation_MorphToPlayOnce(&this->skin.skelAnime,
                                                      &gYoungGanondorfHorsebackLookSidewaysStartAnim, -5.0f);
                            this->state++;
                        }
                        break;
                    case 1:
                        if (animationEnded) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackLookSidewaysLoopAnim,
                                                  -5.0f);
                            this->state++;
                        }
                        break;
                    case 2:
                        if (play->csCtx.npcActions[1]->action == 5) {
                            Animation_MorphToPlayOnce(&this->skin.skelAnime,
                                                      &gYoungGanondorfHorsebackMagicChargeUpStartAnim, -5.0f);
                            this->state++;
                        }
                        break;
                    case 3:
                        if (animationEnded) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackMagicChargeUpLoopAnim,
                                                  -5.0f);
                            this->state++;
                        }
                        break;
                    case 4:
                        if (play->csCtx.npcActions[1]->action == 11) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackLookSidewaysLoopAnim,
                                                  -20.0f);
                            this->state++;
                        }
                        break;
                    case 5:
                        if (play->csCtx.npcActions[1]->action == 8) {
                            Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfHorsebackIdleAnim, -15.0f);
                            this->state++;
                        }
                        break;
                    case 6:
                        if (play->csCtx.npcActions[1]->action == 12) {
                            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GANON_VOICE_DEMO);
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfHorsebackRideAnim, 3.0f);
                            this->state++;
                        }
                        break;
                    case 7:
                        this->state = 0;
                        break;
                }
            } else if (this->skin.skelAnime.animation != &gHorseGanonGallopingAnim &&
                       play->csCtx.npcActions[1]->action == 12) {
                Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gHorseGanonGallopingAnim, 3.0f);
            }
        }
    } else if (type == ENVIEWER_TYPE_1_IMPA) {
        if (gSaveContext.sceneLayer == 5) {
            if (play->csCtx.frames == 845) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_ITEM_OCARINA, 4.0f, 81.0f, 2600.0f, 0, 0,
                                   0, 0);
            }
        } else {
            if (play->csCtx.frames == 195) {
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_ITEM_OCARINA, 4.0f, 81.0f, 2035.0f, 0, 0,
                                   0, 1);
            }
        }
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
                    play->csCtx.npcActions[0]->action == 6 &&
                    this->skin.skelAnime.animation != &object_opening_demo1_Anim_002574) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_002574, 1.5f);
                    this->state++;
                }
                break;
            case 1:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
                    play->csCtx.npcActions[0]->action == 2 &&
                    this->skin.skelAnime.animation != &object_opening_demo1_Anim_0029CC) {
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_0029CC, 3.0f);
                    this->state++;
                }
                break;
        }
    } else if (type == ENVIEWER_TYPE_2_ZELDA) {
        if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
            switch (this->state) {
                case 0:
                    if (play->csCtx.state != CS_STATE_IDLE) {
                        if (play->csCtx.npcActions[0] != NULL && play->csCtx.npcActions[0]->action == 6 &&
                            this->skin.skelAnime.animation != &object_opening_demo1_Anim_001410) {
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_001410, 1.5f);
                            this->state++;
                        }
                    }
                    break;
                case 1:
                    if (play->csCtx.state != CS_STATE_IDLE) {
                        if (play->csCtx.npcActions[0] != NULL && play->csCtx.npcActions[0]->action == 2 &&
                            this->skin.skelAnime.animation != &object_opening_demo1_Anim_000450) {
                            Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_000450, 3.0f);
                            this->state++;
                        }
                    }
                    break;
            }
        } else {
            Audio_SetBaseFilter(0);
            switch (this->state) {
                case 0:
                    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &object_opening_demo1_Anim_00504C, 1.0f);
                    this->state++;
                    break;
                case 1:
                    if (play->csCtx.npcActions[0]->action == 11) {
                        Animation_MorphToPlayOnce(&this->skin.skelAnime, &object_opening_demo1_Anim_00420C, -5.0f);
                        this->state++;
                    }
                    break;
                case 2:
                    if (animationEnded) {
                        Animation_MorphToLoop(&this->skin.skelAnime, &object_opening_demo1_Anim_0048FC, -5.0f);
                        this->state++;
                    }
                    break;
                case 3:
                    break;
            }
        }
    } else if (type == ENVIEWER_TYPE_7_GANONDORF) {
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL &&
                    play->csCtx.npcActions[1]->action == 7) {
                    Audio_PlaySoundGeneral(NA_SE_EN_GANON_LAUGH, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfLaughStartAnim, -5.0f);
                    this->state++;
                }
                break;
            case 1:
                if (animationEnded) {
                    Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfLaughLoopAnim, -5.0f);
                    this->state++;
                }
                break;
        }
    } else if (type == ENVIEWER_TYPE_8_GANONDORF) {
        switch (this->state) {
            case 0:
                if (play->csCtx.state != CS_STATE_IDLE) {
                    if (play->csCtx.npcActions[1] != NULL && play->csCtx.npcActions[1]->action == 9) {
                        Animation_PlayLoopSetSpeed(&this->skin.skelAnime, &gYoungGanondorfWalkAnim, 1.0f);
                        this->state++;
                    }
                }
                break;
            case 1:
                if (play->csCtx.npcActions[1]->action == 10) {
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfKneelStartAnim, -10.0f);
                    this->state++;
                }
                break;
            case 2:
                if (animationEnded) {
                    Animation_MorphToLoop(&this->skin.skelAnime, &gYoungGanondorfKneelLoopAnim, -5.0f);
                    this->state++;
                }
                break;
            case 3:
                if (play->csCtx.npcActions[1]->action == 4) {
                    Animation_MorphToPlayOnce(&this->skin.skelAnime, &gYoungGanondorfKneelLookSidewaysAnim, -5.0f);
                    this->state++;
                }
                break;
            default:
                this->state = 0;
                break;
        }
    }
}

void EnViewer_Update(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;

    if (this->staticState.staticMode) {
        if (this->staticState.initialized) {
            gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
        }
        this->actionFunc(this, play);
        return;
    }
    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->animObjBankIndex].segment);
    this->actionFunc(this, play);
}

s32 EnViewer_Ganondorf3OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    if (gSaveContext.sceneLayer == 4) {
        if (play->csCtx.frames >= 400) {
            if (limbIndex == 5) {
                *dList = gYoungGanondorfOpenLeftHandDL;
            }
        }
    } else {
        if (play->csCtx.frames >= 1510 && play->csCtx.frames <= 1650) {
            if (limbIndex == 5) {
                *dList = gYoungGanondorfOpenLeftHandDL;
            }
        }
    }
    return false;
}

void EnViewer_Ganondorf9PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == 11) {
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gGanondorfEyesDL));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnViewer_GanondorfPostLimbDrawUpdateCapeVec(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == 15) {
        Matrix_MultVec3f(&zeroVec, &sGanondorfNeckWorldPos);
    }
}

void EnViewer_DrawGanondorf(EnViewer* this, PlayState* play) {
    s16 frames = 0;
    s16 type;

    OPEN_DISPS(play->state.gfxCtx);
    type = this->actor.params >> 8;
    if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF || type == ENVIEWER_TYPE_7_GANONDORF ||
        type == ENVIEWER_TYPE_8_GANONDORF) {
        if (gSaveContext.sceneLayer != 4) {
            frames = 149;
        }

        if (frames + 1127 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeOpenTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeOpenTex));

        } else if (frames + 1128 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeHalfTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeHalfTex));

        } else if (frames + 1129 >= play->csCtx.frames) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeClosedTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeClosedTex));

        } else {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeLookingDownTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(&gYoungGanondorfEyeLookingDownTex));
        }
    } else if (type == ENVIEWER_TYPE_9_GANONDORF) {
        gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(&gGanondorfCrazedEyeTex));
    }

    if (type == ENVIEWER_TYPE_9_GANONDORF) {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, EnViewer_Ganondorf9PostLimbDraw, this);
    } else if (type == ENVIEWER_TYPE_3_GANONDORF) {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_Ganondorf3OverrideLimbDraw,
                                  EnViewer_GanondorfPostLimbDrawUpdateCapeVec, this);
        EnViewer_UpdateGanondorfCape(play, this);
    } else if (type == ENVIEWER_TYPE_3_GANONDORF || type == ENVIEWER_TYPE_5_GANONDORF ||
               type == ENVIEWER_TYPE_7_GANONDORF || type == ENVIEWER_TYPE_8_GANONDORF) {
        if ((play->csCtx.state != CS_STATE_IDLE) && (play->csCtx.npcActions[1] != NULL)) {
            SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, EnViewer_GanondorfPostLimbDrawUpdateCapeVec,
                                      this);
            EnViewer_UpdateGanondorfCape(play, this);
        }
    } else {
        SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, NULL, this);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_DrawHorse(EnViewer* this, PlayState* play) {
    func_800A6330(&this->actor, play, &this->skin, NULL, true);
}

s32 EnViewer_ZeldaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
        if (limbIndex == 2) {
            *dList = gChildZeldaCutsceneDressDL;
        }
        if (limbIndex == 7) {
            *dList = NULL;
        }
        if (limbIndex == 8) {
            *dList = NULL;
        }
        if (limbIndex == 9) {
            *dList = NULL;
        }
        if (limbIndex == 3) {
            *dList = NULL;
        }
        if (limbIndex == 5) {
            *dList = NULL;
        }
    }
    return false;
}

void EnViewer_ZeldaPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    s32 pad;

    if (play->sceneNum == SCENE_TEMPLE_OF_TIME) {
        if (limbIndex == 16) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, gChildZeldaOcarinaOfTimeDL);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

void EnViewer_DrawZelda(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    if (play->sceneNum == SCENE_HYRULE_FIELD) { // Hyrule Field
        if (play->csCtx.frames < 771) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeInTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeOutTex));
        } else if (play->csCtx.frames < 772) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
        } else if (play->csCtx.frames < 773) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        } else if (play->csCtx.frames < 791) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeWideTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeWideTex));
        } else if (play->csCtx.frames < 792) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeBlinkTex));
        } else if (play->csCtx.frames < 793) {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        } else {
            gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeInTex));
            gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeOutTex));
        }

        if (gSaveContext.sceneLayer == 6) {
            gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthSurprisedTex));
        } else {
            if (play->csCtx.frames < 758) {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
            } else if (play->csCtx.frames < 848) {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthSurprisedTex));
            } else {
                gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
            }
        }
    } else {
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gChildZeldaEyeShutTex));
        gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gChildZeldaMouthWorriedTex));
    }
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_ZeldaOverrideLimbDraw, EnViewer_ZeldaPostLimbDraw,
                              this);
    CLOSE_DISPS(play->state.gfxCtx);
}

s32 EnViewer_ImpaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 16) {
        *dList = this->staticState.type != STATIC_STORY_ACTOR_NONE ? gImpaHeadUnmaskedDL : gImpaHeadMaskedDL;
    }
    if (this->staticState.type == STATIC_STORY_ACTOR_IMPA &&
        StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_IMPA, this->staticState.pose)) {
        /* Native Demo_Im chest/head convention. */
        if (limbIndex == 9) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (limbIndex == 16) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }
    return false;
}

void EnViewer_DrawImpa(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gImpaEyeOpenTex, gImpaEyeHalfTex, gImpaEyeClosedTex };
    void* eye = this->staticState.type != STATIC_STORY_ACTOR_NONE ? sEyes[this->staticState.eyeIndex] : gImpaEyeOpenTex;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(eye));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0C, &D_80116280[2]);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_ImpaOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticChildMalonOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                     Vec3s* rot, void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 2 || limbIndex == 5) {
        *dList = NULL;
    }
    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_MALON, this->staticState.pose)) {
        if (limbIndex == 15) {
            Matrix_Translate(1400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.headRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.headRot.x), MTXMODE_APPLY);
            Matrix_Translate(-1400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else if (limbIndex == 8) {
            Matrix_RotateX(BINANG_TO_RAD(-this->staticState.interactInfo.torsoRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(-this->staticState.interactInfo.torsoRot.x), MTXMODE_APPLY);
        }
    }
    return false;
}

void EnViewer_DrawStaticChildMalon(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gMalonChildEyeOpenTex, gMalonChildEyeHalfTex, gMalonChildEyeClosedTex };
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gMalonChildNeutralMouthTex));
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticChildMalonOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticSariaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 15 &&
        (StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose)->flags &
         STATIC_POSE_FLAG_OCARINA)) {
        *dList = gSariaRightHandAndOcarinaDL;
    }
    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SARIA, this->staticState.pose)) {
        if (limbIndex == 16) {
            Matrix_Translate(900.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.headRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.headRot.x), MTXMODE_APPLY);
            Matrix_Translate(-900.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else if (limbIndex == 9) {
            Matrix_RotateY(BINANG_TO_RAD(this->staticState.interactInfo.torsoRot.y), MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.torsoRot.x), MTXMODE_APPLY);
        }
    }
    return false;
}

void EnViewer_DrawStaticSaria(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gSariaEyeOpenTex, gSariaEyeHalfTex, gSariaEyeClosedTex };
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gSariaMouthClosedTex));
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticSariaOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticSheikOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (StaticStoryActor_CanTrack((StaticStoryActorType)this->staticState.type, this->staticState.pose)) {
        /* Native En_Xc limb map: torso 9, head 16. */
        if (limbIndex == 9) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (limbIndex == 16) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }
    if (limbIndex == 12 &&
        StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose)->animation ==
            STATIC_ANIM_SHEIK_HARP) {
        *dList = gSheikHarpDL;
    }
    return false;
}

void EnViewer_DrawStaticSheik(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gSheikEyeOpenTex, gSheikEyeHalfClosedTex, gSheikEyeShutTex };

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 20, 0);
    gDPSetEnvColor(POLY_OPA_DISP++, 60, 0, 0, 0);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticSheikOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticAdultRutoOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                    void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (this->staticState.type == STATIC_STORY_ACTOR_ADULT_RUTO_WATER &&
        this->staticState.rutoWater.phase == STATIC_RUTO_PHASE_SURFACED) {
        s16 tread = (s16)(Math_SinS(this->staticState.rutoWater.treadPhase) * 0x300);
        StaticRutoWaterLegPose legPose;

        StaticRutoWater_GetTreadLegPose(tread, &legPose);
        if (limbIndex == 2) {
            rot->x += legPose.leftHip.x;
        } else if (limbIndex == 3) {
            rot->x += legPose.leftKnee.x;
        } else if (limbIndex == 6) {
            rot->x += legPose.rightHip.x;
        } else if (limbIndex == 7) {
            rot->x += legPose.rightKnee.x;
        }
    }

    if (StaticStoryActor_CanTrack((StaticStoryActorType)this->staticState.type, this->staticState.pose) &&
        (this->staticState.type != STATIC_STORY_ACTOR_ADULT_RUTO_WATER ||
         StaticRutoWater_CanTrack(&this->staticState.rutoWater))) {
        StaticStoryAdultRutoTrackingLimb trackingLimb = StaticStoryActor_GetAdultRutoTrackingLimb(limbIndex);

        if (trackingLimb == STATIC_RUTO_TRACKING_LIMB_TORSO) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (trackingLimb == STATIC_RUTO_TRACKING_LIMB_HEAD) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }
    return false;
}

static void* EnViewer_GetAdultRutoEye(u8 eyeIndex) {
    static void* sEyes[] = { gAdultRutoEyeOpenTex, gAdultRutoEyeHalfTex, gAdultRutoEyeClosedTex };
    return sEyes[eyeIndex];
}

void EnViewer_DrawStaticAdultRuto(EnViewer* this, PlayState* play) {
    void* eye = EnViewer_GetAdultRutoEye(this->staticState.eyeIndex);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gAdultRutoMouthTex));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0C, &D_80116280[2]);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticAdultRutoOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_DrawStaticAdultRutoXlu(EnViewer* this, PlayState* play) {
    void* eye = EnViewer_GetAdultRutoEye(this->staticState.eyeIndex);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_XLU_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_XLU_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gAdultRutoMouthTex));
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->staticState.rutoWater.alpha);
    gSPSegment(POLY_XLU_DISP++, 0x0C, &D_80116280[0]);
    POLY_XLU_DISP = SkelAnime_DrawFlex(play, this->skin.skelAnime.skeleton, this->skin.skelAnime.jointTable,
                                       this->skin.skelAnime.dListCount, EnViewer_StaticAdultRutoOverrideLimbDraw,
                                       NULL, this, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticChildRutoOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                    void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_RUTO, this->staticState.pose)) {
        /* Native En_Ru1 convention: left upper arm 8 carries torso, torso 15 carries head. */
        if (limbIndex == 8) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (limbIndex == 15) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }
    return false;
}

void EnViewer_DrawStaticChildRuto(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gRutoChildEyeOpenTex, gRutoChildEyeHalfTex, gRutoChildEyeClosedTex };

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gRutoChildMouthClosedTex));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0C, &D_80116280[2]);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticChildRutoOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticAdultMalonOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                     Vec3s* rot, void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 3 || limbIndex == 6) {
        *dList = NULL;
    }
    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_MALON, this->staticState.pose)) {
        if (limbIndex == 18) {
            Matrix_Translate(1400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.headRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.headRot.x), MTXMODE_APPLY);
            Matrix_Translate(-1400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else if (limbIndex == 11) {
            Matrix_RotateY(BINANG_TO_RAD(-this->staticState.interactInfo.torsoRot.y), MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(-this->staticState.interactInfo.torsoRot.x), MTXMODE_APPLY);
        }
    }
    return false;
}

static void EnViewer_StaticAdultMalonPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot,
                                                  void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 14 &&
        (StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose)->flags &
         STATIC_POSE_FLAG_BASKET)) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, gMalonAdultBasketDL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void EnViewer_DrawStaticAdultMalon(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gMalonAdultEyeOpenTex, gMalonAdultEyeHalfTex, gMalonAdultEyeClosedTex };

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gMalonAdultMouthNeutralTex));
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticAdultMalonOverrideLimbDraw,
                              EnViewer_StaticAdultMalonPostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticAdultZeldaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                     Vec3s* rot, void* thisx, Gfx** gfx) {
    EnViewer* this = (EnViewer*)thisx;
    const StaticStoryActorDefinition* definition =
        StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA);
    Mtx* faceMtx;

    (void)dList;
    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_ZELDA, this->staticState.pose)) {
        /* Native En_Zl3 convention for object_zl2: torso 7, head/face 14. */
        if (limbIndex == 7) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (limbIndex == 14) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }

    if (limbIndex != 14 || definition == NULL || definition->drawContract != STATIC_DRAW_CONTRACT_FACE_FLEX) {
        return false;
    }

    /* object_zl2's face display lists address seven deformation matrices through segment 0x0C. */
    faceMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx) * 7);
    gSPSegment((*gfx)++, 0x0C, faceMtx);

    Matrix_Push();
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_APPLY);
    Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
    Matrix_Push();
    Matrix_Translate(362.0f, -133.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Translate(-188.0f, -184.0f, 0.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[0]);
    Matrix_Translate(-410.0f, -184.0f, 0.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[1]);
    Matrix_Translate(-1019.0f, -26.0f, 0.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[2]);
    Matrix_Pop();

    Matrix_Push();
    Matrix_Translate(467.0f, 265.0f, 389.0f, MTXMODE_APPLY);
    Matrix_Translate(-427.0f, -1.0f, -3.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[3]);
    Matrix_Translate(-446.0f, -52.0f, 84.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[4]);
    Matrix_Pop();

    Matrix_Push();
    Matrix_Translate(467.0f, 265.0f, -389.0f, MTXMODE_APPLY);
    Matrix_Translate(-427.0f, -1.0f, 3.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[5]);
    Matrix_Translate(-446.0f, -52.0f, -84.0f, MTXMODE_APPLY);
    MATRIX_TOMTX(&faceMtx[6]);
    Matrix_Pop();
    Matrix_Pop();
    return false;
}

void EnViewer_DrawStaticAdultZelda(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gZelda2EyeOpenTex, gZelda2EyeHalfTex, gZelda2EyeShutTex };

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gZelda2MouthSeriousTex));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0B, &D_80116280[2]);
    POLY_OPA_DISP = SkelAnime_DrawFlex(play, this->skin.skelAnime.skeleton, this->skin.skelAnime.jointTable,
                                       this->skin.skelAnime.dListCount, EnViewer_StaticAdultZeldaOverrideLimbDraw,
                                       NULL, this, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticDaruniaOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                  void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_DARUNIA, this->staticState.pose)) {
        if (limbIndex == 16) {
            Matrix_Translate(2400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.headRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.headRot.x), MTXMODE_APPLY);
            Matrix_Translate(-2400.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        } else if (limbIndex == 8) {
            Matrix_RotateY(BINANG_TO_RAD(this->staticState.interactInfo.torsoRot.y), MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.torsoRot.x), MTXMODE_APPLY);
        }
    }
    return false;
}

void EnViewer_DrawStaticDarunia(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gDaruniaEyeOpenTex, gDaruniaEyeOpeningTex, gDaruniaEyeShutTex };
    bool dancing = this->staticState.pose == 1;
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA);

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(dancing ? gDaruniaMouthHappyTex : gDaruniaMouthSeriousTex));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(dancing ? gDaruniaNoseHappyTex : gDaruniaNoseSeriousTex));
    if (definition != NULL && definition->drawContract == STATIC_DRAW_CONTRACT_NPC_FLEX) {
        /* Darunia's object uses the native NPC flex contract, including segment 0x0C and environment state. */
        func_80034BA0(play, &this->skin.skelAnime, EnViewer_StaticDaruniaOverrideLimbDraw, NULL, &this->actor, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticNabooruOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                                  void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_NABOORU, this->staticState.pose)) {
        if (limbIndex == 8) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->y -= this->staticState.interactInfo.torsoRot.x;
        } else if (limbIndex == 15) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        }
    }
    return false;
}

void EnViewer_DrawStaticNabooru(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gNabooruEyeOpenTex, gNabooruEyeHalfTex, gNabooruEyeClosedTex };

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gSPSegment(POLY_OPA_DISP++, 0x0C, &D_80116280[2]);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticNabooruOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticGreatFairyOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                      Vec3s* rot, void* thisx) {
    EnViewer* this = (EnViewer*)thisx;
    StaticStoryTrackingMode trackingMode =
        StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, this->staticState.pose);
    StaticStoryGreatFairyTrackingLimb trackingLimb = StaticStoryActor_GetGreatFairyTrackingLimb(limbIndex);

    if (trackingMode == STATIC_TRACKING_MODE_FULL && trackingLimb == STATIC_GREAT_FAIRY_TRACKING_LIMB_TORSO) {
        rot->x += this->staticState.interactInfo.torsoRot.y;
    } else if (trackingMode != STATIC_TRACKING_MODE_NONE &&
               trackingLimb == STATIC_GREAT_FAIRY_TRACKING_LIMB_HEAD) {
        s16 headY = this->staticState.interactInfo.headRot.y;
        s16 headZ = this->staticState.interactInfo.headRot.x;

        if (trackingMode == STATIC_TRACKING_MODE_HEAD_ONLY) {
            headY = StaticStoryActor_ClampGreatFairyHeadRotation(headY);
            headZ = StaticStoryActor_ClampGreatFairyHeadRotation(headZ);
        }
        rot->x += headY;
        rot->z += headZ;
    }
    return false;
}

void EnViewer_DrawStaticGreatFairy(EnViewer* this, PlayState* play) {
    static void* sEyes[] = { gGreatFairyEyeOpenTex, gGreatFairyEyeHalfTex, gGreatFairyEyeClosedTex };

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[this->staticState.eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gGreatFairyMouthClosedTex));
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticGreatFairyOverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticBombShopLadyOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                       Vec3s* rot, void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, this->staticState.pose)) {
        /* object_bba enum order: head 7, torso 9. */
        if (limbIndex == 7) {
            rot->x += this->staticState.interactInfo.headRot.y;
            rot->z += this->staticState.interactInfo.headRot.x;
        } else if (limbIndex == 9) {
            rot->x += this->staticState.interactInfo.torsoRot.y;
            rot->z += this->staticState.interactInfo.torsoRot.x;
        }
    }
    return false;
}

static void EnViewer_DrawStaticMmActor(EnViewer* this, PlayState* play) {
    OverrideLimbDraw overrideLimbDraw = NULL;

    if (this->staticState.type == STATIC_STORY_ACTOR_BOMB_SHOP_LADY) {
        overrideLimbDraw = EnViewer_StaticBombShopLadyOverrideLimbDraw;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, overrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 EnViewer_StaticSkullKidOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                                   Vec3s* rot, void* thisx) {
    if (limbIndex == 17) {
        *dList = NULL;
    }
    return false;
}

static void EnViewer_StaticSkullKidPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot,
                                                void* thisx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 17) {
        OPEN_DISPS(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, this->staticState.skullKidHeadDL);
        gSPDisplayList(POLY_OPA_DISP++, this->staticState.skullKidEyesDL);
        gSPDisplayList(POLY_OPA_DISP++, this->staticState.skullKidMaskDL);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static void EnViewer_DrawStaticTatl(EnViewer* this, PlayState* play) {
    StaticStoryMmVec3f anchor = StaticStoryMm_GetTatlAnchor(this->staticState.pose);
    float orbitX = Math_SinS(this->staticState.tatlPulsePhase) * 4.0f;
    float orbitZ = Math_CosS(this->staticState.tatlPulsePhase) * 4.0f;
    float scale = StaticStoryMm_GetTatlScale(this->staticState.tatlPulsePhase);
    uint8_t outerAlpha = StaticStoryMm_GetTatlOuterAlpha(this->staticState.tatlPulsePhase);
    Gfx* fairySetup = GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Gfx));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_27Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08, fairySetup);
    gDPPipeSync(fairySetup++);
    gDPSetPrimColor(fairySetup++, 0, 1, 255, 255, 230, 255);
    gDPSetRenderMode(fairySetup++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
    gSPEndDisplayList(fairySetup);
    gDPSetEnvColor(POLY_XLU_DISP++, 220, 160, 80, outerAlpha);
    Matrix_Push();
    Matrix_Translate((anchor.x + orbitX) * 100.0f, anchor.y * 100.0f, (anchor.z + orbitZ) * 100.0f,
                     MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    POLY_XLU_DISP = SkelAnime_Draw(play, this->staticState.tatlSkelAnime.skeleton,
                                   this->staticState.tatlSkelAnime.jointTable, NULL, NULL, this, POLY_XLU_DISP);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnViewer_DrawStaticSkullKid(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, EnViewer_StaticSkullKidOverrideLimbDraw,
                              EnViewer_StaticSkullKidPostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
    EnViewer_DrawStaticTatl(this, play);
}

static void EnViewer_DrawStaticPhantomGanon(EnViewer* this, PlayState* play) {
    Gfx* nullDList = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Gfx));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gSPEndDisplayList(nullDList);
    gSPSegment(POLY_OPA_DISP++, 0x08, nullDList);
    SkelAnime_DrawSkeletonOpa(play, &this->skin.skelAnime, NULL, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewerStatic_Draw(EnViewer* this, PlayState* play) {
    switch ((StaticStoryActorType)this->staticState.type) {
        case STATIC_STORY_ACTOR_IMPA:
            EnViewer_DrawImpa(this, play);
            break;
        case STATIC_STORY_ACTOR_CHILD_MALON:
            EnViewer_DrawStaticChildMalon(this, play);
            break;
        case STATIC_STORY_ACTOR_SARIA:
            EnViewer_DrawStaticSaria(this, play);
            break;
        case STATIC_STORY_ACTOR_ADULT_ZELDA:
            EnViewer_DrawStaticAdultZelda(this, play);
            break;
        case STATIC_STORY_ACTOR_SHEIK:
            EnViewer_DrawStaticSheik(this, play);
            break;
        case STATIC_STORY_ACTOR_ADULT_RUTO:
            EnViewer_DrawStaticAdultRuto(this, play);
            break;
        case STATIC_STORY_ACTOR_ADULT_RUTO_WATER:
            if (this->staticState.rutoWater.alpha < 255) {
                EnViewer_DrawStaticAdultRutoXlu(this, play);
            } else {
                EnViewer_DrawStaticAdultRuto(this, play);
            }
            break;
        case STATIC_STORY_ACTOR_CHILD_RUTO:
            EnViewer_DrawStaticChildRuto(this, play);
            break;
        case STATIC_STORY_ACTOR_KOKIRI_GIRL:
        case STATIC_STORY_ACTOR_FADO:
            StaticStoryKokiri_Draw(this, play);
            break;
        case STATIC_STORY_ACTOR_ADULT_MALON:
            EnViewer_DrawStaticAdultMalon(this, play);
            break;
        case STATIC_STORY_ACTOR_DARUNIA:
            EnViewer_DrawStaticDarunia(this, play);
            break;
        case STATIC_STORY_ACTOR_NABOORU:
            EnViewer_DrawStaticNabooru(this, play);
            break;
        case STATIC_STORY_ACTOR_GREAT_FAIRY:
            EnViewer_DrawStaticGreatFairy(this, play);
            break;
        case STATIC_STORY_ACTOR_BOMB_SHOP_LADY:
            EnViewer_DrawStaticMmActor(this, play);
            break;
        case STATIC_STORY_ACTOR_SKULL_KID:
            EnViewer_DrawStaticSkullKid(this, play);
            break;
        case STATIC_STORY_ACTOR_PHANTOM_GANON:
            EnViewer_DrawStaticPhantomGanon(this, play);
            break;
        default:
            break;
    }
}

static EnViewerDrawFunc sDrawFuncs[] = {
    EnViewer_DrawGanondorf,        EnViewer_DrawHorse,       EnViewer_DrawZelda, EnViewer_DrawImpa,
    EnViewer_DrawStaticChildMalon, EnViewer_DrawStaticSaria,
};

void EnViewer_Draw(Actor* thisx, PlayState* play) {
    EnViewer* this = (EnViewer*)thisx;
    s32 pad;
    s16 type;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->isVisible) {
        type = (u16)this->actor.params >> 8;
        if (this->staticState.type != STATIC_STORY_ACTOR_NONE && this->staticState.initialized) {
            if (!this->staticState.diagnosticDrawLogged) {
                StaticStoryObjectRequirements objects =
                    StaticStoryActor_GetObjectRequirements((StaticStoryActorType)this->staticState.type);

                ActorCatalogue_LogLifecycle("draw", this->actor.params, this->staticState.type,
                                            this->staticState.pose, objects.modelObjectId, this->actor.objBankIndex,
                                            objects.animationObjectId, this->animObjBankIndex);
                osSyncPrintf(
                    "[ActorCatalogueProbe] draw params=%04X type=%d pose=%d modelSlot=%d animSlot=%d limbs=%d "
                    "pos=(%.2f,%.2f,%.2f) projectedZ=%.2f flags=%08X\n",
                    (uint16_t)this->actor.params, this->staticState.type, this->staticState.pose,
                    this->actor.objBankIndex, this->animObjBankIndex, this->skin.skelAnime.dListCount,
                    this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z,
                    this->actor.projectedPos.z, this->actor.flags);
                this->staticState.diagnosticDrawLogged = true;
            }
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            EnViewerStatic_Draw(this, play);
        } else if (type <= ENVIEWER_TYPE_2_ZELDA) { // zelda's horse, impa and zelda
            if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL) {
                Gfx_SetupDL_25Opa(play->state.gfxCtx);
                sDrawFuncs[this->drawFuncIndex](this, play);
            }
        } else if ((play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL) ||
                   type == ENVIEWER_TYPE_9_GANONDORF) {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            sDrawFuncs[this->drawFuncIndex](this, play);
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_UpdatePosition(EnViewer* this, PlayState* play) {
    Vec3f startPos;
    Vec3f endPos;
    f32 lerpFactor;
    s16 type = this->actor.params >> 8;

    if (type <= ENVIEWER_TYPE_2_ZELDA) { // zelda's horse, impa and zelda
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[0] != NULL &&
            play->csCtx.frames < play->csCtx.npcActions[0]->endFrame) {
            if (type == ENVIEWER_TYPE_0_HORSE_ZELDA) {
                if (!sHorseSfxPlayed) {
                    sHorseSfxPlayed = true;
                    Audio_PlaySoundGeneral(NA_SE_EV_HORSE_NEIGH, &this->actor.projectedPos, 4,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultReverb);
                }
                Audio_PlayActorSound2(&this->actor, NA_SE_EV_HORSE_RUN_LEVEL - SFX_FLAG);
            }

            startPos.x = play->csCtx.npcActions[0]->startPos.x;
            startPos.y = play->csCtx.npcActions[0]->startPos.y;
            startPos.z = play->csCtx.npcActions[0]->startPos.z;
            endPos.x = play->csCtx.npcActions[0]->endPos.x;
            endPos.y = play->csCtx.npcActions[0]->endPos.y;
            endPos.z = play->csCtx.npcActions[0]->endPos.z;
            lerpFactor = Environment_LerpWeight(play->csCtx.npcActions[0]->endFrame,
                                                play->csCtx.npcActions[0]->startFrame, play->csCtx.frames);
            this->actor.world.pos.x = (endPos.x - startPos.x) * lerpFactor + startPos.x;
            this->actor.world.pos.y = (endPos.y - startPos.y) * lerpFactor + startPos.y;
            this->actor.world.pos.z = (endPos.z - startPos.z) * lerpFactor + startPos.z;
        }
    } else { // ganondorf and ganondorf's horse
        if (play->csCtx.state != CS_STATE_IDLE && play->csCtx.npcActions[1] != NULL &&
            play->csCtx.frames < play->csCtx.npcActions[1]->endFrame) {
            startPos.x = play->csCtx.npcActions[1]->startPos.x;
            startPos.y = play->csCtx.npcActions[1]->startPos.y;
            startPos.z = play->csCtx.npcActions[1]->startPos.z;
            endPos.x = play->csCtx.npcActions[1]->endPos.x;
            endPos.y = play->csCtx.npcActions[1]->endPos.y;
            endPos.z = play->csCtx.npcActions[1]->endPos.z;
            lerpFactor = Environment_LerpWeight(play->csCtx.npcActions[1]->endFrame,
                                                play->csCtx.npcActions[1]->startFrame, play->csCtx.frames);
            this->actor.world.pos.x = (endPos.x - startPos.x) * lerpFactor + startPos.x;
            this->actor.world.pos.y = (endPos.y - startPos.y) * lerpFactor + startPos.y;
            this->actor.world.pos.z = (endPos.z - startPos.z) * lerpFactor + startPos.z;

            if (play->csCtx.npcActions[1]->action == 12) {
                s16 yaw = Math_Vec3f_Yaw(&startPos, &endPos);

                Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 0xA, 0x3E8, 1);
                Math_SmoothStepToS(&this->actor.shape.rot.y, yaw, 0xA, 0x3E8, 1);
            }

            if (type == ENVIEWER_TYPE_9_GANONDORF) {
                this->actor.world.rot.x = play->csCtx.npcActions[1]->urot.x;
                this->actor.world.rot.y = play->csCtx.npcActions[1]->urot.y;
                this->actor.world.rot.z = play->csCtx.npcActions[1]->urot.z;
                this->actor.shape.rot.x = play->csCtx.npcActions[1]->urot.x;
                this->actor.shape.rot.y = play->csCtx.npcActions[1]->urot.y;
                this->actor.shape.rot.z = play->csCtx.npcActions[1]->urot.z;
            }
        }
        if (type == ENVIEWER_TYPE_5_GANONDORF) {
            Audio_PlaySoundGeneral(NA_SE_EV_BURNING - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            EnViewer_DrawFireEffects(this, play);
        }
    }
}

void EnViewer_InitFireEffect(EnViewer* this, PlayState* play, s16 i) {
    EnViewerFireEffect* eff;

    if ((i % 2) == 0) {
        eff = &this->fireEffects[i];
        eff->startPos.x = 100.0f;
        eff->startPos.y = -420.0f;
        eff->startPos.z = 400.0f;
        eff->endPos.x = 100.0f;
        eff->endPos.y = -420.0f;
        eff->endPos.z = -400.0f;
        eff->scale = (Rand_ZeroOne() * 5.0f + 12.0f) * 0.001f;
        eff->epoch++;
    } else {
        eff = &this->fireEffects[i];
        eff->startPos.x = -100.0f;
        eff->startPos.y = -420.0f;
        eff->startPos.z = 400.0f;
        eff->endPos.x = -100.0f;
        eff->endPos.y = -420.0f;
        eff->endPos.z = -400.0f;
        eff->scale = (Rand_ZeroOne() * 5.0f + 12.0f) * 0.001f;
        eff->epoch++;
    }
    if (this) {}
}

void EnViewer_DrawFireEffects(EnViewer* this2, PlayState* play) {
    EnViewer* this = this2;
    s16 i;

    OPEN_DISPS(play->state.gfxCtx);
    for (i = 0; i < ARRAY_COUNT(this->fireEffects); i++) {
        switch (this->fireEffects[i].state) {
            case 0:
                EnViewer_InitFireEffect(this, play, i);
                this->fireEffects[i].lerpFactor = (i >> 1) * 0.1f;
                this->fireEffects[i].lerpFactorSpeed = 0.01f;
                this->fireEffects[i].state++;
                break;
            case 1:
                Math_SmoothStepToF(&this->fireEffects[i].lerpFactor, 1.0f, 1.0f, this->fireEffects[i].lerpFactorSpeed,
                                   this->fireEffects[i].lerpFactorSpeed);
                this->fireEffects[i].pos.x =
                    this->fireEffects[i].startPos.x +
                    (this->fireEffects[i].endPos.x - this->fireEffects[i].startPos.x) * this->fireEffects[i].lerpFactor;
                this->fireEffects[i].pos.y =
                    this->fireEffects[i].startPos.y +
                    (this->fireEffects[i].endPos.y - this->fireEffects[i].startPos.y) * this->fireEffects[i].lerpFactor;
                this->fireEffects[i].pos.z =
                    this->fireEffects[i].startPos.z +
                    (this->fireEffects[i].endPos.z - this->fireEffects[i].startPos.z) * this->fireEffects[i].lerpFactor;
                if (this->fireEffects[i].lerpFactor >= 1.0f) {
                    this->fireEffects[i].state++;
                }
                break;
            case 2:
                EnViewer_InitFireEffect(this, play, i);
                this->fireEffects[i].lerpFactor = 0.0f;
                this->fireEffects[i].lerpFactorSpeed = 0.01f;
                this->fireEffects[i].state--;
                break;
        }

        FrameInterpolation_RecordOpenChild(&this->fireEffects[i], this->fireEffects[i].epoch);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(this->fireEffects[i].pos.x, this->fireEffects[i].pos.y, this->fireEffects[i].pos.z,
                         MTXMODE_NEW);
        Matrix_Scale(this->fireEffects[i].scale, this->fireEffects[i].scale, this->fireEffects[i].scale, MTXMODE_APPLY);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                      (10 * i - 20 * play->state.frames) % 512, 32, 128, 0, 0, 0, -20));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 170, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 50, 00, 255);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPMatrix(POLY_XLU_DISP++, SEG_ADDR(1, 0), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void EnViewer_UpdateGanondorfCape(PlayState* play, EnViewer* this) {
    static s16 yOscillationPhase = 0;
    Vec3f forearmModelOffset;
    Vec3f forearmWorldOffset;

    if ((this->actor.params >> 8) == ENVIEWER_TYPE_5_GANONDORF) {
        sGanondorfCape->backPush = BREG(54) / 10.0f;
        sGanondorfCape->backSwayMagnitude = (BREG(60) + 25) / 100.0f;
        sGanondorfCape->sideSwayMagnitude = (BREG(55) - 45) / 10.0f;
        sGanondorfCape->minY = -10000.0f;
        sGanondorfCape->minDist = 0.0f;
        sGanondorfCape->gravity = (BREG(67) - 10) / 10.0f;
        forearmModelOffset.x = KREG(16) - 13.0f;
        forearmModelOffset.y = KREG(17) + 3.0f + Math_SinS(yOscillationPhase) * KREG(20);
        forearmModelOffset.z = KREG(18) - 10.0f;
        yOscillationPhase += KREG(19) * 0x1000 + 0x2000;

        Matrix_RotateY((this->actor.shape.rot.y / (f32)0x8000) * M_PI, MTXMODE_NEW);
        Matrix_MultVec3f(&forearmModelOffset, &forearmWorldOffset);
        sGanondorfCape->rightForearmPos.x = sGanondorfNeckWorldPos.x + forearmWorldOffset.x;
        sGanondorfCape->rightForearmPos.y = sGanondorfNeckWorldPos.y + forearmWorldOffset.y;
        sGanondorfCape->rightForearmPos.z = sGanondorfNeckWorldPos.z + forearmWorldOffset.z;
        forearmModelOffset.x = -(KREG(16) - 13.0f);
        Matrix_MultVec3f(&forearmModelOffset, &forearmWorldOffset);
        sGanondorfCape->leftForearmPos.x = sGanondorfNeckWorldPos.x + forearmWorldOffset.x;
        sGanondorfCape->leftForearmPos.y = sGanondorfNeckWorldPos.y + forearmWorldOffset.y;
        sGanondorfCape->leftForearmPos.z = sGanondorfNeckWorldPos.z + forearmWorldOffset.z;
    }
}
