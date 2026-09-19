/*
 * File: z_bg_toki_swd.c
 * Overlay: ovl_Bg_Toki_Swd
 * Description: Master Sword (Contains Cutscenes)
 */

#include "z_bg_toki_swd.h"
#include "objects/object_toki_objects/object_toki_objects.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/SwitchAge.h"

#include <stdlib.h>

#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED

void BgTokiSwd_Init(Actor* thisx, PlayState* play);
void BgTokiSwd_Destroy(Actor* thisx, PlayState* play);
void BgTokiSwd_Update(Actor* thisx, PlayState* play);
void BgTokiSwd_Draw(Actor* thisx, PlayState* play);

void func_808BAF40(BgTokiSwd* this, PlayState* play);
void func_808BB0AC(BgTokiSwd* this, PlayState* play);
void func_808BB128(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_TimePedestalWait(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_TimePedestalCutscene(BgTokiSwd* this, PlayState* play);
void BgTokiSwd_FinishTimePedestal(BgTokiSwd* this, PlayState* play);
void func_80068DC0(PlayState* play, CutsceneContext* csCtx);

extern CutsceneData D_808BB2F0[];
extern CutsceneData D_808BB7A0[];
extern CutsceneData D_808BBD90[];
extern const size_t gMasterSwordChildCutsceneWordCount;
extern const size_t gMasterSwordAdultCutsceneWordCount;

const ActorInit Bg_Toki_Swd_InitVars = {
    ACTOR_BG_TOKI_SWD,
    ACTORCAT_PROP,
    FLAGS,
    OBJECT_TOKI_OBJECTS,
    sizeof(BgTokiSwd),
    (ActorFunc)BgTokiSwd_Init,
    (ActorFunc)BgTokiSwd_Destroy,
    (ActorFunc)BgTokiSwd_Update,
    (ActorFunc)BgTokiSwd_Draw,
    NULL,
};

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1 | OC2_UNK1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 10, 70, 0, { 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { 10, 35, 100, MASS_IMMOVABLE };

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 25, ICHAIN_STOP),
};

void BgTokiSwd_SetupAction(BgTokiSwd* this, BgTokiSwdActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void BgTokiSwd_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    this->actor.shape.yOffset = 800.0f;
    BgTokiSwd_SetupAction(this, func_808BAF40);
    this->localCutscene = NULL;
    this->localCutsceneStarted = false;
    this->localCutsceneFinished = false;

    if (this->actor.params == BG_TOKI_SWD_TIME_PEDESTAL) {
        BgTokiSwd_SetupAction(this, BgTokiSwd_TimePedestalWait);
        if (LINK_IS_ADULT) {
            this->actor.draw = NULL;
        }
    } else if (LINK_IS_ADULT) {
        if (IS_RANDO) {
            if (!CUR_UPG_VALUE(UPG_BOMB_BAG)) {
                for (size_t i = 0; i < 8; i++) {
                    if (gSaveContext.equips.buttonItems[i] == ITEM_BOMB) {
                        gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                    }
                }
            }
        }
        this->actor.draw = NULL;
    } else if (IS_RANDO) {
        // don't give child link a kokiri sword if we don't have one
        uint32_t kokiriSwordBitMask = 1 << 0;
        if (!(gSaveContext.inventory.equipment & kokiriSwordBitMask)) {
            Player* player = GET_PLAYER(gPlayState);
            player->currentSwordItemId = ITEM_NONE;
            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        }
    }

    if (this->actor.params != BG_TOKI_SWD_TIME_PEDESTAL && gSaveContext.sceneLayer == 5) {
        play->roomCtx.unk_74[0] = 0xFF;
    }

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
}

void BgTokiSwd_Destroy(Actor* thisx, PlayState* play) {
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    if (this->localCutscene != NULL) {
        // Actor teardown happens after the age-equipment handoff and after the
        // player is deleted. It must never request another age transition.
        if (play->csCtx.segment == this->localCutscene) {
            Player* player = GET_PLAYER(play);
            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
            if (play->state.running && player != NULL) {
                // If only this actor is removed, use the native camera cleanup
                // before releasing the camera spline's backing allocation.
                play->csCtx.unk_0C = 0.0f;
                func_80068DC0(play, &play->csCtx);
                player->interactRangeActor = NULL;
                player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
                Player_SetCsAction(play, NULL, PLAYER_CSACTION_7);
                Environment_PlaySceneSequence(play);
            } else {
                play->csCtx.state = CS_STATE_IDLE;
                gSaveContext.cutsceneIndex = 0;
                Audio_SetCutsceneFlag(0);
            }
            gSaveContext.cutsceneTrigger = 0;
            play->csCtx.segment = NULL;
            play->csCtx.linkAction = NULL;
        }
        free(this->localCutscene);
        this->localCutscene = NULL;
    }
    Collider_DestroyCylinder(play, &this->collider);
}

void BgTokiSwd_TimePedestalWait(BgTokiSwd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (!play->state.running || player == NULL) {
        return;
    }

    if (Actor_HasParent(&this->actor, play)) {
        const CutsceneData* source = LINK_IS_ADULT ? D_808BB7A0 : D_808BB2F0;
        size_t count = LINK_IS_ADULT ? gMasterSwordAdultCutsceneWordCount : gMasterSwordChildCutsceneWordCount;
        this->localCutscene = malloc(count * sizeof(CutsceneData));
        if (this->localCutscene == NULL ||
            !TimePedestalCutscene_Build(this->localCutscene, count, source, count, &this->actor.world.pos,
                                      this->actor.shape.rot.y, &this->ageSwapFrame)) {
            free(this->localCutscene);
            this->localCutscene = NULL;
            this->actor.parent = NULL;
            player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
            Player_SetCsAction(play, NULL, PLAYER_CSACTION_7);
            return;
        }
        this->returnPos = player->actor.world.pos;
        this->returnYaw = player->actor.shape.rot.y;
        this->localCutsceneStarted = false;
        player->interactRangeActor = &this->actor;
        this->actor.parent = NULL;
        play->csCtx.segment = this->localCutscene;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_MASTER_SWORD);
        gSaveContext.cutsceneTrigger = 1;
        BgTokiSwd_SetupAction(this, BgTokiSwd_TimePedestalCutscene);
    } else if (play->transitionTrigger == TRANS_TRIGGER_OFF && !Play_InCsMode(play) &&
               Actor_IsFacingPlayer(&this->actor, 0x2000)) {
        Actor_OfferCarry(&this->actor, play);
    }
}

