/**
 * byrna_orb.c — Implementation. See byrna_orb.h for the design rationale.
 *
 * Hijack pattern (same as trident_charge_ball.c / deku_nut_projectile.c):
 *   - Actor_Spawn(ACTOR_EN_LIGHTBOX, ...) gives a trivial real actor with the
 *     right lifetime and categorisation; we overwrite actor->update/draw before
 *     handing the pointer back, so EnLightbox's own code never runs.
 *   - The orb's state lives in sOrb here, not in the actor, because Actor_Spawn
 *     only allocates sizeof(EnLightbox). There is at most ONE orb, so a single
 *     struct replaces the trident's pool.
 *
 * The orb has NO display list. Its visual is a per-frame KiraKira sparkle tinted
 * by the extract it carries, so nothing has to be loaded into the player object
 * slot at runtime.
 *
 * ---- TIMING ---------------------------------------------------------------
 * Every frame count here is a 20 Hz logic tick: R_UPDATE_RATE = 3 (game.c:437),
 * so the game updates once per 3 VI. 600 frames = 30 seconds, NOT 10. Do not
 * "convert" these to 60 fps — the gerudo_mhr_combat.inc.c header carries a stale
 * "@60fps" comment that is wrong by exactly this factor of three.
 *
 * NOTE: text-included from extended_equipment.c. All OoT headers are already in
 * scope from the parent TU. Everything is static except the exported accessors.
 *
 * Skijer's NEI
 */

// ---------------------------------------------------------------------------
// Tunables (20 Hz logic ticks — see the timing note above)
// ---------------------------------------------------------------------------
#define BORB_MAGIC_COST 12 // matches DEMISE_MAGIC_COST / FIRE_ROD_MAGIC_SPIN_BIG
#define BORB_BUFF_TIME 600 // 30 s of buff
#define BORB_GRACE 5       // post-impact super-damage grace, see header

#define BORB_ORBIT_RADIUS 34.0f
#define BORB_ORBIT_HEIGHT 30.0f
#define BORB_ORBIT_SPEED 0x0A00 // binang/frame -> ~1.3 s per revolution at 20 Hz
#define BORB_ORBIT_BOB 6.0f     // vertical bob so the ring reads as a spiral

#define BORB_FLY_SPEED 24.0f
#define BORB_FLY_HOMING 0.30f
#define BORB_SEEK_RANGE 800.0f
#define BORB_FLY_TIMEOUT 40 // 2 s before an outbound orb gives up and returns

#define BORB_RETURN_SPEED 28.0f
#define BORB_CATCH_DIST 34.0f

#define BORB_RADIUS 20
#define BORB_HEIGHT 28

#define BORB_HEAL_AMOUNT 0x10 // 4 hearts, same figure the old Byrna recovery used

// Buff strengths. Deliberately modest: these stack up to three at a time.
#define BORB_RED_ATTACK_MUL 1.5f
#define BORB_WHITE_SPEED_MUL 1.3f
#define BORB_WHITE_VAULT_MUL 1.4f
#define BORB_ORANGE_DEFENSE_MUL 0.5f // halves incoming damage while ORANGE is up

// ---------------------------------------------------------------------------
// Extract table
//
// MHR picks the extract from the monster PART you hit; OoT enemies have no such
// parts, so the colour is keyed to WHO the enemy is. The rule the table encodes,
// so a player can predict it without memorising the list:
//
//   RED    warriors — they fight you in melee with a weapon or claws
//   WHITE  aerials  — they fly, hop or burrow fast
//   ORANGE walls    — armoured, shelled, or fixed turrets
//   GREEN  organics — plants, jellyfish, parasites and spirits
//
// Families stay together on purpose (both Dodongos, both Stingers) so the table
// is learnable rather than memorised.
// ---------------------------------------------------------------------------
typedef struct {
    s16 actorId;
    u8 extract;
} ByrnaExtractEntry;

