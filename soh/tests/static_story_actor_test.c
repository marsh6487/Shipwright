#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

int main(void) {
    const StaticStoryActorDefinition* definition;
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
    assert(StaticStoryActor_GetType(0x7F00) == 0);
    assert(StaticStoryActor_GetType(0x7F0B) == 0);
    assert(StaticStoryActor_GetType(0x0101) == 0);
    assert(StaticStoryActor_GetType(-1) == 0);
    assert(!StaticStoryActor_IsParam(0x0000));
    assert(!StaticStoryActor_IsParam(0x0200));
    assert(!StaticStoryActor_IsParam(0x0703));
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
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_CHILD_RUTO)->objectId == OBJECT_RU1);
    assert(StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_CHILD_RUTO, 2)->animation ==
           STATIC_ANIM_CHILD_RUTO_SITTING);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->objectId == OBJECT_ZL2);
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
    assert(!StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_ZELDA, 0));
    assert(StaticStoryActor_CanTrack(STATIC_STORY_ACTOR_ADULT_RUTO, 0));
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
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_KOKIRI_GIRL)->trackingAdapter ==
           STATIC_TRACKING_KOKIRI);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_FADO)->trackingAdapter == STATIC_TRACKING_KOKIRI);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_MALON)->trackingAdapter ==
           STATIC_TRACKING_ADULT_MALON);
    assert(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_ADULT_ZELDA)->trackingAdapter == STATIC_TRACKING_NONE);
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
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &early) == 0x404E);
    assert(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_CHILD_RUTO, &complete) == 0x404E);
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

    int16_t placementPitch = 0x2000;
    int16_t placementYaw = -0x3456;
    int16_t placementRoll = 0x1000;
    StaticStoryActor_NormalizePlacementRotation(&placementPitch, &placementYaw, &placementRoll);
    assert(placementPitch == 0);
    assert(placementYaw == -0x3456);
    assert(placementRoll == 0);
    assert(StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_SARIA, 3));
    assert(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_SARIA, 0));
    assert(!StaticStoryActor_LocksRootTranslation(STATIC_STORY_ACTOR_KOKIRI_GIRL, 3));
    return 0;
}
