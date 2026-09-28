// Execute the production C ABI wrapper. Only the engine matrix/render boundary
// is replaced; samplers and dispatch run unchanged.
#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.cpp"
#include <cassert>
#include <iostream>
#include <vector>

namespace Fixture {
struct Pose { Vec3f position{}; float scale = 1; };
Pose pose;
std::vector<Pose> stack, draws;
size_t vertices = 0;
bool textures = false;
size_t texturedVertices = 0;
}
extern "C" {
CustomItemState gCustomItemState{};
uint8_t ResourceMgr_FileExists(const char*) { return Fixture::textures; }
uint8_t ResourceMgr_FileAltExists(const char*) { return 0; }
bool ResourceMgr_IsAltAssetsEnabled() { return false; }
void Matrix_MultVec3f(Vec3f* p, Vec3f* out) {
    *out = {Fixture::pose.position.x + p->x * Fixture::pose.scale,
            Fixture::pose.position.y + p->y * Fixture::pose.scale,
            Fixture::pose.position.z + p->z * Fixture::pose.scale};
}
void Matrix_Push() { Fixture::stack.push_back(Fixture::pose); }
void Matrix_Pop() {
    assert(!Fixture::stack.empty()); Fixture::pose = Fixture::stack.back(); Fixture::stack.pop_back();
}
void Matrix_Translate(float x, float y, float z, uint8_t mode) {
    assert(mode == MTXMODE_NEW); Fixture::pose = {{x,y,z}, 1};
}
void Matrix_Scale(float x, float y, float z, uint8_t mode) {
    assert(mode == MTXMODE_APPLY && x == y && y == z); Fixture::pose.scale *= x;
}
NeiGi::Basis NeiGi_CameraBasis(PlayState*) { return {}; }
void NeiGi_DrawMesh(PlayState*, const NeiGi::Mesh& mesh, NeiGi::Kind kind) {
    assert(kind == NeiGi::Kind::Neutral); // No resource lookup or Alt-owned texture pointer.
    if (!mesh.count) return;
    assert(mesh.count < mesh.vertices.size());
    Fixture::draws.push_back(Fixture::pose); Fixture::vertices += mesh.count;
}
bool NeiGi_DrawTexturedMesh(PlayState*, const NeiGi::Mesh& mesh, const NeiGi::TextureMaterial& material) {
    assert(material.path && mesh.count < mesh.vertices.size());
    if(Fixture::textures)Fixture::texturedVertices += mesh.count;
    return Fixture::textures;
}
}
int main() {
    PlayState play{}; Player player{};
    player.actor.world.pos = {100, 200, 300}; player.meleeWeaponInfo[0].tip = {110, 250, 310};
    Vec3f velocity{18,0,0}, trail[6] = {{100,200,300},{82,200,300},{64,200,300},
                                      {46,200,300},{28,200,300},{10,200,300}};
    auto restored = [] { assert(Fixture::pose.position.x == 7 && Fixture::pose.scale == 3 && Fixture::stack.empty()); };
    Fixture::pose = {{7,8,9}, 3};
    NeiUsedMagic_DrawProjectile(&play, 2, &trail[0], &velocity, 2, 0); restored();
    assert(Fixture::draws.back().position.x == 100);
    NeiUsedMagic_DrawTrail(&play, 1, trail, 6, 2); restored();
    const auto before = Fixture::draws.size();
    NeiUsedMagic_DrawCharge(&play, &player, 0, 1); restored();
    assert(Fixture::draws.size() == before + 1);
    assert(Fixture::draws[before].position.y == 205);
    gCustomItemState.fireRodCharging = 1; gCustomItemState.fireRodChargeLevel = 1;
    NeiUsedMagic_DrawChargeFocus(&play, 0); restored();
    assert(Fixture::draws.back().position.x == 7 && Fixture::draws.back().position.y == 8);
    // The focus is exactly the current held-model matrix origin, not melee tip.
    assert(Fixture::draws.back().position.y != player.meleeWeaponInfo[0].tip.y);
    NeiUsedMagic_DrawSpin(&play, &player, 1, 500, true); restored();
    NeiUsedMagic_DrawBurst(&play, 2, &trail[0], 1.4f, .5f); restored();
    NeiUsedMagic_DrawPortal(&play, &player, 1, 255); restored();
    assert(Fixture::draws.back().position.y == 201);
    const auto valid = Fixture::draws.size();
    NeiUsedMagic_DrawProjectile(nullptr, 2, &trail[0], &velocity, 2, 0);
    NeiUsedMagic_DrawProjectile(&play, 0, &trail[0], &velocity, 2, 0);
    NeiUsedMagic_DrawProjectile(&play, 8, &trail[0], &velocity, 2, 0);
    NeiUsedMagic_DrawTrail(&play, 2, nullptr, 6, 2);
    NeiUsedMagic_DrawTrail(&play, 2, trail, 500, 2); // Reject invalid array length, never over-read.
    NeiUsedMagic_DrawCharge(&play, nullptr, 2, 1);
    NeiUsedMagic_DrawPortal(&play, &player, 1, 0);
    NeiUsedMagic_DrawPortal(&play, &player, 0, 255);
    restored(); assert(Fixture::draws.size() == valid);
    Fixture::textures=true; Fixture::vertices=Fixture::texturedVertices=0;
    NeiUsedMagic_DrawProjectile(&play,1,&trail[0],&velocity,2,0);restored();
    const auto complete=NeiUsedMagic::SampleProjectile(NeiGi::Kind::Ice,0,2,{18,0,0});
    assert(Fixture::texturedVertices>0 && Fixture::vertices>0);
    assert(Fixture::vertices+Fixture::texturedVertices==complete.count);
    // Textured faces replace exactly their geometry fallback, never overlap it.
    std::cout << "USED VFX production dispatch: C ABI, matrix restoration, world placement and empty-state guards passed\n";
}
