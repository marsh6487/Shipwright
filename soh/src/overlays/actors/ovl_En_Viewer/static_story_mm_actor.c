#include "static_story_mm_actor.h"

#include <stddef.h>
#include <math.h>

#define STATIC_STORY_MM_TAU 6.28318530717958647692f

static const StaticStoryMmPresentation sBombShopLadyPresentations[] = {
    { "objects/object_bba/gBombShopLadySkel", "objects/object_bba/gBombShopLadyIdleAnim", NULL, NULL, NULL, NULL,
      NULL, 18, STATIC_STORY_MM_TRACKING_HEAD_TORSO, false },
    { "objects/object_bba/gBombShopLadySkel", "objects/object_bba/gBombShopLadyIdleHoldingBagAnim", NULL, NULL,
      NULL, NULL, NULL, 18,
      STATIC_STORY_MM_TRACKING_NONE, false },
    { "objects/object_bba/gBombShopLadySkel", "objects/object_bba/gBombShopLadySwayAnim", NULL, NULL, NULL, NULL,
      NULL, 18, STATIC_STORY_MM_TRACKING_NONE, false },
};

static const StaticStoryMmPresentation sSkullKidPresentations[] = {
    { "objects/object_stk/gSkullKidSkel", "objects/object_stk2/gSkullKidRecliningFloatAnim",
      "objects/gameplay_keep/gameplay_keep_Skel_02AF58", "objects/gameplay_keep/gameplay_keep_Anim_029140",
      "objects/object_stk/gSkullKidMajorasMask1DL", "objects/object_stk/gSkullKidNormalHeadDL",
      "objects/object_stk/gSkullKidNormalEyesDL", 21, STATIC_STORY_MM_TRACKING_NONE, true },
    { "objects/object_stk/gSkullKidSkel", "objects/object_stk2/gSkullKidFloatingArmsCrossedAnim",
      "objects/gameplay_keep/gameplay_keep_Skel_02AF58", "objects/gameplay_keep/gameplay_keep_Anim_029140",
      "objects/object_stk/gSkullKidMajorasMask1DL", "objects/object_stk/gSkullKidNormalHeadDL",
      "objects/object_stk/gSkullKidNormalEyesDL", 21, STATIC_STORY_MM_TRACKING_NONE, true },
};

static const char* sTatlLimbPaths[] = {
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AEF8",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF04",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF10",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF1C",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF28",
    "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF34",
};

static const char* sTatlDListPaths[] = {
    "objects/gameplay_keep/gameplay_keep_DL_029990",
    "objects/gameplay_keep/gameplay_keep_DL_029A58",
    "objects/gameplay_keep/gameplay_keep_DL_029B20",
    "objects/gameplay_keep/gameplay_keep_DL_029BE8",
    "objects/gameplay_keep/gameplay_keep_DL_029CB0",
    "objects/gameplay_keep/gameplay_keep_DL_029CF0",
};

const StaticStoryMmPresentation* StaticStoryMm_GetPresentation(StaticStoryActorType type, uint8_t pose) {
    if (type == STATIC_STORY_ACTOR_BOMB_SHOP_LADY && pose < 3) {
        return &sBombShopLadyPresentations[pose];
    }
    if (type == STATIC_STORY_ACTOR_SKULL_KID && pose < 2) {
        return &sSkullKidPresentations[pose];
    }
    return NULL;
}

float StaticStoryMm_GetHoverOffset(uint16_t phase) {
    return sinf((float)phase * (STATIC_STORY_MM_TAU / 65536.0f)) * 10.0f;
}

float StaticStoryMm_ComposeHoverY(float authoredY, uint16_t phase) {
    return authoredY + StaticStoryMm_GetHoverOffset(phase);
}

StaticStoryMmVec3f StaticStoryMm_GetTatlAnchor(uint8_t pose) {
    static const StaticStoryMmVec3f sAnchors[] = {
        { -24.0f, 38.0f, -18.0f },
        { 30.0f, 46.0f, 4.0f },
    };

    return sAnchors[pose == 1 ? 1 : 0];
}

uint8_t StaticStoryMm_GetTatlOuterAlpha(uint16_t phase) {
    uint16_t halfPhase = phase & 0x7FFF;
    uint16_t triangle = halfPhase <= 0x4000 ? halfPhase : 0x8000 - halfPhase;

    return (uint8_t)(160 + (triangle * 80U) / 0x4000U);
}

float StaticStoryMm_GetTatlScale(uint16_t phase) {
    return 1.0f + sinf((float)phase * (STATIC_STORY_MM_TAU / 65536.0f)) * 0.08f;
}

const char* StaticStoryMm_GetTatlLimbPath(uint8_t limb) {
    return limb < 6 ? sTatlLimbPaths[limb] : NULL;
}

const char* StaticStoryMm_GetTatlDListPath(uint8_t limb) {
    return limb < 6 ? sTatlDListPaths[limb] : NULL;
}

bool StaticStoryMm_ResourcesComplete(const StaticStoryMmPresentation* presentation, bool hasSkeleton,
                                     bool hasAnimation, bool hasSecondarySkeleton) {
    return presentation != NULL && hasSkeleton && hasAnimation &&
           (!presentation->requiresSecondarySkeleton || hasSecondarySkeleton);
}
