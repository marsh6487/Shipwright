#pragma once
// FleetComboRando.h — Generador del Combo Randomizer (Fases 2+3, lado host / soh). C++ only.
//
// Arquitectura: NO reimplementa el fill de SoH. La generación combo llama al GenerateRandomizer
// nativo (3drando) con un hook de pre-colocación insertado en Fill() (fill.cpp): ahí el combo
// coloca (a) los items FC compartidos ambos-lados (pueden caer en checks de OoT O de MM) y
// (b) TODA la progresión nativa de MM (manifest del oráculo), usando assumed fill con
// reachability de OoT nativa (ReachabilitySearch) + reachability de MM por oráculo IPC.
// Después el fill nativo de SoH termina las etapas de OoT (shops, rewards, keys, junk) como
// siempre — los items FC que cayeron en MM se inyectan al StartingInventory lógico para que
// la lógica nativa los asuma obtenibles (sound: fueron colocados con assumed fill correcto).
//
// Fase 3 integrada: al terminar, escribe el spoiler 2S2H_RANDO_SPOILER de MM (checks RC_* →
// items RI_*, strings de enum) al puente fleet_oracle_spoiler.json y manda la op prepareSeed;
// 2ship lo instala y lo aplicará en su OnFileCreate al crear el save pareado.
//
// CVars combo: gFleetCombo.GoalMode (0 = Beat Both Bosses, 1 = Triforce Hunt),
//              gFleetCombo.TriforceTotal, gFleetCombo.TriforceRequired.
//
// V1 impone: entrance shuffle de OoT OFF y songs de OoT en Anywhere (la pre-colocación puede
// usar cualquier location; las etapas restringidas de canciones no soportan robo de spots).

#include <string>
#include <vector>

// Lanza la generación combo en su propio thread (como GenerateRandomizerImgui). seedString
// vacío = semilla aleatoria. Devuelve false si ya hay una generación corriendo o no hay combo.
bool FleetCombo_StartGeneration(const std::string& seedString);

// Carga una seed combo ya generada desde un .fleet (fleet/<fileName>) SIN regenerar: aplica el
// spoiler OoT al Context y manda el spoiler MM a 2ship. Devuelve false si ocupado/sin combo.
// Loads a .fleet seed AND bakes it into the paired slot in BOTH games (OoT File N + MM File N share
// the seed). slot 0..2 = File 1..3; name is the save name (ASCII, encoded internally for OoT).
bool FleetCombo_LoadFleet(const std::string& fileName, int slot, const std::string& name);

// Lista los archivos .fleet disponibles en la carpeta fleet/.
std::vector<std::string> FleetCombo_ListFleetFiles();

bool FleetCombo_IsRunning();

// Última línea de estado/progreso para la UI (thread-safe, copia).
std::string FleetCombo_GetStatus();

// Hook llamado desde Fill() (fill.cpp) en cada intento. true = seguir; false = reintentar.
// No-op (true) cuando no hay generación combo activa.
bool FleetCombo_PrePlacementHook();

// ---- Options INCOMPATIBLE with the combo ----
// Options one game can do and the other cannot, or that the combo does not model yet. Generation
// forces them to a safe value; this table is the SAME one that applies the forcing, so the Shared
// window's panel cannot lie about what generation will do.
struct FleetIncompat {
    const char* label;    // short name for the UI
    const char* why;      // why it is incompatible (shown on hover)
    const char* forcedTo; // what it ends up as after generating
    // OoT CVars that get checked/forced, nullptr-terminated. nullptr = the option is not forced
    // through a CVar (it goes straight onto the Context at generation, or is an oracle push to the
    // MM side): there is no live state to read, the row only informs.
    const char* const* cvars;
    int safeValue; // value every cvar in the row is forced to
};

// The whole table (including the rows with no CVar, marked cvars == nullptr).
const FleetIncompat* FleetCombo_GetIncompatTable(int* count);

// How many rows with OoT CVars are RIGHT NOW sitting on a conflicting value.
int FleetCombo_CountActiveConflicts();

// Sets every row's OoT CVars to their safe value (the same thing generation will do).
void FleetCombo_ResolveAllConflicts();
