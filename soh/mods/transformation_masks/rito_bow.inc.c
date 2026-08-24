/**
 * rito_bow.inc.c — the Rito's bow moveset (MHR clips from nei/bow_anims.o2r).
 *
 * ORDER MATTERS: text-included right AFTER rito_flight.inc.c, inside the same
 * extern "C" body. It reads sRito to know whether the flight controller owns the
 * body this frame, and reuses that file's clip loader / clip player wholesale.
 * No #include here: mm_player_form.cpp's own include block is fixed and adding a
 * header inside an extern "C" body is not safe.
 *
 * B belongs to the Rito outright — there is no bow item and no ammo:
 *   tap B          -> stab with the arrow in hand
 *   hold B         -> draw, aim, charge
 *   release        -> plain shot (free) or, fully charged, the special shot
 *                     (ground = arcing arrow that lands as a thunder ring,
 *                      air    = three arrows in a line). Those cost magic.
 */

// ovl_En_Arrow/z_en_arrow.h is not reachable from this translation unit; these are
// its ArrowType values. SW97_ELEM_* come in through extended_inventory.h -> nei_save.h.
#define RITO_ARROW_NORMAL 2
#define RITO_ARROW_SW97_FIRE 17

#define RITO_BOWA(name) RITO_ANIM("gPlayerAnim_mhr_bow_" name)

#define RITO_BOW_CLIP_DRAW RITO_BOWA("jump_attack07")
#define RITO_BOW_CLIP_SHEATHE RITO_BOWA("motion02")
#define RITO_BOW_CLIP_AIM RITO_BOWA("motion12")
#define RITO_BOW_CLIP_CHARGE RITO_BOWA("motion14")
#define RITO_BOW_CLIP_SHOT_ARC RITO_BOWA("charge_attack03")
#define RITO_BOW_CLIP_SHOT_AIR RITO_BOWA("charge_attack09")
#define RITO_BOW_CLIP_STAB RITO_BOWA("attack08")
#define RITO_BOW_CLIP_RUN RITO_BOWA("dash_attack10")
#define RITO_BOW_CLIP_RUN_AIM RITO_BOWA("dash_attack07")

// Logic frames: this runs at 20Hz (R_UPDATE_RATE = 3), so 20 == one second.
#define RITO_BOW_TAP_FRAMES 4        // B released inside this window is a stab, not a draw
#define RITO_BOW_CHARGE_FULL 20      // one second of hold arms the special shot
#define RITO_BOW_IDLE_STOW 50        // no B for this long and the bow goes away
#define RITO_BOW_CHARGE_MAGIC 12     // same price as getting airborne
#define RITO_BOW_AIM_PITCH_MAX 14000 // func_8084ABD8's own clamp with a weapon out
#define RITO_BOW_AIM_PITCH_RATE 90
#define RITO_BOW_ARC_PITCH (-0x1800) // lob for the ground special
#define RITO_BOW_AIR_SPREAD 0x0500   // pitch step between the three air arrows
#define RITO_BOW_SHOT_HEIGHT 40.0f

// Aim assist: bend the shot toward an enemy that is already roughly in front,
// never snap to it. EnArrow has no homing of its own.
#define RITO_BOW_ASSIST_RANGE 1200.0f
#define RITO_BOW_ASSIST_CONE 0x2000
#define RITO_BOW_ASSIST_BEND 0x0C00

// An arrow lives well under this; the entry is dropped afterwards so a recycled
// allocation can never inherit somebody else's thunder.
#define RITO_BOW_THUNDER_TTL 90
#define RITO_BOW_THUNDER_MAX 4

typedef enum {
    RITO_BOW_STOWED = 0,
    RITO_BOW_DRAW,
    RITO_BOW_AIM,
    RITO_BOW_CHARGE,
    RITO_BOW_SHOOT,
    RITO_BOW_STAB,
    RITO_BOW_SHEATHE,
} RitoBowState;

typedef struct {
    u8 loaded;
    u8 state;
    u8 holdsAnim; // the shot clip is on the body: the flight must not overwrite it
    s16 timer;
    s16 charge; // frames spent in CHARGE
    s16 idle;   // frames since the last B, drives the auto-stow
    s16 aimYaw;
    s16 aimPitch;
    PlayerActionFunc bowAction;
    struct {
        Actor* arrow;
        s16 ttl;
    } thunder[RITO_BOW_THUNDER_MAX];
    LinkAnimationHeader* draw;
    LinkAnimationHeader* sheathe;
    LinkAnimationHeader* aim;
    LinkAnimationHeader* chargeUp;
    LinkAnimationHeader* shotArc;
    LinkAnimationHeader* shotAir;
    LinkAnimationHeader* stab;
    LinkAnimationHeader* run;
    LinkAnimationHeader* runAim;
} RitoBow;

