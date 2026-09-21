#include "z64.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
}
#include "soh/cvar_prefixes.h"
#include "test_require.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// Use the real GBI macros and unchanged production draw body. Matrix/allocator
// calls are graphics-service boundaries; the game and pixels are not exercised.
static struct {
    s16 barrierIntensity;
    bool fastSwimActive;
    s16 swimPitch;
    s16 swimRollSmoothed;
} gFormState;
static std::vector<Gfx> sBarrierDLSafeCopy;
static size_t sBarrierDLCount;
static Gfx sXlu[256], sAllocated[256], sScroll[4];
static Mtx sMatrix;
static GraphicsContext sGfx;
static PlayState sPlay;
static Player sPlayer;
static float sScale;
static int sScaleCalls;
static bool sChanged[3];
static const Color_RGB8 sColors[] = { { 17, 34, 51 }, { 68, 85, 102 }, { 119, 136, 153 } };
static const char* sNames[] = { "Custom.ZoraMagicShield", "Custom.ZoraMagicShieldGlow",
                                "Custom.ZoraMagicShieldHighlights" };

Gfx gCullBackDList[1];
Gfx gEmptyDL[1];

int32_t CVarGetInteger(const char* key, int32_t fallback) {
    for (int i = 0; i < 3; ++i) {
        if (std::string(key) == std::string("gCosmetics.") + sNames[i] + ".Changed") {
            return sChanged[i];
        }
    }
    return fallback;
}

Color_RGB8 CVarGetColor24(const char* key, Color_RGB8 fallback) {
    for (int i = 0; i < 3; ++i) {
        if (std::string(key) == std::string("gCosmetics.") + sNames[i] + ".Value") {
            return sColors[i];
        }
    }
    return fallback;
}

void Matrix_Push(void) {
}
void Matrix_Pop(void) {
}
void Matrix_Translate(f32, f32, f32, u8) {
}
void Matrix_RotateX(f32, u8) {
}
void Matrix_RotateY(f32, u8) {
}
void Matrix_RotateZ(f32, u8) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8) {
    REQUIRE(x == y && y == z);
    sScale = x;
    ++sScaleCalls;
}
f32 Math_CosS(s16 value) {
    return cosf(BINANG_TO_RAD(value));
}
Mtx* Matrix_NewMtx(GraphicsContext*, char*, s32) {
    return &sMatrix;
}
void* Graph_Alloc(GraphicsContext*, size_t size) {
    REQUIRE(size <= sizeof(sAllocated));
    return sAllocated;
}
void Gfx_SetupDL_25Xlu(GraphicsContext*) {
}
void FrameInterpolation_RecordOpenChild(const void*, int) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void gSPSegment(void* packet, int segment, uintptr_t target) {
    __gSPSegment(static_cast<Gfx*>(packet), segment, target);
}
Gfx* Gfx_TwoTexScroll(GraphicsContext*, s32, u32, u32, s32, s32, s32, u32, u32, s32, s32) {
    return sScroll;
}
static void MmForm_PatchSegmentedDL(Gfx*, size_t, u8, Gfx*) {
}
static void MmForm_PatchCullDLIndex(Gfx*, size_t) {
}
void ApplyCustomCosmeticsToDisplayListCopy(const char*, Gfx*, size_t) {
}
#define gLinkZoraBarrierDL "__OTR__objects/object_link_zora/object_link_zora_DL_011760"

#include "zora_barrier_production.inc"

static std::vector<Gfx> NativeColors() {
    return {
        gsDPSetPrimColor(0, 128, 0, 150, 255, 130),   gsDPSetEnvColor(0, 0, 100, 255),   gsDPPipeSync(),
        gsDPSetPrimColor(0, 128, 170, 255, 255, 130), gsDPSetEnvColor(0, 150, 255, 255), gsSPEndDisplayList(),
    };
}

