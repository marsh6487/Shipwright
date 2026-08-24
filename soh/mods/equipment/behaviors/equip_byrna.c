/**
 * equip_byrna.c - Cane of Byrna (Extended Sword Slot 1)
 *
 * Behavior: Biggoron Sword IA (long range, two-handed) + HP & MP recovery on hit.
 * - Forces PLAYER_IA_SWORD_BIGGORON for long reach
 * - Forces swordHealth > 0 so charge/spin attacks work
 * - Draws Somaria cane mesh with BLUE materials at 1.15x scale
 * - Follows left hand rotation (sword hand)
 * - On melee hit: recover HP + MP
 *
 * Included by ext_equip_behavior.c (unity build).
 */

// Byrna 3D model (blue cane) now lives in soh.o2r as
// objects/object_somaria/g_byrna_cane_dl and is loaded at draw time in
// extended_equipment.c (Byrna_GetCaneDL). No inline C model here anymore.

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define BYRNA_HP_RECOVER 16         // HP recovered per hit
#define BYRNA_MP_RECOVER 4          // MP recovered per hit
#define BYRNA_SCALE (0.05f * 1.15f) // Somaria base scale * 1.15

// ---------------------------------------------------------------------------
// Melee Hit Callback
// ---------------------------------------------------------------------------
static void GreatFairySword_RecoverOnHit(Player* player, PlayState* play) {
    s32 damage = 0;

    if (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) {
        damage = player->meleeWeaponQuads[0].info.toucher.damage;
    } else if (player->meleeWeaponQuads[1].base.atFlags & AT_HIT) {
        damage = player->meleeWeaponQuads[1].info.toucher.damage;
    }

    if (damage <= 0)
        return;

    // Recover 16 HP per hit
    Health_ChangeBy(play, BYRNA_HP_RECOVER);

    // Recover 16 MP per hit
    gSaveContext.magic += BYRNA_MP_RECOVER;
    if (gSaveContext.magic > gSaveContext.magicCapacity) {
        gSaveContext.magic = gSaveContext.magicCapacity;
    }
}

// ---------------------------------------------------------------------------
// Cane of Byrna — Insect Glaive (Skijer 2026-08-15).
//
// The slot's HP/MP-on-hit recovery moved to the Great Fairy's Sword below and is
// NOT coming back. What lives here now is the MHR Insect Glaive kit; see
// nei_hd_models/gerudo_mhr_dualblades_lab/MHR_EXT_SWORD_PORT_SPEC.md §12.
//
// STAGED ON PURPOSE. This is phase 1-3 of that plan: the light orb / Kinsect
// only. The glaive MOVESET (phase 4) will take over B and bring the forced
// PLAYER_IA_SWORD_BIGGORON base with it — until then the player keeps whatever
// sword they had, which is exactly what makes the orb testable on its own.
// Consequence while phase 4 is pending: holding B still charges the vanilla spin
// attack alongside the orb charge, and R still raises a real shield. Both stop
// once the moveset owns those buttons.
//
//   B held  -> charge, then summon the orb (costs magic)
//   R + B   -> send the orb at a target; it harvests an extract and returns
// ---------------------------------------------------------------------------
#define BYRNA_CHARGE_FRAMES 15 // ~0.75 s at 20 Hz (R_UPDATE_RATE = 3)

static s16 sByrnaChargeTimer = 0;

static void Byrna_Behavior(Player* player, PlayState* play) {
    Input* in;
    u8 bHeld;
    u8 bPress;
    u8 rHeld;

    if (player == NULL || play == NULL) {
        return;
    }
    // Never act while the player is not in control of himself.
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        sByrnaChargeTimer = 0;
        return;
    }

    gExtEquipBehavior.byrnaActive = 1;

    in = &play->state.input[0];
    bHeld = CHECK_BTN_ALL(in->cur.button, BTN_B) != 0;
    bPress = CHECK_BTN_ALL(in->press.button, BTN_B) != 0;
    rHeld = CHECK_BTN_ALL(in->cur.button, BTN_R) != 0;

    // R+B sends the orb out. Checked before the charge so the two never fight
    // over the same B press.
    if (rHeld && bPress) {
        ByrnaOrb_Launch(play);
        sByrnaChargeTimer = 0;
        return;
    }

    // B held summons. The timer only fires once per hold (it stops climbing at
    // the threshold), so keeping B down does not drain magic every frame.
    if (bHeld && !rHeld) {
        if (sByrnaChargeTimer < BYRNA_CHARGE_FRAMES) {
            sByrnaChargeTimer++;
            if (sByrnaChargeTimer == BYRNA_CHARGE_FRAMES) {
                ByrnaOrb_Summon(play);
            }
        }
    } else {
        sByrnaChargeTimer = 0;
    }
}

static void Byrna_Cleanup(void) {
    // Runs every frame while the slot is NOT equipped, so it has to be cheap and
    // idempotent — both of these are.
    ByrnaOrb_Cleanup();
    sByrnaChargeTimer = 0;
    gExtEquipBehavior.byrnaActive = 0;
}

// Draw is now handled by PostLimbDraw in z_player_lib.c via ExtEquip_DrawSwordDL
// This ensures the cane follows the exact same rotation as the sword during swings

// ---------------------------------------------------------------------------
// Great Fairy's Sword (NEI progressive BGS level 2) — same combat perks as the
// Cane of Byrna, but it IS the player's real Biggoron Sword (no sword-slot hijack).
// Driven by WeaponUpgrade_HasGreatFairy() from ExtEquip_UpdateBehavior, independent
// of the extended-equipment cheat. We only top up swordHealth/bgsFlag (so charge/spin
// always work and a Giant's Knife never "breaks") and recover HP+MP on each melee hit.
// ---------------------------------------------------------------------------
static void GreatFairySword_Behavior(Player* player, PlayState* play) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }
    // Only while actually wielding the Biggoron Sword.
    if (player->heldItemAction != PLAYER_IA_SWORD_BIGGORON) {
        return;
    }
    gSaveContext.bgsFlag = 1;
    if (gSaveContext.swordHealth <= 0.0f) {
        gSaveContext.swordHealth = 8.0f;
    }
}

static void GreatFairySword_OnMeleeHit(Player* player, PlayState* play) {
    GreatFairySword_RecoverOnHit(player, play);
}