static const ByrnaExtractEntry sBOrbExtractTable[] = {
    // --- RED: attack up -----------------------------------------------------
    { ACTOR_EN_TEST, BYRNA_EX_RED },   // Stalfos
    { ACTOR_EN_ZF, BYRNA_EX_RED },     // Lizalfos / Dinolfos
    { ACTOR_EN_MB, BYRNA_EX_RED },     // Moblins
    { ACTOR_EN_WF, BYRNA_EX_RED },     // Wolfos
    { ACTOR_EN_GELDB, BYRNA_EX_RED },  // Gerudo Fighter
    { ACTOR_EN_TORCH2, BYRNA_EX_RED }, // Dark Link
    { ACTOR_EN_SKB, BYRNA_EX_RED },    // Stalchild
    { ACTOR_EN_RD, BYRNA_EX_RED },     // Redead / Gibdo
    { ACTOR_EN_DH, BYRNA_EX_RED },     // Dead Hand
    { ACTOR_EN_DHA, BYRNA_EX_RED },    // Dead Hand's Hand
    { ACTOR_EN_FD, BYRNA_EX_RED },     // Flare Dancer
    { ACTOR_EN_FW, BYRNA_EX_RED },     // Flare Dancer Core

    // --- WHITE: speed + vault height ----------------------------------------
    { ACTOR_EN_FIREFLY, BYRNA_EX_WHITE },   // Keese
    { ACTOR_EN_CROW, BYRNA_EX_WHITE },      // Guay
    { ACTOR_EN_PEEHAT, BYRNA_EX_WHITE },    // Peahat and larva
    { ACTOR_EN_EIYER, BYRNA_EX_WHITE },     // Stinger (land)
    { ACTOR_EN_WEIYER, BYRNA_EX_WHITE },    // Stinger (water)
    { ACTOR_EN_TITE, BYRNA_EX_WHITE },      // Tektite
    { ACTOR_EN_REEBA, BYRNA_EX_WHITE },     // Leever
    { ACTOR_EN_TP, BYRNA_EX_WHITE },        // Electric Tailpasaran
    { ACTOR_EN_YUKABYUN, BYRNA_EX_WHITE },  // Flying Floor Tile
    { ACTOR_EN_TUBO_TRAP, BYRNA_EX_WHITE }, // Flying Pot
    { ACTOR_EN_ST, BYRNA_EX_WHITE },        // Skulltula
    { ACTOR_EN_SW, BYRNA_EX_WHITE },        // Skullwalltula / Gold Skulltula
    { ACTOR_EN_WALLMAS, BYRNA_EX_WHITE },   // Wallmaster
    { ACTOR_EN_FLOORMAS, BYRNA_EX_WHITE },  // Floormaster
    { ACTOR_EN_BB, BYRNA_EX_WHITE },        // Bubble (flying skull)

    // --- ORANGE: defence up -------------------------------------------------
    { ACTOR_EN_IK, BYRNA_EX_ORANGE },      // Iron Knuckle
    { ACTOR_EN_AM, BYRNA_EX_ORANGE },      // Armos Statue
    { ACTOR_EN_VM, BYRNA_EX_ORANGE },      // Beamos
    { ACTOR_EN_SB, BYRNA_EX_ORANGE },      // Shell Blade
    { ACTOR_EN_FZ, BYRNA_EX_ORANGE },      // Freezard
    { ACTOR_EN_NY, BYRNA_EX_ORANGE },      // Spike
    { ACTOR_EN_DODONGO, BYRNA_EX_ORANGE }, // Dodongo
    { ACTOR_EN_DODOJR, BYRNA_EX_ORANGE },  // Baby Dodongo
    { ACTOR_EN_RR, BYRNA_EX_ORANGE },      // Like-Like (it eats shields; defence is the joke)
    { ACTOR_EN_ANUBICE, BYRNA_EX_ORANGE }, // Anubis
    { ACTOR_EN_BW, BYRNA_EX_ORANGE },      // Torch Slug

    // --- GREEN: heal --------------------------------------------------------
    { ACTOR_EN_DEKUBABA, BYRNA_EX_GREEN },   // Deku Baba
    { ACTOR_EN_KAREBABA, BYRNA_EX_GREEN },   // Withered Deku Baba
    { ACTOR_EN_DEKUNUTS, BYRNA_EX_GREEN },   // Mad Scrub
    { ACTOR_EN_HINTNUTS, BYRNA_EX_GREEN },   // Hint Deku Scrubs
    { ACTOR_EN_SHOPNUTS, BYRNA_EX_GREEN },   // Grounded Sales Scrub
    { ACTOR_EN_BILI, BYRNA_EX_GREEN },       // Biri
    { ACTOR_EN_VALI, BYRNA_EX_GREEN },       // Bari
    { ACTOR_EN_BUBBLE, BYRNA_EX_GREEN },     // Shabom
    { ACTOR_EN_BA, BYRNA_EX_GREEN },         // Tentacle (Jabu-Jabu)
    { ACTOR_EN_BX, BYRNA_EX_GREEN },         // Electrified Tentacle
    { ACTOR_EN_BROB, BYRNA_EX_GREEN },       // Flobbery Muscle Block
    { ACTOR_EN_GOMA, BYRNA_EX_GREEN },       // Gohma Larva
    { ACTOR_EN_OKUTA, BYRNA_EX_GREEN },      // Octorok
    { ACTOR_EN_BIGOKUTA, BYRNA_EX_GREEN },   // Big Octo
    { ACTOR_EN_POH, BYRNA_EX_GREEN },        // Poe (bottled and sold as soup in vanilla)
    { ACTOR_EN_PO_SISTERS, BYRNA_EX_GREEN }, // Poe Sisters
};

