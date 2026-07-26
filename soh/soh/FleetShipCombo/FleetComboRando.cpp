// FleetComboRando.cpp — Generador del Combo Randomizer (Fases 2+3). Ver FleetComboRando.h.
//
// Flujo del thread de generación (GenerateCombo):
//   1. manifest del oráculo (bloqueante) -> checks MM, pool MM clasificado, starting items, opciones.
//   2. PrepareComboData: items FC ambos-lados (dedupe de ambos pools), triforce combo, split prog/junk MM.
//   3. Settings de SoH: SetAllToContext + forzar entrances OFF y songs Anywhere.
//   4. GenerateRandomizer nativo (síncrono en este thread). Dentro de Fill() corre nuestro
//      FleetCombo_PrePlacementHook por intento (assumed fill cross-game con el oráculo).
//   5. Spoiler MM (2S2H_RANDO_SPOILER) -> fleet_oracle_spoiler.json -> op prepareSeed.
//   6. SetSeedGenerated(true): el botón Randomizer de file select ya crea la seed combo en OoT;
//      el save pareado de MM aplica su spoiler vía OnFileCreate (mecanismo vanilla de 2ship).

#include "FleetComboRando.h"
#include "FleetShipCombo.h"
#include "FleetOracleClient.h"
#include "FleetComboItems.h"
#include "FleetComboItemsGlue.h"

#include "soh/Enhancements/randomizer/3drando/fill.hpp"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"
#include "soh/Enhancements/randomizer/3drando/menu.hpp"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/logic.h"
#include <libultraship/libultraship.h> // Ship::Context::GetPathRelativeToAppDirectory (spoiler I/O)
#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// ---------- estado ----------

std::atomic<bool> sRunning{ false };
std::atomic<bool> sComboActive{ false }; // gate del hook dentro de Fill()
std::thread sThread;
std::mutex sStatusMx;
std::string sStatus = "";

void SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(sStatusMx);
    sStatus = status;
    SPDLOG_INFO("[FleetComboRando] {}", status);
}

// datos del manifest de MM
struct MmCheck {
    int id;
    std::string name;
};
std::vector<MmCheck> sMmChecks;
std::unordered_map<int, std::string> sMmCheckName;
std::vector<std::string> sMmPoolProg; // progresión nativa MM (tras dedupe FC)
std::vector<std::string> sMmPoolJunk; // junk/health/trap de MM (relleno local)
std::unordered_set<std::string> sMmManifestPool; // TODOS los nombres RI del pool MM (pre-dedupe FC) —
                                                 // para el filtro per-juego: un FC MM-native solo cruza si su RI está barajado en MM
std::vector<std::string> sMmStarting;
nlohmann::json sMmOptions; // { "RO_X": value }

// items FC ambos-lados (cross-game)
struct ComboFcItem {
    int fcId;
    int rg;             // RandomizerGet (int) — lado OoT
    std::string riName; // "RI_*" — lado MM (spoilerName real de 2ship)
    int count;          // copias (nivel de cadena, v1)
};
std::vector<ComboFcItem> sComboFc;
bool sFcFilterApplied = false; // filtro per-juego aplicado a sComboFc (una vez por generación)

// resultado de la pre-colocación (persiste al terminar Fill para el spoiler)
std::map<std::string, std::string> sMmPlacements; // RC_name -> RI_name

std::mt19937 sRng;

uint64_t NowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ---------- oráculo bloqueante (corre en el thread de generación; el pump es por archivos+shm) ----------

nlohmann::json OracleBlocking(unsigned long long seq, const char* what, uint64_t timeoutMs = 60000) {
    if (seq == 0) {
        throw std::runtime_error(std::string("no active combo to request ") + what);
    }
    uint64_t start = NowMs();
    nlohmann::json resp;
    while (!FleetOracle_TryGetResponse(seq, resp)) {
        if (NowMs() - start > timeoutMs) {
            throw std::runtime_error(std::string("timeout waiting for ") + what + " from the oracle");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (resp.contains("error")) {
        throw std::runtime_error("oracle: " + resp["error"].get<std::string>());
    }
    return resp;
}

// Memo del oráculo: la reachability es determinista dado (fcItems, mmItems) + opciones MM (fijas
// durante una generación). El fixpoint de computeWorlds y pasos de colocación repiten inventarios
// idénticos MUCHAS veces; sin memo son miles de round-trips IPC que saturan el handshake (timeouts /
// colisiones de archivo). Se limpia al arrancar cada generación (PrepareComboFcItems).
std::unordered_map<std::string, std::set<int>> sOracleReachCache;

std::set<int> OracleReachable(const std::vector<std::pair<int, int>>& fcItems,
                              const std::vector<std::pair<std::string, int>>& mmItems) {
    // firma canónica (ordenada: los pares vienen de unordered_map, orden no determinista)
    std::vector<std::pair<int, int>> fcSorted(fcItems);
    std::sort(fcSorted.begin(), fcSorted.end());
    std::vector<std::pair<std::string, int>> mmSorted(mmItems);
    std::sort(mmSorted.begin(), mmSorted.end());
    std::string sig;
    sig.reserve(fcSorted.size() * 8 + mmSorted.size() * 24);
    for (auto& [id, c] : fcSorted) {
        sig += 'f';
        sig += std::to_string(id);
        sig += ':';
        sig += std::to_string(c);
        sig += ';';
    }
    for (auto& [name, c] : mmSorted) {
        sig += 'm';
        sig += name;
        sig += ':';
        sig += std::to_string(c);
        sig += ';';
    }
    auto it = sOracleReachCache.find(sig);
    if (it != sOracleReachCache.end()) {
        return it->second;
    }
    nlohmann::json resp = OracleBlocking(FleetOracle_SendReachableRequest(fcItems, mmItems), "reachable", 45000);
    std::set<int> out;
    for (auto& id : resp["reachable"]) {
        out.insert(id.get<int>());
    }
    sOracleReachCache.emplace(std::move(sig), out);
    return out;
}

// ---------- preparación de datos ----------

void ParseManifest(const nlohmann::json& manifest) {
    sMmChecks.clear();
    sMmCheckName.clear();
    sMmPoolProg.clear();
    sMmPoolJunk.clear();
    sMmManifestPool.clear();
    sMmStarting.clear();
    sMmOptions = nlohmann::json::object();

    // set de nombres RI cubiertos por FC ambos-lados (para dedupe del pool MM)
    std::unordered_set<std::string> fcPeerNames;
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        if (FcCombo_NativeForItem(gFcComboItems[i].fcId) != FCI_NO_ITEM) {
            std::string peer = FcCombo_PeerNameForItem(gFcComboItems[i].fcId);
            if (peer != "FCI_NO_ITEM") {
                fcPeerNames.insert(peer);
            }
        }
    }

    for (auto& pair : manifest["checks"]) {
        sMmChecks.push_back({ pair[0].get<int>(), pair[1].get<std::string>() });
        sMmCheckName[pair[0].get<int>()] = pair[1].get<std::string>();
    }
    for (auto& entry : manifest["pool"]) {
        std::string name = entry[0].get<std::string>();
        std::string cat = entry[1].get<std::string>();
        sMmManifestPool.insert(name); // TODO nombre del pool MM (incl. los que FC dedupe) para el filtro per-juego
        if (name == "RI_TRIFORCE_PIECE" || fcPeerNames.contains(name)) {
            continue; // FC lo aporta (1 copia global) o lo controla el goal combo
        }
        if (cat == "prog") {
            sMmPoolProg.push_back(name);
        } else {
            sMmPoolJunk.push_back(name);
        }
    }
    for (auto& name : manifest["startingItems"]) {
        sMmStarting.push_back(name.get<std::string>());
    }
    for (auto& opt : manifest["options"]) { // [name, cvar, value]
        sMmOptions[opt[0].get<std::string>()] = opt[2].get<int64_t>();
    }
}

void PrepareComboFcItems() {
    sComboFc.clear();
    sFcFilterApplied = false; // se recomputa el filtro per-juego en la 1ª pasada de pre-colocación
    sOracleReachCache.clear(); // memo del oráculo válido solo dentro de una generación
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        const FcComboItemInfo& info = gFcComboItems[i];
        // corazones/traps/triforce se manejan aparte (economías nativas / goal combo)
        if ((info.flags & FCI_F_TRAP) || (info.flags & FCI_F_TRIFORCE) ||
            std::string(info.comboName).rfind("Piece of Heart", 0) == 0 ||
            std::string(info.comboName).rfind("Heart Container", 0) == 0) {
            continue;
        }
        int rg = FcCombo_NativeForItem(info.fcId);
        std::string riName = FcCombo_PeerNameForItem(info.fcId);
        if (rg == FCI_NO_ITEM || riName == "FCI_NO_ITEM") {
            continue; // solo los AMBOS-lados son cross; RG-only quedan en el pool nativo de SoH,
                      // RI-only quedan en el pool nativo de MM (manifest)
        }
        sComboFc.push_back({ info.fcId, rg, riName, (int)info.chainLen });
    }
    // Goal Triforce Hunt: piezas al pool cross (ambos-lados por la tabla FC)
    if (CVarGetInteger("gFleetCombo.GoalMode", 0) == 1) {
        int total = CVarGetInteger("gFleetCombo.TriforceTotal", 15);
        int rg = FcCombo_NativeForItem(FCI_TRIFORCE_PIECE);
        std::string riName = FcCombo_PeerNameForItem(FCI_TRIFORCE_PIECE);
        if (rg != FCI_NO_ITEM && riName != "FCI_NO_ITEM" && total > 0) {
            sComboFc.push_back({ FCI_TRIFORCE_PIECE, rg, riName, total });
        }
    }
}

