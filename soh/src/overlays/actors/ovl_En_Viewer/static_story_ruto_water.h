#ifndef STATIC_STORY_RUTO_WATER_H
#define STATIC_STORY_RUTO_WATER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATIC_RUTO_GROUNDED = 0,
    STATIC_RUTO_SURFACE = 1,
    STATIC_RUTO_DIVE_LOOP = 2,
} StaticRutoWaterMode;

typedef enum {
    STATIC_RUTO_PHASE_GROUNDED,
    STATIC_RUTO_PHASE_SURFACED,
    STATIC_RUTO_PHASE_DIVING,
    STATIC_RUTO_PHASE_SUBMERGED,
    STATIC_RUTO_PHASE_RISING,
} StaticRutoWaterPhase;

typedef struct {
    StaticRutoWaterMode mode;
    StaticRutoWaterPhase phase;
    float homeY;
    float surfaceY;
    float currentY;
    float velocityY;
    uint16_t phaseTimer;
} StaticRutoWaterState;

void StaticRutoWater_Init(StaticRutoWaterState* state, StaticRutoWaterMode mode, bool hasWater, float homeY,
                          float surfaceY);
void StaticRutoWater_Update(StaticRutoWaterState* state, bool hasWater, float surfaceY, bool animationEnded);

#endif
