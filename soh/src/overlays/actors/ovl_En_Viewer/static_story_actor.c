#include "static_story_actor.h"

#include <stddef.h>

#include "z64object.h"

#define STATIC_POSE(animation, flags, skeleton) { animation, 1.0f, flags, skeleton }

static const StaticStoryActorDefinition sDefinitions[STATIC_STORY_ACTOR_MAX] = {
    [STATIC_STORY_ACTOR_IMPA] =
        { 0, 1, OBJECT_IM, STATIC_ADAPTER_IMPA, 0.01f, 50.0f, 18, 46, 0, 30, 30, 80.0f, STATIC_TRACKING_IMPA, 12,
          4.0f },
    [STATIC_STORY_ACTOR_CHILD_MALON] = { 2, 1, OBJECT_MA1, STATIC_ADAPTER_MALON, 0.01f, 42.0f, 18, 46, 0, 30, 30,
                                          70.0f, STATIC_TRACKING_CHILD_MALON, 0, 0.0f, 10.0f },
    [STATIC_STORY_ACTOR_SARIA] = { 3, 1, OBJECT_SA, STATIC_ADAPTER_SARIA, 0.01f, 40.0f, 20, 46, 0, 30, 30, 70.0f,
                                    STATIC_TRACKING_SARIA, 2, 4.0f },
    [STATIC_STORY_ACTOR_ADULT_ZELDA] = { 1, 1, OBJECT_ZL2, STATIC_ADAPTER_ADULT_ZELDA, 0.01f, 60.0f, 25, 80, 0, 30,
                                          30, 90.0f, STATIC_TRACKING_NONE, 0, 0.0f },
    [STATIC_STORY_ACTOR_SHEIK] = { 2, 1, OBJECT_XC, STATIC_ADAPTER_SHEIK, 0.01f, 52.0f, 25, 80, 0, 60, 60, 90.0f,
                                    STATIC_TRACKING_SHEIK, 12, -3.0f },
    [STATIC_STORY_ACTOR_ADULT_RUTO] = { 2, 1, OBJECT_RU2, STATIC_ADAPTER_ADULT_RUTO, 0.01f, 54.0f, 30, 100, 0, 60,
                                         60, 90.0f, STATIC_TRACKING_ADULT_RUTO, 12, -3.0f },
    [STATIC_STORY_ACTOR_CHILD_RUTO] = { 2, 1, OBJECT_RU1, STATIC_ADAPTER_CHILD_RUTO, 0.01f, 42.0f, 25, 80, 0, 60,
                                         60, 80.0f, STATIC_TRACKING_CHILD_RUTO, 12, -3.0f },
    [STATIC_STORY_ACTOR_KOKIRI_GIRL] = { 5, 1, OBJECT_KW1, STATIC_ADAPTER_KOKIRI_GIRL, 0.01f, 40.0f, 20, 46, 0, 30,
                                          30, 70.0f, STATIC_TRACKING_KOKIRI, 2, 0.0f },
    [STATIC_STORY_ACTOR_FADO] = { 5, 1, OBJECT_FA, STATIC_ADAPTER_FADO, 0.01f, 40.0f, 20, 46, 0, 30, 30, 70.0f,
                                   STATIC_TRACKING_KOKIRI, 2, 0.0f },
    [STATIC_STORY_ACTOR_ADULT_MALON] = { 3, 1, OBJECT_MA2, STATIC_ADAPTER_ADULT_MALON, 0.01f, 52.0f, 18, 46, 0, 30,
                                          30, 80.0f, STATIC_TRACKING_ADULT_MALON, 0, 0.0f },
};

