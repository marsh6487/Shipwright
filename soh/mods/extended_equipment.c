/**
 * extended_equipment.c - Extended equipment system (cheat)
 *
 * Core system: page switching, equip/unequip, icon/name lookup, behavior dispatch.
 * Follows the same pattern as extended_inventory.c.
 *
 * When the cheat CVar is enabled, pressing L on the equipment page toggles
 * to a second page showing 12 new equipment pieces (3 per category).
 * Equipped state is persisted via CVars.
 */

#include "extended_equipment.h"
#include "nei_save.h" // Skijer's NEI
#include "transformation_masks/transformation_masks.h"
#include "transformation_masks/assets/mm_asset_loader.h"
#include "pak_loader/pak_loader.h"

extern MmPlayerTransformation MmForm_GetCurrentForm(void);
#include <string.h>
#include <math.h>
#include "z64.h"
#include "z64player.h"
#include "z64save.h"
#include "functions.h"
#include "variables.h"

extern SaveContext gSaveContext;
extern s32 CVarGetInteger(const char* name, s32 defaultValue);
extern f32 CVarGetFloat(const char* name, f32 defaultValue);

// Cane of Byrna 3D model: blue-tinted variant of the Somaria cane, loaded from
// soh.o2r (objects/object_somaria/g_byrna_cane_dl — shares the Somaria tri
// geometry). No inline C model. LoadGfxByName crashes on a missing path, so gate.
extern u8 ResourceMgr_FileExists(const char* resName);
extern Gfx* ResourceMgr_LoadGfxByName(const char* path);

