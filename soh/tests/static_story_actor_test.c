#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

int main(void) {
    const StaticStoryActorDefinition* definition;

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
        if (definition->available) {
            assert(definition->adapter != STATIC_ADAPTER_NONE);
            for (uint8_t pose = 0; pose <= definition->maxPose; ++pose) {
                const StaticStoryPoseDescriptor* poseDescriptor =
                    StaticStoryActor_ResolvePose((StaticStoryActorType)type, pose);
                assert(poseDescriptor != NULL);
                assert(poseDescriptor->animation != STATIC_ANIM_NONE);
                assert(poseDescriptor->skeletonFamily != STATIC_SKELETON_NONE);
            }
        }
    }

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
