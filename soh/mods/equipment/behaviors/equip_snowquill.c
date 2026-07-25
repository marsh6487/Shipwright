/**
 * equip_snowquill.c - Snowquill Tunic (Extended Tunic Slot 3)
 *
 * A WHITE recolor tunic (color painted in Player_DrawImpl) that makes Link IMMUNE to all ice/cold:
 *  - enemy ice attacks (Freezards, Blue Tektites, Wolfos ice, etc.)
 *  - rando ice traps + the raw GameInteractor FreezePlayer
 *  - the freeze ANIMATION itself (Link never enters the frozen state)
 *
 * The immunity is a single GATE, not a per-frame effect: func_80837C0C (z_player.c) turns the
 * PLAYER_HIT_RESPONSE_ICE_TRAP response into PLAYER_HIT_RESPONSE_NONE when this tunic is worn, so the
 * frozen actionfunc (Player_Action_8084FB10) and gPlayerAnim_link_normal_ice_down are never reached.
 * OoT has no passive cold-room hazard (Ice Cavern deals no ambient damage), so no timer to gate.
 *
 * This per-frame behavior is therefore a no-op; the predicate ExtEquip_IsSnowquillTunic() drives the
 * gate and the recolor. Included by ext_equip_behavior.c (unity build).
 */

static void Snowquill_Behavior(Player* player, PlayState* play) {
    (void)player;
    (void)play;
}
