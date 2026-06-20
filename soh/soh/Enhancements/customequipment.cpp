#include <initializer_list>
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_custom_equip/object_custom_equip.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/vanilla-behavior/PlayerAnimOverride.h"
#include "soh/ShipInit.hpp"
#include "soh/ResourceManagerHelpers.h"
// The VB_PLAYER_DRAW_BEGIN handler below uses OPEN_DISPS/CLOSE_DISPS, whose macro
// bodies (macros.h) block-scope-declare + call FrameInterpolation_RecordOpenChild/
// CloseChild. This header provides their namespace-scope extern "C" decls so the
// macro's redeclaration inherits C linkage (otherwise C++ mangles them -> LNK2001).
#include "soh/frame_interpolation.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/transformation_masks/transformation_masks.h"
// NEI player-draw subsystems used by the VB_PLAYER_DRAW_BEGIN handler below
// (these own the custom-model draws that replace/precede the vanilla Link draw).
#include "mods/pak_loader/pak_loader.h"     // PakLoader_FrameBegin
#include "expansions/sm64/sm64_mario.h"     // Sm64Mario_HasMesh/Draw/ShouldHideLink
#include "mods/items/custom_items.h"        // CustomItems_OverrideDraw
#include "mods/extended_equipment.h"        // ExtEquip_DrawBehavior
extern SaveContext gSaveContext;

// Harpoon Prop Hunt local-prop draw intercept. Forward-declared (matching the
// inline `extern` z_player.c previously used) to avoid pulling the Harpoon C++
// headers into this TU. Returns 1 when it rendered a prop (suppress Link), else 0.
s32 HarpoonPropHunt_TryDrawLocalProp(Actor* thisx, PlayState* play);
}

static const char* ResolveCustomChain(std::initializer_list<const char*> paths) {
    const char* fallback = nullptr;
    for (auto path : paths) {
        if (path == nullptr)
            continue;
        fallback = path;
        if (ResourceGetIsCustomByName(path) || ResourceMgr_FileAltExists(path))
            return path;
    }
    return fallback;
}

static Gfx* LoadGfxByName(const char* path) {
    return path ? ResourceMgr_LoadGfxByName(path) : nullptr;
}

static Gfx* LoadCustomGfx(const char* path) {
    if (!path)
        return nullptr;
    if (!ResourceGetIsCustomByName(path) && !ResourceMgr_FileAltExists(path))
        return nullptr;
    return ResourceMgr_LoadGfxByName(path);
}

static u8 sLastValidSwordEquip = EQUIP_VALUE_SWORD_NONE;

static u8 GetEquippedSwordValue(PlayState* play) {
    const u8 sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const bool inCutscene = (play->csCtx.state != CS_STATE_IDLE);
    if (!(inCutscene && gSaveContext.linkAge == LINK_AGE_CHILD && sword == EQUIP_VALUE_SWORD_MASTER)) {
        sLastValidSwordEquip = sword;
    }
    return sLastValidSwordEquip;
}

static const char* GetSwordInSheathDL(PlayState* play) {
    switch (GetEquippedSwordValue(play)) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return gCustomKokiriSwordInSheathDL;
        case EQUIP_VALUE_SWORD_MASTER:
            return gCustomMasterSwordInSheathDL;
        case EQUIP_VALUE_SWORD_BIGGORON:
            if (gSaveContext.bgsFlag)
                return gCustomLongswordInSheathDL;
            if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                return gCustomBrokenLongswordInSheathDL;
            return ResolveCustomChain({ gCustomBreakableLongswordInSheathDL, gCustomLongswordInSheathDL, nullptr });
    }
    return nullptr;
}

static const char* GetSheathOnlyDL(PlayState* play) {
    switch (GetEquippedSwordValue(play)) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return gCustomKokiriSwordSheathDL;
        case EQUIP_VALUE_SWORD_MASTER:
            return gCustomMasterSwordSheathDL;
        case EQUIP_VALUE_SWORD_BIGGORON:
            if (gSaveContext.bgsFlag)
                return gCustomLongswordSheathDL;
            if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE))
                return gCustomBrokenLongswordSheathDL;
            return ResolveCustomChain({ gCustomBreakableLongswordSheathDL, gCustomLongswordSheathDL, nullptr });
    }
    return nullptr;
}

