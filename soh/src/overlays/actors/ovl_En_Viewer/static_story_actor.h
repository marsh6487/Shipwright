#ifndef STATIC_STORY_ACTOR_H
#define STATIC_STORY_ACTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "static_story_ruto_water.h"

#define STATIC_STORY_ACTOR_LEGACY_PARAM_PREFIX 0x7F00
#define STATIC_STORY_ACTOR_EXPANDED_PARAM_PREFIX 0x7E00
#define STATIC_STORY_ACTOR_PARAM_PREFIX STATIC_STORY_ACTOR_LEGACY_PARAM_PREFIX
#define STATIC_STORY_ACTOR_POSE_COUNT 16

typedef enum {
    STATIC_STORY_ACTOR_NONE = 0,
    STATIC_STORY_ACTOR_IMPA = 1,
    STATIC_STORY_ACTOR_CHILD_MALON,
    STATIC_STORY_ACTOR_SARIA,
    STATIC_STORY_ACTOR_ADULT_ZELDA,
    STATIC_STORY_ACTOR_SHEIK,
    STATIC_STORY_ACTOR_ADULT_RUTO,
    STATIC_STORY_ACTOR_CHILD_RUTO,
    STATIC_STORY_ACTOR_KOKIRI_GIRL,
    STATIC_STORY_ACTOR_FADO,
    STATIC_STORY_ACTOR_ADULT_MALON,
    STATIC_STORY_ACTOR_DARUNIA,
    STATIC_STORY_ACTOR_NABOORU,
    STATIC_STORY_ACTOR_ADULT_RUTO_WATER,
    STATIC_STORY_ACTOR_MAX,
} StaticStoryActorType;

typedef enum {
    STATIC_ADAPTER_NONE,
    STATIC_ADAPTER_IMPA,
    STATIC_ADAPTER_MALON,
    STATIC_ADAPTER_SARIA,
    STATIC_ADAPTER_ADULT_ZELDA,
    STATIC_ADAPTER_SHEIK,
    STATIC_ADAPTER_ADULT_RUTO,
    STATIC_ADAPTER_CHILD_RUTO,
    STATIC_ADAPTER_KOKIRI_GIRL,
    STATIC_ADAPTER_FADO,
    STATIC_ADAPTER_ADULT_MALON,
    STATIC_ADAPTER_DARUNIA,
    STATIC_ADAPTER_NABOORU,
} StaticStoryActorAdapter;

typedef enum {
    STATIC_SKELETON_NONE,
    STATIC_SKELETON_IMPA,
    STATIC_SKELETON_MALON_CHILD,
    STATIC_SKELETON_SARIA,
    STATIC_SKELETON_SHEIK,
    STATIC_SKELETON_ADULT_RUTO,
    STATIC_SKELETON_CHILD_RUTO,
    STATIC_SKELETON_KOKIRI,
    STATIC_SKELETON_MALON_ADULT,
    STATIC_SKELETON_ADULT_ZELDA,
    STATIC_SKELETON_DARUNIA,
    STATIC_SKELETON_NABOORU,
} StaticStorySkeletonFamily;

typedef enum {
    STATIC_TRACKING_NONE,
    STATIC_TRACKING_IMPA,
    STATIC_TRACKING_CHILD_MALON,
    STATIC_TRACKING_SARIA,
    STATIC_TRACKING_SHEIK,
    STATIC_TRACKING_ADULT_RUTO,
    STATIC_TRACKING_CHILD_RUTO,
    STATIC_TRACKING_KOKIRI,
    STATIC_TRACKING_ADULT_MALON,
    STATIC_TRACKING_DARUNIA,
    STATIC_TRACKING_NABOORU,
    STATIC_TRACKING_ADULT_ZELDA,
} StaticStoryTrackingAdapter;

typedef enum {
    STATIC_DRAW_CONTRACT_STANDARD_OPA,
    STATIC_DRAW_CONTRACT_NPC_FLEX,
    STATIC_DRAW_CONTRACT_FACE_FLEX,
} StaticStoryDrawContract;

