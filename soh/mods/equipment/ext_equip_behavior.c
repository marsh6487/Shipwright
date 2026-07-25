/**
 * ext_equip_behavior.c - Behavior handlers for extended equipment
 *
 * Unity build hub: includes individual behavior files and dispatches
 * update/draw/hit callbacks to active equipment.
 *
 * Included by extended_equipment.c (unity build).
 */

// No extra includes — inherits all from extended_equipment.c (unity build root)
// Somaria cane DL header included by extended_equipment.c (unity root)

// ---------------------------------------------------------------------------
// Include behavior implementations
// ---------------------------------------------------------------------------
#include "behaviors/equip_byrna.c"
#include "behaviors/equip_ikaxe.c"
#include "behaviors/equip_pegasus.c"
#include "behaviors/equip_dragonscale.c"
#include "behaviors/equip_ikana.c"
#include "behaviors/equip_magiccape.c"
#include "behaviors/equip_breastplate.c"
#include "behaviors/equip_pendant.c"
#include "behaviors/equip_divine_shield.c"
#include "behaviors/equip_champion.c"
#include "behaviors/equip_snowquill.c"
#include "behaviors/equip_foursword.c"

// ---------------------------------------------------------------------------
// Sword behaviors
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Sword1(Player* player, PlayState* play) {
    Byrna_Behavior(player, play);
}

static void ExtEquip_Behavior_Sword2(Player* player, PlayState* play) {
    FourSword_Behavior(player, play);
}

static void ExtEquip_Behavior_Sword3(Player* player, PlayState* play) {
    // Ext sword slot 3 is unused: the Iron Knuckle's Axe is now the Hammer upgrade,
    // driven from ExtEquip_UpdateBehavior via WeaponUpgrade_HasHammerAxe().
    (void)player;
    (void)play;
}

// ---------------------------------------------------------------------------
// Shield behaviors (stubs)
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Shield1(Player* player, PlayState* play) {
    DivineShield_Behavior(player, play);
}

static void ExtEquip_Behavior_Shield2(Player* player, PlayState* play) {
    (void)player;
    (void)play;
}

static void ExtEquip_Behavior_Shield3(Player* player, PlayState* play) {
    Ikana_Behavior(player, play);
}

// ---------------------------------------------------------------------------
// Tunic behaviors (stubs)
// ---------------------------------------------------------------------------
// Tunic slots remapped (Skijer 2026-07-16): 1 = Champion's Tunic, 2 = Spirit Tunic, 3 = Snowquill.
static void ExtEquip_Behavior_Tunic1(Player* player, PlayState* play) {
    Champion_Behavior(player, play);
}

static void ExtEquip_Behavior_Tunic2(Player* player, PlayState* play) {
    Spirit_Behavior(player, play); // renamed Breastplate — rupee-immunity + fire/water timer skip
}

static void ExtEquip_Behavior_Tunic3(Player* player, PlayState* play) {
    Snowquill_Behavior(player, play); // white recolor + total ice immunity (mostly gate-based)
}

// ---------------------------------------------------------------------------
// Boots behaviors
// ---------------------------------------------------------------------------
static void ExtEquip_Behavior_Boots1(Player* player, PlayState* play) {
    Pegasus_Behavior(player, play);
}

static void ExtEquip_Behavior_Boots2(Player* player, PlayState* play) {
    // Boots slot 2 is FREE (reserved for new boots) — the Pendant of Memories moved to the upgrade
    // column; its moveset runs when its toggle is ON (see ExtEquip_DispatchBehavior).
    (void)player;
    (void)play;
}

static void ExtEquip_Behavior_Boots3(Player* player, PlayState* play) {
    // Boots slot 3 is FREE (reserved for Gravity Boots). The Water Dragon Scale item is gone — its
    // Zora swim now runs unconditionally as the Zora Tunic effect (see ExtEquip_DispatchBehavior).
    (void)player;
    (void)play;
}

// ---------------------------------------------------------------------------
// Behavior dispatch tables
// ---------------------------------------------------------------------------
typedef void (*ExtEquipBehaviorFunc)(Player*, PlayState*);

static const ExtEquipBehaviorFunc sExtSwordBehaviors[3] = {
    ExtEquip_Behavior_Sword1,
    ExtEquip_Behavior_Sword2,
    ExtEquip_Behavior_Sword3,
};

static const ExtEquipBehaviorFunc sExtShieldBehaviors[3] = {
    ExtEquip_Behavior_Shield1,
    ExtEquip_Behavior_Shield2,
    ExtEquip_Behavior_Shield3,
};

