/**
 * rito_flight.inc.c — the Rito form's flight mode.
 *
 * Text-included at the END of mm_player_form.cpp (same arrangement as
 * gerudo_mhr_combat.inc.c), so it sees gFormState without exporting it.
 *
 * THE MOVE
 *   press A            -> an updraft starts charging under the rito (wind builds)
 *   release A          -> it rides that wind straight up
 *   hold A in the air  -> glide: a Zora swim, in 3D, in the air. Free.
 *   let go of A        -> the wings stop and it falls
 * A is the whole interface: as a rito, A IS flight mode.
 *
 * HOW IT DRIVES LINK — copied from the Trident's flight (equip_trident.c), which
 * is the shipped precedent for "a clip-driven flying player":
 *   - clips are loaded RESAMPLED. An MHR clip is authored at its own rate and its
 *     frameCount means nothing to this engine; handing the raw header to
 *     LinkAnimation_Change is what makes a move look frozen or absurdly slow.
 *     Trident_LoadHalf does the same division and it is why its clips read right.
 *   - the clip plays on player->skelAnime with PLAYER_STATE3_PAUSE_ACTION_FUNC set
 *     at EVERY clip start (a vanilla path that clears it in between would
 *     otherwise advance the clip twice), and is ticked with LinkAnimation_Update.
 *   - the rito's body follows because MmForm_UsesOotAnim copies OOT's joints onto
 *     the form by default. These states are deliberately NOT listed as
 *     form-specific there, so the pose the clip puts on Link IS what gets drawn.
 *
 * The bow lives next door in rito_bow.inc.c, on B. The two compose: you can shoot
 * out of a glide, and neither may advance the other's clip (MmForm_RitoBowHoldsAnim).
 *
 * ORDER MATTERS: include AFTER gerudo_mhr_combat.inc.c — its MmForm_MhrLoadPath
 * is reused for the graceful "clip missing → move disabled" lookup.
 */

extern u8 MmForm_InputOwnedByMessage(void);

// z_player.c entry points this file drives — same declaration style as the block
// at the top of gerudo_mhr_combat.inc.c, which is included just before this one
// and therefore in the same linkage context. `func_80839FFC` is the clean idle
// action the Trident's flight also lays down before taking over.
extern void func_80839FFC(Player* player, PlayState* play);
extern LinkAnimationHeader* ExtPlayer_GetAnimGroupAnim(s32 group, s32 animType);
extern void ExtPlayer_SetAnimGroupAnim(s32 group, s32 animType, LinkAnimationHeader* anim);

// rito_bow.inc.c is included right after this file; the glide asks it whether the
// bow currently owns the body so the two never advance the same track twice.
extern u8 MmForm_RitoBowHoldsAnim(void);

// ── clips ───────────────────────────────────────────────────────────────────
#define RITO_ANIM(name) "__OTR__misc/link_animetion/" name

#define RITO_CLIP_LAUNCH RITO_ANIM("gMonsterHunterRise_InsectGlaive_BackwardRisingDoubleChargedStaffCombo")
#define RITO_CLIP_FLY RITO_ANIM("gPlayerAnim_mhr_npc_takkuri_fly")
#define RITO_CLIP_LAND RITO_ANIM("gMonsterHunterRise_DualBlade_ForwardRisingAerialMove")
#define RITO_CLIP_HOP RITO_ANIM("gMonsterHunterRise_InsectGlaive_StationaryHighAerialTripleSilkbindStaffStrike")
#define RITO_CLIP_BACKFLIP \
    RITO_ANIM("gMonsterHunterRise_InsectGlaive_BackwardHighAerialMultiHitSilkbindStaffStrike_Variant06")
#define RITO_CLIP_THROW RITO_ANIM("gMonsterHunterRise_InsectGlaive_ForwardDoubleStaffStrike")

