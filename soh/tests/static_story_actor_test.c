#include <assert.h>
#include <stdint.h>

#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

int main(void) {
    assert(StaticStoryActor_GetType(0x7F01) == STATIC_STORY_ACTOR_IMPA);
    assert(StaticStoryActor_GetType(0x7F02) == STATIC_STORY_ACTOR_CHILD_MALON);
    assert(StaticStoryActor_GetType(0x7F03) == STATIC_STORY_ACTOR_SARIA);
    assert(StaticStoryActor_GetType(0x7F00) == 0);
    assert(StaticStoryActor_GetType(0x7F04) == 0);
    assert(StaticStoryActor_GetType(0x0101) == 0);
    assert(StaticStoryActor_GetType(-1) == 0);
    return 0;
}