// ---------- la pre-colocación (corre DENTRO de Fill(), por intento) ----------

bool RunComboPrePlacement() {
    auto ctx = Rando::Context::GetInstance();
    sMmPlacements.clear();

    // FILTRO PER-JUEGO (respeta las opciones de randomizer de cada juego): un item FC solo se
    // cross-coloca si está BARAJADO en su juego de origen — su RG en el itemPool de OoT, o su RI en
    // el pool del manifest de MM. Si no está en ninguno (categoría en vanilla/off) se OMITE: se queda
    // en su colocación nativa, sin duplicarse. El goal Triforce combo siempre se conserva.
    // Se aplica una sola vez por generación (aquí el itemPool aún está completo, antes del dedupe).
    if (!sFcFilterApplied) {
        sFcFilterApplied = true;
        std::unordered_set<int> ootPoolRgs;
        for (RandomizerGet rg : itemPool) {
            ootPoolRgs.insert((int)rg);
        }
        std::vector<ComboFcItem> active;
        for (auto& it : sComboFc) {
            bool inOot = ootPoolRgs.contains(it.rg);
            bool inMm = sMmManifestPool.contains(it.riName);
            if (inOot || inMm || it.fcId == FCI_TRIFORCE_PIECE) {
                active.push_back(it);
            }
        }
        sComboFc = std::move(active);
    }

    // 1) dedupe del itemPool nativo de SoH: fuera TODAS las copias de los RG cubiertos por FC
    std::unordered_set<int> fcRgs;
    for (auto& item : sComboFc) {
        fcRgs.insert(item.rg);
    }
    std::erase_if(itemPool, [&](RandomizerGet rg) { return fcRgs.contains((int)rg); });

    // 2) locations OoT vetadas para la pre-colocación (etapas nativas las necesitan libres)
    std::unordered_set<int> bannedOot;
    for (RandomizerCheck rc : Rando::StaticData::dungeonRewardLocations) {
        bannedOot.insert((int)rc);
    }
    bannedOot.insert((int)RC_LINKS_POCKET);
    bannedOot.insert((int)RC_GIFT_FROM_RAURU);
    // Centinelas del PORTAL (no son item locations): se consultan en la búsqueda pero jamás se
    // usan como candidatos de colocación.
    bannedOot.insert((int)RC_ALTAR_HINT_CHILD);
    bannedOot.insert((int)RC_ALTAR_HINT_ADULT);

    // GATE DEL PORTAL: MM solo es alcanzable si la lógica de OoT puede llegar DENTRO del Temple of
    // Time (donde vive el fleet hole). Centinela = los checks de altar de RR_TEMPLE_OF_TIME
    // (child o adult adentro). Se añaden a la lista de búsqueda para poder consultarlos.
    std::vector<RandomizerCheck> searchLocations = ctx->allLocations;
    searchLocations.push_back(RC_ALTAR_HINT_CHILD);
    searchLocations.push_back(RC_ALTAR_HINT_ADULT);

    // 3) lista de trabajo: copias FC + progresión nativa MM, en orden aleatorio
    struct WorkItem {
        int fcIdx;          // >=0: índice en sComboFc; -1: item nativo MM
        std::string mmName; // válido si fcIdx == -1
        bool gates;         // el item AFECTA la lógica (advancement). Ver el porqué en el bucle:
                            // si NO gatea, sacarlo del inventario asumido no puede cambiar la
                            // reachability, así que se reutiliza la cacheada sin consultar al oráculo.
    };
    std::vector<WorkItem> work;
    std::vector<int> fcUnplaced(sComboFc.size());
    for (size_t i = 0; i < sComboFc.size(); i++) {
        fcUnplaced[i] = sComboFc[i].count;
        // IsAdvancement() = la noción nativa de SoH de "este item abre lógica". Los que no lo son
        // (rupias, corazones, souls/tokens no-gate, ...) jamás cambian qué es alcanzable.
        bool gates = Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[i].rg).IsAdvancement();
        for (int k = 0; k < sComboFc[i].count; k++) {
            work.push_back({ (int)i, "", gates });
        }
    }
    std::vector<std::string> mmUnplaced = sMmPoolProg;
    for (auto& name : sMmPoolProg) {
        // Conservador: el manifest de MM solo distingue prog/junk/health/trap, así que todo lo que
        // llega aquí ya es "prog" y lo tratamos como gateante (recalcula). Si más adelante el
        // manifest manda el RITYPE fino, los LESSER podrían saltar el recomputo también.
        work.push_back({ -1, name, true });
    }
    std::shuffle(work.begin(), work.end(), sRng);

    // 4) estado de colocación cross
    std::vector<std::pair<int, int>> placedMmFc;                  // (mmCheckId, fcIdx)
    std::vector<std::pair<int, std::string>> placedMmNat;         // (mmCheckId, riName)
    std::vector<std::pair<RandomizerCheck, int>> placedOotFc;     // (rc, fcIdx)
    std::unordered_set<int> usedMmChecks;

    // Estado del fixpoint PERSISTENTE entre items (warm start). La reachability apenas cambia de un
    // item al siguiente, así que arrancar desde el resultado anterior converge normalmente en 1
    // iteración en vez de 6 — y cada iteración es un round-trip IPC al oráculo. Mismo punto fijo,
    // solo mejor semilla.
    std::set<int> fpCollectedMmFcIdx; // FC colocados en MM alcanzables
    std::set<int> fpPrevMmReach;

    // reachability de ambos mundos con colección cruzada (fixpoint)
    auto computeWorlds = [&](std::set<int>& outOot, std::set<int>& outMm) {
        std::set<int>& collectedMmFcIdx = fpCollectedMmFcIdx; // warm start (persistente)
        std::set<int>& prevMmReach = fpPrevMmReach;
        std::set<int> prevPrevMmReach; // estado de hace 2 iteraciones (detector de ciclo-2)
        for (int iter = 0; iter < 6; iter++) {
            // --- OoT: assumed = FC sin colocar + FC-en-MM alcanzables + pool nativo OoT restante ---
            logic->Reset();
            for (size_t i = 0; i < sComboFc.size(); i++) {
                for (int k = 0; k < fcUnplaced[i]; k++) {
                    Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[i].rg).ApplyEffect();
                }
            }
            for (int fcIdx : collectedMmFcIdx) {
                Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[fcIdx].rg).ApplyEffect();
            }
            for (RandomizerGet rg : itemPool) {
                if (Rando::StaticData::RetrieveItem(rg).IsAdvancement()) {
                    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
                }
            }
            std::vector<RandomizerCheck> reach = ReachabilitySearch(searchLocations);
            outOot.clear();
            for (RandomizerCheck rc : reach) {
                outOot.insert((int)rc);
            }
            // GATE: ¿puede la lógica de OoT entrar al Temple of Time (portal a MM)?
            bool portalOpen = outOot.contains((int)RC_ALTAR_HINT_CHILD) || outOot.contains((int)RC_ALTAR_HINT_ADULT);
            // FC colocados en OoT alcanzables (para el inventario de MM)
            std::set<int> collectedOotFcIdx;
            for (auto& [rc, fcIdx] : placedOotFc) {
                if (outOot.contains((int)rc)) {
                    collectedOotFcIdx.insert(fcIdx);
                }
            }

            // --- MM: inventario = FC (sin colocar + colocados alcanzables en cualquiera de los dos)
            //         + nativos MM (sin colocar + colocados alcanzables) ---
            std::unordered_map<int, int> fcCounts; // fcId -> count
            for (size_t i = 0; i < sComboFc.size(); i++) {
                fcCounts[sComboFc[i].fcId] += fcUnplaced[i];
            }
            for (int fcIdx : collectedOotFcIdx) {
                fcCounts[sComboFc[fcIdx].fcId]++;
            }
            for (int fcIdx : collectedMmFcIdx) {
                fcCounts[sComboFc[fcIdx].fcId]++;
            }
            std::vector<std::pair<int, int>> fcPairs;
            for (auto& [fcId, count] : fcCounts) {
                if (count > 0) {
                    fcPairs.push_back({ fcId, count });
                }
            }
            std::unordered_map<std::string, int> mmCounts;
            for (auto& name : mmUnplaced) {
                mmCounts[name]++;
            }
            for (auto& [checkId, name] : placedMmNat) {
                if (prevMmReach.contains(checkId)) {
                    mmCounts[name]++;
                }
            }
            std::vector<std::pair<std::string, int>> mmPairs(mmCounts.begin(), mmCounts.end());

            // GATE DEL PORTAL: si OoT no puede entrar al Temple of Time con este estado, MM entero
            // es inalcanzable (no se consulta el oráculo; nada de MM se colecciona esta iteración).
            std::set<int> mmReach;
            if (portalOpen) {
                mmReach = OracleReachable(fcPairs, mmPairs);
            }
            outMm = mmReach;

            // --- colección MM y convergencia ---
            std::set<int> newCollectedMmFcIdx;
            for (auto& [checkId, fcIdx] : placedMmFc) {
                if (mmReach.contains(checkId)) {
                    newCollectedMmFcIdx.insert(fcIdx);
                }
            }
            bool stable = (newCollectedMmFcIdx == collectedMmFcIdx) && (mmReach == prevMmReach);
            // Guarda de oscilación: si la reachability de MM iguala la de HACE DOS iteraciones, el
            // fixpoint está en un ciclo-2 (observado: 1873<->1166) que jamás cumple `stable` — quemaría
            // las 6 iteraciones (cada una un round-trip IPC al oráculo) por CADA item. Aceptamos el
            // estado actual y cortamos. Sin esto una seed Glitchless hace miles de IPC de más.
            bool oscillating = (iter >= 2) && (mmReach == prevPrevMmReach);
            collectedMmFcIdx = std::move(newCollectedMmFcIdx);
            prevPrevMmReach = std::move(prevMmReach);
            prevMmReach = std::move(outMm);
            outMm = prevMmReach;
            if (stable || oscillating) {
                break;
            }
        }
    };

    // 5) bucle de colocación assumed
    // No Logic: la reachability es irrelevante (toda location es válida). Saltamos el fixpoint de
    // reachability cross-mundo por-item — hace un round-trip IPC al oráculo por mundo, por iteración
    // (6), por item: MILES de round-trips con un pool grande, y por eso una seed No-Logic parecía
    // colgarse (2400+ requests sin converger). En No Logic tratamos TODAS las locations de OoT + TODOS
    // los checks de MM como alcanzables (construidos una vez, cero tráfico al oráculo).
    const bool noLogic = CVarGetInteger("gFleetCombo.Logic", 0) == 1;
    std::set<int> allOotReach, allMmReach;
    if (noLogic) {
        for (RandomizerCheck rc : ctx->allLocations) {
            allOotReach.insert((int)rc);
        }
        for (auto& c : sMmChecks) {
            allMmReach.insert(c.id);
        }
        SetStatus("No Logic: placing combo items without reachability (no oracle round-trips)");
    }

    // CACHE de reachability entre items. Por qué es válido: esto es un ASSUMED fill — el inventario
    // asumido son TODOS los items sin colocar, y como solo colocamos en sitios ALCANZABLES, lo ya
    // colocado sigue contando como recolectado. O sea el inventario efectivo (y por tanto lo
    // alcanzable) prácticamente no cambia durante la pasada. Solo puede cambiar al sacar del asumido
    // un item que GATEA lógica; para los que no gatean (la mayoría del pool) reutilizamos la cache y
    // colocamos sin consultar al oráculo. Si la cache quedara corta, el fallback de "sin candidatos"
    // fuerza un recomputo antes de abortar el intento.
    std::set<int> cachedOot, cachedMm;
    bool cacheValid = false;
    int oracleComputes = 0;

    int placedCount = 0;
    int total = (int)work.size();
    for (WorkItem& item : work) {
        // retirar del "unplaced" ANTES de computar reachability (assumed = todo menos este)
        if (item.fcIdx >= 0) {
            fcUnplaced[item.fcIdx]--;
        } else {
            auto it = std::find(mmUnplaced.begin(), mmUnplaced.end(), item.mmName);
            if (it != mmUnplaced.end()) {
                mmUnplaced.erase(it);
            }
        }

        const std::set<int>& ootReach = noLogic ? allOotReach : cachedOot;
        const std::set<int>& mmReach = noLogic ? allMmReach : cachedMm;
        if (!noLogic && (item.gates || !cacheValid)) {
            computeWorlds(cachedOot, cachedMm);
            cacheValid = true;
            oracleComputes++;
        }

        // candidatos POR MUNDO. CRÍTICO: los checks MM se intersectan con el manifest (el oráculo
        // devuelve la reachability de TODO el grafo, incluidos checks inactivos por opciones —
        // colocar ahí perdería el item: no tienen nombre en el pool y el juego no los daría).
        std::vector<int> ootCands;
        std::vector<int> mmCands;
        // Dos pasadas: si la 1ª no da candidatos y estábamos usando la cache, puede que se haya
        // quedado corta -> recalcula con el oráculo UNA vez y reintenta antes de abortar el intento
        // entero (abortar re-hace todo el fill, es carísimo). Es la red de seguridad de la cache.
        for (int pass = 0; pass < 2; pass++) {
            ootCands.clear();
            mmCands.clear();
            if (item.fcIdx >= 0) {
                for (int rcInt : ootReach) {
                    if (bannedOot.contains(rcInt)) {
                        continue;
                    }
                    auto* loc = ctx->GetItemLocation((RandomizerCheck)rcInt);
                    if (loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE) {
                        ootCands.push_back(rcInt);
                    }
                }
            }
            for (int checkId : mmReach) {
                if (!usedMmChecks.contains(checkId) && sMmCheckName.contains(checkId)) {
                    mmCands.push_back(checkId);
                }
            }
            if (!ootCands.empty() || !mmCands.empty() || noLogic || pass == 1) {
                break;
            }
            computeWorlds(cachedOot, cachedMm); // cache posiblemente obsoleta: refresca y reintenta
            cacheValid = true;
            oracleComputes++;
        }
        if (ootCands.empty() && mmCands.empty()) {
            SetStatus("Pre-placement: no candidates for " +
                      std::string(item.fcIdx >= 0 ? sComboFc[item.fcIdx].riName : item.mmName) + " - retrying");
            return false;
        }

        // Balance de mundos: 50/50 cuando ambos tienen hueco (OoT suele tener ~7x locations que
        // MM; la elección uniforme sobre el total mandaría casi todo a OoT).
        bool pickMm;
        if (ootCands.empty()) {
            pickMm = true;
        } else if (mmCands.empty()) {
            pickMm = false;
        } else {
            pickMm = std::uniform_int_distribution<int>(0, 1)(sRng) == 1;
        }
        int world = pickMm ? 1 : 0;
        auto& pool = pickMm ? mmCands : ootCands;
        int id = pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(sRng)];
        if (world == 0) {
            ctx->PlaceItemInLocation((RandomizerCheck)id, (RandomizerGet)sComboFc[item.fcIdx].rg);
            placedOotFc.push_back({ (RandomizerCheck)id, item.fcIdx });
        } else {
            std::string riName = item.fcIdx >= 0 ? sComboFc[item.fcIdx].riName : item.mmName;
            sMmPlacements[sMmCheckName[id]] = riName;
            usedMmChecks.insert(id);
            if (item.fcIdx >= 0) {
                placedMmFc.push_back({ id, item.fcIdx });
            } else {
                placedMmNat.push_back({ id, riName });
            }
        }
        placedCount++;
        if ((placedCount % 10) == 0) {
            SetStatus("Placing combo items: " + std::to_string(placedCount) + "/" + std::to_string(total));
        }
    }

    // 6) los FC que cayeron en MM pasan al StartingInventory LÓGICO del fill nativo de OoT
    //    (sound: se colocaron con assumed fill y MM no depende de items OoT-native)
    for (auto& [checkId, fcIdx] : placedMmFc) {
        StartingInventory.push_back((RandomizerGet)sComboFc[fcIdx].rg);
    }

    // 7) junk MM: cada check del manifest sin item recibe relleno local
    std::vector<std::string> junk = sMmPoolJunk;
    std::shuffle(junk.begin(), junk.end(), sRng);
    size_t junkIdx = 0;
    for (auto& check : sMmChecks) {
        if (usedMmChecks.contains(check.id)) {
            continue;
        }
        sMmPlacements[check.name] = junkIdx < junk.size() ? junk[junkIdx++] : "RI_JUNK";
    }

    SetStatus("Combo pre-placement OK: " + std::to_string(placedCount) + " items (" +
              std::to_string(placedMmFc.size() + placedMmNat.size()) + " in MM), " +
              std::to_string(oracleComputes) + " reachability computes for " + std::to_string(total) + " items");
    return true;
}

