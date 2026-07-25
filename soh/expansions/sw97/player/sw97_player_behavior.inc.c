/**
 * sw97_player_behavior.inc.c - Player behavior hooks for SW97 Medallion Spells
 *
 * Original actors: z64proto/sw97 team (Spaceworld '97 Experience)
 * Adapted for Ship of Harkinian (Shipwright)
 *
 * Provides CVar-gated hooks for:
 * - Magic spell actor spawning (6 spells mapped to spell indices 0-5)
 * - Helper functions for medallion/arrow item identification
 * - Medallion-to-arrow item conversion
 */

// Runtime actor IDs (set by sw97_init.cpp via ActorDB)
extern s16 gSw97ActorId_MagicFire;
extern s16 gSw97ActorId_MagicIce;
extern s16 gSw97ActorId_MagicLight;
extern s16 gSw97ActorId_MagicDark;
extern s16 gSw97ActorId_MagicSoul;
extern s16 gSw97ActorId_MagicWind;
extern s16 gSw97ActorId_ArrowFire;
extern s16 gSw97ActorId_ArrowIce;
extern s16 gSw97ActorId_ArrowLight;
extern s16 gSw97ActorId_ArrowDark;
extern s16 gSw97ActorId_ArrowSoul;
extern s16 gSw97ActorId_ArrowWind;

// SW97 magic spell costs: indices 0-5 match Player_ActionToMagicSpell output
// 0=Wind(12), 1=Soul(24), 2=Dark(12), 3=Ice(24), 4=Light(24), 5=Fire(12)
static u8 sSw97MagicSpellCosts[] = { 12, 24, 12, 24, 24, 12 };

// SW97 magic arrow costs: all 4 except light which is 8
static u8 sSw97MagicArrowCosts[] = { 4, 4, 8, 4, 4, 4 };

/**
 * Spawn the correct SW97 magic spell actor based on spell index.
 * Called from Player_SpawnMagicSpell in z_player.c.
 *
 * Spell index mapping (from Player_ActionToMagicSpell):
 *   0 = IA_MAGIC_SPELL_15 = Forest Medallion → MagicWind
 *   1 = IA_MAGIC_SPELL_16 = Spirit Medallion → MagicSoul
 *   2 = IA_MAGIC_SPELL_17 = Shadow Medallion → MagicDark
 *   3 = IA_FARORES_WIND   = Water Medallion  → MagicIce
 *   4 = IA_NAYRUS_LOVE    = Light Medallion  → MagicLight
 *   5 = IA_DINS_FIRE       = Fire Medallion   → MagicFire
 *
 * Returns the spawned actor, or NULL if SW97 spells are disabled.
 */
static Actor* Sw97_TrySpawnMagicSpell(PlayState* play, Player* player, s32 spell) {
    if (!SW97_MEDALLIONS_ENABLED()) {
        return NULL;
    }

    if (spell < 0 || spell >= 6) {
        return NULL;
    }

    // Shadow medallion heart→magic exchange is handled out-of-band in
    // soh/Enhancements/ShadowMedallionExchange.cpp via an OnPlayerUpdate hook,
    // so the exchange works even when the player has zero magic (otherwise the
    // cast flow short-circuits before reaching this function).

    s16* spellActorIds[] = {
        &gSw97ActorId_MagicWind,  // 0 = Forest
        &gSw97ActorId_MagicSoul,  // 1 = Spirit
        &gSw97ActorId_MagicDark,  // 2 = Shadow
        &gSw97ActorId_MagicIce,   // 3 = Water
        &gSw97ActorId_MagicLight, // 4 = Light
        &gSw97ActorId_MagicFire,  // 5 = Fire
    };

    s16 actorId = *spellActorIds[spell];
    if (actorId < 0) {
        return NULL;
    }

    Actor* spawned = Actor_Spawn(&play->actorCtx, play, actorId, player->actor.world.pos.x,
                                 player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0);

    // Tell teammates to spawn the same spell-effect actor on their side.
    // Spells follow the caster (attached_to_owner=1) — their visual stays
    // around the caster's dummy as long as the spell is active. Map the
    // spell index back to the corresponding HARPOON_VFX_KIND_SW97_MAGIC_*.
    if (spawned != NULL) {
        s32 vfxKindByIndex[] = {
            HARPOON_VFX_KIND_SW97_MAGIC_WIND,   // 0
            HARPOON_VFX_KIND_SW97_MAGIC_SOUL,   // 1
            HARPOON_VFX_KIND_SW97_MAGIC_DARK,   // 2
            HARPOON_VFX_KIND_SW97_MAGIC_ICE,    // 3
            HARPOON_VFX_KIND_SW97_MAGIC_LIGHT,  // 4
            HARPOON_VFX_KIND_SW97_MAGIC_FIRE,   // 5
        };
        Harpoon_NotifyVfxSpawn(spawned, vfxKindByIndex[spell], /*attachedToOwner=*/1);
    }
    return spawned;
}

