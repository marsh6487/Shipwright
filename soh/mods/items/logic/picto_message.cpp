/**
 * picto_message.cpp - Pictograph Box "Keep this picture?" textbox (Skijer's NEI).
 *
 * MM shows a 2-choice message right after the shutter (z_parameter.c: Message_StartTextbox 0xF8):
 * Keep / Discard. SOH has no MM message infra, so we register an OnOpenText hook for a custom textId
 * and build the prompt with the existing NEI custom-message system (CustomMessageManager) — the same
 * pattern as clm_behavior.cpp (Circus Leader's Mask). picto_box.c opens it via
 * Message_StartTextbox(PICTO_KEEP_TEXTID) and reads msgCtx.choiceIndex (0 = keep, !=0 = discard),
 * exactly like MM's PICTO_BOX_STATE_PHOTO handler.
 *
 * New .cpp -> add it in the VS Solution Explorer (the CMake mods glob picks up *.cpp).
 */

#include <spdlog/spdlog.h>
#include <soh/OTRGlobals.h>
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "mods/items/logic/snap.h" // Snap_CheckFlag + PICTO_VALID_* (the photographed subject)
#include "mods/nei_save.h"          // Nei_Save()->pictoHasPhoto (replace-vs-keep wording)
}

#define PICTO_KEEP_TEXTID 0x6F08    // must match soh/mods/items/logic/snap.h
#define PICTO_REPLACE_TEXTID 0x6F09 // must match soh/mods/items/logic/snap.h

// Build MM's "Keep this picture?" 2-choice prompt. TWO_WAY_CHOICE() = \x1B; the two options follow it
// and AutoFormat lays them onto the choice lines. choiceIndex 0 = "Keep it", 1 = "Throw it away".
static void Picto_BuildKeepMessage(uint16_t* textId, bool* loadFromMessageTable) {
    // Name the photographed subject, like 2Ship's BetterPictoMessage: read the validation flags set at
    // the shutter (Snap_RecordPictographedActors) and pick the target. Later checks override earlier;
    // Lulu needs all three body parts. -> "Keep this picture of a Pirate?" etc.
    std::string target;
    if (Snap_CheckFlag(PICTO_VALID_IN_SWAMP))
        target = "the Swamp";
    if (Snap_CheckFlag(PICTO_VALID_MONKEY))
        target = "a Monkey";
    if (Snap_CheckFlag(PICTO_VALID_BIG_OCTO))
        target = "a Big Octo";
    if (Snap_CheckFlag(PICTO_VALID_LULU_HEAD) && Snap_CheckFlag(PICTO_VALID_LULU_RIGHT_ARM) &&
        Snap_CheckFlag(PICTO_VALID_LULU_LEFT_ARM))
        target = "Lulu";
    if (Snap_CheckFlag(PICTO_VALID_SCARECROW))
        target = "a Scarecrow";
    if (Snap_CheckFlag(PICTO_VALID_TINGLE))
        target = "Tingle";
    if (Snap_CheckFlag(PICTO_VALID_PIRATE_GOOD))
        target = "a Pirate";
    if (Snap_CheckFlag(PICTO_VALID_DEKU_KING))
        target = "the Deku King";

    // If a photo is already saved, warn that keeping this one replaces it (the pictograph syncs
    // OoT<->MM). %g = green choices (OoT convention); the color carries across the newline.
    std::string question;
    if (Nei_Save()->pictoHasPhoto) {
        question = target.empty() ? std::string("You already have a %rpicture%w. Replace it?")
                                  : ("You already have a %rpicture%w. Replace it with this of " + target + "?");
    } else {
        question = target.empty() ? std::string("Keep this %rpicture%w?")
                                  : ("Keep this %rpicture of " + target + "%w?");
    }
    CustomMessage msg = CustomMessage(question + CustomMessage::TWO_WAY_CHOICE() + "%gKeep it&Throw it away");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// "You already have a pictograph. Replace it?" — shown before overwriting an existing (synced) photo.
// choiceIndex 0 = "Replace it", 1 = "Cancel".
static void Picto_BuildReplaceMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("You already have a %rpictograph%w. Replace it?" +
                                      CustomMessage::TWO_WAY_CHOICE() + "%gReplace it&Cancel");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// Gag message when an MM trade-quest item is "used" (trade_items.c present flow) — the classic line.
// Plain single-box message; & = newline.
static void Picto_BuildTradeUseMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg =
        CustomMessage("Oak's words echoed... There's a time and place for everything, but not now.");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void Picto_RegisterMessageHooks() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(PICTO_KEEP_TEXTID,
                                                                                Picto_BuildKeepMessage);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(PICTO_REPLACE_TEXTID,
                                                                                Picto_BuildReplaceMessage);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MM_TRADE_USE_TEXTID,
                                                                                Picto_BuildTradeUseMessage);
}

static RegisterShipInitFunc sPictoMessageInit(Picto_RegisterMessageHooks);
