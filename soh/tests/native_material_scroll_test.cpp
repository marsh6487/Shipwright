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
    auto declared = pool;
    declared["nativeAnimation"] = {{"version",1},{"source","mm.bg_keikoku_spr.lower_a"},
        {"binding","material-motion"},{"logicalWidth",32},{"logicalHeight",32}};
    REQUIRE(ResolveNativeMaterial(declared, true) == NativeMaterialProfile::FountainLowerA32);
    for (auto bad : {json(nullptr),json(true),json(1),json("x")}) {
        auto copy = declared; copy["nativeAnimation"] = bad;
        REQUIRE(ResolveNativeMaterial(copy,true) == NativeMaterialProfile::None);
    }
    for (const char* field : {"version","source","binding","logicalWidth","logicalHeight"}) {
        auto copy = declared; copy["nativeAnimation"].erase(field);
        REQUIRE(ResolveNativeMaterial(copy,true) == NativeMaterialProfile::None);
        for (auto bad : {json(nullptr),json(true),json(1.0),json("bad")}) {
            copy = declared; copy["nativeAnimation"][field]=bad;
            REQUIRE(ResolveNativeMaterial(copy,true) == NativeMaterialProfile::None);
        }
    }
    for (int role=0; role<3; ++role) {
        auto direct=pool;
        direct["chain"][0]["path"] = "objects/object_keikoku_obj/object_keikoku_obj_DL_000" +
            std::to_string(1+role*2)+"00";
        REQUIRE(static_cast<int>(ResolveNativeMaterial(direct,true)) == 7+role);
        direct["chain"].push_back(direct["chain"][0]);
        REQUIRE(ResolveNativeMaterial(direct,true) == NativeMaterialProfile::None);
        for (uint32_t f : {0u,1u,31u,32u,63u,64u,127u,128u,2047u,2048u,0xffffffffu}) {
            auto a=NativeScrollParameters(static_cast<NativeMaterialProfile>(4+role),999,f);
            auto b=NativeScrollParameters(static_cast<NativeMaterialProfile>(7+role),999,f);
            REQUIRE((a.y2%128)*2 == b.y2%256);
            REQUIRE(a.dy2*2 == b.dy2 && a.x1==0 && a.y1==0 && a.x2==0);
        }
    }
    std::vector<NativeMaterialCommand> fountain = {{0xf5101000,0x00014050},{0xf5101100,0x01014451},
        {0xf2000000,0x0007c07c},{0xf2000000,0x0107c07c},{0x32004008,0},{0xdf000000,55},
        {0x06000204,0x00000406},{0xdf000000,0}};
    const auto fp=NativeMaterialProfile::FountainLowerA32;
    REQUIRE(FindNativeScrollInsertion(fountain,fp)==6);
    fountain[1].w1=0x01017c5e; // Actual raised authored shifts, not zero-shift stand-ins.
    REQUIRE(FindNativeScrollInsertion(fountain,fp)==6);
    auto large=fountain;
    for (size_t i=0;i<2;++i) large[i].w1 ^= (3u<<14)|(3u<<4); // mask5 -> mask6, preserve shifts
    large[2].w1=0x000fc0fc;large[3].w1=0x010fc0fc;
    REQUIRE(FindNativeScrollInsertion(large,NativeMaterialProfile::FountainCentral64)==6);
    REQUIRE(!FindNativeScrollInsertion(large,fp));
    for (size_t i=0;i<4;++i) {
        auto bad=fountain; bad.erase(bad.begin()+i); REQUIRE(!FindNativeScrollInsertion(bad,fp));
        bad=fountain;bad.insert(bad.begin()+i,bad[i]); REQUIRE(!FindNativeScrollInsertion(bad,fp));
    }
    for (uintptr_t bits : {1u<<18,1u<<19,1u<<8,1u<<9,1u<<14,1u<<4}) {
        auto bad=fountain;bad[0].w1 ^= bits; REQUIRE(!FindNativeScrollInsertion(bad,fp));
    }
    auto bad=fountain;bad[2].w1^=4;REQUIRE(!FindNativeScrollInsertion(bad,fp));
    bad=fountain;bad.insert(bad.begin()+6,{0xde000000,0x08000000});REQUIRE(!FindNativeScrollInsertion(bad,fp));
    bad=fountain;bad.insert(bad.end()-1,{0xf5101000,0x00014050});REQUIRE(!FindNativeScrollInsertion(bad,fp));
    bad=fountain;bad.back().w1=1;REQUIRE(!FindNativeScrollInsertion(bad,fp));
    std::cout << "PASS native material identity, command safety, and native scroll parameters\n";
}