static const StaticStoryPoseDescriptor sPoses[STATIC_STORY_ACTOR_MAX][STATIC_STORY_ACTOR_POSE_COUNT] = {
    [STATIC_STORY_ACTOR_IMPA] = { STATIC_POSE(STATIC_ANIM_IMPA_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_IMPA) },
    [STATIC_STORY_ACTOR_CHILD_MALON] = {
        STATIC_POSE(STATIC_ANIM_MALON_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_MALON_CHILD),
        STATIC_POSE(STATIC_ANIM_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_CHILD),
        STATIC_POSE(STATIC_ANIM_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_CHILD),
    },
    [STATIC_STORY_ACTOR_SARIA] = {
        STATIC_POSE(STATIC_ANIM_SARIA_ARMS_TO_SIDE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_HANDS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_OCARINA, STATIC_POSE_FLAG_OCARINA | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_SARIA),
        STATIC_POSE(STATIC_ANIM_SARIA_SEATED, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_SARIA),
    },
    [STATIC_STORY_ACTOR_ADULT_ZELDA] = {
        STATIC_POSE(STATIC_ANIM_ADULT_ZELDA_NEUTRAL, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_ZELDA),
        /* OBJECT_ZL2_ANIME1 is asynchronous, so pose 1 uses the object-only neutral pose. */
        STATIC_POSE(STATIC_ANIM_ADULT_ZELDA_NEUTRAL, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_ZELDA),
    },
    [STATIC_STORY_ACTOR_SHEIK] = {
        STATIC_POSE(STATIC_ANIM_SHEIK_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SHEIK),
        STATIC_POSE(STATIC_ANIM_SHEIK_ARMS_CROSSED, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_SHEIK),
        STATIC_POSE(STATIC_ANIM_SHEIK_HARP, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_SHEIK),
    },
    [STATIC_STORY_ACTOR_ADULT_RUTO] = {
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
        STATIC_POSE(STATIC_ANIM_ADULT_RUTO_LOOK_DOWN_LEFT, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_ADULT_RUTO),
    },
    [STATIC_STORY_ACTOR_CHILD_RUTO] = {
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_HANDS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_CHILD_RUTO),
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_CHILD_RUTO),
        STATIC_POSE(STATIC_ANIM_CHILD_RUTO_SITTING, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_CHILD_RUTO),
    },
    [STATIC_STORY_ACTOR_KOKIRI_GIRL] = {
        STATIC_POSE(STATIC_ANIM_KOKIRI_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_ARMS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_KOKIRI),
    },
    [STATIC_STORY_ACTOR_FADO] = {
        STATIC_POSE(STATIC_ANIM_KOKIRI_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_ARMS_BEHIND, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_HANDS_HIPS, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS, STATIC_POSE_FLAG_NO_TRACKING, STATIC_SKELETON_KOKIRI),
        STATIC_POSE(STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS, STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_KOKIRI),
    },
    [STATIC_STORY_ACTOR_ADULT_MALON] = {
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_IDLE, STATIC_POSE_FLAG_NONE, STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_BASKET, STATIC_POSE_FLAG_BASKET, STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_ADULT),
        STATIC_POSE(STATIC_ANIM_ADULT_MALON_SING, STATIC_POSE_FLAG_VOCAL | STATIC_POSE_FLAG_NO_TRACKING,
                    STATIC_SKELETON_MALON_ADULT),
    },
};

_Static_assert(sizeof(sDefinitions) / sizeof(sDefinitions[0]) == STATIC_STORY_ACTOR_MAX,
               "Every static story actor type needs a definition");
_Static_assert(sizeof(sPoses) / sizeof(sPoses[0]) == STATIC_STORY_ACTOR_MAX,
               "Every static story actor type needs a pose row");
enum { STATIC_STORY_DEFINITION_COUNT = 10, STATIC_STORY_POSE_ROW_COUNT = 10 };
_Static_assert(STATIC_STORY_DEFINITION_COUNT == STATIC_STORY_ACTOR_MAX - 1,
               "Definition count must change with the actor registry");
_Static_assert(STATIC_STORY_POSE_ROW_COUNT == STATIC_STORY_ACTOR_MAX - 1,
               "Pose-row count must change with the actor registry");

