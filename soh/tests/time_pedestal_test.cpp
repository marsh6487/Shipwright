// The breaks caught here are a variant entering vanilla story/init branches,
// losing room/position at the native cutscene end, or equipping an unowned sword.
#include "time_pedestal_fixture.h"
#include "test_require.h"
#include <cstdio>
#include <cstdarg>
#include <vector>

std::map<u32, Gfx*> fixturePakEquipment;
static int skipStorySetting;
static bool pakActive, altActive, replaceHandHook, invisibleLink;
static Gfx nativeSword, altSword, pakSword, pakHandSword, emptyHand, wrongSword;
static const void* drawnSword;
static bool customMasterAsset, altAssetsSetting = true;
static Gfx customMasterSword, customHandSword;
static bool animationComplete;
static int normalSwordEquipCalls, startModeHookCalls;
static int cutsceneAudioFlag;
static int postSwordDrawCalls;
static int cameraCreates, cameraCopies, cameraClears, letterboxSize;
static bool failSubCamera;
static LinkAnimationHeader idleAnimation, childArrivalAnimation, adultArrivalAnimation;

static Vec3f RotateZYX(Vec3s rot, Vec3f vector) {
    const float sx = sinf(rot.x * (3.14159265358979323846f / 32768.0f));
    const float cx = cosf(rot.x * (3.14159265358979323846f / 32768.0f));
    const float sy = sinf(rot.y * (3.14159265358979323846f / 32768.0f));
    const float cy = cosf(rot.y * (3.14159265358979323846f / 32768.0f));
    const float sz = sinf(rot.z * (3.14159265358979323846f / 32768.0f));
    const float cz = cosf(rot.z * (3.14159265358979323846f / 32768.0f));
    const Vec3f afterX = { vector.x, cx * vector.y - sx * vector.z, sx * vector.y + cx * vector.z };
    const Vec3f afterY = { cy * afterX.x + sy * afterX.z, afterX.y, -sy * afterX.x + cy * afterX.z };
    return { cz * afterY.x - sz * afterY.y, sz * afterY.x + cz * afterY.y, afterY.z };
}

static void RequireOppositeBladeDirection(Vec3s beforeRot, Vec3s afterRot) {
    // All compatible hand/sword DLs use the held-equipment convention: the
    // blade runs from the hand toward local -X. Verify the complete ZYX result,
    // rather than checking one chosen Euler component.
    const Vec3f bladeAxis = { -1.0f, 0.0f, 0.0f };
    const Vec3f before = RotateZYX(beforeRot, bladeAxis);
    const Vec3f after = RotateZYX(afterRot, bladeAxis);
    REQUIRE(fabsf(before.x + after.x) < 0.0001f);
    REQUIRE(fabsf(before.y + after.y) < 0.0001f);
    REQUIRE(fabsf(before.z + after.z) < 0.0001f);
}

