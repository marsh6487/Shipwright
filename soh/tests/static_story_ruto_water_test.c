#include <stdbool.h>

#define REQUIRE(condition)                                                                                              \
    do {                                                                                                                \
        if (!(condition)) {                                                                                             \
            return 1;                                                                                                   \
        }                                                                                                               \
    } while (0)
#define assert(condition) REQUIRE(condition)

#include "../src/overlays/actors/ovl_En_Viewer/static_story_ruto_water.h"

static StaticRutoWaterEvents ReachSurface(StaticRutoWaterState* state, bool playerNear) {
    StaticRutoWaterEvents events = STATIC_RUTO_WATER_EVENT_NONE;

    while (state->phase == STATIC_RUTO_PHASE_RISING) {
        events |= StaticRutoWater_Update(state, true, 100.0f, playerNear, false, 60);
    }
    return events;
}

int main(void) {
    StaticRutoWaterState state;
    StaticRutoWaterEvents events;
    float firstTreadY;
    int rippleSeen = 0;
    int fadeFrames = 0;
    int16_t leftHip;
    int16_t leftKnee;
    int16_t rightHip;
    int16_t rightKnee;
    int i;

    /* A missing initial water query must not erase the placement's requested mode. */
    StaticRutoWater_Init(&state, STATIC_RUTO_SURFACE, false, 10.0f, 0.0f);
    assert(state.mode == STATIC_RUTO_SURFACE);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    assert(state.currentY == 10.0f);
    assert(state.alpha == 0);
    events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);
    assert(state.velocityY == 4.0f);
    assert(state.alpha == 0);
    assert(events == STATIC_RUTO_WATER_EVENT_NONE);

    /* The persistent surface placement visibly ascends from authored Y once water resolves. */
    events = ReachSurface(&state, false);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    /* Adult Ruto's taller rig needs a 10-unit lift over En_Zora's 54-unit anchor. */
    assert(state.currentY == 56.0f);
    assert(state.alpha == 255);
    assert((events & STATIC_RUTO_WATER_EVENT_EMERGED) != 0);

    /* Surfaced Ruto treads around the water line and emits periodic ripple requests. */
    firstTreadY = state.currentY;
    for (i = 0; i < 30; ++i) {
        events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
        if ((events & STATIC_RUTO_WATER_EVENT_RIPPLE) != 0) {
            rippleSeen = 1;
        }
    }
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.currentY != firstTreadY);
    assert(state.treadPhase != 0);
    assert(state.currentY >= 54.5f && state.currentY <= 57.5f);
    assert(rippleSeen);
    assert(StaticRutoWater_CanTrack(&state));
    StaticRutoWater_GetTreadLegRotations(0, &leftHip, &leftKnee, &rightHip, &rightKnee);
    assert(leftHip >= 0x800 && rightHip >= 0x800);
    /* Adult Ruto's knee axis is opposite the old approximation: positive Z folds the shins behind her. */
    assert(leftKnee >= 0x1400 && rightKnee >= 0x1400);
    StaticRutoWater_GetTreadLegRotations(0x300, &leftHip, &leftKnee, &rightHip, &rightKnee);
    assert(leftHip != rightHip);
    assert(leftKnee != rightKnee);
    assert(leftKnee > 0 && rightKnee > 0);

    /* Dive-loop Ruto stays hidden at authored depth until Link approaches. */
    StaticRutoWater_Init(&state, STATIC_RUTO_DIVE_LOOP, true, 10.0f, 100.0f);
    assert(state.mode == STATIC_RUTO_DIVE_LOOP);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    assert(state.currentY == 10.0f);
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);
    events = ReachSurface(&state, true);
    assert((events & STATIC_RUTO_WATER_EVENT_EMERGED) != 0);

    /* Leaving starts the supplied 40-79 frame grace period; returning cancels it. */
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_SURFACED);
    assert(state.phaseTimer == 60);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phaseTimer == 0);
    StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    events = STATIC_RUTO_WATER_EVENT_NONE;
    for (i = 0; i < 60; ++i) {
        events |= StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    }
    assert(state.phase == STATIC_RUTO_PHASE_PREPARING_DIVE);
    assert(state.velocityY == 0.0f);
    assert((events & STATIC_RUTO_WATER_EVENT_DIVE) == 0);
    assert(!StaticRutoWater_CanTrack(&state));
    events = StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_PREPARING_DIVE);
    assert(state.currentY >= 54.5f && state.currentY <= 57.5f);
    events = StaticRutoWater_Update(&state, true, 100.0f, false, true, 60);
    assert(state.phase == STATIC_RUTO_PHASE_DIVING);
    assert(state.velocityY == -4.0f);
    assert(state.alpha == 255);
    assert((events & STATIC_RUTO_WATER_EVENT_DIVE) != 0);
    while (state.phase == STATIC_RUTO_PHASE_DIVING && fadeFrames < 100) {
        StaticRutoWater_Update(&state, true, 100.0f, false, false, 60);
        fadeFrames++;
    }
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    assert(state.currentY == state.homeY);
    assert(fadeFrames >= 26);

    /* Losing a water box is recoverable and never changes the selected water mode. */
    StaticRutoWater_Update(&state, false, 0.0f, false, false, 60);
    assert(state.mode == STATIC_RUTO_DIVE_LOOP);
    assert(state.phase == STATIC_RUTO_PHASE_SUBMERGED);
    StaticRutoWater_Update(&state, true, 100.0f, true, false, 60);
    assert(state.phase == STATIC_RUTO_PHASE_RISING);

    StaticRutoWater_Init(&state, STATIC_RUTO_GROUNDED, true, 15.0f, 100.0f);
    StaticRutoWater_Update(&state, true, 120.0f, true, false, 60);
    assert(state.mode == STATIC_RUTO_GROUNDED);
    assert(state.phase == STATIC_RUTO_PHASE_GROUNDED);
    assert(state.currentY == 15.0f);

    return 0;
}
