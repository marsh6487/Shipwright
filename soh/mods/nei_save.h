// Skijer's NEI
#ifndef NEI_SAVE_H
#define NEI_SAVE_H

#include <stdint.h>
#include "soh/FleetShipCombo/FleetComboIds.h" // FC_COMBO_OBTAINED_FC_SIZE (fcId-indexed store size)

#ifdef __cplusplus
extern "C" {
#endif

// Skijer's NEI: per-save state, serialized via the "nei" SaveManager section
// (NOT in the vanilla SaveContext, which is kept 100% upstream).
typedef struct NeiSaveData {
    uint8_t ownedItems[48];      // custom inventory slots 24..71 (page-2 items + MM masks)
    uint32_t extEquipOwnedBits;  // ext-equipment ownership (was inventory.equipment high bits)
    uint8_t lanternFireType;
    uint8_t lanternCapturedTypes;
    uint8_t twilightUpgrade;
    uint8_t ultrashotOwned;      // Skijer's NEI hookshot overhaul: when owned, the Longshot becomes the
                                 // Ultrashot (4x hookshot reach, 2x speed; Longshot icon + Light-medallion
                                 // corner marker + "Ultrashot" name)
    uint8_t clawshotModeActive;
    uint8_t galeBoomerangModeActive;
    uint8_t weaponUpgrades;
    uint8_t extEquipSword;
    uint8_t extEquipShield;
    uint8_t extEquipTunic;
    uint8_t extEquipBoots;
    // Bottle randomizer (Skijer's NEI). The bottle inventory is 8 slots shown in the save editor as a
    // 4x2 grid: "Bottle A" = slots 0-3, "Bottle B" = slots 4-7. Each holds an OoT ITEM_ content id,
    // ITEM_BOTTLE (empty bottle), or 0xFF (empty slot). This is the OoT-side "which content is in
    // which bottle" state; the kaleido Wheel A/B each cycle their 4 slots. (Cross-game sharing reads/
    // writes these on game switch — layered on later via FscShared.) Wheels A/B map to the vanilla
    // SLOT_BOTTLE_1/2; Net + Bottomless take SLOT_BOTTLE_3/4.
    uint8_t bottleSlots[8];       // 0xFF = empty; ITEM_BOTTLE = empty bottle; else a content id
    uint8_t bottomlessBottleMode; // Bottomless Bottle OWNED (SLOT_BOTTLE_4 item). Skijer's NEI
    uint8_t netEquipped;          // Net OWNED (SLOT_BOTTLE_3 item; behavior deferred)
    // Bottomless Bottle "ammo": SLOT_BOTTLE_4 holds a real bottle content, but instead of emptying in
    // one use it has a per-content use-counter. Each empty (drink/sell) decrements bottomlessCount;
    // while >0 the content auto-refills, at 0 it becomes an empty Bottomless Bottle. (Net has none.)
    uint8_t bottomlessContent;    // content id in the Bottomless Bottle, or ITEM_BOTTLE/0xFF when empty
    uint8_t bottomlessCount;      // remaining uses of bottomlessContent (the counter shown on the icon)
    uint8_t powerKegOwned;        // Power Keg owned (granted via menu); shares the Bomb slot via a
                                  // kaleido wheel, USE gated by form + strength (see power_keg.c)
    uint8_t powerKegCount;        // Power Keg "ammo": how many kegs the player carries (its own
                                  // counter; each use consumes 1). Skijer's NEI
    uint8_t powerKegMode;         // keg mode selected on the Bomb slot (kaleido wheel toggle) — persists
                                  // so the slot doesn't revert to bombs on reload. Skijer's NEI
    // MM adult trade-quest items (Skijer's NEI). Bitmask over a NEI trade index: 0-10 = the OoT items
    // (ITEM_POCKET_EGG..ITEM_CLAIM_CHECK), 11 = Moon's Tear, 12-15 = the four Title Deeds, 16 = Room Key,
    // 17 = Letter to Kafei, 18 = Special Delivery to Mama, 19 = Pendant of Memories. The 2D-grid wheel on
    // SLOT_TRADE_ADULT shows every owned entry. The Pendant's bit is set alongside its combat ownership
    // (extEquipOwnedBits, Ext Boots 2) — both flags on grant. See trade_items.c.
    uint32_t tradeAdultOwned;
    // Pictograph Box (Skijer's NEI). Stored in Majora's Mask's EXACT save layout so a 2Ship bridge
    // can consume it: pictoFlags0/1 are the 64 PICTO_VALID_* bits (set by Snap_SetFlag when a mapped
    // OoT actor is validly photographed), pictoPhotoI5 is the last photo compressed to I5 (160x112).
    // OoT itself gives no reward for these — they exist only to be read by MM/2Ship. See snap.h.
    uint8_t pictoboxOwned;          // Pictobox item owned (granted via CVar/menu)
    uint8_t pictoHasPhoto;          // a photo has been kept (gates the "Replace?" warn before capture)
    uint32_t pictoFlags0;           // MM pictoFlags0: PICTO_VALID_* bits 0x00..0x1F
    uint32_t pictoFlags1;           // MM pictoFlags1: PICTO_VALID_* bits 0x20..0x3F
    uint8_t pictoPhotoI5[11200];    // MM PICTO_PHOTO_COMPRESSED_SIZE = (160*112)*5/8 (I5, last photo)
    // --- Fleet Ship Combo (cross-game) fields — bit/index layouts in FleetShipCombo/FleetComboIds.h ---
    uint16_t shieldOwned;           // unified 10-shield ownership bitmask (FC_SHIELD_*): 3 OoT vanilla +
                                    // 3 NEI ext (Divine/Kite/Ikana) + 4 reserved. Mirrored from the
                                    // vanilla EQUIP_FLAG_SHIELD_* + extEquipOwnedBits on load.
    uint32_t mmQuestItems;          // MM quest ownership OoT-side (FC_MMQ_*: remains, MM songs,
                                    // Bombers' Notebook) — mirror of MM's nei.ootQuestItems pattern
    uint8_t comboObtained[128];     // universal cross-game obtained registry (FC_* index; u8 VALUES:
                                    // flags store 0/1, counters store raw counts). Info-only relatives
                                    // for the combo rando (souls, abilities, trade chain, ...)
    uint16_t comboTriforce;         // shared Triforce-piece count (syncs vs triforcePiecesCollected/MM)
    // Upgrade-column passives (Skijer 2026-07-15): Magic Cape + Pendant of Memories moved out of the
    // ext-equipment grid into the equipment page's upgrade column. Toggled with A on their cells
    // (transparent = off, solid = on — the spiritual-stones visual). Ownership stays in
    // extEquipOwnedBits (TUNIC 1 / BOOTS 2).
    uint8_t capeHidden;             // 1 = don't DRAW the Magic Cape on Link (its magic refund is
                                    // ALWAYS active once owned; this only hides the cloth)
    uint8_t pendantEffectOff;       // 1 = Pendant of Memories moveset disabled (effect toggle)
    // Magic Cape ownership moved OUT of the ext-equipment TUNIC-1 bit (Skijer 2026-07-16): the ext
    // tunic slot 1 is now a real recolor tunic (Champion's Tunic), so the Cape can't squat on that
    // bit anymore. Migrated from the old bit in ExtEquip_Init.
    uint8_t capeOwned;              // 1 = owns the Magic Cape (upgrade-column passive)
    // --- Generic fcId-indexed cross-item sync (FleetComboItems.h FcComboItemId space) ------------
    // SEPARATE id space from comboObtained[128] (that is FC_* registry indices). This store is indexed
    // by FcComboItemId (the X-macro item table); each entry is a u8 COUNT. On obtaining ANY FC cross
    // item in OoT the count is bumped (synced to MM via a MAX-merge); on arrival OoT grants the native
    // deficit for fcIds obtained in the other game. comboAppliedFc is LOCAL bookkeeping (NOT synced):
    // how many copies of each fcId have already been materialized into OoT's native inventory.
    uint8_t comboObtainedFc[FC_COMBO_OBTAINED_FC_SIZE]; // fcId-indexed cross store (counts); synced
    uint8_t comboAppliedFc[FC_COMBO_OBTAINED_FC_SIZE];  // local: copies already materialized here (NOT synced)
} NeiSaveData;

// Single accessor — returns the live per-save state (never NULL).
NeiSaveData* Nei_Save(void);

// Custom inventory slot helpers (slot 24..71 -> ownedItems[slot-24]).
uint8_t Nei_GetOwnedItem(uint8_t slot);
void Nei_SetOwnedItem(uint8_t slot, uint8_t v);

#ifdef __cplusplus
}
#endif

#endif // NEI_SAVE_H
