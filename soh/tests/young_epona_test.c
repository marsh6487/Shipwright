#include "global.h"
#include "vt.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "objects/object_horse_link_child/rideable_young_epona.h"
#if __has_include("young_epona.h")
#include "young_epona.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x)                                                       \
    do {                                                                 \
        if (!(x)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                                     \
        }                                                                \
    } while (0)
#undef osSyncPrintf
#define osSyncPrintf(...) ((void)0)

SaveContext gSaveContext;
u32 gBitFlags[32];
u8 gItemSlots[56];
static GameInfo gameInfo;
GameInfo* gGameInfo = &gameInfo;
static PlayState play;
static Player player;
static EnHorse horse;
static int enabled, assets, notes, failedSpawn, spawnCount, mounted, camera, adultSetup;
static s16 spawnParams;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    return enabled;
}
uint8_t ResourceMgr_FileExists(const char* name) {
    return assets;
}
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...) {
    return notes && result;
}
void Horse_ResetHorseData(PlayState* p) {
    adultSetup++;
}
void Horse_SetupInGameplay(PlayState* p, Player* l) {
    adultSetup++;
}
void Horse_SetupInCutscene(PlayState* p, Player* l) {
    adultSetup++;
}
s32 Flags_GetEventChkInf(s32 flag) {
    return 0;
}
static void Alive(Actor* a, PlayState* p) {
}
Actor* Actor_Spawn(ActorContext* ctx, PlayState* p, s16 id, f32 x, f32 y, f32 z, s16 rx, s16 ry, s16 rz, s16 params) {
    spawnCount++;
    spawnParams = params;
    if (failedSpawn) {
        return NULL;
    }
    horse.actor.id = id;
    horse.actor.update = Alive;
    horse.actor.world.pos = (Vec3f){ x, y, z };
    horse.actor.shape.rot.y = ry;
    horse.type = 2;
    horse.action = (params & 0x3FFF) == 2 ? ENHORSE_ACT_INACTIVE : ENHORSE_ACT_IDLE;
    ctx->actorLists[ACTORCAT_BG].head = &horse.actor;
    AREG(6) = DREG(53) = 0;
    return &horse.actor;
}
void Actor_MountHorse(PlayState* p, Player* l, Actor* a) {
    REQUIRE(a != NULL);
    mounted++;
    l->rideActor = a;
}
void Actor_RequestHorseCameraSetting(PlayState* p, Player* l) {
    camera++;
}

/* PRODUCTION_HORSE */

static void Reset(void) {
    for (int i = 0; i < 32; i++) {
        gBitFlags[i] = 1U << i;
    }
    gItemSlots[ITEM_OCARINA_FAIRY] = SLOT_OCARINA;
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&horse, 0, sizeof(horse));
    memset(&gameInfo, 0, sizeof(gameInfo));
    enabled = assets = notes = 1;
    failedSpawn = spawnCount = mounted = camera = adultSetup = 0;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.inventory.questItems = 1 << QUEST_SONG_EPONA;
    gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
    gSaveContext.horseData.scene = SCENE_HYRULE_FIELD;
    gSaveContext.horseData.pos = (Vec3s){ 123, 456, 789 };
    play.sceneNum = SCENE_HYRULE_FIELD;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
}

int main(void) {
    Reset();
    enabled = 0;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
    Reset();
    assets = 0;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
    Reset();
    gSaveContext.inventory.questItems = 0;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
    Reset();
    HorseData adult = gSaveContext.horseData;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 1);
    REQUIRE(spawnParams == (0x4000 | 2));
    REQUIRE(memcmp(&adult, &gSaveContext.horseData, sizeof(adult)) == 0);
    int scenes[] = { SCENE_HYRULE_FIELD, SCENE_LAKE_HYLIA, SCENE_GERUDO_VALLEY, SCENE_GERUDOS_FORTRESS,
                     SCENE_LON_LON_RANCH };
    for (int i = 0; i < ARRAY_COUNT(scenes); i++) {
        Reset();
        play.sceneNum = scenes[i];
        AREG(6) = 1;
        Horse_InitPlayerHorse(&play, &player);
        REQUIRE(spawnCount == 1 && mounted == 1 && camera == 1);
        REQUIRE(spawnParams == (0x4000 | 9));
    }
    Reset();
    play.sceneNum = SCENE_DEKU_TREE;
    AREG(6) = 1;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0 && AREG(6) == 0);
    Reset();
    gSaveContext.sceneLayer = 4;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
    Reset();
    gSaveContext.linkAge = LINK_AGE_ADULT;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0 && adultSetup == 1);
    Reset();
    failedSpawn = 1;
    AREG(6) = 1;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(mounted == 0);
    Reset();
    gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_NONE;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
    Reset();
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    notes = 0;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 0);
#if __has_include("young_epona.h")
    Reset();
    REQUIRE(Horse_TrySummonYoungEpona(&play));
    REQUIRE(spawnCount == 1 && DREG(53) == 1);
    REQUIRE(Horse_TrySummonYoungEpona(&play));
    REQUIRE(spawnCount == 1);
    REQUIRE(Horse_GetActorSaveData(&horse.actor) == &gSaveContext.ship.youngHorseData);
    Horse_SaveYoungEpona(&play, &horse.actor);
    REQUIRE(!gSaveContext.ship.youngHorseDataValid);
    horse.action = ENHORSE_ACT_IDLE;
    horse.actor.world.pos = (Vec3f){ 12, 34, 56 };
    Horse_SaveYoungEpona(&play, &horse.actor);
    REQUIRE(gSaveContext.ship.youngHorseDataValid);
    REQUIRE(gSaveContext.ship.youngHorseData.pos.y == 34);
    REQUIRE(gSaveContext.horseData.pos.y == 456);
    play.actorCtx.actorLists[ACTORCAT_BG].head = NULL;
    Horse_InitPlayerHorse(&play, &player);
    REQUIRE(spawnCount == 2 && spawnParams == (0x4000 | 1));
    REQUIRE(horse.actor.world.pos.x == 12 && horse.actor.world.pos.z == 56);
    gSaveContext.linkAge = LINK_AGE_ADULT;
    REQUIRE(!Horse_CanUseYoungEpona());
    REQUIRE(!Horse_TrySummonYoungEpona(&play));
    REQUIRE(gSaveContext.horseData.pos.y == 456);
    Reset();
    player.stateFlags1 = PLAYER_STATE1_DEAD;
    REQUIRE(!Horse_TrySummonYoungEpona(&play) && spawnCount == 0);
    Reset();
    player.stateFlags1 = PLAYER_STATE1_ON_HORSE;
    REQUIRE(!Horse_TrySummonYoungEpona(&play) && spawnCount == 0);
#endif
    puts("PASS: child gates, native five areas, mounted transitions, adult isolation, failed allocation");
    return 0;
}
