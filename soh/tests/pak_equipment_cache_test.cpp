#include "test_require.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <libultraship/libultra/gbi.h>
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"

static void guMtxF2L(float[4][4], Mtx* dst) {
    memset(dst, 0, sizeof(*dst));
}
#define PAK_DL_STUB ((Gfx*)(uintptr_t)1)
#define PAK_LOG(...) ((void)0)
static int age = 17;
#define LINK_AGE_IN_YEARS age
#define YEARS_ADULT 17
struct PlayState {
    int sceneNum = 1;
};
static PlayState play;
static PlayState* gPlayState = &play;
static struct { int entranceIndex = 0; } gSaveContext;
static bool enabled = false, remote = false;
static int CVarGetInteger(const char*, int) {
    return enabled;
}
static bool PakLoader_IsRemoteRenderActive() {
    return remote;
}
static s32 sSelectedAdultIndex = -1, sSelectedChildIndex = -1, sSelectedEquipIndex = -1;
static s32 sForcedModelIndex = -1, sForcedEquipIndex = -1;
static s32 sGetActiveIndex() {
    return sForcedModelIndex >= 0 ? sForcedModelIndex : (age == 17 ? sSelectedAdultIndex : sSelectedChildIndex);
}
struct PakModel {
    std::map<u32, Gfx*> adultEquipDLs, childEquipDLs;
    const char* displayName = "fixture";
};
static std::vector<PakModel> sModels(5);
static constexpr int kSlotCount = 3;
static s32 sSlotMix[kSlotCount] = { -1, -1, -1 };
static const struct {
    u32 aliases[5];
} sSlotGroups[] = { { { 0x50D8, 0x50F0, 0 } }, { { 0x50E0, 0x50F8, 0 } }, { { 0x5110, 0 } } };
static u64 sCacheSlotMixHash = 0;
static void EnsureSlotMixLoaded() {
}
#include "pak_slot_hash.inc"
static Gfx vanilla = gsSPEndDisplayList(), relocated = gsSPEndDisplayList();
static std::map<std::string, Gfx*> resourceOverrides;
static bool resourcesAvailable = true;
static Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    auto it = resourceOverrides.find(path);
    return it != resourceOverrides.end() ? it->second : (resourcesAvailable ? &vanilla : nullptr);
}
#include "pak_otr_signature.inc"
#include "pak_equipment_cache.inc"

