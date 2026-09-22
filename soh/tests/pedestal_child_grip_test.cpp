// Reversing the entire limb turns the child's palm back through the wrist.
// A half-turn also misses the native ceremonial sword's grip, blade and guard
// axes. Exercise the real final override and real GBI output against landmarks
// from the matching native adult/child sword vertices, not guessed angles.
#include "test_require.h"
#include <libultraship/libultra/gbi.h>
#include <cmath>
#include <cstring>
#include <vector>

struct Vec3s {
    s16 x, y, z;
};
struct Vec3f {
    float x, y, z;
};
struct PlayState {
    struct {
        void* gfxCtx;
    } state;
};
struct Player {
    Gfx** leftHandDLists;
};
enum { PLAYER_LIMB_L_HAND, PLAYER_LIMB_L_FOREARM };
enum { BG_TOKI_SWD_HAND_UNCHANGED, BG_TOKI_SWD_HAND_MASTER_SWORD, BG_TOKI_SWD_HAND_CLOSED };
static bool adult, altAssets = true, customAsset, altNative;
static s32 handState = BG_TOKI_SWD_HAND_MASTER_SWORD, sDListsLodOffset;
static Gfx childHand, adultHand, pakHand, pakSword, customSword, nativeSword, alternateSword, unrelated;
static Gfx* selectedPakSword;
static Gfx* selectedPakHand;
static std::vector<void*> allocations;
#define LINK_IS_ADULT adult
#define CVAR_SETTING(name) name
#define PAK_DL_STUB ((Gfx*)(uintptr_t)1)
static constexpr float HAND_COUNTER_SCALE_Y_OFFSET = 100.0f;
static const char gLinkChildLeftFistNearDL[] = "child-fist";
static const char gLinkAdultLeftHandClosedNearDL[] = "adult-fist";
static const char gCustomMasterSwordDL[] = "custom-sword";
static const char gLinkChildLeftHandHoldingMasterSwordDL[] = "child-ceremony";
static const char gLinkAdultLeftHandHoldingMasterSwordNearDL[] = "adult-ceremony-near";
static const char gLinkAdultLeftHandHoldingMasterSwordFarDL[] = "adult-ceremony-far";
static const char* ResolveCustomFPSHand(const char* path) {
    return path;
}
static bool ResourceMgr_FileAltExists(const char* path) {
    return customAsset && path == gCustomMasterSwordDL;
}
static bool ResourceGetIsCustomByName(const char*) {
    return false;
}
static bool TransformMasks_IsTransformedAny() {
    return false;
}
static s32 CVarGetInteger(const char*, s32) {
    return altAssets;
}
static Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    if (path == gLinkChildLeftFistNearDL)
        return &childHand;
    if (path == gLinkAdultLeftHandClosedNearDL)
        return &adultHand;
    REQUIRE(path == gCustomMasterSwordDL);
    return &customSword;
}
static Gfx* PakLoader_GetTimePedestalSwordDL() {
    return selectedPakSword;
}
static Gfx* PakLoader_GetTimePedestalHandDL() {
    return selectedPakHand;
}
static Gfx* PakLoader_GetEquipDL(Player*, s32) {
    return selectedPakHand;
}
static s32 BgTokiSwd_GetTimePedestalHandState(PlayState*, Player*) {
    return handState;
}
static Gfx* Player_ResolveLimbDLForDummyOrLocal(void* resource) {
    if (resource == &childHand || resource == &adultHand)
        return static_cast<Gfx*>(resource);
    REQUIRE(resource == gLinkChildLeftHandHoldingMasterSwordDL ||
            resource == gLinkAdultLeftHandHoldingMasterSwordNearDL ||
            resource == gLinkAdultLeftHandHoldingMasterSwordFarDL);
    return altNative ? &alternateSword : &nativeSword;
}
static void* Graph_Alloc(void*, size_t size) {
    void* result = calloc(1, size);
    REQUIRE(result != nullptr);
    allocations.push_back(result);
    return result;
}
static Mtx* Matrix_MtxFToMtx(MtxF* source, Mtx* destination) {
    // The same float-matrix ABI used by the normal SoH renderer. GBI structures
    // and command emission above are the real engine headers.
    *destination = *source;
    return destination;
}
static void gSPDisplayList(Gfx* command, Gfx* resource) {
    // All fixture resources are already loaded Gfx pointers, so the engine's
    // OTR-path resolver takes its ordinary G_DL branch.
    __gSPDisplayList(command, resource);
}
#include "pedestal_child_grip.inc"