/**
 * Check if an item ID is a quest medallion (spell mode).
 */
static s32 Sw97_IsMedallionItem(s32 item) {
    return (item >= ITEM_MEDALLION_FOREST && item <= ITEM_MEDALLION_LIGHT);
}

/**
 * Check if an item ID is an SW97 arrow variant (arrow mode).
 */
static s32 Sw97_IsArrowItem(s32 item) {
    return (item >= ITEM_SW97_ARROW_FIRE && item <= ITEM_SW97_ARROW_WIND);
}

/**
 * Convert a medallion item to its corresponding SW97 arrow item.
 * Used by L+C swap in z_player.c.
 */
s32 Sw97_MedallionToArrowItem(s32 medallionItem) {
    switch (medallionItem) {
        case ITEM_MEDALLION_FIRE:
            return ITEM_SW97_ARROW_FIRE;
        case ITEM_MEDALLION_WATER:
            return ITEM_SW97_ARROW_ICE;
        case ITEM_MEDALLION_LIGHT:
            return ITEM_SW97_ARROW_LIGHT;
        case ITEM_MEDALLION_SHADOW:
            return ITEM_SW97_ARROW_DARK;
        case ITEM_MEDALLION_SPIRIT:
            return ITEM_SW97_ARROW_SOUL;
        case ITEM_MEDALLION_FOREST:
            return ITEM_SW97_ARROW_WIND;
        default:
            return ITEM_NONE;
    }
}

/**
 * Returns true while the Shadow Medallion spell (MagicDark) is active.
 * MagicDark drives gSaveContext.nayrusLoveTimer for its lifetime; in SW97 mode
 * the Shadow medallion replaces Nayru's Love (Light medallion is the new NL slot),
 * so a nonzero timer + SW97 enabled uniquely identifies "Shadow stealth is on".
 *
 * Consumed by z_actor.c so enemies/NPCs can't detect Link (same hook point as
 * MmMaskWear_IsStoneMaskActive).
 */
s32 Sw97_ShadowStealthActive(void) {
    if (!SW97_MEDALLIONS_ENABLED()) return 0;
    return gSaveContext.nayrusLoveTimer > 0;
}

/**
 * Shadow Medallion heart→magic exchange.
 *
 * Hold the C-button that has ITEM_MEDALLION_SHADOW for SHADOW_EXCHANGE_HOLD_FRAMES
 * frames → spend 3 hearts, gain 24 magic. Disarmed until release.
 *
 * Must work even at zero magic (the vanilla cast pipeline short-circuits before
 * reaching Sw97_TrySpawnMagicSpell when magic is insufficient — playing only the
 * "no magic" error sound — so the exchange has to live in a per-frame tick).
 *
 * Called from z_player.c Player_UpdateCommon each frame.
 */
#define SHADOW_EXCHANGE_HOLD_FRAMES 20
#define SHADOW_EXCHANGE_HEART_COST  (3 * 0x10)  // 3 hearts × 16 HP
#define SHADOW_EXCHANGE_MAGIC_GAIN  24

