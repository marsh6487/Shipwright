// Exercise the real renderer. Only the game/graphics boundary is replaced.
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
#include "soh/Enhancements/randomizer/NeiGiPresentation.cpp"
#include "variables.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Fixture {
int enabled, alt, loads, allocations, fallback, interpolation, vanilla, trap,
    triforce;
float matrix = 1;
float matrixY = 0;
std::vector<std::pair<float, float>> stack;
std::vector<std::pair<float, float>> submitted;
std::vector<std::vector<Vtx>> arena;
std::map<Gfx *, std::vector<Vtx>> vertexLoads;
std::set<std::string> files;
Gfx opa[4096], xlu[4096];
Mtx matrices[128];
GraphicsContext gfx{};
PlayState play{};
GetItemEntry shopEntry{};
void Reset() {
  enabled = alt = loads = allocations = fallback = interpolation = 0;
  vanilla = trap = triforce = 0;
  matrix = 1;
  matrixY = 0;
  submitted.clear();
  arena.clear();
  vertexLoads.clear();
  std::memset(opa, 0, sizeof(opa));
  std::memset(xlu, 0, sizeof(xlu));
  stack.clear();
  files.clear();
  gfx.polyOpa.p = opa;
  gfx.polyXlu.p = xlu;
  play.state.gfxCtx = &gfx;
  play.gameplayFrames = 42;
  play.billboardMtxF = {};
  play.billboardMtxF.xx = play.billboardMtxF.yy = play.billboardMtxF.zz = 1;
}
void Original() {
  ++fallback;
  Matrix_Scale(7, 7, 7, MTXMODE_APPLY);
}
std::vector<std::string> Drawn() {
  std::vector<std::string> paths;
  for (auto range :
       {std::pair(opa, gfx.polyOpa.p), std::pair(xlu, gfx.polyXlu.p)}) {
    for (Gfx *p = range.first; p != range.second; ++p) {
      if (((p->words.w0 >> 24) & 255) == G_DL_OTR_FILEPATH)
        paths.emplace_back(reinterpret_cast<const char *>(p->words.w1));
    }
  }
  return paths;
}
} // namespace Fixture