static const char* GetShieldOnBackDL(s32 shield) {
    const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
    switch (shield) {
        case PLAYER_SHIELD_DEKU:
            return gCustomDekuShieldOnBackDL;
        case PLAYER_SHIELD_HYLIAN:
            return isAdult ? gCustomHylianShieldOnBackDL : gCustomHylianShieldOnChildBackDL;
        case PLAYER_SHIELD_MIRROR:
            return gCustomMirrorShieldOnBackDL;
    }
    return nullptr;
}

// Hand/held shield model DL for the given shield value (Deku/Hylian/Mirror).
static const char* GetCustomShieldDL(s32 shield) {
    switch (shield) {
        case PLAYER_SHIELD_DEKU:
            return gCustomDekuShieldDL;
        case PLAYER_SHIELD_HYLIAN:
            return gCustomHylianShieldDL;
        case PLAYER_SHIELD_MIRROR:
            return gCustomMirrorShieldDL;
    }
    return nullptr;
}

// Allocates a small gfx buffer, emits up to two display lists (skipping null
// ones), terminates it, and stores it in *dList. Callers guard with
// "if (a || b)" so at least one is non-null. Mirrors the open-coded
// Graph_Alloc + gSPDisplayList + gSPEndDisplayList sequence used throughout.
static void EmitDLBuffer(PlayState* play, Gfx** dList, Gfx* a, Gfx* b) {
    Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
    Gfx* p = buf;
    if (a)
        gSPDisplayList(p++, a);
    if (b)
        gSPDisplayList(p++, b);
    gSPEndDisplayList(p);
    *dList = buf;
}

static const char* GetSwordInSheathDLForPlayer(Player* player, PlayState* play) {
    if (player == GET_PLAYER(play))
        return GetSwordInSheathDL(play);
    switch (player->heldItemId) {
        case ITEM_SWORD_KOKIRI:
            return gCustomKokiriSwordInSheathDL;
        case ITEM_SWORD_MASTER:
            return gCustomMasterSwordInSheathDL;
        case ITEM_SWORD_BGS:
            return gCustomLongswordInSheathDL;
        case ITEM_SWORD_KNIFE:
            return gCustomBrokenLongswordInSheathDL;
    }
    return nullptr;
}

static const char* GetSheathOnlyDLForPlayer(Player* player, PlayState* play) {
    if (player == GET_PLAYER(play))
        return GetSheathOnlyDL(play);
    switch (player->heldItemId) {
        case ITEM_SWORD_KOKIRI:
            return gCustomKokiriSwordSheathDL;
        case ITEM_SWORD_MASTER:
            return gCustomMasterSwordSheathDL;
        case ITEM_SWORD_BGS:
            return gCustomLongswordSheathDL;
        case ITEM_SWORD_KNIFE:
            return gCustomBrokenLongswordSheathDL;
    }
    return nullptr;
}

static u8 PauseGetLimbType(s32 limbIndex) {
    const u8 swordEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const u8 shieldEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    const bool isBGS = (swordEquip == EQUIP_VALUE_SWORD_BIGGORON);
    const bool isSword = (swordEquip != EQUIP_VALUE_SWORD_NONE);
    const bool childHylian = (!LINK_IS_ADULT && shieldEquip == PLAYER_SHIELD_HYLIAN);

    switch (limbIndex) {
        case PLAYER_LIMB_L_HAND:
            if (!isSword)
                return PLAYER_MODELTYPE_LH_OPEN;
            return isBGS ? PLAYER_MODELTYPE_LH_BGS : PLAYER_MODELTYPE_LH_SWORD;
        case PLAYER_LIMB_R_HAND:
            return (isBGS || childHylian) ? PLAYER_MODELTYPE_RH_CLOSED : PLAYER_MODELTYPE_RH_SHIELD;
        case PLAYER_LIMB_SHEATH:
            if (isBGS || childHylian)
                return PLAYER_MODELTYPE_SHEATH_19;
            return PLAYER_MODELTYPE_SHEATH_17;
    }
    return 0;
}

// Counter-scale hand mesh when EquipmentAlwaysVisible + ScaleAdultEquipmentAsChild is active on child Link.
static constexpr float HAND_COUNTER_SCALE_Y_OFFSET = 100.0f;

