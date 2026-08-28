/**
 * rito_bow.inc.c — the Rito's own bow, on B. Text-included after rito_flight.inc.c,
 * inside the same extern "C" body, and dispatched BEFORE it so a press in mid-air takes
 * the frame off the glide. Three arrows leave on the release, each with a target of its
 * own; switches come before enemies because a switch is the reason you are shooting.
 */

#define RITO_BOW_ANIM(name) "__OTR__misc/link_animetion/gPlayerAnim_mhr_bow_" name

#define RITO_BOW_CLIP_AIR RITO_BOW_ANIM("charge_attack09")
#define RITO_BOW_CLIP_ENTER_STILL RITO_BOW_ANIM("motion12")
#define RITO_BOW_CLIP_HOLD_STILL RITO_BOW_ANIM("idle04_loop")
#define RITO_BOW_CLIP_ENTER_MOVE RITO_BOW_ANIM("dash_attack07")
#define RITO_BOW_CLIP_ENTER_DASH RITO_BOW_ANIM("dash_attack09")
#define RITO_BOW_CLIP_HOLD_MOVE RITO_BOW_ANIM("run07_loop")
#define RITO_BOW_CLIP_RELEASE RITO_BOW_ANIM("dash_attack13")

#define RITO_BOW_SPEED 1.25f

// Frames are LOGIC frames: this runs at 20Hz (R_UPDATE_RATE = 3), so 20 = 1 second.
#define RITO_BOW_VOLLEY 3         // arrows per release, one target each
#define RITO_BOW_LOOSE_FRAME 3    // into the release clip, where the arrows leave
#define RITO_BOW_RANGE 1400.0f    // how far the volley looks for something to hit
#define RITO_BOW_CONE 0x5000      // ~110 degrees each side of the camera: generous, there is no aim
#define RITO_BOW_TURN 0x0600      // per-frame steering an arrow may apply toward its target
#define RITO_BOW_ARROW_SPEED 150.0f
#define RITO_BOW_HIT_DIST 170.0f  // one frame of travel: EnArrow moves 150 units a frame,
                                  // so a tighter sphere is jumped clean over
#define RITO_BOW_MOVE_SPEED 5.0f  // walking with the bow drawn
#define RITO_BOW_MOVE_DEADZONE 10.0f
#define RITO_BOW_TURN_RATE 0x0C00 // how fast the body swings to the stick while drawn
#define RITO_BOW_ICE_RADIUS 90.0f // how near an arrival an Obj_Ice_Poly has to be to shatter
#define RITO_BOW_HANDSHAKE 4      // player->unk_A73: without it EnArrow_Shoot kills its own arrow

// Obj_Switch's own layout (z_obj_switch.h): type in the low 3 bits, frozen in bit 7.
#define RITO_BOW_SWITCH_TYPE(actor) ((actor)->params & 7)
#define RITO_BOW_SWITCH_IS_TARGET(actor)                                          \
    ((RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_EYE) ||                       \
     (RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_CRYSTAL) ||                   \
     (RITO_BOW_SWITCH_TYPE(actor) == OBJSWITCH_TYPE_CRYSTAL_TARGETABLE))

typedef enum {
    RITO_BOW_OFF = 0,
    RITO_BOW_AIR,     // one press in the air: held still, one arrow straight down
    RITO_BOW_ENTER,   // a ground entry clip is playing; the hold loop follows it
    RITO_BOW_HOLD,    // drawn on the ground, waiting for B to come up
    RITO_BOW_RELEASE, // the loose; the volley leaves on RITO_BOW_LOOSE_FRAME
} RitoBowState;

// The actor pointer is only ever COMPARED against the live actor list, never
// dereferenced blind, so an arrow that dies between frames leaves nothing dangling.
typedef struct {
    Actor* arrow;
    Vec3f goal;
} RitoBowArrow;

typedef struct {
    u8 loaded;
    u8 state;
    u8 moving; // which hold loop is running: 0 = idle04, 1 = run07
    s16 timer;
    LinkAnimationHeader* air;
    LinkAnimationHeader* enterStill;
    LinkAnimationHeader* holdStill;
    LinkAnimationHeader* enterMove;
    LinkAnimationHeader* enterDash; // still -> moving, without leaving the draw
    LinkAnimationHeader* holdMove;
    LinkAnimationHeader* release;
    RitoBowArrow arrows[RITO_BOW_VOLLEY];
} RitoBow;