extern "C" {
SaveContext gSaveContext;
PlayState* gPlayState;
bool fixtureRando;
FixtureExtendedEquipment gExtEquipState;
static FixtureNeiSave neiSave;
FixtureNeiSave* Nei_Save(void) { return &neiSave; }
void ExtEquip_CleanupSlot(s16, u8) {}
void ExtEquip_ReloadBIcon(void) {}
void ExtEquip_RefreshPlayer(void) {}
s32 CVarGetInteger(const char* name, s32 fallback) {
    if (strcmp(name, CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Story")) == 0 && skipStorySetting >= 0)
        return skipStorySetting;
    if (strcmp(name, CVAR_SETTING("AltAssets")) == 0) return altAssetsSetting;
    return fallback;
}
const char gLinkChildLeftHandHoldingMasterSwordDL[] = "__OTR__objects/object_link_child/gLinkChildLeftHandHoldingMasterSwordDL";
const char object_toki_objects_DL_001BD0[] = "__OTR__objects/object_toki_objects/object_toki_objects_DL_001BD0";
void Fixture_RecordDraw(const void* dl) { drawnSword = dl; }
void BossRemains_DrawOdolwaSword(PlayState*, Player*) { postSwordDrawCalls++; }
void Matrix_Push(void) {}
void Matrix_Pop(void) {}
void Matrix_Translate(f32, f32, f32, u8) {}
void Matrix_Scale(f32, f32, f32, u8) {}
void Matrix_RotateZ(f32, u8) {}
s16 Play_CreateSubCamera(PlayState* play) {
    cameraCreates++;
    if (failSubCamera) return SUBCAM_NONE;
    REQUIRE(!play->subCameraAllocated);
    play->subCameraAllocated = true;
    play->subCamera.uid = 42;
    return 1;
}
s16 Play_GetActiveCamId(PlayState* play) { return play->activeCamera; }
Camera* Play_GetCamera(PlayState* play, s16 id) {
    REQUIRE(id == CAM_ID_MAIN || id == 1);
    return id == CAM_ID_MAIN ? &play->camera : play->subCameraAllocated ? &play->subCamera : nullptr;
}
s16 Play_CameraGetUID(PlayState* play, s16 id) {
    Camera* camera = Play_GetCamera(play, id);
    return camera != nullptr ? camera->uid : -1;
}
s16 Play_ChangeCameraStatus(PlayState* play, s16 id, s16 status) {
    Camera* camera = Play_GetCamera(play, id);
    REQUIRE(camera != nullptr);
    camera->status = status;
    if (status == CAM_STAT_ACTIVE) play->activeCamera = id;
    return status;
}
void Play_ClearCamera(PlayState* play, s16 id) {
    REQUIRE(id == 1 && play->subCameraAllocated);
    cameraClears++;
    play->subCameraAllocated = false;
}
s32 func_800C0808(PlayState* play, s16 id, Player* player, s16 setting) {
    REQUIRE(player == GET_PLAYER(play));
    Play_GetCamera(play, id)->setting = setting;
    return setting;
}
s32 Play_CameraSetAtEye(PlayState* play, s16 id, Vec3f* at, Vec3f* eye) {
    Play_GetCamera(play, id)->at = *at;
    Play_GetCamera(play, id)->eye = *eye;
    return 3;
}
s32 Play_CameraSetFov(PlayState* play, s16 id, f32 fov) {
    Play_GetCamera(play, id)->fov = fov;
    return 1;
}
void Play_CopyCamera(PlayState* play, s16 to, s16 from) {
    cameraCopies++;
    Camera* dst = Play_GetCamera(play, to);
    Camera* src = Play_GetCamera(play, from);
    dst->at = src->at;
    dst->eye = src->eye;
    dst->fov = src->fov;
}
void Letterbox_SetSizeTarget(s32 size) { letterboxSize = size; }
Gfx* PakLoader_GetTimePedestalSwordDL(void) {
    auto it = fixturePakEquipment.find(0x5450);
    return pakActive && it != fixturePakEquipment.end() && it->second != PAK_DL_STUB ? it->second : nullptr;
}
Gfx* PakLoader_GetTimePedestalHandDL(void) { return pakActive ? &emptyHand : nullptr; }
const char gCustomMasterSwordDL[] = "__OTR__objects/object_custom_equip/gCustomMasterSwordDL";
const char gLinkChildLeftFistNearDL[] = "__OTR__objects/object_link_child/gLinkChildLeftFistNearDL";
const char gLinkAdultLeftHandClosedNearDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftHandClosedNearDL";
const char gLinkAdultLeftHandHoldingMasterSwordNearDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL";
const char gLinkAdultLeftHandHoldingMasterSwordFarDL[] = "__OTR__objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordFarDL";
u8 ResourceMgr_FileAltExists(const char* path) { return customMasterAsset && strcmp(path, gCustomMasterSwordDL) == 0; }
u8 ResourceGetIsCustomByName(const char*) { return false; }
u8 TransformMasks_IsTransformedAny(void) { return false; }
Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    if (strcmp(path, gCustomMasterSwordDL) == 0) return &customMasterSword;
    REQUIRE(strcmp(path, LINK_IS_ADULT ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL) == 0);
    return &emptyHand;
}
void Fixture_BuildHandItemDL(PlayState*, Gfx** dl, Gfx* hand, Gfx* sword, bool scale) {
    REQUIRE(hand == &emptyHand && (sword == &customMasterSword || sword == &pakSword) && !scale);
    *dl = sword == &pakSword ? &pakHandSword : &customHandSword;
}
Gfx* gPlayerLeftHandBgsDLs[] = { &wrongSword, &nativeSword, &wrongSword, &nativeSword };
Gfx* gPlayerLeftHandClosedDLs[] = { &emptyHand, &emptyHand, &emptyHand, &emptyHand };
Gfx* Player_ResolveLimbDLForDummyOrLocal(void* path) {
    if (path == &emptyHand) return &emptyHand;
    REQUIRE(strcmp(static_cast<const char*>(path), LINK_IS_ADULT ? gLinkAdultLeftHandHoldingMasterSwordNearDL :
                                                               gLinkChildLeftHandHoldingMasterSwordDL) == 0);
    return altActive ? &altSword : &nativeSword;
}
u8 PakLoader_HasActiveModel(void) { return pakActive; }
bool GameInteractor_InvisibleLinkActive(void) { return invisibleLink; }
s32 LinkAnimation_Update(PlayState*, SkelAnime*) { return animationComplete; }
void LinkAnimation_Change(PlayState*, SkelAnime* skel, LinkAnimationHeader* anim, f32, f32 start, f32 end, u8, f32) {
    skel->animation = anim;
    skel->curFrame = start;
    skel->endFrame = end;
    skel->animLength = 180;
}
void Player_Action_Idle(Player*, PlayState*) {}
void Player_StartMode_Idle(PlayState*, Player* player) { player->actionFunc = Player_Action_Idle; }
s32 Player_SetupAction(PlayState*, Player* player, PlayerActionFunc action, s32) {
    player->actionFunc = action;
    player->av1.actionVar1 = player->av2.actionVar2 = 0;
    player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
    return true;
}
void func_80846720(PlayState*, Player*, s32) { normalSwordEquipCalls++; }
void Player_AnimPlayOnce(PlayState*, Player*, LinkAnimationHeader*) {}
LinkAnimationHeader* Player_GetIdleAnim(Player*) { return &idleAnimation; }
s32 LinkAnimation_OnFrame(SkelAnime* animation, f32 frame) { return animation->curFrame == frame; }
void Player_PlaySfx(Player*, u16) {}
void Player_PlayVoiceSfx(Player*, u16) {}
void Player_ProcessAnimSfxList(Player*, AnimSfxEntry*) {}
u16 gEquipMasks[4] = { 0xF, 0xF0, 0xF00, 0xF000 };
u16 gEquipNegMasks[4] = { 0xFFF0, 0xFF0F, 0xF0FF, 0x0FFF };
u8 gEquipShifts[4] = { 0, 4, 8, 12 };
static int storyWrites, infoWrites, grants, discoveries, offered, rendered;
static bool facing = true;
static u8 savedB;
static u16 playingBgm;
static bool shuffleMasterSword;
void Actor_ProcessInitChain(Actor*, void*) {}
void Collider_InitCylinder(PlayState*, ColliderCylinder*) {}
void Collider_SetCylinder(PlayState*, ColliderCylinder*, Actor*, void*) {}
void Collider_UpdateCylinder(Actor*, ColliderCylinder*) {}
void CollisionCheck_SetInfo(void*, void*, void*) {}
void Collider_DestroyCylinder(PlayState*, ColliderCylinder*) {}
void CollisionCheck_SetOC(PlayState*, void*, void*) {}
void Inventory_ChangeEquipment(s16 type, u16 value) {
    gSaveContext.equips.equipment = (gSaveContext.equips.equipment & gEquipNegMasks[type]) | (value << (type * 4));
}
s32 Flags_GetEventChkInf(s32 flag) { return gSaveContext.eventChkInf[flag >> 4] & (1 << (flag & 15)); }
s32 Flags_GetInfTable(s32 flag) { return gSaveContext.infTable[flag >> 4] & (1 << (flag & 15)); }
void Flags_SetEventChkInf(s32 flag) { storyWrites++; gSaveContext.eventChkInf[flag >> 4] |= 1 << (flag & 15); }
void Flags_SetInfTable(s32 flag) { infoWrites++; gSaveContext.infTable[flag >> 4] |= 1 << (flag & 15); }
void Flags_UnsetInfTable(s32 flag) { infoWrites++; gSaveContext.infTable[flag >> 4] &= ~(1 << (flag & 15)); }
s32 Actor_IsFacingAndNearPlayer(Actor*, f32, s16) { return facing; }
s32 Actor_IsFacingPlayer(Actor*, s16) { return facing; }
s32 Play_InCsMode(PlayState* play) { return play->csCtx.state != CS_STATE_IDLE; }
s32 Actor_HasParent(Actor* actor, PlayState*) { return actor->parent != nullptr; }
s32 Player_GetExplosiveHeld(Player*) { return -1; }
u8 MmForm_IsZoraSwimming(Player*) { return false; }
bool GameInteractor_Should(int flag, bool value, ...) {
    if (flag == VB_EXECUTE_PLAYER_STARTMODE_FUNC) startModeHookCalls++;
    if (flag == VB_PLAYER_OVERRIDE_LIMB_DRAW && replaceHandHook) {
        va_list args;
        va_start(args, value);
        int limb = va_arg(args, int);
        auto dl = va_arg(args, Gfx**);
        if (limb == PLAYER_LIMB_L_HAND) *dl = &emptyHand;
        va_end(args);
    }
    return value;
}
void Item_Give(PlayState* play, s16 item) { Fixture_GiveSword(play, item); }
u8 Return_Item(u8, int, u8 result) { grants++; return result; }
void GameInteractor_ExecuteOnEquipmentDelete(s16, u16) {}
void TradeItems_SyncWrite(void) {}
void TradeItems_SyncRead(void) {}
void Picto_SyncRead(void) {}
void Bottle_WheelResetTracking(void) {}
void Entrance_SetEntranceDiscovered(s16, bool) { discoveries++; }
void Audio_QueueSeqCmd(u32 command) { playingBgm = command & 0xFFFF; }
void Audio_PlaySceneSequence(u16 sequence) { playingBgm = sequence; }
void Audio_PlayNatureAmbienceSequence(u8) {}
void Audio_SetEnvReverb(s8) {}
s32 Environment_IsForcedSequenceDisabled(void) { return 0; }
void Audio_PlayActorSound2(Actor*, u16) {}
s32 Flags_GetEnv(PlayState*, s16) { return 0; }
s32 Randomizer_GetSettingValue(s32) { return shuffleMasterSword; }
void Gfx_SetupDL_25Opa(void*) { rendered++; }
void func_8002EBCC(Actor*, PlayState*, s32) {}
void Math_Vec3f_Copy(Vec3f* dst, Vec3f* src) { *dst = *src; }
f32 Math_SinS(s16 yaw) { return sinf(yaw * (3.14159265358979323846f / 32768.0f)); }
f32 Math_CosS(s16 yaw) { return cosf(yaw * (3.14159265358979323846f / 32768.0f)); }
void Player_AnimPlayOnceAdjusted(PlayState*, Player*, LinkAnimationHeader*) {}
void Player_StartAnimMovement(PlayState*, Player*, s32) {}
void Player_InitItemAction(PlayState*, Player* player, s8 action) { player->itemAction = player->heldItemAction = action; }
s32 Player_ActionToModelGroup(Player*, s32 action) {
    REQUIRE(action == PLAYER_IA_SWORD_CS || action == PLAYER_IA_NONE);
    return action == PLAYER_IA_SWORD_CS ? PLAYER_MODELGROUP_SWORD : PLAYER_MODELGROUP_DEFAULT;
}
void Player_SetEquipmentData(PlayState*, Player*) {}
s32 Player_SetCsAction(PlayState* play, Actor*, u8 action) { play->player.csAction = action; return 1; }
void Audio_SetCutsceneFlag(s32 flag) { cutsceneAudioFlag = flag; }
void func_80068DC0(PlayState*, CutsceneContext* csCtx) { csCtx->state = CS_STATE_IDLE; }
void func_80083108(PlayState* play) { Fixture_HudRestore(play); }
void Play_SaveSceneFlags(PlayState*) {}
void Save_SaveFile(void) { savedB = gSaveContext.equips.buttonItems[0]; }
void Interface_LoadItemIcon1(PlayState*, u16) {}
void Interface_ChangeHudVisibilityMode(s16) {}
}
static GameInteractor interactor;
GameInteractor* GameInteractor::Instance = &interactor;
static SaveManager metadataStore;
SaveManager* SaveManager::Instance = &metadataStore;

static void Reset(PlayState& play, BgTokiSwd& sword, bool adult = false, bool rando = false, s16 params = 0x4C57) {
    play = {};
    sword = {};
    gSaveContext = {};
    interactor.hooks.clear();
    gExtEquipState = {};
    neiSave = {};
    metadataStore.data.clear();
    shuffleMasterSword = true;
    skipStorySetting = 0;
    pakActive = altActive = replaceHandHook = invisibleLink = false;
    customMasterAsset = false;
    altAssetsSetting = true;
    animationComplete = false;
    normalSwordEquipCalls = startModeHookCalls = 0;
    cutsceneAudioFlag = 0;
    postSwordDrawCalls = 0;
    cameraCreates = cameraCopies = cameraClears = letterboxSize = 0;
    failSubCamera = false;
    fixturePakEquipment.clear();
    gPlayState = &play;
    play.state.running = true;
    fixtureRando = rando;
    storyWrites = infoWrites = grants = discoveries = offered = rendered = 0;
    facing = true;
    gSaveContext.linkAge = play.linkAgeOnLoad = adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    gSaveContext.entranceIndex = 0x11E;
    gSaveContext.seqId = play.sequenceCtx.seqId = playingBgm = 0x3E;
    gSaveContext.natureAmbienceId = play.sequenceCtx.natureAmbienceId = NATURE_ID_NONE;
    gSaveContext.inventory.equipment = 0x1100;
    memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    memset(gSaveContext.equips.buttonItems, ITEM_NONE, sizeof(gSaveContext.equips.buttonItems));
    memset(gSaveContext.equips.cButtonSlots, SLOT_NONE, sizeof(gSaveContext.equips.cButtonSlots));
    gSaveContext.equips.equipment = 0x1100;
    gSaveContext.childEquips = gSaveContext.adultEquips = gSaveContext.equips;
    gSaveContext.childEquips.equipment = gSaveContext.adultEquips.equipment = 0;
    play.sceneNum = SCENE_LOST_WOODS;
    play.roomCtx.curRoom.num = 10;
    play.player.actor.world.pos = { -720.0f, -61.0f, -2368.0f };
    play.player.actor.shape.rot.y = 22000;
    play.player.currentSwordItemId = ITEM_SWORD_KOKIRI;
    play.player.leftHandType = PLAYER_MODELTYPE_LH_OPEN;
    play.player.leftHandDLists = &gPlayerLeftHandClosedDLs[gSaveContext.linkAge];
    sword.actor.id = ACTOR_BG_TOKI_SWD;
    sword.actor.params = params;
    sword.actor.room = 10;
    sword.actor.world.pos = { -736.0f, -63.0f, -2395.0f };
    sword.actor.shape.rot.y = 0x4000; // Quarter-turn makes expected positions hand-checkable.
    sword.actor.draw = BgTokiSwd_Draw;
    sword.actor.xzDistToPlayer = 31.0f;
    sword.actor.yDistToPlayer = 2.0f;
    sword.actor.yawTowardsPlayer = play.player.actor.shape.rot.y + 0x8000;
    play.player.getItemDirection = 0x6000;
}

static void CheckProgressionUnchanged(const SaveContext& before) {
    REQUIRE(memcmp(&before.inventory, &gSaveContext.inventory, sizeof(Inventory)) == 0);
    REQUIRE(memcmp(before.eventChkInf, gSaveContext.eventChkInf, sizeof(before.eventChkInf)) == 0);
    REQUIRE(memcmp(before.infTable, gSaveContext.infTable, sizeof(before.infTable)) == 0);
    REQUIRE(memcmp(before.itemGetInf, gSaveContext.itemGetInf, sizeof(before.itemGetInf)) == 0);
    REQUIRE(storyWrites == 0 && infoWrites == 0 && grants == 0 && discoveries == 0);
}

static void CheckLocalScript(CutsceneData* script, bool adult) {
    REQUIRE(script != nullptr && script != D_808BB2F0 && script != D_808BB7A0);
    size_t word = 2;
    int eyes = 0, ats = 0, cues = 0, fades = 0;
    for (int entry = 0; entry < script[0].i; ++entry) {
        int type = script[word++].i;
        REQUIRE(word < 512);
        if (type == CS_CMD_CAM_EYE || type == CS_CMD_CAM_AT) {
            word += 2;
            if (type == CS_CMD_CAM_EYE && eyes == 0) {
                REQUIRE(script[word + 2].s[0] == (adult ? -846 : -775));
                REQUIRE(script[word + 2].s[1] == (adult ? -30 : -59));
                REQUIRE(script[word + 3].s[0] == (adult ? -2395 : -2354));
                REQUIRE(script[word + 1].f == 60.0f);
            }
            while (script[word].b[0] != CS_CMD_STOP) {
                word += 4;
                REQUIRE(word < 512);
            }
            word += 4;
            if (type == CS_CMD_CAM_EYE) eyes++; else ats++;
        } else if (type == CS_CMD_SET_PLAYER_ACTION) {
            REQUIRE(script[word++].i == 1);
            REQUIRE(script[word].s[0] == 0xC); // Native sword cue, not a substitute action.
            REQUIRE(script[word + 2].s[0] == 0x4000);
            REQUIRE(script[word + 3].i == (adult ? -746 : -684));
            REQUIRE(script[word + 4].i == (adult ? -103 : -77));
            REQUIRE(script[word + 5].i == -2396);
            word += 12;
            cues++;
        } else {
            // No destination, misc, or environment command can reach the engine.
            REQUIRE(type == CS_CMD_SCENE_TRANS_FX);
            word += 3;
            fades++;
        }
    }
    REQUIRE(eyes == 2 && ats == 2 && cues == 1 && fades == 1);
    REQUIRE(script[word].i == -1);
}

static void RunInteraction(bool adult, bool rando, bool skip = false, s16 entrance = 0x11E) {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword, adult, rando);
    gSaveContext.entranceIndex = entrance;
    // Distinct permanent state catches accidental zeroing as well as grants.
    gSaveContext.eventChkInf[1] = 0x2492;
    gSaveContext.infTable[29] = 0x1234;
    gSaveContext.itemGetInf[2] = 0xA581;
    gSaveContext.inventory.questItems = 0x204001;
    auto before = gSaveContext;
    const auto spawn = play.player.actor.world.pos;
    const auto yaw = play.player.actor.shape.rot.y;
    BgTokiSwd_Init(&sword.actor, &play);
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.player.interactRangeActor == &sword.actor); // Usable without Prelude or an owned sword.
    CheckProgressionUnchanged(before);
    if (!adult) {
        BgTokiSwd_Draw(&sword.actor, &play);
        REQUIRE(rendered == 1); // Decorative sword remains visible in shuffled-sword saves.
    }
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    CheckLocalScript(static_cast<CutsceneData*>(play.csCtx.segment), adult);
    REQUIRE(gSaveContext.cutsceneTrigger == 1);
    CheckProgressionUnchanged(before);

    LinkAnimationHeader animation{};
    PlayerAgeProperties age{ &animation };
    play.player.ageProperties = &age;
    func_808519EC(&play, &play.player, nullptr);
    REQUIRE(fabsf(play.player.actor.world.pos.x + 716) < 0.01f);
    REQUIRE(fabsf(play.player.actor.world.pos.y + 61) < 0.01f);
    REQUIRE(fabsf(play.player.actor.world.pos.z + 2395) < 0.01f);
    REQUIRE(play.player.actor.shape.rot.y == -0x4000);
    if (adult) {
        REQUIRE(play.player.heldItemAction == PLAYER_IA_SWORD_CS);
        REQUIRE(play.player.nextModelGroup == PLAYER_MODELGROUP_SWORD);
        CheckProgressionUnchanged(before);
    }
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    play.csCtx.frames = 100;
    sword.actor.parent = &play.player.actor; // Native frame70/87 sword handoff.
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE((sword.actor.draw != nullptr) == adult);
    play.csCtx.frames = adult ? 209 : 229;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.transitionTrigger == TRANS_TRIGGER_OFF);
    if (skip) play.csCtx.state = CS_STATE_UNSKIPPABLE_INIT;
    else play.csCtx.frames++;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.transitionTrigger == TRANS_TRIGGER_START);
    REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
    REQUIRE(play.nextEntranceIndex == entrance && play.sceneNum == SCENE_LOST_WOODS);
    REQUIRE(gSaveContext.respawnFlag == 1 && gSaveContext.respawn[0].roomIndex == 10);
    REQUIRE(memcmp(&spawn, &gSaveContext.respawn[0].pos, sizeof(spawn)) == 0);
    REQUIRE(gSaveContext.respawn[0].yaw == yaw);
    REQUIRE(gSaveContext.cutsceneIndex == 0 && gSaveContext.cutsceneTrigger == 0);
    REQUIRE(playingBgm == NA_BGM_MASTER_SWORD);
    // Both Lost Woods entrance policies must restart normal music on local reload.
    static const bool continueBgm[] = {
#define DEFINE_ENTRANCE(name, scene, spawn, continues, title, fadeOut, fadeIn) continues,
#include "tables/entrance_table.h"
#undef DEFINE_ENTRANCE
    };
    if (!continueBgm[entrance]) {
        gSaveContext.seqId = (u8)NA_BGM_DISABLED;
        gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
    }
    Environment_PlaySceneSequence(&play);
    REQUIRE(playingBgm == play.sequenceCtx.seqId);
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
    // TRANS_MODE_INSTANT clears the trigger before the final Actor_UpdateAll.
    play.state.running = false;
    play.transitionTrigger = TRANS_TRIGGER_OFF;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
    Fixture_PlayDestroyAgeHandoff(&play); // Actual teardown block, including extended equipment validation.
    REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
    REQUIRE((gSaveContext.equips.equipment & 0xF) == 0);
    CheckProgressionUnchanged(before);
    play.playerRemoved = true; // Player category is destroyed before the sword's PROP category.
    BgTokiSwd_Destroy(&sword.actor, &play);
    REQUIRE(sword.localCutscene == nullptr && play.csCtx.segment == nullptr);
    REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
}