// Actors that sit in ACTORCAT_ENEMY but are not enemies: other enemies'
// projectiles, invisible spawners, the player's own actors, NPCs and debug
// leftovers. Hitting one of these yields nothing at all rather than falling
// through to the default.
static const s16 sBOrbNoExtract[] = {
    ACTOR_EN_ANUBICE_FIRE, ACTOR_EN_BDFIRE,   ACTOR_EN_FD_FIRE,   ACTOR_EN_SKJNEEDLE,
    ACTOR_EN_FIRE_ROCK,    ACTOR_EN_ENCOUNT1, ACTOR_EN_ENCOUNT2,  ACTOR_EN_PO_FIELD,
    ACTOR_ARMS_HOOK,       ACTOR_EN_BOM,      ACTOR_EN_DAIKU,     ACTOR_EN_ELF,
    ACTOR_EN_ZL3,          ACTOR_EN_SKJ,      ACTOR_EN_DNT_NOMAL, ACTOR_EN_CLEAR_TAG,
};

// Anything else damageable — pots, crates, bushes — yields WHITE. White is
// useful, harmless and NOT exploitable; green here would turn smashing pottery
// into an infinite healing fountain.
#define BORB_EXTRACT_DEFAULT BYRNA_EX_WHITE

// ---------------------------------------------------------------------------
// Colours, one per extract. prim is the core, env the halo.
// ---------------------------------------------------------------------------
static const Color_RGBA8 sBOrbPrim[BYRNA_EX_MAX] = {
    { 200, 220, 255, 255 }, // NONE   pale blue (unfed orb)
    { 255, 90, 70, 255 },   // RED
    { 255, 255, 255, 255 }, // WHITE
    { 255, 170, 40, 255 },  // ORANGE
    { 110, 255, 130, 255 }, // GREEN
};

static const Color_RGBA8 sBOrbEnv[BYRNA_EX_MAX] = {
    { 60, 110, 200, 0 },  // NONE
    { 160, 20, 20, 0 },   // RED
    { 140, 160, 200, 0 }, // WHITE
    { 170, 80, 0, 0 },    // ORANGE
    { 20, 140, 40, 0 },   // GREEN
};

// ---------------------------------------------------------------------------
// State — a single orb, so no pool is needed.
// ---------------------------------------------------------------------------
typedef enum {
    BORB_STATE_ORBIT = 0,
    BORB_STATE_OUTBOUND,
    BORB_STATE_RETURN,
} BOrbState;

static struct {
    Actor* actor; // NULL = no orb exists
    Actor* target;
    u8 state;
    u8 carrying; // extract picked up, applied on RETURN
    u8 colliderInited;
    s16 orbitAngle;
    s16 flyTimer;
    Vec3f velocity;
    ColliderCylinder collider;
} sOrb = { 0 };

static s16 sBOrbGraceTimer = 0;
static s16 sBOrbBuffTimer[BYRNA_EX_MAX] = { 0 };

