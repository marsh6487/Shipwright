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
    // Cross-game loading-zone WARP request. The trigger side writes the target (in the TARGET
    // game's id/space), bumps warpSeq, and flips activeGame; whichever game BECOMES active applies
    // it once per new seq (load scene + override Link pos/rot). MUST stay byte-identical to 2ship.
    int32_t warpSeq;        // bumped per request (0 = none yet)
    int32_t warpScene;      // target scene id in the target game's space
    float warpX;            // land position override (target game world coords)
    float warpY;
    float warpZ;
    int32_t warpRotY;        // land Y rotation (s16 binary angle stored in int32)
    int32_t warpSaveFileNum; // save SLOT the warp came from; the target game loads its own same slot (-1 = unset)
    int32_t sendFadeAlpha;   // 0..255 sending-fade overlay, written by the ACTIVE (sending) game, drawn by the host consumer
    int32_t doorDLIndex;     // DEV: which Lost Woods room-DL the MM door tunnel tool is showing (for the on-screen readout)
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
    sShared->warpSeq = 0;
    sShared->warpScene = 0;
    sShared->warpX = sShared->warpY = sShared->warpZ = 0.0f;
    sShared->warpRotY = 0;
    sShared->warpSaveFileNum = -1;
    sShared->sendFadeAlpha = 0;
    sShared->doorDLIndex = 0;
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

// Copy an .o2r from whichever combo dir has it into the one that doesn't (best-effort, never throws).
void MirrorO2r(const std::filesystem::path& dirA, const std::filesystem::path& dirB, const char* name) {
    std::error_code ec;
    std::filesystem::path a = dirA / name;
    std::filesystem::path b = dirB / name;
    bool ae = std::filesystem::exists(a, ec);
    bool be = std::filesystem::exists(b, ec);
    if (ae && !be) {
        std::filesystem::copy_file(a, b, std::filesystem::copy_options::overwrite_existing, ec);
    } else if (be && !ae) {
        std::filesystem::copy_file(b, a, std::filesystem::copy_options::overwrite_existing, ec);
    }
}

} // namespace

void FleetShipCombo_ProvisionO2rBothDirs(void) {
    // Combo layout: <root>/soh.exe + <root>/2ship/2ship.exe. Both mm.o2r and oot.o2r should sit next to
    // BOTH exes, so mirror whichever exists into the dir missing it — an extraction done by EITHER game
    // (soh -> oot.o2r, 2ship -> mm.o2r) then provisions both. No-op when there's no sibling (standalone).
    std::filesystem::path selfDir = SelfExeDir();
    std::filesystem::path guestExe = Locate2ShipExe();
    if (selfDir.empty() || guestExe.empty()) {
        return;
    }
    std::filesystem::path guestDir = guestExe.parent_path();
    if (guestDir.empty() || guestDir == selfDir) {
        return;
    }
    MirrorO2r(selfDir, guestDir, "mm.o2r");
    MirrorO2r(selfDir, guestDir, "oot.o2r");
}

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

    // Active game at boot = where the player LAST SAVED (updated only on save; survives window
    // switches). The last-saved record OVERRIDES the --boot=mm handoff, so launching via 2ship.exe after
    // saving in OoT still resumes OoT. (bootMm is ALWAYS set by the 2ship->Ship bounce, so keying the
    // active game off it made the combo always start in MM — the "autostart into MM" bug.) Only with NO
    // last-saved record yet (first run) do we honor the "opened via 2ship" handoff / legacy
    // isPlayerIn2Ship, defaulting to OoT otherwise.
    int lastSaved = CVarGetInteger("gFleetCombo.LastSavedGame", -1);
    int active;
    if (lastSaved >= 0) {
        active = lastSaved;
    } else if (bootMm || CVarGetInteger("isPlayerIn2Ship", 0)) {
        active = 1;
    } else {
        active = 0;
    }

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
    FleetShipCombo_SetUiFocus(active); // front window follows the boot active game (0=OoT, 1=MM)

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

// Per-process seq of the last warp we issued/consumed, so the REQUESTER never re-consumes its own.
static int sLastWarpSeq = 0;

void FleetShipCombo_RequestWarp(int targetGame, int scene, float x, float y, float z, int rotY, int saveFile) {
    FscShared* s = LazyOpen();
    if (!s) {
        return;
    }
    s->warpScene = scene;
    s->warpX = x;
    s->warpY = y;
    s->warpZ = z;
    s->warpRotY = rotY;
    s->warpSaveFileNum = saveFile; // tell the target game which save slot to be in (set before the seq bump)
    s->warpSeq += 1;            // mark a new request
    sLastWarpSeq = s->warpSeq;  // we issued it; don't let THIS process consume its own warp
    s->activeGame = targetGame; // flip: the target game becomes active (unfrozen) and applies it
    s->uiFocus = targetGame;    // front window follows the active game (0=OoT, 1=MM); a peek tab may
                                // override it afterwards without touching activeGame
}

