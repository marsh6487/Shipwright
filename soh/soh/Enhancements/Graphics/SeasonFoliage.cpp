#include "SeasonFoliageTint.h"
#include "soh/Enhancements/audio/SeasonWeather.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <map>
#include <string>
#include <tuple>

extern PlayState* gPlayState;
extern "C" void gfx_texture_cache_clear();

namespace {
using namespace SeasonFoliage;
struct Material {
    const char* path;
    Surface surface;
};
constexpr Material kMaterials[] = {
#include "SeasonFoliageMaterials.inc"
};

struct Binding {
    std::weak_ptr<Ship::ResourceManager> owner;
    std::string path;
    std::shared_ptr<Fast::Texture> source;
    std::shared_ptr<Fast::Texture> copy;
};
std::vector<Binding> sBindings;
// Retain original/copy allocations for queued renderer pointers and GPU cache
// keys, including when a different scene or season stops using a texture.
std::map<std::tuple<const Fast::Texture*, uint32_t, unsigned>, Binding> sCopies;

void Restore(const Binding& binding) {
    if (auto rm = binding.owner.lock(); rm && rm->GetCachedResource(binding.path, true) == binding.copy)
        rm->CacheExternalResource(binding.path, binding.source);
}

void Reset() {
    for (const auto& binding : sBindings)
        Restore(binding);
    sBindings.clear();
}

void Retire() {
    Reset();
    // The incoming scene no longer submits the outgoing texture cohort.
    // LUS uses image pointers as GPU cache keys: evict before freeing them.
    if (!sCopies.empty())
        gfx_texture_cache_clear();
    sCopies.clear();
}

void Update(const PlayState* play) {
    const int season = SeasonWeather_SeasonForPlay(play);
    auto* context = Ship::Context::GetRawInstance();
    auto rm = context ? context->GetResourceManager() : nullptr;
    if (!rm || (season != 0 && season != 2 && season != 3)) {
        Reset();
        return;
    }
    std::vector<Binding> active;
    for (const auto& material : kMaterials) {
        const auto tint = Palette(season, material.surface);
        if (!tint.color)
            continue;
        for (const char* prefix : { "", "alt/" }) {
            if (*prefix && !rm->IsAltAssetsEnabled())
                continue;
            const std::string path = std::string(prefix) + material.path;
            auto source = std::dynamic_pointer_cast<Fast::Texture>(rm->GetCachedResource(path, true));
            if (!source)
                continue; // Unloaded rooms remain on the normal renderer load path.
            const auto current = std::find_if(sBindings.begin(), sBindings.end(), [&](const auto& binding) {
                return binding.owner.lock() == rm && binding.path == path && binding.copy == source;
            });
            if (current != sBindings.end())
                source = current->source;
            const auto key = std::tuple{ source.get(), tint.color, tint.lightScale };
            auto found = sCopies.find(key);
            if (found == sCopies.end()) {
                auto copy = BuildTexture(source, tint);
                if (!copy)
                    continue;
                found = sCopies.emplace(key, Binding{ rm, path, source, std::move(copy) }).first;
            }
            if (current == sBindings.end() || current->copy != found->second.copy)
                rm->CacheExternalResource(path, found->second.copy);
            active.push_back({ rm, path, source, found->second.copy });
        }
    }
    for (const auto& binding : sBindings) {
        if (std::none_of(active.begin(), active.end(), [&](const auto& next) {
                return next.owner.lock() == binding.owner.lock() && next.path == binding.path &&
                       next.copy == binding.copy;
            }))
            Restore(binding); // Never overwrite a reload or another mod's cache replacement.
    }
    sBindings = std::move(active);
}

void Register() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([]() { Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>(Reset);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { Retire(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { Retire(); });
}
static RegisterShipInitFunc init(Register, {});
} // namespace
