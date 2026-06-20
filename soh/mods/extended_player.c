/**
 * extended_player.c - Extended player item action system
 *
 * Maps custom ITEM_xxx values to PLAYER_IA_xxx actions, and each custom
 * PLAYER_IA_xxx to its model group / update func / init func.
 *
 * MM-PORT BOUNDARY: every custom item is one row in sNeiItems[] below. To port
 * an item to MM (2ship), copy its logic module + its single descriptor row.
 * The four ExtPlayer_* getters are thin lookups over that table with a vanilla
 * fallback, so there is exactly one place that describes an item's engine glue.
 *
 * Items whose action is a *vanilla* PLAYER_IA (bow combos, swords, medallions ->
 * spells, Chateau Romani -> blue potion) or is chosen dynamically (SW97 arrows ->
 * bow/slingshot by age) are NOT table rows: they alias vanilla behavior and are
 * resolved in ExtPlayer_GetItemAction before the table lookup.
 */

#include "extended_player.h"
#include "extended_inventory.h" // SLOT_*, AGE_REQ_*, NeiItem (Skijer's NEI)
#include "z64.h"
#include "mods/items/custom_items.h"
#include "assets/soh_assets.h" // icon textures (Skijer's NEI)
#include <stddef.h> // NULL

// External reference to vanilla arrays
extern int8_t sItemActions[];
extern uint8_t sActionModelGroups[];
extern s32 (*sItemActionUpdateFuncs[])(Player* this, PlayState* play);
extern void (*sItemActionInitFuncs[])(PlayState* play, Player* this);

// External vanilla functions used by custom items
extern s32 func_8083485C(Player* this, PlayState* play);
extern s32 Player_UpperAction_Sword(Player* this, PlayState* play);
extern void Player_InitDefaultIA(PlayState* play, Player* this);

// External custom item upper action functions
extern s32 Player_UpperAction_Beetle(Player* this, PlayState* play);
extern s32 Player_UpperAction_BombArrows(Player* this, PlayState* play);
extern s32 Player_UpperAction_CaneOfSomaria(Player* this, PlayState* play);
extern s32 Player_UpperAction_DekuLeaf(Player* this, PlayState* play);
extern s32 Player_UpperAction_Shovel(Player* this, PlayState* play);
extern s32 Player_UpperAction_SwitchHook(Player* this, PlayState* play);

// External custom item init functions (not declared in custom_items.h)
extern void Player_InitHyliasGraceIA(PlayState* play, Player* this);
extern void Player_InitZonaiPermafrostIA(PlayState* play, Player* this);
extern void Player_InitSwitchHookIA(PlayState* play, Player* this);
extern void Player_InitMogmaMittsIA(PlayState* play, Player* this);
extern void Player_InitWhipIA(PlayState* play, Player* this);
extern void Player_InitDominionRodIA(PlayState* play, Player* this);
extern void Player_InitTimeGateIA(PlayState* play, Player* this);
extern void Player_InitMinishCapIA(PlayState* play, Player* this);
extern void Player_InitLanternIA(PlayState* play, Player* this);
extern void Player_InitPending3IA(PlayState* play, Player* this);

// Decide whether an SW97 elemental arrow item should fire from bow or slingshot.
// Default: bow for adult, slingshot for child (vanilla age-based weapon).
// With BowSlingshotAmmoFix + TimelessEquipment both enabled, child can own and
// fire the bow — so prefer bow whenever the bow is in inventory regardless of age.
static s32 Sw97_PreferBow(void) {
    s32 useBow = LINK_IS_ADULT;
    if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) &&
        CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) {
        useBow = (INV_CONTENT(ITEM_BOW) == ITEM_BOW);
    }
    return useBow;
}