static RitoBow sRitoBow;

static struct {
    u8 installed;
    LinkAnimationHeader* savedRun[PLAYER_ANIMTYPE_MAX];
    LinkAnimationHeader* savedWalk[PLAYER_ANIMTYPE_MAX];
} sRitoBowTables;

// Locomotion while the bow is out, same swap the landing uses: OOT keeps owning
// the movement, it just plays the Rito's clip. Restored the moment it is stowed,
// on form exit and on reset — a leak here follows Link out of the transformation.
static void MmForm_RitoBowInstallRun(void) {
    s32 col;

    if (sRitoBowTables.installed || (sRitoBow.run == NULL)) {
        return;
    }
    sRitoBowTables.installed = 1;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        sRitoBowTables.savedRun[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_run, col);
        sRitoBowTables.savedWalk[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, sRitoBow.run);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, sRitoBow.run);
    }
}

extern "C" void MmForm_RitoBowRestoreRun(void) {
    s32 col;

    if (!sRitoBowTables.installed) {
        return;
    }
    sRitoBowTables.installed = 0;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, sRitoBowTables.savedRun[col]);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, sRitoBowTables.savedWalk[col]);
    }
}

static void MmForm_RitoBowLoadClips(void) {
    if (sRitoBow.loaded) {
        return;
    }
    sRitoBow.loaded = 1;
    sRitoBow.draw = MmForm_RitoLoadClip(RITO_BOW_CLIP_DRAW);
    sRitoBow.sheathe = MmForm_RitoLoadClip(RITO_BOW_CLIP_SHEATHE);
    sRitoBow.aim = MmForm_RitoLoadClip(RITO_BOW_CLIP_AIM);
    sRitoBow.chargeUp = MmForm_RitoLoadClip(RITO_BOW_CLIP_CHARGE);
    sRitoBow.shotArc = MmForm_RitoLoadClip(RITO_BOW_CLIP_SHOT_ARC);
    sRitoBow.shotAir = MmForm_RitoLoadClip(RITO_BOW_CLIP_SHOT_AIR);
    sRitoBow.stab = MmForm_RitoLoadClip(RITO_BOW_CLIP_STAB);
    sRitoBow.run = MmForm_RitoLoadClip(RITO_BOW_CLIP_RUN);
    sRitoBow.runAim = MmForm_RitoLoadClip(RITO_BOW_CLIP_RUN_AIM);
    if (sRitoBow.aim == NULL) {
        SPDLOG_WARN("[Rito] bow clips missing ({}) — B stays on the stab", RITO_BOW_CLIP_AIM);
    }
}

extern "C" void MmForm_RitoBowReset(void) {
    s32 i;

    MmForm_RitoBowRestoreRun();
    sRitoBow.state = RITO_BOW_STOWED;
    sRitoBow.holdsAnim = 0;
    sRitoBow.timer = 0;
    sRitoBow.charge = 0;
    sRitoBow.idle = 0;
    sRitoBow.aimPitch = 0;
    sRitoBow.bowAction = NULL;
    for (i = 0; i < RITO_BOW_THUNDER_MAX; i++) {
        sRitoBow.thunder[i].arrow = NULL;
        sRitoBow.thunder[i].ttl = 0;
    }
}

// The bow is out, so B is not OOT's sword any more (TransformMasks_FilterB).
extern "C" u8 MmForm_RitoBowOwnsB(void) {
    return gFormState.currentForm == MM_PLAYER_FORM_RITO;
}

// The flight controller asks this before advancing its own clip: a shot fired
// mid-glide has to keep the body for the length of the clip.
extern "C" u8 MmForm_RitoBowHoldsAnim(void) {
    return sRitoBow.holdsAnim;
}

extern "C" u8 MmForm_RitoBowIsAiming(void) {
    return (sRitoBow.state == RITO_BOW_AIM) || (sRitoBow.state == RITO_BOW_CHARGE);
}

extern "C" u8 MmForm_RitoBowIsOut(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_RITO) && (sRitoBow.state != RITO_BOW_STOWED);
}

static u8 MmForm_RitoBowSpendMagic(void) {
    if ((gSaveContext.magicCapacity <= 0) || (gSaveContext.magic < RITO_BOW_CHARGE_MAGIC)) {
        return 0;
    }
    gSaveContext.magic -= RITO_BOW_CHARGE_MAGIC;
    return 1;
}