static void Draw(int intensity, bool swimming) {
    sGfx.polyXlu.p = sXlu;
    sPlay.state.gfxCtx = &sGfx;
    gFormState.barrierIntensity = intensity;
    gFormState.fastSwimActive = swimming;
    sScaleCalls = 0;
    sBarrierDLSafeCopy = NativeColors();
    sBarrierDLCount = sBarrierDLSafeCopy.size();
    MmForm_DrawZoraBarrier(&sPlayer, &sPlay);
}

int main() {
    // MM's model-space scale is intensity * 10/51 with actor scale 0.01.
    for (bool swimming : { false, true }) {
        for (int intensity : { 1, 50, 128, 255 }) {
            Draw(intensity, swimming);
            REQUIRE(sScaleCalls == 1);
            REQUIRE(fabsf(sScale - intensity * (10.0f / 51.0f) * 0.01f) < 0.00001f);
        }
    }
    Draw(0, false);
    REQUIRE(sScaleCalls == 0);
    puts("PASS native MM scale on land and in water, charge/fade, and inactive draw");

    auto original = NativeColors();
    auto commands = original;
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(memcmp(commands.data(), original.data(), original.size() * sizeof(Gfx)) == 0);
    for (int i = 0; i < 3; ++i) {
        sChanged[i] = true;
    }
    Draw(255, false);
    REQUIRE(sAllocated[0].words.w1 == 0x11223382);
    REQUIRE(sAllocated[1].words.w1 == 0x445566FF);
    REQUIRE(sAllocated[3].words.w1 == 0x77889982);
    REQUIRE(sAllocated[4].words.w1 == 0x112233FF);
    REQUIRE(sAllocated[0].words.w0 == original[0].words.w0);
    REQUIRE(sAllocated[2].words.w0 == original[2].words.w0);
    REQUIRE(memcmp(sBarrierDLSafeCopy.data(), original.data(), original.size() * sizeof(Gfx)) == 0);
    puts("PASS live main/glow/highlight colors preserve alpha, LOD, and cached source");

    // Hash payloads can look like color opcodes; they must stay byte-identical.
    Gfx hashHeader = {};
    hashHeader.words.w0 = 0x20000000;
    Gfx hashPayload = {};
    hashPayload.words.w0 = 0xFA112233;
    hashPayload.words.w1 = 0x44556677;
    commands = original;
    commands.insert(commands.begin(), { hashHeader, hashPayload });
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(commands[1].words.w0 == hashPayload.words.w0 && commands[1].words.w1 == hashPayload.words.w1);
    REQUIRE(commands[2].words.w1 == 0x11223382);
    // XML materials emit filepath calls as one command. The next color must
    // still be visited (the shield cosmetic O2R uses these split-list calls).
    for (u8 opcode : { 0x25, 0x27 }) {
        Gfx filepath = {};
        filepath.words.w0 = static_cast<uint32_t>(opcode) << 24;
        filepath.words.w1 = reinterpret_cast<uintptr_t>("objects/shield_layer");
        commands = original;
        commands.insert(commands.begin(), filepath);
        MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
        REQUIRE(commands[1].words.w1 == 0x11223382);
        REQUIRE(commands[0].words.w1 == filepath.words.w1);
    }
    auto unsupported = original;
    unsupported.insert(unsupported.begin(), gsDPSetEnvColor(20, 30, 40, 50));
    commands = unsupported;
    MmForm_PatchZoraBarrierColors(commands.data(), commands.size());
    REQUIRE(memcmp(commands.data(), unsupported.data(), commands.size() * sizeof(Gfx)) == 0);
    for (int i = 0; i < 3; ++i) {
        sChanged[i] = false;
    }
    Draw(255, false);
    REQUIRE(memcmp(sAllocated, original.data(), original.size() * sizeof(Gfx)) == 0);
    puts("PASS hash payloads, unfamiliar mod shaders, and reset to native commands");
}
