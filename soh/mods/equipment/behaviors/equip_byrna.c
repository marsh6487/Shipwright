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
static void Byrna_OnMeleeHit(Player* player, PlayState* play) {
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
// Per-frame Behavior
// ---------------------------------------------------------------------------
static void Byrna_Behavior(Player* player, PlayState* play) {
    // Skip during cutscenes, dying, loading, etc.
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    // Save original sword state before overriding (only once)
    if (!gExtEquipBehavior.byrnaActive) {
        gExtEquipBehavior.byrnaSavedSwordEquip =
            (gSaveContext.equips.equipment >> gEquipShifts[EQUIP_TYPE_SWORD]) & 0xF;
        gExtEquipBehavior.byrnaSavedButtonItem = gSaveContext.equips.buttonItems[0];
        gExtEquipBehavior.byrnaSavedSwordHealth = gSaveContext.swordHealth;
        gExtEquipBehavior.byrnaSavedBgsFlag = gSaveContext.bgsFlag;
        gExtEquipBehavior.byrnaActive = 1;
    }

    // Only force BGS IA when player is actively holding a sword (not sheathed/NONE, not C-button items)
    if (player->heldItemAction == PLAYER_IA_SWORD_MASTER || player->heldItemAction == PLAYER_IA_SWORD_KOKIRI) {
        player->heldItemAction = PLAYER_IA_SWORD_BIGGORON;
    }

    // Force BGS equipment so the sword system works
    Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_BIGGORON);

    // Keep B button showing BGS item (icon override handled by ExtInv_GetItemIcon)
    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;

    // Force bgsFlag = 1 so the game treats this as true BGS (no durability loss).
    // Without this, if the player owns Giant's Knife (bgsFlag=0), swings would
    // decrement swordHealth and eventually "break" the GK.
    gSaveContext.bgsFlag = 1;

    // Force swordHealth > 0 so spin attack / charge attack works
    // (BGS normally breaks when swordHealth reaches 0)
    if (gSaveContext.swordHealth <= 0.0f) {
        gSaveContext.swordHealth = 8.0f;
    }
}

// Restore original sword state when Byrna is unequipped
static void Byrna_Cleanup(void) {
    if (!gExtEquipBehavior.byrnaActive)
        return;

    // Restore original sword equipment
    Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, gExtEquipBehavior.byrnaSavedSwordEquip);
    gSaveContext.equips.buttonItems[0] = gExtEquipBehavior.byrnaSavedButtonItem;
    gSaveContext.swordHealth = gExtEquipBehavior.byrnaSavedSwordHealth;
    gSaveContext.bgsFlag = gExtEquipBehavior.byrnaSavedBgsFlag;
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
    // Same HP/MP recovery as the Cane of Byrna.
    Byrna_OnMeleeHit(player, play);
}