void Sw97_TickShadowExchange(PlayState* play, Player* player) {
    if (!SW97_MEDALLIONS_ENABLED()) return;
    if (play == NULL || player == NULL) return;

    // Find which C-slot has the Shadow medallion. buttonItems[0]=B, [1..3]=C-LDR.
    u16 medallionMask = 0;
    if (gSaveContext.equips.buttonItems[1] == ITEM_MEDALLION_SHADOW) medallionMask |= BTN_CLEFT;
    if (gSaveContext.equips.buttonItems[2] == ITEM_MEDALLION_SHADOW) medallionMask |= BTN_CDOWN;
    if (gSaveContext.equips.buttonItems[3] == ITEM_MEDALLION_SHADOW) medallionMask |= BTN_CRIGHT;

    static s16 sShadowHoldFrames = 0;
    static u8 sShadowExchanged = 0;

    if (medallionMask == 0) {
        sShadowHoldFrames = 0;
        sShadowExchanged = 0;
        return;
    }

    u16 cur = play->state.input[0].cur.button;
    if (!(cur & medallionMask)) {
        sShadowHoldFrames = 0;
        sShadowExchanged = 0;
        return;
    }

    sShadowHoldFrames++;
    if (sShadowExchanged) return;
    if (sShadowHoldFrames < SHADOW_EXCHANGE_HOLD_FRAMES) return;
    if (gSaveContext.health <= SHADOW_EXCHANGE_HEART_COST) return;

    gSaveContext.health -= SHADOW_EXCHANGE_HEART_COST;
    gSaveContext.magic += SHADOW_EXCHANGE_MAGIC_GAIN;
    if (gSaveContext.magic > gSaveContext.magicCapacity) {
        gSaveContext.magic = gSaveContext.magicCapacity;
    }
    Audio_PlayActorSound2(&player->actor, NA_SE_SY_GET_RUPY);
    sShadowExchanged = 1;
}

/**
 * Shadow-element blindness — per-actor stealth.
 *
 * When Shadow ARROW (ARROW_SW97_0C) hits an enemy, OR when the Gust Jar's
 * Shadow-element BLOW pushes an enemy, the target is "blinded" for ~10
 * seconds: z_actor.c's distance-to-player calculation is spoofed to 32000
 * (same mechanism as Stone Mask + Shadow Medallion stealth), so the enemy
 * stops tracking Link until the timer expires.
 *
 * Storage is a small static table indexed by Actor*. Capacity 32 is plenty
 * for the worst-case crowd you'd reasonably blind in one fight. New tags
 * upsert (longer-of duration); expired slots are reused.
 *
 * `Sw97_TickBlindness` MUST be called once per frame from z_player.c so
 * `framesRemaining` actually counts down.
 */
#define SW97_BLIND_TABLE_SIZE 32
#define SW97_BLIND_DURATION   300  // 10 sec at SW97's 30fps timer convention

typedef struct {
    Actor* actor;
    s16    framesRemaining;
} Sw97BlindEntry;

static Sw97BlindEntry sSw97Blinded[SW97_BLIND_TABLE_SIZE];

void Sw97_TagBlinded(Actor* actor, s16 frames) {
    if (actor == NULL || actor->update == NULL) return;
    s32 empty = -1;
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == actor) {
            if (frames > sSw97Blinded[i].framesRemaining) {
                sSw97Blinded[i].framesRemaining = frames;
            }
            return;
        }
        if (sSw97Blinded[i].actor == NULL && empty < 0) empty = i;
    }
    if (empty >= 0) {
        sSw97Blinded[empty].actor = actor;
        sSw97Blinded[empty].framesRemaining = frames;
    }
}

s32 Sw97_IsBlinded(Actor* actor) {
    if (actor == NULL) return 0;
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == actor && sSw97Blinded[i].framesRemaining > 0) {
            return 1;
        }
    }
    return 0;
}

void Sw97_TickBlindness(void) {
    for (s32 i = 0; i < SW97_BLIND_TABLE_SIZE; i++) {
        if (sSw97Blinded[i].actor == NULL) continue;
        // Drop dead actors immediately so we don't keep their pointer.
        if (sSw97Blinded[i].actor->update == NULL) {
            sSw97Blinded[i].actor = NULL;
            sSw97Blinded[i].framesRemaining = 0;
            continue;
        }
        if (--sSw97Blinded[i].framesRemaining <= 0) {
            sSw97Blinded[i].actor = NULL;
            sSw97Blinded[i].framesRemaining = 0;
        }
    }
}

/**
 * Cucco Mode — Soul arrow + Cucco → 30-second transformation.
 *
 * Triggered by `ArrowSoul_TryTransform` when the soul arrow hits an `EN_NIW`.
 * Visual: Cucco model swap on the Player draw function. Movement: a Flappy
 * Bird-style flap (A press while airborne = upward burst, reduced gravity for
 * slow fall). Bow / slingshot shots become elemental eggs (free, no magic
 * cost). R = spawn 3 attack-cuccos orbiting Link. B in air = peck dive.
 *
 * State is global so other systems (player draw hook, input intercept, egg
 * spawner) can query without threading through a parameter.
 */