// ---------------------------------------------------------------------------
// Custom item descriptor table — single source of truth for engine glue.
//
//   item       ITEM_xxx, or NEI_NO_ITEM for IA-only rows (no inventory item).
//   ia         PLAYER_IA_xxx (unique per row).
//   modelGroup PLAYER_MODELGROUP_xxx.
//   slot       page-2 inventory slot (SLOT_*), or NEI_NO_SLOT.
//   ageReq     AGE_REQ_* (AGE_REQ_NONE for slotless rows).
//   icon       page-2 icon texture (NULL = dynamic, getter handles it).
//   updateFn   upper-action update (func_8083485C = generic "no special update").
//   initFn     action init (Player_InitDefaultIA = generic).
//
// Only items whose IA is a *custom* action live here; vanilla-IA aliases are
// resolved separately in ExtPlayer_GetItemAction. Skijer's NEI
// ---------------------------------------------------------------------------
static const NeiItem sNeiItems[] = {
    // item                          ia                              modelGroup                  slot                       ageReq         icon                                       update                          init
    { ITEM_ROCS_FEATHER_SKIJER,      PLAYER_IA_ROCS_FEATHER_SKIJER,  PLAYER_MODELGROUP_DEFAULT,  SLOT_ROCS,                 AGE_REQ_NONE,  (void*)gItemIconRocsFeatherTex,            func_8083485C,                  Player_InitDefaultIA },
    { ITEM_ROCS_CAPE,                PLAYER_IA_ROCS_CAPE,            PLAYER_MODELGROUP_DEFAULT,  SLOT_ROCS,                 AGE_REQ_NONE,  (void*)gItemIconRocsCapeTex,               func_8083485C,                  Player_InitDefaultIA },
    { ITEM_DESIRE_SENSOR,            PLAYER_IA_DESIRE_SENSOR,        PLAYER_MODELGROUP_DEFAULT,  SLOT_DESIRE_SENSOR,        AGE_REQ_NONE,  (void*)gItemIconDesireSensorTex,           func_8083485C,                  Player_InitDefaultIA },
    { ITEM_HYLIAS_GRACE,             PLAYER_IA_HYLIAS_GRACE,         PLAYER_MODELGROUP_DEFAULT,  SLOT_HYLIAS_GRACE,         AGE_REQ_NONE,  (void*)gItemIconHyliaGraceTex,             func_8083485C,                  Player_InitHyliasGraceIA },
    { ITEM_ZONAI_PERMAFROST,         PLAYER_IA_ZONAI_PERMAFROST,     PLAYER_MODELGROUP_DEFAULT,  SLOT_ZONAI_PERMAFROST,     AGE_REQ_NONE,  (void*)gItemIconZonaiPermafrostTex,        func_8083485C,                  Player_InitZonaiPermafrostIA },
    { ITEM_DEMISE_DESTRUCTION,       PLAYER_IA_DEMISE_DESTRUCTION,   PLAYER_MODELGROUP_DEFAULT,  SLOT_DEMISE_DESTRUCTION,   AGE_REQ_NONE,  (void*)gItemIconDemiseDestructionTex,      func_8083485C,                  Player_InitDemiseDestructionIA },
    { ITEM_DEKU_LEAF,                PLAYER_IA_DEKU_LEAF,            PLAYER_MODELGROUP_DEFAULT,  SLOT_DEKU_LEAF,            AGE_REQ_CHILD, (void*)gItemIconDekuLeafTex,               Player_UpperAction_DekuLeaf,    Player_InitDefaultIA },
    { ITEM_SWITCH_HOOK,              PLAYER_IA_SWITCH_HOOK,          PLAYER_MODELGROUP_HOOKSHOT, SLOT_SWITCH_HOOK,          AGE_REQ_CHILD, (void*)gItemIconSwitchHookTex,             Player_UpperAction_SwitchHook,  Player_InitSwitchHookIA },
    { ITEM_MOGMA_MITTS,              PLAYER_IA_MOGMA_MITTS,          PLAYER_MODELGROUP_DEFAULT,  SLOT_MOGMA_MITTS,          AGE_REQ_NONE,  (void*)gItemIconMogmaMittsTex,             func_8083485C,                  Player_InitMogmaMittsIA },
    { ITEM_GUST_JAR,                 PLAYER_IA_GUST_JAR,             PLAYER_MODELGROUP_DEFAULT,  SLOT_GUST_JAR,             AGE_REQ_CHILD, (void*)gItemIconGustJarTex,                func_8083485C,                  Player_InitGustJarIA },
    { ITEM_BALL_AND_CHAIN,           PLAYER_IA_BALL_AND_CHAIN,       PLAYER_MODELGROUP_DEFAULT,  SLOT_BALL_AND_CHAIN,       AGE_REQ_ADULT, (void*)gItemIconBallAndChainTex,           func_8083485C,                  Player_InitBallAndChainIA },
    { ITEM_WHIP,                     PLAYER_IA_WHIP,                 PLAYER_MODELGROUP_DEFAULT,  SLOT_WHIP,                 AGE_REQ_NONE,  (void*)gItemIconWhipTex,                   func_8083485C,                  Player_InitWhipIA },
    { ITEM_SPINNER,                  PLAYER_IA_SPINNER,              PLAYER_MODELGROUP_DEFAULT,  SLOT_SPINNER,              AGE_REQ_NONE,  (void*)gItemIconSpinnerTex,                func_8083485C,                  Player_InitSpinnerIA },
    { ITEM_CANE_OF_SOMARIA,          PLAYER_IA_CANE_OF_SOMARIA,      PLAYER_MODELGROUP_DEFAULT,  SLOT_CANE_OF_SOMARIA,      AGE_REQ_NONE,  (void*)gItemIconCaneOfSomariaTex,          Player_UpperAction_CaneOfSomaria, Player_InitCaneOfSomariaIA },
    { ITEM_DOMINION_ROD,             PLAYER_IA_DOMINION_ROD,         PLAYER_MODELGROUP_DEFAULT,  SLOT_DOMINION_ROD,         AGE_REQ_NONE,  (void*)gItemIconDominionRodTex,            func_8083485C,                  Player_InitDominionRodIA },
    { ITEM_TIME_GATE,                PLAYER_IA_TIME_GATE,            PLAYER_MODELGROUP_DEFAULT,  SLOT_TIME_GATE,            AGE_REQ_NONE,  (void*)gItemIconTimeGateTex,               func_8083485C,                  Player_InitTimeGateIA },
    { ITEM_BOMB_ARROWS,              PLAYER_IA_BOMB_ARROWS,          PLAYER_MODELGROUP_DEFAULT,  SLOT_BOMB_ARROWS,          AGE_REQ_ADULT, (void*)gItemIconBombArrowsTex,             Player_UpperAction_BombArrows,  Player_InitBombArrowsIA },
    // Rods use the BGS (two-handed) model group + sword mechanics for charge attacks.
    { ITEM_ROD_FIRE,                 PLAYER_IA_ROD_FIRE,             PLAYER_MODELGROUP_BGS,      SLOT_FIRE_ROD,             AGE_REQ_NONE,  (void*)gItemIconFireRodTex,                Player_UpperAction_Sword,       Player_InitFireRodIA },
    { ITEM_ROD_ICE,                  PLAYER_IA_ROD_ICE,              PLAYER_MODELGROUP_BGS,      SLOT_ICE_ROD,              AGE_REQ_NONE,  (void*)gItemIconIceRodTex,                 Player_UpperAction_Sword,       Player_InitIceRodIA },
    { ITEM_ROD_LIGHT,                PLAYER_IA_ROD_LIGHT,            PLAYER_MODELGROUP_BGS,      SLOT_LIGHT_ROD,            AGE_REQ_NONE,  (void*)gItemIconLightRodTex,               Player_UpperAction_Sword,       Player_InitLightRodIA },
    { ITEM_BEETLE,                   PLAYER_IA_BEETLE,               PLAYER_MODELGROUP_DEFAULT,  SLOT_BEETLE,               AGE_REQ_ADULT, (void*)gItemIconBeetleTex,                 Player_UpperAction_Beetle,      Player_InitBeetleIA },
    { ITEM_SHOVEL,                   PLAYER_IA_SHOVEL,               PLAYER_MODELGROUP_DEFAULT,  SLOT_SHOVEL,               AGE_REQ_NONE,  (void*)gItemIconShovelTex,                 Player_UpperAction_Shovel,      Player_InitDefaultIA },
    { ITEM_MINISH_CAP,               PLAYER_IA_MINISH_CAP,           PLAYER_MODELGROUP_DEFAULT,  SLOT_MINISH_CAP,           AGE_REQ_CHILD, (void*)gItemIconMinishCapTex,              func_8083485C,                  Player_InitMinishCapIA },
    // Lantern: icon is dynamic (chosen by fire type) -> NULL, getter handles it. Skijer's NEI
    { ITEM_LANTERN,                  PLAYER_IA_LANTERN,              PLAYER_MODELGROUP_DEFAULT,  SLOT_LANTERN,              AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitLanternIA },
    // Pokéball maps onto the PENDING_3 action slot.
    { ITEM_POKEBALL,                 PLAYER_IA_PENDING_3,            PLAYER_MODELGROUP_DEFAULT,  SLOT_PENDING_3,            AGE_REQ_NONE,  (void*)gItemIconPokeballTex,               func_8083485C,                  Player_InitPending3IA },
    // IA-only: reserved action with no inventory item (default behavior).
    { NEI_NO_ITEM,                   PLAYER_IA_UNUSED_5B,            PLAYER_MODELGROUP_DEFAULT,  NEI_NO_SLOT,               AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitDefaultIA },
    // Bottle with Magic Mushroom — bottle behavior (drop on B-swing via vanilla path).
    { ITEM_BOTTLE_WITH_MAGIC_MUSHROOM, PLAYER_IA_BOTTLE_MAGIC_MUSHROOM, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT,            AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitDefaultIA },

    // MM Mask IAs (all no-op: default model, generic update + init). Page-3 slots + icons stay on gPage3Mask* tables.
    { ITEM_MM_MASK_POSTMAN,      PLAYER_IA_MM_MASK_POSTMAN,       PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_ALL_NIGHT,    PLAYER_IA_MM_MASK_ALL_NIGHT,     PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_BLAST,        PLAYER_IA_MM_MASK_BLAST,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_STONE,        PLAYER_IA_MM_MASK_STONE,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_GREAT_FAIRY,  PLAYER_IA_MM_MASK_GREAT_FAIRY,   PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_DEKU,         PLAYER_IA_MM_MASK_DEKU,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_KEATON,       PLAYER_IA_MM_MASK_KEATON,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_BREMEN,       PLAYER_IA_MM_MASK_BREMEN,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_BUNNY,        PLAYER_IA_MM_MASK_BUNNY,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_DON_GERO,     PLAYER_IA_MM_MASK_DON_GERO,      PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_SCENTS,       PLAYER_IA_MM_MASK_SCENTS,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_GORON,        PLAYER_IA_MM_MASK_GORON,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_ROMANI,       PLAYER_IA_MM_MASK_ROMANI,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_CIRCUS_LEADER, PLAYER_IA_MM_MASK_CIRCUS_LEADER, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_KAFEI,        PLAYER_IA_MM_MASK_KAFEI,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_COUPLE,       PLAYER_IA_MM_MASK_COUPLE,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_TRUTH,        PLAYER_IA_MM_MASK_TRUTH,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_ZORA,         PLAYER_IA_MM_MASK_ZORA,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_KAMARO,       PLAYER_IA_MM_MASK_KAMARO,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_GIBDO,        PLAYER_IA_MM_MASK_GIBDO,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_GARO,         PLAYER_IA_MM_MASK_GARO,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_CAPTAIN,      PLAYER_IA_MM_MASK_CAPTAIN,       PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_GIANT,        PLAYER_IA_MM_MASK_GIANT,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
    { ITEM_MM_MASK_FIERCE_DEITY, PLAYER_IA_MM_MASK_FIERCE_DEITY,  PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA },
};

#define NEI_ITEMS_COUNT (sizeof(sNeiItems) / sizeof(sNeiItems[0]))

// Skijer's NEI
const NeiItem* Nei_FindByItem(int32_t item) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].item != NEI_NO_ITEM && sNeiItems[i].item == item) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