static void CheckResetDuringCutscene() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    auto before = gSaveContext;
    BgTokiSwd_Init(&sword.actor, &play);
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    play.csCtx.frames = 100;
    // Soft reset tears down the game without setting transitionTrigger.
    play.state.running = false;
    BgTokiSwd_Update(&sword.actor, &play);
    Fixture_PlayDestroyAgeHandoff(&play);
    play.playerRemoved = true;
    BgTokiSwd_Destroy(&sword.actor, &play);
    REQUIRE(play.linkAgeOnLoad == LINK_AGE_CHILD && play.transitionTrigger == TRANS_TRIGGER_OFF);
    REQUIRE(sword.localCutscene == nullptr && play.csCtx.segment == nullptr && play.csCtx.linkAction == nullptr);
    REQUIRE(gSaveContext.cutsceneTrigger == 0 && gSaveContext.cutsceneIndex == 0);
    REQUIRE(interactor.hooks.empty());
    CheckProgressionUnchanged(before);
}

static void CheckExtendedHandoff() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    neiSave.extEquipSword = gExtEquipState.currentExtSword = 2; // Child-only Four Sword.
    neiSave.extEquipOwnedBits = 0x20000;
    gSaveContext.equips.buttonItems[0] = ITEM_EXT_SWORD_2;
    const auto before = gSaveContext;
    SwitchAgeWithoutProgression();
    Fixture_PlayDestroyAgeHandoff(&play);
    REQUIRE(neiSave.extEquipSword == 0 && gExtEquipState.currentExtSword == 0);
    REQUIRE(neiSave.extEquipOwnedBits == 0x20000);
    REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
    CheckProgressionUnchanged(before);

    Reset(play, sword, true);
    neiSave.extEquipSword = gExtEquipState.currentExtSword = 1; // Byrna is available to both ages.
    neiSave.extEquipOwnedBits = 0x10000;
    gSaveContext.equips.buttonItems[0] = ITEM_EXT_SWORD_1;
    auto ownedBefore = gSaveContext;
    SwitchAgeWithoutProgression();
    Fixture_PlayDestroyAgeHandoff(&play);
    REQUIRE(neiSave.extEquipSword == 1 && gExtEquipState.currentExtSword == 1);
    REQUIRE(neiSave.extEquipOwnedBits == 0x10000);
    REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_EXT_SWORD_1);
    CheckProgressionUnchanged(ownedBefore);
}

