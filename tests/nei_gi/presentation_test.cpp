// Exercise the real renderer. Only the game/graphics boundary is replaced.
#include "soh/Enhancements/randomizer/NeiGiPresentation.cpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace Fixture {
int enabled, alt, loads, allocations, fallback, interpolation;
float matrix = 1;
std::vector<float> stack;
std::set<std::string> files;
Gfx opa[1024], xlu[1024];
Mtx matrices[128];
GraphicsContext gfx{};
PlayState play{};
void Reset() {
  enabled = alt = loads = allocations = fallback = interpolation = 0;
  matrix = 1;
  stack.clear();
  files.clear();
  gfx.polyOpa.p = opa;
  gfx.polyXlu.p = xlu;
  play.state.gfxCtx = &gfx;
  play.gameplayFrames = 42;
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
int32_t CVarGetInteger(const char *, int32_t) { return Fixture::enabled; }
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
void Matrix_Push() { Fixture::stack.push_back(Fixture::matrix); }
void Matrix_Pop() {
  assert(!Fixture::stack.empty());
  Fixture::matrix = Fixture::stack.back();
  Fixture::stack.pop_back();
}
void Matrix_Scale(float x, float, float, uint8_t) { Fixture::matrix *= x; }
void Matrix_RotateY(float, uint8_t) {}
void Matrix_RotateZ(float, uint8_t) {}
void Matrix_Translate(float, float, float, uint8_t) {}
void Matrix_ReplaceRotation(MtxF *) {}
Mtx *Matrix_NewMtx(GraphicsContext *, char *, int32_t) {
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
void gSPVertex(Gfx *, uintptr_t, int n, int v0) {
  assert(n > 0 && n + v0 <= 32);
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
  assert(fallback == 1 && Drawn().size() == 2 && allocations == 2);
  assert(Drawn()[1].ends_with("/gi_xlu_dl") && gfx.polyXlu.p > xlu);
  assert(matrix == 1 && stack.empty() && interpolation == 0);

  Reset();
  alt = 1;
  entry.drawFunc = Randomizer_DrawWhip;
  files.insert("alt/__OTR__objects/nei_gi_redesign/whip/gi_dl");
  assert(NeiGi_Draw(&play, &entry));
  assert(fallback == 0 && Drawn().size() == 1 && loads == 0);
  std::cout << "NEI production renderer: fallback, OPA/XLU, disabled effects, "
               "Alt paths, and matrix balance passed\n";
}
