/**
 * This file handles custom messages relating to Items,
 * such as Get Item messages for non-vanilla items,
 * Vanilla/MQ hints when collecting Maps, Ice Trap messages,
 * etc.
 */
#include <soh/OTRGlobals.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/Traps.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/ShipInit.hpp"
#include <soh/ResourceManagerHelpers.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#include <cstdarg>

extern "C" {
#include <variables.h>
#include <macros.h>
#include "z64item.h"
extern PlayState* gPlayState;
extern u8 gLanternCatchPending; // item_lantern.c — fire type pending message display
}

// Forward declaration for custom item messages from randomizer.cpp
struct CustomItemMessageEntry {
    s16 rgId;
    ItemID itemId;
    const char* english;
    const char* german;
    const char* french;
};
extern const CustomItemMessageEntry* GetCustomItemMessage(s16 rgId);

void BuildTriforcePieceMessage(CustomMessage& msg) {
    uint8_t current = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected + 1;
    uint8_t required = OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_TRIFORCE_HUNT_PIECES_REQUIRED) + 1;
    uint8_t remaining = required - current;
    float percentageCollected = (float)current / (float)required;

    if (percentageCollected <= 0.25) {
        msg = { "You found a %yTriforce Piece%w!&%g[[current]]%w down, %c[[remaining]]%w to go. It's a start!",
                "Ein %yTriforce-Splitter%w! Du hast&%g[[current]]%w von %c[[required]]%w gefunden. Es ist ein&Anfang!",
                "Vous trouvez un %yFragment de la&Triforce%w! Vous en avez %g[[current]]%w, il en&reste "
                "%c[[remaining]]%w à trouver. C'est un début!" };
    } else if (percentageCollected <= 0.5) {
        msg = { "You found a %yTriforce Piece%w!&%g[[current]]%w down, %c[[remaining]]%w to go. Progress!",
                "Ein %yTriforce-Splitter%w! Du hast&%g[[current]]%w von %c[[required]]%w gefunden. Es geht voran!",
                "Vous trouvez un %yFragment de la&Triforce%w! Vous en avez %g[[current]]%w, il en&reste "
                "%c[[remaining]]%w à trouver. Ça avance!" };
    } else if (percentageCollected <= 0.75) {
        msg = { "You found a %yTriforce Piece%w!&%g[[current]]%w down, %c[[remaining]]%w to go. Over half-way&there!",
                "Ein %yTriforce-Splitter%w! Du hast&schon %g[[current]]%w von %c[[required]]%w gefunden. Schon&über "
                "die Hälfte!",
                "Vous trouvez un %yFragment de la&Triforce%w! Vous en avez %g[[current]]%w, il en&reste "
                "%c[[remaining]]%w à trouver. Il en reste un&peu moins que la moitié!" };
    } else if (percentageCollected < 1.0) {
        msg = {
            "You found a %yTriforce Piece%w!&%g[[current]]%w down, %c[[remaining]]%w to go. Almost done!",
            "Ein %yTriforce-Splitter%w! Du hast&schon %g[[current]]%w von %c[[required]]%w gefunden. Fast&geschafft!",
            "Vous trouvez un %yFragment de la&Triforce%w! Vous en avez %g[[current]]%w, il en&reste %c[[remaining]]%w "
            "à trouver. C'est presque&terminé!"
        };
    } else if (current == required) {
        msg = { "You completed the %yTriforce of&Courage%w! %gGG%w!",
                "Das %yTriforce des Mutes%w! Du hast&alle Splitter gefunden. %gGut gemacht%w!",
                "Vous avez complété la %yTriforce&du Courage%w! %gFélicitations%w!" };
    } else {
        msg = { "You found a spare %yTriforce Piece%w!&You only needed %c[[required]]%w, but you have %g[[current]]%w!",
                "Ein übriger %yTriforce-Splitter%w! Du&hast nun %g[[current]]%w von %c[[required]]%w nötigen gefunden.",
                "Vous avez trouvé un %yFragment de&Triforce%w en plus! Vous n'aviez besoin&que de %c[[required]]%w, "
                "mais vous en avez %g[[current]]%w en&tout!" };
    }
    msg.Replace("[[current]]", std::to_string(current));
    msg.Replace("[[remaining]]", std::to_string(remaining));
    msg.Replace("[[required]]", std::to_string(required));
    msg.Format(ITEM_CUSTOM);
}

