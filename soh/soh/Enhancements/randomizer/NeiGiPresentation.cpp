#include "NeiGiPresentation.h"
#include "NeiGiEffectPolicy.h"
#include "draw.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/frame_interpolation.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
}

namespace {
using NeiGi::Kind;
struct Presentation {
    CustomDrawFunc draw;
    const char* opaque;
    const char* translucent;
    float scale;
    Kind effect;
    Vec3f effectCenter;
};

#define GI_PATH(slug) "__OTR__objects/nei_gi_redesign/" slug "/gi_dl"
#define GI_XLU(slug) "__OTR__objects/nei_gi_redesign/" slug "/gi_xlu_dl"
// These bindings are for presentation only. Actor and held-item resources retain their own paths.
const Presentation kPresentations[] = {
    { Randomizer_DrawRocsFeatherSkijer, GI_PATH("rocs_feather"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawWhip, GI_PATH("whip"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawFireRod, GI_PATH("fire_rod"), nullptr, .2f, Kind::Fire, { 9.883f, 30.415f, 0 } },
    { Randomizer_DrawIceRod, GI_PATH("ice_rod"), nullptr, .2f, Kind::Ice, { 10.365f, 31.899f, 0 } },
    { Randomizer_DrawLightRod, GI_PATH("light_rod"), nullptr, .2f, Kind::Light, { 9.883f, 30.415f, 0 } },
    { Randomizer_DrawDekuLeaf, GI_PATH("deku_leaf"), nullptr, .5f, Kind::Leaf, {} },
    { Randomizer_DrawSwitchHook, GI_PATH("switch_hook"), nullptr, .01f, Kind::Neutral, {} },
    { Randomizer_DrawMogmaMitts, GI_PATH("mogma_mitts"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawGustJar, GI_PATH("gust_jar"), nullptr, 5.f, Kind::Neutral, {} },
    { Randomizer_DrawBallAndChain, GI_PATH("ball_and_chain"), nullptr, .25f, Kind::Neutral, {} },
    { Randomizer_DrawTimeGate, GI_PATH("time_gate"), nullptr, .5f, Kind::Neutral, {} },
    { Randomizer_DrawBeetle, GI_PATH("beetle"), nullptr, .3f, Kind::Neutral, {} },
    { Randomizer_DrawShovel, GI_PATH("shovel"), nullptr, .2f, Kind::Neutral, {} },
    { Randomizer_DrawHyliaGrace, GI_PATH("hylia_grace"), GI_XLU("hylia_grace"), 1.f, Kind::Hylia, {} },
    { Randomizer_DrawZonaiPermafrost, GI_PATH("zonai_permafrost"), GI_XLU("zonai_permafrost"), 1.f, Kind::Zonai, {} },
    { Randomizer_DrawDemiseDestruction,
      GI_PATH("demise_destruction"),
      GI_XLU("demise_destruction"),
      1.f,
      Kind::Demise,
      {} },
    { Randomizer_DrawRocsCape, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawSpinner, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawBombArrows, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawCaneOfSomaria, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawDominionRod, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawMagnesis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawStasis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawLantern, nullptr, nullptr, 1.f, Kind::Neutral, {} },
    { Randomizer_DrawCryonis, nullptr, nullptr, 1.f, Kind::Neutral, {} },
};
#undef GI_PATH
#undef GI_XLU

// Small static billboard meshes. No actors, random-number calls, or persistent particle state.
Vtx kSparkle[] = {
    { { { 0, 3, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -1, 1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -3, 0, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -1, -1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -3, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 1, -1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 3, 0, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 1, 1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, 0, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
};
Vtx kShard[] = {
    { { { 0, 3, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -1, 0, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 1, 0, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
};
Vtx kLeaf[] = {
    { { { 0, 3, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -2, 1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { -1, -2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -3, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 2, -1, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 1, 2, 0 }, 0, { 0, 0 }, { 255, 255, 255, 255 } } },
};

float Spin(PlayState* play) {
    // Match DrawCustomItemDiamond's signed 16-bit rotation, including wrap.
    const uint32_t bits = (static_cast<uint32_t>(play->gameplayFrames) * 2u) & 0xFFFFu;
    const int32_t signedBits = bits >= 0x8000u ? static_cast<int32_t>(bits) - 0x10000 : bits;
    return signedBits * .01f;
}

bool HasResource(const char* path) {
    return path != nullptr &&
           (ResourceMgr_FileExists(path) || (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(path)));
}
} // namespace

// OPEN_DISPS declares interpolation callbacks with the enclosing C linkage.
extern "C" {
static void NeiGi_DrawEffects(PlayState* play, const Presentation& item, bool upgraded) {
    if (!CVarGetInteger(CVAR_NEI_GI_EFFECTS, 0))
        return;
    // Old rod models have different tip origins. A missing replacement gets a neutral
    // origin-centred shimmer instead of incorrectly positioned replacement-tip particles.
    const bool missingRod =
        !upgraded && (item.effect == Kind::Fire || item.effect == Kind::Ice || item.effect == Kind::Light);
    const auto frame = NeiGi::Sample(missingRod ? Kind::Neutral : item.effect, play->gameplayFrames, true);
    const Vec3f center = missingRod ? Vec3f{} : item.effectCenter;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++,
                         G_LIGHTING | G_CULL_BACK | G_CULL_FRONT | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG);
    gSPTexture(POLY_XLU_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    Matrix_Push();
    Matrix_RotateY(Spin(play), MTXMODE_APPLY);
    for (size_t i = 0; i < frame.count; ++i) {
        const auto& p = frame.particles[i];
        if (p.alpha == 0)
            continue;
        Matrix_Push();
        Matrix_Translate(center.x + p.x, center.y + p.y, center.z + p.z, MTXMODE_APPLY);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        Matrix_RotateZ(p.angle, MTXMODE_APPLY);
        // Vertex meshes span ±3; sample.size denotes the final radius, not a multiplier.
        Matrix_Scale(p.size / 3.f, p.size / 3.f, p.size / 3.f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (p.rgb >> 16) & 255, (p.rgb >> 8) & 255, p.rgb & 255, p.alpha);
        if (p.shape == NeiGi::Shape::Sparkle) {
            gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(kSparkle), 9, 0);
            for (int j = 0; j < 8; j += 2)
                gSP2Triangles(POLY_XLU_DISP++, 8, j, j + 1, 0, 8, j + 1, (j + 2) % 8, 0);
        } else if (p.shape == NeiGi::Shape::Leaf) {
            gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(kLeaf), 6, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 3, 4, 0, 0, 4, 5, 0);
        } else {
            gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(kShard), 4, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
        Matrix_Pop();
    }
    Matrix_Pop();
    // Restore the standard translucent pipeline for the next GI/world draw.
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}
}

extern "C" bool NeiGi_Draw(PlayState* play, GetItemEntry* entry) {
    if (play == nullptr || entry == nullptr || entry->drawFunc == nullptr)
        return false;
    const Presentation* item = nullptr;
    for (const auto& candidate : kPresentations) {
        if (candidate.draw == entry->drawFunc) {
            item = &candidate;
            break;
        }
    }
    if (item == nullptr)
        return false;
    // Queue stable paths for the interpreter. Loading through the legacy GBI wrapper
    // here would evict/reload base resources on each draw when Alt Assets is enabled.
    // Archive presence checks preserve the original model if a required pass is absent.
    const bool upgraded = HasResource(item->opaque) && (!item->translucent || HasResource(item->translucent));
    Matrix_Push();
    if (upgraded) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Scale(item->scale, item->scale, item->scale, MTXMODE_APPLY);
        Matrix_RotateY(Spin(play), MTXMODE_APPLY);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gDma1p(POLY_OPA_DISP++, G_DL_OTR_FILEPATH, item->opaque, 0, G_DL_PUSH);
        if (item->translucent != nullptr) {
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                      G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
            gDma1p(POLY_XLU_DISP++, G_DL_OTR_FILEPATH, item->translucent, 0, G_DL_PUSH);
        }
        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        entry->drawFunc(play, entry);
    }
    Matrix_Pop();
    NeiGi_DrawEffects(play, *item, upgraded);
    return true;
}