static Gfx* Byrna_GetCaneDL(void) {
    static Gfx* sCached = NULL;
    static u8 sTried = 0;
    if (!sTried) {
        sTried = 1;
        const char* otr = "__OTR__objects/object_somaria/g_byrna_cane_dl";
        if (ResourceMgr_FileExists(otr)) {
            sCached = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return sCached;
}

// NEI Weapon Upgrades — the Hammer upgrade (Iron Knuckle's Axe) is driven from here,
// independent of the extended-equipment cheat. Accessors are defined in
// mods/items/logic/weapon_upgrades.c (linked via the custom_items.c TU).
#include "items/logic/weapon_upgrades.h"

// Unity build includes
#include "equipment/ext_equip_icons.c"
#include "equipment/ext_equip_names.c"
#include "equipment/ext_equip_behavior.c"

// Age requirements (mirror extended_inventory.h to avoid header cycle)
#ifndef AGE_REQ_NONE
#define AGE_REQ_NONE 9
#endif
#ifndef AGE_REQ_ADULT
#define AGE_REQ_ADULT LINK_AGE_ADULT
#endif
#ifndef AGE_REQ_CHILD
#define AGE_REQ_CHILD LINK_AGE_CHILD
#endif

// Per-piece age requirement: [equipType][index-1]
//   SWORD:  Byrna,            Four Sword,    Drillshaft
//   SHIELD: Divine Shield,    Gerudo Scim.,  Shield of Ikana
//   TUNIC:  Champion's Tunic, Spirit Tunic,  Snowquill
//   BOOTS:  Pegasus Anklet,   Pendant Mem.,  Water Dragon Scale
static const u8 sExtEquipAgeReqs[4][3] = {
    { AGE_REQ_NONE,  AGE_REQ_CHILD, AGE_REQ_ADULT },
    { AGE_REQ_NONE,  AGE_REQ_NONE,  AGE_REQ_CHILD },
    { AGE_REQ_NONE,  AGE_REQ_NONE,  AGE_REQ_NONE }, // recolor tunics: any age (Champion/Spirit/Snowquill)
    { AGE_REQ_NONE,  AGE_REQ_NONE,  AGE_REQ_ADULT },
};

u8 ExtEquip_GetAgeReq(s16 equipType, u8 index) {
    if (equipType < 0 || equipType >= 4 || index < 1 || index > 3)
        return AGE_REQ_NONE;
    return sExtEquipAgeReqs[equipType][index - 1];
}

u8 ExtEquip_CheckAgeReq(s16 equipType, u8 index) {
    if (CVarGetInteger("gCheats.TimelessEquipment", 0))
        return 1;
    u8 req = ExtEquip_GetAgeReq(equipType, index);
    return (req == AGE_REQ_NONE) || (req == gSaveContext.linkAge);
}

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
ExtendedEquipmentState gExtEquipState;
u8 gExtEquipSuppressIconOverride = 0;
f32 gChampionSlowFactor = 1.0f;

// Transform backup: stores equipped ext equipment indices before transformation
static u8 sTransformBackup[4] = { 0 }; // [EQUIP_TYPE_SWORD..BOOTS]
static u8 sTransformBackupValid = 0;

#define EXT_EQUIP_PAGE_SWITCH_COOLDOWN 15

// ---------------------------------------------------------------------------
// Page management
// ---------------------------------------------------------------------------

void ExtEquip_Init(void) {
    memset(&gExtEquipState, 0, sizeof(gExtEquipState));
    memset(&gExtEquipBehavior, 0, sizeof(gExtEquipBehavior));

    // Load equipped state from save data (per-file, persisted only on game save) // Skijer's NEI
    gExtEquipState.currentExtSword = Nei_Save()->extEquipSword;
    gExtEquipState.currentExtShield = Nei_Save()->extEquipShield;
    gExtEquipState.currentExtTunic = Nei_Save()->extEquipTunic;
    gExtEquipState.currentExtBoots = Nei_Save()->extEquipBoots;

    // Migrate the old tunic layout (Cape/Spirit/Champion) to
    // Champion/Spirit/Snowquill exactly once. Equipped implies owned.
    if (Nei_Save()->extTunicLayoutVersion < 1) {
        u8 hadCape = ExtEquip_HasItem(EQUIP_TYPE_TUNIC, 1);
        u8 hadChampion = ExtEquip_HasItem(EQUIP_TYPE_TUNIC, 3);
        u8 legacyEquippedTunic = gExtEquipState.currentExtTunic;

        ExtEquip_RemoveItem(EQUIP_TYPE_TUNIC, 1);
        ExtEquip_RemoveItem(EQUIP_TYPE_TUNIC, 3);
        if (hadCape || legacyEquippedTunic == 1) {
            Nei_Save()->capeOwned = 1;
        }
        if (hadChampion || legacyEquippedTunic == 3) {
            ExtEquip_GiveItem(EQUIP_TYPE_TUNIC, 1);
        }
        if (legacyEquippedTunic == 3) {
            gExtEquipState.currentExtTunic = 1;
            Nei_Save()->extEquipTunic = 1;
        } else if (legacyEquippedTunic == 1) {
            gExtEquipState.currentExtTunic = 0;
            Nei_Save()->extEquipTunic = 0;
        }
        Nei_Save()->extTunicLayoutVersion = 1;
    }
    if (ExtEquip_SlotRetired(EQUIP_TYPE_BOOTS, gExtEquipState.currentExtBoots)) {
        if (gExtEquipState.currentExtBoots == 2) {
            ExtEquip_GiveItem(EQUIP_TYPE_BOOTS, 2); // keep the Pendant of Memories owned
        }
        gExtEquipState.currentExtBoots = 0;
        Nei_Save()->extEquipBoots = 0;
    }

    // Clamp to valid range
    if (gExtEquipState.currentExtSword > 3)
        gExtEquipState.currentExtSword = 0;
    if (gExtEquipState.currentExtShield > 3)
        gExtEquipState.currentExtShield = 0;
    if (gExtEquipState.currentExtTunic > 3)
        gExtEquipState.currentExtTunic = 0;
    if (gExtEquipState.currentExtBoots > 3)
        gExtEquipState.currentExtBoots = 0;

    // Generate placeholder icons
    ExtEquip_GenerateIcons();
}

void ExtEquip_Update(void) {
    if (gExtEquipState.pageSwitchTimer > 0) {
        gExtEquipState.pageSwitchTimer--;
    }

    // If cheat was disabled, reset page and clear equipped state
    if (!ExtEquip_IsEnabled()) {
        gExtEquipState.equipPage = 0;
        // Don't clear equipped state here — it persists in CVars
        // and will be re-applied when cheat is re-enabled
    }
}

int ExtEquip_GetPage(void) {
    if (!ExtEquip_IsEnabled()) {
        return 0;
    }
    return gExtEquipState.equipPage;
}

void ExtEquip_SwitchPage(void) {
    if (!ExtEquip_IsEnabled())
        return;

    gExtEquipState.equipPage = (gExtEquipState.equipPage == 0) ? 1 : 0;
    gExtEquipState.pageSwitchTimer = EXT_EQUIP_PAGE_SWITCH_COOLDOWN;

    // When switching to vanilla page, restore original sword if Byrna was overriding it
    if (gExtEquipState.equipPage == 0 && gExtEquipBehavior.byrnaActive) {
        Byrna_Cleanup();
    }
}

u8 ExtEquip_CanSwitch(void) {
    return gExtEquipState.pageSwitchTimer <= 0;
}

u8 ExtEquip_IsEnabled(void) {
    return CVarGetInteger(CVAR_EXT_EQUIP_ENABLED, 0) != 0;
}

// ---------------------------------------------------------------------------
// Equip / Unequip
// ---------------------------------------------------------------------------

static void ExtEquip_SetCurrentByType(s16 equipType, u8 index) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            gExtEquipState.currentExtSword = index;
            Nei_Save()->extEquipSword = index; // Skijer's NEI
            break;
        case EQUIP_TYPE_SHIELD:
            gExtEquipState.currentExtShield = index;
            Nei_Save()->extEquipShield = index; // Skijer's NEI
            break;
        case EQUIP_TYPE_TUNIC:
            gExtEquipState.currentExtTunic = index;
            Nei_Save()->extEquipTunic = index; // Skijer's NEI
            break;
        case EQUIP_TYPE_BOOTS:
            gExtEquipState.currentExtBoots = index;
            Nei_Save()->extEquipBoots = index; // Skijer's NEI
            break;
    }
}

