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

#ifdef __cplusplus
}
#endif

#endif // FLEET_SHIP_COMBO_HOST_H