s32 BgTokiSwd_RelocateTimePedestalPlayer(PlayState* play, Player* player) {
    Actor* actor = player->interactRangeActor;
    if (actor != NULL && actor->id == ACTOR_BG_TOKI_SWD && actor->params == BG_TOKI_SWD_TIME_PEDESTAL) {
        BgTokiSwd* this = (BgTokiSwd*)actor;
        if (this->localCutscene != NULL && play->csCtx.segment == this->localCutscene) {
            Vec3f pos = { -1.0f, 70.0f, 20.0f };
            TimePedestalCutscene_TransformPoint(&pos, &actor->world.pos, actor->shape.rot.y);
            player->actor.world.pos = pos;
            player->yaw = player->actor.shape.rot.y = actor->shape.rot.y + 0x8000;
            return true;
        }
    }
    return false;
}

void BgTokiSwd_FinishTimePedestal(BgTokiSwd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (!play->state.running || player == NULL || this->localCutsceneFinished ||
        play->transitionTrigger == TRANS_TRIGGER_START || play->csCtx.segment != this->localCutscene) {
        return;
    }

    this->localCutsceneFinished = true;
    player->actor.world.pos = this->returnPos;
    player->actor.shape.rot.y = this->returnYaw;
    player->yaw = this->returnYaw;
    player->interactRangeActor = NULL;
    this->actor.parent = NULL;
    // Match the native terminal cutscene state without executing its destination
    // command, which would edit story flags and enter the Temple of Time.
    play->csCtx.state = CS_STATE_UNSKIPPABLE_EXEC;
    play->csCtx.linkAction = NULL;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.cutsceneTrigger = 0;
    Audio_SetCutsceneFlag(0);
    // Some Lost Woods entrances continue BGM. The sword fanfare replaced the
    // scene track, so force normal scene music to restart on the local reload.
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
    SwitchAgeWithoutProgression();
}

void BgTokiSwd_TimePedestalCutscene(BgTokiSwd* this, PlayState* play) {
    if (!play->state.running || GET_PLAYER(play) == NULL || this->localCutsceneFinished ||
        play->transitionTrigger == TRANS_TRIGGER_START || play->csCtx.segment != this->localCutscene) {
        return;
    }
    if (play->csCtx.state != CS_STATE_IDLE) {
        this->localCutsceneStarted = true;
    }
    // Preserve the native frame-70/87 handoff, sword model, sounds and animation.
    if (Actor_HasParent(&this->actor, play)) {
        if (!LINK_IS_ADULT) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PUTAWAY_STN);
        }
        this->actor.draw = LINK_IS_ADULT ? BgTokiSwd_Draw : NULL;
        this->actor.parent = NULL;
    }
    GET_PLAYER(play)->interactRangeActor = &this->actor;

    if (this->localCutsceneStarted &&
        (play->csCtx.frames >= this->ageSwapFrame || play->csCtx.state == CS_STATE_UNSKIPPABLE_INIT ||
         play->csCtx.state == CS_STATE_IDLE)) {
        BgTokiSwd_FinishTimePedestal(this, play);
    }
}

