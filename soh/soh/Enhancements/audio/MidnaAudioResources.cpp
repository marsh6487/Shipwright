#include "MidnaAudioResources.h"

#include <algorithm>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"

namespace MidnaAudioResources {
bool HasModel() {
    // Audio initializes before OTRExtScanner populates the resource cache.
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HasFile(
        "objects/midna_navi/poc1/MidnaFloatDL");
}

bool ReadClip(const char* path, std::vector<uint8_t>& bytes) {
    // These are raw archive files with extensions. ResourceMgr_FileExists uses
    // the extension-stripped resource cache, which cannot answer this lookup.
    const auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    if (!archives->HasFile(path)) {
        return false;
    }
    auto file = archives->LoadFile(path);
    if (!file || !file->Buffer || file->Buffer->empty() || file->Buffer->size() > 32000 * 10 * 2 + 65536) {
        return false;
    }
    bytes.assign(file->Buffer->begin(), file->Buffer->end());
    return true;
}

float Gain() {
    const float master = std::clamp(CVarGetInteger(CVAR_SETTING("Volume.Master"), 40) / 100.0f, 0.0f, 1.0f);
    const float sfx = std::clamp(CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f, 0.0f, 1.0f);
    return master * sfx;
}
} // namespace MidnaAudioResources
