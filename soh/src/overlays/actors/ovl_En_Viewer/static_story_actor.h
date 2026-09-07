#ifndef STATIC_STORY_ACTOR_H
#define STATIC_STORY_ACTOR_H

#include <stdbool.h>
#include <stdint.h>

#define STATIC_STORY_ACTOR_PARAM_PREFIX 0x7F00
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
} StaticStoryTrackingAdapter;

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
} StaticStoryAnimation;

enum {
    STATIC_POSE_FLAG_NONE = 0,
    STATIC_POSE_FLAG_VOCAL = 1 << 0,
    STATIC_POSE_FLAG_OCARINA = 1 << 1,
    STATIC_POSE_FLAG_BASKET = 1 << 2,
    /* Performance and seated poses preserve their authored silhouette. */
    STATIC_POSE_FLAG_NO_TRACKING = 1 << 3,
    /* Ignore animation-authored root motion and honor the Prelude placement. */
    STATIC_POSE_FLAG_LOCK_ROOT_TRANSLATION = 1 << 4,
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
} StaticStoryActorDefinition;

int StaticStoryActor_IsParam(int16_t params);
StaticStoryActorType StaticStoryActor_GetType(int16_t params);
uint8_t StaticStoryActor_GetPose(int16_t params);
uint8_t StaticStoryActor_SanitizePose(StaticStoryActorType type, uint8_t pose);
int StaticStoryActor_IsAvailable(StaticStoryActorType type);
const StaticStoryActorDefinition* StaticStoryActor_GetDefinition(StaticStoryActorType type);
const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose);
uint16_t StaticStoryActor_SelectTextId(StaticStoryActorType type, const StaticStoryProgression* progression);
int StaticStoryActor_CanTrack(StaticStoryActorType type, uint8_t pose);
void StaticStoryActor_NormalizePlacementRotation(int16_t* pitch, int16_t* yaw, int16_t* roll);
int StaticStoryActor_LocksRootTranslation(StaticStoryActorType type, uint8_t pose);
int StaticStoryActor_ShouldCloseTerminalText(int terminalState, int shouldAdvance);

#endif