int StaticStoryActor_IsParam(int16_t params) {
    return ((uint16_t)params & 0xFF00) == STATIC_STORY_ACTOR_PARAM_PREFIX;
}

StaticStoryActorType StaticStoryActor_GetType(int16_t params) {
    StaticStoryActorType type = (StaticStoryActorType)((uint16_t)params & 0x0F);

    return StaticStoryActor_IsParam(params) && type > STATIC_STORY_ACTOR_NONE && type < STATIC_STORY_ACTOR_MAX
               ? type
               : STATIC_STORY_ACTOR_NONE;
}

uint8_t StaticStoryActor_GetPose(int16_t params) {
    return StaticStoryActor_IsParam(params) ? (((uint16_t)params >> 4) & 0x0F) : 0;
}

const StaticStoryActorDefinition* StaticStoryActor_GetDefinition(StaticStoryActorType type) {
    return type > STATIC_STORY_ACTOR_NONE && type < STATIC_STORY_ACTOR_MAX ? &sDefinitions[type] : NULL;
}

int StaticStoryActor_IsAvailable(StaticStoryActorType type) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    return definition != NULL && definition->available;
}

uint8_t StaticStoryActor_SanitizePose(StaticStoryActorType type, uint8_t pose) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    if (definition == NULL || pose > definition->maxPose || sPoses[type][pose].animation == STATIC_ANIM_NONE) {
        return 0;
    }
    return pose;
}

const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose) {
    if (StaticStoryActor_GetDefinition(type) == NULL) {
        return NULL;
    }
    return &sPoses[type][StaticStoryActor_SanitizePose(type, pose)];
}

uint16_t StaticStoryActor_SelectTextId(StaticStoryActorType type, const StaticStoryProgression* progression) {
    /*
     * These are ordinary actor-offered messages.  The static path only assigns
     * the ID and lets the message system close it; it intentionally never
     * dispatches the source actor's follow-up action/update function.
     */
    StaticStoryProgression empty = { 0 };

    if (progression == NULL) {
        progression = &empty;
    }

    switch (type) {
        case STATIC_STORY_ACTOR_IMPA:
            return progression->metZelda ? 0x708E : 0x702A;
        case STATIC_STORY_ACTOR_CHILD_MALON:
            return progression->eponaComplete ? 0x204A : 0x2041;
        case STATIC_STORY_ACTOR_SARIA:
            return progression->forestComplete ? 0x10AD : 0x1001;
        case STATIC_STORY_ACTOR_ADULT_ZELDA:
            return progression->metZelda ? 0x703D : 0x703C;
        case STATIC_STORY_ACTOR_SHEIK:
            return progression->waterComplete ? 0x7010 : 0x700F;
        case STATIC_STORY_ACTOR_ADULT_RUTO:
            return progression->waterComplete ? 0x403E : 0x402C;
        case STATIC_STORY_ACTOR_CHILD_RUTO:
            return progression->waterComplete ? 0x404E : 0x404C;
        case STATIC_STORY_ACTOR_KOKIRI_GIRL:
            return progression->forestComplete ? 0x10DA : 0x1004;
        case STATIC_STORY_ACTOR_FADO:
            return progression->forestComplete ? 0x10D9 : 0x1005;
        case STATIC_STORY_ACTOR_ADULT_MALON:
            return progression->eponaComplete ? 0x2056 : 0x204C;
        default:
            return 0;
    }
}

int StaticStoryActor_CanTrack(StaticStoryActorType type, uint8_t pose) {
    const StaticStoryPoseDescriptor* poseDescriptor = StaticStoryActor_ResolvePose(type, pose);
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    if (definition == NULL || poseDescriptor == NULL || (poseDescriptor->flags & STATIC_POSE_FLAG_NO_TRACKING)) {
        return false;
    }

    return definition->trackingAdapter != STATIC_TRACKING_NONE;
}