// ---------- spoiler MM + prepareSeed (Fase 3) ----------

std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

nlohmann::json sLastMmSpoiler; // el spoiler MM de la última generación (para el .fleet)

void WriteMmSpoilerAndPrepare(const std::string& seedString) {
    auto ctx = Rando::Context::GetInstance();

    nlohmann::json spoiler;
    spoiler["type"] = "2S2H_RANDO_SPOILER";
    spoiler["inputSeed"] = seedString;
    spoiler["finalSeed"] = (uint32_t)ctx->GetSeed();
    spoiler["options"] = sMmOptions;
    spoiler["startingItems"] = sMmStarting;
    nlohmann::json checks = nlohmann::json::object();
    for (auto& [checkName, itemName] : sMmPlacements) {
        checks[checkName] = itemName;
    }
    spoiler["checks"] = checks;
    sLastMmSpoiler = spoiler;
    // metadata combo (2ship la ignora al aplicar; la Fase 5 la leerá para las metas)
    spoiler["fleetCombo"] = { { "goalMode", CVarGetInteger("gFleetCombo.GoalMode", 0) },
                              { "triforceTotal", CVarGetInteger("gFleetCombo.TriforceTotal", 15) },
                              { "triforceRequired", CVarGetInteger("gFleetCombo.TriforceRequired", 10) } };

    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    std::filesystem::path bridge = SelfExeDir() / "fleet" / "oracle_spoiler.json";
    {
        std::filesystem::path tmp = bridge;
        tmp += ".tmp";
        std::ofstream out(tmp);
        out << spoiler << std::endl;
        out.close();
        std::filesystem::rename(tmp, bridge);
    }

    std::string fileName = "combo_" + seedString + ".json";
    nlohmann::json resp = OracleBlocking(FleetOracle_SendPrepareSeedRequest(fileName), "prepareSeed");
    SPDLOG_INFO("[FleetComboRando] MM spoiler installed: {} (index {})", fileName,
                resp.value("spoilerIndex", 0));
}

