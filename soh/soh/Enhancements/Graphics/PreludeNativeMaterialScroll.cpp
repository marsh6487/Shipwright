#include "PreludeNativeMaterialScroll.h"
#include "NativeMaterialProfile.h"
#include <libultraship/libultra/types.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/type/DisplayList.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include "soh/cvar_prefixes.h"

extern "C" Gfx* Gfx_TwoTexScrollEx(GraphicsContext*, s32, u32, u32, s32, s32, s32, u32, u32, s32, s32, s32, s32, s32,
                                   s32);

namespace Prelude {
namespace {
// Cached resource instructions point only here, never into Graph_Alloc memory.
// Graph_Update finishes Play_Draw before Graph_ProcessGfxCommands runs; the
// buffers remain unchanged throughout that frame's interpolated render passes.
struct ScrollLists {
    std::array<std::array<Gfx, 12>, 4> lists{};
    ScrollLists() {
        for (auto& list : lists) {
            gSPEndDisplayList(list.data());
        }
    }
};
ScrollLists& Lists() {
    static ScrollLists lists;
    return lists;
}

using ProfileMap = std::map<std::string, NativeMaterialProfile>;
struct MetadataSnapshot {
    std::shared_ptr<std::vector<char>> bytes;
    ProfileMap profiles;
};
std::mutex sMetadataMutex;
std::map<std::weak_ptr<Ship::Archive>, MetadataSnapshot, std::owner_less<std::weak_ptr<Ship::Archive>>> sMetadata;

ProfileMap ReadProfiles(const std::vector<char>& bytes) {
    ProfileMap profiles;
    const auto root = nlohmann::json::parse(bytes.begin(), bytes.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("edits") || !root["edits"].is_object()) {
        return profiles;
    }
    for (const auto& edits : root["edits"]) {
        if (!edits.is_array()) {
            continue;
        }
        for (const auto& edit : edits) {
            if (!edit.is_object() || !edit.contains("data") || !edit["data"].is_object()) {
                continue;
            }
            for (const char* kind : { "pastes", "shapes" }) {
                const auto& data = edit["data"];
                auto items = data.find(kind);
                if (items == data.end() || !items->is_array()) {
                    continue;
                }
                for (const auto& item : *items) {
                    if (!item.is_object() || !item.contains("newDlPath") || !item["newDlPath"].is_string()) {
                        continue;
                    }
                    const std::string path = item["newDlPath"];
                    if (!path.starts_with("custom/prelude/")) {
                        continue;
                    }
                    auto profile = ResolveNativeMaterial(item, std::string_view(kind) == "pastes");
                    auto [it, inserted] = profiles.emplace(path, profile);
                    if (!inserted && it->second != profile) {
                        it->second = NativeMaterialProfile::None;
                    }
                }
            }
        }
    }
    return profiles;
}

NativeMaterialProfile ProfileFor(const std::shared_ptr<Ship::Archive>& archive, const std::string& path) {
    if (!archive || !archive->HasFile("prelude/project/edits.json")) {
        return NativeMaterialProfile::None;
    }
    // Archives can reopen without changing their shared_ptr identity. Compare
    // current metadata bytes on resource import, not filesystem timestamps or
    // texture hashes. Parsing is cached; no metadata I/O occurs during draws.
    auto file = archive->LoadFile("prelude/project/edits.json");
    if (!file || !file->Buffer) {
        return NativeMaterialProfile::None;
    }
    std::lock_guard lock(sMetadataMutex);
    std::erase_if(sMetadata, [](const auto& entry) { return entry.first.expired(); });
    auto& snapshot = sMetadata[archive];
    if (!snapshot.bytes || *snapshot.bytes != *file->Buffer) {
        snapshot.profiles = ReadProfiles(*file->Buffer);
        snapshot.bytes = file->Buffer;
    }
    auto profile = snapshot.profiles.find(path);
    return profile == snapshot.profiles.end() ? NativeMaterialProfile::None : profile->second;
}
} // namespace

NativeMaterialDisplayListFactory::NativeMaterialDisplayListFactory(std::shared_ptr<Ship::ArchiveManager> archives)
    : mArchives(std::move(archives)) {
}

std::shared_ptr<Ship::IResource>
NativeMaterialDisplayListFactory::ReadResource(std::shared_ptr<Ship::File> file,
                                               std::shared_ptr<Ship::ResourceInitData> initData) {
    Fast::ResourceFactoryBinaryDisplayListV0 originalFactory;
    auto resource = originalFactory.ReadResource(file, initData);
    if (!resource || !initData || !initData->Path.starts_with("custom/prelude/")) {
        return resource;
    }
    auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(resource);
    if (!dl || dl->UCode != ucode_f3dex2) {
        return resource;
    }
    // The ordinary loader does not currently populate Parent. Resolve the exact
    // DL's owning archive, not the globally highest-priority edits.json.
    auto archive = initData->Parent ? initData->Parent : mArchives->GetArchiveFromFile(initData->Path);
    auto profile = ProfileFor(archive, initData->Path);
    if (profile == NativeMaterialProfile::None) {
        return resource;
    }
    std::vector<NativeMaterialCommand> commands;
    commands.reserve(dl->Instructions.size());
    for (const auto& command : dl->Instructions) {
        commands.push_back({ command.words.w0, command.words.w1 });
    }
    auto insertion = FindNativeScrollInsertion(commands);
    if (!insertion) {
        SPDLOG_INFO("PreludeNativeMaterialScroll skipped unsupported or already-bound list {} profile={}",
                    initData->Path, static_cast<int>(profile));
        return resource;
    }
    Gfx call = gsSPDisplayList(Lists().lists[static_cast<size_t>(profile)].data());
    dl->Instructions.insert(dl->Instructions.begin() + *insertion, call);
    SPDLOG_INFO("PreludeNativeMaterialScroll prepared {} profile={} primitiveIndex={}", initData->Path,
                static_cast<int>(profile), *insertion);
    return resource;
}
} // namespace Prelude

extern "C" void PreludeNativeMaterialScroll_Update(GraphicsContext* gfxCtx, uint32_t stateFrames,
                                                   uint32_t gameplayFrames) {
    const bool enabled = CVarGetInteger(CVAR_ENHANCEMENT("PreludeNativeMaterialScroll"), 1) != 0;
    auto& lists = Prelude::Lists().lists;
    for (size_t i = 1; i < lists.size(); ++i) {
        if (!enabled || !gfxCtx) {
            gSPEndDisplayList(lists[i].data());
            continue;
        }
        const auto p = Prelude::NativeScrollParameters(static_cast<Prelude::NativeMaterialProfile>(i), stateFrames,
                                                       gameplayFrames);
        // Reuse native generator and its interpolation commands. Logical tile
        // dimensions are native scroll parameters, not replacement image sizes.
        const Gfx* native = Gfx_TwoTexScrollEx(gfxCtx, 0, p.x1, p.y1, p.width, p.height, 1, p.x2, p.y2, p.width,
                                               p.height, p.dx1, p.dy1, p.dx2, p.dy2);
        std::copy_n(native, lists[i].size(), lists[i].begin());
    }
}