static void BuildHandItemDL(PlayState* play, Gfx** dList, Gfx* hand, Gfx* item, bool counterScaleHand) {
    if (counterScaleHand) {
        Mtx* scaleMtx = (Mtx*)Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
        MtxF mf = {};
        mf.xx = mf.yy = mf.zz = 1.25f;
        mf.ww = 1.0f;
        mf.yw = HAND_COUNTER_SCALE_Y_OFFSET;
        Matrix_MtxFToMtx(&mf, scaleMtx);
        Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 5 * sizeof(Gfx));
        Gfx* p = buf;
        gSPMatrix(p++, scaleMtx, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(p++, hand);
        gSPPopMatrix(p++, G_MTX_MODELVIEW);
        gSPDisplayList(p++, item);
        gSPEndDisplayList(p);
        *dList = buf;
    } else {
        Gfx* buf = (Gfx*)Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));
        Gfx* p = buf;
        gSPDisplayList(p++, hand);
        gSPDisplayList(p++, item);
        gSPEndDisplayList(p);
        *dList = buf;
    }
}

static bool IsScalingAdultItemAsChild() {
    return CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
           CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && !LINK_IS_ADULT;
}

static void RegisterCustomEquipment() {
    // World (gameplay) character
    COND_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        Player* player = (Player*)va_arg(args, void*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: while transformed (MM mask form) the form draws its own skeleton,
        // so Link's custom-equipment limb override must not run.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }

        const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
        const char* customDL = nullptr;

        switch (limbIndex) {
            case PLAYER_LIMB_L_HAND: {
                const bool isOcarina =
                    player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                    player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->itemAction == PLAYER_IA_OCARINA_FAIRY || player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->modelGroup == PLAYER_MODELGROUP_OCARINA || player->modelGroup == PLAYER_MODELGROUP_OOT;
                if (isOcarina) {
                    Gfx* resolvedHand = LoadGfxByName(isAdult ? gLinkAdultLeftHandNearDL : gLinkChildLeftHandNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, nullptr);
                    }
                    break;
                }
                switch ((u8)player->leftHandType) {
                    case PLAYER_MODELTYPE_LH_SWORD: {
                        if (player == GET_PLAYER(play)) {
                            if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI)
                                customDL = gCustomKokiriSwordDL;
                            else if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER)
                                customDL = gCustomMasterSwordDL;
                        } else {
                            if (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI)
                                customDL = gCustomKokiriSwordDL;
                            else if (player->heldItemAction == PLAYER_IA_SWORD_MASTER)
                                customDL = gCustomMasterSwordDL;
                        }
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_BGS: {
                        const bool isBrokenKnife =
                            (player == GET_PLAYER(play))
                                ? CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)
                                : (player->heldItemId == ITEM_SWORD_KNIFE);
                        if (isBrokenKnife) {
                            customDL = gCustomBrokenLongswordDL;
                        } else if (player == GET_PLAYER(play)) {
                            if (gSaveContext.bgsFlag)
                                customDL = gCustomLongswordDL;
                            else
                                customDL =
                                    ResolveCustomChain({ gCustomBreakableLongswordDL, gCustomLongswordDL, nullptr });
                        } else {
                            customDL = gCustomLongswordDL;
                        }
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_HAMMER:
                        customDL = gCustomHammerDL;
                        break;
                    case PLAYER_MODELTYPE_LH_BOOMERANG:
                        if (!(player->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN))
                            customDL = gCustomBoomerangDL;
                        break;
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
                    if (resolvedHand) {
                        const u8 lht = (u8)player->leftHandType;
                        const bool scaleHand = IsScalingAdultItemAsChild() &&
                                               ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                                                 lht == PLAYER_MODELTYPE_LH_SWORD) ||
                                                lht == PLAYER_MODELTYPE_LH_BGS || lht == PLAYER_MODELTYPE_LH_HAMMER);
                        BuildHandItemDL(play, dList, resolvedHand, resolvedCustom, scaleHand);
                    }
                }
                break;
            }

            case PLAYER_LIMB_R_HAND: {
                if (player->unk_6AD == 2) {
                    const char* fpsHand = nullptr;
                    const char* fpsWeapon = nullptr;

                    switch (player->rightHandType) {
                        case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT:
                        case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2:
                            fpsHand = isAdult ? gCustomAdultFPSHandDL : gCustomChildFPSHandDL;
                            fpsWeapon =
                                Player_HoldsBow(player)
                                    ? ResolveCustomChain({ gCustomFPSBowDL, gCustomBowDL, nullptr })
                                    : ResolveCustomChain({ gCustomFPSSlingshotDL, gCustomSlingshotDL, nullptr });
                            break;
                        case PLAYER_MODELTYPE_RH_HOOKSHOT:
                            fpsHand = isAdult ? gCustomAdultFPSHandDL : gCustomChildFPSHandDL;
                            fpsWeapon = (player->heldItemAction == PLAYER_IA_HOOKSHOT)
                                            ? ResolveCustomChain({ gCustomFPSHookshotDL, gCustomHookshotDL, nullptr })
                                            : ResolveCustomChain({ gCustomFPSLongshotDL, gCustomLongshotDL, nullptr });
                            break;
                    }

                    Gfx* resolvedFpsWeapon = LoadCustomGfx(fpsWeapon);
                    Gfx* resolvedFpsHand = LoadCustomGfx(fpsHand);
                    if (resolvedFpsWeapon || resolvedFpsHand) {
                        EmitDLBuffer(play, dList, resolvedFpsWeapon, resolvedFpsHand);
                    }
                } else {
                    bool useOpenHand = false;
                    const bool holdsTwoHanded = player->heldItemAction >= PLAYER_IA_SWORD_BIGGORON &&
                                                player->heldItemAction <= PLAYER_IA_HAMMER;
                    const bool isChildHylian = !isAdult && player->currentShield == PLAYER_SHIELD_HYLIAN;
                    const bool isShielding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) != 0 && !isChildHylian &&
                                             (!holdsTwoHanded || (CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) &&
                                                                  player->heldItemAction != PLAYER_IA_DEKU_STICK));
                    const bool isOcarina = player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                                           player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->itemAction == PLAYER_IA_OCARINA_FAIRY ||
                                           player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->modelGroup == PLAYER_MODELGROUP_OCARINA ||
                                           player->modelGroup == PLAYER_MODELGROUP_OOT;
                    if (isShielding) {
                        customDL = GetCustomShieldDL(player->currentShield);
                    } else if (isOcarina) {
                        const bool isOoT = player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                                           player->modelGroup == PLAYER_MODELGROUP_OOT;
                        customDL = isOoT ? (isAdult ? gCustomOcarinaOfTimeAdultDL : gCustomOcarinaOfTimeDL)
                                         : (isAdult ? gCustomFairyOcarinaAdultDL : gCustomFairyOcarinaDL);
                        useOpenHand = true;
                    } else {
                        switch ((u8)player->rightHandType) {
                            case PLAYER_MODELTYPE_RH_SHIELD:
                                customDL = GetCustomShieldDL(player->currentShield);
                                break;
                            case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT:
                            case PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2:
                                customDL = Player_HoldsBow(player) ? gCustomBowDL : gCustomSlingshotDL;
                                break;
                            case PLAYER_MODELTYPE_RH_HOOKSHOT:
                                customDL = (player->heldItemAction == PLAYER_IA_HOOKSHOT) ? gCustomHookshotDL
                                                                                          : gCustomLongshotDL;
                                break;
                            case PLAYER_MODELTYPE_RH_OCARINA:
                                customDL = isAdult ? gCustomFairyOcarinaAdultDL : gCustomFairyOcarinaDL;
                                useOpenHand = true;
                                break;
                            case PLAYER_MODELTYPE_RH_OOT:
                                customDL = isAdult ? gCustomOcarinaOfTimeAdultDL : gCustomOcarinaOfTimeDL;
                                useOpenHand = true;
                                break;
                        }
                    }
                    Gfx* resolvedCustom = LoadCustomGfx(customDL);
                    if (resolvedCustom) {
                        const char* handPath =
                            useOpenHand ? (isAdult ? gLinkAdultRightHandNearDL : gLinkChildRightHandNearDL)
                                        : (isAdult ? gLinkAdultRightHandClosedNearDL : gLinkChildRightHandClosedNearDL);
                        Gfx* resolvedHand = LoadGfxByName(handPath);
                        if (resolvedHand) {
                            const u8 rht = (u8)player->rightHandType;
                            const bool scaleHand =
                                IsScalingAdultItemAsChild() && !isOcarina &&
                                ((player->currentShield == PLAYER_SHIELD_MIRROR &&
                                  (isShielding || rht == PLAYER_MODELTYPE_RH_SHIELD)) ||
                                 (!isShielding &&
                                  (rht == PLAYER_MODELTYPE_RH_HOOKSHOT ||
                                   (rht == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(player)))));
                            BuildHandItemDL(play, dList, resolvedHand, resolvedCustom, scaleHand);
                        }
                    }
                }
                break;
            }

            case PLAYER_LIMB_SHEATH: {
                u8 sheathType = (u8)player->sheathType;
                const bool isOcarinaSheath =
                    player->heldItemAction == PLAYER_IA_OCARINA_FAIRY ||
                    player->heldItemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->itemAction == PLAYER_IA_OCARINA_FAIRY || player->itemAction == PLAYER_IA_OCARINA_OF_TIME ||
                    player->modelGroup == PLAYER_MODELGROUP_OCARINA || player->modelGroup == PLAYER_MODELGROUP_OOT;
                if (isOcarinaSheath) {
                    const bool hasShieldEquipped = player->currentShield != PLAYER_SHIELD_NONE;
                    sheathType = hasShieldEquipped ? PLAYER_MODELTYPE_SHEATH_18 : PLAYER_MODELTYPE_SHEATH_16;
                } else if (player->stateFlags1 & PLAYER_STATE1_SHIELDING) {
                    const bool sheathTwoHanded = player->heldItemAction >= PLAYER_IA_SWORD_BIGGORON &&
                                                 player->heldItemAction <= PLAYER_IA_HAMMER;
                    const bool sheathChildHylian = !isAdult && player->currentShield == PLAYER_SHIELD_HYLIAN;
                    const bool sheathCanShield =
                        !sheathChildHylian && (!sheathTwoHanded || (CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) &&
                                                                    player->heldItemAction != PLAYER_IA_DEKU_STICK));
                    if (sheathCanShield) {
                        if (sheathType == PLAYER_MODELTYPE_SHEATH_18)
                            sheathType = PLAYER_MODELTYPE_SHEATH_16;
                        else if (sheathType == PLAYER_MODELTYPE_SHEATH_19)
                            sheathType = PLAYER_MODELTYPE_SHEATH_17;
                    }
                }
                const bool hasSword =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_16 || sheathType == PLAYER_MODELTYPE_SHEATH_18);
                const bool hasShield =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_18 || sheathType == PLAYER_MODELTYPE_SHEATH_19);
                const bool emptySheath =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_17 || sheathType == PLAYER_MODELTYPE_SHEATH_19);

                const char* swordPath = hasSword      ? GetSwordInSheathDLForPlayer(player, play)
                                        : emptySheath ? GetSheathOnlyDLForPlayer(player, play)
                                                      : nullptr;
                const char* shieldPath = hasShield ? GetShieldOnBackDL(player->currentShield) : nullptr;

                Gfx* resolvedSword = LoadCustomGfx(swordPath);
                Gfx* resolvedShield = LoadCustomGfx(shieldPath);
                if (resolvedSword || resolvedShield) {
                    EmitDLBuffer(play, dList, resolvedSword, resolvedShield);
                }
                break;
            }

            default:
                break;
        }
    });

    // Pause/equipment screen character
    COND_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW_PAUSE, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        Player* player = (Player*)va_arg(args, void*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom-equipment limb override while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }

        const bool isAdult = gSaveContext.linkAge == LINK_AGE_ADULT;
        const char* customDL = nullptr;

        switch (limbIndex) {
            case PLAYER_LIMB_L_HAND: {
                switch (PauseGetLimbType(PLAYER_LIMB_L_HAND)) {
                    case PLAYER_MODELTYPE_LH_SWORD: {
                        const u8 swordEquip = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
                        if (swordEquip == EQUIP_VALUE_SWORD_KOKIRI)
                            customDL = gCustomKokiriSwordDL;
                        else
                            customDL = gCustomMasterSwordDL;
                        break;
                    }
                    case PLAYER_MODELTYPE_LH_BGS: {
                        if (gSaveContext.bgsFlag) {
                            customDL = gCustomLongswordDL;
                        } else if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                            customDL = gCustomBrokenLongswordDL;
                        } else {
                            customDL = ResolveCustomChain({ gCustomBreakableLongswordDL, gCustomLongswordDL, nullptr });
                        }
                        break;
                    }
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, resolvedCustom);
                    }
                }
                break;
            }

            case PLAYER_LIMB_R_HAND: {
                if (PauseGetLimbType(PLAYER_LIMB_R_HAND) == PLAYER_MODELTYPE_RH_SHIELD) {
                    customDL = GetCustomShieldDL(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
                }
                Gfx* resolvedCustom = LoadCustomGfx(customDL);
                if (resolvedCustom) {
                    Gfx* resolvedHand =
                        LoadGfxByName(isAdult ? gLinkAdultRightHandClosedNearDL : gLinkChildRightHandClosedNearDL);
                    if (resolvedHand) {
                        EmitDLBuffer(play, dList, resolvedHand, resolvedCustom);
                    }
                }
                break;
            }

            case PLAYER_LIMB_SHEATH: {
                const u8 sheathType = PauseGetLimbType(PLAYER_LIMB_SHEATH);
                const bool hasSword =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_16 || sheathType == PLAYER_MODELTYPE_SHEATH_18);
                const bool hasShield =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_18 || sheathType == PLAYER_MODELTYPE_SHEATH_19);
                const bool emptySheath =
                    (sheathType == PLAYER_MODELTYPE_SHEATH_17 || sheathType == PLAYER_MODELTYPE_SHEATH_19);

                const char* swordPath = hasSword      ? GetSwordInSheathDL(play)
                                        : emptySheath ? GetSheathOnlyDL(play)
                                                      : nullptr;
                const char* shieldPath = hasShield ? GetShieldOnBackDL(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) : nullptr;

                Gfx* resolvedSword = LoadCustomGfx(swordPath);
                Gfx* resolvedShield = LoadCustomGfx(shieldPath);
                if (resolvedSword || resolvedShield) {
                    EmitDLBuffer(play, dList, resolvedSword, resolvedShield);
                }
                break;
            }

            default:
                break;
        }
    });

    COND_VB_SHOULD(VB_DRAW_HOOKSHOT_TIP, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom hookshot tip DL while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }
        const char* tipPath = (player->heldItemAction == PLAYER_IA_LONGSHOT)
                                  ? ResolveCustomChain({ gCustomLongshotTipDL, gCustomHookshotTipDL, nullptr })
                                  : gCustomHookshotTipDL;
        Gfx* resolvedTip = LoadCustomGfx(tipPath);
        if (resolvedTip) {
            *should = false;
            gSPDisplayList(play->state.gfxCtx->polyOpa.p++, resolvedTip);
        }
    });

    COND_VB_SHOULD(VB_DRAW_HOOKSHOT_CHAIN, CVarGetInteger(CVAR_SETTING("AltAssets"), 1), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);

        // NEI: skip custom hookshot chain DL while transformed.
        if (TransformMasks_IsTransformedAny()) {
            va_end(args);
            return;
        }
        const char* chainPath = (player->heldItemAction == PLAYER_IA_LONGSHOT)
                                    ? ResolveCustomChain({ gCustomLongshotChainDL, gCustomHookshotChainDL, nullptr })
                                    : gCustomHookshotChainDL;
        Gfx* resolvedChain = LoadCustomGfx(chainPath);
        if (resolvedChain) {
            *should = false;
            gSPDisplayList(play->state.gfxCtx->polyOpa.p++, resolvedChain);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterCustomEquipment, { CVAR_SETTING("AltAssets") });

