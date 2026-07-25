// FleetSync.cpp (OoT side) — cross-game save cache + shared player-state overlay.
//
// Temp file: <ShipDir>/fleet_temp_flags.json — { "version", "slot", "oot", "mm", "shared" }.
// - WriteDeparture: "oot" = full SaveManager saveBlock (anchor) + regenerate "shared".
// - ApplyArrival: restore "oot" anchor if present (then erase it) + apply "shared" overlay.
// - Save sync: on our OnSaveFile (active game), refresh "shared" + SignalSyncSave; the frozen MM
//   exe applies + saves + acks; on ack we delete the temp file. As the FROZEN side, we do the
//   mirror in ProcessSignals (runs every frame via OnGameFrameUpdate, which still fires while the
//   game update is frozen).
//
// Canonical "shared" schema: OoT item-id space is canonical (bottles/equips translated MM-side via
// FleetComboIds.h). Fields one game can't author natively (e.g. ootMasksOwned here) are ECHOED —
// preserved from the previous shared block instead of regenerated.

#include "FleetSync.h"
#include "FleetShipCombo.h"
#include "FleetComboIds.h"
#include "FleetComboItems.h"                          // FCI_NO_ITEM, FCI_MAX
#include "FleetComboItemsGlue.h"                      // FcCombo_NativeForItem (FcComboItemId -> RG)
#include "soh/SaveManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/static_data.h"  // Rando::StaticData::RetrieveItem + GetGIEntry_Copy
#include "soh/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h> // CVar: persist last-saved game/slot for boot
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/nei_save.h"
extern SaveContext gSaveContext;
extern PlayState* gPlayState;
// Transformation-mask form bridge (mm_player_form.cpp): current MM form (0 FD..4 Human, >4 custom)
// and a setter that wears/removes the matching transformation mask on next gameplay frame.
int MmForm_GetCurrentForm(void);
void MmForm_FleetApplyForm(int mmForm);
}

// Cross-game restart: the raw reset of THIS game (defined in debugconsole.cpp), called by the
// responder pump below WITHOUT signaling so a paired reset never ping-pongs.
extern "C" void FleetCombo_DoLocalReset(void);

