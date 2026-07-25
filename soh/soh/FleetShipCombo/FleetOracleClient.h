#pragma once
// FleetOracleClient.h — Cliente del oráculo lógico de MM (lado host / soh). C++ only.
//
// El oráculo vive en 2ship (FleetOracle.cpp) y responde con la lógica REAL del rando de MM.
// Transporte: fleet_oracle_req.json / fleet_oracle_resp.json en el dir del Ship host +
// seq/ack por FscShared reservedU[4]/[5] (FleetShipCombo.h).
//
// Uso (async, pensado para el fill de la Fase 2):
//   auto seq = FleetOracle_SendManifestRequest();
//   ... por frame: nlohmann::json resp; if (FleetOracle_TryGetResponse(seq, resp)) { ... }
//
// Smoke test integrado (sin Fase 2): CVarSetInteger("gFleetOracle.Test", N) con el combo activo:
//   1 = manifest (loguea nº de checks/pool), 2 = reachable con inventario vacío (sphere 0),
//   3 = reachable con TODOS los items FC compartidos al máximo. El resultado sale por SPDLOG
//   con prefijo [FleetOracleClient] y el CVar vuelve solo a 0.

#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

// Escribe la request y bumpea el seq. Devuelve el seq emitido, o 0 si no hay combo/shm.
unsigned long long FleetOracle_SendManifestRequest();

// fcItems: pares [FcComboItemId, count] (count = nivel de cadena asumido).
// mmItems: pares [spoilerName de 2ship ("RI_*"), count] para items nativos de MM.
unsigned long long FleetOracle_SendReachableRequest(const std::vector<std::pair<int, int>>& fcItems,
                                                    const std::vector<std::pair<std::string, int>>& mmItems);

// cvars: pares [CVar de 2ship, valor int]. cvarsFloat: pares [CVar, valor float] (p.ej. volúmenes
// de MM que son float). El oráculo los aplica con CVarSetInteger/CVarSetFloat + CVarSave (op
// "setOptions"; whitelist gRando./gMods./gEnhancements./gCheats./gSettings.). Respuesta: {"applied": N}.
unsigned long long FleetOracle_SendSetOptionsRequest(
    const std::vector<std::pair<std::string, int>>& cvars,
    const std::vector<std::pair<std::string, float>>& cvarsFloat = {});

// op "prepareSeed": el host ya escribió el spoiler combo en <ShipDir>/fleet_oracle_spoiler.json;
// 2ship lo instala en su randomizer/<fileName>, activa gRando y sincroniza el índice.
// Respuesta: {"spoilerIndex": N, "file": nombre}.
unsigned long long FleetOracle_SendPrepareSeedRequest(const std::string& fileName);

// op "createSave": MM crea su save combo en el slot (overwrite en disco, estado vivo intacto)
// aplicando el spoiler preparado. name en ASCII (se codifica allá). Respuesta: {"randoApplied": b}.
unsigned long long FleetOracle_SendCreateSaveRequest(int slot, const std::string& name);

// True cuando la respuesta para `seq` está lista y parseada en `out` (chequea ack + lee el archivo).
// No bloquea; llamar por frame hasta que devuelva true.
bool FleetOracle_TryGetResponse(unsigned long long seq, nlohmann::json& out);

// Resumen humano del último smoke test completado (gFleetOracle.Test) — lo muestra el tab Shared.
std::string FleetOracle_GetLastTestSummary();
