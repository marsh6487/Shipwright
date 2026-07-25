#ifndef FLEET_SHIP_COMBO_HOST_H
#define FLEET_SHIP_COMBO_HOST_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Host-side Fleet Ship Combo bootstrap (Frente A). Ship (Ocarina of Time) is ALWAYS
// the host. On boot, if the combo is enabled and the player was last in 2ship
// (isPlayerIn2Ship CVar) OR Ship was launched with --boot=mm (handoff from 2ship),
// this launches the 2ship child process (2ship.exe --fleet-child).
//
// Both processes then stay alive; switching games pauses the inactive session (Frente B
// wires the seamless pause + shared-texture compositing). The 2ship child is told
// --fleet-child so it does NOT bounce back to Ship (loop guard).
//
// CVars: isFleetShipCombo.Enabled (master), isPlayerIn2Ship (1 = MM/2ship, 0 = OoT/Ship).
//
// Call once early in boot (after InitOTR), passing the process argc/argv.
void FleetShipCombo_HostBootstrap(int argc, char** argv);

// Mirror mm.o2r + oot.o2r so both sit next to BOTH exes (combo layout: root + /2ship). Call around the
// extractor: an o2r extracted by either game is copied into the sibling dir, and a missing one that's
// present in the sibling is pulled in. No-op with no sibling (standalone).
void FleetShipCombo_ProvisionO2rBothDirs(void);

// ---- Shared-memory coordination (Frente B) ----
// A named shared-memory region carries the active-game flag (and later the D3D11
// shared texture handle + per-frame sync) between Ship and 2ship.
//   activeGame: 0 = Ocarina of Time (Ship), 1 = Majora's Mask (2ship)
//
// Opens (or creates) the shared region. Safe to call more than once. Only meaningful
// in combo mode; standalone Ship never creates it, so IsThisGameActive() stays true.
// instanceKey (Ship's own PID) makes the region name UNIQUE per combo, so several combos
// can run on one machine without colliding. Pass 0 to use the legacy unsuffixed name.
void FleetShipCombo_SharedInit(unsigned long instanceKey);

// Active game stored in shared memory, or -1 if the region is unavailable.
int FleetShipCombo_GetActiveGame(void);

// Write the active game to shared memory (used by the Switch button).
void FleetShipCombo_SetActiveGame(int game);

// ---- Cross-game loading-zone WARP (the world connector) ----
// Trigger side: record the target (in the TARGET game's scene-id + world coords), bump the warp
// seq, and flip activeGame so the target game becomes active and applies it. targetGame: 0 = OoT
// (Ship), 1 = MM (2ship). rotY is the s16 binary-angle Link should face on arrival. saveFile is the
// save SLOT the trigger is in (e.g. gSaveContext.fileNum) so the target game lands in its own same slot.
void FleetShipCombo_RequestWarp(int targetGame, int scene, float x, float y, float z, int rotY, int saveFile);

// Applied by whichever game just became active: returns 1 ONCE per new request when a warp is
// addressed to THIS game, filling the target scene + land position/rotation. The receiver then
// loads `scene` and overrides Link's pos/rot to (x,y,z,rotY). Any out-param may be null.
int FleetShipCombo_ConsumePendingWarp(int* scene, float* x, float* y, float* z, int* rotY);

// The save SLOT the pending warp came from (set by RequestWarp). The receiver loads its own save at
// this slot so OoT file_1 <-> MM file_1. Returns -1 if unset/unavailable.
int FleetShipCombo_GetWarpSaveFile(void);

// Cross-game arrival blackout: paint THIS game's screen black for `frames` frames so the stale frame
// of the other game + the warp scene-load are hidden during a flip. The render path queries
// ...Active() once per frame (it decrements and returns 1 while still blacking out).
void FleetShipCombo_BeginArrivalBlackout(int frames);
int FleetShipCombo_ArrivalBlackoutActive(void);