// Playback rate the MHR clips are resampled to, exactly like TRI_ANIM_SPEED.
#define RITO_ANIM_SPEED 2.0f
#define RITO_THROW_SPEED 3.0f // "1.5x" on top of the 2.0 the others are resampled at

// ── tuning ──────────────────────────────────────────────────────────────────
// Frames are LOGIC frames: this runs at 20Hz (R_UPDATE_RATE = 3), so 20 = 1 second.
#define RITO_CHARGE_MIN 6                           // shortest useful charge
#define RITO_CHARGE_FULL 24                         // fully charged updraft
#define RITO_LAUNCH_MIN 8.0f                        // a minimum charge is about one Roc's Feather
#define RITO_LAUNCH_MAX (RITO_ROCS_VELOCITY * 4.0f) // a full charge is 4x Roc's Feather
#define RITO_GLIDE_SPEED 4.5f                       // half of the Zora's swim, as asked
#define RITO_GLIDE_YAW 0x300                        // vs the swim's 0x640: turning is heavier in the air
#define RITO_TURN_RATE 5.0f                         // Pegasus-dash carve: a small, slow bank
#define RITO_TURN_DEADZONE 10.0f                    // Pegasus uses the same threshold
#define RITO_GLIDE_SINK -0.35f                      // gentle loss of height while gliding level
#define RITO_FLY_GRAVITY_DEFAULT -1.2f              // vanilla fall, the moment A is released
// Magic is charged for GETTING airborne, never for staying there: the launch bills
// once, gliding is free. (Roc's Feather / Roc's Cape in mid-air cost RITO_ROCS_AIR_COST
// each — that hook lives with those items, not here.)
#define RITO_LAUNCH_MAGIC_COST 12
#define RITO_ROCS_AIR_COST 12

#define RITO_DRAW_Y_BASE -1059.0f                        // 2 world units lower again
#define RITO_LAND_LIFT 1500.0f                           // +15 world units, so the landing clip stands normally
#define RITO_HOP_SCALE 1.5f                              // roll     -> 1.5 Roc's Feather jumps
#define RITO_BACKFLIP_SCALE 1.0f                         // backflip -> exactly 1
#define RITO_ROCS_VELOCITY (LINK_IS_ADULT ? 7.5f : 7.0f) // RocsFeather.cpp's own values

typedef enum {
    RITO_FLY_OFF = 0,
    RITO_FLY_CHARGE, // A held: the updraft builds
    RITO_FLY_LAUNCH, // A released: riding it up
    RITO_FLY_GLIDE,  // A held in the air: Zora swim in 3D
    RITO_FLY_FALL,   // A released in the air: falling
    RITO_FLY_LAND,
    RITO_FLY_HOP,   // roll / backflip
    RITO_FLY_THROW, // mid-air item
} RitoFlyState;

typedef struct {
    u8 loaded;
    u8 state;
    s16 charge; // frames A has been held on the ground
    s16 magicTimer;
    s16 timer;
    s16 yaw;                    // heading, accumulated the same way
    PlayerActionFunc flyAction; // actionFunc at takeoff; a change means something took over
    LinkAnimationHeader* charge_;
    LinkAnimationHeader* launch;
    LinkAnimationHeader* fly;
    LinkAnimationHeader* land;
    LinkAnimationHeader* hop;
    LinkAnimationHeader* backflip;
    LinkAnimationHeader* throwItem;
} RitoFlight;

static RitoFlight sRito;

static struct {
    u8 installed;
    LinkAnimationHeader* savedLanding[PLAYER_ANIMTYPE_MAX];
    LinkAnimationHeader* savedShort[PLAYER_ANIMTYPE_MAX];
} sRitoAnimTables;