// Spoiler de VERIFICACIÓN: dónde quedó cada item en ambos mundos + conteos por item, para
// comprobar cantidades/nombres a mano. Se escribe en <ShipDir>/combo_<seed>_summary.json.
void WriteComboSummary(const std::string& seedString) {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json summary;
    summary["seed"] = seedString;

    // Placements + conteos de OoT (nombres display del itemTable de soh)
    nlohmann::json ootPlacements = nlohmann::json::object();
    std::map<std::string, int> ootCounts;
    for (RandomizerCheck rc : ctx->allLocations) {
        auto* loc = ctx->GetItemLocation(rc);
        if (loc == nullptr || loc->GetPlacedRandomizerGet() == RG_NONE) {
            continue;
        }
        std::string itemName = Rando::StaticData::RetrieveItem(loc->GetPlacedRandomizerGet()).GetName().GetEnglish();
        auto* staticLoc = Rando::StaticData::GetLocation(rc);
        std::string locName = staticLoc != nullptr ? staticLoc->GetName() : ("RC_" + std::to_string((int)rc));
        ootPlacements[locName] = itemName;
        ootCounts[itemName]++;
    }
    summary["ootPlacements"] = ootPlacements;
    summary["ootItemCounts"] = ootCounts;

    // Placements + conteos de MM (nombres RI_* del spoiler)
    nlohmann::json mmPlacements = nlohmann::json::object();
    std::map<std::string, int> mmCounts;
    for (auto& [checkName, itemName] : sMmPlacements) {
        mmPlacements[checkName] = itemName;
        mmCounts[itemName]++;
    }
    summary["mmPlacements"] = mmPlacements;
    summary["mmItemCounts"] = mmCounts;

    // Verificación por item FC cross: cuántas copias cayeron en cada mundo vs las esperadas
    nlohmann::json fcItems = nlohmann::json::array();
    for (auto& fc : sComboFc) {
        std::string ootName = Rando::StaticData::RetrieveItem((RandomizerGet)fc.rg).GetName().GetEnglish();
        int inOot = ootCounts.count(ootName) ? ootCounts[ootName] : 0;
        int inMm = mmCounts.count(fc.riName) ? mmCounts[fc.riName] : 0;
        fcItems.push_back({ { "combo", ootName },
                            { "ootName", ootName },
                            { "mmName", fc.riName },
                            { "expected", fc.count },
                            { "placedOot", inOot },
                            { "placedMm", inMm },
                            { "total", inOot + inMm },
                            { "ok", (inOot + inMm) == fc.count } });
    }
    summary["fcCrossItems"] = fcItems;

    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    std::filesystem::path out = SelfExeDir() / "fleet" / ("combo_" + seedString + "_summary.json");
    std::ofstream file(out);
    file << summary.dump(4) << std::endl;
    SPDLOG_INFO("[FleetComboRando] summary de verificación: {}", out.string());
}