static void CheckRepeatedOwnedLoadouts() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    gSaveContext.inventory.equipment = 0x1137; // All three swords, both normal shields, base clothes.
    gSaveContext.inventory.items[SLOT_BOTTLE_1] = ITEM_POTION_RED;
    gSaveContext.equips.equipment = 0x1111;
    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
    gSaveContext.equips.buttonItems[1] = ITEM_BOTTLE;
    gSaveContext.equips.cButtonSlots[0] = SLOT_BOTTLE_1;
    gSaveContext.adultEquips = gSaveContext.equips;
    gSaveContext.adultEquips.equipment = 0x1123;
    gSaveContext.adultEquips.buttonItems[0] = ITEM_SWORD_BGS;
    const auto before = gSaveContext;
    for (int cycle = 0; cycle < 6; ++cycle) {
        const bool adult = gSaveContext.linkAge == LINK_AGE_ADULT;
        BgTokiSwd_Init(&sword.actor, &play);
        sword.actor.parent = &play.player.actor;
        BgTokiSwd_Update(&sword.actor, &play);
        play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
        play.csCtx.frames = adult ? 210 : 230;
        BgTokiSwd_Update(&sword.actor, &play);
        Fixture_PlayDestroyAgeHandoff(&play);
        REQUIRE(gSaveContext.equips.buttonItems[0] == (adult ? ITEM_SWORD_KOKIRI : ITEM_SWORD_BGS));
        REQUIRE(gSaveContext.equips.equipment == (adult ? 0x1111 : 0x1123));
        REQUIRE(gSaveContext.equips.buttonItems[1] == ITEM_POTION_RED);
        CheckProgressionUnchanged(before);
        BgTokiSwd_Destroy(&sword.actor, &play);
        gSaveContext.linkAge = play.linkAgeOnLoad; // Player_Destroy's actual age commit.
        REQUIRE(interactor.hooks.size() == 1);
        auto suppressVoidDamage = interactor.hooks.begin()->second;
        bool inflictDamage = true;
        suppressVoidDamage(&inflictDamage);
        REQUIRE(!inflictDamage && interactor.hooks.empty());
        play.transitionTrigger = TRANS_TRIGGER_OFF;
        play.csCtx = {};
        sword.actor.draw = BgTokiSwd_Draw;
    }
}

static void CheckStockAndInactivePaths() {
    PlayState play;
    BgTokiSwd sword;
    for (s16 params : { (s16)-1, (s16)0, (s16)0x4C56, (s16)0x4C58 }) {
        Reset(play, sword, false, true, params);
        BgTokiSwd_Init(&sword.actor, &play);
        REQUIRE(play.player.currentSwordItemId == ITEM_NONE); // Existing randomizer init remains.
        BgTokiSwd_Update(&sword.actor, &play);
        REQUIRE(storyWrites == 1); // Existing chamber discovery remains for stock parameters.
        BgTokiSwd_Destroy(&sword.actor, &play);
    }
    Reset(play, sword, true, false, -1);
    BgTokiSwd_Init(&sword.actor, &play);
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(offered == 0); // Stock adult actor still requires Prelude of Light.

    Reset(play, sword);
    LinkAnimationHeader animation{};
    PlayerAgeProperties age{ &animation };
    play.player.ageProperties = &age;
    play.player.interactRangeActor = &sword.actor;
    func_808519EC(&play, &play.player, nullptr); // Exact parameter without an active local script is insufficient.
    REQUIRE(play.player.actor.world.pos.x == -1 && play.player.actor.world.pos.y == 70 &&
            play.player.actor.world.pos.z == 20 && play.player.actor.shape.rot.y == -0x8000);

    Reset(play, sword);
    SwitchAge();
    REQUIRE(discoveries == 1);
    Fixture_PlayDestroyAgeHandoff(&play);
    REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER); // Existing Time Gate handoff is unchanged.
    auto save = gSaveContext;
    gPlayState = nullptr;
    SwitchAgeWithoutProgression();
    SwitchAge();
    REQUIRE(memcmp(&save, &gSaveContext, sizeof(save)) == 0);
}

static void CheckRejectedScripts() {
    std::vector<CutsceneData> source(D_808BB2F0, D_808BB2F0 + gMasterSwordChildCutsceneWordCount);
    std::vector<CutsceneData> output(source.size());
    Vec3f origin = { -736, -63, -2395 };
    s16 frame = -1;
    REQUIRE(TimePedestalCutscene_Build(output.data(), output.size() - 1, source.data(), source.size(), &origin, 0,
                                      &frame) == 0);
    source[2].i = CS_CMD_PLAYBGM; // A new, unsupported command cannot silently acquire behavior.
    REQUIRE(TimePedestalCutscene_Build(output.data(), output.size(), source.data(), source.size(), &origin, 0,
                                      &frame) == 0);
    source.assign(D_808BB2F0, D_808BB2F0 + gMasterSwordChildCutsceneWordCount);
    REQUIRE(TimePedestalCutscene_Build(output.data(), output.size(), source.data(), 8, &origin, 0, &frame) == 0);
    origin.x = 40000;
    REQUIRE(TimePedestalCutscene_Build(output.data(), output.size(), source.data(), source.size(), &origin, 0,
                                      &frame) == 0);
    REQUIRE(memcmp(source.data(), D_808BB2F0, source.size() * sizeof(CutsceneData)) == 0);
}

