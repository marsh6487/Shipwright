#include "test_require.h"
#include "../soh/Enhancements/Graphics/NativeMaterialProfile.h"
#include <iostream>

using namespace Prelude;
using nlohmann::json;

int main() {
    auto lake = json::parse(
        R"({"chain":[{"path":"objects/object_spot06_objects/gLakeHyliaHighWaterDL","upto":248}],"placed":{"recipe":"water"}})");
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::LakeHylia);
    auto pool = lake;
    pool["chain"][0]["path"] = "objects/object_spot01_objects/gKakarikoWellWaterDL";
    REQUIRE(ResolveNativeMaterial(pool, true) == NativeMaterialProfile::Pool);
    // Current art, placement UUIDs and destination paths are not behavior identity.
    lake["texture"] = "fire-water-4096";
    lake["newDlPath"] = "custom/prelude/arbitrary_room/paste900";
    lake["placed"]["recipe"] = "a-new-recipe";
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::LakeHylia);
    auto light = json::parse(
        R"({"chain":[],"stored":{"textures":[{"label":"spot10_room_1Tex_008030"},{"label":"spot10_room_1Tex_008030"}],"texPaths":["custom/new-art","custom/second"]},"placed":{"recipe":"user:arbitrary"}})");
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::LostWoodsLightSheet);
    light["stored"]["textures"][0]["w"] = 4096;
    light["stored"]["textures"][0]["rgba"] = { { "$blob", 999 } };
    light["stored"]["texPaths"][0] = "a-different-replacement";
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::LostWoodsLightSheet);
    light["stored"]["textures"][1]["label"] = "unrelated";
    REQUIRE(ResolveNativeMaterial(light, false) == NativeMaterialProfile::None);
    lake["chain"].push_back(pool["chain"][0]);
    REQUIRE(ResolveNativeMaterial(lake, true) == NativeMaterialProfile::None);
    REQUIRE(ResolveNativeMaterial(json::object(), false) == NativeMaterialProfile::None);

    std::vector<NativeMaterialCommand> dl = { { 0x33000000, 0 },          { 0xde000000, 0x08000000 },
                                              { 0x20100000, 0 },          { 0xdeadbeef, 42 },
                                              { 0xf2000000, 0x007cc07c }, { 0xf2000000, 0x017cc07c },
                                              { 0x32001002, 0 },          { 0xdf000000, 55 },
                                              { 0x06000204, 0x00040600 }, { 0xdf000000, 0 } };
    REQUIRE(FindNativeScrollInsertion(dl) == 8); // DE and DF above are hash payloads.
    auto native = dl;
    native.insert(native.begin() + 8, { 0xde000000, 0x08000000 });
    REQUIRE(!FindNativeScrollInsertion(native));
    auto multi = dl;
    multi.insert(multi.end() - 1, { 0xf2000000, 0x007cc07c });
    REQUIRE(!FindNativeScrollInsertion(multi));
    auto empty = dl;
    empty.erase(empty.begin() + 8);
    REQUIRE(!FindNativeScrollInsertion(empty));
    auto truncated = dl;
    truncated.pop_back();
    REQUIRE(!FindNativeScrollInsertion(truncated));

    for (uint32_t f : { 0u, 1u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
        auto a = NativeScrollParameters(NativeMaterialProfile::LakeHylia, f, 77);
        auto b = NativeScrollParameters(NativeMaterialProfile::Pool, f, 77);
        auto c = NativeScrollParameters(NativeMaterialProfile::LostWoodsLightSheet, 77, f);
        REQUIRE(a.x1 == 0u - f && a.y1 == f && a.x2 == f && a.y2 == f);
        REQUIRE(b.x1 == 127 - f % 128 && b.y1 == (f & 127) && b.x2 == f % 128);
        REQUIRE(a.width == 32 && a.height == 32 && a.dx1 == -1 && a.dy1 == 1);
        REQUIRE(b.width == 32 && b.height == 32 && b.dx1 == -1 && b.dy1 == 1);
        REQUIRE(c.x1 == f % 128 && c.x2 == f % 128 && c.y1 == 0 && c.y2 == 0);
        REQUIRE(c.width == 32 && c.height == 16 && c.dx1 == 1 && c.dy1 == 0);
    }
    std::cout << "PASS native material identity, command safety, and native scroll parameters\n";
}