namespace {

std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::path(std::wstring(buf, len)).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

// Ship (host) exe dir IS the shared dir (2ship derives it as parent of its own exe dir).
// All fleet IPC/output files live in a <ShipDir>/fleet/ subfolder to keep the exe dir clean.
std::filesystem::path TempFilePath() {
    std::filesystem::path dir = SelfExeDir();
    if (dir.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(dir / "fleet", ec);
    return dir / "fleet" / "temp_flags.json";
}

bool ReadTemp(nlohmann::json& out) {
    std::filesystem::path p = TempFilePath();
    if (p.empty() || !std::filesystem::exists(p)) {
        return false;
    }
    try {
        std::ifstream in(p);
        in >> out;
        return out.is_object();
    } catch (...) {
        SPDLOG_WARN("[FleetSync] temp file unreadable — treating as absent");
        return false;
    }
}

void WriteTemp(const nlohmann::json& j) {
    std::filesystem::path p = TempFilePath();
    if (p.empty()) {
        return;
    }
    try {
        std::filesystem::path tmp = p;
        tmp += ".tmp";
        {
            std::ofstream out(tmp);
            out << std::setw(1) << j << std::endl;
        }
        std::filesystem::rename(tmp, p);
    } catch (...) {
        SPDLOG_WARN("[FleetSync] temp file write failed");
    }
}

void DeleteTemp() {
    std::filesystem::path p = TempFilePath();
    try {
        if (!p.empty() && std::filesystem::exists(p)) {
            std::filesystem::remove(p);
        }
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------
// Shared-state EXTRACT (live OoT state -> canonical json). `sh` may carry a previous shared block
// so echo-only fields survive.
// ---------------------------------------------------------------------------------------------

// Mirror vanilla + ext shield ownership into nei->shieldOwned (FC_SHIELD_* bits) and return it.
uint16_t ComputeShieldOwned() {
    NeiSaveData* nei = Nei_Save();
    uint16_t sh = nei->shieldOwned;
    uint16_t equip = gSaveContext.inventory.equipment;
    if (equip & (1 << 4)) sh |= FC_SHIELD_DEKU;       // EQUIP_FLAG_SHIELD_DEKU
    if (equip & (1 << 5)) sh |= FC_SHIELD_HYLIAN;     // EQUIP_FLAG_SHIELD_HYLIAN
    if (equip & (1 << 6)) sh |= FC_SHIELD_MIRROR_OOT; // EQUIP_FLAG_SHIELD_MIRROR
    // NEI ext shields: extEquipOwnedBits, shields = equipType 1 -> bits 19..21
    if (nei->extEquipOwnedBits & (1u << 19)) sh |= FC_SHIELD_DIVINE;
    if (nei->extEquipOwnedBits & (1u << 20)) sh |= FC_SHIELD_KITE;
    if (nei->extEquipOwnedBits & (1u << 21)) sh |= FC_SHIELD_IKANA;
    nei->shieldOwned = sh;
    return sh;
}

// Canonical equipped shield: 0 none, 1 deku, 2 hylian/hero, 3 mirror-OoT, 4 divine, 5 kite,
// 6 ikana/mirror-MM.
int GetEquippedShieldCanonical() {
    NeiSaveData* nei = Nei_Save();
    if (nei->extEquipShield >= 1 && nei->extEquipShield <= 3) {
        return 3 + nei->extEquipShield; // 4 divine, 5 kite, 6 ikana
    }
    int nibble = (gSaveContext.equips.equipment >> 4) & 0xF; // shield nibble
    return nibble; // 0 none, 1 deku, 2 hylian, 3 mirror
}

void SetEquippedShieldCanonical(int canon) {
    NeiSaveData* nei = Nei_Save();
    switch (canon) {
        case 1: case 2: case 3:
            nei->extEquipShield = 0;
            gSaveContext.equips.equipment = (gSaveContext.equips.equipment & ~0xF0) | (canon << 4);
            break;
        case 4: case 5: case 6:
            nei->extEquipShield = (uint8_t)(canon - 3);
            // ext shields render over a vanilla base: Ikana over Mirror, others over Hylian
            gSaveContext.equips.equipment =
                (gSaveContext.equips.equipment & ~0xF0) | ((canon == 6 ? 3 : 2) << 4);
            break;
        default:
            break; // 0/unknown: leave as-is
    }
}

void PutInvItem(nlohmann::json& inv, const char* key, int slot, bool withAmmo) {
    uint8_t item = gSaveContext.inventory.items[slot];
    inv[key] = (item != 0xFF);
    if (withAmmo) {
        inv[std::string(key) + "Ammo"] = (int)gSaveContext.inventory.ammo[slot];
    }
}

void ApplyInvItem(const nlohmann::json& inv, const char* key, int slot, uint8_t itemId, bool withAmmo) {
    if (!inv.contains(key)) {
        return;
    }
    if (inv[key].get<bool>()) {
        if (gSaveContext.inventory.items[slot] == 0xFF) {
            gSaveContext.inventory.items[slot] = itemId;
        }
    }
    // (ownership is additive: an item you have never disappears because the other game lacks it)
    if (withAmmo) {
        std::string ak = std::string(key) + "Ammo";
        if (inv.contains(ak) && gSaveContext.inventory.items[slot] != 0xFF) {
            gSaveContext.inventory.ammo[slot] = (int8_t)inv[ak].get<int>();
        }
    }
}

void FoldNativesIntoRegistry() {
    NeiSaveData* nei = Nei_Save();
    uint16_t equip = gSaveContext.inventory.equipment;
    if (equip & (1 << 1)) nei->comboObtained[FC_OOT_SWORD_MASTER] = 1;   // EQUIP_FLAG_SWORD_MASTER
    if (equip & (1 << 2)) nei->comboObtained[FC_OOT_SWORD_BIGGORON] = 1; // EQUIP_FLAG_SWORD_BGS
    if (equip & (1 << 9)) nei->comboObtained[FC_OOT_TUNIC_GORON] = 1;
    if (equip & (1 << 10)) nei->comboObtained[FC_OOT_TUNIC_ZORA] = 1;
    if (equip & (1 << 13)) nei->comboObtained[FC_OOT_BOOTS_IRON] = 1;
    if (equip & (1 << 14)) nei->comboObtained[FC_OOT_BOOTS_HOVER] = 1;
    // Child trade chain: flag at least the currently-held trade item.
    uint8_t trade = gSaveContext.inventory.items[SLOT_TRADE_CHILD];
    if (trade >= 0x21 && trade <= 0x23) {
        nei->comboObtained[FC_OOT_TRADE_WEIRD_EGG + (trade - 0x21)] = 1;
    } else if (trade >= 0x2D && trade <= 0x37) {
        nei->comboObtained[FC_OOT_TRADE_POCKET_EGG + (trade - 0x2D)] = 1;
    }
    // Triforce pool: keep registry counter as the max of both stores.
    uint8_t nativeTf = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    if (nativeTf > nei->comboTriforce) {
        nei->comboTriforce = nativeTf;
    }
}

void ApplyRegistryToNatives() {
    NeiSaveData* nei = Nei_Save();
    if (nei->comboObtained[FC_OOT_SWORD_MASTER]) gSaveContext.inventory.equipment |= (1 << 1);
    if (nei->comboObtained[FC_OOT_SWORD_BIGGORON]) gSaveContext.inventory.equipment |= (1 << 2);
    if (nei->comboObtained[FC_OOT_TUNIC_GORON]) gSaveContext.inventory.equipment |= (1 << 9);
    if (nei->comboObtained[FC_OOT_TUNIC_ZORA]) gSaveContext.inventory.equipment |= (1 << 10);
    if (nei->comboObtained[FC_OOT_BOOTS_IRON]) gSaveContext.inventory.equipment |= (1 << 13);
    if (nei->comboObtained[FC_OOT_BOOTS_HOVER]) gSaveContext.inventory.equipment |= (1 << 14);
    uint8_t& nativeTf = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    if (nei->comboTriforce > nativeTf) {
        nativeTf = (uint8_t)std::min<int>(nei->comboTriforce, 255);
    }
}

void ExtractShared(nlohmann::json& sh) {
    NeiSaveData* nei = Nei_Save();
    FoldNativesIntoRegistry();

    sh["schema"] = 1;
    sh["vitals"] = { { "health", gSaveContext.health },
                     { "healthCapacity", gSaveContext.healthCapacity },
                     { "doubleDefense", gSaveContext.isDoubleDefenseAcquired },
                     { "defenseHearts", gSaveContext.inventory.defenseHearts },
                     { "magic", gSaveContext.magic },
                     { "magicLevel", gSaveContext.magicLevel },
                     { "isMagic", gSaveContext.isMagicAcquired },
                     { "isDoubleMagic", gSaveContext.isDoubleMagicAcquired },
                     { "rupees", gSaveContext.rupees } };
    sh["upgrades"] = { { "wallet", CUR_UPG_VALUE(UPG_WALLET) },     { "quiver", CUR_UPG_VALUE(UPG_QUIVER) },
                       { "bombBag", CUR_UPG_VALUE(UPG_BOMB_BAG) },  { "sticks", CUR_UPG_VALUE(UPG_STICKS) },
                       { "nuts", CUR_UPG_VALUE(UPG_NUTS) },         { "strength", CUR_UPG_VALUE(UPG_STRENGTH) },
                       { "scale", CUR_UPG_VALUE(UPG_SCALE) },       { "bulletBag", CUR_UPG_VALUE(UPG_BULLET_BAG) } };
    sh["weaponUpgrades"] = nei->weaponUpgrades;
    sh["shieldOwned"] = ComputeShieldOwned();
    sh["equippedShield"] = GetEquippedShieldCanonical();
    uint16_t equip = gSaveContext.inventory.equipment;
    sh["swordFlags"] = { { "kokiri", (equip & (1 << 0)) != 0 },
                         { "master", (equip & (1 << 1)) != 0 },
                         { "biggoron", (equip & (1 << 2)) != 0 } };
    sh["equippedSword"] = (int)(gSaveContext.equips.equipment & 0xF); // 0 none,1 kokiri,2 master,3 bgs

    nlohmann::json inv = sh.contains("inv") ? sh["inv"] : nlohmann::json::object();
    PutInvItem(inv, "stick", SLOT_STICK, true);
    PutInvItem(inv, "nut", SLOT_NUT, true);
    PutInvItem(inv, "bomb", SLOT_BOMB, true);
    PutInvItem(inv, "bow", SLOT_BOW, true);
    PutInvItem(inv, "bombchu", SLOT_BOMBCHU, true);
    PutInvItem(inv, "fireArrow", SLOT_ARROW_FIRE, false);
    PutInvItem(inv, "iceArrow", SLOT_ARROW_ICE, false);
    PutInvItem(inv, "lightArrow", SLOT_ARROW_LIGHT, false);
    PutInvItem(inv, "lens", SLOT_LENS, false);
    PutInvItem(inv, "beans", SLOT_BEAN, true);
    PutInvItem(inv, "boomerang", SLOT_BOOMERANG, false);
    PutInvItem(inv, "hammer", SLOT_HAMMER, false);
    PutInvItem(inv, "dins", SLOT_DINS_FIRE, false);
    PutInvItem(inv, "farores", SLOT_FARORES_WIND, false);
    PutInvItem(inv, "nayrus", SLOT_NAYRUS_LOVE, false);
    PutInvItem(inv, "slingshot", SLOT_SLINGSHOT, true);
    inv["ocarinaFairy"] = gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_FAIRY ||
                          gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_TIME;
    inv["ocarinaTime"] = gSaveContext.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_TIME;
    // Hookshot chain: 0 none / 1 hookshot / 2 longshot / 3 ultrashot
    int hookLevel = 0;
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] == ITEM_HOOKSHOT) hookLevel = 1;
    if (gSaveContext.inventory.items[SLOT_HOOKSHOT] == ITEM_LONGSHOT) hookLevel = 2;
    if (hookLevel == 2 && nei->ultrashotOwned) hookLevel = 3;
    inv["hookshotLevel"] = hookLevel;
    // clawshot: no native OoT ownership store -> echo (MM authors it)
    if (!inv.contains("clawshot")) inv["clawshot"] = false;
    inv["pictobox"] = nei->pictoboxOwned != 0;
    inv["powderKeg"] = nei->powerKegOwned != 0;
    inv["powderKegCount"] = nei->powerKegCount;
    inv["net"] = nei->netEquipped != 0;
    inv["bottomlessMode"] = nei->bottomlessBottleMode;
    inv["bottomlessContent"] = nei->bottomlessContent; // OoT id space (canonical)
    inv["bottomlessCount"] = nei->bottomlessCount;
    sh["inv"] = inv;

    sh["bottleSlots"] = nei->bottleSlots; // canonical = OoT id space
    sh["ownedItems"] = nei->ownedItems;
    sh["tradeAdultOwned"] = nei->tradeAdultOwned;
    // Quest bitfields are one-way unlocks authored by BOTH games (cross-placement): merge with the
    // previous shared value instead of overwriting, so bits published by MM that we haven't applied
    // locally yet are never clobbered by our own save.
    sh["ootQuestItems"] = sh.value("ootQuestItems", 0u) | (uint32_t)gSaveContext.inventory.questItems;
    sh["gsTokens"] = gSaveContext.inventory.gsTokens;
    sh["mmQuestItems"] = sh.value("mmQuestItems", 0u) | nei->mmQuestItems;
    sh["comboObtained"] = nei->comboObtained;
    // Generic fcId-indexed cross store (counts). comboAppliedFc is LOCAL-only — never serialized.
    sh["comboObtainedFc"] = nei->comboObtainedFc;
    sh["comboTriforce"] = nei->comboTriforce;
    // ootMasksOwned: echo only (no clean native extractor for OoT trade-mask ownership yet)
    if (!sh.contains("ootMasksOwned")) sh["ootMasksOwned"] = 0;

    // Button equips, canonical = OoT ids raw. buttonItems: 0 B, 1-3 C, 4-7 D-pad (SOH layout).
    sh["cEquips"] = { gSaveContext.equips.buttonItems[1], gSaveContext.equips.buttonItems[2],
                      gSaveContext.equips.buttonItems[3] };
    sh["dEquips"] = { gSaveContext.equips.buttonItems[4], gSaveContext.equips.buttonItems[5],
                      gSaveContext.equips.buttonItems[6], gSaveContext.equips.buttonItems[7] };

    int form = MmForm_GetCurrentForm();
    sh["form"] = (form >= 0 && form <= 4) ? form : 4; // custom forms sync as Human
}

// ---------------------------------------------------------------------------------------------
// Shared-state APPLY (canonical json -> live OoT state)
// ---------------------------------------------------------------------------------------------
void ApplyShared(const nlohmann::json& sh) {
    NeiSaveData* nei = Nei_Save();

    if (sh.contains("vitals")) {
        const auto& v = sh["vitals"];
        gSaveContext.healthCapacity = (int16_t)v.value("healthCapacity", (int)gSaveContext.healthCapacity);
        gSaveContext.health =
            (int16_t)std::min<int>(v.value("health", (int)gSaveContext.health), gSaveContext.healthCapacity);
        gSaveContext.isDoubleDefenseAcquired = (uint8_t)v.value("doubleDefense", 0);
        gSaveContext.inventory.defenseHearts = (int8_t)v.value("defenseHearts", 0);
        gSaveContext.magicLevel = (int8_t)v.value("magicLevel", 0);
        gSaveContext.magic = (int8_t)v.value("magic", 0);
        gSaveContext.isMagicAcquired = (uint8_t)v.value("isMagic", 0);
        gSaveContext.isDoubleMagicAcquired = (uint8_t)v.value("isDoubleMagic", 0);
        gSaveContext.rupees = (int16_t)v.value("rupees", (int)gSaveContext.rupees);
    }
    if (sh.contains("upgrades")) {
        const auto& u = sh["upgrades"];
        Inventory_ChangeUpgrade(UPG_WALLET, u.value("wallet", 0));
        Inventory_ChangeUpgrade(UPG_QUIVER, u.value("quiver", 0));
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, u.value("bombBag", 0));
        Inventory_ChangeUpgrade(UPG_STICKS, u.value("sticks", 0));
        Inventory_ChangeUpgrade(UPG_NUTS, u.value("nuts", 0));
        Inventory_ChangeUpgrade(UPG_STRENGTH, u.value("strength", 0));
        Inventory_ChangeUpgrade(UPG_SCALE, u.value("scale", 0));
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, u.value("bulletBag", 0));
    }
    if (sh.contains("weaponUpgrades")) {
        nei->weaponUpgrades |= (uint8_t)sh["weaponUpgrades"].get<int>(); // additive
    }
    if (sh.contains("shieldOwned")) {
        uint16_t owned = (uint16_t)sh["shieldOwned"].get<int>();
        nei->shieldOwned |= owned;
        if (owned & FC_SHIELD_DEKU) gSaveContext.inventory.equipment |= (1 << 4);
        if (owned & FC_SHIELD_HYLIAN) gSaveContext.inventory.equipment |= (1 << 5);
        if (owned & FC_SHIELD_MIRROR_OOT) gSaveContext.inventory.equipment |= (1 << 6);
        if (owned & FC_SHIELD_DIVINE) nei->extEquipOwnedBits |= (1u << 19);
        if (owned & FC_SHIELD_KITE) nei->extEquipOwnedBits |= (1u << 20);
        if (owned & FC_SHIELD_IKANA) nei->extEquipOwnedBits |= (1u << 21);
    }
    if (sh.contains("equippedShield")) {
        SetEquippedShieldCanonical(sh["equippedShield"].get<int>());
    }
    if (sh.contains("swordFlags")) {
        const auto& s = sh["swordFlags"];
        if (s.value("kokiri", false)) gSaveContext.inventory.equipment |= (1 << 0);
        if (s.value("master", false)) gSaveContext.inventory.equipment |= (1 << 1);
        if (s.value("biggoron", false)) gSaveContext.inventory.equipment |= (1 << 2);
    }
    if (sh.contains("equippedSword")) {
        int sw = sh["equippedSword"].get<int>();
        if (sw >= 1 && sw <= 3) { // 4 (Deity) has no OoT equip: keep current
            gSaveContext.equips.equipment = (gSaveContext.equips.equipment & ~0xF) | sw;
        }
    }