// Landing the way the Gerudo does it. She does NOT hand-play a touchdown clip: she
// swaps the clip OOT's own landing groups point at (PLAYER_ANIMGROUP_landing /
// short_landing, sMhrGroupBindings). Then EVERY landing uses it -- off a glide, off a
// plain fall, off a hop -- with vanilla still owning the landing logic. Hand-playing
// it, which is what this file did, only ever covered the glide path, which is why
// dropping out of the air landed on Link's animation.
static void MmForm_RitoInstallLanding(void) {
    s32 col;

    if (sRitoAnimTables.installed || (sRito.land == NULL)) {
        return;
    }
    sRitoAnimTables.installed = 1;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        sRitoAnimTables.savedLanding[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col);
        sRitoAnimTables.savedShort[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col, sRito.land);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col, sRito.land);
    }
}

// Restored on form exit so nothing leaks into Link.
extern "C" void MmForm_RitoRestoreLanding(void) {
    s32 col;

    if (!sRitoAnimTables.installed) {
        return;
    }
    sRitoAnimTables.installed = 0;
    for (col = 0; col < PLAYER_ANIMTYPE_MAX; col++) {
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_landing, col, sRitoAnimTables.savedLanding[col]);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_short_landing, col, sRitoAnimTables.savedShort[col]);
    }
}

// Resampled load — see the header note. Without this the clip plays at whatever
// rate it was authored at and reads as "no animation".
static LinkAnimationHeader* MmForm_RitoLoadClip(const char* path) {
    LinkAnimationHeader* raw;
    s16 frames;

    if ((path == NULL) || !ResourceMgr_FileExists(path)) {
        return NULL;
    }
    raw = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (raw == NULL) {
        return NULL;
    }
    frames = (s16)(((f32)raw->common.frameCount / RITO_ANIM_SPEED) + 0.5f);
    if (frames < 2) {
        frames = 2; // a 1-frame clip would finish the instant it starts
    }
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, frames);
}

static void MmForm_RitoLoadClips(void) {
    if (sRito.loaded) {
        return;
    }
    sRito.loaded = 1;
    // The wing wind-up is OOT's own bow guard pose, taken as-is. It must NOT go through
    // MmForm_RitoLoadClip: that resampler rewrites the resource IN PLACE, so resampling a
    // vanilla clip would corrupt it for every Link that plays it afterwards.
    sRito.charge_ = (LinkAnimationHeader*)&gPlayerAnim_link_bow_defense_wait;
    sRito.launch = MmForm_RitoLoadClip(RITO_CLIP_LAUNCH);
    sRito.fly = MmForm_RitoLoadClip(RITO_CLIP_FLY);
    sRito.land = MmForm_RitoLoadClip(RITO_CLIP_LAND);
    sRito.hop = MmForm_RitoLoadClip(RITO_CLIP_HOP);
    sRito.backflip = MmForm_RitoLoadClip(RITO_CLIP_BACKFLIP);
    sRito.throwItem = MmForm_RitoLoadClip(RITO_CLIP_THROW);
    MmForm_RitoInstallLanding();
    if (sRito.fly == NULL) {
        SPDLOG_WARN("[Rito] flight clip missing ({}) — A will not fly", RITO_CLIP_FLY);
    }
}

// PAUSE is re-armed at every clip start, not once on entry: a vanilla path that
// cleared it in between would advance the clip a second time (Trident_StartClip's
// own reason for doing it here).
static void MmForm_RitoPlay(PlayState* play, Player* player, LinkAnimationHeader* anim, u8 loop, f32 speed) {
    f32 last;
    f32 start;
    f32 end;

    if (anim == NULL) {
        return;
    }
    last = Animation_GetLastFrame(anim);
    start = (speed < 0.0f) ? last : 0.0f; // a negative speed means "play it backwards"
    end = (speed < 0.0f) ? 0.0f : last;

    // PAUSE is re-armed at EVERY clip start, and again every frame in the states
    // below: Player_UpdateCommon clears it (MmForm_GerudoPlant carries the same note).
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;

    // ONE track: player->skelAnime. The rito's body follows because MmForm_UsesOotAnim
    // copies OOT's joints onto the form by default — so this action must NOT be listed
    // as form-specific there. Doing both (driving formSkelAnime *and* flagging the
    // action) is what broke the poses.
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, loop ? ANIMMODE_LOOP : ANIMMODE_ONCE,
                         -4.0f);
    sRito.timer = 0;
}