extern "C" {
uintptr_t gSegments[NUM_SEGMENTS];
GameInfo gameInfo{};
GameInfo *gGameInfo = &gameInfo;
void GetItem_Draw(PlayState *, s16) { ++Fixture::vanilla; }
void Player_DrawGetItemIceTrap(PlayState *, Player *, Vec3f *, s32, f32) {
  ++Fixture::trap;
}
void Randomizer_DrawTriforcePieceGI(PlayState *, GetItemEntry) {
  ++Fixture::triforce;
}
f32 Math_SinS(s16) { return 0; }
f32 Math_CosS(s16) { return 1; }
void Matrix_RotateZYX(s16, s16, s16, u8) {}
int32_t CVarGetInteger(const char *name, int32_t) {
  return std::strcmp(name, CVAR_NEI_GI_EFFECTS) == 0 ? Fixture::enabled : 0;
}
ShopItemIdentity Randomizer_IdentifyShopItem(s32, u8) { return {}; }
GetItemEntry
Randomizer_GetItemFromKnownCheckWithoutObtainabilityCheck(RandomizerCheck,
                                                          GetItemID) {
  return Fixture::shopEntry;
}
GetItemEntry GetItemMystery() { return {}; }
void EnItem00_CustomItemsParticles(Actor *, PlayState *, GetItemEntry) {}
void func_80A3C498(Actor *, PlayState *, s32) {}
uint8_t ResourceMgr_FileExists(const char *path) {
  return Fixture::files.contains(path);
}
uint8_t ResourceMgr_FileAltExists(const char *path) {
  return Fixture::files.contains(std::string("alt/") + path);
}
bool ResourceMgr_IsAltAssetsEnabled() { return Fixture::alt; }
Gfx *ResourceMgr_LoadGfxByName(const char *path) {
  ++Fixture::loads;
  return Fixture::files.contains(path)
             ? reinterpret_cast<Gfx *>(const_cast<char *>(path))
             : nullptr;
}
void Matrix_Push() {
  Fixture::stack.emplace_back(Fixture::matrix, Fixture::matrixY);
}
void Matrix_Pop() {
  assert(!Fixture::stack.empty());
  Fixture::matrix = Fixture::stack.back().first;
  Fixture::matrixY = Fixture::stack.back().second;
  Fixture::stack.pop_back();
}
void Matrix_Scale(float x, float, float, uint8_t) { Fixture::matrix *= x; }
void Matrix_RotateY(float, uint8_t) {}
void Matrix_RotateZ(float, uint8_t) {}
void Matrix_Translate(float, float y, float, uint8_t mode) {
  if (mode == MTXMODE_NEW) {
    Fixture::matrix = 1;
    Fixture::matrixY = y;
  } else
    Fixture::matrixY += y * Fixture::matrix;
}
void Matrix_ReplaceRotation(MtxF *) {}
void Matrix_Get(MtxF *m) {
  *m = {};
  m->xx = m->yy = m->zz = Fixture::matrix;
  m->yw = Fixture::matrixY;
}
void *Graph_Alloc(GraphicsContext *, size_t size) {
  assert(size <= 1536 * sizeof(Vtx) && size % sizeof(Vtx) == 0);
  Fixture::arena.emplace_back(size / sizeof(Vtx));
  return Fixture::arena.back().data();
}
Mtx *Matrix_NewMtx(GraphicsContext *, char *, int32_t) {
  Fixture::submitted.emplace_back(Fixture::matrix, Fixture::matrixY);
  return &Fixture::matrices[Fixture::allocations++];
}
void Gfx_SetupDL_25Opa(GraphicsContext *) {}
void Gfx_SetupDL_25Xlu(GraphicsContext *) {}
void Graph_OpenDisps(Gfx **, GraphicsContext *, const char *, int32_t) {}
void Graph_CloseDisps(Gfx **, GraphicsContext *, const char *, int32_t) {}
void FrameInterpolation_RecordOpenChild(const void *, int) {
  ++Fixture::interpolation;
}
void FrameInterpolation_RecordCloseChild() { --Fixture::interpolation; }
void gSPVertex(Gfx *cmd, uintptr_t data, int n, int v0) {
  assert(n > 0 && n + v0 <= 32);
  const auto *vertices = reinterpret_cast<const Vtx *>(data);
  Fixture::vertexLoads[cmd] = std::vector<Vtx>(vertices, vertices + n);
  cmd->words.w0 = cmd->words.w1 = 0;
}
void gSPDisplayList(Gfx *, Gfx *) {
  assert(false &&
         "GI paths must be deferred, not resolved through the legacy wrapper");
}
#define ORIGINAL(name)                                                         \
  void name(PlayState *, GetItemEntry *) { Fixture::Original(); }
ORIGINAL(Randomizer_DrawRocsFeatherSkijer)
ORIGINAL(Randomizer_DrawWhip)
ORIGINAL(Randomizer_DrawFireRod)
ORIGINAL(Randomizer_DrawIceRod)
ORIGINAL(Randomizer_DrawLightRod)
ORIGINAL(Randomizer_DrawDekuLeaf)
ORIGINAL(Randomizer_DrawSwitchHook)
ORIGINAL(Randomizer_DrawMogmaMitts)
ORIGINAL(Randomizer_DrawGustJar)
ORIGINAL(Randomizer_DrawBallAndChain)
ORIGINAL(Randomizer_DrawTimeGate)
ORIGINAL(Randomizer_DrawBeetle)
ORIGINAL(Randomizer_DrawShovel)
ORIGINAL(Randomizer_DrawHyliaGrace)
ORIGINAL(Randomizer_DrawZonaiPermafrost)
ORIGINAL(Randomizer_DrawDemiseDestruction)
ORIGINAL(Randomizer_DrawRocsCape)
ORIGINAL(Randomizer_DrawSpinner)
ORIGINAL(Randomizer_DrawBombArrows)
ORIGINAL(Randomizer_DrawCaneOfSomaria)
ORIGINAL(Randomizer_DrawDominionRod)
ORIGINAL(Randomizer_DrawMagnesis)
ORIGINAL(Randomizer_DrawStasis)
ORIGINAL(Randomizer_DrawLantern)
ORIGINAL(Randomizer_DrawCryonis)
ORIGINAL(UnrelatedDraw)
#undef ORIGINAL
#define gSPSegment(pkt, seg, target) ((void)(pkt))
#include "nei_gi_dispatch.inc"
#undef gSPSegment
}