// ---------- .fleet: guardar/cargar una seed combo completa (ambos spoilers) ----------

std::filesystem::path FleetDir() {
    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    return SelfExeDir() / "fleet";
}

// Escribe <ShipDir>/fleet/<seed>.fleet = { oot spoiler, mm spoiler, combo config }.
// El spoiler de OoT se toma del archivo que SpoilerLog_Write dejó (ruta en gGeneral.SpoilerLog).
void WriteFleetFile(const std::string& seedString) {
    nlohmann::json fleet;
    fleet["type"] = "FLEET_COMBO_SEED";
    fleet["version"] = 1;
    fleet["seed"] = seedString;
    fleet["combo"] = { { "goalMode", CVarGetInteger("gFleetCombo.GoalMode", 0) },
                       { "triforceTotal", CVarGetInteger("gFleetCombo.TriforceTotal", 15) },
                       { "triforceRequired", CVarGetInteger("gFleetCombo.TriforceRequired", 10) } };
    fleet["mm"] = sLastMmSpoiler;

    // OoT spoiler: SpoilerLog_Write (dentro de GenerateRandomizer) dejó la ruta en este CVar.
    std::string ootPath = CVarGetString("gGeneral.SpoilerLog", "");
    if (!ootPath.empty()) {
        try {
            std::ifstream in(ootPath);
            nlohmann::json ootSpoiler;
            in >> ootSpoiler;
            fleet["oot"] = ootSpoiler;
        } catch (...) {
            SPDLOG_WARN("[FleetComboRando] no se pudo leer el spoiler OoT en {}", ootPath);
        }
    }

    std::filesystem::path out = FleetDir() / (seedString + ".fleet");
    std::ofstream file(out);
    file << fleet.dump(2) << std::endl;
    SPDLOG_INFO("[FleetComboRando] .fleet guardado: {}", out.string());
}