static void CheckPostRespawnEmptyB() {
    for (u8 status : { (u8)BTN_ENABLED, (u8)ITEM_SWORD_MASTER }) {
        PlayState play;
        BgTokiSwd sword;
        Reset(play, sword);
        SwitchAgeWithoutProgression();
        Fixture_PlayDestroyAgeHandoff(&play);
        gSaveContext.linkAge = play.linkAgeOnLoad;
        play.transitionTrigger = TRANS_TRIGGER_OFF;
        REQUIRE(gSaveContext.infTable[29] == 0 && gSaveContext.equips.buttonItems[0] == ITEM_NONE);
        auto before = gSaveContext;
        for (u8 restriction : { 0, 1 }) {
            gSaveContext.buttonStatus[0] = status;
            play.interfaceCtx.restrictions.bButton = restriction;
            Fixture_HudRestore(&play);
            REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
        }
        gSaveContext.buttonStatus[0] = status;
        func_80084BF4(&play, 1);
        REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
        gSaveContext.buttonStatus[0] = status;
        Play_PerformSave(&play);
        REQUIRE(savedB == ITEM_NONE && gSaveContext.equips.buttonItems[0] == ITEM_NONE);
        Fixture_AdultLoadRepair();
        REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
        CheckProgressionUnchanged(before);

        // A real equipped sword still participates in the existing temp-B restore.
        gSaveContext.inventory.equipment |= 2;
        gSaveContext.equips.equipment |= 2;
        gSaveContext.buttonStatus[0] = ITEM_SWORD_MASTER;
        Fixture_HudRestore(&play);
        REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER);
        gSaveContext.equips.buttonItems[0] = ITEM_BOW;
        Play_PerformSave(&play);
        REQUIRE(savedB == ITEM_SWORD_MASTER && gSaveContext.equips.buttonItems[0] == ITEM_BOW);
    }
}

static void CheckSavedAdultWithoutSword() {
    for (bool rando : { false, true }) {
        for (bool shuffle : { false, true }) {
            PlayState play;
            BgTokiSwd sword;
            Reset(play, sword, false, rando);
            shuffleMasterSword = shuffle;
            SwitchAgeWithoutProgression();
            Fixture_PlayDestroyAgeHandoff(&play);
            gSaveContext.linkAge = play.linkAgeOnLoad;
            const auto before = gSaveContext;
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 1);
            NeiSave_Save(&gSaveContext, 0, true);
            neiSave = {};
            NeiSave_Load();
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 1);
            Fixture_AdultLoadRepair();
            CheckProgressionUnchanged(before);

            // A real award ends the opt-in; Ganon's later loss keeps stock recovery.
            Item_Give(&play, ITEM_SWORD_MASTER);
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 0);
            neiSave.timePedestalNoMasterSwordRepair = 1; // Also cover a direct/FleetSync ownership update.
            Inventory_DeleteEquipment(&play, EQUIP_TYPE_SWORD);
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 0);
            Fixture_AdultLoadRepair();
            REQUIRE(!!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) == (!rando || !shuffle));

            // Missing metadata (old saves) defaults to the unchanged native repair.
            metadataStore.data.clear();
            NeiSave_Load();
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 0);
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
            Fixture_AdultLoadRepair();
            REQUIRE(!!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) == (!rando || !shuffle));

            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
            neiSave.timePedestalNoMasterSwordRepair = 1;
            NeiSave_Save(&gSaveContext, 0, true);
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 0);
            neiSave.timePedestalNoMasterSwordRepair = 1;
            Fixture_AdultLoadRepair();
            REQUIRE(neiSave.timePedestalNoMasterSwordRepair == 0);
        }
    }
}

static void CheckRequestedSkip(bool adult, bool rando, int setting, bool pressB) {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword, adult, rando);
    skipStorySetting = setting;
    const auto before = gSaveContext;
    const auto spawn = play.player.actor.world.pos;
    const auto yaw = play.player.actor.shape.rot.y;
    BgTokiSwd_Init(&sword.actor, &play);
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    play.state.input[0].press.button = pressB ? BTN_B : 0;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.transitionTrigger == TRANS_TRIGGER_OFF); // Wait for native camera startup.
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    play.csCtx.frames = 20;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.transitionTrigger == TRANS_TRIGGER_OFF);
    play.csCtx.frames = 21;
    const Vec3f ceremonyPos = { -716.0f, -61.0f, -2395.0f };
    play.player.actor.world.pos = ceremonyPos;
    BgTokiSwd_Update(&sword.actor, &play);
    const bool shouldSkip = pressB || (setting < 0 ? rando : setting != 0);
    REQUIRE((play.transitionTrigger == TRANS_TRIGGER_START) == shouldSkip);
    if (shouldSkip) {
        REQUIRE(play.transitionType == TRANS_TYPE_FADE_WHITE_FAST);
        REQUIRE(gSaveContext.nextTransitionType == TRANS_TYPE_FADE_WHITE_FAST);
        REQUIRE(memcmp(&play.player.actor.world.pos, &ceremonyPos, sizeof(Vec3f)) == 0);
        REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
        REQUIRE(gSaveContext.respawn[0].roomIndex == 10);
        REQUIRE(memcmp(&spawn, &gSaveContext.respawn[0].pos, sizeof(spawn)) == 0);
        REQUIRE(gSaveContext.respawn[0].yaw == yaw);
        REQUIRE(play.nextEntranceIndex == before.entranceIndex);
        REQUIRE(gSaveContext.cutsceneTrigger == 0 && gSaveContext.cutsceneIndex == 0);
        BgTokiSwd_Update(&sword.actor, &play); // A second update must not reverse the swap.
        REQUIRE(play.linkAgeOnLoad == (adult ? LINK_AGE_CHILD : LINK_AGE_ADULT));
    }
    CheckProgressionUnchanged(before);
    REQUIRE(memcmp(&before.equips, &gSaveContext.equips, sizeof(ItemEquips)) == 0);
    BgTokiSwd_Destroy(&sword.actor, &play);
}