int main() {
  using namespace Fixture;
  Reset();
  GetItemEntry entry{};
  assert(!NeiGi_Draw(nullptr, &entry));
  assert(!NeiGi_Draw(&play, nullptr));
  assert(!NeiGi_Draw(&play, &entry));
  entry.drawFunc = UnrelatedDraw;
  assert(!NeiGi_Draw(&play, &entry));
  assert(allocations == 0 && fallback == 0);

  // A missing model retains the original draw and restores its matrix edits.
  entry.drawFunc = Randomizer_DrawWhip;
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 1 && allocations == 0 && matrix == 1 && stack.empty());

  Reset();
  files.insert("__OTR__objects/nei_gi_redesign/whip/gi_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 0 && allocations == 1 && Drawn().size() == 1);
  assert(gfx.polyXlu.p ==
         xlu); // Disabled effects emit no translucent commands.
  assert(matrix == 1 && stack.empty() && interpolation == 0);
  assert(loads ==
         0); // Deferred paths must not evict/reload Alt resources per draw.
  enabled = 1;
  assert(NeiGi_Draw(&play, &entry));
  assert(allocations > 2 && allocations <= 6 && gfx.polyXlu.p > xlu);
  assert(matrix == 1 && stack.empty() && interpolation == 0);

  // Both spell passes are required; otherwise draw the intact original.
  Reset();
  entry.drawFunc = Randomizer_DrawDemiseDestruction;
  files.insert("__OTR__objects/nei_gi_redesign/demise_destruction/gi_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 1 && Drawn().empty());
  files.insert("__OTR__objects/nei_gi_redesign/demise_destruction/gi_xlu_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 1 && Drawn().size() == 2 && allocations == 4);
  assert(arena.size() == 2 &&
         arena.front().size() > 30); // Intrinsic energy, shimmer OFF.
  assert(Drawn()[1].ends_with("/gi_xlu_dl") && gfx.polyXlu.p > xlu);
  assert(matrix == 1 && stack.empty() && interpolation == 0);

  Reset();
  entry.drawFunc = Randomizer_DrawFireRod;
  files.insert("__OTR__objects/nei_gi_redesign/fire_rod/gi_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(allocations == 3 && arena.size() == 2 && arena.front().size() > 30);
  bool texture = false;
  for (Gfx *cmd = xlu; cmd != gfx.polyXlu.p; ++cmd) {
    if (((cmd->words.w0 >> 24) & 255) == G_SETTIMG) {
      texture = true;
      assert(((cmd->words.w0 >> 21) & 7) == G_IM_FMT_I);
      assert(cmd->words.w1 ==
             reinterpret_cast<uintptr_t>(
                 NeiGi::OrbTexture(NeiGi::Kind::Fire, play.gameplayFrames)
                     .data()));
    }
  }
  assert(texture); // Dense energy exists even with the checkbox disabled.
  enabled = 1;
  assert(NeiGi_Draw(&play, &entry));
  assert(allocations == 7 &&
         arena.size() == 5); // Orb + accents + independent shimmer.
  assert(matrix == 1 && stack.empty() && interpolation == 0 && loads == 0);

  Reset();
  alt = 1;
  entry.drawFunc = Randomizer_DrawWhip;
  files.insert("alt/__OTR__objects/nei_gi_redesign/whip/gi_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 0 && Drawn().size() == 1 && loads == 0);

  // Exercise the actual overhead animation entry point, not a surrogate
  // dispatch.
  Reset();
  Player player{};
  Vec3f ref{};
  player.getItemEntry.drawFunc = Randomizer_DrawWhip;
  files.insert("__OTR__objects/nei_gi_redesign/whip/gi_dl");
  Player_DrawGetItemImpl(&play, &player, &ref, 1);
  assert(fallback == 0 && Drawn().size() == 1);
  player.getItemEntry.drawFunc = nullptr;
  Player_DrawGetItemImpl(&play, &player, &ref, 1);
  assert(vanilla == 1);
  player.getItemEntry.modIndex = MOD_RANDOMIZER;
  player.getItemEntry.getItemId = RG_ICE_TRAP;
  Player_DrawGetItemImpl(&play, &player, &ref, 1);
  assert(trap == 1 && vanilla == 1);
  player.getItemEntry.getItemId = RG_TRIFORCE_PIECE;
  Player_DrawGetItemImpl(&play, &player, &ref, 1);
  assert(triforce == 1 && vanilla == 1);

  // The NEI feather must resolve to the exact same textured resource in shops,
  // freestanding common draws (including the house), and Link's overhead draw.
  std::vector<std::string> featherPaths;
  for (int route = 0; route < 3; ++route) {
    Reset();
    entry = {};
    entry.drawFunc = Randomizer_DrawRocsFeatherSkijer;
    files.insert("__OTR__objects/nei_gi_redesign/rocs_feather/gi_dl");
    if (route == 0) {
      GetItemEntry_Draw(&play, entry);
      featherPaths = Drawn();
    } else if (route == 1) {
      EnGirlA shop{};
      shop.actor.params = SI_RANDOMIZED_ITEM;
      shopEntry = entry;
      EnGirlA_Draw(&shop.actor, &play);
    } else {
      player = {};
      player.getItemEntry = entry;
      Player_DrawGetItemImpl(&play, &player, &ref, 1);
    }
    assert(Drawn() == featherPaths && Drawn().size() == 1 && fallback == 0);
    assert(stack.empty() && interpolation == 0 && loads == 0);
  }

  // Serialized model vertices and resource matrices must clear the shelf by 0.5
  // world units under EnGirlA's real .25 actor scale / 24 local-unit Y offset.
  struct Bounds {
    CustomDrawFunc draw;
    const char *slug;
    float low, high;
  };
  const Bounds bounds[] = {
#include "nei_gi_bounds.inc"
  };
  for (const auto &b : bounds) {
    Reset();
    entry.drawFunc = b.draw;
    files.insert(std::string("__OTR__objects/nei_gi_redesign/") + b.slug +
                 "/gi_dl");
    matrix = .25f;
    matrixY = 6;
    EnGirlA shop{};
    shop.actor.params = SI_RANDOMIZED_ITEM;
    shopEntry = entry;
    EnGirlA_Draw(&shop.actor, &play);
    const auto [scale, y] = submitted.front();
    assert(y + scale * b.low >= .5f);
    assert(scale * (b.high - b.low) <= 19.f);
    assert(matrix == .25f && matrixY == 6 && stack.empty());
    Reset();
    matrix = .25f;
    matrixY = 6;
    assert(NeiGi_DrawShop(&play, &entry));
    assert(fallback == 1 && matrix == .25f && matrixY == 6);
  }
  // Eight occupied potion-shop slots share one frame's XLU buffer. Keep at
  // least a third of its 4096 commands available to the room and other actors.
  Reset();
  enabled = 1;
  for (size_t i : {size_t(2), size_t(3), size_t(4), size_t(13), size_t(14),
                   size_t(15), size_t(8), size_t(10)}) {
    const auto &item = kPresentations[i];
    files.insert(item.opaque);
    if (item.translucent)
      files.insert(item.translucent);
    entry.drawFunc = item.draw;
    assert(NeiGi_DrawShop(&play, &entry));
  }
  assert(gfx.polyXlu.p - xlu < 2700);
  size_t vertexBytes = 0;
  for (const auto &vertices : arena)
    vertexBytes += vertices.size() * sizeof(Vtx);
  std::cout << "Potion shop: " << vertexBytes << " arena vertex bytes, "
            << gfx.polyXlu.p - xlu << " XLU commands\n"
            << std::flush;
  assert(vertexBytes < 80 * 1024 && allocations <= 32 && stack.empty());

  // Deku Leaf's optional shimmer is green, including glint centers and halos.
  Reset();
  enabled = 1;
  entry.drawFunc = Randomizer_DrawDekuLeaf;
  files.insert("__OTR__objects/nei_gi_redesign/deku_leaf/gi_dl");
  assert(NeiGi_Draw(&play, &entry) && arena.size() == 1);
  for (const auto &vertex : arena.front()) {
    assert(vertex.v.cn[1] > vertex.v.cn[0] && vertex.v.cn[1] > vertex.v.cn[2]);
  }

  // Decode the actual GPU commands after cache packing. Shared positions with
  // different alpha/color must stay distinct, including across cache reloads.
  Reset();
  NeiGi::Mesh mesh;
  for (int i = 0; i < 40; ++i) {
    const NeiGi::EffectVertex a{{float(i), 0, 0}, 0xFF2020, 64};
    const NeiGi::EffectVertex b{{float(i + 1), 0, 0}, 0xFF2020, 64};
    const NeiGi::EffectVertex c{{float(i), 1, 0}, 0x20FF20, 128};
    const NeiGi::EffectVertex d{{float(i + 1), 1, 0}, 0x20FF20, 128};
    mesh.Tri(a, b, c);
    mesh.Tri(b, d, c);
  }
  mesh.Tri({{0, 0, 0}, 0x0000FF, 255}, {{1, 0, 0}, 0x0000FF, 255},
           {{0, 1, 0}, 0x0000FF, 255});
  // Same position/color but different UVs must not collapse at texture seams.
  mesh.Tri({{0, 0, 0}, 0x0000FF, 255, 1, 1}, {{1, 0, 0}, 0x0000FF, 255, 0, 1},
           {{0, 1, 0}, 0x0000FF, 255, 1, 0});
  NeiGi_DrawMesh(&play, mesh);
  std::vector<Vtx> cache;
  size_t decoded = 0;
  auto triangle = [&](uintptr_t word) {
    for (int shift : {16, 8, 0}) {
      const size_t index = ((word >> shift) & 255) / 2;
      assert(index < cache.size() && decoded < mesh.count);
      const auto &got = cache[index].v;
      const auto &want = mesh.vertices[decoded++];
      assert(got.ob[0] == want.p.x * 16 && got.ob[1] == want.p.y * 16 &&
             got.ob[2] == want.p.z * 16);
      assert(got.cn[0] == (want.rgb >> 16) &&
             got.cn[1] == ((want.rgb >> 8) & 255) &&
             got.cn[2] == (want.rgb & 255) && got.cn[3] == want.alpha);
      assert(got.tc[0] == std::lround(want.u * 63 * 32) &&
             got.tc[1] == std::lround(want.v * 63 * 32));
    }
  };
  for (Gfx *cmd = xlu; cmd != gfx.polyXlu.p; ++cmd) {
    if (auto it = vertexLoads.find(cmd); it != vertexLoads.end())
      cache = it->second;
    const auto op = (cmd->words.w0 >> 24) & 255;
    if (op == G_TRI1 || op == G_TRI2)
      triangle(cmd->words.w0);
    if (op == G_TRI2)
      triangle(cmd->words.w1);
  }
  assert(decoded == mesh.count && vertexLoads.size() > 1);
  std::cout << "NEI production renderer: fallback, OPA/XLU, disabled effects, "
               "Alt paths, and matrix balance passed\n";
  return 0;
}
