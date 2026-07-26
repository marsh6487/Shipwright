// FleetWarpBoot.cpp — UNIFIED cross-game warp arrival pipeline (OoT side) + combo-pair save
// validation + title-screen temp cleanup + Temple of Time fleet-hole spawner.
//
// EVERY warp addressed to OoT lands here (cold boot AND in-gameplay). The pipeline always:
//   1. force-opens the paired save slot from disk (creating it if missing),
//   2. applies the FleetSync temp overlays (own full anchor + shared player state),
//   3. sets EXPLICIT destination overrides (entrance/cutscene/respawn) LAST,
//   4. boots a fresh Play_Init.
// One code path, overrides always after any load = no stale cutsceneIndex (the old
// Scene_CommandAlternateHeaderList crash) and no load-machine stomping our respawn data.
//
// Runs every frame via GameInteractor::OnGameFrameUpdate (fires in ALL gamestates, and keeps
// firing while the game is combo-frozen).

#include "FleetShipCombo.h"
#include "FleetSync.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"

#include <cstring>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h> // CVar: gFleetCombo.LastSaved* for boot-resume

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
extern SaveContext gSaveContext;
void Sram_OpenSave(void);
void Sram_InitSave(FileChooseContext* fileChooseCtx);
void Play_Init(GameState* state);
void FileChoose_Init(GameState* state);
void FileChoose_LoadGame(GameState* thisx); // SM_LOAD_GAME handler: load a slot at its OWN entrance
// Defined extern "C" in SaveManager.cpp, but SaveManager.h only declares it for C callers.
SaveFileMetaInfo* Save_GetSaveMetaInfo(int fileNum);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId); // z_scene.c (not in a header)
// custom_items_common.c (unity-built into z_player.c): re-arm the Lost Woods trigger after an
// arrival so we don't instantly ping-pong back.
void FleetWarp_NotifyArrived(void);
void GameInteractor_ExecuteOnLoadGame(int32_t fileNum); // mods hook their per-file init on this
}

namespace {

// Locally cached pending warp (ConsumePendingWarp is one-shot, but the cold-boot path may need
// several frames to walk the gamestates: title -> file select -> load/boot).
bool sPending = false;
int sScene = 0;

// The Temple of Time EXTERIOR fleet hole (Door_Ana). Must match the MM South Clock Town pairing.
// Position captured by the user in-game (dev Player tab, room index 1): y=-40 IS solid floor here
// (an earlier assumption that it was void was wrong). Link pops OUT of this exact spot on arrival
// (grotto-return respawn). The hole ALWAYS spawns on scene load (user directive); re-entry right
// after an arrival is suppressed by the FleetSync GRAB COOLDOWN (visible but inert — z_door_ana.c).
constexpr float kTotHoleX = 464.523f;
constexpr float kTotHoleY = -40.0f;
constexpr float kTotHoleZ = 1651.472f;
constexpr s16 kTotHoleYaw = 15278;  // Link's facing when he rises out of the hole (user capture)
constexpr u8 kTotHoleRoom = 1;      // ToT INTERIOR room index the spot lives in (Master Sword chamber)

// The hole lives in the Temple of Time INTERIOR (SCENE_TEMPLE_OF_TIME) — the user's captured spot is
// the Master Sword pedestal chamber (room 1), NOT the exterior courtyard.
bool IsTotHoleScene(s32 sceneNum) {
    return sceneNum == SCENE_TEMPLE_OF_TIME;
}

// Pick the destination entrance for a scene id + set the grotto-return respawn if applicable.
// For the ToT hole it sets respawnFlag=2 + RESPAWN_MODE_RETURN so Link pops OUT of the counterpart
// hole (user's locked decision). The caller must NOT clear respawnFlag after this returns.
s16 FleetSelectDestination(int scene) {
    switch (scene) {
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:   // fleet hole home (MM may send any ToT id; they
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT: // all resolve to the INTERIOR pedestal chamber)
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS:
        case SCENE_TEMPLE_OF_TIME: {
            // GROTTO POP-OUT: Link rises out of the ground at the hole spot the user captured inside
            // the Temple of Time INTERIOR (464.523, -40, 1651.472, room 1 = Master Sword chamber).
            // Scene AND position are BOTH the interior — matching them keeps Link on solid floor.
            // respawnFlag=2 -> Play_Init reads respawn[RETURN] (room 1, pos, yaw, grotto start mode).
            s16 entrance = ENTR_TEMPLE_OF_TIME_ENTRANCE; // SCENE_TEMPLE_OF_TIME interior, spawn 0
            gSaveContext.respawnFlag = 2;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].entranceIndex = entrance;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].roomIndex = kTotHoleRoom;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.x = kTotHoleX;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.y = kTotHoleY;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.z = kTotHoleZ;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].yaw = kTotHoleYaw;
            // 0x04FF = params 0xFF + start mode 4 (PLAYER_START_MODE_GROTTO) — the exact value
            // vanilla Door_Ana passes to Play_SetupRespawnPoint. OoT has no PLAYER_PARAMS macro.
            gSaveContext.respawn[RESPAWN_MODE_RETURN].playerParams = 0x04FF;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].data = 0;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].tempSwchFlags = 0;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].tempCollectFlags = 0;
            return entrance;
        }
        case SCENE_MARKET_DAY:
            return ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP;
        case SCENE_LOST_WOODS:
        default:
            return ENTR_LOST_WOODS_SOUTH_EXIT;
    }
}

