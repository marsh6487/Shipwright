#include <stdbool.h>

#define REQUIRE(condition)                                                                                              \
    do {                                                                                                                \
        if (!(condition)) {                                                                                             \
            return 1;                                                                                                   \
        }                                                                                                               \
    } while (0)
#define assert(condition) REQUIRE(condition)

#include "../src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h"

int main(void) {
    StaticRutoWaterState state;
    float surfacedY;

    StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, false, 10.0f, 0.0f);
    assert(state.mode == STATIC_RUTO_GROUNDED);
    assert(state.phase == STATIC_RUTO_PHASE_GROUNDED);
    assert(state.currentY == 10.0f);

    StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, true, 10.0f, 100.0f);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.currentY == 46.0f);
    StaticRutoWater_Update(&state, true, 104.0f, false);
    assert(state.currentY == 50.0f);

    StaticRutoWater_Init(&state, STATIC_RUTO_DIVE_LOOP, true, 10.0f, 100.0f);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    surfacedY = state.currentY;
    state.phaseTimer = 1;
    StaticRutoWater_Update(&state, true, 100.0f, false);
    assert(state.phase == STATIC_RUTO_PHASE_DIVING);
    assert(state.velocityY == -4.0f);
    StaticRutoWater_Update(&state, true, 100.0f, true);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);

    state.phaseTimer = 1;
    StaticRutoWater_Update(&state, true, 100.0f, false);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);
    assert(state.velocityY == 4.0f);
    while (state.phase == STATIC_RUTO_PHASE_RISING) {
        StaticRutoWater_Update(&state, true, 100.0f, false);
    }
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.currentY == surfacedY);

    StaticRutoWater_Update(&state, false, 0.0f, false);
    assert(state.mode == STATIC_RUTO_GROUNDED);
    assert(state.phase == STATIC_RUTO_PHASE_GROUNDED);
    assert(state.currentY == state.homeY);

    StaticRutoWater_Init(&state, STATIC_RUTO_GROUNDED, true, 15.0f, 100.0f);
    StaticRutoWater_Update(&state, true, 120.0f, false);
    assert(state.currentY == 15.0f);

    return 0;
}
