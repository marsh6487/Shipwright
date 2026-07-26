/**
 * item_zonai_permafrost.c - Zonai Permafrost (time freeze TOGGLE)
 *
 * Controls:
 *   C Button: toggle the time freeze on / off
 *
 * Features:
 *   - Freezes all actors and the day/night cycle the instant you press the button
 *   - No cast animation and no fixed duration: it stays on until you press again
 *     or the magic meter runs dry
 *   - Link moves and fights freely during the effect
 *   - Green Zonai energy runes visual
 *
 * The freeze itself is delegated to timestop_helper (TIMECTL_OWNER_PERMAFROST,
 * the HIGHEST priority claim): a hard stop must win over Champion's Tunic bullet
 * time and over the Phantom Hourglass' rewind scrub. The helper re-applies the
 * freeze every frame from CustomItems_Update, so actors that spawn mid-effect are
 * caught too, it keeps frozen enemies hittable, and it restores the day/night
 * clock on release or scene change.
 *
 * Nothing here touches health, rupees or any flag, and it only ever spends its own
 * magic: damage dealt while the world is stopped, purchases made, and scene flags
 * set all persist normally.
 */

#include "z64.h"
#include "item_zonai_permafrost.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "../helpers/item_voice.h"
#include "../helpers/timestop_helper.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

// ============================================================================
// Audio
//
// Every cue this item plays is emitted from its OWN position vector rather than
// straight from Link's. The ice sounds are sustained samples and the ambient one
// re-triggers while the freeze is held, so they have to be killable in one call
// when the freeze ends — and stopping by Link's own position pointer would take
// his footsteps and everything else he is emitting down with them.
// ============================================================================

static Vec3f sZPermSfxPos;

static void ZPerm_Sfx(Player* p, u16 sfxId) {
    sZPermSfxPos = p->actor.world.pos;
    // Every cue here is fired ONCE, so the continuous flag has to come off. Bit 0x800
    // marks an sfx the caller re-requests every frame; the sound bank only ages those
    // while they sit in SFX_STATE_QUEUED, and its auto-reclaim path is explicitly gated
    // on !(sfxId & 0xC00). Fire one with the flag on and never ask again and it parks in
    // PLAYING forever — which is exactly why the ice sounds outlived the freeze. The
    // sample is chosen by sfxId & 0x1FF, so clearing 0x800 changes the lifetime, never
    // which sound you hear. (NA_SE_EV_ICE_FREEZE is 0x28B2, NA_SE_EV_ICE_MELT 0x28A2 —
    // both carry it.)
    Audio_PlaySoundGeneral(sfxId - SFX_FLAG, &sZPermSfxPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

/** Kill every cue this item still has in flight. */
static void ZPerm_SfxStopAll(void) {
    Audio_StopSfxByPos(&sZPermSfxPos);
}

// ============================================================================
// Visual Effects
// ============================================================================

/**
 * A ring of 8 green rune particles at the given radius. With the cast animation
 * gone this is no longer a slow expanding wind-up; it is stacked into a one-shot
 * flourish (see ZPerm_SpawnBurst) on toggle.
 */
static void ZPerm_SpawnRuneRing(Player* p, PlayState* play, f32 expandRadius) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 100, 255, 150, 255 }; // Bright Zonai green
    Color_RGBA8 envColor = { 0, 200, 80, 255 };     // Deep green

    for (u8 i = 0; i < 8; i++) {
        f32 angle = (f32)i * (65536.0f / 8.0f);
        s16 angleS = (s16)angle;

        Vec3f pos;
        pos.x = p->actor.world.pos.x + Math_SinS(angleS) * expandRadius;
        pos.y = p->actor.world.pos.y + 30.0f + Rand_CenteredFloat(20.0f);
        pos.z = p->actor.world.pos.z + Math_CosS(angleS) * expandRadius;

        Vec3f vel;
        vel.x = Math_SinS(angleS) * 3.0f;
        vel.y = Rand_ZeroFloat(1.5f);
        vel.z = Math_CosS(angleS) * 3.0f;

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 600, 20);
    }
}

/** Toggle flourish: three concentric rings of runes in a single frame. */
static void ZPerm_SpawnBurst(Player* p, PlayState* play) {
    ZPerm_SpawnRuneRing(p, play, 40.0f);
    ZPerm_SpawnRuneRing(p, play, 110.0f);
    ZPerm_SpawnRuneRing(p, play, 180.0f);
}

/**
 * Green particles hanging motionless in the air while the freeze is held.
 * 2 particles per frame at random positions around Link.
 */
static void ZPerm_SpawnFrozenParticles(Player* p, PlayState* play) {
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Vec3f vel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 120, 255, 160, 200 };
    Color_RGBA8 envColor = { 0, 180, 60, 150 };

    for (u8 i = 0; i < 2; i++) {
        Vec3f pos;
        pos.x = p->actor.world.pos.x + Rand_CenteredFloat(300.0f);
        pos.y = p->actor.world.pos.y + 20.0f + Rand_ZeroFloat(100.0f);
        pos.z = p->actor.world.pos.z + Rand_CenteredFloat(300.0f);

        EffectSsKiraKira_SpawnFocused(play, &pos, &vel, &accel, &primColor, &envColor, 400, 15);
    }
}