// ---------------------------------------------------------------------------
// Ownership
// ---------------------------------------------------------------------------

static u32 ExtEquip_GetBit(s16 equipType, u8 index) {
    return 1 << (EXT_EQUIP_OWNED_SHIFT + equipType * 3 + (index - 1));
}

u8 ExtEquip_HasItem(s16 equipType, u8 index) {
    if (index == 0 || index > 3 || equipType < 0 || equipType > 3)
        return 0;
    return (Nei_Save()->extEquipOwnedBits & ExtEquip_GetBit(equipType, index)) != 0; // Skijer's NEI
}

void ExtEquip_GiveItem(s16 equipType, u8 index) {
    if (index == 0 || index > 3 || equipType < 0 || equipType > 3)
        return;
    Nei_Save()->extEquipOwnedBits |= ExtEquip_GetBit(equipType, index); // Skijer's NEI
}

// Clear vanilla equipment base that was set for ext equipment.
// Called only from explicit toggle-off paths (not from vanilla equip path,
// which sets its own vanilla equipment before calling ExtEquip_Unequip).
static void ExtEquip_ClearVanillaEquip(s16 equipType) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
            break;
        case EQUIP_TYPE_SHIELD:
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
            break;
        default:
            break;
    }
}

void ExtEquip_RemoveItem(s16 equipType, u8 index) {
    if (index == 0 || index > 3 || equipType < 0 || equipType > 3)
        return;
    Nei_Save()->extEquipOwnedBits &= ~ExtEquip_GetBit(equipType, index); // Skijer's NEI
    // If currently equipped, unequip and clear vanilla base
    if (ExtEquip_GetCurrent(equipType) == index) {
        ExtEquip_Unequip(equipType);
        ExtEquip_ClearVanillaEquip(equipType);
    }
}

// Retired slots (2026-07-15 rework): pieces that no longer live in the ext-equipment grid.
//   TUNIC 1 (Magic Cape)          -> moved to the equipment page's upgrade column (passive effect).
//   BOOTS 2 (Pendant of Memories) -> moved to the upgrade column (effect toggle there).
//   BOOTS 3 (Water Dragon Scale)  -> deleted; Zora swim is the Zora Tunic's permanent effect.
// Their OWNERSHIP bits remain meaningful (the new systems read them) — only the slot is dead.
u8 ExtEquip_SlotRetired(s16 equipType, u8 index) {
    // TUNIC 1 is LIVE again (Champion's Tunic recolor) — only the BOOTS slots stay retired (Pendant
    // moved to the upgrade column on BOOTS 2, Water Dragon Scale deleted on BOOTS 3).
    return (equipType == EQUIP_TYPE_BOOTS && (index == 2 || index == 3));
}

// ---------------------------------------------------------------------------
// Upgrade-column passives: Magic Cape + Pendant of Memories (Skijer 2026-07-15).
// They live on the equipment page's upgrade column now (replacing the bomb-bag and quiver/bullet
// capacity icons). Ownership = the SAME extEquipOwnedBits they always had (TUNIC 1 / BOOTS 2).
//   Cape:    magic refund is ALWAYS active once owned; the A-toggle only hides the cloth on Link.
//   Pendant: the A-toggle enables/disables its whole moveset.
// ---------------------------------------------------------------------------
u8 ExtEquip_CapeOwned(void) {
    // Own bit now (Skijer 2026-07-16): the ext TUNIC-1 slot is a real recolor tunic (Champion's), so
    // the Cape has its own ownership flag, migrated off the old TUNIC-1 bit in ExtEquip_Init.
    return Nei_Save()->capeOwned;
}

void ExtEquip_GiveCape(void) {
    Nei_Save()->capeOwned = 1;
}

u8 ExtEquip_CapeVisible(void) {
    return ExtEquip_CapeOwned() && !Nei_Save()->capeHidden;
}

void ExtEquip_ToggleCapeVisibility(void) {
    Nei_Save()->capeHidden = !Nei_Save()->capeHidden;
}

u8 ExtEquip_PendantOwned(void) {
    return ExtEquip_HasItem(EQUIP_TYPE_BOOTS, 2);
}

u8 ExtEquip_PendantActive(void) {
    return ExtEquip_PendantOwned() && !Nei_Save()->pendantEffectOff;
}

void ExtEquip_TogglePendantEffect(void) {
    Nei_Save()->pendantEffectOff = !Nei_Save()->pendantEffectOff;
}