std::vector<std::string> ListFleetFiles() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(FleetDir(), ec)) {
        if (e.is_regular_file() && e.path().extension() == ".fleet") {
            out.push_back(e.path().filename().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Carga una seed combo desde un .fleet SIN regenerar: aplica el spoiler OoT al Context
// (ParseSpoiler -> mSpoilerLoaded) y manda el spoiler MM a 2ship (prepareSeed).
void LoadFleetThread(std::string fileName, int slot, std::string name) {
    try {
        SetStatus("Loading " + fileName + "...");
        std::filesystem::path path = FleetDir() / fileName;
        nlohmann::json fleet;
        {
            std::ifstream in(path);
            in >> fleet;
        }
        if (!fleet.contains("type") || fleet["type"] != "FLEET_COMBO_SEED") {
            throw std::runtime_error("not a FLEET_COMBO_SEED file");
        }

        // Config combo
        if (fleet.contains("combo")) {
            auto& c = fleet["combo"];
            CVarSetInteger("gFleetCombo.GoalMode", c.value("goalMode", 0));
            CVarSetInteger("gFleetCombo.TriforceTotal", c.value("triforceTotal", 15));
            CVarSetInteger("gFleetCombo.TriforceRequired", c.value("triforceRequired", 10));
        }

        // OoT: escribir el spoiler embebido a Randomizer/ y ParseSpoiler (fija mSpoilerLoaded=true)
        if (fleet.contains("oot")) {
            std::string ootFile = Ship::Context::GetPathRelativeToAppDirectory("Randomizer/_fleet_oot.json");
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(ootFile).parent_path(), ec);
            {
                std::ofstream out(ootFile);
                out << fleet["oot"] << std::endl;
            }
            Rando::Context::GetInstance()->ParseSpoiler(ootFile.c_str());
            CVarSetString("gGeneral.SpoilerLog", ("./Randomizer/_fleet_oot.json"));
        } else {
            throw std::runtime_error(".fleet has no OoT spoiler");
        }

        // MM: mandar el spoiler embebido al bridge + prepareSeed en 2ship
        if (fleet.contains("mm")) {
            std::error_code ec;
            std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
            std::filesystem::path bridge = SelfExeDir() / "fleet" / "oracle_spoiler.json";
            {
                std::filesystem::path tmp = bridge;
                tmp += ".tmp";
                std::ofstream out(tmp);
                out << fleet["mm"] << std::endl;
                out.close();
                std::filesystem::rename(tmp, bridge);
            }
            std::string mmFile = "combo_" + fleet.value("seed", std::string("fleet")) + ".json";
            OracleBlocking(FleetOracle_SendPrepareSeedRequest(mmFile), "prepareSeed");
        } else {
            throw std::runtime_error(".fleet has no MM spoiler");
        }

        Rando::Context::GetInstance()->SetSpoilerLoaded(true);

        // Loading a .fleet now BAKES the seed straight into the chosen slot in BOTH games (so OoT
        // File N and MM File N always share this seed — no separate "Create save pair" step, which was
        // the source of "MM loaded a different seed"). MM's slot file is written now via the oracle;
        // OoT's is created the next time you reach its title/file-select (FleetCreateSaveTick).
        if (slot >= 0 && slot <= 2) {
            // Encode the name to the file-select charset (digits, A-Z=10.., a-z=36.., space/pad=62).
            unsigned char encoded[8];
            for (int i = 0; i < 8; i++) {
                encoded[i] = 62;
                char c = i < (int)name.size() ? name[i] : '\0';
                if (c >= '0' && c <= '9') {
                    encoded[i] = (unsigned char)(c - '0');
                } else if (c >= 'A' && c <= 'Z') {
                    encoded[i] = (unsigned char)(10 + c - 'A');
                } else if (c >= 'a' && c <= 'z') {
                    encoded[i] = (unsigned char)(36 + c - 'a');
                }
            }
            FleetCombo_RequestCreateSave(slot, encoded);                                 // OoT (deferred to file-select)
            OracleBlocking(FleetOracle_SendCreateSaveRequest(slot, name), "createSave");  // MM (written now)
            SetStatus("Loaded " + fileName + " into File " + std::to_string(slot + 1) +
                      ". MM slot written; go to OoT's title/file-select to finish OoT's slot, then load File " +
                      std::to_string(slot + 1) + ".");
        } else {
            SetStatus("Loaded " + fileName + ". Use 'Create saves in BOTH games' below, then load File N.");
        }
    } catch (const std::exception& e) {
        SetStatus(std::string("Load failed: ") + e.what());
    }
    sRunning = false;
}

// ---------- thread principal ----------

// Aplica los settings COMBO unificados a AMBOS generadores antes de generar (overwrite), para que
// las dos lógicas se generen con la misma config. gFleetCombo.ItemPool: 0 Scarce/1 Normal/2 Plentiful;
// gFleetCombo.Logic: 0 Glitchless/1 No Logic; gFleetCombo.StartingAge: 0 Child/1 Adult/2 Random.
void ApplyComboSettingsToBothGames() {
    int itemPool = CVarGetInteger("gFleetCombo.ItemPool", 1);
    int logic = CVarGetInteger("gFleetCombo.Logic", 0);

    // --- OoT (gRandoSettings.*; SetAllToContext los leerá luego) ---
    // Item pool 3-way -> OoT 4-value: Scarce=2, Normal=Balanced=1, Plentiful=0.
    CVarSetInteger("gRandoSettings.ItemPool", itemPool == 0 ? 2 : (itemPool == 2 ? 0 : 1));
    CVarSetInteger("gRandoSettings.LogicRules", logic);
    CVarSave();

    // --- FORCE: options incompatible with the combo -> safe value ---
    // This is the SAME table the Shared window's "Compatibility" panel reads, so what the panel
    // promises and what generation actually does cannot drift apart.
    FleetCombo_ResolveAllConflicts();

    // --- MM (gRando.Options.*; el manifest los leerá) — push bloqueante ANTES del manifest ---
    // Solo lo COMBO-global (pool/logic/triforce). El resto de opciones rando es POR-JUEGO
    // (decisión: se editan en el menú de cada juego; el combo no las sobreescribe).
    std::vector<std::pair<std::string, int>> mm = {
        { "gRando.Options.RO_PLENTIFUL_ITEMS", itemPool == 2 ? 1 : 0 }, // MM: solo plentiful on/off
        { "gRando.Options.RO_LOGIC", logic },
    };
    // Triforce Hunt combo -> MM pieces (el goal combo lo maneja; sincroniza el shuffle flag + counts)
    mm.push_back({ "gRando.Options.RO_SHUFFLE_TRIFORCE_PIECES",
                   CVarGetInteger("gFleetCombo.GoalMode", 0) == 1 ? 1 : 0 });
    mm.push_back({ "gRando.Options.RO_TRIFORCE_PIECES_MAX", CVarGetInteger("gFleetCombo.TriforceTotal", 15) });
    mm.push_back(
        { "gRando.Options.RO_TRIFORCE_PIECES_REQUIRED", CVarGetInteger("gFleetCombo.TriforceRequired", 10) });
    OracleBlocking(FleetOracle_SendSetOptionsRequest(mm), "applyMmSettings");
}

void GenerateCombo(std::string seedString) {
    try {
        SetStatus("Applying combo settings to both games...");
        ApplyComboSettingsToBothGames();

        SetStatus("Requesting MM manifest...");
        nlohmann::json manifest = OracleBlocking(FleetOracle_SendManifestRequest(), "manifest");
        ParseManifest(manifest);
        PrepareComboFcItems();
        SetStatus("Manifest OK: " + std::to_string(sMmChecks.size()) + " MM checks, " +
                  std::to_string(sMmPoolProg.size()) + " MM prog, " + std::to_string(sComboFc.size()) +
                  " cross FC items");

        auto ctx = Rando::Context::GetInstance();
        Rando::Settings::GetInstance()->SetAllToContext();
        // V1: no OoT entrance shuffle and songs set to Anywhere (see header)
        ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_BOSS_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_OVERWORLD_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_INTERIOR_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_GROTTO_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_OWL_DROPS).Set(0);
        ctx->GetOption(RSK_SHUFFLE_WARP_SONGS).Set(0);
        ctx->GetOption(RSK_SHUFFLE_OVERWORLD_SPAWNS).Set(0);
        ctx->GetOption(RSK_MIXED_ENTRANCE_POOLS).Set(0);
        ctx->GetOption(RSK_DECOUPLED_ENTRANCES).Set(0);
        ctx->GetOption(RSK_SHUFFLE_SONGS).Set(RO_SONG_SHUFFLE_ANYWHERE);
        // Wallet scale must match MM's (see the kIncompat table): no shuffled child wallet, no Tycoon.
        ctx->GetOption(RSK_SHUFFLE_CHILD_WALLET).Set(0);
        ctx->GetOption(RSK_INCLUDE_TYCOON_WALLET).Set(0);
        // El goal Triforce del combo maneja sus propias piezas (FC, contador comboTriforce):
        // el sistema nativo de Triforce Hunt de SoH queda apagado durante la generación combo.
        // Upstream sustituyó RSK_TRIFORCE_HUNT por un selector de condición de victoria, donde el
        // Triforce Hunt nativo es ahora RO_WINCON_TRIFORCE_PIECES. Fijar DEFEAT_GANON deja la
        // Trifuerza en Ganon (item_pool.cpp) y desactiva toda recolección nativa, que es lo que
        // hacía el Set(0) anterior — DEFEAT_GANON es además el primer valor del enum.
        ctx->GetOption(RSK_WINCON).Set(RO_WINCON_DEFEAT_GANON);

        if (seedString.empty()) {
            seedString = std::to_string(std::random_device{}());
        }
        sRng.seed((uint32_t)std::hash<std::string>{}(seedString));

        CVarSetInteger("gGeneral.RandoGenerating", 1);
        sComboActive = true;
        SetStatus("Generating (native fill + combo pre-placement)...");
        bool ok = GenerateRandomizer({}, {}, seedString);
        sComboActive = false;
        CVarSetInteger("gGeneral.RandoGenerating", 0);

        if (!ok) {
            throw std::runtime_error("the fill could not find a valid placement (30 attempts)");
        }

        SetStatus("Writing MM spoiler + prepareSeed...");
        WriteMmSpoilerAndPrepare(seedString);
        WriteComboSummary(seedString);
        WriteFleetFile(seedString); // .fleet portable (both spoilers) para recargar/compartir

        Rando::Context::GetInstance()->SetSeedGenerated(true);
        SetStatus("Combo seed ready (" + seedString + "). Use 'Create saves in BOTH games' below, then load "
                  "File N from OoT's file select.");
    } catch (const std::exception& e) {
        sComboActive = false;
        CVarSetInteger("gGeneral.RandoGenerating", 0);
        SetStatus(std::string("Combo generation FAILED: ") + e.what());
    }
    sRunning = false;
}

} // namespace