// Rito arrows are his own, so no ammo is touched — but the SW97 element still
// rides them, read exactly the way func_80834380 reads it for the real bow.
static s16 MmForm_RitoBowArrowType(void) {
    u8 elem = Sw97_EffectiveElement(0);

    if ((elem >= SW97_ELEM_FIRE) && (elem <= SW97_ELEM_WIND)) {
        return RITO_ARROW_SW97_FIRE + (elem - SW97_ELEM_FIRE);
    }
    return RITO_ARROW_NORMAL;
}

static void MmForm_RitoBowNoteThunder(Actor* arrow) {
    s32 i;

    for (i = 0; i < RITO_BOW_THUNDER_MAX; i++) {
        if (sRitoBow.thunder[i].arrow == NULL) {
            sRitoBow.thunder[i].arrow = arrow;
            sRitoBow.thunder[i].ttl = RITO_BOW_THUNDER_TTL;
            return;
        }
    }
}

static void MmForm_RitoBowTickThunder(void) {
    s32 i;

    for (i = 0; i < RITO_BOW_THUNDER_MAX; i++) {
        if ((sRitoBow.thunder[i].arrow != NULL) && (--sRitoBow.thunder[i].ttl <= 0)) {
            sRitoBow.thunder[i].arrow = NULL;
        }
    }
}

// Called from EnArrow_Fly the frame an arrow sticks in terrain. The pointer is
// alive by construction (the arrow is calling about itself), so the only thing
// the list has to defend against is a stale entry, which the TTL handles.
extern "C" void MmForm_RitoBowOnArrowStick(PlayState* play, Actor* arrow) {
    s32 i;

    for (i = 0; i < RITO_BOW_THUNDER_MAX; i++) {
        if (sRitoBow.thunder[i].arrow != arrow) {
            continue;
        }
        sRitoBow.thunder[i].arrow = NULL;
        // EN_M_THUNDER_STATIC_FLAG (0x40) | swordType+1: the release ring, fired where
        // it was spawned instead of being worn by the player. No magic (high byte 0).
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, arrow->world.pos.x, arrow->world.pos.y,
                    arrow->world.pos.z, 0, 0, 0, 0x41);
        return;
    }
}

extern "C" u8 MmForm_RitoBowOwnsArrow(Actor* arrow) {
    s32 i;

    for (i = 0; i < RITO_BOW_THUNDER_MAX; i++) {
        if (sRitoBow.thunder[i].arrow == arrow) {
            return 1;
        }
    }
    return 0;
}

static void MmForm_RitoBowAssist(PlayState* play, Vec3f* from, s16* yaw, s16* pitch) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head;
    Actor* best = NULL;
    s16 bestOff = RITO_BOW_ASSIST_CONE;

    for (; actor != NULL; actor = actor->next) {
        if ((actor->update == NULL) || (actor->xyzDistToPlayerSq > SQ(RITO_BOW_ASSIST_RANGE))) {
            continue;
        }
        s16 off = Math_Vec3f_Yaw(from, &actor->focus.pos) - *yaw;
        if (ABS(off) < bestOff) {
            bestOff = ABS(off);
            best = actor;
        }
    }
    if (best == NULL) {
        return;
    }
    s16 dYaw = Math_Vec3f_Yaw(from, &best->focus.pos) - *yaw;
    s16 dPitch = Math_Vec3f_Pitch(from, &best->focus.pos) - *pitch;
    *yaw += CLAMP(dYaw, -RITO_BOW_ASSIST_BEND, RITO_BOW_ASSIST_BEND);
    *pitch += CLAMP(dPitch, -RITO_BOW_ASSIST_BEND, RITO_BOW_ASSIST_BEND);
}

static void MmForm_RitoBowFire(PlayState* play, Player* player, s16 pitch, u8 thunder) {
    Vec3f from = player->actor.world.pos;
    s16 yaw = sRitoBow.aimYaw;
    Actor* arrow;

    from.y += RITO_BOW_SHOT_HEIGHT;
    MmForm_RitoBowAssist(play, &from, &yaw, &pitch);

    // EnArrow_Shoot kills any arrow whose parent is NULL unless the player is on the
    // fire frame — func_808350A4 sets this too, and it is the whole handshake.
    player->unk_A73 = 4;
    arrow = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from.x, from.y, from.z, pitch, yaw, 0,
                        MmForm_RitoBowArrowType());
    if ((arrow != NULL) && thunder) {
        MmForm_RitoBowNoteThunder(arrow);
    }
    // EnArrow_Shoot plays NA_SE_IT_ARROW_SHOT itself.
}