    if (sh.contains("inv")) {
        const auto& inv = sh["inv"];
        ApplyInvItem(inv, "stick", SLOT_STICK, ITEM_STICK, true);
        ApplyInvItem(inv, "nut", SLOT_NUT, ITEM_NUT, true);
        ApplyInvItem(inv, "bomb", SLOT_BOMB, ITEM_BOMB, true);
        ApplyInvItem(inv, "bow", SLOT_BOW, ITEM_BOW, true);
        ApplyInvItem(inv, "bombchu", SLOT_BOMBCHU, ITEM_BOMBCHU, true);
        ApplyInvItem(inv, "fireArrow", SLOT_ARROW_FIRE, ITEM_ARROW_FIRE, false);
        ApplyInvItem(inv, "iceArrow", SLOT_ARROW_ICE, ITEM_ARROW_ICE, false);
        ApplyInvItem(inv, "lightArrow", SLOT_ARROW_LIGHT, ITEM_ARROW_LIGHT, false);
        ApplyInvItem(inv, "lens", SLOT_LENS, ITEM_LENS, false);
        ApplyInvItem(inv, "beans", SLOT_BEAN, ITEM_BEAN, true);
        ApplyInvItem(inv, "boomerang", SLOT_BOOMERANG, ITEM_BOOMERANG, false);
        ApplyInvItem(inv, "hammer", SLOT_HAMMER, ITEM_HAMMER, false);
        ApplyInvItem(inv, "dins", SLOT_DINS_FIRE, ITEM_DINS_FIRE, false);
        ApplyInvItem(inv, "farores", SLOT_FARORES_WIND, ITEM_FARORES_WIND, false);
        ApplyInvItem(inv, "nayrus", SLOT_NAYRUS_LOVE, ITEM_NAYRUS_LOVE, false);
        ApplyInvItem(inv, "slingshot", SLOT_SLINGSHOT, ITEM_SLINGSHOT, true);
        if (inv.value("ocarinaTime", false)) {
            gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_TIME;
        } else if (inv.value("ocarinaFairy", false) && gSaveContext.inventory.items[SLOT_OCARINA] == 0xFF) {
            gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
        }
        int hookLevel = inv.value("hookshotLevel", 0);
        if (hookLevel >= 2) {
            gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_LONGSHOT;
        } else if (hookLevel == 1 && gSaveContext.inventory.items[SLOT_HOOKSHOT] == 0xFF) {
            gSaveContext.inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        }
        if (hookLevel >= 3) {
            nei->ultrashotOwned = 1;
        }
        if (inv.value("pictobox", false)) nei->pictoboxOwned = 1;
        if (inv.value("powderKeg", false)) nei->powerKegOwned = 1;
        if (inv.contains("powderKegCount")) {
            int c = inv["powderKegCount"].get<int>();
            if (c > nei->powerKegCount) nei->powerKegCount = (uint8_t)std::min(c, 5);
        }
        if (inv.value("net", false)) nei->netEquipped = 1;
        if (inv.contains("bottomlessMode")) nei->bottomlessBottleMode = (uint8_t)inv["bottomlessMode"].get<int>();
        if (inv.contains("bottomlessContent")) nei->bottomlessContent = (uint8_t)inv["bottomlessContent"].get<int>();
        if (inv.contains("bottomlessCount")) nei->bottomlessCount = (uint8_t)inv["bottomlessCount"].get<int>();
    }