static void CheckChildSwordRendering() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    const auto before = gSaveContext;
    BgTokiSwd_Init(&sword.actor, &play);
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    replaceHandHook = true;
    Gfx* dl = &emptyHand;
    play.player.skelAnime.curFrame = 86;
    func_80851A50(&play, &play.player, nullptr);
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand); // Sword is still in the pedestal before the real handoff.
    play.player.skelAnime.curFrame = 87;
    func_80851A50(&play, &play.player, nullptr);
    REQUIRE(sword.actor.parent == &play.player.actor);
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(sword.actor.draw == nullptr);
    REQUIRE(play.player.leftHandDLists == &gPlayerLeftHandBgsDLs[LINK_AGE_CHILD]);
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &nativeSword); // Late empty-hand hooks must not erase the ceremony sword.
    const Vec3s originalSwordRot = { 0x1234, 0x2345, 0x3456 };
    Vec3s swordRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(swordRot.x == originalSwordRot.x && swordRot.y == originalSwordRot.y &&
            swordRot.z == originalSwordRot.z); // Canonical ceremonial DL already matches the native animation.
    Vec3s armRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_FOREARM, &dl, &armRot);
    REQUIRE(armRot.x == originalSwordRot.x && armRot.y == originalSwordRot.y && armRot.z == originalSwordRot.z);
    altActive = true;
    swordRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(dl == &altSword);
    REQUIRE(swordRot.x == originalSwordRot.x && swordRot.y == originalSwordRot.y &&
            swordRot.z == originalSwordRot.z); // Same ceremonial resource contract.
    altActive = false;
    customMasterAsset = true; // Only the custom-equipment path is replaced, not the legacy child DL.
    swordRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(dl == &customHandSword);
    RequireOppositeBladeDirection(originalSwordRot, swordRot);
    altAssetsSetting = false;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &nativeSword);
    altAssetsSetting = true;
    pakActive = true;
    fixturePakEquipment = {{0x5098, &emptyHand}, {0x5448, &wrongSword}, {0x5458, &wrongSword}, {0x5450, &pakSword}};
    // B is empty: the selected Master Sword slot must not depend on ownership or equipped blade.
    REQUIRE(gSaveContext.equips.buttonItems[0] == ITEM_NONE);
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &pakHandSword);
    for (u8 item : {ITEM_SWORD_KOKIRI, ITEM_SWORD_BGS, ITEM_SWORD_MASTER}) {
        gSaveContext.equips.buttonItems[0] = item;
        Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
        REQUIRE(dl == &pakHandSword && gSaveContext.equips.buttonItems[0] == item);
    }
    swordRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(dl == &pakHandSword);
    RequireOppositeBladeDirection(originalSwordRot, swordRot);
    gSaveContext.equips.buttonItems[0] = before.equips.buttonItems[0];
    customMasterAsset = false;
    altActive = true;
    fixturePakEquipment.erase(0x5450);
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &altSword); // Unrelated custom hands/Kokiri/BGS are not a Master Sword fallback.
    fixturePakEquipment[0x5450] = PAK_DL_STUB;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &altSword && !PakLoader_UsedCombinedDL(true));
    invisibleLink = true;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == nullptr);
    invisibleLink = false;
    dl = &wrongSword;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_R_HAND, &dl);
    REQUIRE(dl == &wrongSword);
    Player otherPlayer = play.player;
    Fixture_ApplyLateHandOverrides(&play, &otherPlayer, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand); // Remote/dummy players must not inherit the local ceremony.
    sword.actor.params = 0;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand); // Stock pedestal remains outside this scoped fix.
    sword.actor.params = BG_TOKI_SWD_TIME_PEDESTAL;
    void* script = play.csCtx.segment;
    play.csCtx.segment = nullptr;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand);
    play.csCtx.segment = script;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand);
    gSaveContext.linkAge = LINK_AGE_CHILD;
    sword.localCutsceneFinished = true;
    swordRot = originalSwordRot;
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(dl == &emptyHand);
    REQUIRE(swordRot.x == originalSwordRot.x && swordRot.y == originalSwordRot.y &&
            swordRot.z == originalSwordRot.z);
    CheckProgressionUnchanged(before);
    REQUIRE(memcmp(&before.equips, &gSaveContext.equips, sizeof(ItemEquips)) == 0);
    BgTokiSwd_Destroy(&sword.actor, &play);
}

static void CheckAdultSwordRendering() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword, true);
    const auto before = gSaveContext;
    BgTokiSwd_Init(&sword.actor, &play);
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    LinkAnimationHeader animation{};
    PlayerAgeProperties age{&animation, nullptr};
    play.player.ageProperties = &age;
    func_808519EC(&play, &play.player, nullptr);
    REQUIRE(play.player.heldItemAction == PLAYER_IA_SWORD_CS);
    // Player_InitItemAction's engine boundary supplies the adult sword hand.
    Gfx* adultSwordHands[] = {&nativeSword, &nativeSword};
    play.player.leftHandDLists = adultSwordHands;
    play.player.leftHandType = PLAYER_MODELTYPE_LH_SWORD;
    replaceHandHook = true;
    Gfx* dl = &emptyHand;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &nativeSword);
    Vec3s swordRot = { 0x1234, 0x2345, 0x3456 };
    Fixture_ApplyLateHandOverridesWithRot(&play, &play.player, PLAYER_LIMB_L_HAND, &dl, &swordRot);
    REQUIRE(swordRot.x == 0x1234 && swordRot.y == 0x2345 && swordRot.z == 0x3456);
    customMasterAsset = true;
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &customHandSword);
    pakActive = true;
    fixturePakEquipment = {{0x50A0, &emptyHand}, {0x5448, &wrongSword},
                          {0x5458, &wrongSword}, {0x5450, &pakSword}};
    for (u8 item : {ITEM_NONE, ITEM_SWORD_BGS, ITEM_SWORD_MASTER}) {
        gSaveContext.equips.buttonItems[0] = item;
        Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
        REQUIRE(dl == &pakHandSword && gSaveContext.equips.buttonItems[0] == item);
    }
    play.player.skelAnime.curFrame = 70;
    // The post-limb renderer must not layer Odolwa's sword over this weapon.
    Fixture_DrawPostHand(&play, &play.player, &dl);
    REQUIRE(postSwordDrawCalls == 0);
    func_80851A50(&play, &play.player, nullptr);
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(sword.actor.draw != nullptr);
    REQUIRE(play.player.leftHandDLists == &gPlayerLeftHandClosedDLs[LINK_AGE_ADULT]);
    // The native insertion cue must also beat late PAK/custom sword overrides.
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand && !PakLoader_UsedCombinedDL(true));
    Fixture_DrawPostHand(&play, &play.player, &dl);
    REQUIRE(postSwordDrawCalls == 0); // Nor redraw a sword after the insertion.
    fixturePakEquipment.erase(0x50A0);
    Fixture_ApplyLateHandOverrides(&play, &play.player, PLAYER_LIMB_L_HAND, &dl);
    REQUIRE(dl == &emptyHand);
    gSaveContext.equips.buttonItems[0] = before.equips.buttonItems[0];
    CheckProgressionUnchanged(before);
    REQUIRE(memcmp(&before.equips, &gSaveContext.equips, sizeof(ItemEquips)) == 0);
    BgTokiSwd_Destroy(&sword.actor, &play);
    play.player.interactRangeActor = nullptr;
    Fixture_DrawPostHand(&play, &play.player, &dl);
    REQUIRE(postSwordDrawCalls == 1); // Ordinary Odolwa equipment still draws.
}

static void CheckNativeTimeTravelHold() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    PlayerAgeProperties age{ nullptr, &childArrivalAnimation };
    play.player.ageProperties = &age;

    Player_StartMode_TimeTravel(&play, &play.player);

    REQUIRE(!BgTokiSwd_IsTimePedestalArrival(&play, &play.player));
    REQUIRE(play.player.av1.actionVar1 == 0);
    REQUIRE(play.player.av2.actionVar2 == 20);
    REQUIRE(play.player.skelAnime.endFrame == 0.0f);
}

enum class ArrivalCameraCase { Normal, NoSlot, OtherCamera, ReusedSlot, SceneTeardown, ImmediateSkip };

