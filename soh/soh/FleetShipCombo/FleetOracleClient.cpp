// FleetOracleClient.cpp (OoT side) — cliente del oráculo lógico de MM. Ver FleetOracleClient.h.
//
// El host escribe fleet_oracle_req.json EN SU PROPIO dir (que es el mismo dir padre que el oráculo
// de 2ship deriva con parent_path del suyo) y bumpea reservedU[4]; 2ship responde en
// fleet_oracle_resp.json y ackea en reservedU[5].

#include "FleetOracleClient.h"
#include "FleetShipCombo.h"
#include "FleetComboItems.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace {

std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

std::filesystem::path ReqPath() {
    std::filesystem::path dir = SelfExeDir();
    if (dir.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(dir / "fleet", ec);
    return dir / "fleet" / "oracle_req.json";
}
std::filesystem::path RespPath() {
    std::filesystem::path dir = SelfExeDir();
    return dir.empty() ? std::filesystem::path{} : dir / "fleet" / "oracle_resp.json";
}

unsigned long long SendRequest(nlohmann::json req) {
    if (FleetShipCombo_GetActiveGame() < 0) {
        SPDLOG_WARN("[FleetOracleClient] sin combo activo — no hay oráculo al que preguntar");
        return 0;
    }
    unsigned long long seq = FleetShipCombo_GetOracleRequestSeq() + 1;
    req["seq"] = seq;
    std::filesystem::path p = ReqPath();
    if (p.empty()) {
        return 0;
    }
    try {
        std::filesystem::path tmp = p;
        tmp += ".tmp"; // sufijo del host, como en FleetSync
        {
            std::ofstream out(tmp);
            out << req << std::endl;
        }
        // Windows: el rename sobre oracle_req.json falla (sharing violation) si MM lo tiene abierto
        // para leer justo en ese instante. Con miles de llamadas rápidas la colisión es frecuente, así
        // que reintentamos brevemente antes de rendirnos (~200ms máx). Sin esto, la request se pierde y
        // la pre-colocación aborta con "no active combo".
        std::error_code ec;
        for (int attempt = 0; attempt < 100; attempt++) {
            std::filesystem::rename(tmp, p, ec);
            if (!ec) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (ec) {
            std::error_code rmec;
            std::filesystem::remove(tmp, rmec); // no dejar el .tmp de basura
            SPDLOG_WARN("[FleetOracleClient] fallo escribiendo la request (rename: {})", ec.message());
            return 0;
        }
    } catch (...) {
        SPDLOG_WARN("[FleetOracleClient] fallo escribiendo la request");
        return 0;
    }
    FleetShipCombo_SignalOracleRequest();
    return seq;
}

} // namespace

unsigned long long FleetOracle_SendManifestRequest() {
    nlohmann::json req;
    req["op"] = "manifest";
    return SendRequest(std::move(req));
}

unsigned long long FleetOracle_SendReachableRequest(const std::vector<std::pair<int, int>>& fcItems,
                                                    const std::vector<std::pair<std::string, int>>& mmItems) {
    nlohmann::json req;
    req["op"] = "reachable";
    nlohmann::json fc = nlohmann::json::array();
    for (auto& [fcId, count] : fcItems) {
        fc.push_back({ fcId, count });
    }
    nlohmann::json mm = nlohmann::json::array();
    for (auto& [name, count] : mmItems) {
        mm.push_back({ name, count });
    }
    req["fcItems"] = fc;
    req["mmItems"] = mm;
    return SendRequest(std::move(req));
}

unsigned long long FleetOracle_SendSetOptionsRequest(
    const std::vector<std::pair<std::string, int>>& cvars,
    const std::vector<std::pair<std::string, float>>& cvarsFloat) {
    nlohmann::json req;
    req["op"] = "setOptions";
    nlohmann::json arr = nlohmann::json::array();
    for (auto& [cvar, value] : cvars) {
        arr.push_back({ cvar, value });
    }
    req["cvars"] = arr;
    nlohmann::json arrF = nlohmann::json::array();
    for (auto& [cvar, value] : cvarsFloat) {
        arrF.push_back({ cvar, value });
    }
    req["cvarsFloat"] = arrF;
    return SendRequest(std::move(req));
}

unsigned long long FleetOracle_SendPrepareSeedRequest(const std::string& fileName) {
    nlohmann::json req;
    req["op"] = "prepareSeed";
    req["file"] = fileName;
    return SendRequest(std::move(req));
}

unsigned long long FleetOracle_SendCreateSaveRequest(int slot, const std::string& name) {
    nlohmann::json req;
    req["op"] = "createSave";
    req["slot"] = slot;
    req["name"] = name;
    return SendRequest(std::move(req));
}

bool FleetOracle_TryGetResponse(unsigned long long seq, nlohmann::json& out) {
    if (seq == 0 || FleetShipCombo_GetOracleResponseAck() < seq) {
        return false;
    }
    std::filesystem::path p = RespPath();
    if (p.empty() || !std::filesystem::exists(p)) {
        return false;
    }
    try {
        std::ifstream in(p);
        in >> out;
    } catch (...) {
        return false;
    }
    return out.is_object() && out.contains("seq") && out["seq"].get<unsigned long long>() == seq;
}

// ---- Smoke test por CVar (gFleetOracle.Test = 1 manifest / 2 sphere-0 / 3 full FC inventory) ----

namespace {

unsigned long long sTestPendingSeq = 0;
uint64_t sTestSentAtMs = 0;
std::string sLastTestSummary;

uint64_t NowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

unsigned long long sLastSharedOpenSeq = 0;
bool sSharedOpenSeqInit = false;

void ProcessOracleTest() {
    // Petición de 2ship de abrir la ventana Fleet Shared (tab Shared de su menú, reservedU[6]).
    unsigned long long openSeq = FleetShipCombo_GetSharedWindowOpenSeq();
    if (!sSharedOpenSeqInit) {
        sSharedOpenSeqInit = true;
        sLastSharedOpenSeq = openSeq;
    }
    if (openSeq != sLastSharedOpenSeq) {
        sLastSharedOpenSeq = openSeq;
        FleetShipCombo_OpenSharedWindow();
    }

    if (sTestPendingSeq != 0) {
        nlohmann::json resp;
        if (FleetOracle_TryGetResponse(sTestPendingSeq, resp)) {
            uint64_t elapsed = NowMs() - sTestSentAtMs;
            if (resp.contains("error")) {
                sLastTestSummary = "Test #" + std::to_string(sTestPendingSeq) +
                                   " ERROR: " + resp["error"].get<std::string>();
                SPDLOG_ERROR("[FleetOracleClient] {}", sLastTestSummary);
            } else if (resp.value("op", "") == "manifest") {
                sLastTestSummary = "Test #" + std::to_string(sTestPendingSeq) + " manifest OK in " +
                                   std::to_string(elapsed) + " ms: " + std::to_string(resp["checks"].size()) +
                                   " checks, " + std::to_string(resp["pool"].size()) + " items in pool, " +
                                   std::to_string(resp["startingItems"].size()) + " starting items";
                SPDLOG_INFO("[FleetOracleClient] {}", sLastTestSummary);
            } else {
                sLastTestSummary = "Test #" + std::to_string(sTestPendingSeq) + " reachable OK in " +
                                   std::to_string(elapsed) + " ms: " + std::to_string(resp["reachable"].size()) +
                                   " reachable checks";
                SPDLOG_INFO("[FleetOracleClient] {}", sLastTestSummary);
            }
            sTestPendingSeq = 0;
        } else if (NowMs() - sTestSentAtMs > 30000) {
            sLastTestSummary = "Test #" + std::to_string(sTestPendingSeq) + ": 30s timeout, no response";
            SPDLOG_ERROR("[FleetOracleClient] {}", sLastTestSummary);
            sTestPendingSeq = 0;
        }
        return;
    }

    int test = CVarGetInteger("gFleetOracle.Test", 0);
    if (test == 0) {
        return;
    }
    CVarSetInteger("gFleetOracle.Test", 0);

    if (test == 1) {
        sTestPendingSeq = FleetOracle_SendManifestRequest();
    } else if (test == 2) {
        sTestPendingSeq = FleetOracle_SendReachableRequest({}, {});
    } else {
        // Inventario FC completo: cada item compartido a su nivel máximo de cadena
        std::vector<std::pair<int, int>> fcItems;
        for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
            fcItems.push_back({ gFcComboItems[i].fcId, (int)gFcComboItems[i].chainLen });
        }
        sTestPendingSeq = FleetOracle_SendReachableRequest(fcItems, {});
    }
    if (sTestPendingSeq != 0) {
        sTestSentAtMs = NowMs();
        SPDLOG_INFO("[FleetOracleClient] test {} enviado (seq #{})", test, sTestPendingSeq);
    }
}

void RegisterFleetOracleClient() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(ProcessOracleTest);
}

static RegisterShipInitFunc initFleetOracleClient(RegisterFleetOracleClient, {});

} // namespace

std::string FleetOracle_GetLastTestSummary() {
    return sLastTestSummary;
}
