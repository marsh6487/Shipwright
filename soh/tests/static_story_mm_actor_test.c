#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(condition)                                                                                              \
    do {                                                                                                                \
        if (!(condition)) {                                                                                             \
            return 1;                                                                                                   \
        }                                                                                                               \
    } while (0)

#include "../src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"

int main(void) {
    const StaticStoryMmPresentation* idle =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0);
    const StaticStoryMmPresentation* sway =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1);
    const StaticStoryMmPresentation* fallback =
        StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2);

    REQUIRE(idle != NULL);
    REQUIRE(strcmp(idle->skeletonPath, "objects/object_bg/gTreasureChestShopGalSkel") == 0);
    REQUIRE(strcmp(idle->animationPath, "objects/object_bg/object_bg_Anim_009890") == 0);
    REQUIRE(strcmp(sway->animationPath, "objects/object_bg/object_bg_Anim_001384") == 0);
    REQUIRE(strcmp(fallback->animationPath, "objects/object_bg/object_bg_Anim_009890") == 0);
    REQUIRE(idle->limbCount == 23);
    REQUIRE(idle->tracking == STATIC_STORY_MM_TRACKING_HEAD_TORSO);
    REQUIRE(sway->tracking == STATIC_STORY_MM_TRACKING_NONE);
    REQUIRE(fallback->tracking == STATIC_STORY_MM_TRACKING_HEAD_TORSO);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 0),
                   "objects/object_bg/gTreasureChestShopGalEyeOpenDownTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 1),
                   "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 2),
                   "objects/object_bg/gTreasureChestShopGalEyeClosedTex") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 3),
                   "objects/object_bg/gTreasureChestShopGalEyeHalfDownTex") == 0);
    REQUIRE(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 4) == NULL);
    REQUIRE(StaticStoryMm_GetEyeTexturePath(STATIC_STORY_ACTOR_SKULL_KID, 0) == NULL);
    REQUIRE(!idle->requiresSecondarySkeleton);
    REQUIRE(StaticStoryMm_ResourcesComplete(idle, true, true, false));
    REQUIRE(!StaticStoryMm_ResourcesComplete(idle, false, true, false));
    REQUIRE(!StaticStoryMm_ResourcesComplete(idle, true, false, false));
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL, 3) == NULL);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0) == NULL);

    const StaticStoryMmPresentation* reclining = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 0);
    const StaticStoryMmPresentation* upright = StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 1);
    StaticStoryMmVec3f recliningAnchor = StaticStoryMm_GetTatlAnchor(0);
    StaticStoryMmVec3f uprightAnchor = StaticStoryMm_GetTatlAnchor(1);

    REQUIRE(reclining != NULL);
    REQUIRE(upright != NULL);
    REQUIRE(strcmp(reclining->skeletonPath, "objects/object_stk/gSkullKidSkel") == 0);
    REQUIRE(strcmp(reclining->animationPath, "objects/object_stk2/gSkullKidRecliningFloatAnim") == 0);
    REQUIRE(strcmp(upright->animationPath, "objects/object_stk2/gSkullKidFloatingArmsCrossedAnim") == 0);
    REQUIRE(strcmp(reclining->secondarySkeletonPath, "objects/gameplay_keep/gameplay_keep_Skel_02AF58") == 0);
    REQUIRE(strcmp(reclining->secondaryAnimationPath, "objects/gameplay_keep/gameplay_keep_Anim_029140") == 0);
    REQUIRE(strcmp(reclining->maskDisplayListPath, "objects/object_stk/gSkullKidMajorasMask1DL") == 0);
    REQUIRE(strcmp(reclining->headDisplayListPath, "objects/object_stk/gSkullKidNormalHeadDL") == 0);
    REQUIRE(strcmp(reclining->eyesDisplayListPath, "objects/object_stk/gSkullKidNormalEyesDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(1) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(2),
                   "objects/object_stk/gSkullKidPelvisDL") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(9),
                   "objects/object_stk/gSkullKidTorsoDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(17) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidLimbDisplayListPath(21),
                   "objects/object_stk/gSkullKidHatTopDL") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidLimbDisplayListPath(22) == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidVertexPath("objects/object_stk/gSkullKidPelvisDL"),
                   "objects/object_stk/object_stkVtx_00CCB0") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidVertexPath(reclining->headDisplayListPath),
                   "objects/object_stk/object_stkVtx_009EF0") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidVertexPath(reclining->eyesDisplayListPath),
                   "objects/object_stk/object_stkVtx_009EF0") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetSkullKidVertexPath(reclining->maskDisplayListPath),
                   "objects/object_stk/object_stkVtx_006120") == 0);
    REQUIRE(StaticStoryMm_GetSkullKidVertexPath("objects/object_stk/gSkullKidLinkMask1DL") == NULL);
    REQUIRE(strcmp(StaticStoryMm_GetTatlLimbPath(0),
                   "objects/gameplay_keep/gameplay_keep_Standardlimb_02AEF8") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlLimbPath(5),
                   "objects/gameplay_keep/gameplay_keep_Standardlimb_02AF34") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlDListPath(0), "objects/gameplay_keep/gameplay_keep_DL_029990") == 0);
    REQUIRE(strcmp(StaticStoryMm_GetTatlDListPath(5), "objects/gameplay_keep/gameplay_keep_DL_029CF0") == 0);
    REQUIRE(StaticStoryMm_GetTatlLimbPath(6) == NULL);
    REQUIRE(StaticStoryMm_GetTatlDListPath(6) == NULL);
    REQUIRE(reclining->requiresSecondarySkeleton);
    REQUIRE(upright->requiresSecondarySkeleton);
    REQUIRE(StaticStoryMm_UsesNativeFairyCompanion(STATIC_STORY_ACTOR_SKULL_KID));
    REQUIRE(!StaticStoryMm_UsesNativeFairyCompanion(STATIC_STORY_ACTOR_TREASURE_CHEST_SHOP_GAL));
    REQUIRE(reclining->tracking == STATIC_STORY_MM_TRACKING_NONE);
    REQUIRE(upright->tracking == STATIC_STORY_MM_TRACKING_NONE);
    REQUIRE(StaticStoryMm_ResourcesComplete(reclining, true, true, true));
    REQUIRE(!StaticStoryMm_ResourcesComplete(reclining, true, true, false));
    REQUIRE(fabsf(StaticStoryMm_GetHoverOffset(0)) < 0.001f);
    REQUIRE(fabsf(StaticStoryMm_GetHoverOffset(0x4000) - 10.0f) < 0.001f);
    REQUIRE(fabsf(StaticStoryMm_ComposeHoverY(123.0f, 0x4000) - 133.0f) < 0.001f);
    REQUIRE(recliningAnchor.x != uprightAnchor.x || recliningAnchor.y != uprightAnchor.y ||
            recliningAnchor.z != uprightAnchor.z);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0) >= 120);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0) <= 255);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0x8000) >= 120);
    REQUIRE(StaticStoryMm_GetTatlOuterAlpha(0x8000) <= 255);
    REQUIRE(StaticStoryMm_GetTatlScale(0) >= 0.8f);
    REQUIRE(StaticStoryMm_GetTatlScale(0) <= 1.2f);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID, 2) == NULL);
    return 0;
}