static RitoBow sRitoBow;

static LinkAnimationHeader* MmForm_RitoBowLoadClip(const char* path) {
    LinkAnimationHeader* raw;
    s16 frames;

    if ((path == NULL) || !ResourceMgr_FileExists(path)) {
        return NULL;
    }
    raw = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (raw == NULL) {
        return NULL;
    }
    // Resampling to fewer frames IS the speed-up, and the resampler rewrites the resource
    // in place: every path here is loaded exactly once, guarded by `loaded`.
    frames = (s16)(((f32)raw->common.frameCount / RITO_BOW_SPEED) + 0.5f);
    if (frames < 2) {
        frames = 2;
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, frames);
}

static void MmForm_RitoBowLoadClips(void) {
    if (sRitoBow.loaded) {
        return;
    }
    sRitoBow.loaded = 1;
    sRitoBow.air = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_AIR);
    sRitoBow.enterStill = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_ENTER_STILL);
    sRitoBow.holdStill = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_HOLD_STILL);
    sRitoBow.enterMove = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_ENTER_MOVE);
    sRitoBow.enterDash = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_ENTER_DASH);
    sRitoBow.holdMove = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_HOLD_MOVE);
    sRitoBow.release = MmForm_RitoBowLoadClip(RITO_BOW_CLIP_RELEASE);
    if (sRitoBow.holdStill == NULL) {
        SPDLOG_WARN("[Rito] bow clips missing ({}) — B will not draw", RITO_BOW_CLIP_HOLD_STILL);
    }
}

extern "C" u8 MmForm_RitoBowIsOut(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_RITO) && (sRitoBow.state != RITO_BOW_OFF);
}

// Where an arrow should be pointed to hit `actor`. Obj_Switch never fills focus.pos,
// so its own origin plus a lift is the only honest aim point for one.
static void MmForm_RitoBowAimPoint(Actor* actor, Vec3f* out) {
    if (actor->id == ACTOR_OBJ_SWITCH) {
        out->x = actor->world.pos.x;
        out->y = actor->world.pos.y + 20.0f;
        out->z = actor->world.pos.z;
        return;
    }
    Math_Vec3f_Copy(out, &actor->focus.pos);
}

static u8 MmForm_RitoBowIsInSight(Actor* actor, s16 camYaw) {
    s16 off;

    if ((actor->update == NULL) || (actor->xyzDistToPlayerSq > SQ(RITO_BOW_RANGE))) {
        return 0;
    }
    off = actor->yawTowardsPlayer + 0x8000 - camYaw;
    return ABS(off) < RITO_BOW_CONE;
}

// Fills `out` with up to `max` DISTINCT actors, nearest first, switches before enemies:
// three arrows must never converge on one target. Returns how many were found.
static s32 MmForm_RitoBowFindTargets(PlayState* play, Actor** out, s32 max, s16 camYaw) {
    static const u8 sPasses[] = { ACTORCAT_SWITCH, ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_ENEMY };
    s32 found = 0;
    s32 pass;

    for (pass = 0; (pass < (s32)ARRAY_COUNT(sPasses)) && (found < max); pass++) {
        u8 wantSwitch = (sPasses[pass] != ACTORCAT_ENEMY);

        // Nearest first, one actor taken per sweep: the list is unordered and the volley
        // is three arrows deep, so a full re-sweep per arrow is cheaper than a sort.
        while (found < max) {
            Actor* best = NULL;
            f32 bestDist = SQ(RITO_BOW_RANGE);
            Actor* actor;
            s32 i;

            for (actor = play->actorCtx.actorLists[sPasses[pass]].head; actor != NULL; actor = actor->next) {
                if (wantSwitch && ((actor->id != ACTOR_OBJ_SWITCH) || !RITO_BOW_SWITCH_IS_TARGET(actor))) {
                    continue;
                }
                if (!MmForm_RitoBowIsInSight(actor, camYaw) || (actor->xyzDistToPlayerSq >= bestDist)) {
                    continue;
                }
                for (i = 0; i < found; i++) {
                    if (out[i] == actor) {
                        break;
                    }
                }
                if (i < found) {
                    continue; // already carries an arrow of its own
                }
                best = actor;
                bestDist = actor->xyzDistToPlayerSq;
            }
            if (best == NULL) {
                break;
            }
            out[found++] = best;
        }
    }
    return found;
}