int FleetShipCombo_ConsumePendingWarp(int* scene, float* x, float* y, float* z, int* rotY) {
    FscShared* s = LazyOpen();
    if (!s) {
        return 0;
    }
    if (s->warpSeq == sLastWarpSeq) {
        return 0; // nothing new
    }
    if (s->activeGame != kThisGame) {
        return 0; // not addressed to this game yet
    }
    sLastWarpSeq = s->warpSeq;
    if (scene) {
        *scene = s->warpScene;
    }
    if (x) {
        *x = s->warpX;
    }
    if (y) {
        *y = s->warpY;
    }
    if (z) {
        *z = s->warpZ;
    }
    if (rotY) {
        *rotY = s->warpRotY;
    }
    return 1;
}

int FleetShipCombo_GetWarpSaveFile(void) {
    FscShared* s = LazyOpen();
    return s ? s->warpSaveFileNum : -1;
}

// Cross-game arrival blackout: while > 0, THIS game's render path paints the screen BLACK (empty DL)
// so the stale frame of the OTHER game and the warp scene-load are never shown during a flip. Set on
// warp arrival; the render path calls ...Active() exactly once per frame (it decrements).
static int sArrivalBlackout = 0;
void FleetShipCombo_BeginArrivalBlackout(int frames) {
    sArrivalBlackout = frames;
}
int FleetShipCombo_ArrivalBlackoutActive(void) {
    if (sArrivalBlackout > 0) {
        sArrivalBlackout--;
        return 1;
    }
    return 0;
}

// Sending-side fade overlay (0..255). The active game's warp logic ramps this up while Link keeps
// walking into the door (no scene transition -> no reload, not frozen); the host PiP consumer draws a
// black overlay at this alpha over the scene, giving a real fade-out, then we flip at full black.
void FleetShipCombo_SetSendFadeAlpha(int alpha) {
    FscShared* s = LazyOpen();
    if (!s) {
        return;
    }
    s->sendFadeAlpha = alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha);
}
int FleetShipCombo_GetSendFadeAlpha(void) {
    FscShared* s = LazyOpen();
    return s ? s->sendFadeAlpha : 0;
}