// Sending-side fade overlay alpha (0..255): the active game ramps it while Link walks into the door
// (no scene transition); the host PiP consumer draws black at this alpha over the scene for a real
// fade-out, then flips at full black. Same-process (host) read/write.
void FleetShipCombo_SetSendFadeAlpha(int alpha);
int FleetShipCombo_GetSendFadeAlpha(void);

// DEV: index of the Lost Woods room display list the MM door-tunnel tool is currently showing, shared
// so Ship's on-screen overlay can display it while you cycle to find the tunnel piece.
void FleetShipCombo_SetDoorDLIndex(int index);
int FleetShipCombo_GetDoorDLIndex(void);

// True if THIS process (Ocarina of Time) is the active game, OR if shared memory is
// unavailable (standalone). Drives the FrameAdvance freeze of the inactive game.
bool FleetShipCombo_IsThisGameActive(void);

// ---- Picture-in-picture: shared D3D11 game texture (Frente B B2-B4) ----
// Read the shared-texture descriptor published by 2ship (the producer). Returns 1 if
// a handle is present, 0 otherwise. Any out-param may be null. Used by the Ship-side
// consumer to OpenSharedResource + draw the 2ship image in an ImGui panel.
int FleetShipCombo_GetSharedTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                    unsigned int* dxgiFormat, unsigned int* frameIndex);

// Consumer (Ship): register the ImGui window that shows 2ship's shared game texture
// (picture-in-picture). Call once after the GUI is set up (e.g. SohGui SetupGuiElements).
// No-op unless the combo is enabled.
void FleetShipCombo_RegisterConsumerWindow(void);

// UI focus: which game's window is in front for CONFIG (0 = Ship, 1 = 2ship). Independent
// of the active game. Ship's NEI "View" selector sets it; 2ship reads it to show/hide its
// own window so the user can reach its BenGui.
int FleetShipCombo_GetUiFocus(void);
void FleetShipCombo_SetUiFocus(int focus);

// ---- FleetSync save-sync handshake (reservedU[1..3]) ----
// The game that just SAVED signals; the other (frozen) exe applies the shared overlay from the
// temp file, saves its own slot, and acks with the seq it processed. See FleetSync.cpp.
void FleetShipCombo_SignalSyncSave(int slot);
unsigned long long FleetShipCombo_GetSyncSaveSeq(void);
int FleetShipCombo_GetSyncSaveSlot(void);
void FleetShipCombo_AckSyncSave(unsigned long long seq);
unsigned long long FleetShipCombo_GetSyncSaveAck(void);

// ---- Fleet Oracle (combo randomizer generation) handshake (reservedU[4..5]) ----
// The HOST (Ship) writes fleet_oracle_req.json in its own dir and bumps the request seq; the MM
// oracle (2ship FleetOracle.cpp) answers into fleet_oracle_resp.json and acks the seq.
// Host-side client: FleetOracleClient.h.
void FleetShipCombo_SignalOracleRequest(void);
unsigned long long FleetShipCombo_GetOracleRequestSeq(void);
void FleetShipCombo_AckOracleResponse(unsigned long long seq);
unsigned long long FleetShipCombo_GetOracleResponseAck(void);

// ---- Shared-window open request (reservedU[6]) ----
// El tab "Shared" de 2ship bumpea el contador; nuestro pump (FleetOracleClient) abre la ventana.
void FleetShipCombo_RequestSharedWindowOpen(void);
unsigned long long FleetShipCombo_GetSharedWindowOpenSeq(void);

// ---- Cross-game RESTART (reservedU[10]) ----
// A reset in one game signals; the other game's per-frame pump consumes it and resets itself too,
// so "restart one, restart both". SignalRestart marks the bump as ours (we never respond to our own
// reset); ConsumeRestartRequest returns 1 ONCE when the OTHER game reset.
void FleetShipCombo_SignalRestart(void);
int FleetShipCombo_ConsumeRestartRequest(void);