// ---------------------------------------------------------------------------
// Extended RECOLOR tunics (Skijer 2026-07-16): the 3 ext tunic slots are now real recolor tunics
// (like vanilla Goron/Zora). They equip with Kokiri as the vanilla base and repaint Link's tunic env
// color in Player_DrawImpl. Predicates = "this ext tunic is currently equipped".
//   Slot 1 = Champion's Tunic (blue) — flurry rush + bullet time
//   Slot 2 = Spirit Tunic (orange w/ rupees, black without) — rupee-immunity + fire/water timer skip
//   Slot 3 = Snowquill Tunic (white) — medallion-driven passive resistances
// ---------------------------------------------------------------------------
// Dedicated upgrade-column icons — the Cape/Pendant no longer live in the ext grid (the TUNIC-1 grid
// slot is Champion now), so their kaleido icons come from here, NOT ExtEquip_GetIcon(grid).
void* ExtEquip_GetCapeIcon(void) {
    return (void*)dgItemIconMagicCapeTex;
}
void* ExtEquip_GetPendantIcon(void) {
    return (void*)"__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex";
}

u8 ExtEquip_IsChampionTunic(void) {
    return ExtEquip_IsEnabled() && ExtEquip_GetCurrent(EQUIP_TYPE_TUNIC) == 1;
}
u8 ExtEquip_IsSpiritTunic(void) {
    return ExtEquip_IsEnabled() && ExtEquip_GetCurrent(EQUIP_TYPE_TUNIC) == 2;
}
u8 ExtEquip_IsSnowquillTunic(void) {
    return ExtEquip_IsEnabled() && ExtEquip_GetCurrent(EQUIP_TYPE_TUNIC) == 3;
}
// Spirit Tunic "has money" gate — its damage-immunity + fire/water-timer-skip only work with rupees.
u8 ExtEquip_HasSnowquillResistance(SnowquillResistance resistance) {
    static const s32 sQuestItems[] = {
        QUEST_MEDALLION_WATER,  QUEST_MEDALLION_FIRE,   QUEST_MEDALLION_LIGHT,
        QUEST_MEDALLION_SHADOW, QUEST_MEDALLION_SPIRIT, QUEST_MEDALLION_FOREST,
    };

    return ExtEquip_IsSnowquillTunic() && resistance >= SNOWQUILL_RESIST_ICE &&
           resistance <= SNOWQUILL_RESIST_WIND && CHECK_QUEST_ITEM(sQuestItems[resistance]);
}

u8 ExtEquip_SpiritHasMoney(void) {
    return ExtEquip_IsSpiritTunic() && (gSaveContext.rupees > 0);
}

void ExtEquip_Equip(s16 equipType, u8 index) {
    if (index == 0 || index > 3)
        return;

    // Freed/retired slots can never be equipped (reserved for the new boots)
    if (ExtEquip_SlotRetired(equipType, index))
        return;

    // Pikachu cannot use extended equipment
    if (TransformMasks_IsTransformedAny() && MmForm_GetCurrentForm() == MM_PLAYER_FORM_PIKACHU)
        return;

    // Must own the item to equip it
    if (!ExtEquip_HasItem(equipType, index))
        return;

    // Age restriction
    if (!ExtEquip_CheckAgeReq(equipType, index))
        return;

    // If already equipped, toggle off (unequip)
    u8 current = ExtEquip_GetCurrent(equipType);
    if (current == index) {
        ExtEquip_Unequip(equipType);
        ExtEquip_ClearVanillaEquip(equipType);
        return;
    }

    // Set extended equipment (also syncs to gSaveContext.ship)
    ExtEquip_SetCurrentByType(equipType, index);

    // Set vanilla equipment base for ext equipment
    // Ext swords use Kokiri Sword as base (model + IA), ext shields use Mirror Shield
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            // Ext swords don't change vanilla sword equipment or B button item.
            // The sword model/IA override is handled by the behavior/draw system.
            // This prevents giving BGS/Kokiri Sword if the player doesn't own them.
            break;
        case EQUIP_TYPE_SHIELD:
            // Shield of Ikana (slot 3) uses Mirror Shield model
            if (index == 3) {
                Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
            } else {
                Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HYLIAN);
            }
            break;
        case EQUIP_TYPE_TUNIC:
            Inventory_ChangeEquipment(EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI);
            break;
        case EQUIP_TYPE_BOOTS:
            // Ext boots are accessories — don't change vanilla boots
            break;
    }
}

void ExtEquip_Unequip(s16 equipType) {
    // Restore sword state if Byrna was active
    if (equipType == EQUIP_TYPE_SWORD && gExtEquipBehavior.byrnaActive) {
        Byrna_Cleanup();
    }

    ExtEquip_SetCurrentByType(equipType, 0);
    // NOTE: vanilla equipment is NOT cleared here — callers that need it
    // (toggle-off, remove) call ExtEquip_ClearVanillaEquip separately.
    // The vanilla equip path (z_kaleido_equipment.c) calls ExtEquip_Unequip
    // after already setting vanilla equipment, so clearing here would undo it.
}