static void CheckArrival(bool sourceAdult, bool rando, bool skipMain, bool skipExit, bool storySkipEnabled = false,
                         bool mismatchedScene = false, ArrivalCameraCase cameraCase = ArrivalCameraCase::Normal) {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword, sourceAdult, rando);
    if (storySkipEnabled) {
        skipStorySetting = 1;
    }
    const auto before = gSaveContext;
    const auto returnPos = play.player.actor.world.pos;
    const auto returnYaw = play.player.actor.shape.rot.y;
    BgTokiSwd_Init(&sword.actor, &play);
    sword.actor.parent = &play.player.actor;
    BgTokiSwd_Update(&sword.actor, &play);
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    play.csCtx.frames = skipMain ? 21 : sword.ageSwapFrame;
    play.state.input[0].press.button = skipMain ? BTN_B : 0;
    play.state.input[0].cur.button = skipMain ? BTN_B : 0;
    BgTokiSwd_Update(&sword.actor, &play);
    REQUIRE(play.transitionTrigger == TRANS_TRIGGER_START);
    // The departure image must fade out before the synchronous age reload.
    REQUIRE(play.csCtx.state == CS_STATE_UNSKIPPABLE_EXEC && play.transitionType == TRANS_TYPE_FADE_WHITE_FAST);
    REQUIRE(gSaveContext.nextTransitionType == TRANS_TYPE_FADE_WHITE_FAST);
    Fixture_PlayDestroyAgeHandoff(&play);
    gSaveContext.linkAge = play.linkAgeOnLoad;
    cutsceneAudioFlag = 1; // Cutscene engine owns this while the departure camera is live.
    play.state.running = false;
    play.playerRemoved = true;
    BgTokiSwd_Destroy(&sword.actor, &play);
    REQUIRE(sword.localCutscene == nullptr);
    REQUIRE(play.csCtx.state == CS_STATE_IDLE && play.csCtx.segment == nullptr && play.csCtx.linkAction == nullptr);
    REQUIRE(gSaveContext.cutsceneTrigger == 0 && gSaveContext.cutsceneIndex == 0);
    REQUIRE(cutsceneAudioFlag == 0);

    PlayState arrival{};
    gPlayState = &arrival;
    arrival.state.running = true;
    arrival.linkAgeOnLoad = gSaveContext.linkAge;
    arrival.sceneNum = SCENE_LOST_WOODS;
    arrival.roomCtx.curRoom.num = 10;
    arrival.player.actor.world.pos = gSaveContext.respawn[0].pos;
    arrival.player.actor.shape.rot.y = gSaveContext.respawn[0].yaw;
    arrival.player.leftHandType = PLAYER_MODELTYPE_LH_OPEN;
    PlayerAgeProperties age{nullptr, LINK_IS_ADULT ? &adultArrivalAnimation : &childArrivalAnimation};
    arrival.player.ageProperties = &age;
    if (mismatchedScene) arrival.sceneNum++;
    // The input manager keeps held state, but consumes press edges every frame.
    arrival.state.input[0] = play.state.input[0];
    arrival.state.input[0].press.button = 0;
    Fixture_PlayerStartMode(&arrival, PLAYER_START_MODE_IDLE);
    REQUIRE(cameraCreates == 0); // Camera setup must wait until Play_Init finishes.
    if (mismatchedScene) {
        REQUIRE(arrival.player.actionFunc == Player_Action_Idle && startModeHookCalls == 1);
        arrival.sceneNum = SCENE_LOST_WOODS;
        Fixture_PlayerStartMode(&arrival, PLAYER_START_MODE_IDLE);
        REQUIRE(arrival.player.actionFunc == Player_Action_Idle && startModeHookCalls == 2);
        CheckProgressionUnchanged(before);
        return; // A reset or different entrance consumes the continuation instead of leaving it armed.
    }
    REQUIRE(arrival.player.actionFunc == Player_Action_8084E9AC);
    REQUIRE(startModeHookCalls == 0); // Never hit the native rando hook that relocates to Temple of Time.
    REQUIRE(normalSwordEquipCalls == 0);
    REQUIRE(arrival.player.skelAnime.animation == age.unk_A0);
    // A local age swap has no Temple of Time cutscene to synchronize with.
    // It must begin the native arrival motion immediately rather than holding
    // its first pose for the native 20 completed updates.
    REQUIRE(arrival.player.av1.actionVar1 == 1);
    REQUIRE(arrival.player.av2.actionVar2 == 0);
    REQUIRE(arrival.player.skelAnime.endFrame == arrival.player.skelAnime.animLength - 1.0f);
    REQUIRE(fabsf(arrival.player.actor.world.pos.x + 716) < 0.01f);
    REQUIRE(fabsf(arrival.player.actor.world.pos.y + 62) < 0.01f);
    REQUIRE(fabsf(arrival.player.actor.world.pos.z + 2395) < 0.01f);
    REQUIRE(arrival.player.actor.shape.rot.y == -0x4000);
    REQUIRE(arrival.player.stateFlags1 & PLAYER_STATE1_IN_CUTSCENE);
    Player otherPlayer = arrival.player;
    BgTokiSwd_UpdateTimePedestalArrivalCamera(&arrival, &otherPlayer);
    REQUIRE(cameraCreates == 0); // Never acquire a camera for a remote/dummy player.
    if (LINK_IS_ADULT) {
        REQUIRE(arrival.player.heldItemAction == PLAYER_IA_SWORD_CS);
        pakActive = true;
        fixturePakEquipment = {{0x5098, &emptyHand}, {0x5450, &pakSword}};
        Gfx* dl = &emptyHand;
        Fixture_ApplyLateHandOverrides(&arrival, &arrival.player, PLAYER_LIMB_L_HAND, &dl);
        REQUIRE(dl == &pakHandSword);
    }
    // A held departure B has no press edge after loading. Neither that held
    // state nor the persistent story-skip setting may cancel the exit action.
    if (cameraCase == ArrivalCameraCase::ImmediateSkip) {
        arrival.state.input[0].press.button = BTN_B;
        Player_Action_8084E9AC(&arrival.player, &arrival);
        REQUIRE(arrival.player.actionFunc == Player_Action_Idle && cameraCreates == 0);
        REQUIRE(!BgTokiSwd_IsTimePedestalArrival(&arrival, &arrival.player));
        REQUIRE(memcmp(&arrival.player.actor.world.pos, &returnPos, sizeof(returnPos)) == 0);
        CheckProgressionUnchanged(before);
        return;
    }
    failSubCamera = cameraCase == ArrivalCameraCase::NoSlot;
    if (cameraCase == ArrivalCameraCase::OtherCamera) arrival.activeCamera = 2;
    Player_Action_8084E9AC(&arrival.player, &arrival);
    REQUIRE(arrival.player.actionFunc == Player_Action_8084E9AC && !animationComplete);
    if (failSubCamera || cameraCase == ArrivalCameraCase::OtherCamera) {
        Player_Action_8084E9AC(&arrival.player, &arrival);
        REQUIRE(cameraCreates == (failSubCamera ? 1 : 0)); // No repeated allocation or camera takeover.
        REQUIRE(!arrival.subCameraAllocated && cameraCopies == 0 && cameraClears == 0);
        arrival.state.input[0].press.button = BTN_B;
        Player_Action_8084E9AC(&arrival.player, &arrival);
        REQUIRE(arrival.player.actionFunc == Player_Action_Idle);
        REQUIRE(arrival.activeCamera == (failSubCamera ? CAM_ID_MAIN : 2));
        REQUIRE(memcmp(&arrival.player.actor.world.pos, &returnPos, sizeof(returnPos)) == 0);
        CheckProgressionUnchanged(before);
        return;
    }
    REQUIRE(cameraCreates == 1 && arrival.activeCamera == 1 && arrival.subCameraAllocated);
    REQUIRE(arrival.camera.status == CAM_STAT_WAIT && arrival.subCamera.status == CAM_STAT_ACTIVE);
    REQUIRE(arrival.subCamera.setting == CAM_SET_FREE0 && letterboxSize == 0x20);
    const Camera exitCamera = arrival.subCamera;
    // Both points must live near this transformed pedestal, not Temple of Time's origin.
    REQUIRE(fabsf(exitCamera.at.x + 736) < 250 && fabsf(exitCamera.at.z + 2395) < 250);
    REQUIRE(fabsf(exitCamera.eye.x + 736) < 250 && fabsf(exitCamera.eye.z + 2395) < 250);
    arrival.player.actor.world.pos.x += 12.0f;
    arrival.state.input[0] = {};
    Player_Action_8084E9AC(&arrival.player, &arrival);
    REQUIRE(arrival.player.actionFunc == Player_Action_8084E9AC);
    REQUIRE(cameraCreates == 1 && memcmp(&arrival.subCamera, &exitCamera, sizeof(Camera)) == 0);
    if (cameraCase == ArrivalCameraCase::ReusedSlot || cameraCase == ArrivalCameraCase::SceneTeardown) {
        if (cameraCase == ArrivalCameraCase::ReusedSlot) {
            arrival.subCamera.uid++; // Another camera has reused this slot.
        } else {
            arrival.state.running = false; // Player_Destroy calls this same cleanup.
        }
        REQUIRE(BgTokiSwd_EndTimePedestalArrival(&arrival, &arrival.player));
        REQUIRE(!BgTokiSwd_IsTimePedestalArrival(&arrival, &arrival.player));
        REQUIRE(cameraCopies == 0);
        REQUIRE(cameraClears == (cameraCase == ArrivalCameraCase::SceneTeardown ? 1 : 0));
        REQUIRE(arrival.activeCamera == (cameraCase == ArrivalCameraCase::SceneTeardown ? CAM_ID_MAIN : 1));
        REQUIRE(!BgTokiSwd_EndTimePedestalArrival(&arrival, &arrival.player)); // Cleanup is one-use.
        CheckProgressionUnchanged(before);
        return;
    }
    if (skipExit) {
        // Release/repress produces the fresh edge that skips this phase, even
        // while Skip Story Cutscenes remains enabled.
        arrival.state.input[0].press.button = arrival.state.input[0].cur.button = BTN_B;
        Player_Action_8084E9AC(&arrival.player, &arrival);
    } else {
        animationComplete = true;
        Player_Action_8084E9AC(&arrival.player, &arrival);
    }
    REQUIRE(arrival.player.actionFunc == Player_Action_Idle);
    REQUIRE(!(arrival.player.stateFlags1 & PLAYER_STATE1_IN_CUTSCENE));
    REQUIRE(arrival.player.heldItemAction == PLAYER_IA_NONE);
    REQUIRE(!BgTokiSwd_IsTimePedestalArrival(&arrival, &arrival.player));
    REQUIRE(arrival.activeCamera == CAM_ID_MAIN && !arrival.subCameraAllocated);
    REQUIRE(cameraCopies == 1 && cameraClears == 1 && letterboxSize == 0);
    REQUIRE(memcmp(&arrival.camera.at, &exitCamera.at, sizeof(Vec3f)) == 0);
    REQUIRE(memcmp(&arrival.camera.eye, &exitCamera.eye, sizeof(Vec3f)) == 0);
    REQUIRE(arrival.csCtx.state == CS_STATE_IDLE);
    REQUIRE(memcmp(&arrival.player.actor.world.pos, &returnPos, sizeof(returnPos)) == 0);
    // Position restoration keeps the actor on the known safe floor point, but
    // completion must not twist the whole character back to the pre-swap yaw.
    REQUIRE(returnYaw != -0x4000);
    REQUIRE(arrival.player.actor.shape.rot.y == -0x4000 && arrival.player.yaw == -0x4000);
    CheckProgressionUnchanged(before);
    REQUIRE(memcmp(&before.equips, &gSaveContext.equips, sizeof(ItemEquips)) == 0);
    skipStorySetting = 0;
    Fixture_PlayerStartMode(&arrival, PLAYER_START_MODE_IDLE);
    REQUIRE(arrival.player.actionFunc == Player_Action_Idle && startModeHookCalls == 1); // One use only.
}