// ---------------------------------------------------------------------------
// Collider
//
// DMG_SLASH_MASTER does double duty, both halves needed (same reasoning as the
// trident ball): it is a REAL weapon bit so the AT/AC match passes on every boss
// bumper (bosses key their super-damage path off BUMP_HIT, and a projectile the
// bumper rejects never registers at all), and each enemy's own DamageTable then
// resolves it as a Master Sword hit — which is the "one sword hit" the orb is
// specified to deal.
// ---------------------------------------------------------------------------
static ColliderCylinderInit sBOrbColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { DMG_SLASH_MASTER, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { BORB_RADIUS, BORB_HEIGHT, 0, { 0, 0, 0 } },
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static u8 BOrb_TargetIsUsable(Actor* target) {
    return (target != NULL) && (target->update != NULL) && (target->colChkInfo.health > 0);
}

// Lock-on first (the player aimed at it deliberately), then the nearest boss,
// then the nearest ordinary enemy.
static Actor* BOrb_AcquireTarget(PlayState* play, Actor* from) {
    Player* player = GET_PLAYER(play);
    Actor* found;

    if (player != NULL && BOrb_TargetIsUsable(player->focusActor)) {
        return player->focusActor;
    }
    found = Actor_FindNearby(play, from, -1, ACTORCAT_BOSS, BORB_SEEK_RANGE);
    if (BOrb_TargetIsUsable(found)) {
        return found;
    }
    found = Actor_FindNearby(play, from, -1, ACTORCAT_ENEMY, BORB_SEEK_RANGE);
    if (BOrb_TargetIsUsable(found)) {
        return found;
    }
    return NULL;
}

static u8 BOrb_ResolveExtract(Actor* target) {
    s32 i;

    if (target == NULL) {
        return BYRNA_EX_NONE;
    }
    // Bosses always feed RED. That closes the loop the orb is built around:
    // send it into a boss -> attack up -> the super-damage hits land harder.
    if (target->category == ACTORCAT_BOSS) {
        return BYRNA_EX_RED;
    }
    for (i = 0; i < (s32)(sizeof(sBOrbExtractTable) / sizeof(sBOrbExtractTable[0])); i++) {
        if (sBOrbExtractTable[i].actorId == target->id) {
            return sBOrbExtractTable[i].extract;
        }
    }
    for (i = 0; i < (s32)(sizeof(sBOrbNoExtract) / sizeof(sBOrbNoExtract[0])); i++) {
        if (sBOrbNoExtract[i] == target->id) {
            return BYRNA_EX_NONE;
        }
    }
    return BORB_EXTRACT_DEFAULT;
}

// The colour the orb currently shows: what it is carrying home, else the
// strongest buff running, else the unfed pale blue.
static u8 BOrb_VisualExtract(void) {
    s32 i;

    if (sOrb.carrying != BYRNA_EX_NONE) {
        return sOrb.carrying;
    }
    for (i = BYRNA_EX_RED; i < BYRNA_EX_MAX; i++) {
        if (sBOrbBuffTimer[i] > 0) {
            return (u8)i;
        }
    }
    return BYRNA_EX_NONE;
}

static void BOrb_Sparkle(PlayState* play, Vec3f* pos, u8 extract, s16 scale) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = sBOrbPrim[extract];
    Color_RGBA8 env = sBOrbEnv[extract];

    EffectSsKiraKira_SpawnDispersed(play, pos, &zero, &zero, &prim, &env, scale, 8);
}

static void BOrb_Burst(PlayState* play, Vec3f* pos, u8 extract, s32 count) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f vel;
    Color_RGBA8 prim = sBOrbPrim[extract];
    Color_RGBA8 env = sBOrbEnv[extract];
    s32 i;

    for (i = 0; i < count; i++) {
        vel.x = Rand_CenteredFloat(10.0f);
        vel.y = Rand_ZeroFloat(7.0f) + 1.0f;
        vel.z = Rand_CenteredFloat(10.0f);
        EffectSsKiraKira_SpawnDispersed(play, pos, &vel, &zero, &prim, &env, (s16)(Rand_ZeroOne() * 300.0f) + 500, 14);
    }
}

static void BOrb_Destroy(void) {
    if (sOrb.actor != NULL) {
        Actor_Kill(sOrb.actor);
    }
    sOrb.actor = NULL;
    sOrb.target = NULL;
    sOrb.carrying = BYRNA_EX_NONE;
    sOrb.colliderInited = 0;
    sOrb.state = BORB_STATE_ORBIT;
}