// ---------------------------------------------------------------------------
// Transform integration
// ---------------------------------------------------------------------------

void ExtEquip_UnequipForTransform(void) {
    if (!ExtEquip_IsEnabled())
        return;
    if (sTransformBackupValid)
        return; // Already backed up (form-to-form switch)

    sTransformBackup[EQUIP_TYPE_SWORD] = gExtEquipState.currentExtSword;
    sTransformBackup[EQUIP_TYPE_SHIELD] = gExtEquipState.currentExtShield;
    sTransformBackup[EQUIP_TYPE_TUNIC] = gExtEquipState.currentExtTunic;
    sTransformBackup[EQUIP_TYPE_BOOTS] = gExtEquipState.currentExtBoots;
    sTransformBackupValid = 1;

    for (s16 t = EQUIP_TYPE_SWORD; t <= EQUIP_TYPE_BOOTS; t++) {
        if (ExtEquip_GetCurrent(t) != 0) {
            ExtEquip_Unequip(t);
        }
    }
}

void ExtEquip_RestoreFromTransform(void) {
    if (!sTransformBackupValid)
        return;
    if (!ExtEquip_IsEnabled()) {
        sTransformBackupValid = 0;
        return;
    }

    for (s16 t = EQUIP_TYPE_SWORD; t <= EQUIP_TYPE_BOOTS; t++) {
        if (sTransformBackup[t] != 0 && ExtEquip_HasItem(t, sTransformBackup[t])) {
            ExtEquip_Equip(t, sTransformBackup[t]);
        }
    }
    sTransformBackupValid = 0;
}

void ExtEquip_ClearTransformBackup(void) {
    sTransformBackupValid = 0;
    memset(sTransformBackup, 0, sizeof(sTransformBackup));
}