// Advance both tracks. Returns 1 when a one-shot clip has finished.
static s32 MmForm_RitoAdvance(PlayState* play, Player* player) {
    s32 done;

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC; // see the note above
    return LinkAnimation_Update(play, &player->skelAnime);
}

extern "C" void MmForm_RitoResetFlight(void) {
    MmForm_RitoRestoreLanding();
    sRito.state = RITO_FLY_OFF;
    sRito.charge = 0;
    sRito.magicTimer = 0;
    sRito.timer = 0;
    sRito.yaw = 0;
    sRito.flyAction = NULL;
}

// Roc's Feather / Roc's Cape in mid-air: a rito may use them as many times as it
// likes, but each use costs magic. Answers 1 AND bills the cost, so the caller only
// has to ask once. Anything not a rito, on the ground, or short on magic gets 0 and
// keeps whatever limit it already had.
// How far below the actor the rito's model is drawn. The mesh is correct in Blender;
// it simply sits high on the shared rig, so this is a DRAW-time offset, the same tool
// the Deku uses for its flower depth. shape.yOffset is MODEL space and the actor draws
// at scale 0.01, so 1 world unit = 100 here.
//
// The landing clip is authored standing on the floor, so during it the model must come
// back UP by RITO_LAND_LIFT or the rito sinks through the ground as it touches down.
extern "C" f32 MmForm_RitoDrawYOffset(Player* player) {
    if ((player != NULL) && (sRito.land != NULL) && (player->skelAnime.animation == sRito.land)) {
        return RITO_DRAW_Y_BASE + RITO_LAND_LIFT;
    }
    return RITO_DRAW_Y_BASE;
}

extern "C" u8 MmForm_RitoAirRocsAllowed(Player* player) {
    if ((gFormState.currentForm != MM_PLAYER_FORM_RITO) || (player == NULL)) {
        return 0;
    }
    if (MMFORM_ON_GROUND(player)) {
        return 0; // on the ground the item behaves exactly as it always has
    }
    if ((gSaveContext.magicCapacity <= 0) || (gSaveContext.magic < RITO_ROCS_AIR_COST)) {
        return 0;
    }
    gSaveContext.magic -= RITO_ROCS_AIR_COST;
    return 1;
}

extern "C" u8 MmForm_RitoIsFlying(void) {
    return (gFormState.currentForm == MM_PLAYER_FORM_RITO) &&
           ((sRito.state == RITO_FLY_LAUNCH) || (sRito.state == RITO_FLY_GLIDE) || (sRito.state == RITO_FLY_FALL));
}

// Magic is billed for TIME in the air, not per wing beat: hovering is not cheaper
// than going somewhere. 0 means the meter just ran dry.
static u8 MmForm_RitoSpendLaunchMagic(void) {
    if ((gSaveContext.magicCapacity <= 0) || (gSaveContext.magic < RITO_LAUNCH_MAGIC_COST)) {
        return 0;
    }
    gSaveContext.magic -= RITO_LAUNCH_MAGIC_COST;
    return 1;
}

// The updraft: dust rising around the rito, denser as the charge fills. Same idea
// (and the same effect call) as the Trident's flight wind, which is deliberate —
// it already reads as "the air is moving" without being a tornado.
static Color_RGBA8 sRitoWindPrim = { 235, 240, 255, 140 };
static Color_RGBA8 sRitoWindEnv = { 130, 160, 200, 0 };