// SEAMLESS in-game arrival (spiritual_stones.cpp ExecuteWarp style): OoT is ALREADY in gameplay
// with its save loaded — just apply the shared overlay and start a NORMAL scene transition to the
// destination. NO Play_Init reboot (that lost HUD/button/magic state = the "broken status") and NO
// disk reload (the save-sync signal keeps the paired file consistent separately). The flip's black
// covers the INSTANT-out; the FADE_BLACK-in reveals the destination.
void ExecuteWarpInGame(int slot) {
    FleetSync_ApplyArrival(slot); // shared player-state overlay only

    gSaveContext.respawnFlag = 0;
    s16 entrance = FleetSelectDestination(sScene);

    gSaveContext.entranceIndex = entrance;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = entrance; // void-out anchor (never -1)
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = 0;

    gPlayState->nextEntranceIndex = entrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;        // no fade-out (the flip is already black)
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK; // fade-in reveal at the destination

    FleetWarp_NotifyArrived();
    sPending = false;
    // NO SET_NEXT_GAMESTATE — the transition FSM loads the scene; respawnFlag survives to Play_Init.
}

// Destination overrides + fresh boot. Runs AFTER the slot load + FleetSync overlays so nothing
// can stomp these values before Play_Init consumes them.
void ApplyDestinationAndBoot(GameState* state, int slot) {
    FleetSync_ApplyArrival(slot);

    // "START SAVE FILE" defaults — the exact block FileChoose_LoadGame applies after Sram_OpenSave
    // (z_file_choose.c). Skipping it booted a half-initialized file: disabled buttons, broken magic
    // meter/HUD alphas, stale timers ("broken status"). Destination overrides come AFTER.
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.eventInf[0] = 0;
    gSaveContext.eventInf[1] = 0;
    gSaveContext.eventInf[2] = 0;
    gSaveContext.eventInf[3] = 0;
    gSaveContext.prevHudVisibilityMode = 0x32; // HUD visibility alpha
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    gSaveContext.skyboxTime = 0;
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = 0xFFFF;
    gSaveContext.retainWeatherMode = 0;
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = gSaveContext.nextHudVisibilityMode = gSaveContext.hudVisibilityMode = gSaveContext.hudVisibilityModeTimer =
        gSaveContext.magicCapacity = 0;
    gSaveContext.magicFillTarget = gSaveContext.magic; // boot-time magic refill animation
    gSaveContext.magic = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.naviTimer = 0;

    gSaveContext.respawnFlag = 0;
    s16 entrance = FleetSelectDestination(sScene); // natural entrance (no void); grotto only where safe
    gSaveContext.entranceIndex = entrance;
    gSaveContext.cutsceneIndex = 0; // NEVER inherit a stale scene-setup selector (crash C1)
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    // Void-out anchor = the DESTINATION entrance (NEVER ENTR_LOAD_OPENING = -1: a void-out with
    // that sentinel garbage-loads entrance 0 = Inside the Deku Tree, in an inescapable loop).
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = entrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.nextDayTime = 0xFFFF;

    gSaveContext.gameMode = GAMEMODE_NORMAL; // NEVER inherit the title-demo mode (no pause +
                                             // loading zones cycling demo scenes like Dodongo's)
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum); // mods' per-file init (rando etc.)
    FleetWarp_NotifyArrived(); // arms the proximity trigger so we don't instantly flip back
    sPending = false;
    state->running = false;
    SET_NEXT_GAMESTATE(state, Play_Init, PlayState);
}