// Ball-and-chain, not fire: shards on contact and the switch live the same frame.
// Obj_Ice_Poly is a CHILD of the switch it covers, which is how the switch is reached.
static void MmForm_RitoBowShatterIce(PlayState* play, Vec3f* at) {
    static Color_RGBA8 sIceWhite = { 250, 250, 250, 255 };
    static Color_RGBA8 sIceGray = { 180, 200, 230, 255 };
    s32 cat;

    for (cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;

        while (actor != NULL) {
            Actor* next = actor->next;

            if ((actor->id == ACTOR_OBJ_ICE_POLY) && (actor->update != NULL) &&
                (Math_Vec3f_DistXYZ(at, &actor->world.pos) < RITO_BOW_ICE_RADIUS)) {
                Vec3f vel = { 0.0f, 0.0f, 0.0f };
                Vec3f accel = { 0.0f, -1.0f, 0.0f };
                s32 i;

                for (i = 0; i < 8; i++) {
                    Vec3f pos;

                    pos.x = actor->world.pos.x + Rand_CenteredFloat(40.0f);
                    pos.y = actor->world.pos.y + (Rand_ZeroOne() * 70.0f);
                    pos.z = actor->world.pos.z + Rand_CenteredFloat(40.0f);
                    vel.x = Rand_CenteredFloat(6.0f);
                    vel.y = Rand_ZeroOne() * 6.0f;
                    vel.z = Rand_CenteredFloat(6.0f);
                    func_8002829C(play, &pos, &vel, &accel, &sIceWhite, &sIceGray, 350, 20);
                }
                // At the position, not on the actor: the actor is killed on the next line.
                Sfx_PlaySfxAtPos(&actor->world.pos, NA_SE_EV_ICE_BROKEN);
                if (actor->parent != NULL) {
                    actor->parent->params &= ~0x80; // the switch stops being frozen at once
                }
                Actor_Kill(actor);
            }
            actor = next;
        }
    }
}

static void MmForm_RitoBowClearArrows(void) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        sRitoBow.arrows[i].arrow = NULL;
    }
}

// EnArrow_Shoot asks before it re-derives its yaw from the camera. Answering 1 both
// claims the arrow and lays its aim down, pitch included, which the camera would flatten.
extern "C" u8 MmForm_RitoBowClaimArrow(PlayState* play, Actor* arrow) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        if (sRitoBow.arrows[i].arrow != arrow) {
            continue;
        }
        arrow->world.rot.y = Math_Vec3f_Yaw(&arrow->world.pos, &sRitoBow.arrows[i].goal);
        arrow->world.rot.x = Math_Vec3f_Pitch(&arrow->world.pos, &sRitoBow.arrows[i].goal);
        arrow->shape.rot = arrow->world.rot;
        return 1;
    }
    return 0;
}

// Bends every arrow in the air toward its point and shatters the ice it arrives at.
// Ticked ahead of every early return below: a volley outlives the state that fired it.
static void MmForm_RitoBowTickArrows(PlayState* play) {
    s32 i;

    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        Actor* arrow;
        Actor* live = NULL;
        s16 dYaw;
        s16 dPitch;

        arrow = sRitoBow.arrows[i].arrow;
        if (arrow == NULL) {
            continue;
        }
        // Prove the pointer before using it: an arrow that hit something is gone from the
        // list and its memory is already back in the pool.
        for (live = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; live != NULL; live = live->next) {
            if (live == arrow) {
                break;
            }
        }
        if (live == NULL) {
            sRitoBow.arrows[i].arrow = NULL;
            continue;
        }
        if (Math_Vec3f_DistXYZ(&arrow->world.pos, &sRitoBow.arrows[i].goal) < RITO_BOW_HIT_DIST) {
            MmForm_RitoBowShatterIce(play, &sRitoBow.arrows[i].goal);
            sRitoBow.arrows[i].arrow = NULL;
            continue;
        }
        dYaw = Math_Vec3f_Yaw(&arrow->world.pos, &sRitoBow.arrows[i].goal) - arrow->world.rot.y;
        dPitch = Math_Vec3f_Pitch(&arrow->world.pos, &sRitoBow.arrows[i].goal) - arrow->world.rot.x;
        arrow->world.rot.y += CLAMP(dYaw, -RITO_BOW_TURN, RITO_BOW_TURN);
        arrow->world.rot.x += CLAMP(dPitch, -RITO_BOW_TURN, RITO_BOW_TURN);
        arrow->shape.rot = arrow->world.rot;
        // Rotating alone curves nothing: EnArrow_Fly rides the velocity vector laid down
        // once, so it is rebuilt each frame from the new heading. Gravity drop goes with it.
        Actor_SetProjectileSpeed(arrow, RITO_BOW_ARROW_SPEED);
    }
}