static const ExtEquipBehaviorFunc sExtTunicBehaviors[3] = {
    ExtEquip_Behavior_Tunic1,
    ExtEquip_Behavior_Tunic2,
    ExtEquip_Behavior_Tunic3,
};

static const ExtEquipBehaviorFunc sExtBootsBehaviors[3] = {
    ExtEquip_Behavior_Boots1,
    ExtEquip_Behavior_Boots2,
    ExtEquip_Behavior_Boots3,
};

static void ExtEquip_DispatchBehavior(Player* player, PlayState* play) {
    // ZORA TUNIC swim (formerly Water Dragon Scale): runs every frame, self-gated on the worn tunic.
    DragonScale_Behavior(player, play);

    // Upgrade-column passives (Skijer 2026-07-15) — decoupled from the ext slots:
    // Magic Cape: the HALF-COST passive lives at the magic-cost sites (Magic_RequestChange /
    // ItemMagic_* / MAGIC_REQ) and is always active once owned; the cloth physics only run
    // while the cape is set visible (kaleido upgrade-cell toggle).
    if (ExtEquip_CapeVisible()) {
        MagicCape_Behavior(player, play);
    }
    // Pendant of Memories: its moveset runs while its effect toggle is ON.
    if (ExtEquip_PendantActive()) {
        Pendant_Behavior(player, play);
    }

    // Always run cleanup for behaviors that need it (cape boost removal, etc.)
    MagicCape_Cleanup();

    // Byrna cleanup: restore original sword when Byrna is no longer active
    if (gExtEquipState.currentExtSword != 1) {
        Byrna_Cleanup();
    }
    // Pegasus cleanup: disable collider when Pegasus boots are no longer active
    if (gExtEquipState.currentExtBoots != 1) {
        Pegasus_Cleanup();
    }
    // Four Sword cleanup: clear forced equipment when sword slot 2 is no longer active
    if (gExtEquipState.currentExtSword != 2) {
        FourSword_Cleanup();
    }
    // Champion's Tunic cleanup: clear screen tint when the Champion slot (now 1) is lost
    if (gExtEquipState.currentExtTunic != 1) {
        Champion_Cleanup(play);
    }

    if (gExtEquipState.currentExtSword > 0 && gExtEquipState.currentExtSword <= 3) {
        sExtSwordBehaviors[gExtEquipState.currentExtSword - 1](player, play);
    }
    if (gExtEquipState.currentExtShield > 0 && gExtEquipState.currentExtShield <= 3) {
        sExtShieldBehaviors[gExtEquipState.currentExtShield - 1](player, play);
    }
    if (gExtEquipState.currentExtTunic > 0 && gExtEquipState.currentExtTunic <= 3) {
        sExtTunicBehaviors[gExtEquipState.currentExtTunic - 1](player, play);
    }
    if (gExtEquipState.currentExtBoots > 0 && gExtEquipState.currentExtBoots <= 3) {
        sExtBootsBehaviors[gExtEquipState.currentExtBoots - 1](player, play);
    }
}

// ---------------------------------------------------------------------------
// Melee hit dispatch (called from z_player.c)
// ---------------------------------------------------------------------------
static void ExtEquip_OnMeleeHitDispatch(Player* player, PlayState* play) {
    // Cane of Byrna: MP recovery on sword hit
    if (gExtEquipState.currentExtSword == 1) {
        Byrna_OnMeleeHit(player, play);
    }
    // Champion's Tunic: count hits during Flurry Rush window (slot 1 now)
    if (gExtEquipState.currentExtTunic == 1) {
        Champion_OnMeleeHit(player, play);
    }
}

// ---------------------------------------------------------------------------
// Draw dispatch (called from z_player.c draw section)
// ---------------------------------------------------------------------------
static void ExtEquip_DrawDispatch(Player* player, PlayState* play) {
    // Cane of Byrna: drawn from PostLimbDraw via ExtEquip_DrawSwordDL (follows limb matrix)
    // Pegasus Anklet: wind barrier
    if (gExtEquipState.currentExtBoots == 1) {
        Pegasus_Draw(player, play);
    }
    // Zora Tunic swim: blue water-entry sparkles (self-timed; triggered on swim entry)
    DScale_Draw(player, play);
    // Magic Cape: Ganondorf cloth physics cape — draws whenever owned and set visible (the effect
    // itself is passive; this is only the look)
    if (ExtEquip_CapeVisible()) {
        MagicCape_Draw(player, play);
    }
    // Four Sword: ghost clone Links
    if (gExtEquipState.currentExtSword == 2) {
        FourSword_Draw(player, play);
    }
}