void func_808BAF40(BgTokiSwd* this, PlayState* play) {
    if (((Flags_GetEventChkInf(EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER)) == 0) && (gSaveContext.sceneLayer < 4) &&
        Actor_IsFacingAndNearPlayer(&this->actor, 800.0f, 0x7530) && !Play_InCsMode(play)) {
        Flags_SetEventChkInf(EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER);
        if (GameInteractor_Should(VB_PLAY_ENTRANCE_CS, true, EVENTCHKINF_ENTERED_MASTER_SWORD_CHAMBER,
                                  gSaveContext.entranceIndex)) {
            play->csCtx.segment = D_808BBD90;
            gSaveContext.cutsceneTrigger = 1;
        }
    }

    if (!LINK_IS_ADULT || (Flags_GetEventChkInf(EVENTCHKINF_LEARNED_PRELUDE_OF_LIGHT) && !IS_RANDO) || IS_RANDO) {
        if (Actor_HasParent(&this->actor, play)) {
            if (!LINK_IS_ADULT) {
                if (GameInteractor_Should(VB_GIVE_ITEM_MASTER_SWORD, true)) {
                    Item_Give(play, ITEM_SWORD_MASTER);
                }
                play->csCtx.segment = D_808BB2F0;

                // Discover adult spawn
                Entrance_SetEntranceDiscovered(ENTR_HYRULE_FIELD_10, false);
            } else {
                play->csCtx.segment = D_808BB7A0;

                // Discover child spawn
                Entrance_SetEntranceDiscovered(ENTR_LINKS_HOUSE_CHILD_SPAWN, false);
            }
            Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);
            Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_MASTER_SWORD);
            gSaveContext.cutsceneTrigger = 1;
            this->actor.parent = NULL;
            BgTokiSwd_SetupAction(this, func_808BB0AC);
        } else {
            Player* player = GET_PLAYER(play);
            if (Actor_IsFacingPlayer(&this->actor, 0x2000)) {
                Actor_OfferCarry(&this->actor, play);
            }
        }
    }
    if (gSaveContext.sceneLayer == 5) {
        if (play->roomCtx.unk_74[0] > 0) {
            play->roomCtx.unk_74[0]--;
        } else {
            play->roomCtx.unk_74[0] = 0;
        }
    }
}

void func_808BB0AC(BgTokiSwd* this, PlayState* play) {
    Player* player;

    // if sword has a parent it has been pulled/placed from the pedestal
    if (Actor_HasParent(&this->actor, play)) {
        if (!LINK_IS_ADULT) {
            Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_PUTAWAY_STN);
            this->actor.draw = NULL; // sword has been pulled, dont draw sword
        } else {
            this->actor.draw = BgTokiSwd_Draw; // sword has been placed, draw the master sword
        }
        BgTokiSwd_SetupAction(this, func_808BB128);
    } else {
        player = GET_PLAYER(play);
        player->interactRangeActor = &this->actor;
    }
}

void func_808BB128(BgTokiSwd* this, PlayState* play) {
    if (Flags_GetEnv(play, 1) && (play->roomCtx.unk_74[0] < 0xFF)) {
        play->roomCtx.unk_74[0] += 5;
    }
}

void BgTokiSwd_Update(Actor* thisx, PlayState* play) {
    BgTokiSwd* this = (BgTokiSwd*)thisx;

    this->actionFunc(this, play);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void BgTokiSwd_Draw(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    BgTokiSwd* this = (BgTokiSwd*)thisx;
    s32 pad[3];

    // Do not draw the Master Sword in the pedestal if the player has not found it yet
    if (this->actor.params != BG_TOKI_SWD_TIME_PEDESTAL && IS_RANDO &&
        Randomizer_GetSettingValue(RSK_SHUFFLE_MASTER_SWORD) &&
        !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    func_8002EBCC(&this->actor, play, 0);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TexScrollEx(play->state.gfxCtx, 0, -(play->gameplayFrames % 0x80), 32, 32, 0, -1));
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_toki_objects_DL_001BD0);

    CLOSE_DISPS(play->state.gfxCtx);
}
