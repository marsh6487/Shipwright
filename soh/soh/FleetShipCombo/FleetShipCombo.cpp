#include "FleetShipCombo.h"

#include <libultraship/libultraship.h>
#include <libultraship/bridge.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace {

// Candidate 2ship executable file names, in priority order, per platform.
// 2ship's CMake target is "2ship" (-> 2ship.exe on Windows, 2s2h.elf on Linux,
// 2ship/2s2h-macos on macOS).
const std::vector<std::string>& TwoShipExeNames() {
#ifdef _WIN32
    static const std::vector<std::string> names = { "2ship.exe" };
#elif defined(__APPLE__)
    static const std::vector<std::string> names = { "2ship", "2s2h-macos" };
#else
    static const std::vector<std::string> names = { "2s2h.elf", "2ship" };
#endif
    return names;
}

// Directory containing the currently running Ship executable.
std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::path(std::wstring(buf, len)).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return {};
    }
    return std::filesystem::weakly_canonical(std::filesystem::path(buf)).parent_path();
#else
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return p.parent_path();
#endif
}

// 2ship lives at Ship/2ship/, so look in <shipDir>/2ship first, then <shipDir> as
// a fallback (in case both exes share a folder during development).
std::filesystem::path Locate2ShipExe() {
    std::filesystem::path selfDir = SelfExeDir();
    if (selfDir.empty()) {
        return {};
    }

    std::vector<std::filesystem::path> searchDirs = { selfDir / "2ship", selfDir };

    std::error_code ec;
    for (const auto& dir : searchDirs) {
        for (const auto& name : TwoShipExeNames()) {
            std::filesystem::path candidate = dir / name;
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
        }
    }
    return {};
}

