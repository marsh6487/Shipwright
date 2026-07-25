/**
 * equip_breastplate.c - Spirit Tunic (Extended Tunic Slot 2)
 *
 * Behavior: Magic Armor (TP-style) — rupee-cost damage immunity + environment protection.
 * - Immune to all damage while wearing AND holding rupees; each HP of damage costs 1 rupee.
 * - No rupees = slow movement (cursed weight) and NO protection.
 * - Passive rupee drain: 1 rupee per 30 frames.
 * - With rupees ALSO: the FIRE (hot-room) and WATER (underwater breath) survival timers are skipped
 *   — the money-gated environment immunity (gated in z_parameter.c via ExtEquip_SpiritHasMoney).
 *
 * Skijer 2026-07-16 rework: this is now a RECOLOR tunic (no armor overlay). The visual is the tunic
 * env color painted in Player_DrawImpl — ORANGE while active (rupees > 0), BLACK when broke. So the
 * old Iron-Knuckle armor DLs + gold/dark material are GONE.
 *
 * Damage immunity is a direct call from Health_ChangeBy (z_parameter.c) to
 * Breastplate_OnHealthChangeBefore below (invincibilityTimer runs too late in Player_Update).
 * Included by ext_equip_behavior.c (unity build).
 */

// No extra includes — unity-built from ext_equip_behavior.c
extern void Rupees_ChangeBy(s16 rupeeChange);
u8 Breastplate_IsActive(void);

#define BREASTPLATE_RUPEE_INTERVAL 30 // Passive drain: 1 rupee every N frames
#define BREASTPLATE_SLOW_MULT 0.5f    // Speed multiplier when broke

static s16 sBreastplateRupeeTick = 0;

// ---------------------------------------------------------------------------
// Main Behavior — passive rupee drain + broke-mode movement penalty (damage interception is
// separate, in Breastplate_OnHealthChangeBefore; the fire/water timer skip is in z_parameter.c).
// ---------------------------------------------------------------------------
static void Spirit_Behavior(Player* player, PlayState* play) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    if (gSaveContext.rupees > 0) {
        sBreastplateRupeeTick++;
        if (sBreastplateRupeeTick >= BREASTPLATE_RUPEE_INTERVAL) {
            sBreastplateRupeeTick = 0;
            Rupees_ChangeBy(-1);
        }
    } else {
        // No rupees: heavy and slow, no protection
        sBreastplateRupeeTick = 0;
        player->linearVelocity *= BREASTPLATE_SLOW_MULT;
        player->actor.speedXZ *= BREASTPLATE_SLOW_MULT;
    }
}

// ---------------------------------------------------------------------------
// Pre-damage hook — convert incoming damage to rupee cost while Spirit is active and Link has rupees.
// Called from Health_ChangeBy (z_parameter.c) BEFORE health is mutated; *amount = 0 blocks it.
// ---------------------------------------------------------------------------
void Breastplate_OnHealthChangeBefore(int16_t* amount) {
    if (!Breastplate_IsActive()) {
        return;
    }
    if (*amount >= 0) {
        return; // healing — pass through
    }
    if (gSaveContext.rupees <= 0) {
        return; // broke — take damage normally
    }

    s16 damageHP = -*amount;
    s16 rupeeCost = damageHP;
    if (rupeeCost > gSaveContext.rupees) {
        rupeeCost = (s16)gSaveContext.rupees;
    }
    Rupees_ChangeBy(-rupeeCost);
    Sfx_PlaySfxCentered(NA_SE_IT_SHIELD_BOUND);

    *amount = 0; // block the damage
}

u8 Breastplate_IsActive(void) {
    return ExtEquip_IsEnabled() && gExtEquipState.currentExtTunic == 2;
}