// The untracked shot: it goes where it is pointed and never bends.
static Actor* MmForm_RitoBowSpawnArrow(PlayState* play, Player* player, s16 yaw, s16 pitch) {
    Vec3f* from = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    // EnArrow_Shoot kills any parentless arrow it finds while this countdown is clear.
    player->unk_A73 = RITO_BOW_HANDSHAKE;
    return Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from->x, from->y, from->z, pitch, yaw, 0, ARROW_NORMAL);
}

// The tracked shot: aimed at `goal` on the way out and bent toward it every frame after.
static void MmForm_RitoBowFireAt(PlayState* play, Player* player, s32 slot, Vec3f* goal) {
    Vec3f* from = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Actor* arrow = MmForm_RitoBowSpawnArrow(play, player, Math_Vec3f_Yaw(from, goal), Math_Vec3f_Pitch(from, goal));

    if (arrow != NULL) {
        sRitoBow.arrows[slot].arrow = arrow;
        Math_Vec3f_Copy(&sRitoBow.arrows[slot].goal, goal);
    }
}

static void MmForm_RitoBowLoose(PlayState* play, Player* player) {
    Actor* targets[RITO_BOW_VOLLEY];
    s16 camYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    s32 count = MmForm_RitoBowFindTargets(play, targets, RITO_BOW_VOLLEY, camYaw);
    s32 i;

    MmForm_RitoBowClearArrows();
    for (i = 0; i < RITO_BOW_VOLLEY; i++) {
        if (i < count) {
            Vec3f goal;

            MmForm_RitoBowAimPoint(targets[i], &goal);
            MmForm_RitoBowFireAt(play, player, i, &goal);
        } else {
            // Untargeted arrows fan, so a volley into an empty room still reads as three.
            MmForm_RitoBowSpawnArrow(play, player, camYaw + (s16)((i - 1) * 0x0500), 0);
        }
    }
}

static void MmForm_RitoBowPlay(PlayState* play, Player* player, LinkAnimationHeader* anim, u8 loop) {
    if (anim == NULL) {
        return;
    }
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
    sRitoBow.timer = 0;
}

static s32 MmForm_RitoBowAdvance(PlayState* play, Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    sRitoBow.timer++;
    return LinkAnimation_Update(play, &player->skelAnime);
}

// Walking with the bow drawn. The action function is paused, but OOT still integrates
// linearVelocity and gravity, so heading and speed are the whole of it.
static u8 MmForm_RitoBowGroundMove(PlayState* play, Player* player) {
    f32 stickMag;
    s16 stickAngle;
    s16 worldYaw;

    func_80077D10(&stickMag, &stickAngle, &play->state.input[0]);
    if (stickMag < RITO_BOW_MOVE_DEADZONE) {
        player->linearVelocity = 0.0f;
        return 0;
    }
    worldYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play)) + stickAngle;
    Math_ScaledStepToS(&player->yaw, worldYaw, RITO_BOW_TURN_RATE);
    player->actor.world.rot.y = player->yaw;
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity = RITO_BOW_MOVE_SPEED;
    return 1;
}

static void MmForm_RitoBowEnd(Player* player) {
    sRitoBow.state = RITO_BOW_OFF;
    sRitoBow.moving = 0;
    sRitoBow.timer = 0;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
}

