/**
 * cane_testwarp.cpp — the `uh` console command: go stand next to the thing you want to test.
 *
 * SoH already warps by scene. `entrance <hex>` sets nextEntranceIndex, the Better Debug Warp screen
 * is a menu of every scene, and Warping.cpp saves named points that DO carry a room number and an
 * exact position. What none of them does is get you to a specific ROOM you have not been to yet,
 * which is what testing Ultrahand needs: the ferry, the coffin lids and the chain platform are all
 * several rooms deep and the actors are the whole point of going.
 *
 * So this does not ask for a room. It asks for an ACTOR, and hunts for it:
 *
 *   1. Warp to the dungeon's entrance, the way Warping.cpp does it.
 *   2. Once the scene is up, walk the rooms one at a time with Room_RequestNewRoom, checking each
 *      for the target actor id.
 *   3. When it turns up, put Link down beside it.
 *
 * The reason it hunts instead of carrying a table of room numbers and coordinates is that a table
 * would be numbers nobody has checked. Numbers that are almost right put you inside a wall, and
 * quietly - whereas a hunt that fails says so in the console. The actor knows where it is; asking
 * it is the only answer that cannot be stale.
 *
 * Room stepping is exactly the sequence En_Holl performs when you walk through a door
 * (z_en_holl.c:199-206): request while nothing is in flight, then finish the change once the load
 * lands. Link is pinned in place for the duration, because between two rooms he is standing on
 * nothing.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <ship/Context.h>
#include <ship/debug/Console.h>
#include <ship/window/Window.h>
#include <ship/window/gui/ConsoleWindow.h>
#include <libultraship/libultraship.h>

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#define CMD_REGISTER Ship::Context::GetRawInstance()->GetConsole()->AddCommand
#define UH_SAY                                                                           \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendInfoMessage
#define UH_ERR                                                                           \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(                                  \
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendErrorMessage

typedef struct {
    const char* name;
    s32 entrance;
    s16 actorId;
    const char* what;
} UhSpot;

// One line per Ultrahand behaviour worth seeing, named after the behaviour rather than the room.
static const UhSpot sSpots[] = {
    { "ship", ENTR_SHADOW_TEMPLE_ENTRANCE, ACTOR_BG_HAKA_SHIP, "ferry: PLANE_XZ, push and pull it" },
    { "coffin", ENTR_SHADOW_TEMPLE_ENTRANCE, ACTOR_BG_HAKA_HUTA, "coffin lid: LOCKED toggle" },
    { "statue", ENTR_SHADOW_TEMPLE_ENTRANCE, ACTOR_BG_HAKA_ZOU, "bird statue: PULLABLE" },
    { "grate", ENTR_SPIRIT_TEMPLE_ENTRANCE, ACTOR_BG_JYA_KANAAMI, "grate: must finish its fall on release" },
    { "chain", ENTR_SPIRIT_TEMPLE_ENTRANCE, ACTOR_BG_JYA_LIFT, "chain platform: must NOT tilt" },
    { "chu", ENTR_SPIRIT_TEMPLE_ENTRANCE, ACTOR_BG_JYA_BOMBCHUIWA, "bombchu rock: STRIKES, one blast" },
    { "jaw", ENTR_DODONGOS_CAVERN_ENTRANCE, ACTOR_BG_DODOAGO, "skull jaw: JAW, D-pad opens it" },
    { "stairs", ENTR_DODONGOS_CAVERN_ENTRANCE, ACTOR_BG_DDAN_KD, "staircase: AXIS_Y" },
    { "totem", ENTR_FIRE_TEMPLE_ENTRANCE, ACTOR_BG_HIDAN_DALM, "hammer totem: PULLABLE" },
    { "fslift", ENTR_FIRE_TEMPLE_ENTRANCE, ACTOR_BG_HIDAN_FSLIFT, "hookshot lift: DRIVE_HOME" },
    { "flame", ENTR_FIRE_TEMPLE_ENTRANCE, ACTOR_BG_HIDAN_CURTAIN, "flame circle: BURNS, carry it" },
    { "movebg", ENTR_WATER_TEMPLE_ENTRANCE, ACTOR_BG_MIZU_MOVEBG, "water platforms: four rows by type" },
    { "spout", ENTR_GERUDO_TRAINING_GROUND_ENTRANCE, ACTOR_EN_SIOFUKI, "water spout: HEIGHT, D-pad grows it" },
    { "tentacle", ENTR_JABU_JABU_ENTRANCE, ACTOR_EN_BA, "tentacle: STRIKES, four boomerang hits" },
    { "poe", ENTR_FOREST_TEMPLE_ENTRANCE, ACTOR_BG_PO_EVENT, "Poe block: PULLABLE" },
    { "ice", ENTR_ICE_CAVERN_ENTRANCE, ACTOR_BG_ICE_OBJECTS, "ice block: PLANE_XZ" },
    { "bridge", ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN, ACTOR_BG_SPOT00_HANEBASI, "drawbridge: HINGE (child only)" },
    { "pillar", ENTR_OUTSIDE_GANONS_CASTLE_0_2, ACTOR_BG_HEAVY_BLOCK, "gauntlets pillar: THROWS" },
};

typedef enum {
    UH_HUNT_OFF,
    UH_HUNT_WAIT_SCENE, // the warp is in flight
    UH_HUNT_LOOK,       // scene is up; search, then step to the next room
    UH_HUNT_LOADING,    // a room change is in flight
} UhHuntPhase;

static struct {
    UhHuntPhase phase;
    s16 actorId;
    s16 room;
    s16 patience;
    // Where Link is held while rooms come and go. Captured on the first frame of the search rather
    // than when the search is ordered, because at OnSceneInit time the new scene's Player does not
    // exist yet and the old scene's position is meaningless in it.
    Vec3f pin;
    u8 pinned;
    const char* label;
} sHunt = {};

static Actor* UhFindActor(PlayState* play, s16 actorId) {
    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* it = play->actorCtx.actorLists[cat].head; it != NULL; it = it->next) {
            if ((it->id == actorId) && (it->update != NULL)) {
                return it;
            }
        }
    }
    return NULL;
}

// Put Link down beside it, on the side facing away from the body so the camera has somewhere to be,
// and looking at it. Raised a little because a dynapoly's world.pos is usually its base.
static void UhStandBeside(PlayState* play, Player* player, Actor* target) {
    s16 yaw = target->shape.rot.y + 0x8000;

    player->actor.world.pos.x = target->world.pos.x + (Math_SinS(yaw) * 180.0f);
    player->actor.world.pos.y = target->world.pos.y + 40.0f;
    player->actor.world.pos.z = target->world.pos.z + (Math_CosS(yaw) * 180.0f);
    player->actor.prevPos = player->actor.world.pos;
    player->actor.shape.rot.y = player->actor.world.rot.y = yaw + 0x8000;
    player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.room = play->roomCtx.curRoom.num;
}

// Link is given back a real room on the way out. He spends the search as room -1 (see UhHuntTick),
// and leaving him there would mean he never gets cleaned up by a later room change.
static void UhHuntStop(PlayState* play) {
    if ((play != NULL) && sHunt.pinned) {
        GET_PLAYER(play)->actor.room = play->roomCtx.curRoom.num;
    }
    sHunt.phase = UH_HUNT_OFF;
    sHunt.actorId = 0;
    sHunt.pinned = 0;
    sHunt.label = NULL;
}

static void UhHuntTick() {
    PlayState* play = gPlayState;
    Player* player;
    Actor* found;

    if ((sHunt.phase == UH_HUNT_OFF) || (play == NULL)) {
        return;
    }
    player = GET_PLAYER(play);
    if (player == NULL) {
        return;
    }

    if (sHunt.phase == UH_HUNT_WAIT_SCENE) {
        return; // OnSceneInit starts the search; until then there is nothing to search
    }

    // Between rooms Link is standing on nothing, so he is held wherever the search began rather
    // than allowed to fall out of the world while it runs.
    if (!sHunt.pinned) {
        sHunt.pin = player->actor.world.pos;
        sHunt.pinned = 1;
    }
    // AND he is taken out of the room system entirely, because Room_FinishRoomChange calls
    // func_80031B14, which DELETES every actor whose room is >= 0 and is neither the new room nor
    // the old one (z_actor.c:3363). It walks every category, Player included. Vanilla never trips
    // over that - a room change only happens under En_Holl while Link is physically in the doorway,
    // so his room always matches - but the search moves him nowhere and changes rooms underneath
    // him, and the second hop deleted him. GET_PLAYER then returned NULL and the first actor to ask
    // where Link was took the crash: a Flying Pot, in Spirit Temple room 25.
    //
    // -1 is the engine's own "survives a room change", the same value the cane writes onto anything
    // it picks up.
    player->actor.room = -1;
    player->actor.world.pos = sHunt.pin;
    player->actor.prevPos = sHunt.pin;
    player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;

    // actorId 0 means `uh room`, which is a destination and not a search. Searching for it would
    // find ACTOR_PLAYER and politely stand Link next to himself.
    found = (sHunt.actorId != 0) ? UhFindActor(play, sHunt.actorId) : NULL;
    if (found != NULL) {
        UhStandBeside(play, player, found);
        UH_SAY("[uh] %s - room %d", sHunt.label, play->roomCtx.curRoom.num);
        UhHuntStop(play);
        return;
    }

    if (sHunt.phase == UH_HUNT_LOADING) {
        // En_Holl's rule: the change is only finishable once the load has landed and there is still
        // a previous room to retire.
        if ((play->roomCtx.status == 0) && (play->roomCtx.prevRoom.num >= 0)) {
            Room_FinishRoomChange(play, &play->roomCtx);
            if (sHunt.actorId == 0) {
                UH_SAY("[uh] room %d", play->roomCtx.curRoom.num);
                UhHuntStop(play); // asked for, arrived at, and Link is released onto whatever is there
                return;
            }
            sHunt.phase = UH_HUNT_LOOK;
        } else if (--sHunt.patience <= 0) {
            UH_ERR("[uh] room %d never finished loading", sHunt.room);
            UhHuntStop(play);
        }
        return;
    }

    // Not here. Next room.
    sHunt.room++;
    if (sHunt.room >= play->numRooms) {
        UH_ERR("[uh] no %s anywhere in this scene (%d rooms searched)", sHunt.label, play->numRooms);
        UhHuntStop(play);
        return;
    }
    if (play->roomCtx.status == 0) {
        Room_RequestNewRoom(play, &play->roomCtx, sHunt.room);
        sHunt.phase = UH_HUNT_LOADING;
        sHunt.patience = 600;
    } else {
        sHunt.room--; // something else is loading; try again next frame
    }
}

static void UhHuntBegin(const UhSpot* spot) {
    sHunt.actorId = spot->actorId;
    sHunt.label = spot->what;
    sHunt.room = -1;
    sHunt.patience = 600;
    sHunt.pinned = 0;
    sHunt.phase = UH_HUNT_WAIT_SCENE;
}

// The warp itself, lifted from Warping.cpp's Warp(). Only the in-game half: a console command
// cannot run before there is a PlayState, so the boot-to-point branch has no meaning here.
static void UhWarp(s32 entrance) {
    gPlayState->nextEntranceIndex = entrance;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
    gSaveContext.respawnFlag = 0;
}

static const UhSpot* UhLookup(const std::string& name) {
    for (size_t i = 0; i < ARRAY_COUNT(sSpots); i++) {
        if (name == sSpots[i].name) {
            return &sSpots[i];
        }
    }
    return NULL;
}

static void UhList() {
    UH_SAY("[uh] uh <spot> warps and finds it | uh find <spot> searches here | uh room <n>");
    for (size_t i = 0; i < ARRAY_COUNT(sSpots); i++) {
        UH_SAY("  %-9s %s", sSpots[i].name, sSpots[i].what);
    }
}

static bool UhHandler(std::shared_ptr<Ship::Console> console, const std::vector<std::string>& args,
                      std::string* output) {
    if (args.size() < 2) {
        UhList();
        return 0;
    }
    if (gPlayState == NULL) {
        UH_ERR("[uh] not in game");
        return 1;
    }

    // uh room <n> — the plain "take me to that room" the rest of this file exists to avoid needing.
    // Kept because when the hunt guesses wrong about which actor you meant, this is the way out.
    if (args[1] == "room") {
        s32 room;

        if (args.size() < 3) {
            UH_SAY("[uh] room %d of %d", gPlayState->roomCtx.curRoom.num, gPlayState->numRooms);
            return 0;
        }
        room = atoi(args[2].c_str());
        if ((room < 0) || (room >= gPlayState->numRooms)) {
            UH_ERR("[uh] this scene has rooms 0..%d", gPlayState->numRooms - 1);
            return 1;
        }
        if (gPlayState->roomCtx.status != 0) {
            UH_ERR("[uh] a room is already loading");
            return 1;
        }
        sHunt.pinned = 0;
        sHunt.actorId = 0; // nothing to find; the room IS the request
        sHunt.label = "room";
        sHunt.room = room - 1;
        sHunt.phase = UH_HUNT_LOOK;
        return 0;
    }

    // uh find <spot> — search the scene you are already standing in.
    if (args[1] == "find") {
        const UhSpot* spot = (args.size() >= 3) ? UhLookup(args[2]) : NULL;

        if (spot == NULL) {
            UH_ERR("[uh] unknown spot; `uh` lists them");
            return 1;
        }
        UhHuntBegin(spot);
        sHunt.room = gPlayState->roomCtx.curRoom.num; // start where you stand, not back at room 0
        sHunt.phase = UH_HUNT_LOOK;
        return 0;
    }

    {
        const UhSpot* spot = UhLookup(args[1]);

        if (spot == NULL) {
            UH_ERR("[uh] unknown spot; `uh` lists them");
            return 1;
        }
        UhHuntBegin(spot);
        UhWarp(spot->entrance);
        return 0;
    }
}

void RegisterCaneTestWarp() {
    CMD_REGISTER("uh", { UhHandler,
                         "Ultrahand test spots: warp to a scene and stand next to the actor.",
                         {
                             { "spot|find|room", Ship::ArgumentType::TEXT, true },
                             { "argument", Ship::ArgumentType::TEXT, true },
                         } });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        (void)sceneNum;
        if (sHunt.phase == UH_HUNT_WAIT_SCENE) {
            sHunt.phase = UH_HUNT_LOOK;
            sHunt.room = -1;
            sHunt.pinned = 0;
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(UhHuntTick);
}

static RegisterShipInitFunc initCaneTestWarp(RegisterCaneTestWarp, {});
