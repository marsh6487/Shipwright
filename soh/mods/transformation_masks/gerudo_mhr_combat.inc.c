/**
 * gerudo_mhr_combat.inc.c — Gerudo "Monster Hunter Rise / Dual Blades" combat.
 *
 * Text-included at the END of mm_player_form.cpp (after MmForm_SetAction,
 * MmForm_GerudoPushSlashAnim, gFormState, sGerudoComboLockedYaw, the
 * stick/Z-target helpers, GERUDO_*_DAMAGE, etc. are all defined). Self-contained:
 * owns ALL Gerudo combat through one entry, MmForm_GerudoMhrUpdate(), called at
 * the top of the MmForm_UpdateActive dispatch. Returns 1 when driving a move
 * this frame (skip the goron dispatch); 0 when idle so idle/walk/run run.
 *
 * Anims: mhr_db dual-blade clips in gerudo.o2r at
 * __OTR__misc/link_animetion/gPlayerAnim_mhr_db_<name>. Damage = Kokiri tier via
 * the existing gFormState.gerudoQuadsActive/Damage quads built in PostLimbDraw.
 *
 * v1: 2x demon run-mode locomotion and the peahat-anchor wirebug VISUAL are
 * deferred to polish (marked TODO). The mechanics (combo/charge/wirebug launch/
 * air combos/demon buff/homing) are live. Skijer's NEI.
 */

// ---- tunables -------------------------------------------------------------
#define GMHR_WIREBUG_MAX        3
#define GMHR_WIREBUG_RECHARGE   300    // ~5s @60fps per wirebug
#define GMHR_DEMON_MAX          600.0f // ~10s of demon at full drain
#define GMHR_DEMON_FILL_HIT     60.0f  // meter gained per landed hit
#define GMHR_DEMON_DRAIN        1.0f   // per frame while active
#define GMHR_DEMON_RUN_DRAIN    0.25f  // run-mode drains at 25%
#define GMHR_CHARGE_LOOP_FRAMES 30     // frames per charge "loop"
#define GMHR_CHARGE_LOOP_MAX    3
#define GMHR_PUTAWAY_IDLE       120    // ~2s idle -> sheathe latch (OOT-style)
#define GMHR_LAUNCH_UP          22.0f
#define GMHR_LAUNCH_FWD         8.0f
#define GMHR_LAUNCH_GRAVITY     (-0.8f) // gentler than a normal jump (-1.2) → longer, higher parabola
#define GMHR_WALL_MARGIN        14.0f   // stop this far before a wall (body half-width-ish)
#define GMHR_WALL_RECOIL        7.0f    // backward bounce distance on wall hit
#define GMHR_PLAY_SPEED         1.3f    // global speed for the mhr_db clips (snappier, not too fast)

// ---- anim path helper -----------------------------------------------------
// mhr_db clips are SOH_PlayerAnimation resources = a RAW s16 payload, NOT a
// LinkAnimationHeader. A naive (LinkAnimationHeader*)ResourceMgr_LoadAnimByName
// cast reads frame-0 data as frameCount/segment → garbage → OOB memcpy crash in
// AnimationContext_SetLoadFrame. ResourceMgr_LoadPlayerAnimAsHeader wraps the raw
// payload in a real header (frameCount = totalS16/67, segment = data) and caches
// it persistently — the same path the Animation Viewer uses.
static LinkAnimationHeader* MmForm_MhrLoadPath(const char* path) {
    if (path == NULL || !ResourceMgr_FileExists(path)) return NULL;
    return ResourceMgr_LoadPlayerAnimAsHeader(path);
}
#define MHR(name) MmForm_MhrLoadPath("__OTR__misc/link_animetion/gPlayerAnim_mhr_db_" name)

// ---- state ----------------------------------------------------------------
typedef enum {
    GMHR_IDLE = 0,
    GMHR_COMBO,        // ground B combo: attack02 -> charge_attack03 -> jump_attack07
    GMHR_WALK_STAB,    // B + stick-fwd: charge_attack09
    GMHR_DASH_A,       // A: roll/dodge -> dash_attack03
    GMHR_CHARGE,       // B-hold charging
    GMHR_CHARGE_REL,   // release: dash_attack20 / side_attack09(demon) / dash_attack16
    GMHR_CHARGE_MAX,   // max-charge: jump_attack18 -> wirebug_attack09
    GMHR_WIREBUG_UP,   // R+B: throw (jump_attack14) -> launch
    GMHR_WIREBUG_FWD,  // R+A: back_attack05 / Z-target wirebug_dash*
    GMHR_AIR,          // airborne after wirebug-up (free float)
    GMHR_AIR_CHAIN,    // air follow-up anims
    GMHR_AIR_HOMING,   // demon-B aerial homing
    GMHR_LAND,         // landing recovery (attack04 / motion04)
    GMHR_DEMON_ENTER,  // attack05
    GMHR_DEMON_FINISH, // attack08
    GMHR_RECOVER,      // vanilla Link sword-swing recovery after an mhr swing
} GerudoMhrState;

