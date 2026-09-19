// Exercise the production decorator and libultraship's binary DL parser. Only
// archive I/O/lookup is replaced with in-memory metadata and physical path owners.
#include "../soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp"
#include "test_require.h"
#include <fast/lus_gbi.h>
#include <fstream>
#include <iostream>

#include "native_binary_display_list_factory.inc"

static std::map<std::string, std::shared_ptr<Ship::Archive>> sOwners;
namespace Ship {
Archive::Archive(const std::string& path) : mPath(path) {
}
Archive::~Archive() = default;
bool Archive::HasFile(const std::string& path) {
    return path == "prelude/project/edits.json";
}
ArchiveManager::ArchiveManager() = default;
ArchiveManager::~ArchiveManager() = default;
std::shared_ptr<Archive> ArchiveManager::GetArchiveFromFile(const std::string& path) {
    const auto owner = sOwners.find(path);
    return owner == sOwners.end() ? nullptr : owner->second;
}
} // namespace Ship

class MetadataArchive : public Ship::Archive {
  public:
    MetadataArchive() : Archive("fixture") {
    }
    nlohmann::json project;
    std::shared_ptr<Ship::File> LoadFile(const std::string& path) override {
        REQUIRE(path == "prelude/project/edits.json");
        auto file = std::make_shared<Ship::File>();
        const auto text = project.dump();
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

static std::shared_ptr<Fast::DisplayList>
ReadDisplayList(Prelude::NativeMaterialDisplayListFactory& factory, const std::string& path,
                const std::vector<Prelude::NativeMaterialCommand>& commands,
                const std::shared_ptr<Ship::Archive>& parent = nullptr, UcodeHandlers ucode = ucode_f3dex2) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Path = path;
    init->Parent = parent;
    init->ByteOrder = Ship::Endianness::Little;
    init->Type = 0x4f444c54; // Binary display list, "TLDO" on disk.
    init->ResourceVersion = 0;
    init->Id = 0;
    init->IsCustom = path.starts_with("alt/");
    init->Format = RESOURCE_FORMAT_BINARY;
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(8, char(0xff));
    (*file->Buffer)[0] = static_cast<char>(ucode);
    for (const auto& command : commands) {
        for (uintptr_t word : { command.w0, command.w1 }) {
            for (unsigned shift : { 0u, 8u, 16u, 24u }) {
                file->Buffer->push_back(static_cast<char>(word >> shift));
            }
        }
    }
    auto reader = std::make_shared<Ship::BinaryReader>(file->Buffer->data(), file->Buffer->size());
    reader->SetEndianness(init->ByteOrder);
    file->Reader = reader;
    file->IsLoaded = true;
    auto result = std::dynamic_pointer_cast<Fast::DisplayList>(factory.ReadResource(file, init));
    REQUIRE(result);
    REQUIRE(result->GetInitData()->Path == path); // Physical resource identity stays intact.
    return result;
}

static void CheckBinding(const std::shared_ptr<Fast::DisplayList>& result,
                         const std::vector<Prelude::NativeMaterialCommand>& commands,
                         Prelude::NativeMaterialProfile profile, std::optional<size_t> insertion) {
    const size_t expectedSize = commands.size() + (insertion ? 1 : 0);
    if (result->Instructions.size() != expectedSize) {
        std::cerr << result->GetInitData()->Path << ": expected " << expectedSize << " instructions, got "
                  << result->Instructions.size() << '\n';
    }
    REQUIRE(result->Instructions.size() == expectedSize);
    if (insertion) {
        REQUIRE(*insertion < commands.size());
        const Gfx call = gsSPDisplayList(Prelude::Lists().lists[static_cast<size_t>(profile)].data());
        REQUIRE(result->Instructions[*insertion].words.w0 == call.words.w0);
        REQUIRE(result->Instructions[*insertion].words.w1 == call.words.w1);
    }
    for (size_t i = 0; i < commands.size(); ++i) {
        const auto& actual = result->Instructions[i + (insertion && i >= *insertion ? 1 : 0)];
        REQUIRE(actual.words.w0 == commands[i].w0);
        REQUIRE(actual.words.w1 == commands[i].w1);
    }
}

static void CheckAlternateOwnership() {
    const std::string path = "custom/prelude/test/sage_platform";
    const std::string altPath = "alt/" + path;
    const std::vector<Prelude::NativeMaterialCommand> material = {
        { 0xf5101000, 0x00017c5e }, { 0xf2000000, 0x0007c07c },
        { 0xf5101000, 0x0101785f }, { 0xf2000000, 0x0107c07c }, { 0xdf000000, 0 },
    };
    const auto profile = Prelude::NativeMaterialProfile::ChamberOfSagesPlatform;
    auto bound = std::make_shared<MetadataArchive>();
    bound->project = nlohmann::json::parse(
        R"({"edits":{"test":[{"data":{"materials":[{"newDlPath":"custom/prelude/test/sage_platform","nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.chamber_of_sages.platform","logicalWidth":32,"logicalHeight":32}}]}}]}})");
    auto unbound = std::make_shared<MetadataArchive>();
    unbound->project = bound->project;
    unbound->project["edits"]["test"][0]["data"]["materials"][0].erase("nativeAnimation");
    auto archives = std::make_shared<Ship::ArchiveManager>();
    Prelude::NativeMaterialDisplayListFactory factory(archives);

    sOwners = { { path, bound }, { altPath, bound } };
    CheckBinding(ReadDisplayList(factory, path, material), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, 4);

    // The canonical path is owned by a different archive. The alternate list
    // may borrow the canonical metadata key, but never that archive's metadata.
    sOwners = { { path, bound }, { altPath, unbound } };
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, std::nullopt);
    sOwners = { { path, unbound }, { altPath, bound } };
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, 4);

    // An explicitly supplied parent remains authoritative over path lookup.
    CheckBinding(ReadDisplayList(factory, altPath, material, unbound), material, profile, std::nullopt);
    sOwners[altPath] = unbound;
    CheckBinding(ReadDisplayList(factory, altPath, material, bound), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, "alt/" + altPath, material, bound), material, profile, std::nullopt);
    sOwners.clear();
}