void BuildCustomItemMessage(Player* player, CustomMessage& msg) {
    int16_t rgid;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        rgid = player->getItemEntry.getItemId;
    } else {
        rgid = player->getItemId;
    }

    // Check if this is a custom item with a detailed message
    const CustomItemMessageEntry* customMsg = GetCustomItemMessage(rgid);
    if (customMsg != nullptr) {
        // Use the detailed custom message. Pass the real ItemID so Message_LoadItemIcon's
        // ">= ITEM_ROCS_FEATHER_SKIJER" branch fires (z_message_PAL.c:1671) and loads the
        // 32x32 icon via ExtInv_GetItemIcon(itemId). Without this, AutoFormat() with no
        // argument leaves the message without an ITEM_OBTAINED token at all, and the
        // textbox renders with no icon on the left.
        msg = CustomMessage(customMsg->english, customMsg->german, customMsg->french, TEXTBOX_TYPE_BLUE);
        msg.AutoFormat(customMsg->itemId);
        return;
    }

    // Fall back to generic "You found X!" message for other items
    msg = CustomMessage("You found [[article]][[color]][[name]]%w!",
                        "Du erhältst [[article]][[color]][[name]]%w gefunden!",
                        "Vous avez trouvé [[article]][[color]][[name]]%w!", TEXTBOX_TYPE_BLUE);
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetName(), TEXTBOX_TYPE_BLUE);
    CustomMessage article = CustomMessage(
        Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetArticle(), TEXTBOX_TYPE_BLUE);
    msg.Replace("[[article]]", article);
    msg.Replace("[[color]]", Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetColor());
    msg.Replace("[[name]]", name);
    if (Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).HasCustomIcon()) {
        // Use the real ItemID from the item table so vanilla's Message_LoadItemIcon picks
        // up the ">= ITEM_ROCS_FEATHER_SKIJER" branch and resolves via ExtInv_GetItemIcon.
        ItemID itemId =
            static_cast<ItemID>(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetItemID());
        msg.AutoFormat(itemId);
    } else {
        msg.AutoFormat();
    }
}

void LoadCustomItemIcon(bool displayAsEnglish) {
    Player* player = GET_PLAYER(gPlayState);
    const char* customIcon = nullptr;
    CustomIconSize iconSize = ICON_SIZE_32;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        customIcon = Rando::StaticData::RetrieveItem(rgid).GetCustomIcon();
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    }
    if (customIcon != nullptr) {
        static int16_t sIconItem32XOffsets[] = { 74, 74, 74, 54 };
        static int16_t sIconItem24XOffsets[] = { 72, 72, 72, 50 };
        MessageContext* msgCtx = &gPlayState->msgCtx;
        uint8_t language = displayAsEnglish ? LANGUAGE_ENG : gSaveContext.language;
        if (iconSize == ICON_SIZE_32) {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem32XOffsets[language];
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 6;
            R_TEXTBOX_ICON_SIZE = 32;
        } else {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem24XOffsets[language];
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 10;
            R_TEXTBOX_ICON_SIZE = 24;
        }
        strcpy((char*)((uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE), customIcon);
        msgCtx->msgBufPos++;
        msgCtx->choiceNum = 1;
    }
}

void DrawCustomItemIcon(Gfx** p) {
    Gfx* gfx = *p;
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Player* player = GET_PLAYER(gPlayState);
    CustomIconSize iconSize = ICON_SIZE_32;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    }
    if (iconSize == ICON_SIZE_24) {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 24, 24, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    *p = gfx;
}