typedef enum {
    STATIC_ANIM_NONE,
    STATIC_ANIM_IMPA_IDLE,
    STATIC_ANIM_MALON_IDLE,
    STATIC_ANIM_MALON_SING,
    STATIC_ANIM_SARIA_ARMS_TO_SIDE,
    STATIC_ANIM_SARIA_HANDS_BEHIND,
    STATIC_ANIM_SARIA_OCARINA,
    STATIC_ANIM_SARIA_SEATED,
    STATIC_ANIM_SHEIK_IDLE,
    STATIC_ANIM_SHEIK_ARMS_CROSSED,
    STATIC_ANIM_SHEIK_HARP,
    STATIC_ANIM_ADULT_RUTO_IDLE,
    STATIC_ANIM_ADULT_RUTO_HANDS_HIPS,
    STATIC_ANIM_ADULT_RUTO_LOOK_DOWN_LEFT,
    STATIC_ANIM_CHILD_RUTO_HANDS_BEHIND,
    STATIC_ANIM_CHILD_RUTO_HANDS_HIPS,
    STATIC_ANIM_CHILD_RUTO_SITTING,
    STATIC_ANIM_KOKIRI_IDLE,
    STATIC_ANIM_KOKIRI_ARMS_BEHIND,
    STATIC_ANIM_KOKIRI_HANDS_HIPS,
    STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND,
    STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS,
    STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS,
    STATIC_ANIM_FADO_IDLE,
    STATIC_ANIM_FADO_POSE_1,
    STATIC_ANIM_FADO_POSE_2,
    STATIC_ANIM_FADO_POSE_3,
    STATIC_ANIM_FADO_POSE_4,
    STATIC_ANIM_FADO_POSE_5,
    STATIC_ANIM_ADULT_MALON_IDLE,
    STATIC_ANIM_ADULT_MALON_BASKET,
    STATIC_ANIM_ADULT_MALON_SING,
    STATIC_ANIM_ADULT_ZELDA_NEUTRAL,
    STATIC_ANIM_ADULT_ZELDA_IDLE,
    STATIC_ANIM_DARUNIA_IDLE,
    STATIC_ANIM_DARUNIA_DANCE_1,
    STATIC_ANIM_DARUNIA_DANCE_2,
    STATIC_ANIM_DARUNIA_DANCE_3,
    STATIC_ANIM_DARUNIA_DANCE_4,
    STATIC_ANIM_NABOORU_IDLE,
} StaticStoryAnimation;

enum {
    STATIC_POSE_FLAG_NONE = 0,
    STATIC_POSE_FLAG_VOCAL = 1 << 0,
    STATIC_POSE_FLAG_OCARINA = 1 << 1,
    STATIC_POSE_FLAG_BASKET = 1 << 2,
    /* Performance and seated poses preserve their authored silhouette. */
    STATIC_POSE_FLAG_NO_TRACKING = 1 << 3,
};

/* A read-only snapshot: selectors never inspect or mutate save state directly. */
typedef struct {
    bool metZelda;
    bool forestComplete;
    bool waterComplete;
    bool eponaComplete;
} StaticStoryProgression;

typedef struct {
    uint16_t animation;
    float playbackSpeed;
    uint16_t flags;
    StaticStorySkeletonFamily skeletonFamily;
    uint8_t waterMode;
} StaticStoryPoseDescriptor;

typedef struct {
    uint8_t maxPose;
    uint8_t available;
    int16_t objectId;
    StaticStoryActorAdapter adapter;
    float scale;
    float focusHeight;
    int16_t colliderRadius;
    int16_t colliderHeight;
    int16_t colliderYShift;
    int16_t blinkMin;
    int16_t blinkRange;
    float talkDistance;
    StaticStoryTrackingAdapter trackingAdapter;
    int16_t trackingPreset;
    float trackingYOffset;
    float trackingTargetYOffset;
    StaticStoryDrawContract drawContract;
} StaticStoryActorDefinition;

typedef struct {
    int16_t modelObjectId;
    int16_t animationObjectId;
} StaticStoryObjectRequirements;

int StaticStoryActor_IsParam(int16_t params);
StaticStoryActorType StaticStoryActor_GetType(int16_t params);
uint8_t StaticStoryActor_GetPose(int16_t params);
uint8_t StaticStoryActor_SanitizePose(StaticStoryActorType type, uint8_t pose);
int StaticStoryActor_IsAvailable(StaticStoryActorType type);
const StaticStoryActorDefinition* StaticStoryActor_GetDefinition(StaticStoryActorType type);
int16_t StaticStoryActor_GetAnimationObjectId(StaticStoryActorType type);
StaticStoryObjectRequirements StaticStoryActor_GetObjectRequirements(StaticStoryActorType type);
const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose);
uint16_t StaticStoryActor_SelectTextId(StaticStoryActorType type, const StaticStoryProgression* progression);
int StaticStoryActor_CanTrack(StaticStoryActorType type, uint8_t pose);

#endif