extern "C" void MmForm_RitoBowReset(void) {
    sRitoBow.state = RITO_BOW_OFF;
    sRitoBow.moving = 0;
    sRitoBow.timer = 0;
    MmForm_RitoBowClearArrows();
}

// Returns 1 when the bow owns the frame. Called before the flight controller, so a press
// in mid-air takes the frame off the glide instead of racing it.
static u8 MmForm_RitoBowUpdate(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 bHeld;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    // Ahead of every early return below: a volley outlives the state that fired it.
    MmForm_RitoBowTickArrows(play);
    MmForm_RitoBowLoadClips();
    if (MmForm_InputOwnedByMessage()) {
        return 0;
    }
    bHeld = CHECK_BTN_ALL(input->cur.button, BTN_B);

    // Damage, water or a cutscene ends the draw wherever it is.
    if ((sRitoBow.state != RITO_BOW_OFF) &&
        (player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED))) {
        MmForm_RitoBowEnd(player);
        return 0;
    }

    switch (sRitoBow.state) {
        case RITO_BOW_AIR:
            // One press, and the rito hangs there for the length of the clip. Nothing is
            // held: the arrow goes down on the loose frame and the fall picks up after.
            player->actor.velocity.y = 0.0f;
            player->actor.gravity = 0.0f;
            player->linearVelocity = 0.0f;
            if (sRitoBow.timer == RITO_BOW_LOOSE_FRAME) {
                MmForm_RitoBowClearArrows();
                MmForm_RitoBowSpawnArrow(play, player, player->actor.shape.rot.y, 0x4000);
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                // Straight back into the fall, morphed rather than cut: the flight
                // controller owns the air again from the next frame.
                player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
                MmForm_RitoBowEnd(player);
                if (sRito.fly != NULL) {
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                }
                return 0;
            }
            return 1;

        case RITO_BOW_ENTER:
            MmForm_RitoBowGroundMove(play, player);
            if (MmForm_RitoBowAdvance(play, player)) {
                sRitoBow.state = RITO_BOW_HOLD;
                MmForm_RitoBowPlay(play, player, sRitoBow.moving ? sRitoBow.holdMove : sRitoBow.holdStill, 1);
            }
            return 1;

        case RITO_BOW_HOLD: {
            u8 moving = MmForm_RitoBowGroundMove(play, player);

            if (!bHeld) {
                sRitoBow.state = RITO_BOW_RELEASE;
                MmForm_RitoBowPlay(play, player, sRitoBow.release, 0);
                return 1;
            }
            if (moving != sRitoBow.moving) {
                sRitoBow.moving = moving;
                if (moving) {
                    // Standing still to moving is its own step-off, and it lands in the
                    // run loop rather than back in the entry the draw started from.
                    sRitoBow.state = RITO_BOW_ENTER;
                    MmForm_RitoBowPlay(play, player, sRitoBow.enterDash, 0);
                } else {
                    MmForm_RitoBowPlay(play, player, sRitoBow.holdStill, 1);
                }
                return 1;
            }
            MmForm_RitoBowAdvance(play, player);
            return 1;
        }

        case RITO_BOW_RELEASE:
            MmForm_RitoBowGroundMove(play, player);
            if (sRitoBow.timer == RITO_BOW_LOOSE_FRAME) {
                MmForm_RitoBowLoose(play, player);
            }
            if (MmForm_RitoBowAdvance(play, player)) {
                MmForm_RitoBowEnd(player);
                return 0;
            }
            return 1;

        default:
            break;
    }

    if (!CHECK_BTN_ALL(input->press.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        return 0;
    }
    if (!MMFORM_ON_GROUND(player)) {
        if (sRitoBow.air == NULL) {
            return 0;
        }
        sRitoBow.state = RITO_BOW_AIR;
        MmForm_RitoBowPlay(play, player, sRitoBow.air, 0);
        return 1;
    }
    if (sRitoBow.holdStill == NULL) {
        return 0;
    }
    func_80839FFC(player, play);
    sRitoBow.moving = (fabsf(player->linearVelocity) > 1.0f);
    sRitoBow.state = RITO_BOW_ENTER;
    MmForm_RitoBowPlay(play, player, sRitoBow.moving ? sRitoBow.enterMove : sRitoBow.enterStill, 0);
    return 1;
}
