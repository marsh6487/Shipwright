#include <stdint.h>

#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

#define REQUIRE(condition) do { if (!(condition)) return 1; } while (0)

int main(void) {
    StaticStoryObjectRequirements zelda = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_ZELDA);
    StaticStoryObjectRequirements malon = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_ADULT_MALON);
    StaticStoryObjectRequirements childMalon = StaticStoryActor_GetObjectRequirements(STATIC_STORY_ACTOR_CHILD_MALON);

    /* Animation decoding may temporarily own segment 6, but drawing must always
     * resolve skeleton/model segmented addresses against the model object. */
    REQUIRE(zelda.modelObjectId == OBJECT_ZL2);
    REQUIRE(zelda.animationObjectId == OBJECT_ZL2_ANIME2);
    REQUIRE(StaticStoryActor_GetDrawObjectId(STATIC_STORY_ACTOR_ADULT_ZELDA) == OBJECT_ZL2);
    REQUIRE(StaticStoryActor_GetDrawObjectId(STATIC_STORY_ACTOR_ADULT_MALON) == OBJECT_MA2);
    REQUIRE(StaticStoryActor_GetDrawObjectId(STATIC_STORY_ACTOR_CHILD_MALON) == OBJECT_MA1);
    REQUIRE(StaticStoryActor_GetDrawObjectId(STATIC_STORY_ACTOR_NONE) == OBJECT_INVALID);
    REQUIRE(malon.modelObjectId == malon.animationObjectId);
    REQUIRE(childMalon.modelObjectId == childMalon.animationObjectId);
    return 0;
}