bool HasArg(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

// Launch 2ship as a child, telling it --fleet-child so it does not bounce back.
// The host keeps running (no replace/exit). Returns true on success.
bool Launch2ShipChild(const std::filesystem::path& twoShipExe) {
    std::filesystem::path workDir = twoShipExe.parent_path();
#ifdef _WIN32
    // Pass our PID so 2ship can exit itself if this host process dies (no orphan).
    std::wstring cmd = L"\"" + twoShipExe.wstring() + L"\" --fleet-child --fleet-host-pid=" +
                       std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    std::wstring workDirW = workDir.wstring();

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(twoShipExe.wstring().c_str(), mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                             workDirW.empty() ? nullptr : workDirW.c_str(), &si, &pi);
    if (!ok) {
        SPDLOG_ERROR("[FleetShipCombo] CreateProcessW failed for '{}' (error {})", twoShipExe.string(), GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    pid_t pid = fork();
    if (pid < 0) {
        SPDLOG_ERROR("[FleetShipCombo] fork failed for '{}'", twoShipExe.string());
        return false;
    }
    if (pid == 0) {
        // Child: move into the 2ship folder so it finds its own assets, then exec.
        std::string exe = twoShipExe.string();
        std::string dir = workDir.string();
        if (!dir.empty()) {
            (void)chdir(dir.c_str());
        }
        std::string hostPid = "--fleet-host-pid=" + std::to_string((long)getppid());
        char* args[] = { exe.data(), const_cast<char*>("--fleet-child"), hostPid.data(), nullptr };
        execv(exe.c_str(), args);
        _exit(127); // exec failed
    }
    return true; // parent (host) keeps running
#endif
}

// ===================== Shared-memory coordination (Frente B) =====================

// THIS_GAME identity for this build: 0 = Ocarina of Time (Ship).
constexpr int kThisGame = 0;
constexpr uint32_t kFscMagic = 0x46534331u;   // 'FSC1'
constexpr uint32_t kFscVersion = 1u;

// Layout shared between Ship and 2ship. MUST stay byte-identical to the 2ship copy.
struct FscShared {
    uint32_t magic;
    uint32_t version;
    int32_t activeGame; // 0 = Ocarina of Time (Ship), 1 = Majora's Mask (2ship)
    // Picture-in-picture: 2ship publishes its game image as a D3D11 shared texture.
    uint64_t texHandle;     // OS shared handle from IDXGIResource::GetSharedHandle (0 = none)
    uint32_t texWidth;
    uint32_t texHeight;
    uint32_t texFormat;     // DXGI_FORMAT value
    uint32_t texFrameIndex; // bumped each publish (lets the consumer detect new frames)
    int32_t uiFocus;        // which window is front for CONFIG: 0 = Ship, 1 = 2ship
    int32_t reservedI[3];
    uint64_t reservedU[12];
};

#ifdef _WIN32
std::wstring sShmName = L"Local\\FleetShipComboShared";
HANDLE sShmHandle = nullptr;
#else
std::string sShmName = "/FleetShipComboShared";
int sShmFd = -1;
#endif
FscShared* sShared = nullptr;
bool sLazyOpenTried = false;

// Make the shared-memory region name UNIQUE per combo instance, keyed by the host Ship's PID,
// so SEVERAL combos can run on one machine without colliding on a single region. Ship (host)
// keys it by its own PID and launches 2ship with --fleet-host-pid=<that PID>, so the child
// derives the SAME name and the pair is matched. Key 0 keeps the legacy unsuffixed name.
void SetInstanceKey(unsigned long key) {
    if (key == 0) {
        return;
    }
#ifdef _WIN32
    sShmName = L"Local\\FleetShipComboShared_" + std::to_wstring(key);
#else
    sShmName = "/FleetShipComboShared_" + std::to_string(key);
#endif
}

void InitFreshRegion() {
    if (!sShared) {
        return;
    }
    sShared->magic = kFscMagic;
    sShared->version = kFscVersion;
    sShared->activeGame = 0; // default to OoT until host/child sets it
    sShared->texHandle = 0;
    sShared->texWidth = 0;
    sShared->texHeight = 0;
    sShared->texFormat = 0;
    sShared->texFrameIndex = 0;
    sShared->uiFocus = 0;
    for (int i = 0; i < 3; ++i) {
        sShared->reservedI[i] = 0;
    }
    for (int i = 0; i < 12; ++i) {
        sShared->reservedU[i] = 0;
    }
}

// create=true: create-or-open (FleetShipCombo_SharedInit). create=false: open an
// EXISTING region only (lazy), so standalone play never allocates one.
FscShared* MapShared(bool create) {
    if (sShared) {
        return sShared;
    }
#ifdef _WIN32
    if (create) {
        sShmHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(FscShared), sShmName.c_str());
        bool existed = (sShmHandle != nullptr && GetLastError() == ERROR_ALREADY_EXISTS);
        if (sShmHandle != nullptr) {
            sShared = (FscShared*)MapViewOfFile(sShmHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FscShared));
            if (sShared != nullptr && !existed) {
                InitFreshRegion();
            }
        }
    } else {
        sShmHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, sShmName.c_str());
        if (sShmHandle != nullptr) {
            sShared = (FscShared*)MapViewOfFile(sShmHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FscShared));
        }
    }
#else
    int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
    sShmFd = shm_open(sShmName.c_str(), flags, 0666);
    if (sShmFd >= 0) {
        if (create) {
            (void)ftruncate(sShmFd, sizeof(FscShared));
        }
        void* p = mmap(nullptr, sizeof(FscShared), PROT_READ | PROT_WRITE, MAP_SHARED, sShmFd, 0);
        if (p != MAP_FAILED) {
            sShared = (FscShared*)p;
            if (create && sShared->magic != kFscMagic) {
                InitFreshRegion();
            }
        }
    }
#endif
    return sShared;
}

// Attach to an existing region once (used by the per-frame freeze check). If none
// exists (standalone), leaves sShared null so IsThisGameActive() returns true.
FscShared* LazyOpen() {
    if (sShared) {
        return sShared;
    }
    if (sLazyOpenTried) {
        return nullptr;
    }
    sLazyOpenTried = true;
    return MapShared(false);
}

} // namespace

