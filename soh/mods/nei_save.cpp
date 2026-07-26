// Skijer's NEI — per-save state moved out of the (now 100% vanilla) SaveContext
// into a dedicated "nei" SaveManager section. Old saves lose this state (accepted).
#include <string.h>
#include <string>
#include <fstream>    // pictograph OoT<->MM picture file
#include <filesystem> // Save directory path

#include "nei_save.h"
#include "items/custom_bottles.h" // Bottle_WheelResetTracking (wheel session trackers)
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include <soh/OTRGlobals.h>          // gSaveContext
#include <libultraship/libultraship.h> // Ship::Context (full def for GetPathRelativeToAppDirectory)

static NeiSaveData gNeiSave;

extern "C" SaveContext gSaveContext;

extern "C" NeiSaveData* Nei_Save(void) {
    return &gNeiSave;
}

// Skijer's NEI hookshot overhaul — tiny accessor for decomp TUs that don't pull nei_save.h in
// (z_player_lib.c doubles the Longshot reticle raycast while the Ultrashot unlock is owned).
extern "C" uint8_t Nei_UltrashotOwned(void) {
    return gNeiSave.ultrashotOwned;
}

extern "C" uint8_t Nei_GetOwnedItem(uint8_t slot) {
    if (slot >= 24 && slot < 72) {
        return gNeiSave.ownedItems[slot - 24];
    }
    return 0;
}

extern "C" void Nei_SetOwnedItem(uint8_t slot, uint8_t v) {
    if (slot >= 24 && slot < 72) {
        gNeiSave.ownedItems[slot - 24] = v;
    }
}

// ── Pictograph OoT <-> MM picture sync (Skijer's NEI) ───────────────────────
// Two files next to the save file, both in MM's EXACT format, so 2Ship reads/writes them per slot and
// the pictograph crosses between games (picture + which-subject reward data):
//   <Save>/file<N>_picture.bin    = pictoPhotoI5, raw I5-compressed buffer (11200 bytes, byte-for-byte
//                                   what MM/2Ship hold in gSaveContext...pictoPhotoI5; a byte array, so
//                                   no endianness).
//   <Save>/file<N>_pictoflags.bin = pictoFlags0 then pictoFlags1 (MM's two u32 PICTO_VALID_* bit-sets
//                                   that say WHICH mapped subject was validly photographed — the data
//                                   that drives the MM reward). 8 bytes, native byte order (both PC
//                                   ports hold them native).
// N is 1-based, matching file<N>.sav.
static std::string Picto_SavePath(const char* suffix) {
    std::filesystem::path dir(Ship::Context::GetPathRelativeToAppDirectory("Save"));
    return (dir / ("file" + std::to_string(gSaveContext.fileNum + 1) + suffix)).string();
}

extern "C" void Picto_SyncWrite(void) {
    {
        std::ofstream f(Picto_SavePath("_picture.bin"), std::ios::binary | std::ios::trunc);
        if (f) {
            f.write(reinterpret_cast<const char*>(gNeiSave.pictoPhotoI5), sizeof(gNeiSave.pictoPhotoI5));
        }
    }
    {
        std::ofstream f(Picto_SavePath("_pictoflags.bin"), std::ios::binary | std::ios::trunc);
        if (f) {
            f.write(reinterpret_cast<const char*>(&gNeiSave.pictoFlags0), sizeof(gNeiSave.pictoFlags0));
            f.write(reinterpret_cast<const char*>(&gNeiSave.pictoFlags1), sizeof(gNeiSave.pictoFlags1));
        }
    }
}

static void Picto_SyncRead(void) {
    bool gotPhoto = false;
    {
        std::ifstream f(Picto_SavePath("_picture.bin"), std::ios::binary);
        if (f) {
            f.read(reinterpret_cast<char*>(gNeiSave.pictoPhotoI5), sizeof(gNeiSave.pictoPhotoI5));
            gotPhoto = (bool)f; // a full 11200-byte read succeeded
        }
    }
    {
        std::ifstream f(Picto_SavePath("_pictoflags.bin"), std::ios::binary);
        if (f) {
            f.read(reinterpret_cast<char*>(&gNeiSave.pictoFlags0), sizeof(gNeiSave.pictoFlags0));
            f.read(reinterpret_cast<char*>(&gNeiSave.pictoFlags1), sizeof(gNeiSave.pictoFlags1));
        }
    }
    if (gotPhoto) { // there's a cross-game photo for this slot
        gNeiSave.pictoboxOwned = 1;
        gNeiSave.pictoHasPhoto = 1;
    }
}