    if (sh.contains("bottleSlots") && sh["bottleSlots"].is_array()) {
        for (int i = 0; i < 8 && i < (int)sh["bottleSlots"].size(); i++) {
            uint8_t t = (uint8_t)sh["bottleSlots"][i].get<int>(); // already OoT ids
            if (t == FC_BOTTLE_UNMAPPED) {
                t = 0x14; // ITEM_BOTTLE: keep an empty bottle, never store the sentinel
            }
            nei->bottleSlots[i] = t;
        }
    }
    if (sh.contains("ownedItems") && sh["ownedItems"].is_array()) {
        for (int i = 0; i < 48 && i < (int)sh["ownedItems"].size(); i++) {
            uint8_t v = (uint8_t)sh["ownedItems"][i].get<int>();
            if (v != 0xFF) nei->ownedItems[i] = v; // additive
        }
    }
    if (sh.contains("tradeAdultOwned")) nei->tradeAdultOwned |= sh["tradeAdultOwned"].get<uint32_t>();
    if (sh.contains("ootQuestItems")) gSaveContext.inventory.questItems |= sh["ootQuestItems"].get<uint32_t>();
    if (sh.contains("gsTokens")) {
        int gs = sh["gsTokens"].get<int>();
        if (gs > gSaveContext.inventory.gsTokens) gSaveContext.inventory.gsTokens = (int16_t)gs;
    }
    if (sh.contains("mmQuestItems")) nei->mmQuestItems |= sh["mmQuestItems"].get<uint32_t>();
    if (sh.contains("comboObtained") && sh["comboObtained"].is_array()) {
        for (int i = 0; i < FC_COMBO_OBTAINED_SIZE && i < (int)sh["comboObtained"].size(); i++) {
            uint8_t v = (uint8_t)sh["comboObtained"][i].get<int>();
            if (v > nei->comboObtained[i]) nei->comboObtained[i] = v; // OR/max merge
        }
    }
    // Generic fcId-indexed cross store: MAX-merge the synced counts. comboAppliedFc is untouched, so a
    // count that grows here opens a deficit that ApplyFcRegistryToNatives grants natively next tick.
    if (sh.contains("comboObtainedFc") && sh["comboObtainedFc"].is_array()) {
        int n = std::min<int>(FC_COMBO_OBTAINED_FC_SIZE, (int)sh["comboObtainedFc"].size());
        for (int i = 0; i < n; i++) {
            uint8_t v = (uint8_t)sh["comboObtainedFc"][i].get<int>();
            if (v > nei->comboObtainedFc[i]) nei->comboObtainedFc[i] = v; // OR/max merge
        }
    }
    if (sh.contains("comboTriforce")) {
        uint16_t tf = (uint16_t)sh["comboTriforce"].get<int>();
        if (tf > nei->comboTriforce) nei->comboTriforce = tf;
    }
    ApplyRegistryToNatives();