void FleetShipCombo_HostBootstrap(int argc, char** argv) {
    // For picture-in-picture BOTH games run whenever the combo is enabled; isPlayerIn2Ship
    // only decides which one is ACTIVE (unfrozen). Bring up 2ship when:
    //  - the combo is enabled (master toggle), OR
    //  - 2ship handed off to us with --boot=mm.
    bool bootMm = HasArg(argc, argv, "--boot=mm");
    bool enabled = CVarGetInteger("isFleetShipCombo.Enabled", 0);

    if (!bootMm && !enabled) {
        return; // combo off -> plain OoT
    }

    std::filesystem::path twoShipExe = Locate2ShipExe();
    if (twoShipExe.empty()) {
        SPDLOG_WARN("[FleetShipCombo] 2ship executable not found under Ship/2ship/; staying in OoT only.");
        return;
    }

    // Active game = the one the player was last in (MM if handed off via --boot=mm).
    int active = (bootMm || CVarGetInteger("isPlayerIn2Ship", 0)) ? 1 : 0;

    SPDLOG_INFO("[FleetShipCombo] Host bringing up 2ship child at '{}' (--fleet-child). active={}.",
                twoShipExe.string(), active);

    // Key the shared region by OUR pid; the child is launched with --fleet-host-pid=<our pid>
    // (see Launch2ShipChild) and attaches to the same name, so multiple combos on one machine
    // stay isolated. Create the region + set the active game BEFORE launching the child, so the
    // child sees the correct active game immediately (no freeze race).
#ifdef _WIN32
    unsigned long selfPid = GetCurrentProcessId();
#else
    unsigned long selfPid = static_cast<unsigned long>(getpid());
#endif
    FleetShipCombo_SharedInit(selfPid);
    FleetShipCombo_SetActiveGame(active);

    if (Launch2ShipChild(twoShipExe)) {
        CVarSetInteger("isPlayerIn2Ship", active);
        Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
    }
}

void FleetShipCombo_SharedInit(unsigned long instanceKey) {
    SetInstanceKey(instanceKey);
    MapShared(true);
}

int FleetShipCombo_GetActiveGame(void) {
    FscShared* s = LazyOpen();
    return s ? s->activeGame : -1;
}

void FleetShipCombo_SetActiveGame(int game) {
    FscShared* s = LazyOpen();
    if (s) {
        s->activeGame = game;
    }
}

bool FleetShipCombo_IsThisGameActive(void) {
    FscShared* s = LazyOpen();
    if (!s) {
        return true; // standalone / no combo: always active, never freeze
    }
    return s->activeGame == kThisGame;
}

int FleetShipCombo_GetSharedTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                    unsigned int* dxgiFormat, unsigned int* frameIndex) {
    FscShared* s = LazyOpen();
    if (!s) {
        return 0;
    }
    if (handle) {
        *handle = s->texHandle;
    }
    if (width) {
        *width = s->texWidth;
    }
    if (height) {
        *height = s->texHeight;
    }
    if (dxgiFormat) {
        *dxgiFormat = s->texFormat;
    }
    if (frameIndex) {
        *frameIndex = s->texFrameIndex;
    }
    return s->texHandle != 0 ? 1 : 0;
}

int FleetShipCombo_GetUiFocus(void) {
    FscShared* s = LazyOpen();
    return s ? s->uiFocus : -1;
}

void FleetShipCombo_SetUiFocus(int focus) {
    FscShared* s = LazyOpen();
    if (s) {
        s->uiFocus = focus;
    }
#ifdef _WIN32
    // Handing the foreground to 2ship for config: as the process that currently holds the
    // foreground (Ship), bless ANY process to take it. This is the documented way to let
    // 2ship's SetForegroundWindow succeed WITHOUT the user clicking the window first;
    // otherwise Windows' foreground-lock blocks cross-process focus changes and the
    // keyboard (ESC) keeps going to Ship instead of 2ship's BenGui.
    if (focus == 1) {
        AllowSetForegroundWindow(ASFW_ANY);
    }
#endif
}
