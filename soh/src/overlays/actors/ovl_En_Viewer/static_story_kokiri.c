#include "static_story_kokiri.h"

#include "z_en_viewer.h"
#include "objects/object_fa/object_fa.h"
#include "objects/object_kw1/object_kw1.h"
#include "objects/object_os_anime/object_os_anime.h"
#include "static_story_actor.h"

s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

static void* sGirlEyes[] = { gKw1EyeOpenTex, gKw1EyeHalfTex, gKw1EyeClosedTex };
static void* sFadoEyes[] = { gFaEyeOpenTex, gFaEyeHalfTex, gFaEyeClosedTex };

static AnimationHeader* StaticStoryKokiri_GetAnimation(uint16_t animation) {
    switch (animation) {
        case STATIC_ANIM_KOKIRI_IDLE: return (AnimationHeader*)gKokiriIdleAnim;
        case STATIC_ANIM_KOKIRI_ARMS_BEHIND: return (AnimationHeader*)gKokiriStandingArmsBehindBackAnim;
        case STATIC_ANIM_KOKIRI_HANDS_HIPS: return (AnimationHeader*)gKokiriStandingHandsOnHipsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND: return (AnimationHeader*)gKokiriSittingHeadOnHandAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS: return (AnimationHeader*)gKokiriSittingCrossedLegsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS: return (AnimationHeader*)gKokiriSittingCrossedArmsLegsAnim;
        default: return NULL;
    }
}

int StaticStoryKokiri_RequestObjects(EnViewer* this, PlayState* play) {
    int16_t ids[4] = { this->staticState.type == STATIC_STORY_ACTOR_FADO ? OBJECT_FA : OBJECT_KW1, OBJECT_KW1,
                       OBJECT_KW1, OBJECT_OS_ANIME };

    for (int slot = 0; slot < 4; ++slot) {
        this->staticState.objectSlots[slot] = Object_GetIndex(&play->objectCtx, ids[slot]);
        if (this->staticState.objectSlots[slot] < 0) {
            this->staticState.objectSlots[slot] = Object_Spawn(&play->objectCtx, ids[slot]);
        }
        if (this->staticState.objectSlots[slot] < 0) {
            return false;
        }
    }
    return true;
}

void StaticStoryKokiri_Init(EnViewer* this, PlayState* play) {
    const StaticStoryPoseDescriptor* pose =
        StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose);

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[3]].segment);
    SkelAnime_InitFlex(play, &this->skin.skelAnime, (FlexSkeletonHeader*)gKw1Skel, NULL, NULL, NULL, 0);
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, StaticStoryKokiri_GetAnimation(pose->animation), pose->playbackSpeed);
}

static s32 StaticStoryKokiri_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* thisx, Gfx** gfx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 15) {
        gSPSegment((*gfx)++, 0x06, play->objectCtx.status[this->staticState.objectSlots[0]].segment);
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[0]].segment);
        *dList = this->staticState.type == STATIC_STORY_ACTOR_FADO ? (Gfx*)gFaDL : (Gfx*)object_kw1_DL_002C10;
        gSPSegment((*gfx)++, 0x0A,
                   SEGMENTED_TO_VIRTUAL(this->staticState.type == STATIC_STORY_ACTOR_FADO
                                            ? sFadoEyes[this->staticState.eyeIndex]
                                            : sGirlEyes[this->staticState.eyeIndex]));
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[2]].segment);
    }
    return false;
}

static void StaticStoryKokiri_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                            Gfx** gfx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 7) {
        gSPSegment((*gfx)++, 0x06, play->objectCtx.status[this->staticState.objectSlots[1]].segment);
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[1]].segment);
    }
}

static Gfx* StaticStoryKokiri_ColorDL(GraphicsContext* gfxCtx, u8 r, u8 g, u8 b) {
    Gfx* displayList = Graph_Alloc(gfxCtx, sizeof(Gfx) * 2);

    gDPSetEnvColor(displayList, r, g, b, 255);
    gSPEndDisplayList(displayList + 1);
    return displayList;
}

void StaticStoryKokiri_Draw(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, StaticStoryKokiri_ColorDL(play->state.gfxCtx, 70, 190, 60));
    gSPSegment(POLY_OPA_DISP++, 0x09, StaticStoryKokiri_ColorDL(play->state.gfxCtx, 100, 30, 0));
    POLY_OPA_DISP = SkelAnime_DrawFlex(play, this->skin.skelAnime.skeleton, this->skin.skelAnime.jointTable,
                                       this->skin.skelAnime.dListCount, StaticStoryKokiri_OverrideLimbDraw,
                                       StaticStoryKokiri_PostLimbDraw, this, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}