// MM adult trade-quest items OoT<->MM sync (Skijer's NEI). A sidecar next to the save holds the
// tradeAdultOwned bitmask (4 bytes, native) so 2Ship can mirror which trade items the player owns for
// the Anju exchange. <Save>/file<N>_tradeitems.bin. The Pendant of Memories crossing over also re-grants
// its Ext Boots 2 combat moveset on the OoT side.
static void TradeItems_SyncWrite(void) {
    std::ofstream f(Picto_SavePath("_tradeitems.bin"), std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(&gNeiSave.tradeAdultOwned), sizeof(gNeiSave.tradeAdultOwned));
    }
}

static void TradeItems_SyncRead(void) {
    std::ifstream f(Picto_SavePath("_tradeitems.bin"), std::ios::binary);
    if (f) {
        uint32_t v = 0;
        f.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (f) {
            gNeiSave.tradeAdultOwned |= v; // merge cross-game ownership
            // Pendant of Memories = trade index 19 -> also unlock its Ext Boots 2 combat moveset.
            // Bit 26 = ExtEquip owned (16 + EQUIP_TYPE_BOOTS*3 + (index-1)); see extended_equipment.c.
            if (v & (1u << 19)) {
                gNeiSave.extEquipOwnedBits |= (1u << 26);
            }
        }
    }
}