static Gfx custom[10];
static void SeedSword() {
    sModels[0].adultEquipDLs = { { 0x50E0, &custom[0] }, { 0x50F8, &custom[1] } };
    sModels[0].childEquipDLs = sModels[0].adultEquipDLs;
    sSlotMix[1] = 0;
    PakLoader_FrameBegin();
}
static void Cleanup() {
    for (const auto& pool : { sEquipCombinedDLs, sEquipCombinedDLsPrev, sRuntimeCombinedDLs, sRuntimeCombinedDLsPrev })
        for (auto* p : pool)
            free(p);
}
int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    for (auto& dl : custom)
        dl.words.w0 = 0xDF000000;
    SeedSword();
    std::string test = argv[1];
    if (test == "deferred") {
        resourcesAvailable = false;
        auto* eq = sGetEquipDLs();
        Gfx* combined = eq->at(0x5450);
        Gfx* hand = eq->at(0x50A0);
        REQUIRE(hand->words.w0 == (uintptr_t)G_DL_OTR_FILEPATH << 24);
        REQUIRE(hand->words.w1 == (uintptr_t)gLinkAdultLeftHandClosedNearDL);
        PakLoader_FrameBegin();
        REQUIRE(sGetEquipDLs()->at(0x5450) == combined); // Stable deferred paths are not relocated resources.
        resourcesAvailable = true; // Deferred resolution still uses the path when the resource arrives.
        PakLoader_FrameBegin();
        REQUIRE(sGetEquipDLs()->at(0x50A0) == hand);
    } else if (test == "sheath") {
        REQUIRE(sGetEquipDLs()->at(0x50C8) == &vanilla);
        resourceOverrides[gLinkAdultSheathNearDL] = &relocated;
        PakLoader_FrameBegin();
        REQUIRE(sGetEquipDLs()->at(0x50C8) == &relocated); // All native fallback dependencies must be refreshed.
        age = 9;
        REQUIRE(sGetEquipDLs()->at(0x50C8) == &relocated); // Child Master slot still uses the adult sheath path.
        resourceOverrides[gLinkAdultSheathNearDL] = &vanilla;
        PakLoader_FrameBegin();
        REQUIRE(sGetEquipDLs()->at(0x50C8) == &vanilla);
    } else if (test == "remote") {
        REQUIRE(sGetEquipDLs()->count(0x5450));
        remote = true; // Vanilla remote: identical body/equipment indices, but local slot mix must be excluded.
        REQUIRE(sGetEquipDLs() == nullptr);
        remote = false;
        REQUIRE(sGetEquipDLs()->count(0x5450));
    } else if (test == "opcodes") {
        vanilla.words.w0 = (uintptr_t)G_MARKER << 24;
        REQUIRE(sGetEquipDLs()->at(0x50A0) == &vanilla); // Resource markers are native commands, not missing assets.
        for (u8 opcode : { G_VTX_OTR_HASH, G_MTX_OTR, G_SETINTENSITY, G_VTX_WIDE }) {
            custom[0].words.w0 = (uintptr_t)opcode << 24;
            REQUIRE(IsValidGfxPtr(&custom[0]));
        }
        REQUIRE(!IsValidGfxPtr((Gfx*)gLinkAdultLeftHandClosedNearDL));
        alignas(Gfx) char rawPath[] = "/objects/object_link_boy/fist";
        REQUIRE(!IsValidGfxPtr((Gfx*)rawPath));
        custom[0].words.w0 = 0x54000000; // ASCII opcode seen in the crash log.
        REQUIRE(!IsValidGfxPtr(&custom[0]));
    } else if (test == "layers") {
        enabled = true;
        sSelectedAdultIndex = 1;
        sModels[1].adultEquipDLs = { { 0x50A0, &custom[2] }, { 0x50E0, &custom[3] }, { 0x50F8, &custom[4] } };
        sSelectedEquipIndex = 2;
        sModels[2].adultEquipDLs = { { 0x50A0, &custom[5] }, { 0x50E0, &custom[6] } };
        auto* eq = sGetEquipDLs();
        REQUIRE(eq->at(0x50A0) == &custom[2]); // Equipment never replaces the body hand.
        REQUIRE(eq->at(0x50E0) == &custom[0]); // Slot beats equipment pack.
        sForcedEquipIndex = 3;
        sModels[3].adultEquipDLs = { { 0x50E0, &custom[7] }, { 0x50A0, &custom[8] } };
        eq = sGetEquipDLs();
        REQUIRE(eq->at(0x50E0) == &custom[7]);
        REQUIRE(eq->at(0x50A0) == &custom[2]);
        sForcedEquipIndex = -1;
        sSelectedEquipIndex = -1;
        age = 9;
        sModels[0].childEquipDLs.clear(); // Opposite-age slot fallback still works.
        REQUIRE(sGetEquipDLs()->at(0x50F8) == &custom[1]);
        sModels[0].childEquipDLs[0x50E0] = PAK_DL_STUB;
        play.sceneNum++;
        PakLoader_FrameBegin();
        REQUIRE(sGetEquipDLs()->at(0x5450) == PAK_DL_STUB);
    } else if (test == "pool") {
        auto* sword = sGetEquipDLs()->at(0x5450);
        auto oldSize = sEquipCombinedDLs.size();
        sSelectedEquipIndex = 2;
        sGetEquipDLs();
        REQUIRE(sEquipCombinedDLs.size() > oldSize);
        REQUIRE(sword[0].words.w1 == (uintptr_t)&custom[0]); // Earlier draws survive same-frame rebuilds.
    } else
        REQUIRE(false);
    Cleanup();
    printf("PASS equipment cache %s\n", argv[1]);
}