void BuildItemMessage(u16* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    CustomMessage msg;

    if (player->getItemEntry.getItemId == RG_ICE_TRAP) {
        Rando::Traps::BuildIceTrapMessage(msg, player->getItemEntry);
    } else if (player->getItemEntry.getItemId == RG_TRIFORCE_PIECE) {
        BuildTriforcePieceMessage(msg);
    } else {
        BuildCustomItemMessage(player, msg);
    }
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildMapMessage(uint16_t* textId, bool* loadFromMessageTable) {
    GetItemEntry itemEntry = GET_PLAYER(gPlayState)->getItemEntry;
    auto ctx = OTRGlobals::Instance->gRandoContext;
    CustomMessage msg =
        CustomMessage("You found the %g[[name]]%w! [[typeHint]]", "Du erhältst das %g[[name]]%w! [[typeHint]]",
                      "Vous ebtenez %g[[name]]%w! [[typeHint]]", TEXTBOX_TYPE_BLUE);
    int sceneNum;
    switch (itemEntry.getItemId) {
        case RG_DEKU_TREE_MAP:
            sceneNum = SCENE_DEKU_TREE;
            break;
        case RG_DODONGOS_CAVERN_MAP:
            sceneNum = SCENE_DODONGOS_CAVERN;
            break;
        case RG_JABU_JABUS_BELLY_MAP:
            sceneNum = SCENE_JABU_JABU;
            break;
        case RG_FOREST_TEMPLE_MAP:
            sceneNum = SCENE_FOREST_TEMPLE;
            break;
        case RG_FIRE_TEMPLE_MAP:
            sceneNum = SCENE_FIRE_TEMPLE;
            break;
        case RG_WATER_TEMPLE_MAP:
            sceneNum = SCENE_WATER_TEMPLE;
            break;
        case RG_SPIRIT_TEMPLE_MAP:
            sceneNum = SCENE_SPIRIT_TEMPLE;
            break;
        case RG_SHADOW_TEMPLE_MAP:
            sceneNum = SCENE_SHADOW_TEMPLE;
            break;
        case RG_BOTTOM_OF_THE_WELL_MAP:
            sceneNum = SCENE_BOTTOM_OF_THE_WELL;
            break;
        case RG_ICE_CAVERN_MAP:
            sceneNum = SCENE_ICE_CAVERN;
            break;
    }
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(itemEntry.getItemId)).GetName());
    msg.Replace("[[name]]", name);
    if (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_NONE) ||
        (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_SET_NUMBER) &&
         ctx->GetOption(RSK_MQ_DUNGEON_COUNT).Is(MAX_MQ_DUNGEON_COUNT))) {
        msg.Replace("[[typeHint]]", "");
    } else if (ResourceMgr_IsSceneMasterQuest(sceneNum)) {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_MASTERFUL].GetHintMessage());
    } else {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_ORDINARY].GetHintMessage());
    }
    *loadFromMessageTable = false;
    msg.AutoFormat(ITEM_DUNGEON_MAP);
    msg.LoadIntoFont();
}

void BuildBossKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildSmallKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GERUDO_FORTRESS_SMALL_KEY &&
        OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).Is(RO_GERUDO_KEYS_VANILLA)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GERUDO_FORTRESS_SMALL_KEY &&
        DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

// Time Gate custom item - "Travel through time?" Yes/No prompt
void BuildTimeGateMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("Travel through time?\x1B%g&&Yes&No%w", "Durch die Zeit reisen?\x1B%g&&Ja&Nein%w",
                                      "Voyager dans le temps?\x1B%g&&Oui&Non%w");
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterItemMessages() {
    COND_ID_HOOK(OnOpenText, TEXT_RANDOMIZER_CUSTOM_ITEM, IS_RANDO, BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_DUNGEON_MAP, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildMapMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_COMPASS, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_BOSS,
                 (DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)),
                 BuildBossKeyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_SMALL,
                 (OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).IsNot(RO_GERUDO_KEYS_VANILLA) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)),
                 BuildSmallKeyMessage);
}

// ── Lantern fire catch messages (always available) ──────────────────────────

#define TEXT_LANTERN_CATCH 0x00F9