// ----------------------------------------------------------------------------
// NEI player-draw fork (VB_PLAYER_DRAW_BEGIN)
//
// Moved verbatim out of the top of `Player_Draw` (z_player.c). Fires as the
// very first statement of `Player_Draw`, before any vanilla draw setup. Returns
// `false` (sets *should) to suppress the entire vanilla `Player_Draw` body when
// a custom model is rendered (SM64 Mario / Harpoon prop / fully-loaded MM form);
// returns `true` (default) to fall through into the vanilla draw for additive
// side-effects (Dragon Scale swim barrier, MM pre-flash overlay, MM FP aim).
//
// NOTE: the Pak/O2r skeleton swap + restore and the post-draw additive draws
// (CustomItems/ExtEquip/SSBB/GetItem/Garo body) remain INLINE in z_player.c —
// they thread `pakActive`/`o2rActive` function-locals across the vanilla draw
// and so cannot be relocated to a VB with provably-identical behavior.
//
// Registered unconditionally: each block self-guards (Sm64Mario_HasMesh(),
// TransformMasks_*(), etc.), so it must NOT be gated on the AltAssets CVar.
static void RegisterPlayerDrawForkNEI() {
    REGISTER_VB_SHOULD(VB_PLAYER_DRAW_BEGIN, {
        PlayState* play = va_arg(args, PlayState*);
        Player* player = va_arg(args, Player*);
        u8 isLocalPlayer = (player == GET_PLAYER(play));

        // PAK Loader: free previous frame's GbiWrap combined DLs (once per frame, main player only)
        if (isLocalPlayer) {
            PakLoader_FrameBegin();
        }

        // Harpoon Prop Hunt — direct prop-draw intercept. Only fires for the LOCAL
        // player; remote dummies are handled separately in HarpoonDummyPlayer. The
        // shim internally checks isPropHuntMode + IsLocalHiderWithProp + AreGhostsReady
        // and only returns 1 when it actually rendered a prop; 0 falls through to
        // vanilla Link draw.
        if (isLocalPlayer) {
            if (HarpoonPropHunt_TryDrawLocalProp(&player->actor, play)) {
                *should = false;
                va_end(args);
                return;
            }
        }

        // SM64 MARIO: Draw Mario mesh instead of Link. Uses HasMesh (stricter than
        // IsReady) so Link falls back to normal draw during the brief window between
        // mario_create success and the first successful tick.
        if (isLocalPlayer) {
            // Mario has a current mesh → draw Mario instead of Link.
            if (Sm64Mario_HasMesh()) {
                Sm64Mario_Draw(play, player);
                *should = false;
                va_end(args);
                return;
            }
            // CVAR on but Mario isn't drawable right now (between Reset and Init
            // during detransform, or while Lens of Truth is held) — skip Link's draw
            // entirely so the player doesn't see Link briefly pop in.
            if (Sm64Mario_ShouldHideLink()) {
                *should = false;
                va_end(args);
                return;
            }
        }

        // Transformation Masks: If transformed, draw MM form instead of OOT Link.
        // Dragon Scale swim: draw barrier, then fall through to OOT Link draw.
        if (isLocalPlayer && TransformMasks_IsZoraSwimEnabled()) {
            TransformMasks_Draw(play, player); // Draws barrier only (state=INACTIVE + zoraSwimEnabled)
        }

        if (isLocalPlayer && (TransformMasks_IsTransformed() || TransformMasks_IsFDSkinMode())) {
            if (TransformMasks_HasSkeleton()) {
                // Always draw transformed forms (no invincibility blink — use color flash instead)
                {
                    TransformMasks_Draw(play, player);

                    // Update hookshot anchor position (unk_3C8) since Player_PostLimbDrawGameplay
                    // won't run. Arms_Hook uses this to calculate distance for pull termination.
                    // Without this, hookshot pull never ends because unk_3C8 stays stale.
                    if ((player->heldItemAction == PLAYER_IA_HOOKSHOT) ||
                        (player->heldItemAction == PLAYER_IA_LONGSHOT)) {
                        player->unk_3C8.x = player->actor.world.pos.x;
                        player->unk_3C8.y = player->actor.world.pos.y + 40.0f; // Approximate hand height
                        player->unk_3C8.z = player->actor.world.pos.z;
                    }

                    // Still draw get-item animations and custom items on MM forms.
                    OPEN_DISPS(play->state.gfxCtx);
                    if (!(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
                        if (player->unk_862 > 0) {
                            Player_DrawGetItem(play, player);
                        }
                        CustomItems_OverrideDraw(player, play);
                        ExtEquip_DrawBehavior(player, play);
                    }
                    CLOSE_DISPS(play->state.gfxCtx);
                    *should = false;
                    va_end(args);
                    return;
                }
                // First-person aim (unk_6AD != 0): fall through to OOT draw but all limbs
                // will be hidden (see transform check at overrideLimbDraw selection below).
                // Skeleton still processes for body part positions (hookshot chain, arrow spawn).
            } else {
                // Skeleton not loaded: draw flash overlay only, then fall through to OOT Link draw
                TransformMasks_Draw(play, player);
            }
        }
    });
}

static RegisterShipInitFunc initFuncPlayerDrawFork(RegisterPlayerDrawForkNEI, {});

// ----------------------------------------------------------------------------
// NEI player animation-override fork (VB_PLAYER_ANIM_OVERRIDE)
//
// Each block below was moved VERBATIM out of an inline anim-override site in
// `z_player.c`. At every converted site the vanilla code first stores the stock
// animation in a local, then fires this positioned VB at the exact point the
// animation is about to be played, passing `&anim` so a handler can replace it.
// The vanilla code then plays whatever `*animOut` points to.
//
// Behavior equivalence: with this handler unregistered, GameInteractor_Should
// returns its default (true) and never touches *animOut, so the vanilla
// animation plays unchanged. With it registered, *animOut is overwritten only
// when the same getter the inline code called returns a non-NULL replacement,
// under the same TransformMasks_IsTransformed()/animation-range gating — so the
// played animation is identical in every case to the original inline code.
//
// The MM form getters return NULL unless an MM transformation form is active;
// the MHR getters return NULL unless the user bound a moveset animation. Hence
// this is registered unconditionally (each block self-guards), like the draw
// fork above.
//
// Sites intentionally LEFT INLINE in z_player.c (NOT converted) because they do
// not have the uniform "compute the final anim, then play once" shape:
//   - func_808358F0 (boomerang throw): plays the vanilla throw anim first, then
//     RE-plays an override on top of it (double LinkAnimation_PlayOnce).
//   - func_80831F00 melee start: plays the vanilla swing first, then RE-plays a
//     jump-slash override, then RE-plays an MHR override (two stacked re-plays).
// Relocating those to this single-play VB would change the transformed-path side
// effects (a single play instead of the original double play), so they stay put.
extern "C" {
LinkAnimationHeader* MmForm_GetJumpSlashAnim(s32 phase);
LinkAnimationHeader* MmForm_GetZoraBoomerangAnim(s32 phase);
LinkAnimationHeader* MhrMoveset_GetMoveAnim(s32 moveId);
LinkAnimationHeader* MhrMoveset_GetShieldAnim(s32 loopPhase);
}

static void RegisterPlayerAnimOverrideNEI() {
    REGISTER_VB_SHOULD(VB_PLAYER_ANIM_OVERRIDE, {
        s32 siteId = va_arg(args, s32);
        s32 siteArg = va_arg(args, s32);
        LinkAnimationHeader** animOut = va_arg(args, LinkAnimationHeader**);
        Player* player = va_arg(args, Player*);

        switch (siteId) {
            case VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_WAIT: {
                // z_player.c func_80835884: phase 0, gated on IsTransformed (NULL otherwise).
                LinkAnimationHeader* formAnim =
                    TransformMasks_IsTransformed() ? MmForm_GetZoraBoomerangAnim(0) : nullptr;
                if (formAnim != nullptr) {
                    *animOut = formAnim;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_CATCH: {
                // z_player.c func_80835B60: phase 2, gated on IsTransformed (NULL otherwise).
                LinkAnimationHeader* zoraCatch =
                    TransformMasks_IsTransformed() ? MmForm_GetZoraBoomerangAnim(2) : nullptr;
                if (zoraCatch != nullptr) {
                    *animOut = zoraCatch;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_ROLL: {
                // z_player.c Player_SetupRoll: MHR moveset roll binding (moveId 4 = MHR_MOVE_ROLL).
                LinkAnimationHeader* rollAnim = MhrMoveset_GetMoveAnim(4 /* MHR_MOVE_ROLL */);
                if (rollAnim != nullptr) {
                    *animOut = rollAnim;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_DODGE_HOP: {
                // z_player.c func_8083BCD0: MHR moveset dodge-hop binding keyed on
                // controlStickDirection (passed through siteArg).
                LinkAnimationHeader* hopAnim = MhrMoveset_GetMoveAnim(siteArg);
                if (hopAnim != nullptr) {
                    *animOut = hopAnim;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_SHIELD_RAISE: {
                // z_player.c shield raise: MHR moveset shield binding, loop phase 0.
                LinkAnimationHeader* mhrShield = MhrMoveset_GetShieldAnim(0);
                if (mhrShield != nullptr) {
                    *animOut = mhrShield;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_SHIELD_LOOP: {
                // z_player.c Player_Action_80843188: MHR moveset shield binding, loop phase 1.
                LinkAnimationHeader* shieldLoop = MhrMoveset_GetShieldAnim(1);
                if (shieldLoop != nullptr) {
                    *animOut = shieldLoop;
                }
                break;
            }
            case VB_PLAYER_ANIM_SITE_JUMPSLASH_RECOVERY: {
                // z_player.c jump-slash recovery: gated on IsTransformed AND
                // meleeWeaponAnimation in [FLIPSLASH_FINISH, JUMPSLASH_FINISH].
                if (TransformMasks_IsTransformed() &&
                    (player->meleeWeaponAnimation >= PLAYER_MWA_FLIPSLASH_FINISH) &&
                    (player->meleeWeaponAnimation <= PLAYER_MWA_JUMPSLASH_FINISH)) {
                    LinkAnimationHeader* formAnim = MmForm_GetJumpSlashAnim(player->meleeWeaponAnimation);
                    if (formAnim != nullptr) {
                        *animOut = formAnim;
                    }
                }
                break;
            }
            default:
                break;
        }
    });
}

static RegisterShipInitFunc initFuncPlayerAnimOverride(RegisterPlayerAnimOverrideNEI, {});
