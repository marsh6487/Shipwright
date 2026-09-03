#ifndef STATIC_STORY_ACTOR_H
#define STATIC_STORY_ACTOR_H

#include <stdint.h>

#define STATIC_STORY_ACTOR_PARAM_PREFIX 0x7F00

typedef enum {
    STATIC_STORY_ACTOR_IMPA = 1,
    STATIC_STORY_ACTOR_CHILD_MALON = 2,
    STATIC_STORY_ACTOR_SARIA = 3,
    STATIC_STORY_ACTOR_MAX
} StaticStoryActorType;

static inline int StaticStoryActor_IsParam(int16_t params) {
    return ((uint16_t)params & 0xFF00) == STATIC_STORY_ACTOR_PARAM_PREFIX;
}

static inline int StaticStoryActor_GetType(int16_t params) {
    int type = (uint16_t)params & 0xFF;
    return StaticStoryActor_IsParam(params) && type > 0 && type < STATIC_STORY_ACTOR_MAX ? type : 0;
}

#endif
