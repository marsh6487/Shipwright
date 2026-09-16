#include "NativeMaterialProfile.h"
namespace Prelude {
NativeMaterialProfile ResolveNativeMaterial(const nlohmann::json& item, bool pasted) {
    if (!item.is_object()) {
        return NativeMaterialProfile::None;
    }
    if (item.contains("nativeAnimation")) {
        const auto& a = item["nativeAnimation"];
        if (!a.is_object() || a.size() != 5 || !a.contains("version") || !a["version"].is_number_integer() ||
            a["version"] != 1 || !a.contains("binding") || a["binding"] != "material-motion" || !a.contains("source") ||
            !a["source"].is_string() || !a.contains("logicalWidth") || !a["logicalWidth"].is_number_integer() ||
            !a.contains("logicalHeight") || !a["logicalHeight"].is_number_integer() ||
            a["logicalWidth"] != a["logicalHeight"]) {
            return NativeMaterialProfile::None;
        }
        const bool small = a["logicalWidth"] == 32;
        if (!small && a["logicalWidth"] != 64)
            return NativeMaterialProfile::None;
        if (a["source"] == "mm.bg_keikoku_spr.lower_a")
            return small ? NativeMaterialProfile::FountainLowerA32 : NativeMaterialProfile::FountainLowerA64;
        if (a["source"] == "mm.bg_keikoku_spr.lower_b")
            return small ? NativeMaterialProfile::FountainLowerB32 : NativeMaterialProfile::FountainLowerB64;
        if (a["source"] == "mm.bg_keikoku_spr.central")
            return small ? NativeMaterialProfile::FountainCentral32 : NativeMaterialProfile::FountainCentral64;
        return NativeMaterialProfile::None;
    }
    if (pasted) {
        auto chain = item.find("chain");
        if (chain == item.end() || !chain->is_array() || chain->size() != 1 || !(*chain)[0].is_object()) {
            return NativeMaterialProfile::None;
        }
        auto path = (*chain)[0].find("path");
        if (path == (*chain)[0].end() || !path->is_string()) {
            return NativeMaterialProfile::None;
        }
        if (*path == "objects/object_keikoku_obj/object_keikoku_obj_DL_000100")
            return NativeMaterialProfile::FountainLowerA64;
        if (*path == "objects/object_keikoku_obj/object_keikoku_obj_DL_000300")
            return NativeMaterialProfile::FountainLowerB64;
        if (*path == "objects/object_keikoku_obj/object_keikoku_obj_DL_000500")
            return NativeMaterialProfile::FountainCentral64;
        if (*path == "objects/object_spot06_objects/gLakeHyliaHighWaterDL") {
            return NativeMaterialProfile::LakeHylia;
        }
        if (*path == "objects/object_spot01_objects/gKakarikoWellWaterDL") {
            return NativeMaterialProfile::Pool;
        }
    } else {
        // These are the original material labels stored by the recipe, NOT the
        // resource names/images currently resolved by the texture manager.
        auto stored = item.find("stored");
        if (stored == item.end() || !stored->is_object()) {
            return NativeMaterialProfile::None;
        }
        auto textures = stored->find("textures");
        if (textures == stored->end() || !textures->is_array() || textures->size() != 2) {
            return NativeMaterialProfile::None;
        }
        for (const auto& texture : *textures) {
            if (!texture.is_object()) {
                return NativeMaterialProfile::None;
            }
            auto label = texture.find("label");
            if (label == texture.end() || *label != "spot10_room_1Tex_008030") {
                return NativeMaterialProfile::None;
            }
        }
        return NativeMaterialProfile::LostWoodsLightSheet;
    }
    return NativeMaterialProfile::None;
}

std::optional<size_t> FindNativeScrollInsertion(const std::vector<NativeMaterialCommand>& commands,
                                                NativeMaterialProfile profile) {
    const bool fountain = profile >= NativeMaterialProfile::FountainLowerA32 && profile < NativeMaterialProfile::Count;
    const unsigned dimension = profile >= NativeMaterialProfile::FountainLowerA64 ? 64 : 32;
    const unsigned mask = dimension == 64 ? 6 : 5;
    unsigned descriptors = 0;
    std::optional<size_t> firstPrimitive;
    unsigned tiles = 0;
    for (size_t i = 0; i < commands.size(); ++i) {
        auto opcode = static_cast<uint8_t>(commands[i].w0 >> 24);
        if (opcode == 0xdf) { // F3DEX2 ENDDL
            return i + 1 == commands.size() && tiles == 3 &&
                           (!fountain || (descriptors == 3 && commands[i].w0 == 0xdf000000 && commands[i].w1 == 0))
                       ? firstPrimitive
                       : std::nullopt;
        }
        if (opcode == 0x05 || opcode == 0x06 || opcode == 0x07 || opcode == 0x49) {
            if (!firstPrimitive) {
                firstPrimitive = i;
            }
            continue;
        }
        // Preserve hash payloads as data, never reinterpret their high bytes.
        if (opcode == 0x32 || opcode == 0x33 || opcode == 0x20) {
            if (i + 1 >= commands.size() || (firstPrimitive && opcode != 0x32)) {
                return std::nullopt;
            }
            ++i;
            continue;
        }
        if (opcode == 0x01 || opcode == 0x48) { // vertex load
            continue;
        }
        if (firstPrimitive) {
            return std::nullopt; // Another material or control flow: out of scope.
        }
        switch (opcode) {
            case 0xf2: { // static tile sizes must initialize both layers
                auto tile = (commands[i].w1 >> 24) & 7;
                if (tile > 1) {
                    return std::nullopt;
                }
                if (fountain &&
                    ((tiles & (1u << tile)) || commands[i].w0 != 0xf2000000 ||
                     commands[i].w1 != ((tile << 24) | (((dimension - 1) * 4) << 12) | ((dimension - 1) * 4))))
                    return std::nullopt;
                tiles |= 1u << tile;
                break;
            }
            case 0xf5: {
                if (!fountain)
                    break;
                const auto word = commands[i].w1;
                const unsigned tile = (word >> 24) & 7;
                if (tile == 7)
                    break; // separate load tile
                if (tile > 1 || (descriptors & (1u << tile)) || ((word >> 18) & 3) || ((word >> 8) & 3) ||
                    ((word >> 14) & 15) != mask || ((word >> 4) & 15) != mask)
                    return std::nullopt;
                descriptors |= 1u << tile;
                break;
            }
            case 0xe7:
            case 0xe8:
            case 0xe6: // pipe/tile/load sync
            case 0xe3:
            case 0xe2:
            case 0xd7:
            case 0xd9: // modes/texture/geometry
            case 0xf3:
            case 0xfc:
            case 0xfa:
            case 0xfb:
                break;
            default:
                // In particular: no DL, segmented DL, branch, matrix, or segment
                // writes. Already animated/relocated native materials are untouched.
                return std::nullopt;
        }
    }
    return std::nullopt;
}

ScrollParameters NativeScrollParameters(NativeMaterialProfile profile, uint32_t stateFrames, uint32_t gameplayFrames) {
    if (profile >= NativeMaterialProfile::FountainLowerA32 && profile < NativeMaterialProfile::Count) {
        const int index = static_cast<int>(profile) - static_cast<int>(NativeMaterialProfile::FountainLowerA32);
        const int dimension = index < 3 ? 32 : 64;
        const int rate = (index % 3 == 0 ? -20 : index % 3 == 1 ? 20 : 10) * (dimension / 32);
        return { 0, 0, 0, gameplayFrames * static_cast<uint32_t>(rate), dimension, dimension, 0, 0, 0, rate };
    }
    switch (profile) {
        case NativeMaterialProfile::LakeHylia:
            // BgSpot06Objects_DrawLakeHyliaWater, segment 08 (not its segment 09).
            return { 0u - stateFrames, stateFrames, stateFrames, stateFrames, 32, 32, -1, 1, 1, 1 };
        case NativeMaterialProfile::Pool:
            // BgSpot01Idomizu_Draw. Native differs in phase, not scroll increment.
            return {
                127 - stateFrames % 128, stateFrames & 127, stateFrames % 128, stateFrames & 127, 32, 32, -1, 1, 1, 1
            };
        case NativeMaterialProfile::LostWoodsLightSheet:
            // Scene draw config 9 / func_8009EE44, segment 08.
            return { gameplayFrames % 128, 0, gameplayFrames % 128, 0, 32, 16, 1, 0, 1, 0 };
        default:
            return {};
    }
}
} // namespace Prelude