// =================================================================================================
// Options incompatible with the combo
// =================================================================================================
// SINGLE source of truth: ApplyComboSettingsToBothGames forces exactly this table, and the Shared
// window's "Compatibility" panel draws it. Adding an incompatibility = adding one row here, so the
// panel can never promise something different from what generation actually does.

namespace {

const char* const kCvEntrances[] = {
    "gRandoSettings.ShuffleEntrances",
    "gRandoSettings.ShuffleDungeonsEntrances",
    "gRandoSettings.ShuffleBossEntrances",
    "gRandoSettings.ShuffleGanonTowerEntrance",
    "gRandoSettings.ShuffleOverworldEntrances",
    "gRandoSettings.ShuffleInteriorsEntrances",
    "gRandoSettings.ShuffleThievesHideoutEntrances",
    "gRandoSettings.ShuffleGrottosEntrances",
    "gRandoSettings.ShuffleOwlDrops",
    "gRandoSettings.ShuffleWarpSongs",
    "gRandoSettings.ShuffleOverworldSpawns",
    "gRandoSettings.MixedEntrances",
    "gRandoSettings.MixDungeons",
    "gRandoSettings.MixBosses",
    "gRandoSettings.MixOverworld",
    "gRandoSettings.MixInteriors",
    "gRandoSettings.MixThievesHideout",
    "gRandoSettings.MixGrottos",
    "gRandoSettings.DecoupleEntrances",
    nullptr,
};
const char* const kCvMq[] = { "gRandoSettings.MQDungeons", "gRandoSettings.MQDungeonsSelection", nullptr };
const char* const kCvWallet[] = { "gRandoSettings.ShuffleChildWallet", "gRandoSettings.IncludeTycoonWallet",
                                  nullptr };

const FleetIncompat kIncompat[] = {
    { "Wallets (no wallet / Tycoon)",
      "The shared state syncs the wallet LEVEL, so a level has to mean the same thing in both games. "
      "MM cannot represent 'no wallet' (its level 0 already holds 99 rupees) and has no Tycoon tier. "
      "If OoT runs on a scale MM cannot reproduce, the frozen game clamps the value and republishes "
      "it, and your rupees drain on their own.",
      "Shuffle Child's Wallet OFF, Include Tycoon Wallet OFF (level 0/1/2 = 99/200/500 in both)",
      kCvWallet, 0 },
    { "Entrance Shuffle",
      "The combo already connects the two worlds through its own loading zones. Shuffling entrances "
      "on top of that is not modelled by the combo logic yet, and the combo's pre-placement may use "
      "any location.",
      "All entrance shuffle back to vanilla (including mixed and decoupled)", kCvEntrances, 0 },
    { "Master Quest",
      "The combo logic works on OoT's vanilla dungeons; the MM oracle does not know the MQ layouts.",
      "MQ Dungeons set to 0", kCvMq, 0 },
    // No CVar: these are forced straight onto the Context at generation (RSK_SHUFFLE_SONGS /
    // RSK_WINCON) rather than through a menu CVar, so there is no live state to read here. The rows
    // exist only to tell the player what will change.
    { "Songs (OoT)",
      "The combo's pre-placement may use any location, and the restricted song stages do not support "
      "having their spots taken.",
      "Song Shuffle set to 'Anywhere'", nullptr, 0 },
    { "Native Triforce Hunt",
      "The combo's Triforce goal keeps its own cross-game counter (comboTriforce). Each game's native "
      "Triforce Hunt would collect pieces in parallel.",
      "Native win condition set to 'Defeat Ganon'; pieces are handled by the combo goal", nullptr, 0 },
};

} // namespace