    // Button equips: canonical = OoT ids -> direct, skipping unmappable (0xFF keeps current).
    if (sh.contains("cEquips") && sh["cEquips"].is_array()) {
        for (int i = 0; i < 3 && i < (int)sh["cEquips"].size(); i++) {
            uint8_t id = (uint8_t)sh["cEquips"][i].get<int>();
            if (id != 0xFF) gSaveContext.equips.buttonItems[1 + i] = id;
        }
    }
    if (sh.contains("dEquips") && sh["dEquips"].is_array()) {
        for (int i = 0; i < 4 && i < (int)sh["dEquips"].size(); i++) {
            uint8_t id = (uint8_t)sh["dEquips"][i].get<int>();
            if (id != 0xFF) gSaveContext.equips.buttonItems[4 + i] = id;
        }
    }

    if (sh.contains("form")) {
        MmForm_FleetApplyForm(sh["form"].get<int>());
    }
}

// ---------------------------------------------------------------------------------------------
// Generic fcId-indexed cross-item materialization.
//
// comboObtainedFc[fcId] (synced) counts how many copies of each FC cross item exist in the combo;
// comboAppliedFc[fcId] (local) counts how many this OoT save has already granted natively. When the
// former grows past the latter (e.g. after ApplyShared max-merges an obtain from MM) we grant the
// deficit via the Anchor give idiom (RetrieveItem -> GetGIEntry_Copy -> Randomizer_Item_Give, one
// call per missing copy), then converge applied = obtained.
//
// DOUBLE-COUNT GUARD: Randomizer_Item_Give re-enters the record hook in randomizer.cpp, which would
// bump comboObtainedFc AND comboAppliedFc again (and worse, re-inflate the SYNCED comboObtainedFc and
// send it back to MM). Merely pre-incrementing comboAppliedFc does NOT help, because the hook also
// bumps comboObtainedFc. So we set sApplyingFc for the whole pass; the record hook checks
// FleetSync_IsApplyingFc() and skips recording entirely while we grant. Must run with a live
// gPlayState (Randomizer_Item_Give needs a PlayState) — the caller gates on gPlayState != NULL.
// ---------------------------------------------------------------------------------------------
bool sApplyingFc = false;