static void MmForm_RitoWind(PlayState* play, Player* player, f32 strength) {
    Vec3f pos;
    Vec3f vel;
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    s32 i;
    s32 count = 2 + (s32)(4.0f * strength); // denser as the charge fills

    // A ring around the feet, rising. One mote every other frame (the first cut of
    // this) is invisible under a standing player — this is what "the wind does not
    // show up" was.
    for (i = 0; i < count; i++) {
        f32 ang = Rand_ZeroFloat(6.28f);
        f32 rad = 8.0f + Rand_ZeroFloat(22.0f);
        pos = player->actor.world.pos;
        pos.x += Math_SinF(ang) * rad;
        pos.z += Math_CosF(ang) * rad;
        pos.y += Rand_ZeroFloat(10.0f);
        vel.x = Math_SinF(ang) * 0.4f;
        vel.y = 3.0f + (6.0f * strength);
        vel.z = Math_CosF(ang) * 0.4f;
        EffectSsDust_Spawn(play, 0, &pos, &vel, &accel, &sRitoWindPrim, &sRitoWindEnv, 90, 16, 10, 0);
    }
}

static void MmForm_RitoEnterAir(PlayState* play, Player* player) {
    // A clean idle action underneath, then PAUSE on top of it, so nothing of
    // vanilla's runs while the flight owns Link (Trident_FlyEnter's opening).
    func_80839FFC(player, play);
    sRito.flyAction = player->actionFunc;
    player->actor.bgCheckFlags &= ~1; // leave the ground this frame
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->stateFlags1 |= PLAYER_STATE1_JUMPING;
    Camera_ChangeMode(GET_ACTIVE_CAM(play), CAM_MODE_JUMP);
    sRito.yaw = player->actor.shape.rot.y; // keep facing where it took off
}

static void MmForm_RitoRelease(Player* player, u8 land) {
    player->stateFlags3 &= ~(PLAYER_STATE3_PAUSE_ACTION_FUNC | PLAYER_STATE3_MIDAIR);
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
    player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
    player->actor.minVelocityY = -20.0f;
    sRito.state = land ? RITO_FLY_LAND : RITO_FLY_OFF;
}

// Zora free-swim, in the air. Straight off Odolwa's moth-cloud flight
// (BossRemains_OdolwaFlightTick): yaw and pitch are ACCUMULATED from the stick and
// INVERTED (stick up = nose down) — that is what makes it a free 3D swim instead of
// "aim somewhere and go". The rito flies slower and turns lazier than the cloud.
static void MmForm_RitoGlideMove(PlayState* play, Player* player) {
    Input* in = &play->state.input[0];

    // Steering is a slow BANK and NOTHING else: sides only, gentle enough that you
    // cannot spin round to look behind you. Rate and deadzone are the Pegasus Boots
    // dash's (equip_pegasus.c:265), which is the small-turn feel asked for.
    if (fabsf(in->rel.stick_x) > RITO_TURN_DEADZONE) {
        sRito.yaw -= (s16)(in->rel.stick_x * RITO_TURN_RATE);
    }
    player->actor.world.rot.y = sRito.yaw;
    player->actor.shape.rot.y = sRito.yaw;
    player->yaw = sRito.yaw;

    // Forward only. The stick does NOT aim up or down — no climbing, no diving; the
    // rito flies level and sinks slowly, and altitude comes from the launch alone.
    player->linearVelocity = RITO_GLIDE_SPEED;
    player->actor.velocity.y = RITO_GLIDE_SINK;
    player->actor.gravity = 0.0f;
}