namespace {

constexpr const char* kSaveSectionName = "nei";

void NeiSave_Init(bool isDebug) {
    memset(&gNeiSave, 0, sizeof(gNeiSave));
    // Empty custom slots = ITEM_NONE (0xFF), not 0 (=ITEM_STICK). Skijer's NEI
    memset(gNeiSave.ownedItems, 0xFF, sizeof(gNeiSave.ownedItems));
    // Bottle slots start empty.
    memset(gNeiSave.bottleSlots, 0xFF, sizeof(gNeiSave.bottleSlots));
    gNeiSave.bottomlessContent = 0xFF; // empty (0 would read as ITEM_STICK)
    // Session wheel trackers must not leak across files (per-frame reconcile would ghost-write).
    Bottle_WheelResetTracking();
}

void NeiSave_Save(SaveContext* saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveArray("ownedItems", 48, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.ownedItems[i]);
    });
    SaveManager::Instance->SaveData("extEquipOwnedBits", gNeiSave.extEquipOwnedBits);
    SaveManager::Instance->SaveData("lanternFireType", gNeiSave.lanternFireType);
    SaveManager::Instance->SaveData("lanternCapturedTypes", gNeiSave.lanternCapturedTypes);
    SaveManager::Instance->SaveData("twilightUpgrade", gNeiSave.twilightUpgrade);
    SaveManager::Instance->SaveData("ultrashotOwned", gNeiSave.ultrashotOwned); // Skijer's NEI hookshot overhaul
    SaveManager::Instance->SaveData("clawshotModeActive", gNeiSave.clawshotModeActive);
    SaveManager::Instance->SaveData("galeBoomerangModeActive", gNeiSave.galeBoomerangModeActive);
    SaveManager::Instance->SaveData("weaponUpgrades", gNeiSave.weaponUpgrades);
    SaveManager::Instance->SaveData("extEquipSword", gNeiSave.extEquipSword);
    SaveManager::Instance->SaveData("extEquipShield", gNeiSave.extEquipShield);
    SaveManager::Instance->SaveData("extEquipTunic", gNeiSave.extEquipTunic);
    SaveManager::Instance->SaveData("extEquipBoots", gNeiSave.extEquipBoots);
    // Bottle randomizer
    SaveManager::Instance->SaveArray("bottleSlots", 8, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.bottleSlots[i]);
    });
    SaveManager::Instance->SaveData("bottomlessBottleMode", gNeiSave.bottomlessBottleMode);
    SaveManager::Instance->SaveData("netEquipped", gNeiSave.netEquipped);
    SaveManager::Instance->SaveData("bottomlessContent", gNeiSave.bottomlessContent);
    SaveManager::Instance->SaveData("bottomlessCount", gNeiSave.bottomlessCount);
    SaveManager::Instance->SaveData("powerKegOwned", gNeiSave.powerKegOwned);
    SaveManager::Instance->SaveData("powerKegCount", gNeiSave.powerKegCount);
    SaveManager::Instance->SaveData("powerKegMode", gNeiSave.powerKegMode);
    SaveManager::Instance->SaveData("tradeAdultOwned", gNeiSave.tradeAdultOwned);
    // Pictograph Box (MM-format). Flags are tiny; the 11200-byte I5 photo is only written when the
    // pictobox is owned so non-users don't bloat their save.
    SaveManager::Instance->SaveData("pictoboxOwned", gNeiSave.pictoboxOwned);
    SaveManager::Instance->SaveData("pictoHasPhoto", gNeiSave.pictoHasPhoto);
    SaveManager::Instance->SaveData("pictoFlags0", gNeiSave.pictoFlags0);
    SaveManager::Instance->SaveData("pictoFlags1", gNeiSave.pictoFlags1);
    if (gNeiSave.pictoboxOwned) {
        SaveManager::Instance->SaveArray("pictoPhotoI5", 11200, [](size_t i) {
            SaveManager::Instance->SaveData("", gNeiSave.pictoPhotoI5[i]);
        });
    }
    // Fleet Ship Combo cross-game fields (layouts in FleetShipCombo/FleetComboIds.h)
    SaveManager::Instance->SaveData("shieldOwned", gNeiSave.shieldOwned);
    SaveManager::Instance->SaveData("mmQuestItems", gNeiSave.mmQuestItems);
    SaveManager::Instance->SaveArray("comboObtained", 128, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.comboObtained[i]);
    });
    // Generic fcId-indexed cross-item store (comboObtainedFc synced, comboAppliedFc local — both persisted)
    SaveManager::Instance->SaveArray("comboObtainedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.comboObtainedFc[i]);
    });
    SaveManager::Instance->SaveArray("comboAppliedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.comboAppliedFc[i]);
    });
    SaveManager::Instance->SaveData("comboTriforce", gNeiSave.comboTriforce);
    SaveManager::Instance->SaveData("capeHidden", gNeiSave.capeHidden);
    SaveManager::Instance->SaveData("pendantEffectOff", gNeiSave.pendantEffectOff);
    SaveManager::Instance->SaveData("capeOwned", gNeiSave.capeOwned);
    SaveManager::Instance->SaveData("extTunicLayoutVersion", gNeiSave.extTunicLayoutVersion);
    TradeItems_SyncWrite(); // mirror the MM trade-item flags next to the save for 2Ship
}