void ApplyFcRegistryToNatives() {
    NeiSaveData* nei = Nei_Save();
    sApplyingFc = true; // suppress the record hook's re-entry for the duration of this pass
    for (int fcId = 0; fcId < FCI_MAX; fcId++) {
        int native = FcCombo_NativeForItem(fcId);
        if (native == FCI_NO_ITEM) {
            continue; // no OoT-native relative for this fcId (info-only / MM-only here)
        }
        int deficit = (int)nei->comboObtainedFc[fcId] - (int)nei->comboAppliedFc[fcId];
        if (deficit <= 0) {
            continue;
        }
        GetItemEntry e = Rando::StaticData::RetrieveItem((RandomizerGet)native).GetGIEntry_Copy();
        if (e.modIndex == MOD_RANDOMIZER) { // valid itemTable row (rowless logic-only RGs would assert)
            for (int c = 0; c < deficit; c++) {
                Randomizer_Item_Give(gPlayState, e); // Anchor pattern: one give per missing copy
            }
        }
        // Converge either way so an ungrantable RG doesn't re-run RetrieveItem every frame.
        nei->comboAppliedFc[fcId] = nei->comboObtainedFc[fcId];
    }
    sApplyingFc = false;
}

// ---------------------------------------------------------------------------------------------
// Save sync state
// ---------------------------------------------------------------------------------------------
unsigned long long sLastSeenSyncSeq = 0;
bool sSyncSeqInit = false;
unsigned long long sWaitingAckSeq = 0;
bool sTitleDeleteDone = false;