// Skijer's NEI
const NeiItem* Nei_FindBySlot(uint8_t slot) {
    if (slot == NEI_NO_SLOT) {
        return NULL;
    }
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].slot == slot) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

static const NeiItem* ExtPlayer_FindByIA(int32_t itemAction) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].ia == itemAction) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

/**
 * Get the PLAYER_IA_xxx value for a given ITEM_xxx value.
 */
int8_t ExtPlayer_GetItemAction(int32_t item) {
    // Handle special cases first
    if (item >= ITEM_NONE_FE) {
        return PLAYER_IA_NONE;
    }
    if (item == ITEM_LAST_USED) {
        return PLAYER_IA_SWORD_CS;
    }
    if (item == ITEM_FISHING_POLE) {
        return PLAYER_IA_FISHING_POLE;
    }

    // Vanilla-IA aliases: custom items that behave as an existing vanilla action.
    // (Their model group / update / init come from the vanilla arrays, so they are
    // intentionally NOT table rows.)
    switch (item) {
        // Bow combos and swords (originally in the expanded vanilla array).
        case ITEM_BOW_ARROW_FIRE:
            return PLAYER_IA_BOW_FIRE;
        case ITEM_BOW_ARROW_ICE:
            return PLAYER_IA_BOW_ICE;
        case ITEM_BOW_ARROW_LIGHT:
            return PLAYER_IA_BOW_LIGHT;
        case ITEM_SWORD_KOKIRI:
            return PLAYER_IA_SWORD_KOKIRI;
        case ITEM_SWORD_MASTER:
            return PLAYER_IA_SWORD_MASTER;
        case ITEM_SWORD_BGS:
            return PLAYER_IA_SWORD_BIGGORON;

        // Chateau Romani (bottle item - drink to activate infinite magic)
        case ITEM_CHATEAU_ROMANI:
            return PLAYER_IA_BOTTLE_POTION_BLUE;

        // SW97 Medallion spells (quest medallions → spell IAs)
        case ITEM_MEDALLION_FOREST:
            return PLAYER_IA_MAGIC_SPELL_15;
        case ITEM_MEDALLION_SPIRIT:
            return PLAYER_IA_MAGIC_SPELL_16;
        case ITEM_MEDALLION_SHADOW:
            return PLAYER_IA_MAGIC_SPELL_17;
        case ITEM_MEDALLION_WATER:
            return PLAYER_IA_FARORES_WIND;
        case ITEM_MEDALLION_LIGHT:
            return PLAYER_IA_NAYRUS_LOVE;
        case ITEM_MEDALLION_FIRE:
            return PLAYER_IA_DINS_FIRE;

        // SW97 Arrow items: bow IA if Sw97_PreferBow() (adult by default, or
        // child-with-bow when both BowSlingshotAmmoFix and TimelessEquipment are on),
        // slingshot IA otherwise. (Dynamic — cannot be a static table cell.)
        case ITEM_SW97_ARROW_FIRE:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_FIRE : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_ICE:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_ICE : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_LIGHT:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_LIGHT : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_DARK:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0C : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_SOUL:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0D : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_WIND:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0E : PLAYER_IA_SLINGSHOT;

        default:
            break;
    }

    // Custom items: unified NEI registry. Skijer's NEI
    const NeiItem* desc = Nei_FindByItem(item);
    if (desc != NULL) {
        return (int8_t)desc->ia;
    }

    // For vanilla items, use the original array if within bounds
    if (item < VANILLA_SITEMACTIONS_SIZE) {
        return sItemActions[item];
    }

    // For items in the gap (equipment, songs, quest items, etc.), return NONE
    return PLAYER_IA_NONE;
}

/**
 * Get the model group for a given PLAYER_IA_xxx value.
 */
uint8_t ExtPlayer_GetActionModelGroup(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->modelGroup;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sActionModelGroups[itemAction];
    }

    return PLAYER_MODELGROUP_DEFAULT;
}

/**
 * Get the update function for a given PLAYER_IA_xxx value.
 */
ItemActionUpdateFunc ExtPlayer_GetItemActionUpdateFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->updateFn;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sItemActionUpdateFuncs[itemAction];
    }

    return func_8083485C;
}

/**
 * Get the init function for a given PLAYER_IA_xxx value.
 */
ItemActionInitFunc ExtPlayer_GetItemActionInitFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->initFn;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sItemActionInitFuncs[itemAction];
    }

    return Player_InitDefaultIA;
}
