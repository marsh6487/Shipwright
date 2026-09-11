#ifndef STATIC_STORY_MM_ACTOR_H
#define STATIC_STORY_MM_ACTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "static_story_actor.h"

typedef enum {
    STATIC_STORY_MM_TRACKING_NONE,
    STATIC_STORY_MM_TRACKING_HEAD_TORSO,
} StaticStoryMmTracking;

typedef struct {
    float x;
    float y;
    float z;
} StaticStoryMmVec3f;

typedef struct {
    const char* skeletonPath;
    const char* animationPath;
    const char* secondarySkeletonPath;
    const char* secondaryAnimationPath;
    const char* maskDisplayListPath;
    const char* headDisplayListPath;
    const char* eyesDisplayListPath;
    uint8_t limbCount;
    StaticStoryMmTracking tracking;
    bool requiresSecondarySkeleton;
} StaticStoryMmPresentation;

const StaticStoryMmPresentation* StaticStoryMm_GetPresentation(StaticStoryActorType type, uint8_t pose);
const char* StaticStoryMm_GetEyeTexturePath(StaticStoryActorType type, uint8_t eyeIndex);
bool StaticStoryMm_ResourcesComplete(const StaticStoryMmPresentation* presentation, bool hasSkeleton,
                                     bool hasAnimation, bool hasSecondarySkeleton);
float StaticStoryMm_GetHoverOffset(uint16_t phase);
float StaticStoryMm_ComposeHoverY(float authoredY, uint16_t phase);
StaticStoryMmVec3f StaticStoryMm_GetTatlAnchor(uint8_t pose);
uint8_t StaticStoryMm_GetTatlOuterAlpha(uint16_t phase);
float StaticStoryMm_GetTatlScale(uint16_t phase);
const char* StaticStoryMm_GetTatlLimbPath(uint8_t limb);
const char* StaticStoryMm_GetTatlDListPath(uint8_t limb);
const char* StaticStoryMm_GetSkullKidLimbDisplayListPath(uint8_t limb);
bool StaticStoryMm_UsesNativeFairyCompanion(StaticStoryActorType type);

#endif