// ============================================================================
// Toggle On / Off
// ============================================================================

static void ZPerm_Stop(Player* p, PlayState* play) {
    if (!zpActive) {
        return;
    }

    // Release the world: actors resume on the next frame and the day/night clock
    // goes back to the speed it had before the freeze.
    TimeCtl_Release(TIMECTL_OWNER_PERMAFROST);

    ZPerm_SpawnBurst(p, play);
    Rumble_Request(200.0f, 100, 15, 40);
    // Kill the activation cue and every ambient tick still ringing BEFORE starting the
    // release one-shot, or the stop would swallow the cue we just started.
    ZPerm_SfxStopAll();
    ZPerm_Sfx(p, NA_SE_EV_ICE_MELT);

    zpActive = 0;
    zpState = ZPERM_STATE_IDLE;
    zpSubPhase = 0;
    zpTimer = 0;
    zpSavedTime = 0;
}

static void ZPerm_Start(Player* p, PlayState* play) {
    if (zpActive) {
        return;
    }

    if (!ItemMagic_HasEnough(play, ZPERM_MAGIC_ACTIVATION)) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    ItemMagic_Consume(play, ZPERM_MAGIC_ACTIVATION);

    // Straight to ACTIVE — there is no cast state any more. The old version locked
    // Link into a three-part Din's Fire animation first, which is exactly what the
    // toggle is meant to remove. Because nothing poses Link now, the freeze also
    // works in mid-air and in water, where the animation used to forbid it.
    zpActive = 1;
    zpState = ZPERM_STATE_ACTIVE;
    zpSubPhase = 0;
    zpTimer = 0;

    TimeCtl_Request(TIMECTL_OWNER_PERMAFROST, 0.0f, 1);

    ZPerm_SpawnBurst(p, play);
    ItemVoice_PlayId(p, NA_SE_VO_LI_MAGIC_NALE);
    Rumble_Request(300.0f, 150, 20, 60);
    ZPerm_SfxStopAll(); // clear anything left over from a previous toggle
    ZPerm_Sfx(p, NA_SE_EV_ICE_FREEZE);
}

// ============================================================================
// Upkeep while the freeze is held
// ============================================================================

static void ZPerm_StateActive(Player* p, PlayState* play) {
    u8 runningLow;

    // Defensive: nothing should be posing Link, but make sure he stays free to act.
    p->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);

    // The claim is re-applied to every actor each frame by TimeCtl_Update
    // (CustomItems_Update), which also catches actors that spawn mid-effect.

    // Elapsed counter, visuals only. Clamped so it cannot wrap the s16.
    if (zpTimer < 0x7000) {
        zpTimer++;
    }

    // Drain. Testing BEFORE spending means the freeze switches itself off on the
    // frame the meter can no longer pay, instead of going negative.
    // Ticked off zpTimer, not play->gameplayFrames: the counter resets on every
    // activation, so the first drain always lands a full interval after you switch
    // on rather than on whatever phase the global frame counter happened to be at.
    if ((zpTimer % ZPERM_DRAIN_INTERVAL) == 0) {
        if (!ItemMagic_HasEnough(play, ZPERM_DRAIN_COST)) {
            ZPerm_Stop(p, play);
            return;
        }
        ItemMagic_Consume(play, ZPERM_DRAIN_COST);
    }

    // Ambient frozen particles; they flicker once the meter is nearly out, which is
    // the player's warning that time is about to start moving again.
    runningLow = !ItemMagic_HasEnough(play, ZPERM_FLICKER_MAGIC);
    if (!runningLow || ((play->gameplayFrames % 4) >= 2)) {
        ZPerm_SpawnFrozenParticles(p, play);
    }

    // Ambient SFX
    if ((play->gameplayFrames % 40) == 0) {
        ZPerm_Sfx(p, NA_SE_EV_ICE_MELT);
    }
}

// ============================================================================
// Main Handler
// ============================================================================

void Handle_ZonaiPermafrost(Player* p, PlayState* play) {
    ItemInputState in;
    ItemInput_Update(&in, ITEM_ZONAI_PERMAFROST, p, play);

    // Unequipped: drop the freeze rather than stranding the world stopped.
    if (!in.wasEquipped) {
        if (zpActive) {
            ZPerm_Stop(p, play);
        }
        return;
    }

    if (zpActive) {
        // Toggle OFF. Handled before the upkeep so the press that ends the freeze
        // does not also pay a drain tick on its way out.
        if (in.isPressed) {
            ZPerm_Stop(p, play);
            return;
        }
        // No IsBlocked check here on purpose: once time is stopped the freeze must
        // survive whatever Link gets up to. Ending it is the player's call, or the
        // magic meter's.
        ZPerm_StateActive(p, play);
        return;
    }

    if (ItemInput_IsBlocked(p, play)) {
        return;
    }
    if (in.isPressed) {
        ZPerm_Start(p, play);
    }
}

// ============================================================================
// Stubs
// ============================================================================

void Player_InitZonaiPermafrostIA(PlayState* play, Player* p) {
    zpActive = 0;
    zpState = ZPERM_STATE_IDLE;
    zpSubPhase = 0;
    zpTimer = 0;
    zpSavedTime = 0;
}

s32 Player_UpperAction_ZonaiPermafrost(Player* p, PlayState* play) {
    return 0;
}
