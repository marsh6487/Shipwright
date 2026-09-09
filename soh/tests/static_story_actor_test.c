#include <stddef.h>
#include <stdint.h>

#define REQUIRE(condition)                                                                                              \
    do {                                                                                                                \
        if (!(condition)) {                                                                                             \
            return 1;                                                                                                   \
        }                                                                                                               \
    } while (0)
#define assert(condition) REQUIRE(condition)

#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

int main(void) {
    const StaticStoryActorDefinition* definition;
    StaticStoryObjectRequirements objects;
    StaticStoryProgression early = { 0 };
    StaticStoryProgression complete = {
        .metZelda = true,
        .forestComplete = true,
        .waterComplete = true,
        .eponaComplete = true,
    };

    assert(StaticStoryActor_IsParam((int16_t)0x7F01));
    assert(StaticStoryActor_GetType(0x7F01) == STATIC_STORY_ACTOR_IMPA);
    assert(StaticStoryActor_GetType(0x7F02) == STATIC_STORY_ACTOR_CHILD_MALON);
    assert(StaticStoryActor_GetType(0x7F03) == STATIC_STORY_ACTOR_SARIA);
    assert(StaticStoryActor_GetType(0x7F04) == STATIC_STORY_ACTOR_ADULT_ZELDA);
    assert(StaticStoryActor_GetType(0x7F05) == STATIC_STORY_ACTOR_SHEIK);
    assert(StaticStoryActor_GetType(0x7F06) == STATIC_STORY_ACTOR_ADULT_RUTO);
    assert(StaticStoryActor_GetType(0x7F07) == STATIC_STORY_ACTOR_CHILD_RUTO);
    assert(StaticStoryActor_GetType(0x7F0A) == STATIC_STORY_ACTOR_ADULT_MALON);
    assert(StaticStoryActor_GetType(0x7F59) == STATIC_STORY_ACTOR_FADO);
    assert(StaticStoryActor_GetPose(0x7F59) == 5);
    assert(StaticStoryActor_IsParam((int16_t)0x7E01));
    assert(StaticStoryActor_GetType((int16_t)0x7E01) == STATIC_STORY_ACTOR_DARUNIA);
    assert(StaticStoryActor_GetType((int16_t)0x7E02) == STATIC_STORY_ACTOR_NABOORU);
    assert(StaticStoryActor_GetType((int16_t)0x7E03) == STATIC_STORY_ACTOR_ADULT_RUTO_WATER);
    assert(StaticStoryActor_GetType((int16_t)0x7E04) == STATIC_STORY_ACTOR_GREAT_FAIRY);
    assert(StaticStoryActor_GetType((int16_t)0x7E05) == STATIC_STORY_ACTOR_BOMB_SHOP_LADY);
    assert(StaticStoryActor_GetType((int16_t)0x7E15) == STATIC_STORY_ACTOR_BOMB_SHOP_LADY);
    assert(StaticStoryActor_GetType((int16_t)0x7E25) == STATIC_STORY_ACTOR_BOMB_SHOP_LADY);
    assert(StaticStoryActor_GetType((int16_t)0x7E06) == STATIC_STORY_ACTOR_ADULT_GANONDORF);
    assert(StaticStoryActor_GetType((int16_t)0x7E07) == STATIC_STORY_ACTOR_PHANTOM_GANON);
    assert(StaticStoryActor_GetType((int16_t)0x7E08) == STATIC_STORY_ACTOR_SKULL_KID);
    assert(StaticStoryActor_GetType((int16_t)0x7E18) == STATIC_STORY_ACTOR_SKULL_KID);
    assert(StaticStoryActor_GetType((int16_t)0x7E09) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    assert(StaticStoryActor_GetType((int16_t)0x7E19) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    assert(StaticStoryActor_GetType((int16_t)0x7E29) == STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN);
    assert(StaticStoryActor_GetPose((int16_t)0x7E11) == 1);
    assert(StaticStoryActor_GetPose((int16_t)0x7E23) == 2);
    assert(StaticStoryActor_GetType((int16_t)0x7E00) == STATIC_STORY_ACTOR_NONE);
    assert(StaticStoryActor_GetType((int16_t)0x7E0A) == STATIC_STORY_ACTOR_NONE);
    assert(StaticStoryActor_GetType((int16_t)0x7D01) == STATIC_STORY_ACTOR_NONE);
    assert(StaticStoryActor_GetType((int16_t)0x7F06) == STATIC_STORY_ACTOR_ADULT_RUTO);
    assert(StaticStoryActor_GetType(0x7F00) == 0);
    assert(StaticStoryActor_GetType(0x7F0B) == 0);
    assert(StaticStoryActor_GetType(0x0101) == 0);
    assert(StaticStoryActor_GetType(-1) == 0);
    assert(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_IMPA, 15) == 0);
    assert(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_FADO, 5) == 5);
    assert(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_FADO, 6) == 0);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_MALON, 0)->animation == STATIC_ANIM_MALON_IDLE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_MALON, 1)->animation == STATIC_ANIM_MALON_SING);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 1)->animation == STATIC_ANIM_SARIA_HANDS_BEHIND);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 2)->animation == STATIC_ANIM_SARIA_OCARINA);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SARIA, 3)->animation == STATIC_ANIM_SARIA_SEATED);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_IMPA, 0)->playbackSpeed == 1.0f);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SHEIK)->objectId == OBJECT_XC);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SHEIK, 2)->animation == STATIC_ANIM_SHEIK_HARP);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->objectId == OBJECT_RU2);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO, 1)->animation ==
           STATIC_ANIM_ADULT_RUTO_HANDS_HIPS);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 0)->animation ==
           STATIC_ANIM_ADULT_RUTO_IDLE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 0)->waterMode == STATIC_RUTO_GROUNDED);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 1)->waterMode == STATIC_RUTO_SURFACE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2)->waterMode == STATIC_RUTO_DIVE_LOOP);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, &complete) == 0x403E);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->objectId == OBJECT_DY_OBJ);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->adapter == STATIC_ADAPTER_GREAT_FAIRY);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->scale == 0.035f);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_GREAT_FAIRY)->focusHeight == 262.5f);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 0)->animation ==
           STATIC_ANIM_GREAT_FAIRY_SITTING);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 1)->animation ==
           STATIC_ANIM_GREAT_FAIRY_LAYING);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_GREAT_FAIRY, 2)->animation ==
           STATIC_ANIM_GREAT_FAIRY_AFTER_SPELL);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_GREAT_FAIRY, &early) == 0x00DB);
    assert(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_BOMB_SHOP_LADY) ==
           STATIC_STORY_RESOURCE_MM_ARCHIVE);
    assert(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_SKULL_KID) == STATIC_STORY_RESOURCE_MM_ARCHIVE);
    assert(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN) ==
           STATIC_STORY_RESOURCE_MM_ARCHIVE);
    assert(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_PHANTOM_GANON) == STATIC_STORY_RESOURCE_OOT_OBJECT);
    assert(StaticStoryActor_GetResourceSource(STATIC_STORY_ACTOR_ADULT_GANONDORF) ==
           STATIC_STORY_RESOURCE_OOT_OBJECT);
    assert(!StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_GANONDORF));
    assert(!StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN));
    assert(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_BOMB_SHOP_LADY));
    assert(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_SKULL_KID));
    assert(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_PHANTOM_GANON));
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 0)->animation ==
           STATIC_ANIM_BOMB_SHOP_LADY_IDLE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 1)->animation ==
           STATIC_ANIM_BOMB_SHOP_LADY_HOLDING_BAG);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 2)->animation ==
           STATIC_ANIM_BOMB_SHOP_LADY_SWAY);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SKULL_KID, 0)->animation ==
           STATIC_ANIM_SKULL_KID_RECLINING_FLOAT);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_SKULL_KID, 1)->animation ==
           STATIC_ANIM_SKULL_KID_ARMS_CROSSED_FLOAT);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_PHANTOM_GANON, 0)->animation ==
           STATIC_ANIM_PHANTOM_GANON_NEUTRAL);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0)->animation ==
           STATIC_ANIM_ADULT_GANONDORF_STAND);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0)->animation ==
           STATIC_ANIM_HAPPY_MASK_SALESMAN_IDLE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 1)->animation ==
           STATIC_ANIM_HAPPY_MASK_SALESMAN_HANDS_CLASPED);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 2)->animation ==
           STATIC_ANIM_HAPPY_MASK_SALESMAN_ARMS_OUT);
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 0));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 1));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_BOMB_SHOP_LADY, 2));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SKULL_KID, 0));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SKULL_KID, 1));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_PHANTOM_GANON, 0));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_GANONDORF, 0));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 0));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 1));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, 2));
    assert(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 0) == STATIC_TRACKING_MODE_FULL);
    assert(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 1) == STATIC_TRACKING_MODE_HEAD_ONLY);
    assert(StaticStoryActor_GetTrackingMode(STATIC_STORY_ACTOR_GREAT_FAIRY, 2) == STATIC_TRACKING_MODE_HEAD_ONLY);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->objectId == OBJECT_DU);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->adapter == STATIC_ADAPTER_DARUNIA);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_DARUNIA)->drawContract ==
           STATIC_DRAW_CONTRACT_NPC_FLEX);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 0)->animation == STATIC_ANIM_DARUNIA_IDLE);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_DARUNIA, 1)->animation == STATIC_ANIM_DARUNIA_DANCE_1);
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_DARUNIA, 1));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_DARUNIA, &complete) != 0);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->objectId == OBJECT_NB);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->adapter == STATIC_ADAPTER_NABOORU);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_NABOORU)->drawContract ==
           STATIC_DRAW_CONTRACT_STANDARD_OPA);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_NABOORU, 0)->animation == STATIC_ANIM_NABOORU_IDLE);
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_NABOORU, 0));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_NABOORU, &complete) != 0);
    assert(StaticStoryActor_GetAnimationObjectId(STATIC_STORY_ACTOR_ADULT_ZELDA) == OBJECT_ZL2_ANIME2);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 0)->animation == STATIC_ANIM_ADULT_ZELDA_IDLE);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->objectId == OBJECT_RU1);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_RUTO, 2)->animation == STATIC_ANIM_CHILD_RUTO_SITTING);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->objectId == OBJECT_ZL2);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->drawContract ==
           STATIC_DRAW_CONTRACT_FACE_FLEX);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_RUTO_WATER);
    assert(objects.modelObjectId == OBJECT_RU2);
    assert(objects.animationObjectId == OBJECT_RU2);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_NABOORU);
    assert(objects.modelObjectId == OBJECT_NB);
    assert(objects.animationObjectId == OBJECT_NB);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_ZELDA);
    assert(objects.modelObjectId == OBJECT_ZL2);
    assert(objects.animationObjectId == OBJECT_ZL2_ANIME2);
    objects = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_DARUNIA);
    assert(objects.modelObjectId == OBJECT_DU);
    assert(objects.animationObjectId == OBJECT_DU);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 0) != NULL);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 1) != NULL);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_ADULT_ZELDA, 1)->skeletonFamily ==
           STATIC_SKELETON_ADULT_ZELDA);

    for (int type = STATIC_STORY_ACTOR_IMPA; type < STATIC_STORY_ACTOR_MAX; ++type) {
        definition = StaticStoryActor_GetDefinition(type);
        assert(definition != NULL);
        assert(definition->scale > 0.0f);
        assert(definition->focusHeight > 0.0f);
        assert(definition->colliderRadius > 0);
        assert(definition->colliderHeight > definition->colliderRadius);
        assert(definition->talkDistance > 0.0f);
        if (definition->available) {
            assert(definition->adapter != STATIC_ADAPTER_NONE);
            for (uint8_t pose = 0; pose <= definition->maxPose; ++pose) {
                const StaticStoryPoseDescriptor* poseDescriptor =
                    StaticStoryActor_ResolvePose((StaticStoryActorType)type, pose);
                assert(poseDescriptor != NULL);
                assert(poseDescriptor->animation != STATIC_ANIM_NONE);
                assert(poseDescriptor->skeletonFamily != STATIC_SKELETON_NONE);
                assert(StaticStoryActor_CanTrack((StaticStoryActorType)type, pose) ==
                       ((definition->trackingAdapter != STATIC_TRACKING_NONE) &&
                        !(poseDescriptor->flags & STATIC_POSE_FLAG_NO_TRACKING)));
            }
        }
        assert(StaticStoryActor_SelectTextId((StaticStoryActorType)type, &early) != 0);
        assert(StaticStoryActor_SelectTextId((StaticStoryActorType)type, &complete) != 0);
    }

    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 0));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SHEIK, 2));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_FADO, 3));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_ZELDA, 0));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO_WATER, 2));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO, 0));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO, 1));
    assert(StaticStoryActor_GetAdultRutoTrackingLimb(9) == STATIC_RUTO_TRACKING_LIMB_NONE);
    assert(StaticStoryActor_GetAdultRutoTrackingLimb(10) == STATIC_RUTO_TRACKING_LIMB_TORSO);
    assert(StaticStoryActor_GetAdultRutoTrackingLimb(20) == STATIC_RUTO_TRACKING_LIMB_NONE);
    assert(StaticStoryActor_GetAdultRutoTrackingLimb(21) == STATIC_RUTO_TRACKING_LIMB_HEAD);
    assert(StaticStoryActor_GetGreatFairyTrackingLimb(8) == STATIC_GREAT_FAIRY_TRACKING_LIMB_TORSO);
    assert(StaticStoryActor_GetGreatFairyTrackingLimb(15) == STATIC_GREAT_FAIRY_TRACKING_LIMB_HEAD);
    assert(StaticStoryActor_GetGreatFairyTrackingLimb(14) == STATIC_GREAT_FAIRY_TRACKING_LIMB_NONE);
    assert(StaticStoryActor_ClampGreatFairyHeadRotation(0x2000) == 0x1000);
    assert(StaticStoryActor_ClampGreatFairyHeadRotation(-0x2000) == -0x1000);
    assert(StaticStoryActor_ClampGreatFairyHeadRotation(0x0800) == 0x0800);
    assert(StaticStoryActor_GetGreatFairyHoverAmplitude(0) == 5.0f);
    assert(StaticStoryActor_GetGreatFairyHoverAmplitude(1) == 3.0f);
    assert(StaticStoryActor_GetGreatFairyHoverAmplitude(2) == 5.0f);
    assert(StaticStoryActor_GetFixedEyeIndex(STATIC_STORY_ACTOR_SHEIK, 2) == 2);
    assert(StaticStoryActor_GetFixedEyeIndex(STATIC_STORY_ACTOR_SHEIK, 0) == -1);
    assert(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_IMPA) == STATIC_FACE_PROFILE_IMPA);
    assert(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_ADULT_RUTO) == STATIC_FACE_PROFILE_ADULT_RUTO);
    assert(StaticStoryActor_GetFaceProfile(STATIC_STORY_ACTOR_ADULT_RUTO_WATER) == STATIC_FACE_PROFILE_ADULT_RUTO);
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_MALON, 1));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SARIA, 2));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_SARIA, 3));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_CHILD_RUTO, 2));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_KOKIRI_GIRL, 3));
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_MALON, 2));
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_IMPA)->trackingAdapter == STATIC_TRACKING_IMPA);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingAdapter ==
           STATIC_TRACKING_CHILD_MALON);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SARIA)->trackingAdapter == STATIC_TRACKING_SARIA);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SHEIK)->trackingAdapter == STATIC_TRACKING_SHEIK);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->trackingAdapter ==
           STATIC_TRACKING_CHILD_RUTO);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KOKIRI_GIRL)->trackingAdapter == STATIC_TRACKING_KOKIRI);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_FADO)->trackingAdapter == STATIC_TRACKING_KOKIRI);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_MALON)->trackingAdapter ==
           STATIC_TRACKING_ADULT_MALON);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->trackingAdapter ==
           STATIC_TRACKING_ADULT_ZELDA);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->trackingAdapter ==
           STATIC_TRACKING_ADULT_RUTO);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_IMPA)->trackingPreset == 12);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingPreset == 0);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_SARIA)->trackingPreset == 2);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->trackingPreset == 12);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->trackingPreset == 12);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KOKIRI_GIRL)->trackingPreset == 2);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_MALON)->trackingPreset == 0);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingYOffset == 0.0f);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_MALON)->trackingTargetYOffset == 10.0f);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_MALON, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_MALON, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SARIA, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SARIA, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_RUTO, &complete));
    /* Static Child Ruto must never enter the 0x404C-0x404E Jabu dialogue chain. */
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &early) == 0x402C);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &complete) == 0x402C);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KOKIRI_GIRL, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_KOKIRI_GIRL, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_FADO, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_FADO, &complete));
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_MALON, &early) !=
           StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_MALON, &complete));

    assert(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_ZELDA));
    assert(StaticStoryActor_IsAvailable(STATIC_STORY_ACTOR_ADULT_MALON));
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->colliderRadius >
           StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->colliderRadius);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->colliderHeight >
           StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->colliderHeight);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->blinkMin == 60);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_RUTO)->blinkRange == 60);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->blinkMin == 60);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->blinkRange == 60);
    return 0;
}