// ---------------------------------------------------------------------------
// Buffs
// ---------------------------------------------------------------------------
static void BOrb_ApplyExtract(PlayState* play, u8 extract) {
    if (extract == BYRNA_EX_NONE) {
        return;
    }
    // Green is instant and does not linger — it is a heal, not a state.
    if (extract == BYRNA_EX_GREEN) {
        Health_ChangeBy(play, BORB_HEAL_AMOUNT);
        Sfx_PlaySfxCentered(NA_SE_SY_HP_RECOVER);
        return;
    }
    sBOrbBuffTimer[extract] = BORB_BUFF_TIME;
    Sfx_PlaySfxCentered(NA_SE_SY_GET_BOXITEM);
}

// ---------------------------------------------------------------------------
// Per-state motion
// ---------------------------------------------------------------------------
static void BOrb_TickOrbit(Actor* self, Player* player) {
    f32 bob;

    sOrb.orbitAngle += BORB_ORBIT_SPEED;
    // Double frequency so the orb bobs twice per revolution — that is what makes
    // the ring read as a spiral rather than a flat disc.
    bob = Math_SinS((s16)(sOrb.orbitAngle * 2)) * BORB_ORBIT_BOB;

    // Pinned to Link every frame rather than integrated, so the ring can never
    // drift out of sync with him no matter what moves the player.
    self->world.pos.x = player->actor.world.pos.x + Math_SinS(sOrb.orbitAngle) * BORB_ORBIT_RADIUS;
    self->world.pos.z = player->actor.world.pos.z + Math_CosS(sOrb.orbitAngle) * BORB_ORBIT_RADIUS;
    self->world.pos.y = player->actor.world.pos.y + BORB_ORBIT_HEIGHT + bob;
}