#define CUCCO_MODE_FRAMES    1800  // 30 sec
#define CUCCO_FLAP_VELOCITY  9.0f  // upward burst per A press while airborne
#define CUCCO_GRAVITY       -1.2f  // Cucco terminal: gentle fall, not Link's -7
#define CUCCO_MAX_VY_DOWN   -3.0f  // Cucco terminal velocity cap (float, not plummet)
#define CUCCO_SPEED_MULT     1.15f // Slightly faster than Link
#define CUCCO_SPEED_MAX      11.0f // Cap so flap doesn't compound forever

s32 gSw97CuccoModeActive  = 0;
// Pending → waiting for Link to leave PLAYER_STATE1_IN_ITEM_CS (the
// first-person aim/throw cutscene). Same pattern as magic_soul.inc.c:117
// where the diamond update returns until the player is free. Without this
// the camera stays glued in first-person mode and breaks on entry.
s32 gSw97CuccoModePending = 0;
s32 gSw97CuccoModeTimer   = 0;
// Once-shot exit fx flag — guarantees the un-transform flash/sound only
// plays once even though Sw97_TickCuccoMode keeps running on inactive.
static s32 gSw97CuccoExitFx = 0;
// 180° flip animation on egg throw — counts down each frame, used by
// Sw97_DrawCuccoModel to rotate the model. ~12 frames = ~0.4s flip.
s32   gSw97CuccoFlipTimer = 0;
#define CUCCO_FLIP_FRAMES 12

// Cucco draw — direct copy of HGrace's draw-override pattern (no actor
// puppet). Skeleton inited once per cucco-mode session via Sw97_InitCuccoSkel,
// rendered via Sw97_DrawCucco which Link's actor.draw points at.
#include "objects/object_niw/object_niw.h"
static SkelAnime sCuccoSkel;
static Vec3s sCuccoJointTable[16];
static Vec3s sCuccoMorphTable[16];
static u8 sCuccoSkelInited = 0;

static void Sw97_InitCuccoSkel(PlayState* play) {
    if (sCuccoSkelInited) return;
    SkelAnime_InitFlex(play, &sCuccoSkel, (FlexSkeletonHeader*)&gCuccoSkel,
                       (AnimationHeader*)&gCuccoAnim,
                       sCuccoJointTable, sCuccoMorphTable, 16);
    sCuccoSkelInited = 1;
}

// Null-body override — used to walk Link's skeleton without rendering any
// limb geometry. Same idea as GaroForm_OverrideLimbDraw in
// garo_post_limb.cpp:47.
static s32 Sw97_CuccoLinkOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList,
                                          Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play; (void)limbIndex; (void)pos; (void)rot; (void)arg;
    *dList = NULL;
    return 0;
}

// PostLimbDraw — refreshes the per-frame Link tracking fields that vanilla
// Player_Draw normally populates: bodyPartsPos[], focus.pos (Navi anchor),
// feetPos[] (shadow anchor). Without this, shadow + Navi stay frozen at the
// transformation point. Copied from GaroForm_PostLimbDraw (the essential
// shadow/Navi/bodyParts bits — Garo's sword-trail / held-actor branches are
// not needed for the cucco model swap).
static void Sw97_CuccoLinkPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList,
                                       Vec3s* rot, void* thisx) {
    (void)dList; (void)rot;
    Player* player = (Player*)thisx;
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };

    if (limbIndex > 0 && limbIndex < PLAYER_LIMB_MAX) {
        s8 bodyPart = gPlayerLimbToBodyPart[limbIndex];
        if (bodyPart >= 0) {
            Matrix_MultVec3f(&zeroVec, &player->bodyPartsPos[bodyPart]);
        }
    }
    if (limbIndex == PLAYER_LIMB_HEAD) {
        Vec3f headOffset = { 1100.0f, -700.0f, 0.0f };
        Matrix_MultVec3f(&headOffset, &player->actor.focus.pos);
    }
    if (limbIndex == PLAYER_LIMB_L_FOOT || limbIndex == PLAYER_LIMB_R_FOOT) {
        Actor_SetFeetPos(&player->actor, limbIndex,
                         PLAYER_LIMB_L_FOOT, &zeroVec,
                         PLAYER_LIMB_R_FOOT, &zeroVec);
    }
}