void RefreshSharedInTemp() {
    nlohmann::json temp;
    ReadTemp(temp);
    nlohmann::json sh = temp.contains("shared") ? temp["shared"] : nlohmann::json::object();
    ExtractShared(sh);
    temp["version"] = 1;
    temp["slot"] = gSaveContext.fileNum;
    temp["shared"] = sh;
    WriteTemp(temp);
}

void HandleOwnSave(int32_t fileNum, int32_t sectionID) {
    if (FleetShipCombo_GetActiveGame() < 0 || !FleetShipCombo_IsThisGameActive()) {
        return; // combo off, or we're the frozen responder (avoid signal loops)
    }
    if (sectionID != SECTION_ID_BASE) {
        return; // only full saves
    }
    // Remember WHERE the player last saved (this game = OoT, this slot) so the next combo boot resumes
    // here. Updated ONLY on a real save, unlike isPlayerIn2Ship which tracks every window switch.
    if (fileNum >= 0 && fileNum <= 2) {
        CVarSetInteger("gFleetCombo.LastSavedGame", 0);
        CVarSetInteger("gFleetCombo.LastSavedSlot", fileNum);
        CVarSave();
    }
    RefreshSharedInTemp();
    FleetShipCombo_SignalSyncSave(fileNum);
    sWaitingAckSeq = FleetShipCombo_GetSyncSaveSeq();
}

int sHoleGrabCooldown = 0; // frames the fleet hole may not GRAB after an arrival (visible, inert)

void ProcessSignals() {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    // Cross-game restart: the OTHER game reset -> reset ourselves too. DoLocalReset does NOT re-signal,
    // so this never ping-pongs.
    if (FleetShipCombo_ConsumeRestartRequest()) {
        FleetCombo_DoLocalReset();
        return;
    }
    // Materialize any FC cross items obtained in the OTHER game (max-merged into comboObtainedFc by
    // ApplyShared). Randomizer_Item_Give needs a live PlayState, so gate on gPlayState. Self-healing:
    // an unapplied deficit lives in the persisted+synced registry and is granted on a later frame if
    // this one lacks a play context, so no grant is ever lost even if a save happens in between.
    if (gPlayState != NULL) {
        ApplyFcRegistryToNatives();
    }
    if (sHoleGrabCooldown > 0) {
        sHoleGrabCooldown--;
    }
    unsigned long long seq = FleetShipCombo_GetSyncSaveSeq();
    if (!sSyncSeqInit) {
        sSyncSeqInit = true;
        sLastSeenSyncSeq = seq; // don't react to pre-attach signals
    }
    // RESPONDER: the other (active) game saved -> absorb shared + save our slot + ack.
    if (seq != sLastSeenSyncSeq) {
        sLastSeenSyncSeq = seq;
        if (!FleetShipCombo_IsThisGameActive()) {
            nlohmann::json temp;
            if (ReadTemp(temp) && temp.contains("shared")) {
                // Guard the deserialize: MM's shared block after a check picked up there can carry
                // rando/FC data whose parse throws (nlohmann .at()); an uncaught throw here would
                // std::terminate SoH. Catch + log so the host survives.
                try {
                    ApplyShared(temp["shared"]);
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("[FleetSync] ApplyShared threw (responder): {}", e.what());
                } catch (...) {
                    SPDLOG_ERROR("[FleetSync] ApplyShared threw a non-std exception (responder)");
                }
            }
            int slot = FleetShipCombo_GetSyncSaveSlot();
            // The OTHER (active) game = MM just saved: record MM + its slot as the last-saved location
            // so the next combo boot resumes into MM. OoT is the host, so it owns this persistent record
            // for BOTH games.
            if (slot >= 0 && slot <= 2) {
                CVarSetInteger("gFleetCombo.LastSavedGame", 1);
                CVarSetInteger("gFleetCombo.LastSavedSlot", slot);
                CVarSave();
            }
            if (slot >= 0 && slot <= 2 && gSaveContext.fileNum == slot && gPlayState != NULL) {
                SaveManager::Instance->SaveFile(slot);
            }
            FleetShipCombo_AckSyncSave(seq);
        }
    }
    // REQUESTER: our save was absorbed by the other exe -> both files combined, delete the temp.
    if (sWaitingAckSeq != 0 && FleetShipCombo_GetSyncSaveAck() >= sWaitingAckSeq) {
        sWaitingAckSeq = 0;
        DeleteTemp();
    }
}

