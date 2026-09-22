// Exercise the production archive adapter. The archive boundary uses exact
// filenames; the legacy ResourceMgr existence boundary strips extensions.
#include "test_require.h"
#include "soh/Enhancements/audio/MidnaAudioResources.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#define CVAR_SETTING(name) "gSettings." name
static int master = 40, sfx = 50, loads;
int CVarGetInteger(const char* key, int fallback) {
    if (std::strcmp(key, "gSettings.Volume.Master") == 0)
        return master;
    if (std::strcmp(key, "gSettings.Volume.SFX") == 0)
        return sfx;
    REQUIRE(false);
    return fallback;
}
uint8_t ResourceMgr_FileExists(const char* key) {
    // OTRAudio_Init runs before OTRExtScanner fills this cache.
    return 0;
}
namespace Ship {
struct File {
    std::shared_ptr<std::vector<char>> Buffer;
};
struct ArchiveManager {
    std::map<std::string, std::shared_ptr<File>> files;
    bool HasFile(const std::string& key) {
        return files.count(key) != 0;
    }
    std::shared_ptr<File> LoadFile(const std::string& key) {
        ++loads;
        return files.at(key);
    }
} archive;
struct ResourceManager {
    ArchiveManager* GetArchiveManager() {
        return &archive;
    }
} manager;
struct Context {
    static Context* GetRawInstance() {
        static Context context;
        return &context;
    }
    ResourceManager* GetResourceManager() {
        return &manager;
    }
};
} // namespace Ship

namespace MidnaAudioResources {
/* PRODUCTION_MIDNA_AUDIO_RESOURCES */
}

int main() {
    const char* path = "objects/midna_navi/audio/dash.wav";
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(std::initializer_list<char>{ 'R', 'I', 'F', 'F', -1 });
    Ship::archive.files[path] = file;
    Ship::archive.files["objects/midna_navi/poc1/MidnaFloatDL"] = file;
    std::vector<uint8_t> bytes;
    REQUIRE(MidnaAudioResources::HasModel());
    REQUIRE(MidnaAudioResources::ReadClip(path, bytes));
    REQUIRE(loads == 1 && bytes.size() == 5 && bytes[4] == 255);
    REQUIRE(!MidnaAudioResources::ReadClip("objects/midna_navi/audio/absent.wav", bytes));
    REQUIRE(loads == 1);
    file->Buffer->clear();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    file->Buffer.reset();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    Ship::archive.files[path].reset();
    REQUIRE(!MidnaAudioResources::ReadClip(path, bytes));
    REQUIRE(MidnaAudioResources::Gain() == 0.2f);
    master = 0;
    REQUIRE(MidnaAudioResources::Gain() == 0);
    master = 100;
    sfx = 0;
    REQUIRE(MidnaAudioResources::Gain() == 0);
    sfx = 200;
    REQUIRE(MidnaAudioResources::Gain() == 1);
    puts("PASS: exact WAV archive filenames and production Master/SFX volume adapter");
}