static struct {
    u8  inited;
    GerudoMhrState state;
    s16 timer;
    u8  comboStep;      // 0..2
    u8  comboBuf;       // re-press buffered to chain
    u8  chargeLoops;
    u8  airStep;        // air-combo sub-step (0..3 = A path, 10..12 = B path)
    u8  airDemon;       // air combo started while demon active
    u8  landMotion04;   // landing should use motion04 (post-homing)
    u8  hitCredited;    // demon meter credited for current swing
    u8  wirebug;
    s16 wirebugTimer;
    u8  demonActive;
    u8  demonRun;       // entered demon while not in fighter
    f32 demonMeter;
    s16 idleTimer;
    u8  swordsOut;      // fighter latch (recently in combat)
    Actor* homeTarget;
    // Root motion: mhr_db clips translate jointTable[0] (the lunge). We apply that
    // delta to the actor so camera+collider follow, and reset the visual root to
    // base so it isn't double-moved. Re-captured per clip (no snap between steps).
    u8  rootInit;
    s16 baseRootX, baseRootY, baseRootZ;
    s16 prevRootX, prevRootZ;
    u8  wallBounce;     // root motion hit a wall this move → recoil + end the combo
    u8  airFalling;     // R+B air arc started descending → switch to the fall clip
} sMhr;

// Defined at the bottom; used by the air-chain state.
static void MmForm_MhrEnterIdleAir(Player* player);

// ---- small helpers --------------------------------------------------------
static void MmForm_MhrStart(PlayState* play, Player* player, LinkAnimationHeader* anim, f32 spd, u8 mode) {
    if (anim == NULL) return;
    // CRITICAL: pause OOT's actionFunc the instant we put a custom anim on
    // player->skelAnime. The gerudo body draws from player->skelAnime, so we push
    // the mhr_db anim there — but if PAUSE isn't set, OOT's Player_Action_Idle
    // advances that custom anim next frame and crashes in
    // AnimationContext_SetLoadFrame. Set it here so EVERY move start is covered
    // (trigger detection doesn't go through a state case that plants first).
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    // Global 2x so the mhr_db clips read snappy (user request).
    f32 aspd = spd * GMHR_PLAY_SPEED;
    // Drive both the form skeleton (MmForm draw) and Link's skel (the bone
    // matrix PostLimbDraw uses for sword trails + hit quads).
    MmForm_SetAction(GORON_ACT_PUNCH_A, play, anim, aspd, mode);
    MmForm_GerudoPushSlashAnim(play, player, anim, aspd);
    // NOTE: sword trails are NOT spawned here — only the real SWING attacks call
    // MmForm_GerudoSpawnSlashTrails explicitly. Spawning on every move (held charge
    // stance, dashes, air poses, recovery) made the trail look wrong on non-swings.
    sMhr.timer = 0;
    sMhr.hitCredited = 0;
    sMhr.rootInit = 0;   // re-capture root base for THIS clip (no snap from prev clip)
    sMhr.wallBounce = 0; // fresh clip can recoil again
}

// Apply the clip's root translation. mhr_db clips move jointTable[0] (Link lunges
// forward with the swing); if we don't apply it the model slides off the actor and
// the camera/collider don't follow, and each clip snaps the model back to the actor
// origin (the "weird" combo seam). We replicate SkelAnime_UpdateTranslation: rotate
// the per-frame root delta by the facing yaw, push it onto actor.world.pos, then
// reset the visual root to base so the body draws at the actor (no double-move).
// applyToActor=0 for air states (the actor already moves via MmForm_MhrAirStep) —
// there we only reset the visual root.
static void MmForm_MhrRootMotion(PlayState* play, Player* player, u8 applyToActor) {
    Vec3s* root = &player->skelAnime.jointTable[0];
    if (!sMhr.rootInit) {
        sMhr.rootInit = 1;
        sMhr.baseRootX = root->x; sMhr.baseRootY = root->y; sMhr.baseRootZ = root->z;
        sMhr.prevRootX = root->x; sMhr.prevRootZ = root->z;
    } else if (applyToActor) {
        f32 ldx = (f32)(root->x - sMhr.prevRootX);
        f32 ldz = (f32)(root->z - sMhr.prevRootZ);
        sMhr.prevRootX = root->x;
        sMhr.prevRootZ = root->z;
        f32 sn = Math_SinS(player->actor.shape.rot.y);
        f32 cs = Math_CosS(player->actor.shape.rot.y);
        f32 wdx = (ldx * cs + ldz * sn) * player->actor.scale.x;
        f32 wdz = (ldz * cs - ldx * sn) * player->actor.scale.x;
        f32 len = sqrtf(wdx * wdx + wdz * wdz);
        // Wall check: trace from the chest along the intended move. If blocked,
        // DON'T move into it (no clipping through walls) and bounce back — the
        // recoil the user asked for, parallel to the Goron/Zora punch wall-hit.
        u8 blocked = 0;
        if (len > 0.01f) {
            Vec3f from = player->actor.world.pos; from.y += 20.0f;
            Vec3f to;
            to.x = from.x + wdx / len * (len + GMHR_WALL_MARGIN);
            to.y = from.y;
            to.z = from.z + wdz / len * (len + GMHR_WALL_MARGIN);
            Vec3f hitPos; CollisionPoly* poly = NULL; s32 bgId;
            if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId)) {
                blocked = 1;
            }
        }
        if (blocked) {
            if (!sMhr.wallBounce) {
                sMhr.wallBounce = 1;
                CollisionCheck_SpawnShieldParticles(play, &player->actor.world.pos);
                Player_PlaySfx(&player->actor, NA_SE_IT_WALL_HIT_HARD);
                Player_RequestRumble(player, 180, 20, 100, 0);
                // Bounce straight back from the facing (direct world.pos — speedXZ
                // doesn't integrate under PAUSE).
                player->actor.world.pos.x -= sn * GMHR_WALL_RECOIL;
                player->actor.world.pos.z -= cs * GMHR_WALL_RECOIL;
            }
        } else {
            player->actor.world.pos.x += wdx;
            player->actor.world.pos.z += wdz;
        }
        // Follow the floor as we lunge. PAUSE skips OOT's ground-snap, so without
        // this the actor keeps its start Y → floats over dips and clips into steps
        // ("saliendo del piso" / "atravesando el piso"). floorHeight is refreshed
        // every frame by Actor_UpdateBgCheckInfo (runs unconditionally).
        if (player->actor.floorHeight > -30000.0f) {
            player->actor.world.pos.y = player->actor.floorHeight;
        }
    }
    // Keep the visual root planted at the actor horizontally (avoid double-move).
    // Do NOT touch .y — forcing it floated/sank the body off the floor; the anim's
    // own vertical root is correct (the lunge is purely X/Z, handled on the actor).
    root->x = sMhr.baseRootX;
    root->z = sMhr.baseRootZ;
    // CRITICAL: the gerudo body is drawn from gFormState.formSkelAnime (for our
    // GORON_ACT_PUNCH_A action willCopyOoT is false, so player->skelAnime is NOT
    // copied into it — see mm_player_form.cpp:15846/15906). If we don't reset the
    // FORM root too, the visible body keeps lunging via the anim's root translation
    // while the collider/position move in code → they desync, the body clips
    // through walls, and it snaps back to the actor when the clip ends. Planting
    // the form root makes the code-driven actor movement BE the visual.
    if (gFormState.formSkelAnime.jointTable != NULL) {
        gFormState.formSkelAnime.jointTable[0].x = sMhr.baseRootX;
        gFormState.formSkelAnime.jointTable[0].z = sMhr.baseRootZ;
    }
}