void NeiSave_Load() {
    // memset first so a save lacking this section loads as a clean new game.
    memset(&gNeiSave, 0, sizeof(gNeiSave));
    memset(gNeiSave.ownedItems, 0xFF, sizeof(gNeiSave.ownedItems)); // empty slot = ITEM_NONE
    memset(gNeiSave.bottleSlots, 0xFF, sizeof(gNeiSave.bottleSlots)); // empty bottle slots
    SaveManager::Instance->LoadArray("ownedItems", 48, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.ownedItems[i], (uint8_t)0xFF); // ITEM_NONE
    });
    SaveManager::Instance->LoadData("extEquipOwnedBits", gNeiSave.extEquipOwnedBits, (uint32_t)0);
    SaveManager::Instance->LoadData("lanternFireType", gNeiSave.lanternFireType, (uint8_t)0);
    SaveManager::Instance->LoadData("lanternCapturedTypes", gNeiSave.lanternCapturedTypes, (uint8_t)0);
    SaveManager::Instance->LoadData("twilightUpgrade", gNeiSave.twilightUpgrade, (uint8_t)0);
    SaveManager::Instance->LoadData("ultrashotOwned", gNeiSave.ultrashotOwned, (uint8_t)0); // Skijer's NEI hookshot overhaul
    SaveManager::Instance->LoadData("clawshotModeActive", gNeiSave.clawshotModeActive, (uint8_t)0);
    SaveManager::Instance->LoadData("galeBoomerangModeActive", gNeiSave.galeBoomerangModeActive, (uint8_t)0);
    SaveManager::Instance->LoadData("weaponUpgrades", gNeiSave.weaponUpgrades, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipSword", gNeiSave.extEquipSword, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipShield", gNeiSave.extEquipShield, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipTunic", gNeiSave.extEquipTunic, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipBoots", gNeiSave.extEquipBoots, (uint8_t)0);
    // Bottle randomizer
    SaveManager::Instance->LoadArray("bottleSlots", 8, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.bottleSlots[i], (uint8_t)0xFF);
    });
    SaveManager::Instance->LoadData("bottomlessBottleMode", gNeiSave.bottomlessBottleMode, (uint8_t)0);
    SaveManager::Instance->LoadData("netEquipped", gNeiSave.netEquipped, (uint8_t)0);
    SaveManager::Instance->LoadData("bottomlessContent", gNeiSave.bottomlessContent, (uint8_t)0xFF); // empty
    SaveManager::Instance->LoadData("bottomlessCount", gNeiSave.bottomlessCount, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegOwned", gNeiSave.powerKegOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegCount", gNeiSave.powerKegCount, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegMode", gNeiSave.powerKegMode, (uint8_t)0);
    SaveManager::Instance->LoadData("tradeAdultOwned", gNeiSave.tradeAdultOwned, (uint32_t)0);
    // Pictograph Box (MM-format). Photo array only present when the pictobox is owned.
    SaveManager::Instance->LoadData("pictoboxOwned", gNeiSave.pictoboxOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("pictoHasPhoto", gNeiSave.pictoHasPhoto, (uint8_t)0);
    SaveManager::Instance->LoadData("pictoFlags0", gNeiSave.pictoFlags0, (uint32_t)0);
    SaveManager::Instance->LoadData("pictoFlags1", gNeiSave.pictoFlags1, (uint32_t)0);
    if (gNeiSave.pictoboxOwned) {
        SaveManager::Instance->LoadArray("pictoPhotoI5", 11200, [](size_t i) {
            SaveManager::Instance->LoadData("", gNeiSave.pictoPhotoI5[i], (uint8_t)0);
        });
    }
    // Fleet Ship Combo cross-game fields (older saves load as zero = nothing obtained)
    SaveManager::Instance->LoadData("shieldOwned", gNeiSave.shieldOwned, (uint16_t)0);
    SaveManager::Instance->LoadData("mmQuestItems", gNeiSave.mmQuestItems, (uint32_t)0);
    SaveManager::Instance->LoadArray("comboObtained", 128, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboObtained[i], (uint8_t)0);
    });
    // Generic fcId-indexed cross-item store (LoadArray tolerates absent/short arrays -> defaults 0,
    // so older saves and future FC-table growth stay backward compatible).
    SaveManager::Instance->LoadArray("comboObtainedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboObtainedFc[i], (uint8_t)0);
    });
    SaveManager::Instance->LoadArray("comboAppliedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboAppliedFc[i], (uint8_t)0);
    });
    SaveManager::Instance->LoadData("comboTriforce", gNeiSave.comboTriforce, (uint16_t)0);
    SaveManager::Instance->LoadData("capeHidden", gNeiSave.capeHidden, (uint8_t)0);
    SaveManager::Instance->LoadData("pendantEffectOff", gNeiSave.pendantEffectOff, (uint8_t)0);
    SaveManager::Instance->LoadData("capeOwned", gNeiSave.capeOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("extTunicLayoutVersion", gNeiSave.extTunicLayoutVersion, (uint8_t)0);
    // Cross-game: a pictograph synced from 2Ship (shared sidecar) wins over the SOH save copy.
    Picto_SyncRead();
    TradeItems_SyncRead(); // merge MM trade-item ownership from the cross-game sidecar
    // Session wheel trackers must not leak across files (per-frame reconcile would ghost-write).
    Bottle_WheelResetTracking();
}

void Register() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    SaveManager::Instance->AddInitFunction(NeiSave_Init);
    SaveManager::Instance->AddSaveFunction(kSaveSectionName, 1, NeiSave_Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(kSaveSectionName, 1, NeiSave_Load);
}

static RegisterShipInitFunc gNeiSaveInit(Register);

} // namespace