const FleetIncompat* FleetCombo_GetIncompatTable(int* count) {
    if (count) {
        *count = (int)(sizeof(kIncompat) / sizeof(kIncompat[0]));
    }
    return kIncompat;
}

int FleetCombo_CountActiveConflicts() {
    int n = 0;
    for (const auto& e : kIncompat) {
        if (!e.cvars) {
            continue; // no CVar to read: forced onto the Context at generation, not via the menu
        }
        for (const char* const* cv = e.cvars; *cv; ++cv) {
            if (CVarGetInteger(*cv, 0) != e.safeValue) {
                n++;
                break; // a row counts once, even when several of its CVars are in conflict
            }
        }
    }
    return n;
}

void FleetCombo_ResolveAllConflicts() {
    for (const auto& e : kIncompat) {
        if (!e.cvars) {
            continue;
        }
        for (const char* const* cv = e.cvars; *cv; ++cv) {
            CVarSetInteger(*cv, e.safeValue);
        }
    }
    CVarSave();
}

bool FleetCombo_PrePlacementHook() {
    if (!sComboActive) {
        return true;
    }
    try {
        return RunComboPrePlacement();
    } catch (const std::exception& e) {
        SetStatus(std::string("Pre-placement failed: ") + e.what());
        return false;
    }
}

bool FleetCombo_StartGeneration(const std::string& seedString) {
    if (sRunning.exchange(true)) {
        return false;
    }
    if (FleetShipCombo_GetActiveGame() < 0) {
        sRunning = false;
        SetStatus("No active combo: MM oracle unavailable");
        return false;
    }
    if (sThread.joinable()) {
        sThread.join();
    }
    sThread = std::thread(GenerateCombo, seedString);
    return true;
}

bool FleetCombo_LoadFleet(const std::string& fileName, int slot, const std::string& name) {
    if (sRunning.exchange(true)) {
        return false;
    }
    if (FleetShipCombo_GetActiveGame() < 0) {
        sRunning = false;
        SetStatus("No active combo: MM oracle unavailable");
        return false;
    }
    if (sThread.joinable()) {
        sThread.join();
    }
    sThread = std::thread(LoadFleetThread, fileName, slot, name);
    return true;
}

std::vector<std::string> FleetCombo_ListFleetFiles() {
    return ListFleetFiles();
}

bool FleetCombo_IsRunning() {
    return sRunning;
}

std::string FleetCombo_GetStatus() {
    std::lock_guard<std::mutex> lock(sStatusMx);
    return sStatus;
}

// ---- File-select "COMBO" (QUEST_OOTXMM) C bridges (called from z_file_choose.c / z_sram.c) ----
#include "FleetShipCombo.h"
#include "FleetOracleClient.h"

// Cache of the .fleet files for the file-select "Load Combo Seed" picker (refreshed on menu open,
// so the DL draw doesn't hit the disk every frame).
static std::vector<std::string> sFsFleetCache;

extern "C" {

void FleetComboFS_RefreshFleets(void) {
    sFsFleetCache = FleetCombo_ListFleetFiles();
}

int FleetComboFS_FleetCount(void) {
    return (int)sFsFleetCache.size();
}

const char* FleetComboFS_FleetName(int idx) {
    if (idx < 0 || idx >= (int)sFsFleetCache.size()) {
        return "";
    }
    return sFsFleetCache[idx].c_str();
}

void FleetComboFS_LoadSeedIndex(int idx) {
    if (idx < 0 || idx >= (int)sFsFleetCache.size()) {
        return;
    }
    // slot < 0 = SEED-ONLY: apply the OoT spoiler to the Rando context + send MM prepareSeed, but do
    // NOT bake a save slot. This makes the seed "ready" (IsSpoilerLoaded) so "Start Combo" then
    // creates the OoT+MM save pair at the file-select slot using THIS loaded seed.
    FleetCombo_LoadFleet(sFsFleetCache[idx], -1, "LINK");
}

void FleetComboFS_Generate(void) {
    FleetCombo_StartGeneration(""); // async, own thread; marks the Rando context seed-generated on finish
}

void FleetComboFS_OpenSettings(void) {
    // Open the shared combo randomizer settings = the (trimmed) Fleet Shared "Randomizer" header.
    CVarSetString("gSettings.Menu.ActiveHeader", "Randomizer##FleetShared");
    FleetShipCombo_OpenSharedWindow(); // sets gFleetCombo.MenuMode=1 + shows the SohMenu
}

int FleetComboFS_IsBusy(void) {
    return FleetCombo_IsRunning() ? 1 : 0;
}

void FleetComboFS_OnCreateSave(int slot) {
    if (slot < 0 || slot > 2) {
        return;
    }
    // MM: delete + recreate its paired slot on disk WITH the prepared seed (its live state is
    // untouched). prepareSeed already ran at the end of generation, so OnFileCreate applies it.
    FleetOracle_SendCreateSaveRequest(slot, "LINK"); // fire-and-forget; MM name is cosmetic (its file select is bypassed)

    // Publish the combo slot cross-process so MM auto-loads the SAME slot when it becomes active.
    FleetShipCombo_SetComboSlot(slot);

    // Bake this file's "start in MM vs OoT" choice from the shared toggle, per slot, so a later
    // launch/boot resumes into the right game.
    int startInMm = CVarGetInteger("gFleetCombo.StartInMM", 0) ? 1 : 0;
    CVarSetInteger(("gFleetCombo.StartInMM.Slot" + std::to_string(slot)).c_str(), startInMm);
    CVarSetInteger("gFleetCombo.LastSlot", slot);
    CVarSave();
    SPDLOG_INFO("[FleetComboFS] combo save created for slot {} (startInMM={})", slot, startInMm);
}

void FleetComboFS_OnLoadSave(int slot) {
    if (slot < 0 || slot > 2) {
        return;
    }
    FleetShipCombo_SetComboSlot(slot); // MM auto-loads this slot when it becomes active
    CVarSetInteger("gFleetCombo.LastSlot", slot);
    int startInMm = CVarGetInteger(("gFleetCombo.StartInMM.Slot" + std::to_string(slot)).c_str(), 0) ? 1 : 0;
    // Persist which game this combo file boots into. NOTE (v1): this sets the PERSISTENT boot game
    // (next launch resumes there); the seamless in-session hand-off to MM right after loading from
    // OoT's file select (MM auto-loading its own paired slot at its own entrance) is a follow-up —
    // it needs the MM title/file-select bypass. For now loading a combo file keeps you in OoT this
    // session and remembers the start-in choice.
    CVarSetInteger("isPlayerIn2Ship", startInMm);
    CVarSave();
    SPDLOG_INFO("[FleetComboFS] combo save loaded from slot {} (startInMM={} persisted)", slot, startInMm);
}

} // extern "C"