static void BOrb_Home(Actor* self, Vec3f* goal, f32 speed, f32 lerp) {
    Vec3f to;
    f32 len;

    to.x = goal->x - self->world.pos.x;
    to.y = goal->y - self->world.pos.y;
    to.z = goal->z - self->world.pos.z;

    len = sqrtf(to.x * to.x + to.y * to.y + to.z * to.z);
    if (len < 1.0f) {
        return;
    }

    to.x = to.x / len * speed;
    to.y = to.y / len * speed;
    to.z = to.z / len * speed;

    sOrb.velocity.x += (to.x - sOrb.velocity.x) * lerp;
    sOrb.velocity.y += (to.y - sOrb.velocity.y) * lerp;
    sOrb.velocity.z += (to.z - sOrb.velocity.z) * lerp;

    self->world.pos.x += sOrb.velocity.x;
    self->world.pos.y += sOrb.velocity.y;
    self->world.pos.z += sOrb.velocity.z;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static void BOrb_Update(Actor* thisx, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u8 visual;

    if (sOrb.actor != thisx || player == NULL) {
        Actor_Kill(thisx);
        return;
    }

    // Lazy collider init — Collider_SetCylinder needs the actor to be live in
    // the actor list, which it is not yet inside Actor_Spawn.
    if (!sOrb.colliderInited) {
        Collider_InitCylinder(play, &sOrb.collider);
        Collider_SetCylinder(play, &sOrb.collider, thisx, &sBOrbColliderInit);
        sOrb.colliderInited = 1;
    }

    switch (sOrb.state) {
        case BORB_STATE_ORBIT:
            BOrb_TickOrbit(thisx, player);
            break;

        case BORB_STATE_OUTBOUND: {
            Vec3f goal;

            if (!BOrb_TargetIsUsable(sOrb.target)) {
                sOrb.target = BOrb_AcquireTarget(play, thisx);
            }
            if (!BOrb_TargetIsUsable(sOrb.target) || sOrb.flyTimer == 0) {
                // Nothing to harvest, or it took too long: come home empty.
                sOrb.state = BORB_STATE_RETURN;
                break;
            }
            sOrb.flyTimer--;

            goal.x = sOrb.target->world.pos.x;
            goal.y = (sOrb.target->world.pos.y + sOrb.target->focus.pos.y) * 0.5f;
            goal.z = sOrb.target->world.pos.z;
            BOrb_Home(thisx, &goal, BORB_FLY_SPEED, BORB_FLY_HOMING);
            break;
        }

        case BORB_STATE_RETURN: {
            Vec3f goal;
            f32 dx, dy, dz;

            goal.x = player->actor.world.pos.x;
            goal.y = player->actor.world.pos.y + BORB_ORBIT_HEIGHT;
            goal.z = player->actor.world.pos.z;
            BOrb_Home(thisx, &goal, BORB_RETURN_SPEED, 0.5f);

            dx = goal.x - thisx->world.pos.x;
            dy = goal.y - thisx->world.pos.y;
            dz = goal.z - thisx->world.pos.z;
            if (sqrtf(dx * dx + dy * dy + dz * dz) < BORB_CATCH_DIST) {
                BOrb_ApplyExtract(play, sOrb.carrying);
                BOrb_Burst(play, &thisx->world.pos, BOrb_VisualExtract(), 6);
                sOrb.carrying = BYRNA_EX_NONE;
                sOrb.state = BORB_STATE_ORBIT;
                sOrb.velocity.x = sOrb.velocity.y = sOrb.velocity.z = 0.0f;
                Sfx_PlaySfxCentered(NA_SE_PL_CATCH_BOOMERANG);
            }
            break;
        }
    }

    visual = BOrb_VisualExtract();
    BOrb_Sparkle(play, &thisx->world.pos, visual, (sOrb.state == BORB_STATE_ORBIT) ? 460 : 620);

    Collider_UpdateCylinder(thisx, &sOrb.collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sOrb.collider.base);

    if (sOrb.collider.base.atFlags & AT_HIT) {
        sOrb.collider.base.atFlags &= ~AT_HIT;

        if (sOrb.state == BORB_STATE_OUTBOUND) {
            // Open the grace window BEFORE anything else: the boss reads
            // BUMP_HIT next frame and must still see this orb as active.
            sBOrbGraceTimer = BORB_GRACE;
            sOrb.carrying = BOrb_ResolveExtract(sOrb.target);
            BOrb_Burst(play, &thisx->world.pos, sOrb.carrying, 8);
            sOrb.state = BORB_STATE_RETURN;
        }
        // An ORBITing orb keeps going after it grazes something: the LttP
        // barrier damages continuously, it does not spend itself on a kill.
    }
}

static void BOrb_Draw(Actor* thisx, PlayState* play) {
    // Intentionally empty — the update emits the KiraKira sparkle every frame,
    // which IS the visual. A display list here would mean loading an object into
    // the player object slot at runtime for no gain.
    (void)thisx;
    (void)play;
}

// ---------------------------------------------------------------------------
// Exported accessors
// ---------------------------------------------------------------------------
u8 ByrnaOrb_Summon(PlayState* play) {
    Player* player;
    Vec3f pos;
    Actor* actor;

    if (play == NULL || sOrb.actor != NULL) {
        return 0;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return 0;
    }
    if (!Magic_RequestChange(play, MAGIC_REQ(BORB_MAGIC_COST), MAGIC_CONSUME_NOW)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return 0;
    }

    pos = player->actor.world.pos;
    pos.y += BORB_ORBIT_HEIGHT;

    actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHTBOX, pos.x, pos.y, pos.z, 0, 0, 0, 0);
    if (actor == NULL) {
        return 0;
    }

    // EnLightbox_Init already ran inside Actor_Spawn and registered the lightbox's
    // solid DynaPoly box here — an invisible wall that would orbit Link with the
    // orb. Drop it (somaria_cubes.c / trident_charge_ball.c do the same); Destroy
    // then sees BGACTOR_NEG_ONE and skips its own delete.
    {
        DynaPolyActor* dyna = (DynaPolyActor*)actor;
        if (dyna->bgId != BGACTOR_NEG_ONE) {
            DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, dyna->bgId);
            dyna->bgId = BGACTOR_NEG_ONE;
        }
    }

    sOrb.actor = actor;
    sOrb.target = NULL;
    sOrb.state = BORB_STATE_ORBIT;
    sOrb.carrying = BYRNA_EX_NONE;
    sOrb.colliderInited = 0;
    sOrb.orbitAngle = 0;
    sOrb.flyTimer = 0;
    sOrb.velocity.x = sOrb.velocity.y = sOrb.velocity.z = 0.0f;

    actor->update = BOrb_Update;
    actor->draw = BOrb_Draw;

    BOrb_Burst(play, &pos, BYRNA_EX_NONE, 8);
    Sfx_PlaySfxCentered(NA_SE_PL_MAGIC_SOUL_BALL);
    return 1;
}

