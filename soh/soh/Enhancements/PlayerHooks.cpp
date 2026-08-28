// Skijer's NEI: one handler per player DECISION, not one per mod. Every mod with a say in the same
// vanilla decision is chained here in an explicit order, so z_player.c keeps a single
// GameInteractor_Should line at the site and the priority between mods is readable in one place.
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/transformation_masks/transformation_masks.h"
#include "mods/boss_remains/boss_remains.h"
#include "mods/extended_equipment.h"
#include "mods/transformation_masks/mm_mask_wear.h"

MmPlayerTransformation MmForm_GetCurrentForm(void);
u8 Pacci_UltrahandModeActive(void);
u8 MasterCycle_IsRiding(void);
void MmForm_StartDekuSpinFromOot(Player* player, PlayState* play);
void MmForm_StartGoronCurlFromOot(Player* player, PlayState* play);
u8 GerudoForm_IsActive(void);
}

static bool RollIsOverridden(Player* player, PlayState* play) {
    // Odolwa's remains put a run on A, Goht's a bull charge.
    if (BossRemains_SuppressRoll()) {
        return true;
    }

    // Gerudo: A is hold-to-sprint and tap-to-roll, and OOT fires the roll on the press — so the
    // press is swallowed while the tap/hold detector decides, and the controller calls
    // Player_SetupRoll back itself on a short release.
    if (GerudoMhr_SuppressRoll(player)) {
        return true;
    }

    if (!TransformMasks_IsTransformed()) {
        return false;
    }

    // MM's Deku and Goron have no roll at all: A while moving is a spin (func_80839A84) and a curl
    // (func_80839F98) respectively.
    switch (MmForm_GetCurrentForm()) {
        case MM_PLAYER_FORM_DEKU:
            MmForm_StartDekuSpinFromOot(player, play);
            return true;
        case MM_PLAYER_FORM_GORON:
            MmForm_StartGoronCurlFromOot(player, play);
            return true;
        default:
            return false;
    }
}

// Goron carries the tunic's protection without wearing it, and the Gerudo are native to the desert.
static bool IsHeatImmune() {
    return TransformMasks_HasFireResistance() || GerudoForm_IsActive() ||
           ExtEquip_HasSagesResistance(SAGES_RESIST_FIRE);
}

static bool ShieldSurvivesFire() {
    // Burning an ext shield would delete a Deku Shield the player never had.
    if (ExtEquip_GetCurrent(EQUIP_TYPE_SHIELD) != 0 || TransformMasks_GetShieldMode() != MMFORM_SHIELD_VANILLA) {
        return true;
    }
    if (ExtEquip_HasSagesResistance(SAGES_RESIST_FIRE)) {
        ExtEquip_SagesFlash(SAGES_RESIST_FIRE);
        return true;
    }
    return false;
}

static bool StatusIsResisted(s32 hitResponse) {
    SagesResistance resist;

    switch (hitResponse) {
        case PLAYER_HIT_RESPONSE_FROZEN:
            resist = SAGES_RESIST_ICE;
            break;
        case PLAYER_HIT_RESPONSE_ELECTRIFIED:
            resist = SAGES_RESIST_THUNDER;
            break;
        default:
            return false;
    }

    if (!ExtEquip_HasSagesResistance(resist)) {
        return false;
    }
    ExtEquip_SagesFlash(resist);
    return true;
}

// An empty B slot is how you tell OOT to leave the button alone — it is what keeps the sword in its
// scabbard while a form, a mask or a mount owns the press.
static s32 ResolveItemOnButton(PlayState* play, s32 index, s32 item) {
    if ((index >= 4) && (Pacci_UltrahandModeActive() || MasterCycle_IsRiding())) {
        return ITEM_NONE;
    }
    if (index != 0) {
        return item;
    }
    if (MasterCycle_IsRiding() || MmMaskWear_BlocksSword() ||
        (TransformMasks_IsTransformed() && MmForm_GetCurrentForm() == MM_PLAYER_FORM_DEKU)) {
        return ITEM_NONE;
    }
    // Aim phase only: once the fins are in the air the flag is cleared, so B goes back to punching.
    if (Player_IsZoraBoomerangActive() && (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG)) {
        return ITEM_BOOMERANG;
    }
    if (Player_IsDekuBubbleActive()) {
        return ITEM_SLINGSHOT;
    }
    return item;
}

static void RegisterPlayerHooks() {
    REGISTER_VB_SHOULD(VB_PLAYER_ROLL, {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);
        if (RollIsOverridden(player, play)) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_SUFFER_HEAT, {
        if (IsHeatImmune()) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_CATCH_FIRE, {
        if (ExtEquip_HasSagesResistance(SAGES_RESIST_FIRE)) {
            ExtEquip_SagesFlash(SAGES_RESIST_FIRE);
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_BURN_SHIELD, {
        if (ShieldSurvivesFire()) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_GET_ITEM_ON_BUTTON, {
        s32 index = va_arg(args, s32);
        s32* item = va_arg(args, s32*);
        PlayState* play = va_arg(args, PlayState*);
        *item = ResolveItemOnButton(play, index, *item);
    });

    REGISTER_VB_SHOULD(VB_PLAYER_PUTAWAY_HELD_ITEM, {
        Player* player = va_arg(args, Player*);
        // FD's SHEATH limb is nulled, so his sword would sheathe into nothing; Zora's fin
        // boomerang parks PLAYER_IA_BOOMERANG in heldItemAction, so A unequipped his real sword.
        if (GerudoMhr_OwnsPutaway(player) || Player_IsFDHoldingSword(player) ||
            (TransformMasks_IsTransformed() && MmForm_GetCurrentForm() == MM_PLAYER_FORM_ZORA)) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_TOGGLE_NAVI, {
        if (GerudoMhr_OwnsPutaway(va_arg(args, Player*))) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_SUFFER_STATUS, {
        if (StatusIsResisted(va_arg(args, s32))) {
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAYER_USE_CHILD_HYLIAN_STANCE, {
        // An ext shield only borrows the Hylian slot for its model, and no form is affected by the
        // equipped shield at all.
        if (ExtEquip_GetCurrent(EQUIP_TYPE_SHIELD) != 0 || TransformMasks_GetShieldMode() != MMFORM_SHIELD_VANILLA) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFuncPlayerHooks(RegisterPlayerHooks, {});