// Advance both tracks; return 1 when Link's track finishes (ONCE). Also applies
// root motion (actor follows the lunge on ground; visual-only in air).
static s32 MmForm_MhrAdvance(PlayState* play, Player* player) {
    LinkAnimation_Update(play, &gFormState.formSkelAnime);
    s32 done = LinkAnimation_Update(play, &player->skelAnime);
    u8 air = (sMhr.state == GMHR_AIR || sMhr.state == GMHR_AIR_CHAIN || sMhr.state == GMHR_AIR_HOMING);
    MmForm_MhrRootMotion(play, player, !air);
    return done;
}

// Manual airborne physics. PLAYER_STATE3_PAUSE_ACTION_FUNC skips OOT's action
// func, which is where the player's gravity + position integration normally
// runs (z_player.c) — so an air state that keeps PAUSE set must integrate the
// arc itself. Actor_UpdateBgCheckInfo still runs unconditionally each frame, so
// MMFORM_ON_GROUND / floorHeight stay valid for landing detection. `gravity`=0
// for homing (velocity is already aimed at the target).
static void MmForm_MhrAirStep(PlayState* play, Player* player, u8 gravity) {
    if (gravity) {
        player->actor.velocity.y += player->actor.gravity; // gravity is negative
        if (player->actor.velocity.y < -20.0f) player->actor.velocity.y = -20.0f;
    }
    // Wall check on the horizontal move so the wirebug arc / homing can't fly
    // through walls (same "don't translate into geometry" rule as ground root
    // motion). On a hit we kill horizontal velocity — Link keeps falling but stops
    // at the wall instead of clipping through.
    f32 hlen = sqrtf(player->actor.velocity.x * player->actor.velocity.x +
                     player->actor.velocity.z * player->actor.velocity.z);
    if (hlen > 0.01f) {
        Vec3f from = player->actor.world.pos; from.y += 20.0f;
        Vec3f to;
        to.x = from.x + player->actor.velocity.x / hlen * (hlen + GMHR_WALL_MARGIN);
        to.y = from.y;
        to.z = from.z + player->actor.velocity.z / hlen * (hlen + GMHR_WALL_MARGIN);
        Vec3f hitPos; CollisionPoly* poly = NULL; s32 bgId;
        if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId)) {
            player->actor.velocity.x = 0.0f;
            player->actor.velocity.z = 0.0f;
        }
    }
    player->actor.world.pos.x += player->actor.velocity.x;
    player->actor.world.pos.y += player->actor.velocity.y;
    player->actor.world.pos.z += player->actor.velocity.z;
}

static f32 MmForm_MhrCurFrame(void) {
    return gFormState.formSkelAnime.curFrame;
}

static void MmForm_MhrQuadOn(s16 damage) {
    gFormState.gerudoQuadsActive = 1;
    gFormState.gerudoQuadDamage = (u8)damage;
}
static void MmForm_MhrQuadOff(Player* player) {
    gFormState.gerudoQuadsActive = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
}