// NOTE: OoT's boot auto-resume was REMOVED (2026-07-23, user request). OoT's title/file select is
// the combo ENTRY POINT now — you pick your file (or the "COMBO" quest) there, so OoT must NOT
// auto-load its last-saved slot on boot. Cross-game WARP arrivals still auto-load (the sPending
// path below); only the no-warp boot-resume is gone, leaving OoT at its file select.

void FleetWarpBoot_Tick() {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return; // combo not running
    }
    if (gGameState == NULL) {
        return;
    }

    // One-shot per process: reaching the title/file-select after launch wipes the temp file.
    // (No gPlayState check: OoT's title screen runs INSIDE a PlayState — the title demo.)
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN || gSaveContext.gameMode == GAMEMODE_FILE_SELECT) {
        FleetSync_OnTitleScreen();
    }

    if (!sPending) {
        int scene = 0, rotY = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (FleetShipCombo_ConsumePendingWarp(&scene, &x, &y, &z, &rotY)) {
            sPending = true;
            sScene = scene;
        }
    }
    if (!sPending) {
        return; // no cross-game warp -> leave OoT at its title/file select (the combo entry point)
    }

    int slot = FleetShipCombo_GetWarpSaveFile();
    if (slot < 0 || slot > 2) {
        slot = 0;
    }

    // REAL GAMEPLAY (already in a scene, save loaded): seamless in-game transition — NO reboot,
    // NO disk reload. This is the common case (flipping between two running games).
    if (gPlayState != NULL && gSaveContext.gameMode == GAMEMODE_NORMAL) {
        gSaveContext.fileNum = slot;
        ExecuteWarpInGame(slot);
        return;
    }

    // TITLE DEMO with the paired save already on disk: reboot straight into the destination
    // (full FileChoose_LoadGame "start save file" defaults) — the title demo can't do an in-game
    // transition. A MISSING pair still routes through the file select below (Sram_InitSave needs a
    // real FileChooseContext).
    if (Save_GetSaveMetaInfo(slot)->valid && gGameState != NULL) {
        gSaveContext.fileNum = slot;
        Sram_OpenSave(); // reload the slot from disk; FleetSync anchor/shared overlay next
        ApplyDestinationAndBoot(gGameState, slot);
        return;
    }

    // COLD BOOT: title -> file select -> validate/create pair -> load -> unified pipeline.
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) {
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
        return;
    }
    if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT) {
        return; // logo/other boot states -> wait; we'll land at title/file select
    }
    if (gPlayState != NULL) {
        return; // the old PlayState (title demo) is still tearing down: gGameState isn't the
                // FileChooseContext yet — casting now would scribble on a dying gamestate
    }

    FileChooseContext* fc = (FileChooseContext*)gGameState;
    gSaveContext.fileNum = slot;

    // COMBO-PAIR VALIDATION: if OoT's relative of the MM file doesn't exist, CREATE it now
    // (fresh vanilla save persisted to disk) so OoT file_N <-> MM file_N always exists.
    if (!Save_GetSaveMetaInfo(slot)->valid) {
        static const u8 kDefaultName[8] = { 21, 44, 43, 46, 62, 62, 62, 62 }; // "LINK"
        memcpy(Save_GetSaveMetaInfo(slot)->playerName, kDefaultName, 8);
        fc->buttonIndex = (s16)slot;
        fc->n64ddFlag = 0;
        Sram_InitSave(fc);
    }

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Sram_OpenSave();
    ApplyDestinationAndBoot(gGameState, slot);
}