static void CheckPedestalProximity() {
    for (bool adult : {false, true}) {
        for (s16 npcParams : {0x7F23, 0x7E08}) {
            for (s16 yaw : {0, 0x2000, 0x4000, 0x6000}) {
                PlayState play;
                BgTokiSwd sword;
                Reset(play, sword, adult);
                BgTokiSwd_Init(&sword.actor, &play);
                facing = false; // Approach outside the stock actor's front wedge.
                sword.actor.xzDistToPlayer = 85.0f;
                sword.actor.yDistToPlayer = 25.0f;
                sword.actor.yawTowardsPlayer = yaw;
                play.player.actor.shape.rot.y = 0;
                Actor npc{};
                npc.id = ACTOR_EN_VIEWER;
                npc.params = npcParams;
                play.player.talkActor = &npc;
                play.player.stateFlags2 |= PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER;
                BgTokiSwd_Update(&sword.actor, &play);
                REQUIRE(play.player.interactRangeActor == &sword.actor);
                REQUIRE(play.player.getItemId == GI_NONE);
                REQUIRE(play.player.talkActor == nullptr);
                REQUIRE(!(play.player.stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER));
                BgTokiSwd_Destroy(&sword.actor, &play);
            }
        }
    }
    for (int blocked = 0; blocked < 5; ++blocked) {
        PlayState play;
        BgTokiSwd sword;
        Reset(play, sword);
        BgTokiSwd_Init(&sword.actor, &play);
        if (blocked == 0) sword.actor.xzDistToPlayer = 110.0f;
        if (blocked == 1) sword.actor.yDistToPlayer = 50.0f;
        if (blocked == 2) play.player.stateFlags1 |= PLAYER_STATE1_JUMPING;
        if (blocked == 3) play.player.stateFlags1 |= PLAYER_STATE1_TALKING;
        if (blocked == 4) play.player.stateFlags1 |= PLAYER_STATE1_FIRST_PERSON;
        BgTokiSwd_Update(&sword.actor, &play);
        REQUIRE(play.player.interactRangeActor == nullptr);
        REQUIRE(play.player.getItemDirection == 0x6000);
        BgTokiSwd_Destroy(&sword.actor, &play);
    }
    puts("PASS custom proximity from all sides, dialogue priority, height and player-state guards");
}

static void CheckPedestalSwordSource() {
    PlayState play;
    BgTokiSwd sword;
    Reset(play, sword);
    BgTokiSwd_Init(&sword.actor, &play);
    pakActive = true;
    fixturePakEquipment = {{0x5450, &pakSword}};
    BgTokiSwd_Draw(&sword.actor, &play);
    REQUIRE(drawnSword == &pakSword);
    // The same weapon remains authoritative after adult insertion and while
    // the skip fade is running; changing age must not select a different mesh.
    gSaveContext.linkAge = LINK_AGE_ADULT;
    BgTokiSwd_Draw(&sword.actor, &play);
    REQUIRE(drawnSword == &pakSword);
    pakActive = false;
    customMasterAsset = true;
    BgTokiSwd_Draw(&sword.actor, &play);
    REQUIRE(drawnSword == &customMasterSword);
    altAssetsSetting = false;
    BgTokiSwd_Draw(&sword.actor, &play);
    REQUIRE(drawnSword == object_toki_objects_DL_001BD0);
    pakActive = true;
    sword.actor.params = 0;
    BgTokiSwd_Draw(&sword.actor, &play);
    REQUIRE(drawnSword == object_toki_objects_DL_001BD0); // Stock actor is unaffected.
    BgTokiSwd_Destroy(&sword.actor, &play);
    puts("PASS pedestal and ceremonial hands share selected geometry, with Alt and stock fallbacks");
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "pedestal_model") == 0) {
        CheckPedestalSwordSource();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "proximity") == 0) {
        CheckPedestalProximity();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "arrival") == 0) {
        CheckArrival(false, true, true, false);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "render") == 0) {
        CheckChildSwordRendering();
        CheckAdultSwordRendering();
        puts("PASS child/adult handoffs, late overrides, alternate assets and Pak Master Sword selection");
        return 0;
    }
    for (bool adult : {false, true}) {
        for (bool rando : {false, true}) {
            for (int setting : {-1, 0, 1}) {
                CheckRequestedSkip(adult, rando, setting, false);
                CheckRequestedSkip(adult, rando, setting, true);
            }
        }
    }
    puts("PASS both ages honor story skip and B, preserving room, position, equipment and progression");
    CheckPedestalProximity();
    CheckPedestalSwordSource();
    CheckChildSwordRendering();
    CheckAdultSwordRendering();
    puts("PASS child/adult handoffs, late overrides, alternate assets and Pak Master Sword selection");
    CheckNativeTimeTravelHold();
    for (bool sourceAdult : {false, true}) {
        for (bool rando : {false, true}) {
            CheckArrival(sourceAdult, rando, false, false, false, true);
            for (bool skipMain : {false, true}) {
                for (bool storySkipEnabled : {false, true}) {
                    CheckArrival(sourceAdult, rando, skipMain, false, storySkipEnabled);
                    CheckArrival(sourceAdult, rando, skipMain, true, storySkipEnabled);
                }
            }
        }
    }
    puts("PASS native arrival animations, independent skips, selected sword and one-use local return");
    for (bool sourceAdult : {false, true}) {
        for (ArrivalCameraCase cameraCase : {ArrivalCameraCase::NoSlot, ArrivalCameraCase::OtherCamera,
                                            ArrivalCameraCase::ReusedSlot, ArrivalCameraCase::SceneTeardown,
                                            ArrivalCameraCase::ImmediateSkip}) {
            CheckArrival(sourceAdult, false, true, true, false, false, cameraCase);
        }
    }
    puts("PASS fixed exit camera, normal/B release, first-update timing, unavailable/reused cameras and teardown");
    PlayState play;
    BgTokiSwd sword;
    // A decorative pedestal must not edit the player's equipment on scene load.
    Reset(play, sword, false, true);
    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
    auto before = gSaveContext;
    BgTokiSwd_Init(&sword.actor, &play);
    REQUIRE(memcmp(&before, &gSaveContext, sizeof(before)) == 0);
    REQUIRE(play.player.currentSwordItemId == ITEM_SWORD_KOKIRI);
    puts("PASS time pedestal init preserves save and player equipment");
    CheckPostRespawnEmptyB();
    CheckSavedAdultWithoutSword();
    CheckResetDuringCutscene();
    CheckExtendedHandoff();
    for (bool adult : { false, true }) {
        for (bool rando : { false, true }) {
            RunInteraction(adult, rando);
            RunInteraction(adult, rando, true);
            RunInteraction(adult, rando, false, 0x1A9);
        }
    }
    puts("PASS native camera and animation, both ages, no progression, local return, early-end recovery");
    CheckRepeatedOwnedLoadouts();
    CheckStockAndInactivePaths();
    CheckRejectedScripts();
    puts("PASS repeated owned loadouts, extended weapons, stock behavior and bounded script validation");
    return 0;
}