// ---- UI-overlay texture (reservedU[7..9]) ----
// SECOND shared texture published by 2ship with ONLY its ImGui windows (trackers etc.) on a
// transparent background — excludes its game image and BenGui menu. The consumer draws it on
// top of whichever game is active so both games' trackers can be visible at once. Display is
// gated by gFleetCombo.ShowMmUiOverlay (default on); 2ship's publishing by gFleetShipCombo.UiOverlay.
void FleetShipCombo_PublishUiTexture(unsigned long long handle, unsigned int width, unsigned int height,
                                     unsigned int dxgiFormat, unsigned int frameIndex);
int FleetShipCombo_GetUiTexture(unsigned long long* handle, unsigned int* width, unsigned int* height,
                                unsigned int* dxgiFormat, unsigned int* frameIndex);

// ---- Creación del par de saves combo ----
// Encola la creación del save RANDOMIZER de OoT en el slot (overwrite). Se ejecuta cuando el
// juego está en title/file select (Sram_InitSave necesita FileChooseContext). name = 8 bytes ya
// codificados al charset del file select. El lado MM va por la op createSave del oráculo.
void FleetCombo_RequestCreateSave(int slot, const unsigned char name[8]);

// ---- Ventana "Fleet Shared" (FleetSharedWindow.cpp) ----
// La abre/cierra el tab "Shared" del tab-row del combo (Menu.cpp). Contiene los smoke tests del
// oráculo, el editor de opciones del rando de MM (op setOptions) y las variables compartidas.
void FleetShipCombo_RegisterSharedWindow(void);
void FleetShipCombo_ToggleSharedWindow(void);
void FleetShipCombo_OpenSharedWindow(void);
int FleetShipCombo_IsSharedWindowVisible(void);

// ---- File-select "COMBO" (QUEST_OOTXMM) bridges (called from C: z_file_choose.c / z_sram.c) ----
// C wrappers around the C++ combo generator / oracle so the OoT file-select COMBO mode can:
//   - Generate: kick the combo seed generation (async, own thread).
//   - OpenSettings: open the shared combo randomizer settings (the trimmed Fleet Shared "Randomizer").
//   - IsBusy: true while a combo generation is running (grays the menu / shows "generating").
//   Seed-ready gate reuses the existing Randomizer_IsSeedGenerated() — combo generation marks the
//   SAME Rando context as seed-generated, so no separate accessor is needed.
void FleetComboFS_Generate(void);
void FleetComboFS_OpenSettings(void);
int FleetComboFS_IsBusy(void);
// File-select "Load Combo Seed" picker: refresh the .fleet list (call on menu open), read its size
// / entries for the DL cursor, and load the chosen entry SEED-ONLY (Start Combo then bakes it into
// the file-select slot). LoadSeedIndex uses the last refreshed list.
void FleetComboFS_RefreshFleets(void);
int FleetComboFS_FleetCount(void);
const char* FleetComboFS_FleetName(int idx);
void FleetComboFS_LoadSeedIndex(int idx);
// Combo active save slot published to shared memory (0..2), so MM auto-loads the same slot when it
// becomes active. -1 when unset. (reservedU[11].)
void FleetShipCombo_SetComboSlot(int slot);
int FleetShipCombo_GetComboSlot(void);
// Called from Sram_InitSave right after the OoT combo slot is born (quest.id == QUEST_OOTXMM):
// tells MM to delete+recreate its paired slot WITH the prepared seed, and bakes this file's
// "start in MM vs OoT" choice (from gFleetCombo.StartInMM) so loading it later boots the right game.
void FleetComboFS_OnCreateSave(int slot);
// Called from FileChoose_LoadGame when a QUEST_OOTXMM file is loaded: sets the active game (and
// isPlayerIn2Ship) from this slot's baked start-in flag, handing off to MM when requested.
void FleetComboFS_OnLoadSave(int slot);

#ifdef __cplusplus
}
#endif

#endif // FLEET_SHIP_COMBO_HOST_H