void RegisterFleetSync() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveFile>(HandleOwnSave);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(ProcessSignals);
}

// ---------------------------------------------------------------------------------------------
// Door_Ana fleet-hole registry
// ---------------------------------------------------------------------------------------------
void* sFleetHole = nullptr;
bool sHoleFallPending = false;

} // namespace

extern "C" {

void FleetSync_WriteDeparture(int slot) {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    if (slot < 0 || slot > 2) {
        SPDLOG_WARN("[FleetSync] departure with no real file loaded (slot {}) — ignored", slot);
        return; // never anchor/extract an unloaded save (title demo etc.)
    }
    nlohmann::json temp;
    ReadTemp(temp);
    temp["version"] = 1;
    temp["slot"] = slot;
    temp["oot"] = SaveManager::Instance->SaveToJsonObject(); // full anchor, live state, no disk IO
    nlohmann::json sh = temp.contains("shared") ? temp["shared"] : nlohmann::json::object();
    ExtractShared(sh);
    temp["shared"] = sh;
    WriteTemp(temp);
    SPDLOG_INFO("[FleetSync] OoT departure written (slot {})", slot);
}

void FleetSync_ApplyArrival(int slot) {
    (void)slot;
    if (FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    nlohmann::json temp;
    if (!ReadTemp(temp)) {
        return;
    }
    bool dirty = false;
    if (temp.contains("oot")) {
        try {
            SaveManager::Instance->LoadFromJsonObject(temp["oot"]); // full state restore (anchor wins over disk)
            SPDLOG_INFO("[FleetSync] OoT anchor restored");
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetSync] OoT anchor restore threw: {} — kept the loaded save", e.what());
        } catch (...) {
            SPDLOG_ERROR("[FleetSync] OoT anchor restore threw a non-std exception");
        }
        temp.erase("oot");
        dirty = true;
    }
    if (temp.contains("shared")) {
        // Guard the shared overlay: MM's shared block after a check picked up there can carry rando/FC
        // data whose parse throws (nlohmann .at()/.get type). An UNCAUGHT throw here crashed OoT on
        // ARRIVAL, and 2ship's host-death watchdog then exit(0)'d — so it LOOKED like 2ship closed after
        // "get a check + return to OoT". Catch + log so the host survives.
        try {
            ApplyShared(temp["shared"]);
            SPDLOG_INFO("[FleetSync] shared overlay applied (OoT)");
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetSync] ApplyShared threw on arrival: {}", e.what());
        } catch (...) {
            SPDLOG_ERROR("[FleetSync] ApplyShared threw a non-std exception on arrival");
        }
    }
    if (dirty) {
        WriteTemp(temp);
    }
}

void FleetSync_OnTitleScreen(void) {
    if (sTitleDeleteDone || FleetShipCombo_GetActiveGame() < 0) {
        return;
    }
    sTitleDeleteDone = true;
    DeleteTemp();
    SPDLOG_INFO("[FleetSync] title screen -> temp file deleted");
}

void FleetSync_RegisterFleetHole(void* actor) {
    sFleetHole = actor;
}
int FleetSync_IsFleetHole(void* actor) {
    return actor != nullptr && actor == sFleetHole;
}
void FleetSync_OnHoleFall(void) {
    sHoleFallPending = true;
}
int FleetSync_HoleFallPending(void) {
    return sHoleFallPending ? 1 : 0;
}
void FleetSync_ClearHoleFall(void) {
    sHoleFallPending = false;
}
void FleetSync_SetHoleGrabCooldown(int frames) {
    sHoleGrabCooldown = frames;
}
int FleetSync_HoleGrabInert(void) {
    return sHoleGrabCooldown > 0 ? 1 : 0;
}

// Cross-TU guard: the randomizer record hook (randomizer.cpp Randomizer_Item_Give) queries this and
// skips recording while ApplyFcRegistryToNatives is granting the FC deficit, preventing a double-count.
int FleetSync_IsApplyingFc(void) {
    return sApplyingFc ? 1 : 0;
}

} // extern "C"

static RegisterShipInitFunc initFleetSync(RegisterFleetSync, {});