// Cucco visual at thisx->world.pos. Same scaled translate + Y-flip on egg
// throws as before, called from Sw97_DrawCuccoForm after the null-body pass.
static void Sw97_DrawCuccoModel(Actor* thisx, PlayState* play) {
    if (!sCuccoSkelInited) return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    f32 baseYaw = (f32)thisx->shape.rot.y * (M_PI / 32768.0f);
    f32 flipYaw = 0.0f;
    if (gSw97CuccoFlipTimer > 0) {
        f32 t = (f32)gSw97CuccoFlipTimer / (f32)CUCCO_FLIP_FRAMES;
        flipYaw = (1.0f - t) * M_PI;
    }
    Matrix_RotateY(baseYaw + flipYaw, MTXMODE_APPLY);
    f32 s = 0.015f;
    Matrix_Scale(s, s, s, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, sCuccoSkel.skeleton, sCuccoSkel.jointTable,
                          sCuccoSkel.dListCount, NULL, NULL, NULL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Public entry — called from customequipment.cpp's VB_PLAYER_DRAW_BEGIN hook
// when cucco mode is active. Mirrors the GaroForm / MmForm pattern:
//   Pass 1: walk Link's skeleton with nulled DLs so PostLimbDraw refreshes
//           bodyPartsPos / focus.pos / feetPos[] → shadow + Navi follow
//   Pass 2: render the cucco model at Link's world.pos
void Sw97_DrawCuccoForm(PlayState* play, Player* player) {
    if (player->skelAnime.skeleton != NULL && player->skelAnime.jointTable != NULL) {
        SkelAnime_DrawFlexLod(play, player->skelAnime.skeleton, player->skelAnime.jointTable,
                              player->skelAnime.dListCount, Sw97_CuccoLinkOverrideLimbDraw,
                              Sw97_CuccoLinkPostLimbDraw, player, 0);
    }
    Sw97_DrawCuccoModel(&player->actor, play);
}

void Sw97_StartCuccoMode(void) {
    if (gSw97CuccoModeActive || gSw97CuccoModePending) return; // idempotent
    // Don't activate immediately — Link is in first-person aim CS right
    // now. Set pending and let the per-frame tick activate once the
    // first-person camera setting releases (mirror magic_soul.inc.c:117).
    gSw97CuccoModePending = 1;
}

static void Sw97_ActivateCuccoMode(void) {
    gSw97CuccoModePending = 0;
    gSw97CuccoModeActive  = 1;
    gSw97CuccoModeTimer   = CUCCO_MODE_FRAMES;
}

void Sw97_EndCuccoMode(void) {
    if (!gSw97CuccoModeActive && !gSw97CuccoModePending) return;
    gSw97CuccoModeActive  = 0;
    gSw97CuccoModePending = 0;
    gSw97CuccoModeTimer   = 0;
    gSw97CuccoExitFx      = 1;
    // Player flag cleanup + draw restoration happens in the inactive branch
    // of Sw97_TickCuccoMode on the next frame.
}

s32 Sw97_IsCuccoModeActive(void) {
    return gSw97CuccoModeActive;
}

// Scene tracking — reset state on scene change so the next tick doesn't
// reference a stale collision context / pos.
static s32 gSw97CuccoLastScene = -1;

// ───────────────────────────────────────────────────────────────────────
// Cucco eggs — while cucco mode is active, ANY arrow Link fires via the
// vanilla bow/slingshot aim+release CS gets swapped visually to a pocket
// egg + throttled to a slow drift. Elemental params (fire/ice/light/dark/
// soul/wind) are still respected — the SW97 hit hooks fire normally
// because the underlying actor is still EnArrow. Aim behavior is 100%
// vanilla: user pulls the bow, aims first-person, releases → egg flies.
// ───────────────────────────────────────────────────────────────────────

// Cucco egg tuning — vanilla arrows use Actor_SetProjectileSpeed(150), so
// SPEED_MAX / 150 is the scale factor applied to speedXZ AND velocity.y on
// the release frame (preserves the aim's pitch angle proportionally). Extra
// timer beat keeps eggs airborne long enough to reach enemies at range,
// which also fixes the "no damage" report — vanilla timer=12 was too short
// once we slowed the egg down, so it died before hitting anything.
#define CUCCO_EGG_SPEED_MAX  30.0f
#define CUCCO_EGG_TIMER      40   // frames of flight (vanilla arrow = 12)
#define CUCCO_EGG_ARC_BOOST  1.5f // extra +vY on release for a proper egg arc

// Needed to bump EnArrow::timer from the update hook (extend flight time).
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

// Banjo-Kazooie style 3D egg — vanilla 3D bubble sphere (gEffBubbleDL) with:
//   * ellipsoid scaling (Y taller than X = Z)
//   * per-element primColor tint (fire=red, ice=cyan, light=gold, dark=purple,
//     wind=green, soul=amber, neutral=white)
//   * envColor darker shade for a soft outline highlight
// The bubble DL expects a texture at segment 0x08; we bind gEffBubble1Tex so
// the surface has a subtle patterned shading (like BK's slight egg noise).
#include "objects/gameplay_keep/gameplay_keep.h"
static void Sw97_CuccoEgg_GetColors(s16 arrowParams, Color_RGBA8* prim, Color_RGBA8* env) {
    switch (arrowParams) {
        case ARROW_SW97_FIRE:  *prim = (Color_RGBA8){ 255, 110,  40, 255 }; *env = (Color_RGBA8){ 180,  30,   0, 255 }; break;
        case ARROW_SW97_ICE:   *prim = (Color_RGBA8){ 100, 210, 255, 255 }; *env = (Color_RGBA8){  10,  90, 200, 255 }; break;
        case ARROW_SW97_LIGHT: *prim = (Color_RGBA8){ 255, 240, 130, 255 }; *env = (Color_RGBA8){ 200, 150,   0, 255 }; break;
        case ARROW_SW97_0C:    *prim = (Color_RGBA8){ 150,  70, 210, 255 }; *env = (Color_RGBA8){  60,  10, 120, 255 }; break; // Dark
        case ARROW_SW97_0D:    *prim = (Color_RGBA8){ 255, 200, 100, 255 }; *env = (Color_RGBA8){ 180, 130,   0, 255 }; break; // Soul
        case ARROW_SW97_0E:    *prim = (Color_RGBA8){ 150, 255, 150, 255 }; *env = (Color_RGBA8){   0, 130,   0, 255 }; break; // Wind
        default:               *prim = (Color_RGBA8){ 255, 255, 255, 255 }; *env = (Color_RGBA8){ 130, 130, 130, 255 }; break;
    }
}

void Sw97_DrawCuccoEgg(Actor* thisx, PlayState* play) {
    Color_RGBA8 prim, env;
    Sw97_CuccoEgg_GetColors(thisx->params, &prim, &env);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY((f32)thisx->shape.rot.y * (M_PI / 32768.0f), MTXMODE_APPLY);
    // Ellipse: X = Z base, Y taller for the classic egg silhouette.
    Matrix_Scale(0.02f, 0.028f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++,
              Matrix_NewMtx(play->state.gfxCtx, "cucco_egg", __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, prim.r, prim.g, prim.b, prim.a);
    gDPSetEnvColor (POLY_OPA_DISP++, env.r, env.g, env.b, env.a);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gEffBubble1Tex));
    gSPDisplayList(POLY_OPA_DISP++, gEffBubbleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Called from customequipment.cpp's OnActorInit hook when an EnArrow spawns
// while cucco mode is active. Marks the arrow via home.rot.z (unused by
// EnArrow) so the Update hook can identify + throttle it, and swaps its draw
// to the pocket-egg model.
#define CUCCO_EGG_MARKER 0x1E66
void Sw97_TagCuccoEgg(Actor* arrow) {
    arrow->draw = Sw97_DrawCuccoEgg;
    arrow->home.rot.z = CUCCO_EGG_MARKER;
}

// Called from customequipment.cpp's OnActorUpdate hook every frame the
// tagged arrow is alive.
//
// Vanilla release: Actor_SetProjectileSpeed(actor, 150) sets
//   speedXZ    = cos(rot.x) * 150   (horizontal component from aim pitch)
//   velocity.y = -sin(rot.x) * 150  (vertical component from aim pitch)
// So the release is very fast (150 units/frame) and its pitch encodes the
// aim direction. Vanilla timer = 12 frames → 1800-unit range.
//
// For a BK-style thrown egg we want ~⅕ speed BUT proportionally more
// airtime so the range is still usable AND enemies can be hit (short
// timer + slow speed = "no damage" report). On the release frame we:
//   1. Scale BOTH speedXZ and velocity.y by SPEED_MAX/150 (preserves the
//      aim pitch: steep aim still steep, flat still flat).
//   2. Add a small upward boost so eggs always start with a clean arc
//      instead of nose-diving on flat aim.
//   3. Extend arrow->timer well past vanilla so the egg reaches enemies.
// home.rot.z encodes state (MARKER = tagged pre-fire, MARKER+1 = scaled).
#define CUCCO_EGG_VANILLA_RELEASE_SPEED 150.0f
void Sw97_TickCuccoEggClamp(Actor* arrow) {
    if (arrow->home.rot.z == 0) return;
    EnArrow* enArrow = (EnArrow*)arrow;
    if (arrow->home.rot.z == CUCCO_EGG_MARKER) {
        // Pre-fire: wait for release frame (speedXZ jumps above vanilla
        // "held" range — anything > 10 means the projectile-speed set fired).
        if (arrow->speedXZ > 10.0f) {
            f32 scale = CUCCO_EGG_SPEED_MAX / CUCCO_EGG_VANILLA_RELEASE_SPEED;
            arrow->speedXZ    *= scale;
            arrow->velocity.y *= scale;
            arrow->velocity.y += CUCCO_EGG_ARC_BOOST;  // clean arc
            enArrow->timer     = CUCCO_EGG_TIMER;      // extend flight
            arrow->home.rot.z  = CUCCO_EGG_MARKER + 1;
        }
    } else {
        // Post-fire: cap horizontal speed and keep the timer topped up so
        // the slow egg has time to reach enemies at range. Bumping (not
        // reset) — if a wall clamp already dropped it to 20, we don't want
        // to make the arrow immortal, just make sure it lives long enough
        // to hit its target at BK-style thrown-egg speed.
        if (arrow->speedXZ > CUCCO_EGG_SPEED_MAX) {
            arrow->speedXZ = CUCCO_EGG_SPEED_MAX;
        }
        if (enArrow->timer < CUCCO_EGG_TIMER - 1) {
            enArrow->timer = CUCCO_EGG_TIMER - 1;
        }
    }
}

void Sw97_TickCuccoMode(PlayState* play, Player* player) {
    // ─── PENDING: wait for first-person aim CS to end ───────────────────
    // magic_soul.inc.c:117 pattern — defer activation until the player has
    // left PLAYER_STATE1_IN_ITEM_CS. Activating mid-aim leaves the camera
    // setting stuck in first-person mode and the next setting change
    // glitches the angle.
    if (gSw97CuccoModePending && player != NULL && play != NULL) {
        if (!(player->stateFlags1 & PLAYER_STATE1_IN_ITEM_CS)) {
            Sw97_ActivateCuccoMode();
            // Flash + sound on actual entry (mirrors magic_soul's flash
            // before kill on line 123).
            func_800AA000(200.0f, 150, 20, 80);
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
        }
    }

    // ─── INACTIVE: cleanup ──────────────────────────────────────────────
    // The VB_PLAYER_DRAW_BEGIN hook in customequipment.cpp checks
    // Sw97_IsCuccoModeActive() each frame, so just deactivating the flag
    // is enough — no draw swap to undo. We only do the entry-exit flash
    // once via gSw97CuccoExitFx.
    if (!gSw97CuccoModeActive) {
        if (gSw97CuccoExitFx && player != NULL) {
            gSw97CuccoExitFx = 0;
            player->invincibilityTimer = 20;
            sCuccoSkelInited = 0;
            Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_M);
            func_800AA000(200.0f, 150, 20, 80);
        }
        gSw97CuccoLastScene = -1;
        return;
    }

    if (--gSw97CuccoModeTimer <= 0) {
        Sw97_EndCuccoMode();
        return;
    }

    if (player == NULL || play == NULL) return;

    // Scene change → skel seg pointers reference the old scene's gfxCtx; re-init.
    if (gSw97CuccoLastScene >= 0 && gSw97CuccoLastScene != play->sceneNum) {
        sCuccoSkelInited = 0;
    }
    gSw97CuccoLastScene = play->sceneNum;


    // ─── First-frame setup: init the cucco skel ─────────────────────────
    // We do NOT swap player->actor.draw — the customequipment.cpp
    // VB_PLAYER_DRAW_BEGIN hook detects Sw97_IsCuccoModeActive() and routes
    // through Sw97_DrawCuccoForm, which walks Link's skeleton (null body) to
    // keep shadow + Navi tracking, then draws the cucco model on top.
    if (!sCuccoSkelInited) {
        Sw97_InitCuccoSkel(play);
    }

    // ─── Ivan-style: do NOT disable input/colliders, do NOT zero velocity
    // and do NOT do manual position math. Vanilla Player movement runs
    // normally — sword swings, walking anim, item C-buttons, doors, ladders,
    // collision — and we only TWEAK the physics quantities the engine
    // already produced.

    // 1) Cucco fall: clamp downward velocity to terminal float speed.
    // The engine added vanilla gravity (~-7) into velocity.y this frame;
    // clipping it to -3 makes Link float instead of plummet, without
    // touching `actor.gravity` (which gets stomped each frame by
    // Player_StepHorizontalSpeed @ z_player.c:7870 anyway).
    if (player->actor.velocity.y < CUCCO_MAX_VY_DOWN) {
        player->actor.velocity.y = CUCCO_MAX_VY_DOWN;
    }

    // 2) Cucco speed: small horizontal boost over vanilla.
    player->linearVelocity *= CUCCO_SPEED_MULT;
    player->actor.speedXZ  *= CUCCO_SPEED_MULT;
    if (player->linearVelocity > CUCCO_SPEED_MAX) {
        player->linearVelocity = CUCCO_SPEED_MAX;
    }
    if (player->actor.speedXZ > CUCCO_SPEED_MAX) {
        player->actor.speedXZ = CUCCO_SPEED_MAX;
    }

    // 3) A press → flap burst (Flappy Bird), ALWAYS — including on ground,
    // so cucco never rolls. The A press is also cleared from Link's input
    // in the customequipment.cpp VB_SM64_PLAYER_PRE_ACTION hook so his
    // actionFunc doesn't trigger the roll before we get here.
    u8 grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    u8 aPress   = CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A);
    if (aPress) {
        player->actor.velocity.y = CUCCO_FLAP_VELOCITY;
        Audio_PlayActorSound2(&player->actor, NA_SE_EV_CHICKEN_CRY_A);
    }

    // 4) Face the camera when idle. OOT keeps shape.rot.y frozen when Link
    // stops moving — the cucco would keep pointing at the last direction he
    // walked. Smoothly turn to the camera's aim yaw whenever the stick is
    // idle, so the cucco always looks at what the player looks at.
    f32 hSpeed = fabsf(player->linearVelocity);
    f32 stickMag;
    s16 stickAngle;
    func_80077D10(&stickMag, &stickAngle, &play->state.input[0]);
    if (stickMag < 10.0f && hSpeed < 0.5f) {
        s16 camYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
        Math_SmoothStepToS(&player->actor.shape.rot.y, camYaw, 4, 0x800, 0x100);
    }

    // ─── Cucco wing animation rate, responsive to Link's state ──────────
    // Only one cucco anim exists in vanilla OOT (gCuccoAnim) — a wing-flap
    // loop. Modulate playSpeed to imply walking/running/flying visually:
    //   Airborne + moving  → 4.0× (rapid flap)
    //   Airborne + still   → 2.5×
    //   Ground + running   → 2.0× (fast walking-like flap)
    //   Ground + walking   → 1.4× (moderate)
    //   Ground + idle      → 0.7× (subtle idle bob)
    f32 rate;
    if (!grounded) {
        rate = (hSpeed > 1.5f) ? 4.0f : 2.5f;
    } else if (hSpeed > 8.0f) {
        rate = 2.0f;
    } else if (hSpeed > 1.5f) {
        rate = 1.4f;
    } else {
        rate = 0.7f;
    }
    sCuccoSkel.playSpeed = rate;
    SkelAnime_Update(&sCuccoSkel);

    // Tick down the 180° flip timer used by Sw97_DrawCuccoModel on egg throws.
    if (gSw97CuccoFlipTimer > 0) gSw97CuccoFlipTimer--;
}