u8 ByrnaOrb_Launch(PlayState* play) {
    if (play == NULL || sOrb.actor == NULL || sOrb.state != BORB_STATE_ORBIT) {
        return 0;
    }

    sOrb.target = BOrb_AcquireTarget(play, sOrb.actor);
    if (!BOrb_TargetIsUsable(sOrb.target)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return 0;
    }

    sOrb.state = BORB_STATE_OUTBOUND;
    sOrb.flyTimer = BORB_FLY_TIMEOUT;
    sOrb.velocity.x = sOrb.velocity.y = sOrb.velocity.z = 0.0f;
    Sfx_PlaySfxCentered(NA_SE_IT_BOOMERANG_THROW);
    return 1;
}

u8 ByrnaOrb_TryAbsorb(PlayState* play) {
    Vec3f pos;

    // Only a barrier that is actually AROUND Link can shield him. An orb that is
    // out harvesting leaves him open — that is the whole risk of sending it.
    if (play == NULL || sOrb.actor == NULL || sOrb.state != BORB_STATE_ORBIT) {
        return 0;
    }

    pos = sOrb.actor->world.pos;
    BOrb_Burst(play, &pos, BOrb_VisualExtract(), 12);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_REFLECT_SW);
    BOrb_Destroy();
    return 1;
}

u8 ByrnaOrb_IsActive(void) {
    if (sBOrbGraceTimer > 0) {
        return 1;
    }
    // ONLY the launched orb carries the super-damage claim. An orbiting orb must
    // never make a boss treat proximity as a super hit — that is exactly the
    // failure trident_charge_ball.h warns about.
    return (sOrb.actor != NULL) && (sOrb.state == BORB_STATE_OUTBOUND);
}

void ByrnaOrb_Tick(void) {
    s32 i;

    if (sBOrbGraceTimer > 0) {
        sBOrbGraceTimer--;
    }
    for (i = BYRNA_EX_RED; i < BYRNA_EX_MAX; i++) {
        if (sBOrbBuffTimer[i] > 0) {
            sBOrbBuffTimer[i]--;
        }
    }
}

void ByrnaOrb_Forget(void) {
    // Deliberately NOT BOrb_Destroy(): on a scene load the actor heap has already
    // been wiped, so Actor_Kill would touch freed memory. Buffs are left running.
    sOrb.actor = NULL;
    sOrb.target = NULL;
    sOrb.carrying = BYRNA_EX_NONE;
    sOrb.colliderInited = 0;
    sOrb.state = BORB_STATE_ORBIT;
    sBOrbGraceTimer = 0;
}

void ByrnaOrb_Cleanup(void) {
    s32 i;

    BOrb_Destroy();
    for (i = 0; i < BYRNA_EX_MAX; i++) {
        sBOrbBuffTimer[i] = 0;
    }
    sBOrbGraceTimer = 0;
}

u8 ByrnaOrb_HasBuff(u8 extract) {
    if (extract >= BYRNA_EX_MAX) {
        return 0;
    }
    return sBOrbBuffTimer[extract] > 0;
}

u8 ByrnaOrb_DefenseActive(void) {
    return sBOrbBuffTimer[BYRNA_EX_ORANGE] > 0;
}

f32 ByrnaOrb_IncomingDamageMul(void) {
    return (sBOrbBuffTimer[BYRNA_EX_ORANGE] > 0) ? BORB_ORANGE_DEFENSE_MUL : 1.0f;
}

f32 ByrnaOrb_AttackMul(void) {
    return (sBOrbBuffTimer[BYRNA_EX_RED] > 0) ? BORB_RED_ATTACK_MUL : 1.0f;
}

f32 ByrnaOrb_SpeedMul(void) {
    return (sBOrbBuffTimer[BYRNA_EX_WHITE] > 0) ? BORB_WHITE_SPEED_MUL : 1.0f;
}

f32 ByrnaOrb_VaultMul(void) {
    return (sBOrbBuffTimer[BYRNA_EX_WHITE] > 0) ? BORB_WHITE_VAULT_MUL : 1.0f;
}

u8 ByrnaOrb_NoFlinch(void) {
    // MHR's triple-buff "final form": red + white + orange at once.
    return (sBOrbBuffTimer[BYRNA_EX_RED] > 0) && (sBOrbBuffTimer[BYRNA_EX_WHITE] > 0) &&
           (sBOrbBuffTimer[BYRNA_EX_ORANGE] > 0);
}