void BuildLanternCatchMessage(uint16_t* textId, bool* loadFromMessageTable) {
    u8 fireType = gLanternCatchPending;
    CustomMessage msg;

    // \x13\xB4 = item icon for ITEM_LANTERN (0xB4)
    // All fire types: swing lights torches, burns grass (updraft + spread)
    switch (fireType) {
        case 1: // REGULAR (orange)
            msg = CustomMessage(
                "\x13\xB4"
                "You caught %rRegular Fire%w!&Swing to %rlight torches%w,&%rburn grass%w and spawn flames.",
                "\x13\xB4"
                "Du hast %rnormales Feuer%w!&Schwinge um %rFackeln%w und&%rGras zu verbrennen%w.",
                "\x13\xB4"
                "Vous avez le %rFeu Normal%w!&Agitez pour %rallumer%w et&%rbruler l'herbe%w.",
                TEXTBOX_TYPE_BLUE);
            break;
        case 2: // BLUE
            msg = CustomMessage("\x13\xB4"
                                "You caught %bBlue Fire%w!&Swing to release %bblue fire%w&that %cmelts red ice%w.",
                                "\x13\xB4"
                                "Du hast %bblaues Feuer%w!&Schwinge um %crotes Eis%w&%bzu schmelzen%w.",
                                "\x13\xB4"
                                "Vous avez le %bFeu Bleu%w!&Agitez pour %cfondre la&glace rouge%w.",
                                TEXTBOX_TYPE_BLUE);
            break;
        case 3: // POE (purple)
            msg = CustomMessage("\x13\xB4"
                                "You caught %pPoe Fire%w!&%pReveals hidden spirits%w&and invisible actors.",
                                "\x13\xB4"
                                "Du hast %pIrrlichterfeuer%w!&%pEnthullt verborgene Geister%w&und unsichtbare Akteure.",
                                "\x13\xB4"
                                "Vous avez le %pFeu Spectral%w!&%pRevele les esprits caches%w&et acteurs invisibles.",
                                TEXTBOX_TYPE_BLUE);
            break;
        case 4: // GREEN
            msg = CustomMessage(
                "\x13\xB4"
                "You caught %gGreen Fire%w!&%gReveals hidden spirits%w.&Slowly %gregenerates health%w.",
                "\x13\xB4"
                "Du hast %ggruenes Feuer%w!&%gEnthullt verborgene Geister%w.&%gRegeneriert langsam Leben%w.",
                "\x13\xB4"
                "Vous avez le %gFeu Vert%w!&%gRevele les esprits caches%w.&%gRegene lentement la vie%w.",
                TEXTBOX_TYPE_BLUE);
            break;
        default:
            msg = CustomMessage("\x13\xB4"
                                "The lantern is empty.",
                                "\x13\xB4"
                                "Die Laterne ist leer.",
                                "\x13\xB4"
                                "La lanterne est vide.",
                                TEXTBOX_TYPE_BLUE);
            break;
    }

    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterLanternCatchMessage() {
    // Always available — not randomizer-dependent
    static HOOK_ID hookId = 0;
    GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnOpenText>(hookId);
    hookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_LANTERN_CATCH,
                                                                                         BuildLanternCatchMessage);
}

// Time Gate message registration (always available, not rando-dependent)
void RegisterTimeGateMessage() {
    COND_ID_HOOK(OnOpenText, 0x9213, true, BuildTimeGateMessage);
}

// Chateau Romani get-item message (always available, not rando-dependent)
void BuildChateauRomaniMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("You got %r\x08"
                                      "Chateau Romani%w!\x04"
                                      "Your magic power won't run out!%w",
                                      "Du hast %r\x08"
                                      "Chateau Romani%w erhalten!\x04"
                                      "Deine Magie wird nicht leer!%w",
                                      "Vous obtenez le %r\x08"
                                      "Chateau Romani%w!\x04"
                                      "Votre magie ne s'\xE9puisera pas!%w");
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterChateauRomaniMessage() {
    COND_ID_HOOK(OnOpenText, 0x9214, true, BuildChateauRomaniMessage);
}

static RegisterShipInitFunc initFunc(RegisterItemMessages, { "IS_RANDO" });
static RegisterShipInitFunc initTimeGate(RegisterTimeGateMessage);
static RegisterShipInitFunc initChateau(RegisterChateauRomaniMessage);
static RegisterShipInitFunc initLanternCatch(RegisterLanternCatchMessage);

void RegisterCustomIconHooks() {
    // The original hook only fires when *should == false, but nothing in the call path
    // ever sets it to false for custom items — so the custom icon loaders never run and
    // vanilla tries to load Message_LoadItemIcon(ITEM_CUSTOM=0x9C) which is not a valid
    // OBJECT_GI_*. Detect custom-icon items via the player's getItemEntry, suppress
    // vanilla, and call our loader/drawer.
    COND_VB_SHOULD(VB_LOAD_ITEM_ICON, IS_RANDO, {
        Player* player = GET_PLAYER(gPlayState);
        if (player->getItemEntry.objectId != OBJECT_INVALID) {
            RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
            if (Rando::StaticData::RetrieveItem(rgid).HasCustomIcon()) {
                *should = false;
                LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
                return;
            }
        }
        if (*should == false) {
            LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
        }
    });
    COND_VB_SHOULD(VB_DRAW_ITEM_ICON, IS_RANDO, {
        Player* player = GET_PLAYER(gPlayState);
        if (player->getItemEntry.objectId != OBJECT_INVALID) {
            RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
            if (Rando::StaticData::RetrieveItem(rgid).HasCustomIcon()) {
                *should = false;
                DrawCustomItemIcon(va_arg(args, Gfx**));
                return;
            }
        }
        if (*should == false) {
            DrawCustomItemIcon(va_arg(args, Gfx**));
        }
    });
}

static RegisterShipInitFunc customIconInitFunc(RegisterCustomIconHooks, { "IS_RANDO" });
