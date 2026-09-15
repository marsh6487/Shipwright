// Compile with the production bridge in this translation unit so the lifetime
// test can inspect the command buffers reached by cached display-list calls.
#include "../soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp"
#include "test_require.h"
#include <iostream>

// A reloadable in-memory archive is the seam for testing provenance refresh.
// Runtime byte parsing/cache behavior is production code; ZIP I/O is not mocked
// as a success assertion and real O2Rs are checked separately by the runner.
namespace Ship {
Archive::Archive(const std::string& path) : mPath(path) {
}
Archive::~Archive() = default;
bool Archive::HasFile(const std::string&) {
    return true;
}
} // namespace Ship
class ReloadableArchive : public Ship::Archive {
  public:
    ReloadableArchive() : Archive("fixture") {
    }
    nlohmann::json project;
    std::shared_ptr<Ship::File> LoadFile(const std::string&) override {
        auto file = std::make_shared<Ship::File>();
        auto text = project.dump();
        file->Buffer = std::make_shared<std::vector<char>>(text.begin(), text.end());
        return file;
    }
    std::shared_ptr<Ship::File> LoadFile(uint64_t) override {
        return nullptr;
    }
    bool Open() override {
        return true;
    }
    bool Close() override {
        return true;
    }
    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override {
        return false;
    }
};

struct GraphicsContext {};
static int sEnabled = -1;
extern "C" int32_t CVarGetInteger(const char*, int32_t defaultValue) {
    return sEnabled < 0 ? defaultValue : sEnabled;
}
static std::array<Gfx, 12> sFrameAllocation;
static Gfx* Graph_Alloc(GraphicsContext*, size_t size) {
    REQUIRE(size == sizeof(sFrameAllocation));
    sFrameAllocation = {};
    return sFrameAllocation.data();
}

// The runner extracts this function verbatim from z_rcp.c, rather than testing
// a reimplementation. Only the allocation and interpolation-recording wrapper
// are replaced; actual native GBI macros generate all 12 commands.
#define gDPSetTileSizeLerp __gDPSetTileSizeLerp
#include "native_two_tex_scroll.inc"

static void CheckCommands(const std::array<Gfx, 12>& actual, const Gfx* expected) {
    for (size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i].words.w0 == expected[i].words.w0);
        REQUIRE(actual[i].words.w1 == expected[i].words.w1);
    }
}

int main() {
    auto archive = std::make_shared<ReloadableArchive>();
    archive->project = nlohmann::json::parse(
        R"({"edits":{"any_scene":[{"data":{"pastes":[{"newDlPath":"custom/prelude/any/paste0","chain":[{"path":"objects/object_spot06_objects/gLakeHyliaHighWaterDL"}]}]}}]}})");
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/any/paste0") == Prelude::NativeMaterialProfile::LakeHylia);
    archive->project["edits"]["any_scene"][0]["data"]["pastes"][0]["chain"][0]["path"] = "unrelated";
    REQUIRE(Prelude::ProfileFor(archive, "custom/prelude/any/paste0") == Prelude::NativeMaterialProfile::None);
    auto& items=archive->project["edits"]["any_scene"][0]["data"]["pastes"];
    auto a=items[0];a["chain"][0]["path"]="objects/object_spot06_objects/gLakeHyliaHighWaterDL";
    auto b=a;b["chain"][0]["path"]="objects/object_spot01_objects/gKakarikoWellWaterDL";
    items=nlohmann::json::array({a,b,a});
    REQUIRE(Prelude::ProfileFor(archive,"custom/prelude/any/paste0")==Prelude::NativeMaterialProfile::None);
    GraphicsContext ctx;
    auto& lists = Prelude::Lists().lists;
    auto lakePointer = lists[1].data();
    REQUIRE(lakePointer != lists[2].data() && lakePointer != lists[3].data());
    for (uint32_t f : { 0u, 1u, 31u, 32u, 63u, 64u, 127u, 128u, 2047u, 2048u, 0xffffffffu }) {
        const uint32_t game = f + 53u; // Verify different native clock sources.
        PreludeNativeMaterialScroll_Update(&ctx, f, game);
        CheckCommands(lists[1], Gfx_TwoTexScrollEx(&ctx, 0, 0u - f, f, 32, 32, 1, f, f, 32, 32, -1, 1, 1, 1));
        CheckCommands(lists[2], Gfx_TwoTexScrollEx(&ctx, 0, 127 - f % 128, f & 127, 32, 32, 1, f % 128, f & 127, 32, 32,
                                                   -1, 1, 1, 1));
        CheckCommands(lists[3],
                      Gfx_TwoTexScrollEx(&ctx, 0, game % 128, 0, 32, 16, 1, game % 128, 0, 32, 16, 1, 0, 1, 0));
        for (size_t i=4;i<lists.size();++i) {
            const int size=i<7?32:64;
            const int rate=((i-4)%3==0?-20:(i-4)%3==1?20:10)*(size/32);
            CheckCommands(lists[i],Gfx_TwoTexScrollEx(&ctx,0,0,0,size,size,1,0,
                game*static_cast<uint32_t>(rate),size,size,0,0,0,rate));
            for (size_t j=1;j<i;++j) REQUIRE(lists[i].data()!=lists[j].data());
        }
        auto before = lists;
        sFrameAllocation = {}; // Simulate transient allocation reuse after draw.
        for (size_t i = 1; i < lists.size(); ++i) {
            CheckCommands(lists[i], before[i].data());
        }
        REQUIRE(lakePointer == lists[1].data());
    }
    sEnabled = 0;
    PreludeNativeMaterialScroll_Update(&ctx, 5, 8);
    for (size_t i = 1; i < lists.size(); ++i) {
        REQUIRE(lists[i][0].words.w0 == 0xdf000000);
    }
    sEnabled = 1;
    PreludeNativeMaterialScroll_Update(&ctx, 0, 0); // New scene / reset frame counts.
    REQUIRE(lakePointer == lists[1].data());
    REQUIRE(lists[1][1].words.w0 >> 24 == 0x4a); // Native interpolated tile command.
    std::cout << "PASS native generated commands, independent buffers, frame reset, disable, lifetime\n";
}