void ExtEquip_ToggleFromCButton(u16 itemId) {
    if (itemId < ITEM_EXT_SWORD_1 || itemId > ITEM_EXT_BOOTS_3)
        return;

    // Pikachu cannot use extended equipment
    if (TransformMasks_IsTransformedAny() && MmForm_GetCurrentForm() == MM_PLAYER_FORM_PIKACHU)
        return;

    // Map itemId to equipType + index
    // ITEM_EXT_SWORD_1=0xE0, _2=0xE1, _3=0xE2
    // ITEM_EXT_SHIELD_1=0xE3, _2=0xE4, _3=0xE5
    // ITEM_EXT_TUNIC_1=0xE6, _2=0xE7, _3=0xE8
    // ITEM_EXT_BOOTS_1=0xE9, _2=0xEA, _3=0xEB
    u16 offset = itemId - ITEM_EXT_SWORD_1; // 0-11
    s16 equipType = offset / 3;             // 0=sword, 1=shield, 2=tunic, 3=boots
    u8 index = (offset % 3) + 1;            // 1-3

    // Age restriction (allow unequip even if age fails — player can always remove)
    u8 current = ExtEquip_GetCurrent(equipType);
    if (current != index && !ExtEquip_CheckAgeReq(equipType, index)) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }

    // Toggle: if already equipped with this index, unequip; otherwise equip
    if (current == index) {
        ExtEquip_Unequip(equipType);
        ExtEquip_ClearVanillaEquip(equipType);
        Audio_PlaySoundGeneral(NA_SE_IT_SHIELD_REMOVE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else {
        ExtEquip_Equip(equipType, index);
        Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

u8 ExtEquip_GetCurrent(s16 equipType) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            return gExtEquipState.currentExtSword;
        case EQUIP_TYPE_SHIELD:
            return gExtEquipState.currentExtShield;
        case EQUIP_TYPE_TUNIC:
            return gExtEquipState.currentExtTunic;
        case EQUIP_TYPE_BOOTS:
            return gExtEquipState.currentExtBoots;
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// Icons / Names
// ---------------------------------------------------------------------------

// Icon lookup table: [type][index-1] = OTR path string
static const char* sExtEquipIconPaths[4][3] = {
    // Swords
    { dgItemIconCaneOfByrnaTex, dgItemIconFourSwordTex, dgItemIconDrillshaftTex },
    // Shields
    { dgItemIconDivineShieldTex, dgItemIconGerudoScimitarTex,
      "__OTR__icon_item_static_yar/gItemIconMirrorShieldTex" }, // Shield of Ikana (MM mirror shield)
    // Tunics — recolor tunics (Skijer 2026-07-16): 1=Champion (blue), 2=Spirit (orange), 3=Snowquill
    // (white). Icons = the vanilla OoT tunic icon recolored (assets/custom/textures/icon_item_custom).
    { dgItemIconChampionsTunicTex, dgItemIconSpiritTunicTex, dgItemIconSnowquillTunicTex },
    // Boots — slot 2 RETIRED (Pendant on the upgrade column; icon still served for that cell) and
    // slot 3 FREED for good (Water Dragon Scale deleted; Zora swim = Zora Tunic effect). Slots 2-3
    // of the grid are reserved for new boots (Gravity Boots + TBD).
    { dgItemIconPegasusAnkletTex, "__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex", NULL },
};

void* ExtEquip_GetIcon(s16 equipType, u8 index) {
    if (equipType < 0 || equipType >= 4 || index < 1 || index > 3) {
        return NULL;
    }

    return (void*)sExtEquipIconPaths[equipType][index - 1];
}

u16 ExtEquip_GetItemId(s16 equipType, u8 index) {
    // Map (type, index) to ITEM_EXT_xxx
    // type 0 (sword): 0xE0 + (index-1)
    // type 1 (shield): 0xE3 + (index-1)
    // type 2 (tunic): 0xE6 + (index-1)
    // type 3 (boots): 0xE9 + (index-1)
    if (index < 1 || index > 3 || equipType < 0 || equipType >= 4) {
        return 0;
    }
    return ITEM_EXT_SWORD_1 + (equipType * 3) + (index - 1);
}

void* ExtEquip_GetNameTex(u16 itemId, u8 language) {
    return ExtEquip_LookupNameTex(itemId, language);
}

// ---------------------------------------------------------------------------
// Behavior
// ---------------------------------------------------------------------------

ExtEquipBehaviorState gExtEquipBehavior;

void ExtEquip_UpdateBehavior(void* playerVoid, void* playVoid) {
    Player* player = (Player*)playerVoid;
    PlayState* play = (PlayState*)playVoid;

    // NEI weapon upgrades are NOT extended equipment — they run whenever the upgrade is owned,
    // regardless of the ext-equipment cheat. Gate on the local player (read global save state +
    // local input for the throw).
    if (gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        if (WeaponUpgrade_HasHammerAxe()) {
            IKAxe_Behavior(player, play);
        } else {
            IKAxe_Cleanup();
        }
        if (WeaponUpgrade_HasGreatFairy()) {
            GreatFairySword_Behavior(player, play);
        }

        // Zora Tunic swim and upgrade-column passives are ownership/equipment based,
        // not extended-page-cheat based.
        DragonScale_Behavior(player, play);
        if (ExtEquip_CapeVisible()) {
            MagicCape_Behavior(player, play);
        }
        MagicCape_Cleanup();
        if (ExtEquip_PendantActive()) {
            Pendant_Behavior(player, play);
        } else {
            Pendant_Reset();
        }
    }

    if (!ExtEquip_IsEnabled()) {
        Champion_Cleanup(play);
        return;
    }

    ExtEquip_DispatchBehavior(player, play);
}

void ExtEquip_OnMeleeHit(void* playerVoid, void* playVoid) {
    Player* player = (Player*)playerVoid;
    PlayState* play = (PlayState*)playVoid;

    // Great Fairy's Sword recovers HP+MP on hit, independent of the ext-equipment cheat.
    if (WeaponUpgrade_HasGreatFairy() && player->heldItemAction == PLAYER_IA_SWORD_BIGGORON) {
        GreatFairySword_OnMeleeHit(player, play);
    }

    if (!ExtEquip_IsEnabled())
        return;

    ExtEquip_OnMeleeHitDispatch(player, play);
}

void ExtEquip_DrawBehavior(void* playerVoid, void* playVoid) {
    Player* player = (Player*)playerVoid;
    PlayState* play = (PlayState*)playVoid;

    // Skip remote dummy players. HarpoonDummyPlayer_Draw delegates to
    // Player_Draw for skeleton/anim parity, which routes here. But these draws
    // read GLOBAL state (the LOCAL player's slots / save) — drawing Four Sword
    // clones / Pegasus wind cone / Magic Cape / IK Axe reticle / Water-Dragon
    // barrier on remote dummies would render the local player's effects on every
    // peer's body. Gate on "this player is the local player actor".
    if (gPlayState != NULL) {
        Player* localPlayer = GET_PLAYER(gPlayState);
        if (player != localPlayer) {
            return;
        }
    }

    // Hammer upgrade reticle — independent of the ext-equipment cheat.
    if (WeaponUpgrade_HasHammerAxe()) {
        IKAxe_DrawReticle(player, play);
    }

    // Ownership/equipped passives draw independently of the extended-page cheat.
    DScale_Draw(player, play);
    if (ExtEquip_CapeVisible()) {
        MagicCape_Draw(player, play);
    }

    if (!ExtEquip_IsEnabled())
        return;

    ExtEquip_DrawDispatch(player, play);
}

void ExtEquip_DrawSwordDL(void* playVoid) {
    PlayState* play = (PlayState*)playVoid;

    // Hammer upgrade: draw the Iron Knuckle's Axe in place of the hammer DL.
    // IKAxe_DrawAxe self-guards on heldItemAction == HAMMER / throw state, and the
    // hammer DL itself is hidden via ExtEquip_ShouldHideSwordDL. Independent of cheat.
    if (WeaponUpgrade_HasHammerAxe()) {
        IKAxe_DrawAxe(play);
    }

    if (gExtEquipState.currentExtSword == 1) {
        // Byrna: draw blue cane DL only when sword is held (not sheathed)
        Player* drawPlayer = GET_PLAYER(play);
        if (Player_GetMeleeWeaponHeld(drawPlayer) != 0) {
            Gfx* byrnaDL = Byrna_GetCaneDL();
            if (byrnaDL != NULL) {
                OPEN_DISPS(play->state.gfxCtx);
                gSPDisplayList(POLY_OPA_DISP++, byrnaDL);
                CLOSE_DISPS(play->state.gfxCtx);
            }
        }
    }
}

u8 ExtEquip_ShouldHideSwordDL(void) {
    // Hammer upgrade: hide the hammer DL only while the axe is actually being drawn
    // (in free mode / putaway, don't hide — vanilla shows the open hand). Independent
    // of the ext-equipment cheat.
    if (WeaponUpgrade_HasHammerAxe() && gExtEquipBehavior.ikAxeDrawing)
        return 1;

    if (!ExtEquip_IsEnabled())
        return 0;

    // Cane of Byrna replaces the sword model with its own draw
    if (gExtEquipState.currentExtSword == 1)
        return 1;

    return 0;
}

const char* ExtEquip_GetShieldDLOverride(void) {
    if (!ExtEquip_IsEnabled())
        return NULL;

    // Divine (1), Kite (2), Shield of Ikana (3): hide OOT shield, draw custom in PostLimbDraw
    if (gExtEquipState.currentExtShield >= 1 && gExtEquipState.currentExtShield <= 3)
        return "HIDE";

    return NULL;
}

// Cached MM Mirror Shield DLs (loaded once from mm.o2r with hash pre-resolution)
static Gfx* sCachedMmShieldHandDL = NULL;
static Gfx* sCachedMmShieldBackDL = NULL;
static u8 sMmShieldLoadAttempted = 0;

static void ExtEquip_LoadMmShieldDLs(void) {
    if (sMmShieldLoadAttempted)
        return;
    sMmShieldLoadAttempted = 1;

    sCachedMmShieldHandDL =
        (Gfx*)TransformMasks_LoadMmDL("objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL");
    // Use the plain shield DL (no embedded matrix) for back — we control the transform
    sCachedMmShieldBackDL = (Gfx*)TransformMasks_LoadMmDL("objects/object_link_child/gLinkHumanMirrorShieldDL");
}

// Shared draw for the cached MM Mirror Shield DLs (hand + back differ only in
// which cached DL is passed). Drawn on XLU to avoid corrupting the OPA pipeline
// (prevents black tint on the tunic).
static void DrawCachedShieldDL(void* playVoid, Gfx* dl) {
    PlayState* play = (PlayState*)playVoid;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, dl);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Custom soh.o2r shield models (brought in from kite_shield.blend via the
// blend_to_nei -> c2obj_nei pipeline). Cached gated loads.
//   slot 1 = Divine Shield (object_nei_divine_shield)
//   slot 2 = Kite Shield    (object_nei_kite_shield)
static Gfx* ExtEquip_GetCachedDL(const char* otr, Gfx** cache, u8* tried) {
    if (!*tried) {
        *tried = 1;
        if (ResourceMgr_FileExists(otr)) {
            *cache = ResourceMgr_LoadGfxByName(otr);
        }
    }
    return *cache;
}

static Gfx* ExtEquip_GetKiteShieldDL(void) {
    static Gfx* sCached = NULL;
    static u8 sTried = 0;
    return ExtEquip_GetCachedDL("__OTR__objects/object_nei_kite_shield/g_kite_shield_dl", &sCached, &sTried);
}

static Gfx* ExtEquip_GetDivineShieldDL(void) {
    static Gfx* sCached = NULL;
    static u8 sTried = 0;
    return ExtEquip_GetCachedDL("__OTR__objects/object_nei_divine_shield/g_divine_shield_dl", &sCached, &sTried);
}

// Shared transform that seats a custom shield model in Link's shield-limb space.
// The model is drawn relative to the sheath/hand limb matrix, whose LOCAL space is
// huge (~6000 N64 units across — the Hylian shield collider quad size in z_player_lib.c).
// Divine + Kite share this placement (both modeled in the same space).
// Final, visually-tuned values (degrees for rotation, N64 units for offset).
#define CUSTOM_SHIELD_SCALE  44.2f
#define CUSTOM_SHIELD_ROT_X  (-95.0f * (M_PI / 180.0f))
#define CUSTOM_SHIELD_ROT_Y  (-27.0f * (M_PI / 180.0f))
#define CUSTOM_SHIELD_ROT_Z  (-99.0f * (M_PI / 180.0f))
#define CUSTOM_SHIELD_OFF_X  (-508.0f)
#define CUSTOM_SHIELD_OFF_Y  (-372.0f)
#define CUSTOM_SHIELD_OFF_Z  (-5.0f)

static void DrawCustomShieldDL(void* playVoid, Gfx* dl) {
    if (dl == NULL)
        return;

    PlayState* play = (PlayState*)playVoid;
    OPEN_DISPS(play->state.gfxCtx);

    // Drawn on XLU (like the MM Ikana shield): a custom DL leaves its combiner/texture
    // state on the pipe; on OPA that bleeds onto the limbs drawn after it (black tunic).
    // The XLU pass runs after all OPA limbs, so the body stays clean.
    Matrix_Push();
    Matrix_Translate(CUSTOM_SHIELD_OFF_X, CUSTOM_SHIELD_OFF_Y, CUSTOM_SHIELD_OFF_Z, MTXMODE_APPLY);
    Matrix_RotateX(CUSTOM_SHIELD_ROT_X, MTXMODE_APPLY);
    Matrix_RotateY(CUSTOM_SHIELD_ROT_Y, MTXMODE_APPLY);
    Matrix_RotateZ(CUSTOM_SHIELD_ROT_Z, MTXMODE_APPLY);
    Matrix_Scale(CUSTOM_SHIELD_SCALE, CUSTOM_SHIELD_SCALE, CUSTOM_SHIELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, dl);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// Hand + back draws share the per-slot model dispatch (onBack picks hand vs sheath
// DL for the MM Mirror Shield; the custom models use one DL for both).
static void ExtEquip_DrawShieldCommon(void* playVoid, u8 onBack) {
    if (!ExtEquip_IsEnabled())
        return;

    switch (gExtEquipState.currentExtShield) {
        case 1: // Divine Shield: custom soh.o2r model
            DrawCustomShieldDL(playVoid, ExtEquip_GetDivineShieldDL());
            break;
        case 2: // Kite Shield: custom soh.o2r model
            DrawCustomShieldDL(playVoid, ExtEquip_GetKiteShieldDL());
            break;
        case 3: { // Shield of Ikana: MM Mirror Shield from mm.o2r
            ExtEquip_LoadMmShieldDLs();
            Gfx* mmDL = onBack ? sCachedMmShieldBackDL : sCachedMmShieldHandDL;
            if (mmDL != NULL)
                DrawCachedShieldDL(playVoid, mmDL);
            break;
        }
    }
}

void ExtEquip_DrawShieldDL(void* playVoid) {
    ExtEquip_DrawShieldCommon(playVoid, 0);
}

// Draw the ext shield on Link's back (sheath position)
void ExtEquip_DrawShieldBackDL(void* playVoid) {
    ExtEquip_DrawShieldCommon(playVoid, 1);
}

// Common prologue for the per-piece dispatch wrappers below: bail out unless
// the cheat is enabled AND the given slot is currently equipped with `index`.
// (ExtEquip_GetCurrent returns the same field these used to read directly.)
#define EXT_EQUIP_REQUIRE(type, index)                                      \
    if (!ExtEquip_IsEnabled() || ExtEquip_GetCurrent(type) != (index))      \
    return

// ExtEquip_DrawWaistScale removed — the Water Dragon Scale item (and its waist pendant model) no
// longer exists; Zora swim is the Zora Tunic's permanent effect (equip_dragonscale.c driver).

// ExtEquip_DrawAnklet / ExtEquip_UpdateAnkletPhysics removed (Skijer 2026-07-15): the Pegasus
// Anklet's model is now the RED-recolored vanilla hover boots drawn in Player_DrawImpl.

void ExtEquip_CaptureCapeShoulderPos(s32 limbIndex) {
    // Cape decoupled from the ext-tunic slot (Skijer 2026-07-15): capture whenever the cloth draws.
    if (!ExtEquip_CapeVisible())
        return;

    MagicCape_CaptureShoulderPos(limbIndex);
}

// ExtEquip_DrawBreastplate removed (Skijer 2026-07-16): Spirit Tunic is a recolor tunic now, no armor
// overlay. Kept as an empty stub so the PostLimbDraw call site needs no edit.
void ExtEquip_DrawBreastplate(void* playVoid) {
    (void)playVoid;
}

u8 ExtEquip_IkanaDeathSave(void* playVoid) {
    if (!Ikana_ShouldRevive())
        return 0;

    PlayState* play = (PlayState*)playVoid;
    Ikana_ConsumeDeathSave(play);
    return 1;
}