void FleetShipCombo_SetDoorDLIndex(int index) {
    FscShared* s = LazyOpen();
    if (s) {
        s->doorDLIndex = index;
    }
}
int FleetShipCombo_GetDoorDLIndex(void) {
    FscShared* s = LazyOpen();
    return s ? s->doorDLIndex : 0;
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

// ---- FleetSync save-sync handshake over reservedU (layout unchanged — MUST match 2ship) ----
// reservedU[0] is reserved (legacy bottles-sync plan). [1] = syncSaveSeq (bumped by the game that
// just SAVED), [2] = syncSaveAck (set by the OTHER game once it applied the shared overlay and
// wrote its own file), [3] = syncSaveSlot.
void FleetShipCombo_SignalSyncSave(int slot) {
    FscShared* s = LazyOpen();
    if (!s) {
        return;
    }
    s->reservedU[3] = (uint64_t)(uint32_t)slot;
    s->reservedU[1] = s->reservedU[1] + 1;
}

unsigned long long FleetShipCombo_GetSyncSaveSeq(void) {
    FscShared* s = LazyOpen();
    return s ? s->reservedU[1] : 0;
}

int FleetShipCombo_GetSyncSaveSlot(void) {
    FscShared* s = LazyOpen();
    return s ? (int)(uint32_t)s->reservedU[3] : -1;
}

void FleetShipCombo_AckSyncSave(unsigned long long seq) {
    FscShared* s = LazyOpen();
    if (s) {
        s->reservedU[2] = seq;
    }
}

unsigned long long FleetShipCombo_GetSyncSaveAck(void) {
    FscShared* s = LazyOpen();
    return s ? s->reservedU[2] : 0;
}

// ---- Fleet Oracle (combo randomizer) handshake over reservedU[4..5] (layout unchanged — MUST match 2ship) ----
// [4] = oracleReqSeq (bumped by the HOST after writing fleet_oracle_req.json), [5] = oracleRespAck
// (set by the MM oracle to the req seq it answered, after writing fleet_oracle_resp.json).
void FleetShipCombo_SignalOracleRequest(void) {
    FscShared* s = LazyOpen();
    if (s) {
        s->reservedU[4] = s->reservedU[4] + 1;
    }
}

unsigned long long FleetShipCombo_GetOracleRequestSeq(void) {
    FscShared* s = LazyOpen();
    return s ? s->reservedU[4] : 0;
}

void FleetShipCombo_AckOracleResponse(unsigned long long seq) {
    FscShared* s = LazyOpen();
    if (s) {
        s->reservedU[5] = seq;
    }
}

unsigned long long FleetShipCombo_GetOracleResponseAck(void) {
    FscShared* s = LazyOpen();
    return s ? s->reservedU[5] : 0;
}

// ---- Shared-window open request over reservedU[6] (layout unchanged — MUST match 2ship) ----
// 2ship's "Shared" tab bumps this counter; our client pump (FleetOracleClient) opens the
// Fleet Shared window when it sees the change.
void FleetShipCombo_RequestSharedWindowOpen(void) {
    FscShared* s = LazyOpen();
    if (s) {
        s->reservedU[6] = s->reservedU[6] + 1;
    }
}

unsigned long long FleetShipCombo_GetSharedWindowOpenSeq(void) {
    FscShared* s = LazyOpen();
    return s ? s->reservedU[6] : 0;
}

// ---- Cross-game RESTART over reservedU[10] (layout unchanged — MUST match the other repo) ----
// A reset in one game bumps reservedU[10]; the other game's per-frame pump sees the new value and
// resets itself too. sRestartSelfSeq marks the value we ourselves bumped/observed so we never respond
// to our own reset (which would ping-pong forever).
static unsigned long long sRestartSelfSeq = 0;
static bool sRestartSeqInit = false;

void FleetShipCombo_SignalRestart(void) {
    FscShared* s = LazyOpen();
    if (s) {
        s->reservedU[10] = s->reservedU[10] + 1;
        sRestartSelfSeq = s->reservedU[10]; // our own bump; our pump must not respond to it
        sRestartSeqInit = true;
    }
}

int FleetShipCombo_ConsumeRestartRequest(void) {
    FscShared* s = LazyOpen();
    if (!s) {
        return 0;
    }
    if (!sRestartSeqInit) {
        sRestartSeqInit = true;
        sRestartSelfSeq = s->reservedU[10]; // don't fire on any pre-existing value at attach
        return 0;
    }
    if (s->reservedU[10] == sRestartSelfSeq) {
        return 0; // nothing new, or our own reset
    }
    sRestartSelfSeq = s->reservedU[10];
    return 1;
}

// ---- UI-overlay texture over reservedU[7..9] (layout unchanged — MUST match 2ship) ----
// A SECOND shared texture with ONLY 2ship's ImGui windows (trackers etc.) over a transparent
// background, so we can draw MM's UI on top of whichever game is active (both trackers at once).
// [7] = D3D11 shared handle (0 = none), [8] = (width<<32)|height, [9] = (dxgiFormat<<32)|frameIndex.
void FleetShipCombo_PublishUiTexture(unsigned long long handle, unsigned int width, unsigned int height,
                                     unsigned int dxgiFormat, unsigned int frameIndex) {
    FscShared* s = LazyOpen();
    if (!s) {
        return;
    }
    s->reservedU[8] = ((unsigned long long)width << 32) | height;
    s->reservedU[9] = ((unsigned long long)dxgiFormat << 32) | frameIndex;
    s->reservedU[7] = handle; // handle last: the consumer keys re-opens off it
}

int FleetShipCombo_GetUiTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                unsigned int* dxgiFormat, unsigned int* frameIndex) {
    FscShared* s = LazyOpen();
    if (!s) {
        return 0;
    }
    if (handle) {
        *handle = s->reservedU[7];
    }
    if (width) {
        *width = (unsigned int)(s->reservedU[8] >> 32);
    }
    if (height) {
        *height = (unsigned int)(s->reservedU[8] & 0xFFFFFFFFu);
    }
    if (dxgiFormat) {
        *dxgiFormat = (unsigned int)(s->reservedU[9] >> 32);
    }
    if (frameIndex) {
        *frameIndex = (unsigned int)(s->reservedU[9] & 0xFFFFFFFFu);
    }
    return s->reservedU[7] != 0 ? 1 : 0;
}

// ---- Combo active SLOT over reservedU[11] (layout unchanged — MUST match the other repo) ----
// OoT publishes which save slot the current combo file is (0..2 stored as 1..3; 0 = unset) when a
// combo file is created/loaded, so MM auto-loads the SAME slot when it becomes active (its own
// per-process gFleetCombo.LastSlot may not match on a fresh combo).
void FleetShipCombo_SetComboSlot(int slot) {
    FscShared* s = LazyOpen();
    if (s && slot >= 0 && slot <= 2) {
        s->reservedU[11] = (uint64_t)(slot + 1);
    }
}

int FleetShipCombo_GetComboSlot(void) {
    FscShared* s = LazyOpen();
    if (!s || s->reservedU[11] == 0) {
        return -1;
    }
    return (int)s->reservedU[11] - 1;
}