static MtxF Identity() {
    MtxF result = {};
    result.xx = result.yy = result.zz = result.ww = 1.0f;
    return result;
}
static Vec3f Transform(const MtxF& m, Vec3f v) {
    return { m.xx * v.x + m.xy * v.y + m.xz * v.z + m.xw, m.yx * v.x + m.yy * v.y + m.yz * v.z + m.yw,
             m.zx * v.x + m.zy * v.y + m.zz * v.z + m.zw };
}
static void ExpectPoint(Vec3f actual, Vec3f expected, float tolerance) {
    REQUIRE(fabsf(actual.x - expected.x) <= tolerance);
    REQUIRE(fabsf(actual.y - expected.y) <= tolerance);
    REQUIRE(fabsf(actual.z - expected.z) <= tolerance);
}
struct Draw {
    Gfx* resource;
    MtxF matrix;
};
static std::vector<Draw> ExecuteComposite(Gfx* dl) {
    std::vector<Draw> draws;
    std::vector<MtxF> stack;
    MtxF current = Identity();
    for (s32 index = 0; index < 12; ++index) {
        const Gfx& command = dl[index];
        switch (command.words.w0 >> 24) {
            case G_DL:
                draws.push_back({ reinterpret_cast<Gfx*>(command.words.w1), current });
                break;
            case G_MTX: {
                const u32 flags = (command.words.w0 & 0xFF) ^ G_MTX_PUSH;
                REQUIRE(flags == (G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW));
                stack.push_back(current);
                // Every composite starts at the actual animated limb matrix;
                // identity expresses points in that local frame. No production
                // transform helper is reused to calculate expected landmarks.
                current = *reinterpret_cast<const MtxF*>(command.words.w1);
                break;
            }
            case G_POPMTX:
                REQUIRE(!stack.empty());
                current = stack.back();
                stack.pop_back();
                break;
            case G_ENDDL:
                REQUIRE(stack.empty());
                ExpectPoint(Transform(current, { 17, 29, 43 }), { 17, 29, 43 }, 0.0001f);
                return draws;
            default:
                REQUIRE(false);
        }
    }
    REQUIRE(false);
    return draws;
}
static void ExpectNativeSwordBasis(const MtxF& matrix) {
    // Native adult object_link_boyVtx_010EE8[0..135] and child
    // object_link_childVtx_00D4E0[27..162] have matching triangle topology.
    // These literal correspondences cover blade tip, pommel, guard and grip.
    static const Vec3f adultPoints[] = { { 3387, 328, -77 }, { -787, 328, -152 }, { 565, -305, -78 },
                                         { 565, 962, -78 },  { 327, 328, -153 },  { 432, 513, -78 } };
    static const Vec3f childPoints[] = { { -3171, -784, -41 }, { 788, 543, -48 },  { -389, -203, 496 },
                                         { -575, 344, -633 },  { -275, 206, -36 }, { -382, 190, -234 } };
    for (size_t i = 0; i < sizeof(adultPoints) / sizeof(adultPoints[0]); ++i)
        ExpectPoint(Transform(matrix, adultPoints[i]), childPoints[i], 1.1f);
}
static void CheckSelectedSword(Gfx* expectedHand, Gfx* expectedSword) {
    PlayState play = {};
    Player player = {};
    const Vec3s original = { 0x1234, 0x2345, 0x3456 };
    Vec3s rot = original;
    Gfx* dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(rot.x == original.x && rot.y == original.y && rot.z == original.z);
    const auto draws = ExecuteComposite(dl);
    REQUIRE(draws.size() == 2);
    REQUIRE(draws[0].resource == expectedHand && draws[1].resource == expectedSword);
    // Actual ordinary child fist fingertip: keep its palm beyond the +Y wrist,
    // as both native fist and native ceremonial hand meshes are authored.
    ExpectPoint(Transform(draws[0].matrix, { 73, 499, -80 }), { 73, 499, -80 }, 0.0001f);
    if (adult)
        ExpectPoint(Transform(draws[1].matrix, { 3387, 328, -77 }), { 3387, 328, -77 }, 0.0001f);
    else
        ExpectNativeSwordBasis(draws[1].matrix);
    rot = original;
    dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_FOREARM, &dl, &rot);
    REQUIRE(dl == &unrelated && memcmp(&rot, &original, sizeof(rot)) == 0);
}
int main() {
    selectedPakSword = &pakSword;
    CheckSelectedSword(&childHand, &pakSword);
    selectedPakHand = &pakHand;
    CheckSelectedSword(&pakHand, &pakSword);
    selectedPakHand = PAK_DL_STUB;
    CheckSelectedSword(&childHand, &pakSword);
    adult = true;
    CheckSelectedSword(&adultHand, &pakSword);
    selectedPakHand = &pakHand;
    CheckSelectedSword(&pakHand, &pakSword);
    adult = false;
    selectedPakSword = selectedPakHand = nullptr;
    customAsset = true;
    CheckSelectedSword(&childHand, &customSword);
    selectedPakSword = &pakSword; // Selected PAK still wins over Alt custom sword.
    CheckSelectedSword(&childHand, &pakSword);
    selectedPakSword = nullptr;
    PlayState play = {};
    Gfx* emptyHand[] = { &childHand };
    Player player = { emptyHand };
    for (bool alternate : { false, true }) {
        altAssets = false;
        altNative = alternate;
        Gfx* dl = nullptr;
        Vec3s rot = { 1, 2, 3 };
        Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
        REQUIRE(dl == (alternate ? &alternateSword : &nativeSword));
        REQUIRE(rot.x == 1 && rot.y == 2 && rot.z == 3);
    }
    handState = BG_TOKI_SWD_HAND_CLOSED;
    Gfx* dl = nullptr;
    Vec3s rot = { 1, 2, 3 };
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(dl == &childHand && rot.x == 1 && rot.y == 2 && rot.z == 3);
    handState = BG_TOKI_SWD_HAND_UNCHANGED;
    dl = &unrelated;
    Player_ApplyTimePedestalSword(&play, &player, PLAYER_LIMB_L_HAND, &dl, &rot);
    REQUIRE(dl == &unrelated);
    for (void* allocation : allocations)
        free(allocation);
    puts("PASS native child sword landmarks, unchanged wrist/palm, balanced matrix stack, adult and source fallbacks");
}
