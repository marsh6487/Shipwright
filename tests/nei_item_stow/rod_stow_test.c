// Real initializers, equip polling and teardown. Audio, camera and effect
// allocation are fixtures.
#include "global.h"
#include "mods/items/helpers/equip_helper.h"
#include "mods/items/helpers/fx_helper.h"
#include "mods/items/logic/item_rod_fire.h"
#include "mods/items/logic/item_rod_ice.h"
#include "mods/items/logic/item_rod_light.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define fireRodFlameActive gCustomItemState.fireRodFlameActive
#define iceRodWaveActive gCustomItemState.iceRodWaveActive
#define lightRodBeamActive gCustomItemState.lightRodBeamActive

CustomItemState gCustomItemState;
static Player player;
static PlayState play;
static int cameraExits, trailKills, stopSounds, unequipSounds;

u8 ItemInput_CheckDamage(Player *p, s8 *previous) { return 0; }
void FirstPerson_Exit(Player *p, PlayState *context) { ++cameraExits; }
void Audio_StopSfxById(u32 sound) { ++stopSounds; }
void ItemEquip_PlayUnequipSFX(PlayState *context, Player *p) {
  ++unequipSounds;
}
s32 FX_InitSwordTrail(PlayState *context, RodColor *color) { return 7; }
void FX_KillSwordTrail(PlayState *context, s32 index) {
  assert(index == 7);
  ++trailKills;
}
s32 Collider_InitCylinder(PlayState *context, ColliderCylinder *collider) {
  return 0;
}
s32 Collider_SetCylinder(PlayState *context, ColliderCylinder *collider,
                         Actor *actor, ColliderCylinderInit *init) {
  return 0;
}
s32 Collider_DestroyCylinder(PlayState *context, ColliderCylinder *collider) {
  return 0;
}
static void FireRod_InitFlameColliders(Player *p, PlayState *context) {}
static void IceRod_InitWaveColliders(Player *p, PlayState *context) {}
static void LightRod_InitBeamColliders(Player *p, PlayState *context) {}

#include "rod_stow_functions.inc"

// A native animation can finish equipping after the initial C press has passed.
// The A press must still call the rod's real teardown, even though its custom
// polling never saw an equip press.
#define CHECK_NATIVE_EQUIP_AND_CANCEL(Camel, lower, equip)                     \
  do {                                                                         \
    memset(&gCustomItemState, 0, sizeof(gCustomItemState));                    \
    memset(&(equip), 0, sizeof(equip));                                        \
    Player_Init##Camel##IA(&play, &player);                                    \
    assert(lower##Active && (equip).isEquipped);                               \
    lower##FirstPerson = 1;                                                    \
    lower##Charging = 1;                                                       \
    lower##ChargeLevel = 0.6f;                                                 \
    lower##SpinActive = 1;                                                     \
    lower##SpinRadius = 125.0f;                                                \
    ItemInputState input = {.wasEquipped = 1, .otherButtonPressed = 1};        \
    ItemEquip_Update(&(equip), &input, NULL, Camel##_OnUnequip, &player,       \
                     &play);                                                   \
    assert(!lower##Active && !(equip).isEquipped && !lower##FirstPerson);      \
    assert(!lower##Charging && lower##ChargeLevel == 0.0f &&                   \
           !lower##SpinActive);                                                \
    assert(lower##SpinRadius == 0.0f && lower##BlureIdx == -1);                \
  } while (0)

#define CHECK_EXPLICIT_PUTAWAY(Camel, lower, equip)                            \
  do {                                                                         \
    Player_Init##Camel##IA(&play, &player);                                    \
    (equip).isEquipped =                                                       \
        0; /* Cleanup follows actual held state, even if polling was stale. */ \
    lower##FirstPerson = lower##Charging = 1;                                  \
    int previousSounds = unequipSounds;                                        \
    Camel##_PutAway(&player, &play);                                           \
    assert(!lower##Active && !lower##FirstPerson && !lower##Charging &&        \
           !(equip).isEquipped);                                               \
    assert(unequipSounds == previousSounds + 1);                               \
    CustomItemState expected = gCustomItemState;                               \
    int previousStops = stopSounds;                                            \
    Camel##_PutAway(&player, &play);                                           \
    assert(!memcmp(&expected, &gCustomItemState, sizeof(expected)));           \
    assert(unequipSounds == previousSounds + 1 &&                              \
           stopSounds == previousStops);                                       \
  } while (0)

int main(void) {
  CHECK_NATIVE_EQUIP_AND_CANCEL(FireRod, fireRod, sEquipState);
  CHECK_NATIVE_EQUIP_AND_CANCEL(IceRod, iceRod, sIceEquipState);
  CHECK_NATIVE_EQUIP_AND_CANCEL(LightRod, lightRod, sLightEquipState);
  assert(cameraExits == 3 && trailKills == 3 && unequipSounds == 3 &&
         stopSounds > 0);
  CHECK_EXPLICIT_PUTAWAY(FireRod, fireRod, sEquipState);
  CHECK_EXPLICIT_PUTAWAY(IceRod, iceRod, sIceEquipState);
  CHECK_EXPLICIT_PUTAWAY(LightRod, lightRod, sLightEquipState);
  puts("PASS: three native rod equips cancel through real polling; explicit "
       "teardown is idempotent");
  return 0;
}
