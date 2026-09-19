// The breaks caught here are a variant entering vanilla story/init branches,
// losing room/position at the native cutscene end, or equipping an unowned sword.
#include "time_pedestal_fixture.h"
#include "test_require.h"
#include <cstdio>
#include <vector>

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
s32 CVarGetInteger(const char*, s32 fallback) { return fallback; }
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
void Actor_OfferCarry(Actor*, PlayState*) { offered++; }
bool GameInteractor_Should(int, bool value, ...) { return value; }
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
s32 Player_ActionToModelGroup(Player*, s32 action) { REQUIRE(action == PLAYER_IA_SWORD_CS); return PLAYER_MODELGROUP_SWORD; }
void Player_SetEquipmentData(PlayState*, Player*) {}
s32 Player_SetCsAction(PlayState* play, Actor*, u8 action) { play->player.csAction = action; return 1; }
void Audio_SetCutsceneFlag(s32) {}
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
    sword.actor.id = ACTOR_BG_TOKI_SWD;
    sword.actor.params = params;
    sword.actor.room = 10;
    sword.actor.world.pos = { -736.0f, -63.0f, -2395.0f };
    sword.actor.shape.rot.y = 0x4000; // Quarter-turn makes expected positions hand-checkable.
    sword.actor.draw = BgTokiSwd_Draw;
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
    REQUIRE(offered == 1); // Adult is usable even without Prelude or a Master Sword.
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

int main() {
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
