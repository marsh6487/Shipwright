#include "soh/Enhancements/Graphics/SeasonFoliage.cpp"
#include "test_require.h"
#include <iostream>

Ship::Context* Ship::Context::current = nullptr;
GameInteractor* GameInteractor::Instance = nullptr;
PlayState* gPlayState = nullptr;
static int season = -1;
static size_t evicted = 0;
extern "C" int32_t SeasonWeather_SeasonForPlay(const PlayState*) {
    return season;
}
extern "C" void gfx_texture_cache_clear() {
    ++evicted;
}

static std::shared_ptr<Fast::Texture> Make(const std::string& path) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Path = path;
    auto source = std::make_shared<Fast::Texture>(init);
    source->Type = Fast::TextureType::RGBA16bpp;
    source->Flags = TEX_FLAG_LOAD_AS_RAW;
    source->Width = source->Height = 1;
    source->ImageDataSize = 4;
    source->mImageBuffer = std::make_shared<std::vector<char>>(std::initializer_list<char>{ 60, 110, 70, 83 });
    source->ImageData = reinterpret_cast<uint8_t*>(source->mImageBuffer->data());
    return source;
}

int main() {
    Ship::Context context;
    context.rm = std::make_shared<Ship::ResourceManager>();
    Ship::Context::current = &context;
    const std::string canopy = "alt/objects/object_wood02/tree03_model_01_0";
    const std::string grass = "alt/scenes/shared/spot04_scene/s04_yuka_01r_46800";
    const auto tree = Make(canopy), ground = Make(grass);
    context.rm->cache[canopy] = tree;
    context.rm->cache[grass] = ground;
    const auto originalTree = *tree->mImageBuffer, originalGround = *ground->mImageBuffer;
    season = 0;
    Update(nullptr);
    const auto pink = context.rm->cache.at(canopy);
    REQUIRE(pink != tree);
    REQUIRE(context.rm->cache.at(grass) == ground);
    const auto populated = context.rm->cache.size();
    season = 2;
    Update(nullptr);
    const auto amber = context.rm->cache.at(canopy);
    REQUIRE(amber != tree && amber != pink && context.rm->cache.at(grass) != ground);
    season = 3;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) != amber);
    season = 2;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) == amber && "re-entering autumn must reuse original-source copy");
    REQUIRE(context.rm->cache.size() == populated && "cold assets must not be loaded by tint");
    season = 1;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) == tree && context.rm->cache.at(grass) == ground);
    season = 0;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) == pink);
    context.rm->alt = false;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) == tree && "Alt-off must restore its bindings");
    context.rm->alt = true;
    Update(nullptr);
    const auto replacement = Make(canopy);
    context.rm->cache[canopy] = replacement;
    Reset();
    REQUIRE(context.rm->cache.at(canopy) == replacement && "cleanup must respect external reload");
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) != replacement);
    season = -1;
    Update(nullptr);
    REQUIRE(context.rm->cache.at(canopy) == replacement);
    REQUIRE(*tree->mImageBuffer == originalTree && *ground->mImageBuffer == originalGround);
    // Scene travel unloads Alt allocations. Retirement must release the old
    // source/copy cohort after draw/texture-cache teardown, before the next scene.
    Retire();
    REQUIRE(sCopies.empty() && evicted > 0);
    for (int visit = 0; visit < 50; ++visit) {
        context.rm->cache.clear();
        context.rm->cache[canopy] = Make(canopy);
        season = 2;
        Update(nullptr);
        REQUIRE(sCopies.size() == 1);
        Retire();
        REQUIRE(sCopies.empty());
    }
    Ship::Context::current = nullptr;
    Update(nullptr);
    std::cout << "PASS seasonal foliage bindings: spring/ground, native/Alt, season transitions, cold assets, reload "
                 "guards, cleanup\n";
}