// ── the controller ──────────────────────────────────────────────────────────
// Returns 1 on the frames it is driving Link, mirroring MmForm_GerudoMhrUpdate.
static u8 MmForm_RitoFlightUpdate(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    u8 aHeld;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    MmForm_RitoLoadClips();
    if (MmForm_InputOwnedByMessage()) {
        return 0; // a textbox or the ocarina owns the buttons
    }
    aHeld = CHECK_BTN_ALL(input->cur.button, BTN_A);

    // Anything that takes Link away (damage, water, a cutscene) ends the flight.
    if ((sRito.state != RITO_FLY_OFF) && (sRito.flyAction != NULL) && (player->actionFunc != sRito.flyAction) &&
        (sRito.state != RITO_FLY_CHARGE)) {
        MmForm_RitoRelease(player, 0);
        return 0;
    }

    switch (sRito.state) {
        case RITO_FLY_CHARGE: {
            // Wind builds under the rito while A is down. The clip is held on the
            // wings-up frame instead of looping.
            sRito.charge++;
            player->linearVelocity = 0.0f;
            // Tick it. LinkAnimation_Change only ARMS the clip — Update is what writes
            // the pose into the joint table, so a state that never ticked (this one)
            // showed no animation at all on the ground.
            MmForm_RitoAdvance(play, player);
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            MmForm_RitoWind(play, player, (f32)sRito.charge / RITO_CHARGE_FULL);
            if ((sRito.charge % 4) == 0) {
                Player_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
            }

            if (aHeld) {
                return 1; // keep charging
            }
            // Released: ride the wind up. A short tap gives a small hop of a launch.
            if (sRito.charge < RITO_CHARGE_MIN) {
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            // THE one charge: getting off the ground. Nothing else about flying costs.
            if (!MmForm_RitoSpendLaunchMagic()) {
                Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            {
                f32 t = (f32)sRito.charge / RITO_CHARGE_FULL;
                if (t > 1.0f) {
                    t = 1.0f;
                }
                MmForm_RitoEnterAir(play, player);
                player->actor.velocity.y = RITO_LAUNCH_MIN + ((RITO_LAUNCH_MAX - RITO_LAUNCH_MIN) * t);
                player->actor.gravity = 0.0f;
                sRito.state = RITO_FLY_LAUNCH;
                MmForm_RitoPlay(play, player, sRito.launch, 0, 1.0f);
                Player_PlaySfx(&player->actor, NA_SE_PL_ROLL);
            }
            return 1;
        }

        case RITO_FLY_LAUNCH:
            sRito.timer++;
            MmForm_RitoWind(play, player, 1.0f);
            player->actor.velocity.y -= 0.8f; // the push runs out
            // Once the climb tops out, A decides: glide on, or fall.
            if ((player->actor.velocity.y <= 1.0f) || (sRito.timer > 20)) {
                if (aHeld) {
                    sRito.state = RITO_FLY_GLIDE;
                    MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                } else {
                    sRito.state = RITO_FLY_FALL;
                    MmForm_RitoRelease(player, 0);
                    return 0;
                }
            }
            return 1;

        case RITO_FLY_GLIDE:
            if (MMFORM_ON_GROUND(player)) {
                // Hand back to vanilla: OOT runs its own landing, and the clip it
                // plays IS the rito's because of the group swap above.
                MmForm_RitoRelease(player, 0);
                Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
                return 0;
            }
            // Let go of A and the wings stop — that is the whole fall condition.
            if (!aHeld) {
                MmForm_RitoRelease(player, 0);
                return 0;
            }
            MmForm_RitoGlideMove(play, player);
            if (!MmForm_RitoBowHoldsAnim()) {
                MmForm_RitoAdvance(play, player);
            }
            if ((++sRito.timer % 8) == 0) {
                Player_PlaySfx(&player->actor, NA_SE_EN_KAICHO_FLUTTER);
            }
            return 1;

        case RITO_FLY_LAND:
            if (MmForm_RitoAdvance(play, player) || (++sRito.timer > 16)) {
                sRito.state = RITO_FLY_OFF;
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                return 0;
            }
            return 1;

        case RITO_FLY_HOP:
            // A during the hop turns it into real flight, so a hop can be extended.
            if (aHeld && (sRito.timer > 2) && (sRito.fly != NULL)) {
                MmForm_RitoEnterAir(play, player);
                sRito.state = RITO_FLY_GLIDE;
                MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
                return 1;
            }
            sRito.timer++;
            if (MMFORM_ON_GROUND(player) && (sRito.timer > 4)) {
                MmForm_RitoRelease(player, 0);
                Player_PlaySfx(&player->actor, NA_SE_PL_LAND);
                return 0;
            }
            MmForm_RitoAdvance(play, player);
            return 1;

        case RITO_FLY_THROW:
            if (MmForm_RitoAdvance(play, player) || (++sRito.timer > 20)) {
                sRito.state = RITO_FLY_OFF;
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                return 0;
            }
            return 1;

        default:
            break;
    }

    // Idle: A starts the charge on the ground, or grabs flight straight away in the air.
    if (!CHECK_BTN_ALL(input->press.button, BTN_A) || (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE)) {
        return 0;
    }
    if (MmForm_RitoBowHoldsAnim()) {
        return 0; // the bow has the body; A does not start a second clip on top of it
    }
    if (MMFORM_ON_GROUND(player)) {
        if ((sRito.charge_ == NULL) || (fabsf(player->linearVelocity) > 1.0f)) {
            return 0; // moving: leave the roll/hop path alone
        }
        func_80839FFC(player, play);
        sRito.flyAction = player->actionFunc;
        sRito.state = RITO_FLY_CHARGE;
        sRito.charge = 0;
        MmForm_RitoPlay(play, player, sRito.charge_, 1, 1.0f); // already a wait loop
        return 1;
    }
    if (sRito.fly != NULL) { // gliding is free — holding A is the whole cost
        MmForm_RitoEnterAir(play, player);
        sRito.state = RITO_FLY_GLIDE;
        MmForm_RitoPlay(play, player, sRito.fly, 1, 1.0f);
        return 1;
    }
    return 0;
}

// Roll / backflip → Roc's-Feather hops. z_player.c asks before starting either.
extern "C" u8 MmForm_RitoTryHop(Player* player, PlayState* play, u8 isBackflip) {
    LinkAnimationHeader* clip;

    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    MmForm_RitoLoadClips();
    clip = isBackflip ? sRito.backflip : sRito.hop;
    if (clip == NULL) {
        return 0;
    }

    MmForm_RitoEnterAir(play, player);
    player->actor.velocity.y = RITO_ROCS_VELOCITY * (isBackflip ? RITO_BACKFLIP_SCALE : RITO_HOP_SCALE);
    player->actor.gravity = RITO_FLY_GRAVITY_DEFAULT;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING; // ledges stay grabbable, as Roc's does
    sRito.state = RITO_FLY_HOP;
    sRito.timer = 0;
    MmForm_RitoPlay(play, player, clip, 0, 1.0f);
    Player_PlaySfx(&player->actor, NA_SE_PL_SKIP);
    return 1;
}

// Items the rito may throw while airborne: bombs, Deku nuts, the SW97 elemental
// seeds (slingshot ammo) and the boomerang. OOT still spawns and throws them —
// this only puts the clip on the body while that happens.
extern "C" u8 MmForm_RitoTryAirThrow(Player* player, PlayState* play, s32 itemAction) {
    if (gFormState.currentForm != MM_PLAYER_FORM_RITO) {
        return 0;
    }
    if (MMFORM_ON_GROUND(player) || (sRito.throwItem == NULL)) {
        return 0;
    }
    switch (itemAction) {
        case PLAYER_IA_BOMB:
        case PLAYER_IA_BOMBCHU:
        case PLAYER_IA_DEKU_NUT:
        case PLAYER_IA_SLINGSHOT:
        case PLAYER_IA_BOOMERANG:
            break;
        default:
            return 0;
    }
    sRito.state = RITO_FLY_THROW;
    sRito.timer = 0;
    MmForm_RitoPlay(play, player, sRito.throwItem, 0, RITO_THROW_SPEED);
    return 1;
}