// ---------------------------------------------------------------------------------------------
// Temple of Time fleet hole: spawn a real Door_Ana (open grotto) at the warp spot. Needs
// OBJECT_GAMEPLAY_FIELD_KEEP (gGrottoDL) which is NOT resident indoors -> Object_Spawn it,
// then retry Actor_Spawn each frame until it sticks. The spawned actor is registered with
// FleetSync so z_door_ana.c's fall commit redirects to the cross-game flip.
// ---------------------------------------------------------------------------------------------
void FleetHoleSpawnTick() {
    static Actor* sHole = nullptr;
    static s16 sSceneWithHole = -1;
    static u32 sLastFrameCount = 0;

    if (FleetShipCombo_GetActiveGame() < 0 || gPlayState == NULL) {
        sHole = nullptr;
        sSceneWithHole = -1;
        return;
    }
    // Scene-load detection (mailbox_actor.c pattern): a fresh PlayState re-inits state.frames to 0,
    // so a frame-counter REWIND means a reload even when sceneNum is unchanged AND even when the
    // new PlayState reuses the same arena address (a pointer compare misses that).
    s32 sceneLoaded = (gPlayState->sceneNum != sSceneWithHole) || (gPlayState->state.frames < sLastFrameCount);
    sLastFrameCount = gPlayState->state.frames;
    if (sceneLoaded) {
        sHole = nullptr;
        sSceneWithHole = -1;
    }
    if (!IsTotHoleScene(gPlayState->sceneNum)) {
        return;
    }
    if (sHole != nullptr) {
        return; // already placed this scene
    }
    if (Object_GetIndex(&gPlayState->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP) < 0) {
        Object_Spawn(&gPlayState->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP);
        return; // object not resident this frame yet -> retry next frame (Actor_Spawn would fail)
    }
    // FLEET_HOLE_PARAM marks this Door_Ana as ours (z_door_ana.c makes it a pure visual — never
    // grabs/warps). The FleetWarp proximity trigger runs the cross-game flip.
    sHole = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_DOOR_ANA, kTotHoleX, kTotHoleY, kTotHoleZ, 0, 0, 0,
                        FLEET_HOLE_PARAM);
    if (sHole != nullptr) {
        sHole->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        sHole->room = -1; // global actor: survives room transitions (hall <-> pedestal chamber)
        sSceneWithHole = gPlayState->sceneNum;
        SPDLOG_INFO("[FleetSync] ToT fleet hole spawned at ({},{},{})", kTotHoleX, kTotHoleY, kTotHoleZ);
    } else {
        SPDLOG_WARN("[FleetSync] ToT fleet hole Actor_Spawn FAILED (retrying)");
    }
}

// ---------------------------------------------------------------------------------------------
// Creación programática del save combo de OoT (botón "Crear saves" del Fleet Shared).
// Sram_InitSave necesita un FileChooseContext REAL, así que la petición queda pendiente hasta
// estar en file select (desde el title se fuerza el paso a file select, como el warp boot).
// Con questType[slot] = QUEST_RANDOMIZER, Sram_InitSave llama Randomizer_InitSaveFile y el save
// nace con la seed combo ya generada. Overwrite incondicional del slot.
// ---------------------------------------------------------------------------------------------
int sPendingCreateSlot = -1;
u8 sPendingCreateName[8];

void FleetCreateSaveTick() {
    if (sPendingCreateSlot < 0 || FleetShipCombo_GetActiveGame() < 0 || gGameState == NULL) {
        return;
    }
    if (gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) {
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, FileChoose_Init, FileChooseContext);
        return;
    }
    if (gSaveContext.gameMode != GAMEMODE_FILE_SELECT || gPlayState != NULL) {
        return; // en gameplay no hay FileChooseContext: espera a que el usuario salga al title
    }

    int slot = sPendingCreateSlot;
    sPendingCreateSlot = -1;

    FileChooseContext* fc = (FileChooseContext*)gGameState;
    gSaveContext.fileNum = (s16)slot;
    memcpy(Save_GetSaveMetaInfo(slot)->playerName, sPendingCreateName, 8);
    fc->buttonIndex = (s16)slot;
    fc->n64ddFlag = 0;
    fc->questType[slot] = QUEST_RANDOMIZER;
    Sram_InitSave(fc);
    SPDLOG_INFO("[FleetCombo] save OoT combo creado en File {} (quest RANDOMIZER, overwrite)", slot + 1);
}

void RegisterFleetWarpBoot() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(FleetWarpBoot_Tick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(FleetHoleSpawnTick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(FleetCreateSaveTick);
}

} // namespace

// Encola la creación del save combo de OoT (name ya codificado al charset del file select).
void FleetCombo_RequestCreateSave(int slot, const unsigned char name[8]) {
    if (slot < 0 || slot > 2) {
        return;
    }
    memcpy(sPendingCreateName, name, 8);
    sPendingCreateSlot = slot;
}

static RegisterShipInitFunc initFleetWarpBoot(RegisterFleetWarpBoot, {});