static void ProbeArchive(const char* fixturePath) {
    std::ifstream input(fixturePath);
    const auto fixture = nlohmann::json::parse(input);
    auto owner = std::make_shared<MetadataArchive>();
    owner->project = fixture["project"];
    Prelude::NativeMaterialDisplayListFactory factory(std::make_shared<Ship::ArchiveManager>());
    nlohmann::json output = nlohmann::json::array();
    for (const auto& item : fixture["resources"]) {
        const std::string path = item["path"];
        sOwners[path] = owner;
        std::vector<Prelude::NativeMaterialCommand> commands;
        for (const auto& pair : item["commands"]) {
            commands.push_back({ pair[0].get<uintptr_t>(), pair[1].get<uintptr_t>() });
        }
        const auto profile = static_cast<Prelude::NativeMaterialProfile>(item["profile"].get<int>());
        const auto insertion = item["insertion"].is_null() ? std::nullopt
                                                         : std::optional<size_t>(item["insertion"].get<size_t>());
        auto result = ReadDisplayList(factory, path, commands, nullptr,
                                       static_cast<UcodeHandlers>(item["ucode"].get<int>()));
        CheckBinding(result, commands, profile, insertion);
        output.push_back({ { "path", path }, { "profile", static_cast<int>(profile) },
                           { "insertion", item["insertion"] }, { "instruction_count", result->Instructions.size() },
                           { "adapter_verified", true } });
    }
    sOwners.clear();
    std::cout << output.dump(2) << '\n';
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    if (argc == 2) {
        ProbeArchive(argv[1]);
        return 0;
    }
    REQUIRE(argc == 1);
    CheckAlternateOwnership();
    std::cout << "PASS native material factory canonical/alt binding, physical ownership, and parent precedence\n";
}