static void MmForm_RitoBowAimCamera(PlayState* play) {
    Camera* cam = GET_ACTIVE_CAM(play);

    // BOWARROWZ is already the over-the-shoulder aim (eyeDist 70 / atOffsetX -120 in
    // sSetNormal0ModeBowArrowZData); vanilla only reaches it through Z-targeting.
    // Ask first: ChangeMode falls back to CAM_MODE_NORMAL for a mode the current
    // setting does not list, which would yank the camera every frame.
    if (Camera_CheckValidMode(cam, CAM_MODE_BOWARROWZ)) {
        Camera_ChangeMode(cam, CAM_MODE_BOWARROWZ);
    }
}

static void MmForm_RitoBowAim(PlayState* play, Player* player, Input* input) {
    s32 pitch;

    sRitoBow.aimYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    pitch = sRitoBow.aimPitch - ((input->rel.stick_y * RITO_BOW_AIM_PITCH_RATE) / 60);
    sRitoBow.aimPitch = CLAMP(pitch, -RITO_BOW_AIM_PITCH_MAX, RITO_BOW_AIM_PITCH_MAX);
    player->actor.shape.rot.y = sRitoBow.aimYaw;
    player->actor.world.rot.y = sRitoBow.aimYaw;
    MmForm_RitoBowAimCamera(play);
}

static u8 MmForm_RitoBowRunning(Player* player, u8 onGround) {
    return onGround && (fabsf(player->linearVelocity) > 1.0f);
}

// dash_attack07 is the wind-up taken at a run; the standing one is motion14.
static LinkAnimationHeader* MmForm_RitoBowChargeClip(Player* player, u8 onGround) {
    if (MmForm_RitoBowRunning(player, onGround) && (sRitoBow.runAim != NULL)) {
        return sRitoBow.runAim;
    }
    return sRitoBow.chargeUp;
}

static void MmForm_RitoBowTake(PlayState* play, Player* player) {
    if (MMFORM_ON_GROUND(player)) {
        func_80839FFC(player, play);
        sRitoBow.bowAction = player->actionFunc;
    }
}

static void MmForm_RitoBowEnd(Player* player) {
    sRitoBow.state = RITO_BOW_STOWED;
    sRitoBow.holdsAnim = 0;
    sRitoBow.charge = 0;
    sRitoBow.idle = 0;
    MmForm_RitoBowRestoreRun();
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
}

