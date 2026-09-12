#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(condition)                                                                                              \
    do {                                                                                                                \
        if (!(condition)) {                                                                                             \
            return 1;                                                                                                   \
        }                                                                                                               \
    } while (0)

#include "z64object.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_ganon.h"

int main(void) {
    const StaticStoryGanonPresentation* phantom =
        StaticStoryGanon_GetPresentation(STATIC_STORY_ACTOR_PHANTOM_GANON);

    REQUIRE(phantom != NULL);
    REQUIRE(phantom->modelObjectId == OBJECT_GND);
    REQUIRE(phantom->animationObjectId == OBJECT_GND);
    REQUIRE(strcmp(phantom->skeletonPath, "__OTR__objects/object_gnd/gPhantomGanonSkel") == 0);
    REQUIRE(strcmp(phantom->animationPath, "__OTR__objects/object_gnd/gPhantomGanonNeutralAnim") == 0);
    REQUIRE(phantom->segment08 == STATIC_STORY_GANON_SEGMENT_NULL_DL);
    REQUIRE(!phantom->tracking);
    REQUIRE(!phantom->spawnHorse);
    REQUIRE(!phantom->spawnDynamicCape);
    REQUIRE(!phantom->usesBossActions);
    /* At 0.01 actor scale, this preserves authored Y and raises the rendered model by 10 world units. */
    REQUIRE(phantom->shapeYOffset * 0.01f == 10.0f);
    REQUIRE(StaticStoryGanon_GetPresentation(STATIC_STORY_ACTOR_SKULL_KID) == NULL);
    return 0;
}
