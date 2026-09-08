#include "static_story_ruto_water.h"

enum {
    STATIC_RUTO_SURFACE_FRAMES = 80,
    STATIC_RUTO_SUBMERGED_FRAMES = 40,
};

static const float sSurfaceOffset = 54.0f;
static const float sDiveVelocity = -4.0f;
static const float sRiseVelocity = 4.0f;

static void StaticRutoWater_Ground(StaticRutoWaterState* state) {
    state->mode = STATIC_RUTO_GROUNDED;
    state->phase = STATIC_RUTO_PHASE_GROUNDED;
    state->currentY = state->homeY;
    state->velocityY = 0.0f;
    state->phaseTimer = 0;
}

static void StaticRutoWater_Surface(StaticRutoWaterState* state) {
    state->phase = STATIC_RUTO_PHASE_SURFACED;
    state->currentY = state->surfaceY - sSurfaceOffset;
    state->velocityY = 0.0f;
    state->phaseTimer = state->mode == STATIC_RUTO_DIVE_LOOP ? STATIC_RUTO_SURFACE_FRAMES : 0;
}

void StaticRutoWater_Init(StaticRutoWaterState* state, StaticRutoWaterMode mode, bool hasWater, float homeY,
                          float surfaceY) {
    state->mode = mode;
    state->homeY = homeY;
    state->surfaceY = surfaceY;
    state->currentY = homeY;
    state->velocityY = 0.0f;
    state->phaseTimer = 0;

    if (mode == STATIC_RUTO_GROUNDED || !hasWater) {
        StaticRutoWater_Ground(state);
    } else {
        StaticRutoWater_Surface(state);
    }
}

void StaticRutoWater_Update(StaticRutoWaterState* state, bool hasWater, float surfaceY, bool animationEnded) {
    float surfaceTarget;

    if (state->mode == STATIC_RUTO_GROUNDED) {
        state->currentY = state->homeY;
        state->velocityY = 0.0f;
        return;
    }
    if (!hasWater) {
        StaticRutoWater_Ground(state);
        return;
    }

    state->surfaceY = surfaceY;
    surfaceTarget = surfaceY - sSurfaceOffset;

    switch (state->phase) {
        case STATIC_RUTO_PHASE_SURFACED:
            state->currentY = surfaceTarget;
            state->velocityY = 0.0f;
            if (state->mode == STATIC_RUTO_DIVE_LOOP && state->phaseTimer > 0 && --state->phaseTimer == 0) {
                state->phase = STATIC_RUTO_PHASE_DIVING;
                state->velocityY = sDiveVelocity;
            }
            break;
        case STATIC_RUTO_PHASE_DIVING:
            state->currentY += state->velocityY;
            if (animationEnded) {
                state->phase = STATIC_RUTO_PHASE_SUBMERGED;
                state->velocityY = sDiveVelocity;
                state->phaseTimer = STATIC_RUTO_SUBMERGED_FRAMES;
            }
            break;
        case STATIC_RUTO_PHASE_SUBMERGED:
            state->currentY += state->velocityY;
            if (state->phaseTimer > 0 && --state->phaseTimer == 0) {
                state->phase = STATIC_RUTO_PHASE_RISING;
                state->velocityY = sRiseVelocity;
            }
            break;
        case STATIC_RUTO_PHASE_RISING:
            state->currentY += state->velocityY;
            if (state->currentY >= surfaceTarget) {
                StaticRutoWater_Surface(state);
            }
            break;
        case STATIC_RUTO_PHASE_GROUNDED:
        default:
            StaticRutoWater_Ground(state);
            break;
    }
}