static void MmForm_MhrCreditHit(Player* player) {
    if (sMhr.hitCredited) return;
    if ((player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
        (player->meleeWeaponQuads[1].base.atFlags & AT_HIT)) {
        sMhr.hitCredited = 1;
        if (sMhr.demonMeter < GMHR_DEMON_MAX) {
            sMhr.demonMeter += GMHR_DEMON_FILL_HIT;
            if (sMhr.demonMeter > GMHR_DEMON_MAX) sMhr.demonMeter = GMHR_DEMON_MAX;
        }
    }
}

static void MmForm_MhrPlant(Player* player) {
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->actor.shape.rot.y = sGerudoComboLockedYaw;
    player->actor.world.rot.y = sGerudoComboLockedYaw;
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
}

static void MmForm_MhrEnterIdle(Player* player) {
    sMhr.state = GMHR_IDLE;
    sMhr.timer = 0;
    sMhr.comboStep = 0;
    sMhr.comboBuf = 0;
    MmForm_MhrQuadOff(player);
    MmForm_KillTrail(gPlayState, &gFormState.punchTrailEffectIndex, &gFormState.punchTrailActive);
    MmForm_KillTrail(gPlayState, &gFormState.punchTrailEffectIndexR, &gFormState.punchTrailActiveR);
    // Hand player->skelAnime back to OOT. We must NOT force gFormState.idleAnim onto
    // it here: that anim is built for the 21-limb FORM skeleton, and applying it to
    // the 22-limb player skeleton flails the limbs ("limbs everywhere" on combo
    // exit). The leftover mhr_db clip is a valid 22-limb player anim (ANIMMODE_ONCE,
    // already at its end), so once PAUSE clears OOT's Player_Action_Idle advances it,
    // sees it finished, and morphs cleanly into its own wait anim.
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    MmForm_SetAction(GORON_ACT_IDLE, gPlayState, gFormState.idleAnim, 1.0f, ANIMMODE_LOOP);
}

// After an mhr swing, play Link's VANILLA sword-swing recovery (the same
// NORMAL_KIRU_FINSH_END the form already loads into gerudoSlashEnd[0]) before
// returning to idle, so the swing eases out naturally instead of snapping. The
// recovery clip is a real 22-limb player LinkAnimationHeader (MmAnim_Load builds
// frameCount+segment), so it's safe on player->skelAnime. NULL → just idle.
static void MmForm_MhrEnterRecover(PlayState* play, Player* player) {
    LinkAnimationHeader* rec = gFormState.gerudoSlashEnd[0];
    if (rec == NULL) { MmForm_MhrEnterIdle(player); return; }
    MmForm_MhrQuadOff(player);
    MmForm_KillTrail(play, &gFormState.punchTrailEffectIndex, &gFormState.punchTrailActive);
    MmForm_KillTrail(play, &gFormState.punchTrailEffectIndexR, &gFormState.punchTrailActiveR);
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    MmForm_SetAction(GORON_ACT_PUNCH_END, play, rec, 1.0f, ANIMMODE_ONCE);
    MmForm_GerudoPushSlashAnim(play, player, rec, 1.0f);
    sMhr.rootInit = 0;
    sMhr.wallBounce = 0;
    sMhr.timer = 0;
    sMhr.state = GMHR_RECOVER;
}

static void MmForm_MhrEndDemon(Player* player, PlayState* play) {
    if (!sMhr.demonActive) return;
    sMhr.demonActive = 0;
    sMhr.demonRun = 0;
    sGerudoComboLockedYaw = player->actor.shape.rot.y;
    MmForm_MhrStart(play, player, MHR("attack08"), 1.0f, ANIMMODE_ONCE);
    sMhr.state = GMHR_DEMON_FINISH;
}

static f32 MmForm_MhrSpeed(f32 base) {
    return sMhr.demonActive ? (base * 1.35f) : base;
}

// ---- cleanup (called on detransform / form reset) -------------------------
static void MmForm_GerudoMhrReset(void) {
    sMhr.homeTarget = NULL;
    sMhr.inited = 0;
    sMhr.state = GMHR_IDLE;
    sMhr.timer = 0;
    sMhr.comboStep = 0;
    sMhr.comboBuf = 0;
    sMhr.chargeLoops = 0;
    sMhr.airStep = 0;
    sMhr.airDemon = 0;
    sMhr.landMotion04 = 0;
    sMhr.demonActive = 0;
    sMhr.demonRun = 0;
    sMhr.demonMeter = 0.0f;
    sMhr.idleTimer = 0;
    sMhr.swordsOut = 0;
    sMhr.rootInit = 0;
    sMhr.wallBounce = 0;
    sMhr.airFalling = 0;
}

// ---- HUD getters (consumed by gerudo_hud.cpp) -----------------------------
extern "C" u8  GerudoMhr_IsActive(void)      { return gFormState.currentForm == MM_PLAYER_FORM_GERUDO; }
extern "C" u8  GerudoMhr_GetWirebugs(void)   { return sMhr.wirebug; }
extern "C" f32 GerudoMhr_GetWirebugFill(void) {
    if (sMhr.wirebug >= GMHR_WIREBUG_MAX) return 1.0f;
    return (f32)sMhr.wirebugTimer / (f32)GMHR_WIREBUG_RECHARGE;
}
extern "C" u8  GerudoMhr_IsDemonActive(void)   { return sMhr.demonActive; }
extern "C" f32 GerudoMhr_GetDemonMeter01(void) { return sMhr.demonMeter / GMHR_DEMON_MAX; }

// ===========================================================================
// Main controller
// ===========================================================================
static u8 MmForm_GerudoMhrUpdate(Player* player, PlayState* play) {
    if (player == NULL || play == NULL) return 0;

    if (!sMhr.inited) {
        sMhr.inited = 1;
        sMhr.wirebug = GMHR_WIREBUG_MAX;
        sMhr.wirebugTimer = 0;
        sMhr.state = GMHR_IDLE;
        sMhr.swordsOut = 0;
    }

    Input* in = &play->state.input[0];
    u8 bPress = CHECK_BTN_ALL(in->press.button, BTN_B) != 0;
    u8 bHeld  = CHECK_BTN_ALL(in->cur.button,  BTN_B) != 0;
    u8 aPress = CHECK_BTN_ALL(in->press.button, BTN_A) != 0;
    u8 rHeld  = CHECK_BTN_ALL(in->cur.button,  BTN_R) != 0;
    u8 lPress = CHECK_BTN_ALL(in->press.button, BTN_L) != 0;
    s32 stickDir = MmForm_GetStickDirection(player);
    u8  zt = MmForm_IsZTargeting(player);
    u8  onGround = MMFORM_ON_GROUND(player) != 0;

    // --- wirebug recharge (always) ---
    if (sMhr.wirebug < GMHR_WIREBUG_MAX) {
        if (++sMhr.wirebugTimer >= GMHR_WIREBUG_RECHARGE) {
            sMhr.wirebugTimer = 0;
            sMhr.wirebug++;
        }
    } else {
        sMhr.wirebugTimer = 0;
    }

    // --- demon meter drain + glow (always) ---
    if (sMhr.demonActive) {
        sMhr.demonMeter -= GMHR_DEMON_DRAIN * (sMhr.demonRun ? GMHR_DEMON_RUN_DRAIN : 1.0f);
        Actor_SetColorFilter(&player->actor, 0x4000, 200, 0, 4); // red glow (whole-body v1)
        if (sMhr.demonMeter <= 0.0f) {
            sMhr.demonMeter = 0.0f;
            MmForm_MhrEndDemon(player, play);
        }
    }

    sMhr.timer++;

    // ============================ ACTIVE STATES ============================
    switch (sMhr.state) {
        case GMHR_IDLE:
            break; // fall through to trigger detection

        case GMHR_COMBO: {
            MmForm_MhrPlant(player);
            f32 cf = MmForm_MhrCurFrame();
            s16 dmg = (sMhr.comboStep >= 2) ? GERUDO_FINISHER_DAMAGE : GERUDO_SLASH_DAMAGE;
            // explicit hit windows tuned to the mhr clips
            // (attack02 / charge_attack08 / jump_attack07)
            f32 hb = (sMhr.comboStep == 0) ? 8.0f : (sMhr.comboStep == 1) ? 10.0f : 12.0f;
            f32 he = (sMhr.comboStep == 0) ? 22.0f : (sMhr.comboStep == 1) ? 34.0f : 44.0f;
            if (cf >= hb && cf <= he) MmForm_MhrQuadOn(dmg);
            else MmForm_MhrQuadOff(player);
            MmForm_MhrCreditHit(player);
            if (bPress) sMhr.comboBuf = 1;
            s32 done = MmForm_MhrAdvance(play, player);
            if (sMhr.wallBounce) { // hit a wall → recoil applied; stop the combo into the recovery
                MmForm_MhrEnterRecover(play, player);
                return 1;
            }
            // Fluid mashing (Zora/Goron feel): once the hit window has passed, a
            // buffered B CANCELS the recovery straight into the next slash instead
            // of waiting for the whole clip to finish. Falls back to chaining at
            // anim-end for late presses.
            u8 canCancel = (MmForm_MhrCurFrame() > he) && sMhr.comboBuf;
            if (done || canCancel) {
                if (sMhr.comboStep == 0 && sMhr.comboBuf) {
                    sMhr.comboStep = 1; sMhr.comboBuf = 0;
                    MmForm_MhrStart(play, player, MHR("charge_attack08"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
                    MmForm_GerudoSpawnSlashTrails(play);
                    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
                } else if (sMhr.comboStep == 1 && sMhr.comboBuf) {
                    sMhr.comboStep = 2; sMhr.comboBuf = 0;
                    MmForm_MhrStart(play, player, MHR("jump_attack07"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
                    MmForm_GerudoSpawnSlashTrails(play);
                    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_L); // finisher → strong voice
                } else if (done) {
                    MmForm_MhrQuadOff(player);
                    MmForm_MhrEnterRecover(play, player); // vanilla swing recovery → idle
                }
            }
            return 1;
        }

        case GMHR_WALK_STAB: {
            MmForm_MhrPlant(player);
            f32 cf = MmForm_MhrCurFrame();
            if (cf >= 6.0f && cf <= 26.0f) MmForm_MhrQuadOn(GERUDO_SLASH_DAMAGE);
            else MmForm_MhrQuadOff(player);
            MmForm_MhrCreditHit(player);
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterRecover(play, player);
            return 1;
        }

        case GMHR_DASH_A: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->linearVelocity = (sMhr.timer < 10) ? 9.0f : 0.0f;
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterIdle(player);
            return 1;
        }

        case GMHR_WIREBUG_FWD: {
            // Forward wirebug (back_attack05) or Z-target directional dash:
            // brief lunge with a live blade window, then back to idle.
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->linearVelocity = (sMhr.timer < 12) ? 14.0f : 0.0f;
            f32 cf = MmForm_MhrCurFrame();
            if (cf >= 4.0f && cf <= 18.0f) MmForm_MhrQuadOn(GERUDO_SLASH_DAMAGE);
            else MmForm_MhrQuadOff(player);
            MmForm_MhrCreditHit(player);
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterIdle(player);
            return 1;
        }

        case GMHR_CHARGE: {
            MmForm_MhrPlant(player);
            MmForm_MhrAdvance(play, player); // pose holds (ONCE) — fine for a charge stance
            if (sMhr.chargeLoops < GMHR_CHARGE_LOOP_MAX &&
                sMhr.timer > 0 && (sMhr.timer % GMHR_CHARGE_LOOP_FRAMES) == 0) {
                sMhr.chargeLoops++;
            }
            if (!bHeld) { // release
                sGerudoComboLockedYaw = player->actor.shape.rot.y;
                if (sMhr.chargeLoops >= GMHR_CHARGE_LOOP_MAX) {
                    MmForm_MhrStart(play, player, MHR("jump_attack18"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
                    sMhr.state = GMHR_CHARGE_MAX;
                } else if (sMhr.demonActive) {
                    MmForm_MhrStart(play, player, MHR("side_attack09"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
                    sMhr.state = GMHR_CHARGE_REL;
                } else {
                    MmForm_MhrStart(play, player, MHR("dash_attack20"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
                    sMhr.state = GMHR_CHARGE_REL;
                }
                sMhr.chargeLoops = 0;
            }
            return 1;
        }

        case GMHR_CHARGE_REL: {
            MmForm_MhrPlant(player);
            f32 cf = MmForm_MhrCurFrame();
            if (cf >= 2.0f && cf <= 18.0f) MmForm_MhrQuadOn(GERUDO_FINISHER_DAMAGE);
            else MmForm_MhrQuadOff(player);
            MmForm_MhrCreditHit(player);
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterRecover(play, player);
            return 1;
        }

        case GMHR_CHARGE_MAX: {
            MmForm_MhrPlant(player);
            MmForm_MhrQuadOn(GERUDO_FINISHER_DAMAGE);
            MmForm_MhrCreditHit(player);
            if (MmForm_MhrAdvance(play, player)) {
                MmForm_MhrStart(play, player, MHR("wirebug_attack09"), 1.0f, ANIMMODE_ONCE);
                sMhr.state = GMHR_CHARGE_REL;
            }
            return 1;
        }

        case GMHR_WIREBUG_UP: {
            // Throw phase (jump_attack14): planted on the ground until the anim
            // completes, then launch into a self-integrated parabola.
            MmForm_MhrPlant(player);
            if (MmForm_MhrAdvance(play, player)) {
                f32 s = Math_SinS(player->actor.shape.rot.y);
                f32 c = Math_CosS(player->actor.shape.rot.y);
                player->actor.velocity.y = GMHR_LAUNCH_UP;
                player->actor.velocity.x = s * GMHR_LAUNCH_FWD;
                player->actor.velocity.z = c * GMHR_LAUNCH_FWD;
                player->actor.world.rot.y = player->actor.shape.rot.y;
                player->actor.gravity = GMHR_LAUNCH_GRAVITY; // gentler → longer parabola
                player->actor.bgCheckFlags &= ~0x1;
                player->stateFlags1 |= PLAYER_STATE1_JUMPING;
                player->fallStartHeight = player->actor.world.pos.y;
                Camera_ChangeMode(GET_ACTIVE_CAM(play), CAM_MODE_JUMP);
                Player_PlaySfx(&player->actor, NA_SE_PL_JUMP);
                sMhr.state = GMHR_AIR;
                sMhr.airStep = 0;
                sMhr.landMotion04 = 0;
                sMhr.airFalling = 0;
                // Rising pose (grab/launch). When the arc starts descending, GMHR_AIR
                // swaps to the fall clip (charge_attack03).
                MmForm_MhrStart(play, player, MHR("attack10"), 1.0f, ANIMMODE_ONCE);
            }
            return 1;
        }

        case GMHR_AIR: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            MmForm_MhrAirStep(play, player, 1); // self-integrated arc (PAUSE skips OOT physics)
            MmForm_MhrAdvance(play, player);
            // Once the parabola starts descending, swap rising pose → fall clip.
            if (!sMhr.airFalling && player->actor.velocity.y < 0.0f) {
                sMhr.airFalling = 1;
                MmForm_MhrStart(play, player, MHR("charge_attack03"), 1.0f, ANIMMODE_ONCE);
            }
            if (onGround) {
                sGerudoComboLockedYaw = player->actor.shape.rot.y;
                player->actor.velocity.y = 0.0f;
                // MHR() needs a string literal (compile-time concat) — pick the
                // anim first, then load by full path.
                MmForm_MhrStart(play, player,
                                sMhr.landMotion04 ? MHR("motion04") : MHR("attack04"),
                                1.0f, ANIMMODE_ONCE);
                sMhr.state = GMHR_LAND;
                return 1;
            }
            if (aPress) {
                sMhr.airDemon = sMhr.demonActive;
                MmForm_MhrStart(play, player, MHR("jump_attack19"), 1.0f, ANIMMODE_ONCE);
                sMhr.airStep = 0;
                sMhr.state = GMHR_AIR_CHAIN;
            } else if (bPress) {
                if (sMhr.demonActive && player->focusActor != NULL) {
                    sMhr.homeTarget = player->focusActor;
                    MmForm_MhrStart(play, player, MHR("wirebug_attack09"), 1.0f, ANIMMODE_LOOP);
                    sMhr.state = GMHR_AIR_HOMING;
                } else {
                    sMhr.airDemon = sMhr.demonActive;
                    MmForm_MhrStart(play, player, MHR("jump_attack18"), 1.0f, ANIMMODE_ONCE);
                    sMhr.airStep = 10;
                    sMhr.state = GMHR_AIR_CHAIN;
                }
            }
            return 1;
        }

        case GMHR_AIR_CHAIN: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            MmForm_MhrAirStep(play, player, 1);
            f32 cf = MmForm_MhrCurFrame();
            if (cf >= 2.0f && cf <= 14.0f) MmForm_MhrQuadOn(GERUDO_SLASH_DAMAGE);
            else MmForm_MhrQuadOff(player);
            MmForm_MhrCreditHit(player);
            s32 done = MmForm_MhrAdvance(play, player);
            if (onGround) {
                sGerudoComboLockedYaw = player->actor.shape.rot.y;
                MmForm_MhrStart(play, player, MHR("attack04"), 1.0f, ANIMMODE_ONCE);
                sMhr.state = GMHR_LAND;
                return 1;
            }
            if (done) {
                if (sMhr.airStep == 0) {
                    if (sMhr.airDemon) { MmForm_MhrStart(play, player, MHR("dash_attack22"), 1.0f, ANIMMODE_ONCE); sMhr.airStep = 1; }
                    else MmForm_MhrEnterIdleAir(player);
                } else if (sMhr.airStep == 1) {
                    MmForm_MhrStart(play, player, MHR("action02_loop"), 1.0f, ANIMMODE_ONCE); sMhr.airStep = 2;
                } else if (sMhr.airStep == 2) {
                    MmForm_MhrStart(play, player, MHR("attack15"), 1.0f, ANIMMODE_ONCE); sMhr.airStep = 3;
                } else if (sMhr.airStep == 10) {
                    MmForm_MhrStart(play, player, MHR("action01_loop"), 1.0f, ANIMMODE_ONCE); sMhr.airStep = 11;
                } else if (sMhr.airStep == 11) {
                    MmForm_MhrStart(play, player, MHR("attack03"), 1.0f, ANIMMODE_ONCE); sMhr.airStep = 12;
                } else {
                    MmForm_MhrEnterIdleAir(player);
                }
            }
            return 1;
        }

        case GMHR_AIR_HOMING: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            MmForm_MhrAdvance(play, player);
            MmForm_MhrQuadOn(GERUDO_SLASH_DAMAGE);
            Actor* t = sMhr.homeTarget;
            u8 hit = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                     (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);
            MmForm_MhrCreditHit(player);
            if (t == NULL || t->update == NULL || hit || !sMhr.demonActive || onGround) {
                sMhr.homeTarget = NULL;
                MmForm_MhrQuadOff(player);
                sMhr.landMotion04 = 1;
                if (onGround) {
                    sGerudoComboLockedYaw = player->actor.shape.rot.y;
                    MmForm_MhrStart(play, player, MHR("motion04"), 1.0f, ANIMMODE_ONCE);
                    sMhr.state = GMHR_LAND;
                } else {
                    sMhr.state = GMHR_AIR;
                }
                return 1;
            }
            // home toward target
            f32 dx = t->world.pos.x - player->actor.world.pos.x;
            f32 dy = (t->world.pos.y + 20.0f) - player->actor.world.pos.y;
            f32 dz = t->world.pos.z - player->actor.world.pos.z;
            f32 d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (d > 1.0f) {
                f32 sp = 24.0f;
                player->actor.velocity.x = dx / d * sp;
                player->actor.velocity.y = dy / d * sp;
                player->actor.velocity.z = dz / d * sp;
                player->actor.world.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &t->world.pos);
                player->actor.shape.rot.y = player->actor.world.rot.y;
            }
            player->actor.bgCheckFlags &= ~0x1;
            MmForm_MhrAirStep(play, player, 0); // integrate the aimed velocity (no gravity)
            return 1;
        }

        case GMHR_LAND: {
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->linearVelocity = 0.0f;
            player->actor.velocity.y = 0.0f;
            // Snap onto the floor — a fast fall can overshoot below it before
            // onGround triggers (the "atravesando el piso" on landing).
            if (player->actor.floorHeight > -30000.0f) {
                player->actor.world.pos.y = player->actor.floorHeight;
            }
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterIdle(player);
            return 1;
        }

        case GMHR_DEMON_ENTER:
        case GMHR_DEMON_FINISH: {
            MmForm_MhrPlant(player);
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterIdle(player);
            return 1;
        }

        case GMHR_RECOVER: {
            // Vanilla swing recovery (no quads). Plays out, then hands to OOT idle.
            MmForm_MhrPlant(player);
            if (MmForm_MhrAdvance(play, player)) MmForm_MhrEnterIdle(player);
            return 1;
        }
    }

    // ============================ TRIGGER DETECTION (idle) =================
    // L: toggle demon mode.
    if (lPress) {
        if (!sMhr.demonActive) {
            sMhr.demonActive = 1;
            if (sMhr.demonMeter < GMHR_DEMON_FILL_HIT) sMhr.demonMeter = GMHR_DEMON_FILL_HIT;
            if (sMhr.swordsOut && onGround) {
                sGerudoComboLockedYaw = player->actor.shape.rot.y;
                sMhr.demonRun = 0;
                MmForm_MhrStart(play, player, MHR("attack05"), 1.0f, ANIMMODE_ONCE);
                sMhr.state = GMHR_DEMON_ENTER;
                return 1;
            }
            // Not in fighter -> demon buff (run-mode 2x locomotion: TODO polish).
            sMhr.demonRun = 1;
            return 0;
        } else {
            MmForm_MhrEndDemon(player, play);
            return 1;
        }
    }

    // R modifier: wirebug moves (consume 1 wirebug).
    if (rHeld && sMhr.wirebug > 0) {
        if (bPress) { // R+B: wirebug UP
            sMhr.wirebug--;
            sGerudoComboLockedYaw = player->actor.shape.rot.y;
            MmForm_MhrStart(play, player, MHR("jump_attack14"), 1.0f, ANIMMODE_ONCE);
            sMhr.state = GMHR_WIREBUG_UP;
            sMhr.swordsOut = 1;
            return 1;
        }
        if (aPress) { // R+A: forward wirebug, or Z-target directional dash
            sMhr.wirebug--;
            sGerudoComboLockedYaw = player->actor.shape.rot.y;
            const char* path;
            if (zt) {
                if (stickDir == 1)      path = "__OTR__misc/link_animetion/gPlayerAnim_mhr_db_wirebug_dash02"; // left
                else if (stickDir == 3) path = "__OTR__misc/link_animetion/gPlayerAnim_mhr_db_wirebug_dash03"; // right
                else if (stickDir == 2) path = "__OTR__misc/link_animetion/gPlayerAnim_mhr_db_wirebug_dash08"; // back
                else                    path = "__OTR__misc/link_animetion/gPlayerAnim_mhr_db_wirebug_dash16"; // static
            } else {
                path = "__OTR__misc/link_animetion/gPlayerAnim_mhr_db_back_attack05";
            }
            MmForm_MhrStart(play, player, MmForm_MhrLoadPath(path), 1.0f, ANIMMODE_ONCE);
            sMhr.state = GMHR_WIREBUG_FWD;
            return 1;
        }
        // R held alone: aiming reticle (TODO). Don't own the frame.
    }

    // A (no R), ground: roll/dodge -> dash_attack03.
    if (aPress && onGround && !rHeld) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        MmForm_MhrStart(play, player, MHR("dash_attack03"), 1.2f, ANIMMODE_ONCE);
        sMhr.state = GMHR_DASH_A;
        return 1;
    }

    // B (no R), ground: combo / walking-stab / demon-run-attack.
    if (bPress && !rHeld && onGround && MmForm_GerudoCanStartGroundCombo(player)) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        sMhr.swordsOut = 1;
        sMhr.idleTimer = 0;
        if (sMhr.demonActive && stickDir == 0) { // demon + running fwd -> dash_attack16
            MmForm_MhrStart(play, player, MHR("dash_attack16"), 1.0f, ANIMMODE_ONCE);
            sMhr.state = GMHR_CHARGE_REL;
        } else if (stickDir == 0) { // walking stab
            MmForm_MhrStart(play, player, MHR("charge_attack09"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
            MmForm_GerudoSpawnSlashTrails(play);
            Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
            sMhr.state = GMHR_WALK_STAB;
        } else {
            MmForm_MhrStart(play, player, MHR("attack02"), MmForm_MhrSpeed(1.0f), ANIMMODE_ONCE);
            MmForm_GerudoSpawnSlashTrails(play);
            Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N); // gerudo voice (auto-routed)
            sMhr.state = GMHR_COMBO;
            sMhr.comboStep = 0;
            sMhr.comboBuf = 0;
        }
        return 1;
    }

    // B-hold from idle (no fresh press, swords idle) -> charge.
    if (bHeld && !bPress && !rHeld && onGround && sMhr.state == GMHR_IDLE &&
        MmForm_GerudoCanStartGroundCombo(player)) {
        sGerudoComboLockedYaw = player->actor.shape.rot.y;
        sMhr.chargeLoops = 0;
        sMhr.swordsOut = 1;
        MmForm_MhrStart(play, player, MHR("idle06_loop"), 1.0f, ANIMMODE_ONCE);
        sMhr.state = GMHR_CHARGE;
        return 1;
    }

    // idle housekeeping: OOT-style auto put-away latch.
    if (sMhr.swordsOut) {
        if (++sMhr.idleTimer >= GMHR_PUTAWAY_IDLE) {
            sMhr.swordsOut = 0;
            sMhr.idleTimer = 0;
        }
    }

    return 0; // idle: let normal locomotion dispatch run
}

// Drop back to free-fall air float cleanly (used by the air chain).
static void MmForm_MhrEnterIdleAir(Player* player) {
    sMhr.state = GMHR_AIR;
    sMhr.timer = 0;
    MmForm_MhrQuadOff(player);
}