// Returns 1 on the frames it is driving the body, exactly like the flight controller.
static u8 MmForm_RitoBowUpdate(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 bHeld;
    u8 onGround;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    MmForm_RitoBowLoadClips();
    MmForm_RitoBowTickThunder();
    if (MmForm_InputOwnedByMessage()) {
        return 0;
    }
    if (sRitoBow.aim == NULL) {
        return 0; // no clips: leave B alone entirely
    }

    bHeld = CHECK_BTN_ALL(input->cur.button, BTN_B);
    onGround = MMFORM_ON_GROUND(player) != 0;
    sRitoBow.idle = bHeld ? 0 : (sRitoBow.idle + 1);
    // While the bow is out it owns the body, so the flight controller must not
    // advance its own clip underneath it (MmForm_RitoBowHoldsAnim).
    sRitoBow.holdsAnim = (sRitoBow.state != RITO_BOW_STOWED);

    if (sRitoBow.state != RITO_BOW_STOWED) {
        // Running with the bow out hands the body BACK to vanilla on purpose, so there
        // the actionFunc no longer matches the snapshot and the identity check below
        // cannot apply — the explicit state flags are what ends the bow in that case.
        u8 vanillaOwnsBody = (sRitoBow.state == RITO_BOW_AIM) && MmForm_RitoBowRunning(player, onGround);

        if (player->stateFlags1 &
            (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DEAD)) {
            MmForm_RitoBowEnd(player);
            return 0;
        }
        if (!vanillaOwnsBody && (sRitoBow.bowAction != NULL) && onGround &&
            (player->actionFunc != sRitoBow.bowAction)) {
            MmForm_RitoBowEnd(player);
            return 0;
        }
    }

    switch (sRitoBow.state) {
        case RITO_BOW_STAB:
            if (MmForm_RitoAdvance(play, player) || (++sRitoBow.timer > 20)) {
                MmForm_RitoBowEnd(player);
                return 0;
            }
            return 1;

        case RITO_BOW_DRAW:
            // Let go inside the tap window and it was never a draw: it was a stab.
            if (!bHeld && (sRitoBow.timer < RITO_BOW_TAP_FRAMES)) {
                sRitoBow.state = RITO_BOW_STAB;
                sRitoBow.timer = 0;
                MmForm_RitoPlay(play, player, sRitoBow.stab, 0, 1.0f);
                return 1;
            }
            sRitoBow.timer++;
            if (!MmForm_RitoAdvance(play, player)) {
                return 1;
            }
            sRitoBow.state = RITO_BOW_AIM;
            MmForm_RitoBowInstallRun();
            MmForm_RitoPlay(play, player, sRitoBow.aim, 0, 1.0f);
            return 1;

        case RITO_BOW_AIM: {
            // Bow out and ready. The nock plays out first, then B starts the charge.
            u8 running = MmForm_RitoBowRunning(player, onGround);

            MmForm_RitoBowAim(play, player, input);
            if (running) {
                // Let OOT animate: the group swap already points its run clip at the
                // Rito's. Aim and camera keep tracking regardless.
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                sRitoBow.bowAction = player->actionFunc;
            } else if (!MmForm_RitoAdvance(play, player)) {
                return 1;
            }
            if (bHeld) {
                sRitoBow.state = RITO_BOW_CHARGE;
                sRitoBow.charge = 0;
                MmForm_RitoPlay(play, player, MmForm_RitoBowChargeClip(player, onGround), 0, 1.0f);
                return 1;
            }
            if (sRitoBow.idle > RITO_BOW_IDLE_STOW) {
                sRitoBow.state = RITO_BOW_SHEATHE;
                sRitoBow.timer = 0;
                MmForm_RitoPlay(play, player, sRitoBow.sheathe, 0, 1.0f);
                return 1;
            }
            return running ? 0 : 1;
        }

        case RITO_BOW_CHARGE:
            MmForm_RitoBowAim(play, player, input);
            if (onGround && !MmForm_RitoBowRunning(player, onGround)) {
                player->linearVelocity = 0.0f;
            }
            if (++sRitoBow.charge == RITO_BOW_CHARGE_FULL) {
                Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_CHARGE); // the shot is armed
            }
            // The wind-up parks on its last frame; holding B is what keeps it there.
            if (MmForm_RitoAdvance(play, player)) {
                player->skelAnime.playSpeed = 0.0f;
            }
            if (bHeld) {
                return 1;
            }
            sRitoBow.state = RITO_BOW_SHOOT;
            sRitoBow.timer = 0;
            if (sRitoBow.charge < RITO_BOW_CHARGE_FULL) {
                MmForm_RitoPlay(play, player, onGround ? sRitoBow.shotArc : sRitoBow.shotAir, 0, 1.0f);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch, 0);
                return 1;
            }
            if (!MmForm_RitoBowSpendMagic()) {
                Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                MmForm_RitoPlay(play, player, onGround ? sRitoBow.shotArc : sRitoBow.shotAir, 0, 1.0f);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch, 0);
                return 1;
            }
            if (onGround) {
                // One lobbed arrow; the thunder ring goes off where it lands.
                MmForm_RitoPlay(play, player, sRitoBow.shotArc, 0, 1.0f);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch + RITO_BOW_ARC_PITCH, 1);
            } else {
                // Three in a line, fanned down-forward.
                MmForm_RitoPlay(play, player, sRitoBow.shotAir, 0, 1.0f);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch + RITO_BOW_AIR_SPREAD, 0);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch, 0);
                MmForm_RitoBowFire(play, player, sRitoBow.aimPitch - RITO_BOW_AIR_SPREAD, 0);
            }
            return 1;

        case RITO_BOW_SHOOT:
            MmForm_RitoBowAimCamera(play);
            if (MmForm_RitoAdvance(play, player) || (++sRitoBow.timer > 24)) {
                sRitoBow.state = RITO_BOW_AIM; // re-nock
                MmForm_RitoPlay(play, player, sRitoBow.aim, 0, 1.0f);
            }
            return 1;

        case RITO_BOW_SHEATHE:
            if (MmForm_RitoAdvance(play, player) || (++sRitoBow.timer > 20)) {
                MmForm_RitoBowEnd(player);
                return 0;
            }
            return 1;

        default:
            break;
    }

    // Stowed. B is the only way in — but not on top of a flight move that is already
    // driving the body. Gliding is the one that composes: you can shoot out of it.
    if (!CHECK_BTN_ALL(input->press.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        return 0;
    }
    if ((sRito.state != RITO_FLY_OFF) && (sRito.state != RITO_FLY_GLIDE)) {
        return 0;
    }
    MmForm_RitoBowTake(play, player);
    sRitoBow.state = RITO_BOW_DRAW;
    sRitoBow.timer = 0;
    sRitoBow.aimPitch = 0;
    MmForm_RitoPlay(play, player, sRitoBow.draw, 0, 1.0f);
    return 1;
}
